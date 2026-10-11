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
#include "bg_pmove.h"

/**
 * @brief Distance within which an unarmed or retreating bot will still fight back.
 */
#define AI_SELF_DEFENSE_DISTANCE 512.f

/**
 * @brief A bot that should retreat disengages beyond this multiple of its self-defense distance,
 * so that an enemy near that distance does not start and end a fight every frame.
 */
#define AI_DISENGAGE_SCALE 1.25f

/**
 * @brief Returns true if the given entity is a valid (enemy, alive, boxed) target for the AI.
 */
static inline bool G_Ai_IsTargetable(const GameClient *cl, const GameEntity *other) {

  if (other->client && other->client != cl && other->solid == SOLID_BOX && !G_OnSameTeam(cl, other->client)) {
    return true;
  }

  return false;
}

/**
 * @brief Returns true if the AI can both target and see the given entity.
 */
static inline bool G_Ai_CanTarget(const GameClient *cl, const GameEntity *other) {
  return G_Ai_IsTargetable(cl, other) && G_Ai_CanSee(cl, other);
}

/**
 * @brief Range constants.
 */
typedef enum {
  RANGE_DONT_CARE = 0,
  RANGE_MELEE = 32,
  RANGE_SHORT = 128,
  RANGE_MED = 512,
  RANGE_LONG = 1024
} GameAiRange;

/**
 * @brief Classifies a distance value into a discrete range category.
 */
static GameAiRange G_Ai_GetRange(const float distance) {
  if (distance < (float) RANGE_MELEE) {
    return RANGE_MELEE;
  } else if (distance < (float) RANGE_SHORT) {
    return RANGE_SHORT;
  } else if (distance < (float) RANGE_MED) {
    return RANGE_MED;
  }

  return RANGE_LONG;
}

/**
 * @brief Picks a weapon for the AI based on its target
 */
static void G_Ai_PickWeapon(GameClient *cl) {

  cl->ai->weaponCheckTime = gameLevel.time + 250; // don't try again for a bit

  GameAiRange targRange;

  if (cl->ai->combatTarget.type == AI_GOAL_ENTITY) {
    targRange = G_Ai_GetRange(Vec3_Distance(cl->entity->s.origin, cl->ai->combatTarget.entity.ent->s.origin));
  } else {
    targRange = RANGE_DONT_CARE;
  }

  GameAiItemPick weapons[WEAPON_TOTAL];
  size_t numWeapons = 0;

  const int16_t *inventory = cl->inventory;

  for (GameItemTag t = WEAPON_FIRST; t < WEAPON_LAST; t++) {
    const GameItem *it = &gameItems[t];

    if (!inventory[t]) { // not in stock
      continue;
    }

    if (it->def.ammo) {
      const int32_t ammoHave = inventory[it->def.ammo];
      const int32_t ammoNeed = it->def.quantity;
      if (ammoHave < ammoNeed) {
        continue;
      }
    }

    // calculate weight, start with base weapon priority
    float weight = it->def.priority;

    switch (targRange) { // range bonus
      case RANGE_DONT_CARE:
        break;
      case RANGE_MELEE:
      case RANGE_SHORT:
        if (it->def.flags & WF_SHORT_RANGE) {
          weight *= 2.5f;
        } else {
          weight /= 2.5f;
        }
        break;
      case RANGE_MED:
        if (it->def.flags & WF_MED_RANGE) {
          weight *= 2.5f;
        } else {
          weight /= 2.5f;
        }
        break;
      case RANGE_LONG:
        if (it->def.flags & WF_LONG_RANGE) {
          weight *= 2.5f;
        } else {
          weight /= 2.5f;
        }
        break;
    }

    if (cl->ai->combatTarget.type == AI_GOAL_ENTITY) {
      if ((cl->ai->combatTarget.entity.ent->health < 25) &&
          (it->def.flags & WF_EXPLOSIVE)) { // bonus for explosive at low enemy health
        weight *= 1.5f;
      }
    }

    // additional penalty for long range + projectile unless explicitly long range
    if (targRange == RANGE_LONG &&
        (it->def.flags & WF_PROJECTILE) &&
        !(it->def.flags & WF_LONG_RANGE)) {
      weight /= 2.f;
    }

    // penalty for explosive weapons at short range
    if ((it->def.flags & WF_EXPLOSIVE) &&
        (targRange != RANGE_DONT_CARE && targRange <= RANGE_SHORT)) {
      weight /= 2.f;
    }

    // penalty for explosive weapons at low cl health
    if ((cl->entity->health < 25) &&
        (it->def.flags & WF_EXPLOSIVE)) {
      weight /= 2.f;
    }

    // less aware bots have noisier weapon scoring
    if (cl->ai->personality.awareness < .5f) {
      weight *= RandomRangef(.7f, 1.3f);
    }

    weapons[numWeapons++] = (GameAiItemPick) {
      .item = it,
      .weight = weight
    };
  }

  if (numWeapons <= 1) { // if we only have 1 here, we're already using it
    return;
  }

  qsort(weapons, numWeapons, sizeof(GameAiItemPick), G_Ai_CompareItems);

  const GameAiItemPick *bestWeapon = &weapons[0];

  if (cl->weapon == bestWeapon->item) {
    return;
  }

  gi.TokenizeString(va("use %s", bestWeapon->item->def.name));
  ge.ClientCommand(cl);
  cl->ai->weaponCheckTime = gameLevel.time + 300; // don't try again for a bit
  G_Ai_Debug("weapon choice: %s (%d choices)\n", bestWeapon->item->def.name, (int32_t) numWeapons);
}

