/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "cg_local.h"

/**
 * @brief Updates the sound stage from the interpolated frame.
 */
void Cg_PrepareStage(const ClientFrame *frame) {

  cgi.stage->origin = cgi.view->origin;
  cgi.stage->angles = cgi.view->angles;
  cgi.stage->forward = cgi.view->forward;
  cgi.stage->right = cgi.view->right;
  cgi.stage->up = cgi.view->up;
  cgi.stage->velocity = frame->ps.pm_state.velocity;
  cgi.stage->contents = cgi.view->contents;
}

/**
 * @brief Parses a positioned sound message from the server and plays it.
 * @remarks Every field the flags promise is read before the sample is judged. Giving up early
 * leaves the rest of the message to be read as the next command, so a sound this client does not
 * have would not merely go unheard: it would desync the stream and drop the connection with an
 * illegible server message.
 */
void Cg_ParseSound(void) {

  const byte flags = cgi.ReadByte();

  const uint8_t sample_index = cgi.ReadByte();
  SoundPlaySample play = {
    .sample = cgi.client->sounds[sample_index]
  };

  if (!play.sample) {
    Cg_Warn("NULL sample for sound index %u\n", sample_index);
  }

  if (flags & SOUND_ENTITY) {
    const int16_t number = cgi.ReadShort();
    assert(number < MAX_ENTITIES);
    const ClientEntity *ent = &cgi.client->entities[number];
    play.entity = ent;
    if (ent->current.solid == SOLID_BSP) {
      play.origin = Box3_Center(ent->abs_bounds);
    } else {
      play.origin = ent->current.origin;
      if (play.sample && play.sample->media.name[0] == '*') {
        if (ent->current.client >= MAX_CLIENTS) {
          Cg_Warn("Bad client %u for entity %d\n", ent->current.client, number);
          play.sample = NULL;
        } else {
          const ClientGameClientInfo *info = Cg_ClientInfo(ent);
          play.sample = cgi.LoadClientModelSample(info->model, info->torso->mesh->sounds, play.sample->media.name);
        }
      }
    }
  } else {
    play.entity = NULL;
  }

  if (flags & SOUND_ORIGIN) {
    play.origin = cgi.ReadPosition();
  }

  if (flags & SOUND_PITCH) {
    play.pitch = cgi.ReadChar() * 2;
  }

  if (flags & SOUND_GAIN) {
    play.gain = cgi.ReadByte() / 255.f;
  }

  if (flags & SOUND_RELATIVE) {
    play.flags |= S_PLAY_RELATIVE;
  }

  if (play.sample) {
    Cg_AddSample(cgi.stage, &play);
  }
}


/**
 * @brief `S_PlaySampleThink` implementation.
 */
static void Cg_PlaySampleThink(const SoundStage *stage, SoundPlaySample *play) {
  
  if (play->entity) {
    const ClientEntity *ent = play->entity;
    if (ent == Cg_Self()) {
      play->flags |= S_PLAY_RELATIVE;
    } else if (ent->current.solid == SOLID_BSP) {
      play->origin = Box3_ClampPoint(ent->abs_bounds, stage->origin);
      play->velocity = Vec3_Subtract(ent->prev.origin, ent->current.origin);
    } else {
      play->origin = ent->origin;
      play->velocity = Vec3_Subtract(ent->prev.origin, ent->current.origin);
    }
  }

  if (play->flags & S_PLAY_RELATIVE) {
    play->origin = play->velocity = Vec3_Zero();
  } else {
    if ((cgi.PointContents(play->origin) & CONTENTS_MASK_LIQUID) != (cgi.PointContents(stage->origin) & CONTENTS_MASK_LIQUID)) {
      play->flags |= S_PLAY_UNDERWATER;
    } else {
      play->flags &= ~S_PLAY_UNDERWATER;
    }

    const CmTrace tr = cgi.Trace(stage->origin, play->origin, Box3_Zero(), play->entity, CONTENTS_MASK_CLIP_PROJECTILE);
    if (tr.fraction < 1.f) {
      play->flags |= S_PLAY_OCCLUDED;
    } else {
      play->flags &= ~S_PLAY_OCCLUDED;
    }
  }
}

/**
 * @brief Wraps `cgi.AddSample`, installing the default PlaySampleThink function.
 */
void Cg_AddSample(SoundStage *stage, const SoundPlaySample *play) {

  SoundPlaySample s = *play;

  if (s.Think == NULL) {
    s.Think = Cg_PlaySampleThink;
  }

  cgi.AddSample(stage, &s);
}
