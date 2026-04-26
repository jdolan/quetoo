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

/**
 * @brief Updates a spectating client's chase camera state to mirror their target.
 */
void G_ClientChaseThink(g_client_t *cl) {

  g_entity_t *ent = cl->entity;
  g_client_t *target = cl->chase_target;

  if (target) {
    vec3_t new_delta;

    // calculate delta angles if switching targets
    if (target != cl->old_chase_target) {
      new_delta = Vec3_Subtract(cl->angles, target->angles);
      cl->old_chase_target = target;
    } else {
      new_delta = Vec3_Zero();
    }

    // copy origin
    ent->s.origin = target->entity->s.origin;

    // velocity
    ent->velocity = target->entity->velocity;

    // and angles
    cl->angles = target->angles;

    // and player state
    memcpy(&cl->ps.pm_state, &target->ps.pm_state, sizeof(pm_state_t));

    // add in delta angles in case we've switched targets
    if (!Vec3_Equal(new_delta, Vec3_Zero())) {
      cl->ps.pm_state.delta_angles = Vec3_Add(cl->ps.pm_state.delta_angles, new_delta);
    }

    // disable the spectator's input
    cl->ps.pm_state.type = PM_FREEZE;
  } else {
    cl->ps.pm_state.delta_angles.z = -cl->ps.pm_state.delta_angles.z;

    // enable the spectator's input
    cl->ps.pm_state.type = PM_SPECTATOR;
  }

  gi.LinkEntity(ent);
}

/**
 * @brief Advances the chase camera to the next living player.
 */
void G_ClientChaseNext(g_client_t *cl) {

  if (!cl->chase_target) {
    return;
  }

  g_client_t *next = NULL;

  int32_t i = cl->chase_target->ps.client;
  do {
    i++;

    if (i == sv_max_clients->integer) {
      i = 0;
    }

    next = ge.clients[i];

    if (G_IsMeat(next->entity)) {
      break;
    }

  } while (next != cl->chase_target);

  cl->chase_target = next;
}

/**
 * @brief Moves the chase camera back to the previous living player.
 */
void G_ClientChasePrevious(g_client_t *cl) {

  if (!cl->chase_target) {
    return;
  }

  g_client_t *prev = NULL;

  int32_t i = cl->chase_target->ps.client;
  do {
    i--;

    if (i == -1) {
      i = sv_max_clients->integer - 1;
    }

    prev = ge.clients[i];

    if (G_IsMeat(prev->entity)) {
      break;
    }

  } while (prev != cl->chase_target);

  cl->chase_target = prev;
}

/**
 * @brief Finds the first available chase target and assigns it to the specified ent.
 */
void G_ClientChaseTarget(g_client_t *cl) {

  G_ForEachClient(other, {
    if (other != cl && G_IsMeat(other->entity)) {
      cl->chase_target = other;
      break;
    }
  });
}