/**
 * @brief Calculate a priority for the specified target.
 */
static float G_Ai_EnemyPriority(const GameClient *cl, const GameEntity *target, const bool visible) {

  float priority = 1.f;

  // closer enemies are higher priority
  const float dist = Vec3_Distance(cl->entity->s.origin, target->s.origin);
  priority += Clampf(1.f - dist / 1024.f, 0.f, 1.f) * 3.f;

  // low health enemies are higher priority
  if (target->health < 50) {
    priority += 2.f;
  }

#if defined(G_CTF)
  // flag carriers are highest priority
  if (target->client) {
    const int16_t *inventory = target->client->inventory;
    for (GameItemTag t = FLAG_FIRST; t < FLAG_LAST; t++) {
      if (inventory[t]) {
        priority += 5.f;
        break;
      }
    }
  }
#endif

  return priority;
}

/**
 * @brief Figure out if we should chase the enemy and close in on them.
 */
static bool G_Ai_ChaseEnemy(const GameClient *cl, const GameEntity *target) {

  if (target->solid == SOLID_DEAD || (target->serverFlags & SVF_NO_CLIENT)) {
    return false;
  }

  if (!target->client) {
    return false;
  }

  // base chance is our weapon's weight
  const GameItem *const weapon = cl->weapon;
  float chance = weapon->def.priority;

  // if they're low health, higher chance
  if (target->health < 50) {
    chance *= 1.5f;
  }

#if defined(G_CTF)
  // if they have a flag, higher chance
  const int16_t *inventory = target->client->inventory;

  for (GameItemTag t = FLAG_FIRST; t < FLAG_LAST; t++) {
    if (inventory[t]) {
      chance *= 2.0f;
      break;
    }
  }
#endif

  // aggressive bots are more willing to chase
  chance *= Mixf(.6f, 1.4f, cl->ai->personality.aggression);

  return Randomf() < chance;
}

/**
 * @brief The time in milliseconds that a bot keeps an enemy as its target after losing sight of it,
 * so that a brief occlusion does not swing its view away and back.
 */
#define AI_TARGET_MEMORY_MILLIS 750

/**
 * @brief The time in milliseconds after a bot last saw its target within which it may fire.
 */
#define AI_TARGET_SEEN_MILLIS 100

/**
 * @brief Funcgoal that controls the AI's lust for blood
 */
