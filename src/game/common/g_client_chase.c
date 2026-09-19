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
void G_ClientChaseThink(GameClient *cl) {

  GameEntity *ent = cl->entity;
  GameClient *target = cl->chaseTarget;

  if (target) {
    Vec3 newDelta;

    // calculate delta angles if switching targets
    if (target != cl->oldChaseTarget) {
      newDelta = Vec3_Subtract(cl->angles, target->angles);
      cl->oldChaseTarget = target;
    } else {
      newDelta = Vec3_Zero();
    }

    // copy origin
    ent->s.origin = target->entity->s.origin;

    // velocity
    ent->velocity = target->entity->velocity;

    // and angles
    cl->angles = target->angles;

    // and player state
    memcpy(&cl->ps.pmState, &target->ps.pmState, sizeof(PlayerMoveState));

    // add in delta angles in case we've switched targets
    if (!Vec3_Equal(newDelta, Vec3_Zero())) {
      cl->ps.pmState.deltaAngles = Vec3_Add(cl->ps.pmState.deltaAngles, newDelta);
    }

    // disable the spectator's input
    cl->ps.pmState.type = PM_FREEZE;
  } else {
    cl->ps.pmState.deltaAngles.z = -cl->ps.pmState.deltaAngles.z;

    // drop any death camera inherited from the client we were chasing, which
    // pmove won't reclaim for a spectator
    cl->ps.pmState.flags &= ~PMF_DEATH_CAM;
    cl->ps.pmState.viewOffset = Vec3_Zero();

    // enable the spectator's input
    cl->ps.pmState.type = PM_SPECTATOR;
  }

  gi.LinkEntity(ent);
}

/**
 * @brief Advances the chase camera to the next living player.
 */
void G_ClientChaseNext(GameClient *cl) {

  if (!cl->chaseTarget) {
    G_ClientChaseStart(cl); // nobody to advance from, so acquire one
    return;
  }

  GameClient *next = NULL;

  int32_t i = cl->chaseTarget->ps.client;
  do {
    i++;

    if (i == sv_max_clients->integer) {
      i = 0;
    }

    next = ge.clients[i];

    if (G_IsMeat(next->entity)) {
      break;
    }

  } while (next != cl->chaseTarget);

  cl->chaseTarget = next;
}

/**
 * @brief Moves the chase camera back to the previous living player.
 */
void G_ClientChasePrevious(GameClient *cl) {

  if (!cl->chaseTarget) {
    G_ClientChaseStart(cl); // nobody to step back from, so acquire one
    return;
  }

  GameClient *prev = NULL;

  int32_t i = cl->chaseTarget->ps.client;
  do {
    i--;

    if (i == -1) {
      i = sv_max_clients->integer - 1;
    }

    prev = ge.clients[i];

    if (G_IsMeat(prev->entity)) {
      break;
    }

  } while (prev != cl->chaseTarget);

  cl->chaseTarget = prev;
}

/**
 * @brief Finds the first available chase target and assigns it to the specified ent.
 */
void G_ClientChaseTarget(GameClient *cl) {

  G_ForEachClient(other, {
    if (other != cl && G_IsMeat(other->entity)) {
      cl->chaseTarget = other;
      break;
    }
  });
}

/**
 * @brief Detaches a chasing spectator, returning them to free `PM_SPECTATOR` flight.
 * @details Exposes the same detach logic `BUTTON_ATTACK` already performs
 * (`G_ClientThink`), as a standalone command, so a unified camera-mode cycle control can
 * drive it without stealing the attack button.
 */
void G_ClientChaseStop(GameClient *cl) {

  if (cl->chaseTarget) {
    cl->chaseTarget = cl->oldChaseTarget = NULL;
    G_ClientChaseThink(cl);
  }
}

/**
 * @brief Attaches a free-flying spectator to the first available chase target.
 * @details The attach counterpart to `G_ClientChaseStop`. Reached by asking to step to another
 * target while detached, so the spectator check lives here rather than in the command dispatch:
 * a dead player cycling weapons must not be put on someone else's back.
 */
void G_ClientChaseStart(GameClient *cl) {

  if (!cl->chaseTarget && cl->persistent.spectator) {
    G_ClientChaseTarget(cl);
    G_ClientChaseThink(cl);
  }
}

