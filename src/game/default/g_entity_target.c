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

#include "g_local.h"

#define LIGHT_START_ON       1
#define LIGHT_BACK_AND_FORTH 2

/**
 * @brief Returns the team member immediately preceding `node`, or the master
 * itself if `node` is the first member. The team is a singly-linked list, so we
 * simply walk it from the head; chains are short, so this is cheap.
 */
static g_entity_t *G_target_light_Prev(g_entity_t *master, const g_entity_t *node) {

  g_entity_t *prev = master;
  while (prev->team_next && prev->team_next != node) {
    prev = prev->team_next;
  }
  return prev;
}

/**
 * @brief For singular lights, simply toggle them. For teamed lights, advance the
 * lit member through the team, one lit at a time. By default the chase wraps from
 * the tail back to the master; with the `back_and_forth` flag it instead reverses
 * direction at each end, bouncing the lit member back and forth. The direction is
 * stored on the master in `count` (0 = forward, 1 = backward).
 */
static void G_target_light_Cycle(g_entity_t *ent) {

  g_entity_t *master = ent->team_master;
  if (!master || master->team_next == NULL) {
    // no team, or a team of one: just toggle
    ent->s.effects ^= EF_LIGHT;
    return;
  }

  G_Debug("Cycling %s\n", etos(master->enemy));

  master->enemy->s.effects ^= EF_LIGHT;

  if (master->spawn_flags & LIGHT_BACK_AND_FORTH) {

    if (master->count == 0) { // forward
      if (master->enemy->team_next) {
        master->enemy = master->enemy->team_next;
      } else { // reached the tail, reverse
        master->count = 1;
        master->enemy = G_target_light_Prev(master, master->enemy);
      }
    } else { // backward
      if (master->enemy != master) {
        master->enemy = G_target_light_Prev(master, master->enemy);
      } else { // reached the head, reverse
        master->count = 0;
        master->enemy = master->team_next;
      }
    }
  } else {

    master->enemy = master->enemy->team_next;
    if (master->enemy == NULL) {
      master->enemy = master;
    }
  }

  master->enemy->s.effects ^= EF_LIGHT;
}

/**
 * @brief Handles use activation of a `target_light`, toggling or cycling the light after an optional delay.
 */
static void G_target_light_Use(g_entity_t *ent, g_entity_t *other, g_entity_t *activator) {

  if (ent->delay) {
    ent->Think = G_target_light_Cycle;
    ent->next_think = g_level.time + ent->delay * 1000.0;
  } else {
    G_target_light_Cycle(ent);
  }

  if (ent->wait) {
    ent->Think = G_target_light_Cycle;
    ent->next_think = g_level.time + (ent->delay + ent->wait) * 1000.0;
  }
}

/*QUAKED target_light (1 1 1) (-4 -4 -4) (4 4 4) start_on back_and_forth
 Emits a user-defined light when used. Lights can be chained with teams.

 -------- Keys --------
 color : The light color (default 1.0 1.0 1.0).
 radius : The radius of the light in units (default 300).
 intensity : The light brightness multiplier, matching point lights (default 1.0).
 material : The name of a material whose `toggle` stages (and flares) switch with this
            light, e.g. quake2/ceil2_base. Apply the `toggle` surface flag to the faces.
 delay : The delay before activating, in seconds (default 0).
 targetname : The target name of this entity.
 team : The team name for alternating lights.
 wait : If specified, an additional cycle will fire after this interval.

 -------- Spawn flags --------
 start_on : The light will start on.
 back_and_forth : Teamed lights bounce the lit member back and forth instead of
                  wrapping from the tail back to the master. Set on the master.

 -------- Notes --------
 Use this entity to add switched lights. Use the wait key to synchronize
 color cycles with other entities.
*/
void G_target_light(g_entity_t *ent) {

  vec3_t color = gi.EntityValue(ent->def, "color")->vec3;
  if (Vec3_Equal(color, Vec3_Zero())) {
    color = Vec3_One();
  }

  float radius = gi.EntityValue(ent->def, "radius")->value;
  radius = radius ?: 300.f;

  float intensity = gi.EntityValue(ent->def, "intensity")->value;
  intensity = intensity ?: 1.f;

  ent->s.color = Color_Color32(Color3fv(color));
  ent->s.termination.x = radius;
  ent->s.termination.y = intensity;

  if (ent->spawn_flags & LIGHT_START_ON) {
    ent->s.effects |= EF_LIGHT;
  }

  ent->enemy = ent;
  ent->Use = G_target_light_Use;

  gi.LinkEntity(ent);
}

#define SPEAKER_LOOP_ON 1
#define SPEAKER_LOOP_OFF 2

#define SPEAKER_LOOP (SPEAKER_LOOP_ON | SPEAKER_LOOP_OFF)

/**
 * @brief Handles use activation of a `target_speaker`, toggling looping sounds or playing a one-shot sound.
 */
static void G_target_speaker_Use(g_entity_t *ent, g_entity_t *other, g_entity_t *activator) {

  if (ent->spawn_flags & SPEAKER_LOOP) { // looping sound toggles
    if (ent->s.sound) {
      ent->s.sound = 0;
    } else {
      ent->s.sound = ent->sound;
    }
  } else { // intermittent sound
    G_MulticastSound(&(const g_play_sound_t) {
      .index = ent->sound,
      .origin = &ent->s.origin,
    }, MULTICAST_PHS);
  }
}

/*QUAKED target_speaker (1 0 0) (-8 -8 -8) (8 8 8) loop_on loop_off
 Plays a sound each time it is used, or in loop if requested.

 -------- Keys --------
 sound : The name of the sample to play, e.g. voices/haunting.
 targetname : The target name of this entity.

 -------- Spawn flags --------
 loop_on : The sound can be toggled, and will play in loop until used.
 loop_off : The sound can be toggled, and will begin playing when used.

 -------- Notes --------
 Use this entity only when a sound must be triggered by another entity. For
 ambient sounds, use the client-side version, misc_sound.
*/
void G_target_speaker(g_entity_t *ent) {

  const char *sound = gi.EntityValue(ent->def, "sound")->string;
  if (!q_strlen(sound)) {
    gi.Warn("No sound specified for %s\n", etos(ent));
    return;
  }

  ent->sound = gi.SoundIndex(sound);

  const int32_t spawn_flags = gi.EntityValue(ent->def, "spawnflags")->integer;

  // check for looping sound
  if (spawn_flags & SPEAKER_LOOP_ON) {
    ent->s.sound = ent->sound;
  }

  ent->Use = G_target_speaker_Use;

  // link the entity so the server can determine who to send updates to
  gi.LinkEntity(ent);
}

/*QUAKED target_string (0 0 1) (-8 -8 -8) (8 8 8)
 Displays a center-printed message to the player when used.
 -------- KEYS --------
 message : The message to display.
 targetname : The target name of this entity.
 */
void G_target_string(g_entity_t *ent) {

  if (!ent->message) {
    ent->message = "";
  }

  // the rest is handled by G_UseTargets
}