uint32_t G_Ai_Hunt(GameClient *cl, PMoveCmd *cmd) {

  if (cl->entity->solid == SOLID_DEAD) {
    return 1;
  }


  if (g_aiNoTarget->integer || g_aiNodeDev->integer) {

    if (cl->ai->combatTarget.type == AI_GOAL_ENTITY) {
      G_Ai_ClearGoal(&cl->ai->combatTarget);
    }

    if (cl->ai->moveTarget.type == AI_GOAL_ENTITY && cl->ai->moveTarget.entity.ent->client) {
      G_Ai_ClearGoal(&cl->ai->moveTarget);
    }

    return 1;
  }

  // disengage if we're unarmed or low health (except self-defense)
  const bool armed = G_Ai_IsArmed(cl);
  const bool retreat = G_Ai_ShouldRetreat(cl);

  // keep fighting if they're right on top of us (aggressive bots fight at longer range)
  const float defenseDist = AI_SELF_DEFENSE_DISTANCE * Mixf(.5f, 1.5f, cl->ai->personality.aggression);

  if (!armed || retreat) {

    if (cl->ai->combatTarget.type == AI_GOAL_ENTITY) {
      const float dist = Vec3_Distance(cl->entity->s.origin,
          cl->ai->combatTarget.entity.ent->s.origin);

      if (dist > defenseDist * AI_DISENGAGE_SCALE) {

        if (cl->ai->moveTarget.type == AI_GOAL_ENTITY &&
          cl->ai->moveTarget.entity.ent == cl->ai->combatTarget.entity.ent) {
          G_Ai_ClearGoal(&cl->ai->moveTarget);
        }

        G_Ai_ClearGoal(&cl->ai->combatTarget);
        G_Ai_Debug("%s: disengaging\n", !armed ? "Unarmed" : "Retreating");
        return 50;
      }
    } else {
      // don't acquire new targets; focus on items
      return 50;
    }
  }

  // see if we're already hunting
  if (cl->ai->combatTarget.type == AI_GOAL_ENTITY) {

    const GameEntity *enemy = cl->ai->combatTarget.entity.ent;

    if (G_Ai_CanTarget(cl, enemy)) {
      cl->ai->combatTargetSeenTime = gameLevel.time;
    } else if (!G_Ai_IsTargetable(cl, enemy) || gameLevel.time - cl->ai->combatTargetSeenTime > AI_TARGET_MEMORY_MILLIS) {

      // enemy dead/out of LOS/disconnected; chase them!
      if (!G_Ai_IsSeekingItem(cl) && !G_Ai_IsOutgunned(cl, enemy) && G_Ai_ChaseEnemy(cl, cl->ai->combatTarget.entity.ent)) {

        const Vec3 whereTo = cl->ai->combatTarget.entity.ent->s.origin;
        
        const GameAiNodeId closest = G_Ai_Node_FindClosest(cl->entity->s.origin, 128.f, true, true);
        const GameAiNodeId closestToTarget = G_Ai_Node_FindClosest(whereTo, 128.f, true, true);
        Vector *path = G_Ai_Node_FindPath(cl, closest, closestToTarget, G_Ai_Node_Heuristic, 0.f, NULL);

        if (path) {
          G_Ai_SetPathGoal(cl, &cl->ai->moveTarget, 0.7f, path, cl->ai->combatTarget.entity.ent);
          release(path);
          G_Ai_Debug("Enemy out of sight & chasing\n");
        }
      }
      
      // if we had a move target still set on hunting that entity, reset to wander.
      if (cl->ai->moveTarget.type == AI_GOAL_ENTITY && cl->ai->moveTarget.entity.ent == cl->ai->combatTarget.entity.ent) {
        G_Ai_ClearGoal(&cl->ai->moveTarget);
      }

      G_Ai_ClearGoal(&cl->ai->combatTarget);
    }
  }

  // scan for the best visible enemy, even if we already have a target
  GameEntity *bestEnemy = NULL;
  float bestPriority = 0.f;

  G_ForEachEntity(ent, {
    if (G_Ai_CanTarget(cl, ent)) {

      if (G_Ai_IsOutgunned(cl, ent) && Vec3_Distance(cl->entity->s.origin, ent->s.origin) > defenseDist) {
        continue;
      }

      const float priority = G_Ai_EnemyPriority(cl, ent, true);
      if (priority > bestPriority) {
        bestPriority = priority;
        bestEnemy = ent;
      }
    }
  });

  // switch targets if we found a significantly better one
  if (bestEnemy) {
    const float currentPriority = cl->ai->combatTarget.type == AI_GOAL_ENTITY && cl->ai->combatTargetSeenTime == gameLevel.time
        ? G_Ai_EnemyPriority(cl, cl->ai->combatTarget.entity.ent, true)
        : 0.f;

    if (bestPriority > currentPriority + 1.f || cl->ai->combatTarget.type != AI_GOAL_ENTITY) {

      G_Ai_SetEntityGoal(cl, &cl->ai->combatTarget, bestPriority, bestEnemy);
      cl->ai->combatTargetSeenTime = gameLevel.time;

      const Vec3 eyeOrigin = Vec3_Add(cl->entity->s.origin, cl->ps.pmState.viewOffset);
      const Vec3 toEnemy = Vec3_Normalize(Vec3_Subtract(Box3_Center(bestEnemy->absBounds), eyeOrigin));
      const float flick = Degrees(acosf(Clampf(Vec3_Dot(cl->forward, toEnemy), -1.f, 1.f))) * Mixf(.25f, .08f, cl->ai->personality.skill);
      const float direction = RandomRadian();

      cl->ai->perceivedVelocity = Vec3_Zero();
      cl->ai->aimError = MakeVec2(sinf(direction) * flick * .6f, cosf(direction) * flick);

      const float leadSpread = Mixf(.3f, .05f, cl->ai->personality.skill);
      cl->ai->leadError = RandomRangef(-leadSpread, leadSpread);

      G_Ai_PickWeapon(cl);

      // aggressive bots prefer close combat; cautious bots flank/wander
      if (cl->ai->personality.aggression > .6f) {
        cl->ai->combatTarget.entity.combatType = Randomb() ? AI_COMBAT_CLOSE : RandomRangei(AI_COMBAT_CLOSE, AI_COMBAT_TOTAL);
      } else if (cl->ai->personality.aggression < .3f) {
        cl->ai->combatTarget.entity.combatType = Randomb() ? AI_COMBAT_FLANK : AI_COMBAT_WANDER;
      } else {
        cl->ai->combatTarget.entity.combatType = RandomRangei(AI_COMBAT_CLOSE, AI_COMBAT_TOTAL);
      }

      // skilled bots react faster (100-400ms vs 500-1200ms)
      const uint32_t lockMin = (uint32_t) Mixf(500.f, 100.f, cl->ai->personality.skill);
      const uint32_t lockMax = (uint32_t) Mixf(1200.f, 400.f, cl->ai->personality.skill);
      cl->ai->combatTarget.entity.lockOnTime = gameLevel.time + RandomRangeu(lockMin, lockMax);

      if (cl->ai->combatTarget.entity.combatType == AI_COMBAT_FLANK) {
        cl->ai->combatTarget.entity.flankAngle = Randomb() ? -90 : 90;
      }
    }
  }

  // we have somebody to kill; go get'em!
  if (cl->ai->combatTarget.type == AI_GOAL_ENTITY && !G_Ai_GoalHasEntity(&cl->ai->moveTarget, cl->ai->combatTarget.entity.ent) && !G_Ai_IsSeekingItem(cl) && G_Ai_ChaseEnemy(cl, cl->ai->combatTarget.entity.ent)) {

    const GameEntity *enemy = cl->ai->combatTarget.entity.ent;
    const float dist = Vec3_Distance(cl->entity->s.origin, enemy->s.origin);

    // try to find a safe path to the enemy via navigation nodes
    const GameAiNodeId myNode = G_Ai_Node_FindClosest(cl->entity->s.origin, 128.f, true, true);
    const GameAiNodeId enemyNode = G_Ai_Node_FindClosest(enemy->s.origin, 128.f, true, true);
    bool pathed = false;

    if (myNode != AI_NODE_INVALID && enemyNode != AI_NODE_INVALID && myNode != enemyNode) {
      Vector *path = G_Ai_Node_FindPath(cl, myNode, enemyNode, G_Ai_Node_Heuristic, 0.f, NULL);
      if (path) {
        G_Ai_SetPathGoal(cl, &cl->ai->moveTarget, 0.7f, path, enemy);
        release(path);
        pathed = true;
        G_Ai_Debug("Chasing enemy via path\n");
      }
    }

    // fall back to direct movement only at close range
    if (!pathed && dist < 256.f) {
      G_Ai_CopyGoal(&cl->ai->combatTarget, &cl->ai->moveTarget);
      G_Ai_Debug("Chasing enemy directly (close range)\n");
    }
  }

  // still have an enemy
  if (cl->ai->combatTarget.type == AI_GOAL_ENTITY) {
    return 50;
  }

  return 100;
}

