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

#include "cg_race.h"

/**
 * @file
 * @brief The hooks, the ghost, and the barriers. See cg_race.h.
 *
 * The ghost the server sends wears the viewer's client slot, because a player
 * model needs one; here it is dressed instead as the course record's holder,
 * whose client info arrives on `CS_RACE_GHOST`, and drawn translucent. Another
 * player's ghost is theirs to see and not ours.
 *
 * The barriers are the `func_race_*` brushes. The server decides which of them
 * pass this client and says so every frame, as `SV_CMD_RACE_BARRIERS`, so that
 * prediction clips exactly the set the server will.
 */

// how much of the ghost is there
#define RACE_GHOST_ALPHA .5f

static struct {
  ParseConfigString ParseConfigString;
  ParseServerCommand ParseServerCommand;
  MediaDidLoad MediaDidLoad;
  FilterEntity FilterEntity;
  ClientInfo ClientInfo;
  EntityEffects EntityEffects;
  ClipEntity ClipEntity;
} previous;

static cg_client_info_t cg_race_ghost;

// the entity numbers of the barriers that pass this client, as the server last said
static int32_t cg_race_passable[RACE_MAX_BARRIERS];
static size_t cg_race_passable_count;

uint32_t Cg_Race_Time(const player_state_t *ps) {
  return (uint16_t) ps->stats[STAT_RACE_TIME_LOW] | ((uint32_t) (uint16_t) ps->stats[STAT_RACE_TIME_HIGH] << 16);
}

/**
 * @brief Dresses the ghost as the record holder, or as nobody when there is no
 * record, which `Cg_LoadClient` reads as the default.
 */
static void Cg_Race_LoadGhost(void) {
  Cg_LoadClient(&cg_race_ghost, cgi.ConfigString(CS_RACE_GHOST));
}

static bool Cg_Race_IsGhost(const cl_entity_t *ent) {
  return ent->current.effects & EF_RACE_GHOST;
}

static bool Cg_ParseConfigString_Race(int32_t index) {

  if (index == CS_RACE_GHOST) {
    Cg_Race_LoadGhost();
    return true;
  }

  return previous.ParseConfigString(index);
}

/**
 * @brief The barriers that pass this client, which the server sends every frame.
 * More than fit are read and dropped, so that the message stays in step.
 */
static bool Cg_ParseServerCommand_Race(int32_t cmd) {

  if (cmd != SV_CMD_RACE_BARRIERS) {
    return previous.ParseServerCommand(cmd);
  }

  const int32_t count = cgi.ReadByte();

  cg_race_passable_count = 0;

  for (int32_t i = 0; i < count; i++) {
    const int32_t entity = cgi.ReadShort();

    if (cg_race_passable_count < RACE_MAX_BARRIERS) {
      cg_race_passable[cg_race_passable_count++] = entity;
    }
  }

  return true;
}

/**
 * @brief The client infos are media, and are reloaded with it; a new level
 * starts with no barriers passed until the server says otherwise.
 */
static void Cg_MediaDidLoad_Race(void) {

  previous.MediaDidLoad();

  Cg_Race_LoadGhost();
  cg_race_passable_count = 0;
}

/**
 * @brief `G_Race_ClipEntity`, from the set the server sent: a barrier the
 * server let this client pass does not clip this client's own moves; it clips
 * everything else, as it does on the server.
 */
static bool Cg_ClipEntity_Race(const cl_entity_t *mover, const cl_entity_t *ent, const vec3_t start, const vec3_t end, const box3_t bounds) {

  for (size_t i = 0; mover == cgi.client->entity && i < cg_race_passable_count; i++) {
    if (cg_race_passable[i] == ent->current.number) {
      return false;
    }
  }

  return previous.ClipEntity ? previous.ClipEntity(mover, ent, start, end, bounds) : true;
}

static bool Cg_FilterEntity_Race(const cl_entity_t *ent) {

  if (Cg_Race_IsGhost(ent) && ent->current.client != cgi.client->frame.ps.client) {
    return false;
  }

  return previous.FilterEntity(ent);
}

static cg_client_info_t *Cg_ClientInfo_Race(const cl_entity_t *ent) {

  if (Cg_Race_IsGhost(ent)) {
    return &cg_race_ghost;
  }

  return previous.ClientInfo(ent);
}

static void Cg_EntityEffects_Race(cl_entity_t *ent, r_entity_t *e) {

  previous.EntityEffects(ent, e);

  if (Cg_Race_IsGhost(ent)) {
    e->effects |= EF_BLEND | EF_NO_SHADOW;
    e->color = Vec4_Scale(e->color, RACE_GHOST_ALPHA);
  }
}

void Cg_Race_Init(void) {
  static bool installed;

  if (installed) {
    return;
  }

  installed = true;

  Cg_DrawHudElements = Cg_Race_DrawHud;
  Cg_DrawScores = Cg_Race_DrawScores;

  previous.ParseConfigString = Cg_ParseConfigString;
  Cg_ParseConfigString = Cg_ParseConfigString_Race;

  previous.ParseServerCommand = Cg_ParseServerCommand;
  Cg_ParseServerCommand = Cg_ParseServerCommand_Race;

  previous.MediaDidLoad = Cg_MediaDidLoad;
  Cg_MediaDidLoad = Cg_MediaDidLoad_Race;

  previous.FilterEntity = Cg_FilterEntity;
  Cg_FilterEntity = Cg_FilterEntity_Race;

  previous.ClientInfo = Cg_ClientInfo;
  Cg_ClientInfo = Cg_ClientInfo_Race;

  previous.EntityEffects = Cg_EntityEffects;
  Cg_EntityEffects = Cg_EntityEffects_Race;

  previous.ClipEntity = Cg_ClipEntity;
  Cg_ClipEntity = Cg_ClipEntity_Race;
}
