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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "g_local.h"

/**
 * @brief The distance from its team's flag within which a defender stays.
 */
#define AI_CTF_DEFEND_RADIUS 768.f

/**
 * @brief The distance from its team's flag within which a carrier holds while its team's flag is
 * away, until the flag returns and the carrier can capture.
 */
#define AI_CTF_HOLD_RADIUS 512.f

/**
 * @brief The distance from its team's carrier within which an escort stays.
 */
#define AI_CTF_ESCORT_RADIUS 384.f

/**
 * @brief The distance from its team's dropped flag within which a bot that is not a defender goes
 * to return it.
 */
#define AI_CTF_RETURN_DISTANCE 1024.f

/**
 * @brief The distance from its team's dropped flag within which a bot that goes to return it keeps
 * on, so that a route that bends away from the flag does not turn the bot back and forth.
 */
#define AI_CTF_RETURN_KEEP_DISTANCE 1536.f

static struct {
  GameAiTaskHook Task;
} previous;

/**
 * @return The team's flag where it lies dropped, or NULL if it lies at its base or is carried.
 */
static const GameEntity *G_Ai_Ctf_DroppedFlag(const GameTeam *team) {

  const GameEntity *base = G_FlagForTeam(team);

  if (!base) {
    return NULL;
  }

  G_ForEachEntity(ent, {
    if (ent->item == base->item && (ent->spawnFlags & SF_ITEM_DROPPED)) {
      return ent;
    }
  });

  return NULL;
}

/**
 * @return The client that carries the team's flag, or NULL.
 */
static const GameClient *G_Ai_Ctf_Carrier(const GameTeam *team) {

  const GameEntity *base = G_FlagForTeam(team);

  if (!base) {
    return NULL;
  }

  G_ForEachClient(cl, {
    if (cl->inventory[base->item->def.tag]) {
      return cl;
    }
  });

  return NULL;
}

/**
 * @return True if the bot defends its team's flag: the first third of its team's bots, by client
 * number, if the team has two bots or more.
 */
static bool G_Ai_Ctf_IsDefender(const GameClient *cl) {

  int32_t bots = 0, index = -1;

  G_ForEachClient(other, {
    if (other->ai && other->persistent.team == cl->persistent.team) {
      if (other == cl) {
        index = bots;
      }
      bots++;
    }
  });

  return bots >= 2 && index < Maxi(1, bots / 3);
}

/**
 * @return The enemy flag that the bot goes for: the nearest that lies dropped, else the nearest at
 * its base, or NULL if none is to be had.
 */
static const GameEntity *G_Ai_Ctf_EnemyFlag(const GameClient *cl) {

  const GameEntity *dropped = NULL, *home = NULL;
  float droppedDistance = FLT_MAX, homeDistance = FLT_MAX;

  for (int32_t t = 0; t < gameLevel.numTeams; t++) {
    const GameTeam *team = &gameTeamList[t];

    if (team == cl->persistent.team) {
      continue;
    }

    const GameEntity *flag = G_Ai_Ctf_DroppedFlag(team);

    if (flag) {
      const float distance = Vec3_Distance(cl->entity->s.origin, flag->s.origin);
      if (distance < droppedDistance) {
        dropped = flag;
        droppedDistance = distance;
      }
      continue;
    }

    flag = G_FlagForTeam(team);

    if (flag && flag->solid == SOLID_TRIGGER) {
      const float distance = Vec3_Distance(cl->entity->s.origin, flag->s.origin);
      if (distance < homeDistance) {
        home = flag;
        homeDistance = distance;
      }
    }
  }

  return dropped ?: home;
}

/**
 * @return The bot's teammate that carries an enemy flag, or NULL.
 */
static const GameClient *G_Ai_Ctf_TeamCarrier(const GameClient *cl) {

  for (int32_t t = 0; t < gameLevel.numTeams; t++) {
    const GameTeam *team = &gameTeamList[t];

    if (team == cl->persistent.team) {
      continue;
    }

    const GameClient *carrier = G_Ai_Ctf_Carrier(team);

    if (carrier && carrier != cl && carrier->persistent.team == cl->persistent.team) {
      return carrier;
    }
  }

  return NULL;
}

/**
 * @brief Gives the bot its task in capture the flag. A carrier captures, or holds near its team's
 * flag while the flag is away, which goes before a fight. A bot near its team's dropped flag, or a
 * defender, returns it. A defender hunts the enemy that carries its team's flag, or defends the
 * flag. An attacker hunts that enemy while its team's carrier holds, else escorts the carrier,
 * takes an enemy flag, or hunts the enemy that carries its team's flag.
 */
static bool G_Ai_Task_Ctf(const GameClient *cl, GameAiTask *task) {

  const GameTeam *team = cl->persistent.team;
  const GameEntity *base = team ? G_FlagForTeam(team) : NULL;

  if (!base) {
    return previous.Task(cl, task);
  }

  if (G_GetFlag(cl)) {
    if (base->solid == SOLID_TRIGGER) {
      *task = (GameAiTask) { .name = "capture", .ent = base, .urgent = true };
    } else {
      *task = (GameAiTask) {
        .name = "hold", .ent = base, .radius = AI_CTF_HOLD_RADIUS, .urgent = true
      };
    }
    return true;
  }

  const bool defender = G_Ai_Ctf_IsDefender(cl);
  const GameEntity *dropped = G_Ai_Ctf_DroppedFlag(team);

  const float reach = cl->ai->task.ent == dropped ? AI_CTF_RETURN_KEEP_DISTANCE
                                                  : AI_CTF_RETURN_DISTANCE;

  if (dropped && (defender || Vec3_Distance(cl->entity->s.origin, dropped->s.origin) < reach)) {
    *task = (GameAiTask) { .name = "return", .ent = dropped };
    return true;
  }

  const GameClient *thief = G_Ai_Ctf_Carrier(team);

  if (defender) {
    if (thief) {
      *task = (GameAiTask) { .name = "hunt", .ent = thief->entity, .urgent = G_Ai_IsArmed(cl) };
    } else {
      *task = (GameAiTask) { .name = "defend", .ent = base, .radius = AI_CTF_DEFEND_RADIUS };
    }
    return true;
  }

  const GameClient *carrier = G_Ai_Ctf_TeamCarrier(cl);

  if (carrier && thief) {
    *task = (GameAiTask) { .name = "hunt", .ent = thief->entity, .urgent = G_Ai_IsArmed(cl) };
    return true;
  }

  if (carrier) {
    *task = (GameAiTask) {
      .name = "escort", .ent = carrier->entity, .radius = AI_CTF_ESCORT_RADIUS
    };
    return true;
  }

  const GameEntity *flag = G_Ai_Ctf_EnemyFlag(cl);

  if (flag) {
    *task = (GameAiTask) { .name = "steal", .ent = flag };
    return true;
  }

  if (thief) {
    *task = (GameAiTask) { .name = "hunt", .ent = thief->entity, .urgent = G_Ai_IsArmed(cl) };
    return true;
  }

  return previous.Task(cl, task);
}

/**
 * @see g_ai_ctf.h
 */
void G_Ai_Ctf_Init(void) {
  static bool installed;

  if (installed) {
    return;
  }

  previous.Task = G_Ai_Task;
  G_Ai_Task = G_Ai_Task_Ctf;

  installed = true;
}