/**
 * @brief The projectile speed that a bot assumes for a weapon with no ballistics type, such as
 * hand grenades, whose speed depends on how long they are held.
 */
#define AI_PROJECTILE_SPEED 1050

/**
 * @return The projectile speed of the given weapon.
 */
static float G_Ai_ProjectileSpeed(const GameItem *weapon) {

  const char *classname = weapon->def.classname;
  const char *name = classname + (Str_HasPrefix(classname, "weapon_") ? strlen("weapon_") : 0);

  return G_Ballistics_Speed(name) ?: AI_PROJECTILE_SPEED;
}

/**
 * @brief The upward speeds that launched and thrown grenades receive on top of their launch speed.
 */
#define AI_GRENADE_LOFT 100.f
#define AI_HAND_GRENADE_LOFT 200.f

/**
 * @brief The range of distances at which a bot lobs grenades at a spot, beyond its own splash.
 */
#define AI_SPAM_MIN_DISTANCE 256.f
#define AI_SPAM_MAX_DISTANCE 1000.f

/**
 * @brief The largest size of a brush entity that a bot will spam, so that it does not lob
 * grenades at the center of a map-wide trigger.
 */
#define AI_SPAM_MAX_SIZE 512.f

/**
 * @brief The grenades that a bot keeps in reserve for combat.
 */
#define AI_SPAM_RESERVE 5

/**
 * @brief The longest time in milliseconds that a burst of grenade spam lasts.
 */
#define AI_SPAM_BURST_MILLIS 3000

/**
 * @brief The view error in degrees within which a bot releases a grenade at its spot.
 */
#define AI_SPAM_AIM_DEGREES 3.f

/**
 * @return The grenade weapon that the bot would spam with, or NULL if it has none to spare.
 */
static const GameItem *G_Ai_SpamWeapon(const GameClient *cl) {

  static const GameItemTag tags[] = {
    WEAPON_GRENADE_LAUNCHER,
    WEAPON_QUAKE_GRENADE_LAUNCHER,
    WEAPON_HAND_GRENADE,
  };

  for (size_t i = 0; i < lengthof(tags); i++) {
    const GameItem *weapon = &gameItems[tags[i]];

    if (cl->inventory[tags[i]] && cl->inventory[weapon->def.ammo] > AI_SPAM_RESERVE) {
      return weapon;
    }
  }

  return NULL;
}

/**
 * @brief Resolves the view angles that lob a grenade from the bot's eye to the given point.
 * @param holdMillis For hand grenades, the time to hold the grenade so that it flies far enough.
 * @return True if a grenade can reach the point.
 */
static bool G_Ai_SpamAngles(const GameClient *cl, const GameItem *weapon, const Vec3 point, Vec3 *angles, uint32_t *holdMillis) {

  const Vec3 eye = Vec3_Add(cl->entity->s.origin, cl->ps.pmState.viewOffset);
  const float gravity = G_LevelGravity();
  const float distance = Vec2_Distance(Vec3_XY(eye), Vec3_XY(point));
  const float height = point.z - eye.z;

  if (distance < 1.f || gravity <= 0.f) {
    return false;
  }

  float speed, loft;
  if (weapon->def.flags & WF_TIMED) {
    loft = AI_HAND_GRENADE_LOFT;
    speed = Clampf(sqrtf(gravity * distance) * 1.2f, 500.f, 1200.f);
    *holdMillis = (uint32_t) (speed / 500.f * 1000.f);
  } else {
    loft = AI_GRENADE_LOFT;
    speed = G_Ai_ProjectileSpeed(weapon);
    *holdMillis = 0;
  }

  const float launch = sqrtf(speed * speed + loft * loft);
  const float launchSquared = launch * launch;
  const float discriminant = launchSquared * launchSquared - gravity * (gravity * distance * distance + 2.f * height * launchSquared);

  if (discriminant < 0.f) {
    return false;
  }

  const float elevation = atanf((launchSquared - sqrtf(discriminant)) / (gravity * distance)) - atan2f(loft, speed);
  const Vec3 dir = Vec3_Normalize(Vec3_Subtract(point, eye));

  *angles = MakeVec3(-Degrees(elevation), Vec3_Euler(dir).y, 0.f);
  return true;
}

/**
 * @return True if the entity is a spot where players gather or pass, and so worth spamming.
 */
static bool G_Ai_IsSpamSpot(const GameEntity *ent) {

  const char *classname = ent->classname;

  if (classname == NULL) {
    return false;
  }

  if (Str_HasPrefix(classname, "info_player_")) {
    return Str_Compare(classname, "info_player_intermission") != 0;
  }

  return Str_HasPrefix(classname, "func_") ||
         Str_HasPrefix(classname, "trigger_") ||
         !Str_Compare(classname, "misc_teleporter_dest");
}

/**
 * @brief Picks a random visible spot within grenade range for the bot to spam.
 * @return True if a spot was found.
 */
static bool G_Ai_SpamPoint(const GameClient *cl, const GameItem *weapon, Vec3 *point) {

  const Vec3 eye = Vec3_Add(cl->entity->s.origin, cl->ps.pmState.viewOffset);
  uint32_t count = 0;

  G_ForEachEntity(ent, {
    if (!G_Ai_IsSpamSpot(ent)) {
      continue;
    }

    const Vec3 size = Box3_Size(ent->absBounds);
    if (size.x > AI_SPAM_MAX_SIZE || size.y > AI_SPAM_MAX_SIZE || size.z > AI_SPAM_MAX_SIZE) {
      continue;
    }

    const Vec3 spot = Vec3_Equal(size, Vec3_Zero()) ? ent->s.origin : Box3_Center(ent->absBounds);
    const float distance = Vec3_Distance(eye, spot);
    if (distance < AI_SPAM_MIN_DISTANCE || distance > AI_SPAM_MAX_DISTANCE) {
      continue;
    }

    const CollisionTrace tr = gi.Trace(eye, spot, Box3_Zero(), cl->entity, CONTENTS_MASK_SOLID);
    if (tr.fraction < 1.f && tr.ent != ent) {
      continue;
    }

    Vec3 angles;
    uint32_t holdMillis;
    if (!G_Ai_SpamAngles(cl, weapon, spot, &angles, &holdMillis)) {
      continue;
    }

    if (Randomf() * ++count < 1.f) {
      *point = spot;
    }
  });

  return count > 0;
}

/**
 * @return True if the bot is in a burst of grenade spam.
 */
static inline bool G_Ai_IsSpamming(const GameClient *cl) {
  return cl->ai->spamAmmo > 0;
}

/**
 * @brief Ends the bot's burst of grenade spam, and schedules the next by its aggression.
 */
static void G_Ai_EndSpam(GameClient *cl) {

  const float cooldown = Mixf(15000.f, 5000.f, cl->ai->personality.aggression) * RandomRangef(.75f, 1.25f);

  cl->ai->spamAmmo = 0;
  cl->ai->spamTime = gameLevel.time + (uint32_t) cooldown;
  cl->ai->weaponCheckTime = 0;
}

/**
 * @return True if the bot moves in a way that leaves its view free to spam, which is not the case
 * while it trick-jumps, climbs a ladder or swims.
 */
static bool G_Ai_CanSpam(const GameClient *cl) {

  if (cl->ai->moveTarget.type == AI_GOAL_PATH && cl->ai->moveTarget.path.trickJump) {
    return false;
  }

  if (cl->ps.pmState.flags & PMF_ON_LADDER) {
    return false;
  }

  return cl->entity->waterLevel < WATER_WAIST;
}

/**
 * @brief Lobs grenades at spots where players gather while the bot has no combat target.
 */
static void G_Ai_Weaponry_spam(GameClient *cl, PMoveCmd *cmd) {

  const GameItem *weapon = G_Ai_SpamWeapon(cl);

  if (!G_Ai_CanSpam(cl)) {
    if (G_Ai_IsSpamming(cl)) {
      G_Ai_EndSpam(cl);
    }
    return;
  }

  if (!G_Ai_IsSpamming(cl)) {
    if (cl->ai->spamTime > gameLevel.time) {
      return;
    }

    G_Ai_EndSpam(cl);

    if (weapon == NULL || !G_Ai_SpamPoint(cl, weapon, &cl->ai->spamPoint)) {
      return;
    }

    cl->ai->spamAmmo = cl->inventory[weapon->def.ammo] - RandomRangei(1, 4);
    cl->ai->spamTime = gameLevel.time + AI_SPAM_BURST_MILLIS;
    G_Ai_Debug("spamming %s\n", weapon->def.name);
  }

  const bool expired = gameLevel.time > cl->ai->spamTime && !cl->grenadeHoldTime;

  if (weapon == NULL || expired || cl->inventory[weapon->def.ammo] <= cl->ai->spamAmmo) {
    G_Ai_EndSpam(cl);
    return;
  }

  if (cl->weapon != weapon) {
    if (cl->ai->weaponCheckTime < gameLevel.time) {
      gi.TokenizeString(va("use %s", weapon->def.name));
      ge.ClientCommand(cl);
      cl->ai->weaponCheckTime = gameLevel.time + 300;
    }
    return;
  }

  Vec3 angles;
  uint32_t holdMillis;
  if (!G_Ai_SpamAngles(cl, weapon, cl->ai->spamPoint, &angles, &holdMillis)) {
    G_Ai_EndSpam(cl);
    return;
  }

  if (cl->grenadeHoldTime) {
    if (gameLevel.time - cl->grenadeHoldTime < holdMillis) {
      cmd->buttons |= BUTTON_ATTACK;
    }
    return;
  }

  const Vec3 view = cl->angles;
  if (fabsf(G_Ai_AngleDelta(view.x, angles.x)) < AI_SPAM_AIM_DEGREES &&
      fabsf(G_Ai_AngleDelta(view.y, angles.y)) < AI_SPAM_AIM_DEGREES) {
    cmd->buttons |= BUTTON_ATTACK;
  }
}

/**
 * @brief Funcgoal that controls the AI's weaponry.
 */
uint32_t G_Ai_Weaponry(GameClient *cl, PMoveCmd *cmd) {

  // if we're dead, just keep clicking so we respawn.
  if (cl->entity->dead) {

    if (gameLevel.frameNum & 1) {
      cmd->buttons = BUTTON_ATTACK;
    }
    return 1;
  }


  if (cl->ai->combatTarget.type == AI_GOAL_ENTITY) {
    if (G_Ai_IsSpamming(cl)) {
      G_Ai_EndSpam(cl);
    }
  } else {
    G_Ai_Weaponry_spam(cl, cmd);
  }

  if (!G_Ai_IsSpamming(cl) && cl->ai->weaponCheckTime < gameLevel.time) { // check for a new weapon every once in a while
    G_Ai_PickWeapon(cl);
  }

  // we're alive - if we're aiming at an enemy, start-a-firin
  if (cl->ai->combatTarget.type == AI_GOAL_ENTITY) {
    if (cl->ai->combatTarget.entity.lockOnTime < gameLevel.time &&
        gameLevel.time - cl->ai->combatTargetSeenTime <= AI_TARGET_SEEN_MILLIS) {

      const Vec3 eyeOrigin = Vec3_Add(cl->entity->s.origin, cl->ps.pmState.viewOffset);
      const Vec3 toEnemy = Vec3_Normalize(Vec3_Subtract(
          Box3_Center(cl->ai->combatTarget.entity.ent->absBounds), eyeOrigin));

      // skilled bots fire with tighter aim cone (10° to 25°)
      const float fireCone = Mixf(25.f, 10.f, cl->ai->personality.skill);
      if (Vec3_Dot(cl->forward, toEnemy) > cosf(Radians(fireCone))) {
        const uint32_t grenadeHoldTime = cl->grenadeHoldTime;
        if (grenadeHoldTime) {
          if (gameLevel.time - grenadeHoldTime < RandomRangeu(1500, 2500)) {
            cmd->buttons |= BUTTON_ATTACK;
          }
        } else {
          cmd->buttons |= BUTTON_ATTACK;
        }
      }
    }
  }

  return 1;
}

/**
 * @brief Funcgoal that controls the AI's crouch/jumping while hunting.
 */
uint32_t G_Ai_Acrobatics(GameClient *cl, PMoveCmd *cmd) {

  if (cl->entity->solid == SOLID_DEAD) {
    return 1;
  }


  if (cl->ai->combatTarget.type != AI_GOAL_ENTITY) {
    return 200;
  }

  // do some acrobatics (aggressive bots dodge more)
  const uint32_t crouchFreq = (uint32_t) Mixf(48.f, 20.f, cl->ai->personality.aggression);
  const uint32_t jumpFreq = (uint32_t) Mixf(120.f, 50.f, cl->ai->personality.aggression);

  if (cl->entity->ground.ent) {

    if (cl->ps.pmState.flags & PMF_DUCKED) {

      if ((RandomRangeu(0, crouchFreq)) == 0) { // uncrouch eventually
        cmd->up = 0;
      } else {
        cmd->up = -PM_SPEED_JUMP;
      }
    } else {

      if ((RandomRangeu(0, crouchFreq)) == 0) { // randomly crouch
        cmd->up = -PM_SPEED_JUMP;
      } else if ((RandomRangeu(0, jumpFreq)) == 0 &&
                 !G_Ai_FallsToVoid(cl, cl->entity->s.origin, G_Ai_JumpVelocity(cl))) { // randomly pop
        cmd->up = PM_SPEED_JUMP;
      }
    }
  } else {

    cmd->up = 0;
  }

  return 1;
}

/**
 * @brief The interval in seconds over which the rate of change of the aim angles is measured.
 */
#define AI_AIM_RATE_SECONDS .05f

/**
 * @brief Advances the aim error, a random walk toward zero that widens with the target's angular
 * rate and narrows with skill.
 * @param rate The angular rate of the aim point in degrees per second.
 */
static void G_Ai_UpdateAimError(GameClient *cl, const Vec3 rate, const float seconds) {

  const float skill = cl->ai->personality.skill;
  const float correction = .35f;

  const float angularSpeed = sqrtf(rate.x * rate.x + rate.y * rate.y);
  const float deviation = Mixf(4.f, .4f, skill) + Mixf(.1f, .015f, skill) * angularSpeed;
  const float diffusion = deviation * sqrtf(2.f * seconds / correction);

  Vec2 *error = &cl->ai->aimError;

  for (int32_t i = 0; i < 2; i++) {
    const float noise = (Randomf() + Randomf() + Randomf() - 1.5f) * 2.f;
    const float scale = i == 0 ? .6f : 1.f;

    error->xy[i] += -error->xy[i] * seconds / correction + noise * diffusion * scale;
  }
}

/**
 * @brief Points the view at the grenade spam spot while the bot is in a burst of spam.
 */
void G_Ai_SpamViewAngles(const GameClient *cl, Vec3 *angles) {

  if (G_Ai_IsSpamming(cl) && cl->weapon == G_Ai_SpamWeapon(cl)) {
    uint32_t holdMillis;
    G_Ai_SpamAngles(cl, cl->weapon, cl->ai->spamPoint, angles, &holdMillis);
  }
}

/**
 * @brief Resolves the view angles and their rate of change that aim at the combat target. The
 * aim point lags the target's changes in velocity by the bot's reaction time, plus the aim error.
 */
void G_Ai_CombatAngles(GameClient *cl, const float seconds, Vec3 *angles, Vec3 *rate) {

  const GameEntity *enemy = cl->ai->combatTarget.entity.ent;
  const Vec3 eyeOrigin = Vec3_Add(cl->entity->s.origin, cl->ps.pmState.viewOffset);
  const Vec3 enemyCenter = Box3_Center(enemy->absBounds);

  const float reaction = Mixf(.25f, .05f, cl->ai->personality.skill);
  cl->ai->perceivedVelocity = Vec3_Mix(cl->ai->perceivedVelocity, enemy->velocity, 1.f - expf(-seconds / reaction));

  Vec3 aimPoint = Vec3_Fmaf(enemyCenter, reaction, Vec3_Subtract(cl->ai->perceivedVelocity, enemy->velocity));

  const GameItem *const weapon = cl->weapon;

  if (weapon->def.flags & WF_PROJECTILE) {
    const float dist = Vec3_Distance(eyeOrigin, enemyCenter);
    const float speed = G_Ai_ProjectileSpeed(weapon);
    aimPoint = Vec3_Fmaf(aimPoint, dist / (speed * (1.f + cl->ai->leadError)), cl->ai->perceivedVelocity);
  }

  *angles = Vec3_Euler(Vec3_Normalize(Vec3_Subtract(aimPoint, eyeOrigin)));

  const Vec3 relativeVelocity = Vec3_Subtract(cl->ai->perceivedVelocity, cl->entity->velocity);
  const Vec3 nextAimPoint = Vec3_Fmaf(aimPoint, AI_AIM_RATE_SECONDS, relativeVelocity);
  const Vec3 nextAngles = Vec3_Euler(Vec3_Normalize(Vec3_Subtract(nextAimPoint, eyeOrigin)));

  for (int32_t i = 0; i < 2; i++) {
    rate->xyz[i] = G_Ai_AngleDelta(angles->xyz[i], nextAngles.xyz[i]) / AI_AIM_RATE_SECONDS;
  }

  G_Ai_UpdateAimError(cl, *rate, seconds);

  angles->x = Clampf(G_Ai_AngleDelta(0.f, angles->x + cl->ai->aimError.x), -90.f, 90.f);
  angles->y += cl->ai->aimError.y;
}
