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

Cvar *g_aiNoTarget;
Cvar *g_aiNodeDev;

/**
 * @brief Minimum weapon priority to be considered "armed" for combat.
 * Blaster (0.10) is below this threshold; Shotgun (0.15) and above are armed.
 */
#define AI_ARMED_PRIORITY 0.15f

/**
 * @brief Health threshold below which bots disengage to seek health/armor.
 */
#define AI_RETREAT_HEALTH 40

/**
 * @brief Distance within which an unarmed or retreating bot will still fight back.
 */
#define AI_SELF_DEFENSE_DISTANCE 512.f

/**
 * @return True if the bot has at least a Shotgun or better.
 * Blaster-only bots are considered unarmed and will prioritize weapon pickups.
 */
static bool G_Ai_IsArmed(const GameClient *cl) {

  const float threshold = AI_ARMED_PRIORITY * Mixf(1.25f, .6f, cl->ai->personality.aggression);

  for (GameItemTag t = WEAPON_FIRST; t < WEAPON_LAST; t++) {
    const GameItem *it = &gameItems[t];
    if (cl->inventory[t] && it->def.priority >= threshold) {
      return true;
    }
  }

  return false;
}

/**
 * @return True if the bot should disengage from combat to seek health/armor.
 * Aggressive bots retreat at lower health; cautious bots retreat earlier.
 * Bots also flee from enemies carrying dangerous powerups.
 */
static bool G_Ai_ShouldRetreat(const GameClient *cl) {
  const int32_t threshold = (int32_t)(AI_RETREAT_HEALTH * Mixf(1.5f, .5f, cl->ai->personality.aggression));
  if (cl->entity->health < threshold) {
    return true;
  }

  if (cl->ai->combatTarget.type == AI_GOAL_ENTITY) {
    const GameEntity *target = cl->ai->combatTarget.entity.ent;

    if (target->s.effects & EF_INVULNERABILITY) {
      return true; // no point fighting an invulnerable enemy
    }

    // all but the most aggressive bots flee from quad damage
    if ((target->s.effects & EF_QUAD) && cl->ai->personality.aggression < 0.8f) {
      return true;
    }
  }

  return false;
}

/**
 * @return True if nothing solid lies between the AI's eyes and the entity.
 */
static bool G_Ai_HasLineOfSight(const GameClient *cl, const GameEntity *other) {

  const Vec3 eyeOrigin = Vec3_Add(cl->entity->s.origin, cl->ps.pmState.viewOffset);
  const CollisionTrace tr = gi.Trace(eyeOrigin, other->s.origin, Box3_Zero(), cl->entity, CONTENTS_MASK_CLIP_PROJECTILE);

  if (tr.ent == other) {
    return true;
  }

  return Box3_ContainsPoint(Box3_Expand(other->absBounds, 1.f), tr.end);
}

/**
 * @brief Returns true if the AI client has line of sight to the target entity.
 */
static bool G_Ai_CanSee(const GameClient *cl, const GameEntity *other) {

  // invisible enemies are only detectable within a skill-dependent range
  if (other->s.effects & EF_INVISIBILITY) {
    const float dist = Vec3_Distance(cl->entity->s.origin, other->s.origin);
    const float detectRange = Mixf(256.f, 512.f, cl->ai->personality.skill);
    if (dist > detectRange) {
      return false;
    }
  }

  // see if we're even facing the object

  const Vec3 eyeOrigin = Vec3_Add(cl->entity->s.origin, cl->ps.pmState.viewOffset);
  const Vec3 dir = Vec3_Normalize(Vec3_Subtract(other->s.origin, eyeOrigin));

  float dot = Vec3_Dot(cl->forward, dir);

  if (dot < 0.1f) {
    return false;
  }

  return G_Ai_HasLineOfSight(cl, other);
}

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
 * @brief The max distance we'll try to hunt an item at.
 */
#define AI_MAX_ITEM_DISTANCE 768.f

/**
 * @brief Candidate item considered for AI pickup, weighted by desirability.
 */
typedef struct {
  const GameEntity *entity;
  const GameItem *item;
  float weight;
} GameAiItemPick;

/**
 * @brief Comparison function for sorting item pick candidates by descending weight.
 */
static int32_t G_Ai_CompareItems(const void *a, const void *b) {

  const GameAiItemPick *w0 = (const GameAiItemPick *) a;
  const GameAiItemPick *w1 = (const GameAiItemPick *) b;

  return SignOf(w1->weight - w0->weight);
}

static Order G_Ai_CompareItemsOrder(const ident a, const ident b) {
  const int32_t cmp = G_Ai_CompareItems(a, b);
  return cmp < 0 ? OrderAscending : cmp > 0 ? OrderDescending : OrderSame;
}

static inline int64_t G_Ai_Microseconds(void) {
  return (int64_t) gameLevel.time * 1000;
}

/**
 * @return True if the item decides who controls the map: a powerup, mega health or body armor.
 */
static bool G_Ai_IsMapControlItem(const GameItem *item) {

  switch (item->def.type) {
    case ITEM_TYPE_POWERUP:
      return true;
    case ITEM_TYPE_HEALTH:
      return item->def.tag == HEALTH_MEGA || item->def.tag == HEALTH_QUAKE_MEGA;
    case ITEM_TYPE_ARMOR:
      return item->def.tag == ARMOR_BODY || item->def.tag == ARMOR_QUAKE_BODY;
    default:
      return false;
  }
}

/**
 * @return True if the entity is a map control item, or a dropped weapon that the AI lacks. Bots
 * go after these above all else, even in combat.
 */
static bool G_Ai_IsCovetedItem(const GameClient *cl, const GameEntity *ent) {

  if (!ent->item) {
    return false;
  }

  if (G_Ai_IsMapControlItem(ent->item)) {
    return true;
  }

  return (ent->spawnFlags & SF_ITEM_DROPPED) && ent->item->def.type == ITEM_TYPE_WEAPON && !cl->inventory[ent->item->def.tag];
}

/**
 * @return True if the AI's move target is a coveted item.
 */
static bool G_Ai_IsSeekingCovetedItem(const GameClient *cl) {

  const GameAiGoal *goal = &cl->ai->moveTarget;

  if (goal->type == AI_GOAL_ENTITY) {
    return G_Ai_IsCovetedItem(cl, goal->entity.ent);
  }

  if (goal->type == AI_GOAL_PATH && goal->path.pathTarget) {
    return G_Ai_IsCovetedItem(cl, goal->path.pathTarget);
  }

  return false;
}

#define AI_ITEM_UNREACHABLE -1.0

/**
 * @brief Returns the distance to a nearby item, or `AI_ITEM_UNREACHABLE` if beyond awareness range.
 */
static float G_Ai_ItemReachable(const GameClient *cl, const GameEntity *other) {

  const float dist = Vec3_Distance(cl->entity->s.origin, other->s.origin);

  // aware bots spot items from farther away (512 to 1024)
  float range = AI_MAX_ITEM_DISTANCE * Mixf(.67f, 1.33f, cl->ai->personality.awareness);

  if (G_Ai_IsCovetedItem(cl, other)) {
    range = AI_MAX_ITEM_DISTANCE * 3.f;
  }

  if (dist > range) {
    return AI_ITEM_UNREACHABLE;
  }

  return dist;
}

static inline void G_Ai_BackupPath(GameAi *ai) {

  if (!ai->backupMoveTarget.type && ai->moveTarget.type == AI_GOAL_PATH) {
    G_Ai_CopyGoal(&ai->moveTarget, &ai->backupMoveTarget);
    G_Ai_Debug("Backing up main path; new temporary path incoming\n");
  }
}

static inline void G_Ai_RestorePath(const GameClient *cl, GameAi *ai) {

  if (ai->backupMoveTarget.type == AI_GOAL_PATH) {
    // generate a new path to the old target, because we might have gotten a bit out
    // of sync with moving to the item
    const GameAiNodeId src = G_Ai_Node_FindClosest(cl->entity->s.origin, 512.f, true, true);
    const GameAiNodeId dest = VectorValue(ai->backupMoveTarget.path.path, GameAiNodeId, ai->backupMoveTarget.path.path->count - 1);
    Vector *path = G_Ai_Node_FindPath(cl, src, dest, G_Ai_Node_Heuristic, NULL);

    if (!path) {
      G_Ai_ClearGoal(&ai->moveTarget);
    } else {
      G_Ai_SetPathGoal(cl, &ai->moveTarget, ai->backupMoveTarget.priority, path, ai->backupMoveTarget.path.pathTarget);
      release(path);
    }
  } else {
    G_Ai_ClearGoal(&ai->moveTarget);
    G_Ai_Debug("No main path?\n");
  }

  G_Ai_ClearGoal(&ai->backupMoveTarget);
  G_Ai_Debug("Returning to main path\n");

  // no matter what, clear long range func goal so we can try it immediately
  ai->funcGoalNextThinks[AI_FUNC_GOAL_LONGRANGE] = 0;
}

/**
 * @brief Seek for items if we're not doing anything better.
 */
static uint32_t G_Ai_FindItems(GameClient *cl, PMoveCmd *cmd) {

  if (cl->entity->solid == SOLID_DEAD) {
    return 1;
  }


  // if we got stuck, don't hunt for items for a little bit
  if (cl->ai->reacquireTime > gameLevel.time) {
    return cl->ai->reacquireTime - gameLevel.time; 
  }

  // skip item seeking if we're in the air; if we're armed, healthy, and fighting, only a coveted item will do
  if (!cl->entity->ground.ent) {
    return 50;
  }

  bool onlyCovetedItems = cl->ai->combatTarget.type && G_Ai_IsArmed(cl) && !G_Ai_ShouldRetreat(cl);

  // we're not attacking, so we probably care about items.
  if (cl->ai->moveTarget.type == AI_GOAL_ENTITY || cl->ai->moveTarget.type == AI_GOAL_PATH) {
    const GameEntity *target = (cl->ai->moveTarget.type == AI_GOAL_ENTITY) ? cl->ai->moveTarget.entity.ent : cl->ai->moveTarget.path.pathTarget;

    // check to see if the thing we are moving to has been taken
    if (target && target->item) {

      if (target->solid != SOLID_TRIGGER ||
        !G_Ai_CanPickup(cl, target)) {

        G_Ai_ClearGoal(&cl->ai->moveTarget);

        // if we had a backup, return to it now
        if (cl->ai->backupMoveTarget.type) {

          G_Ai_RestorePath(cl, cl->ai);
        }
      // still a good goal
      } else if (G_Ai_IsSeekingCovetedItem(cl)) {
        return 50;
      } else {
        onlyCovetedItems = true;
      }
    }
  }

  // we have nothing to do, start looking for a new one
  Vector *itemsVisible = $(alloc(Vector), initWithSize, sizeof(GameAiItemPick));

  G_ForEachEntity(ent, {
    if (ent->s.solid != SOLID_TRIGGER) {
      continue;
    }

    const GameItem *item = ent->item;

    if (!item) {
      continue;
    }

    // we're already pathing to this item, so ignore it in our short range goal finding
    if (cl->ai->moveTarget.type == AI_GOAL_PATH && cl->ai->moveTarget.path.pathTarget == ent && cl->ai->moveTarget.path.pathTargetSpawnId == ent->s.spawnId) {
      continue;
    }

    const bool coveted = G_Ai_IsCovetedItem(cl, ent);

    if (onlyCovetedItems && !coveted) {
      continue;
    }

    float distance;

    if (!(coveted ? G_Ai_HasLineOfSight(cl, ent) : G_Ai_CanSee(cl, ent)) ||
        !G_Ai_CanPickup(cl, ent) ||
        (distance = G_Ai_ItemReachable(cl, ent)) <= AI_ITEM_UNREACHABLE) {
      continue;
    }

    float weight = (AI_MAX_ITEM_DISTANCE - distance) * item->def.priority;

    // boost weapons when unarmed, health/armor when retreating
    if (!G_Ai_IsArmed(cl) && item->def.type == ITEM_TYPE_WEAPON) {
      weight *= 3.f;
    } else if (G_Ai_ShouldRetreat(cl) && (item->def.type == ITEM_TYPE_HEALTH || item->def.type == ITEM_TYPE_ARMOR)) {
      weight *= 3.f;
    }

    if (coveted) {
      weight = (AI_MAX_ITEM_DISTANCE * 8.f - distance) * item->def.priority;
    }

    $(itemsVisible, add, &(GameAiItemPick) {
      .entity = ent,
      .item = item,
      .weight = weight
    });
  });

  // found one, set it up
  if (itemsVisible->count) {

    if (itemsVisible->count > 1) {
      $(itemsVisible, sort, G_Ai_CompareItemsOrder);
    }

    for (uint32_t i = 0; i < itemsVisible->count; i++) {
      const GameAiItemPick pick = VectorValue(itemsVisible, GameAiItemPick, i);
      const bool found = pick.weight > cl->ai->moveTarget.priority;

      if (!found) {
        continue;
      }

      bool pathFound = false;

      const GameAiNodeId src = G_Ai_Node_FindClosest(cl->entity->s.origin, 512.f, true, true);
      const GameAiNodeId dest = pick.entity->node != AI_NODE_INVALID
        ? pick.entity->node
        : G_Ai_Node_FindClosest(pick.entity->s.origin, 256.f, true, true);

      if (src != AI_NODE_INVALID && dest != AI_NODE_INVALID) {
        const float maxLength = AI_MAX_ITEM_DISTANCE * (G_Ai_IsCovetedItem(cl, pick.entity) ? 3.f : 1.f);

        float length;
        Vector *path = G_Ai_Node_FindPath(cl, src, dest, G_Ai_Node_Heuristic, &length);

        if (path && length <= maxLength) {
          G_Ai_BackupPath(cl->ai);
          G_Ai_SetPathGoal(cl, &cl->ai->moveTarget, pick.weight, path, pick.entity);
          pathFound = true;
        }

        release(path);
      }

      if (!pathFound && g_aiNodeDev->integer) {
        gi.WriteByte(SV_CMD_TEMP_ENTITY);
        gi.WriteByte(TE_TRACER);
        gi.WritePosition(cl->entity->s.origin);
        gi.WritePosition(pick.entity->s.origin);
        gi.Multicast(cl->entity->s.origin, MULTICAST_PHS);
      }

      // if we can't find a path to the item, don't try to get it.
      // entity goals are too finnicky for this to work properly.
    }
  }

  release(itemsVisible);
  return 100;
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
    if ((it->def.flags & WF_PROJECTILE) &&
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
 * @brief Funcgoal that controls the AI's lust for blood
 */
static uint32_t G_Ai_Hunt(GameClient *cl, PMoveCmd *cmd) {

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

  if (!armed || retreat) {

    if (cl->ai->combatTarget.type == AI_GOAL_ENTITY) {
      const float dist = Vec3_Distance(cl->entity->s.origin,
          cl->ai->combatTarget.entity.ent->s.origin);

      // keep fighting if they're right on top of us (aggressive bots fight at longer range)
      const float defenseDist = AI_SELF_DEFENSE_DISTANCE * Mixf(.5f, 1.5f, cl->ai->personality.aggression);
      if (dist > defenseDist) {

        if (cl->ai->moveTarget.type == AI_GOAL_ENTITY &&
          cl->ai->moveTarget.entity.ent == cl->ai->combatTarget.entity.ent) {
          G_Ai_ClearGoal(&cl->ai->moveTarget);
        }

        G_Ai_ClearGoal(&cl->ai->combatTarget);
        G_Ai_Debug("%s: disengaging\n", !armed ? "Unarmed" : "Low health");
        return 50;
      }
    } else {
      // don't acquire new targets; focus on items
      return 50;
    }
  }

  // see if we're already hunting
  if (cl->ai->combatTarget.type == AI_GOAL_ENTITY) {

    // check to see if the enemy has gone out of our line of sight
    if (!G_Ai_CanTarget(cl, cl->ai->combatTarget.entity.ent)) {

      // enemy dead/out of LOS/disconnected; chase them!
      if (!G_Ai_IsSeekingCovetedItem(cl) && G_Ai_ChaseEnemy(cl, cl->ai->combatTarget.entity.ent)) {

        const Vec3 whereTo = cl->ai->combatTarget.entity.ent->s.origin;
        
        const GameAiNodeId closest = G_Ai_Node_FindClosest(cl->entity->s.origin, 128.f, true, true);
        const GameAiNodeId closestToTarget = G_Ai_Node_FindClosest(whereTo, 128.f, true, true);
        Vector *path = G_Ai_Node_FindPath(cl, closest, closestToTarget, G_Ai_Node_Heuristic, NULL);

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
      const float priority = G_Ai_EnemyPriority(cl, ent, true);
      if (priority > bestPriority) {
        bestPriority = priority;
        bestEnemy = ent;
      }
    }
  });

  // switch targets if we found a significantly better one
  if (bestEnemy) {
    const float currentPriority = cl->ai->combatTarget.type == AI_GOAL_ENTITY
        ? G_Ai_EnemyPriority(cl, cl->ai->combatTarget.entity.ent, true)
        : 0.f;

    if (bestPriority > currentPriority + 1.f || cl->ai->combatTarget.type != AI_GOAL_ENTITY) {

      G_Ai_SetEntityGoal(cl, &cl->ai->combatTarget, bestPriority, bestEnemy);

      const Vec3 eyeOrigin = Vec3_Add(cl->entity->s.origin, cl->ps.pmState.viewOffset);
      const Vec3 toEnemy = Vec3_Normalize(Vec3_Subtract(Box3_Center(bestEnemy->absBounds), eyeOrigin));
      const float flick = Degrees(acosf(Clampf(Vec3_Dot(cl->forward, toEnemy), -1.f, 1.f))) * Mixf(.25f, .08f, cl->ai->personality.skill);
      const float direction = RandomRadian();

      cl->ai->perceivedVelocity = Vec3_Zero();
      cl->ai->aimError = MakeVec2(sinf(direction) * flick * .6f, cosf(direction) * flick);

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
  if (cl->ai->combatTarget.type == AI_GOAL_ENTITY && !G_Ai_GoalHasEntity(&cl->ai->moveTarget, cl->ai->combatTarget.entity.ent) && !G_Ai_IsSeekingCovetedItem(cl) && G_Ai_ChaseEnemy(cl, cl->ai->combatTarget.entity.ent)) {

    const GameEntity *enemy = cl->ai->combatTarget.entity.ent;
    const float dist = Vec3_Distance(cl->entity->s.origin, enemy->s.origin);

    // try to find a safe path to the enemy via navigation nodes
    const GameAiNodeId myNode = G_Ai_Node_FindClosest(cl->entity->s.origin, 128.f, true, true);
    const GameAiNodeId enemyNode = G_Ai_Node_FindClosest(enemy->s.origin, 128.f, true, true);
    bool pathed = false;

    if (myNode != AI_NODE_INVALID && enemyNode != AI_NODE_INVALID && myNode != enemyNode) {
      Vector *path = G_Ai_Node_FindPath(cl, myNode, enemyNode, G_Ai_Node_Heuristic, NULL);
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
 * @brief Funcgoal that controls the AI's weaponry.
 */
static uint32_t G_Ai_Weaponry(GameClient *cl, PMoveCmd *cmd) {

  // if we're dead, just keep clicking so we respawn.
  if (cl->entity->dead) {

    if (gameLevel.frameNum & 1) {
      cmd->buttons = BUTTON_ATTACK;
    }
    return 1;
  }


  if (cl->ai->weaponCheckTime < gameLevel.time) { // check for a new weapon every once in a while
    G_Ai_PickWeapon(cl);
  }

  // we're alive - if we're aiming at an enemy, start-a-firin
  if (cl->ai->combatTarget.type == AI_GOAL_ENTITY) {
    if (cl->ai->combatTarget.entity.lockOnTime < gameLevel.time) {

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
static uint32_t G_Ai_Acrobatics(GameClient *cl, PMoveCmd *cmd) {

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
      } else if ((RandomRangeu(0, jumpFreq)) == 0) { // randomly pop
        cmd->up = PM_SPEED_JUMP;
      }
    }
  } else {

    cmd->up = 0;
  }

  return 1;
}

/**
 * @brief Wander aimlessly, hoping to find something to love.
 */
static inline float G_Ai_Wander(GameClient *cl, PMoveCmd *cmd) {

  GameEntity *ent = cl->entity;

  float *angle = cl->ai->combatTarget.type == AI_GOAL_ENTITY
    ? &cl->ai->combatTarget.entity.flankAngle
    : &cl->ai->moveTarget.wander.angle;

  Vec3 forward;
  Vec3_Vectors(MakeVec3(0.f, *angle, 0.f), &forward, NULL, NULL);

  const Vec3 end = Vec3_Fmaf(ent->s.origin, Box3_Size(ent->bounds).x * 2.0f, forward);
  const CollisionTrace tr = gi.Trace(ent->s.origin, end, Box3_Zero(), ent, CONTENTS_MASK_CLIP_PLAYER);

  bool blocked = tr.fraction < 1.0f;

  // check for ground ahead to avoid walking off edges
  if (!blocked) {
    const Vec3 dropStart = end;
    const Vec3 dropEnd = Vec3_Subtract(dropStart, MakeVec3(0, 0, PM_STEP_HEIGHT * 4.f));
    const CollisionTrace groundTr = gi.Trace(dropStart, dropEnd, Box3_Zero(), ent, CONTENTS_MASK_SOLID);
    blocked = groundTr.fraction >= 1.0f;
  }

  if (blocked) {
    if (cl->ai->combatTarget.type == AI_GOAL_ENTITY) {
      if (cl->ai->combatTarget.entity.combatType == AI_COMBAT_FLANK && Randomb()) {
        cl->ai->combatTarget.entity.combatType = Randomb() ? AI_COMBAT_CLOSE : AI_COMBAT_WANDER;
      }

      if (cl->ai->combatTarget.entity.combatType == AI_COMBAT_FLANK) {
        *angle = -*angle;
      } else {
        float angleChange = 45 + Randomf() * 45;
        *angle += Randomb() ? -angleChange : angleChange;
      }
    } else {
      float angleChange = 45 + Randomf() * 45;
      *angle += Randomb() ? -angleChange : angleChange;
    }
  }

  return *angle;
}

static GameEntity *aiCurrentEntity;

/**
 * @brief Ignore ourselves, clipping to the correct mask based on our status.
 */
static CollisionTrace G_Ai_MoveTrace(const Vec3 start, const Vec3 end, const Box3 bounds) {

  const GameEntity *ent= aiCurrentEntity;

  if (ent->solid == SOLID_DEAD) {
    return gi.Trace(start, end, bounds, ent, CONTENTS_MASK_CLIP_CORPSE);
  } else {
    return gi.Trace(start, end, bounds, ent, CONTENTS_MASK_CLIP_PLAYER);
  }
}

/**
 * @brief Increase our path pointer.
 */
static bool G_Ai_AdvancePath(GameClient *cl, GameAiGoal *goal) {

  goal->path.pathIndex++;

  // we're done with this path
  if (goal->path.pathIndex == goal->path.path->count) {
    return false;
  }

  const Vector *path = goal->path.path;
  const uint32_t index = goal->path.pathIndex;

  const GameAiNodeId node = VectorValue(path, GameAiNodeId, index);
  const GameAiNodeId next = VectorValue(path, GameAiNodeId, Minz(path->count - 1, index + 1));
  goal->path.pathPosition = G_Ai_Node_GetPosition(node);
  goal->path.nextPathPosition = G_Ai_Node_GetPosition(next);
  goal->distress = 0;
  goal->distressExtension = false;
  goal->lastDistance = FLT_MAX;

  return true;
}

/**
 * @brief The minimum interval in milliseconds between attempts to path around a raised plat.
 */
#define AI_REPLAN_MILLIS 1000

/**
 * @brief Plans a new path to the current path's destination, if the next node is under a raised
 * `func_plat`. The search skips raised plats, so the new path goes around this one.
 * @return True if the AI has a new path.
 */
static bool G_Ai_AvoidRaisedPlatform(GameClient *cl) {

  GameAiGoal *goal = &cl->ai->moveTarget;

  if (cl->ai->replanTime > gameLevel.time || !G_Ai_Node_RaisedPlatform(goal->path.pathPosition)) {
    return false;
  }

  cl->ai->replanTime = gameLevel.time + AI_REPLAN_MILLIS;

  const GameAiNodeId src = G_Ai_Node_FindClosest(cl->entity->s.origin, 512.f, true, true);
  const GameAiNodeId dest = VectorValue(goal->path.path, GameAiNodeId, goal->path.path->count - 1);

  if (src == AI_NODE_INVALID) {
    return false;
  }

  Vector *path = G_Ai_Node_FindPath(cl, src, dest, G_Ai_Node_Heuristic, NULL);

  if (!path) {
    return false;
  }

  G_Ai_SetPathGoal(cl, goal, goal->priority, path, goal->path.pathTarget);
  release(path);

  if (goal->path.path->count > 1 && G_Ai_Node_RaisedPlatform(goal->path.pathPosition)) {
    G_Ai_AdvancePath(cl, goal);
  }

  G_Ai_Debug("Pathing around a raised plat\n");
  return true;
}

/**
 * @return The command direction that takes the AI out from under the given plat, which its trigger
 * would otherwise hold up, or zero if the AI is already clear of it.
 */
static Vec3 G_Ai_LeavePlatform(const GameClient *cl, const GameEntity *plat) {

  const Box3 footprint = Box3_Expand3(plat->absBounds, MakeVec3(-16.f, -16.f, 0.f));
  const Box3 bounds = cl->entity->absBounds;

  if (bounds.maxs.x < footprint.mins.x || bounds.mins.x > footprint.maxs.x ||
      bounds.maxs.y < footprint.mins.y || bounds.mins.y > footprint.maxs.y) {
    return Vec3_Zero();
  }

  Vec3 away = Vec3_Subtract(cl->entity->s.origin, Box3_Center(plat->absBounds));
  away.z = 0.f;

  Vec3 dir;
  Vec3_Vectors(MakeVec3(0.f, cl->angles.y - Vec3_Euler(Vec3_Normalize(away)).y, 0.f), &dir, NULL, NULL);

  return Vec3_Scale(dir, PM_SPEED_RUN);
}

/**
 * @brief The distance within which a bot at the end of its path walks on to an item with no node.
 */
#define AI_ITEM_APPROACH_DISTANCE 256.f

/**
 * @brief At the end of a path to an item with no node of its own, such as a dropped item, replaces
 * the path with an entity goal on the item, if the item is still there and in sight.
 * @return True if the AI now heads for the item.
 */
static bool G_Ai_ApproachPathTarget(GameClient *cl) {

  GameAiGoal *goal = &cl->ai->moveTarget;
  const GameEntity *target = goal->path.pathTarget;

  if (!target || !target->item || target->node != AI_NODE_INVALID || target->solid != SOLID_TRIGGER) {
    return false;
  }

  if (Vec3_Distance(cl->entity->s.origin, target->s.origin) > AI_ITEM_APPROACH_DISTANCE) {
    return false;
  }

  const CollisionTrace tr = gi.Trace(cl->entity->s.origin, target->s.origin, Box3_Zero(), cl->entity, CONTENTS_MASK_SOLID);

  if (tr.fraction < 1.f) {
    return false;
  }

  G_Ai_SetEntityGoal(cl, goal, goal->priority, target);
  return true;
}

/**
 * @brief See if we're in a good spot to keep going towards our node goal.
 */
static bool G_Ai_CheckNav(GameClient *cl, GameAiGoal *goal) {

  /*
   * The AI's bbox is expanded for collision checks. This is so
   * nodes on ledges don't cause the bot to misjudge a jump as much.
   */
  const Vec3 padding = { .x = 8.f, .y = 8.f, .z = 0.f };

  // if we're touching our nav goal, we can go next
  if (Box3_ContainsPoint(Box3_Expand3(cl->entity->absBounds, padding), goal->path.pathPosition)) {
    return G_Ai_AdvancePath(cl, goal);
  }

  return true;
}

/**
 * @brief Updates the distress counter for a goal and returns false if the goal should be abandoned.
 */
static bool G_Ai_GoalDistress(GameClient *cl, GameAiGoal *goal, const Vec3 dest) {
  const float pathDist = Vec3_Distance(cl->entity->s.origin, dest);

  // wander's distress is handled elsewhere
  if (goal->type) {
    // closing in
    if (pathDist < goal->lastDistance) {
      goal->lastDistance = pathDist;
      goal->distress /= 2;
    // getting further away
    } else {
      goal->distress += (gi.PointContents(cl->entity->s.origin) & CONTENTS_MASK_LIQUID ? 0.05f : 0.25f);
    }

    Vec3 dest = Vec3_Zero();

    switch (goal->type) {
    default:
      assert(false);
      break;
    case AI_GOAL_POSITION:
      dest = goal->position.pos;
      break;
    case AI_GOAL_PATH:
      dest = goal->path.pathPosition;
      break;
    case AI_GOAL_ENTITY:
      dest = goal->entity.ent->s.origin;
      break;
    }

    // something is blocking our destination
    const Vec3 eyeOrigin = Vec3_Add(cl->entity->s.origin, cl->ps.pmState.viewOffset);
    const CollisionTrace tr = gi.Trace(eyeOrigin, dest, Box3_Zero(), cl->entity, CONTENTS_MASK_CLIP_CORPSE);

    if (tr.fraction < 1.0f) {
      goal->distress += 0.25f;
    }
  }

  if (goal->lastDistress != goal->distress) {
    //G_Ai_Debug("Distress: %f\n", goal->distress);
    goal->lastDistress = goal->distress;
  }

  if (goal->distress > (goal->distressExtension ? (QUETOO_TICK_RATE * 15) : QUETOO_TICK_RATE)) {
    goal->distress = 0;
    goal->lastDistance = 0;
    goal->distressExtension = false;
    cl->ai->reacquireTime = gameLevel.time + 1000;
      
    G_Ai_Debug("Distress threshold reached\n");
    return false;
  }

  return true;
}

/**
 * @brief Returns true if consecutive nodes at path indices a and b are linked.
 */
static inline bool G_Ai_Path_IsLinked(const Vector *path, const uint32_t a, const uint32_t b) {

  return G_Ai_Node_IsLinked(VectorValue(path, GameAiNodeId, a), VectorValue(path, GameAiNodeId, b));
}

/**
 * @brief Returns true if the bot is roughly facing the given target (within ~37 degrees).
 */
static bool G_Ai_FacingTarget(const GameClient *cl, const Vec3 target) {
  Vec3 sub = Vec3_Subtract(target, cl->entity->s.origin);
  sub.z = 0;

  sub = Vec3_Normalize(sub);

  Vec3 fwd;
  Vec3_Vectors(MakeVec3(0, cl->entity->s.angles.y, 0), &fwd, NULL, NULL);

  const float dot = Vec3_Dot(sub, fwd);

  return dot > 0.8f;
}

/**
 * @brief A slow-drop occurs when a connection is mono-directional, not far horizontally
 * but far vertically.
 */
bool G_Ai_ShouldSlowDrop(const GameAiNodeId fromNode, const GameAiNodeId toNode) {
  static const float minDrop = 128.f;
  static const float maxDrop = 512.f;

  if (fromNode <= 0 || toNode <= 0) {
    return false;
  }

  if (G_Ai_Node_IsLinked(toNode, fromNode)) {
    return false;
  }
  
  const Vec3 from = G_Ai_Node_GetPosition(fromNode);
  const Vec3 to = G_Ai_Node_GetPosition(toNode);
  const float drop = from.z - to.z;

  return drop >= minDrop && drop <= maxDrop &&
    Vec2_Distance(Vec3_XY(to), Vec3_XY(from)) < PM_STEP_HEIGHT * 8.f;
}

/**
 * @return True if the entity stands on a mover that carries it, such as a plat, a door or a train,
 * and not on one that only rotates in place.
 */
static bool G_Ai_IsRidingMover(const GameEntity *ent) {
  const GameEntity *ground = ent->ground.ent;

  if (!ground || ground->s.number == 0) {
    return false;
  }

  return !Vec3_Equal(ground->velocity, Vec3_Zero()) || Vec3_Equal(ground->avelocity, Vec3_Zero());
}

/**
 * @brief Move towards our current target
 */
static uint32_t G_Ai_Move(GameClient *cl, PMoveCmd *cmd) {

  GameEntity *ent = cl->entity;

  bool targetEnemy = false;

  Vec3 dir, angles, dest = Vec3_Zero();
  bool moveWander = false;

  // Resolve the move goal iteratively. The original code used tail-recursive calls
  // when the path check or distress check failed, which could overflow the stack since
  // G_Ai_Move has a large frame (~6.5 KB for two PMove locals). Three tries matches
  // the maximum depth of the original recursion.
  bool goalReady = false;
  for (int32_t tries = 0; tries < 3 && !goalReady; tries++) {

    targetEnemy = false;
    dest = Vec3_Zero();
    moveWander = false;

    switch (cl->ai->moveTarget.type) {
      default:
        dest = ent->s.origin;
        moveWander = true;
        break;
      case AI_GOAL_POSITION:
        dest = cl->ai->moveTarget.position.pos;
        break;
      case AI_GOAL_ENTITY:
        switch (cl->ai->moveTarget.entity.combatType) {
        case AI_COMBAT_NONE:
        default:
          dest = cl->ai->moveTarget.entity.ent->s.origin;
          break;
        case AI_COMBAT_CLOSE:
          dest = cl->ai->moveTarget.entity.ent->s.origin;
          targetEnemy = true;
          break;
        case AI_COMBAT_FLANK:
        case AI_COMBAT_WANDER:
          moveWander = true;
          targetEnemy = true;
          break;
        }
        break;
      case AI_GOAL_PATH:
        if (!G_Ai_CheckNav(cl, &cl->ai->moveTarget)) {
          if (!G_Ai_ApproachPathTarget(cl)) {
            G_Ai_RestorePath(cl, cl->ai);
          }
          continue;
        }

        if (G_Ai_AvoidRaisedPlatform(cl)) {
          continue;
        }

        if (cl->ai->moveTarget.path.trickJump) {
          dest = cl->ai->moveTarget.path.trickPosition;
        } else {
          dest = cl->ai->moveTarget.path.pathPosition;
        }
        break;
    }

    if (!G_Ai_GoalDistress(cl, &cl->ai->moveTarget, dest)) {
      G_Ai_ClearGoal(&cl->ai->moveTarget);
      continue;
    }

    goalReady = true;
  }

  if (!goalReady) {
    return 1;
  }

  if (moveWander) {
    const float wanderAngle = G_Ai_Wander(cl, cmd);

    angles = MakeVec3(0.0, wanderAngle, 0.0);
    Vec3_Vectors(angles, &dir, NULL, NULL);
    dest = Vec3_Add(ent->s.origin, dir);
  }

  dir = Vec3_Subtract(dest, ent->s.origin);
  const float len = Vec3_Length(dir);

  if (targetEnemy && len < 200.0f) {
    // switch to flank/wander, this helps us recover from being up close to enemies
    if (cl->ai->combatTarget.entity.combatType == AI_COMBAT_CLOSE && Randomb()) {
      cl->ai->combatTarget.entity.combatType = Randomb() ? AI_COMBAT_FLANK : AI_COMBAT_WANDER;
    }
  }

  dir = Vec3_Normalize(dir);
  angles = Vec3_Euler(dir);

  const float deltaYaw = cl->angles.y - angles.y;
  Vec3_Vectors(MakeVec3(0.0, deltaYaw, 0.0), &dir, NULL, NULL);

  const bool swimming = ent->waterLevel >= WATER_WAIST;
  bool waitPolitely = false;

  if (cl->ai->moveTarget.type == AI_GOAL_PATH) {
    // the next step(s) will be onto a mover, so if we can't move yet, we wait.
    if (!(gi.PointContents(ent->s.origin) & CONTENTS_MASK_LIQUID) && !G_Ai_Path_CanPathTo(cl->ai->moveTarget.path.path, cl->ai->moveTarget.path.pathIndex)) {
      const GameEntity *plat = G_Ai_Node_RaisedPlatform(cl->ai->moveTarget.path.pathPosition);
      dir = plat ? G_Ai_LeavePlatform(cl, plat) : Vec3_Scale(dir, PM_SPEED_RUN);
      waitPolitely = true;
      cl->ai->moveTarget.distressExtension = true;
    // trick-jumping requires a bit of finesse
    } else if (cl->ai->moveTarget.path.trickJump) {
      if (cl->ai->moveTarget.path.trickJump == TRICK_JUMP_TURNING) {
        dir = Vec3_Zero();
      } else {
        dir = Vec3_Scale(dir, Clampf(len * 2.f, 10.f, PM_SPEED_RUN));
      }
    // if we aren't underwater, have no ground entity & falling downwards, we're probably in the air;
    // rather than full-blasting, pull us towards what we're trying to drop on.
    } else if (!swimming
               && !ent->ground.ent
               && ent->velocity.z < 0.f
               && Vec3_Dot(Vec3_Subtract(ent->s.origin, cl->ai->moveTarget.path.pathPosition), Vec3_Down()) > 0.7f
               && (cl->ai->moveTarget.path.pathPosition.z - ent->s.origin.z) <= -PM_STEP_HEIGHT) {
      dir = Vec3_Scale(dir, Clampf(len, 10.f, PM_SPEED_RUN));
      G_Ai_Debug("Trying to land on target\n");
    // we're navigating on land, and our next target is below us & a drop node (one-way connection); rather than full-blast
    // running off the edge, transition to walking so we don't overshoot targets beneath us
    } else if (!swimming && cl->ai->moveTarget.path.pathIndex > 0 &&
               G_Ai_ShouldSlowDrop(
                 VectorValue(cl->ai->moveTarget.path.path, GameAiNodeId, cl->ai->moveTarget.path.pathIndex - 1),
                 VectorValue(cl->ai->moveTarget.path.path, GameAiNodeId, cl->ai->moveTarget.path.pathIndex))) {
      dir = Vec3_Scale(dir, PM_SPEED_RUN * 0.5f);
    // run full speed towards the target
    } else {
      dir = Vec3_Scale(dir, PM_SPEED_RUN);
    }

    // if we're swimming, node is above us and we're "sticky feet", jump
    if (swimming && ent->ground.ent) {
      cmd->up = PM_SPEED_JUMP;
    // if we're on a ladder and the node is a bbox below us, crouch to get down,
    // otherwise hold jump
    } else if (cl->ps.pmState.flags & PMF_ON_LADDER) {
      if ((cl->ai->moveTarget.path.pathPosition.z - ent->s.origin.z) < -Box3_Size(Pm_Bounds(&cl->ps.pmState.params, false)).z) {
        cmd->up = -PM_SPEED_DUCKED;
      } else {
        cmd->up = PM_SPEED_JUMP;
      }
    }
  // run full speed towards the target
  } else {
    dir = Vec3_Scale(dir, PM_SPEED_RUN);

    // if we're not navving, hold jump under water
    if (ent->waterLevel >= WATER_WAIST) {
      cmd->up = PM_SPEED_JUMP;
    }
  }

  cmd->forward = dir.x;
  cmd->right = dir.y;

  aiCurrentEntity = ent;

  // predict ahead
  PMove pm;

  memset(&pm, 0, sizeof(pm));
  pm.s = cl->ps.pmState;

  pm.s.origin = ent->s.origin;

  pm.s.velocity = ent->velocity;

  pm.s.type = PM_NORMAL;

  pm.cmd = *cmd;
  pm.ground = ent->ground;

  pm.PointContents = gi.PointContents;
  pm.BoxContents = gi.BoxContents;
  
  pm.Trace = G_Ai_MoveTrace;

  pm.Debug = gi.Debug;
  pm.DebugMask = gi.DebugMask;
  pm.debugMask = DEBUG_PMOVE_SERVER;

  // perform a move; predict our next frame
  Pm_Move(&pm);

  // predict a few frames ahead for timely edge/mover stoppage; cache result per
  // tick so the three sub-passes of G_Ai_ClientThink share one expensive Pm_Move
  if (cl->ai->lookaheadFrame != gameLevel.frameNum) {
    PMove ahead = pm;
    ahead.cmd.msec = 100;
    Pm_Move(&ahead);
    cl->ai->lookaheadFrame = gameLevel.frameNum;
    cl->ai->lookaheadNoGround = !ahead.ground.ent;
  }

  // predicted ground is gone
  if (ent->ground.ent && (cl->ai->lookaheadNoGround || !pm.ground.ent)) {

    if (cl->ai->moveTarget.type == AI_GOAL_PATH) {

      // path goals: only apply edge logic when not intentionally jumping
      if (cmd->up <= 0) {
        const float xyDist = Vec2_Distance(Vec3_XY(cl->ai->moveTarget.path.pathPosition), Vec3_XY(ent->s.origin));
        
        // we're most likely on or going to a mover; if we'll be falling in a few frames, stop us early
        if (!(gi.PointContents(ent->s.origin) & CONTENTS_MASK_LIQUID) && !G_Ai_Path_CanPathTo(cl->ai->moveTarget.path.path, cl->ai->moveTarget.path.pathIndex)) {
          cmd->forward = -cmd->forward;
          cmd->right = -cmd->right; // stop for now
          G_Ai_Debug("Stopping early to prevent mover issues\n");
        // if the node is above us step-wise OR it's not far below us & across a big distance, we gotta jump
        } else if (((cl->ai->moveTarget.path.pathPosition.z - ent->s.origin.z) > -PM_STEP_HEIGHT ||
          (xyDist > fabsf(cl->ai->moveTarget.path.pathPosition.z - ent->s.origin.z) && (xyDist >= PM_STEP_HEIGHT * 6.f))) && !pm.ground.ent) {
          cmd->up = PM_SPEED_JUMP;
        }
      }

    } else {

      // non-path movement: back away from edges, cancel acrobatics jumps
      cmd->forward = -cmd->forward;
      cmd->right = -cmd->right;
      cmd->up = 0;

      if (moveWander) {
        const float angle = 45 + Randomf() * 45;

        if (targetEnemy) {
          cl->ai->moveTarget.entity.combatType = Randomb() ? AI_COMBAT_FLANK : AI_COMBAT_WANDER;
          cl->ai->moveTarget.entity.flankAngle += Randomb() ? -angle : angle;
        } else {
          cl->ai->moveTarget.wander.angle += Randomb() ? -angle : angle;
        }
      } else if (targetEnemy) {
        // charging toward enemy would go off edge - switch to flanking
        cl->ai->moveTarget.entity.combatType = AI_COMBAT_WANDER;
      }
    }
  // trick jump code
  } else if (cl->ai->moveTarget.type == AI_GOAL_PATH) {

    if (cl->ai->moveTarget.path.trickJump == TRICK_JUMP_START) {
      cl->ai->moveTarget.path.trickJump++;
      cmd->up = 0;
      G_Ai_Debug("Trick jump: letting go for a frame\n");
    } else if (cl->ai->moveTarget.path.trickJump == TRICK_JUMP_WAITING) {
      cmd->up = PM_SPEED_JUMP;
      G_Ai_Debug("Trick jump: holding jump again!!\n");
    }
  }
  
  // check for getting stuck
  const Vec3 moveDir = Vec3_Subtract(ent->s.origin, pm.s.origin);
  const float moveLen = Vec3_Length(moveDir);
  
  // check for teleport
  if (moveLen > 64.f) {
    if (cl->ai->moveTarget.type == AI_GOAL_PATH) {
      if (!G_Ai_AdvancePath(cl, &cl->ai->moveTarget)) {
        G_Ai_RestorePath(cl, cl->ai);
      }
    }
  // if we're not waiting to turn...
  // and not waiting politely...
  // and not riding a mover...
  } else if (((cl->ai->moveTarget.type == AI_GOAL_PATH
               && cl->ai->moveTarget.path.trickJump != TRICK_JUMP_TURNING)
               || cl->ai->moveTarget.type != AI_GOAL_PATH)
               && !waitPolitely
               && !G_Ai_IsRidingMover(ent)) {

    // we'll be pushed up against something
    float smolDist = PM_SPEED_RUN * PM_SPEED_MOD_WALK * MILLIS_TO_SECONDS(cmd->msec);

    if (gi.PointContents(ent->s.origin) & CONTENTS_MASK_LIQUID) {
      smolDist *= 0.1f;
    }

    if (moveLen < smolDist) {
      
      if (cl->ai->distressJumpOffset <= gameLevel.time) {
        // if we're navving, node is above us, and we're on ground, jump; we're probably trying
        // to trick-jump or something
        if (cl->ai->moveTarget.type == AI_GOAL_PATH && ent->ground.ent && pm.ground.ent) {
          if ((cl->ai->moveTarget.path.pathPosition.z - ent->s.origin.z) > PM_STEP_HEIGHT * 7.f) {

            cl->ai->moveTarget.path.trickJump = TRICK_JUMP_START;
            cl->ai->moveTarget.path.trickPosition = cl->ai->moveTarget.path.pathPosition;
            cmd->up = PM_SPEED_JUMP;

            G_Ai_Debug("Node *far* above us, and we're probably stuck; trick jump most likely!\n");
          } else if ((cl->ai->moveTarget.path.pathPosition.z - ent->s.origin.z) > PM_STEP_HEIGHT &&
            Vec2_Distance(Vec3_XY(cl->ai->moveTarget.path.pathPosition), Vec3_XY(ent->s.origin)) < PM_STEP_HEIGHT * 7.f &&
            G_Ai_FacingTarget(cl, cl->ai->moveTarget.path.pathPosition)) {

            cmd->up = PM_SPEED_JUMP;
            G_Ai_Debug("Node above us & close, and we're probably stuck; regular jump\n");
          }
        }
      }

      // if we're on a mover, distress differently so we don't unexpectedly
      // jump off of it
      if (G_Ai_IsRidingMover(ent)) {
        cl->ai->moveTarget.distress += 0.02f;
      } else {
        cl->ai->moveTarget.distress += 0.2f;

        // try moving left/right if we weren't already trying this
        if (cl->ai->moveTarget.distress > 8.f && !cmd->right) {
          cmd->right = cmd->forward;

          if (cl->ai->moveTarget.time & 1) {
            cmd->right = -cmd->right;
          }
        // try a jump
        } else if (cl->ai->moveTarget.distress > 12.f) {
          cmd->up = PM_SPEED_JUMP;
        }
      }
    // we're making some distance, reduce our distress
    } else {
      if (cl->ai->moveTarget.distress > 0) {
        cl->ai->moveTarget.distress -= 0.2f;
      }
    }
  }

  return 0;
}

/**
 * @return The shortest signed rotation in degrees from `from` to `to`.
 */
static inline float G_Ai_AngleDelta(const float from, const float to) {
  return remainderf(to - from, 360.f);
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
  const float deviation = Mixf(4.f, 1.f, skill) + Mixf(.1f, .04f, skill) * angularSpeed;
  const float diffusion = deviation * sqrtf(2.f * seconds / correction);

  Vec2 *error = &cl->ai->aimError;

  for (int32_t i = 0; i < 2; i++) {
    const float noise = (Randomf() + Randomf() + Randomf() - 1.5f) * 2.f;
    const float scale = i == 0 ? .6f : 1.f;

    error->xy[i] += -error->xy[i] * seconds / correction + noise * diffusion * scale;
  }
}

/**
 * @brief Turns the view toward the current target with a critically damped spring. In combat, the
 * aim point lags the target's changes in velocity by the bot's reaction time, plus the aim error.
 */
static uint32_t G_Ai_Turn(GameClient *cl, PMoveCmd *cmd) {

  GameAiGoal *combatTarget = &cl->ai->combatTarget;

  GameEntity *ent = cl->entity;
  const float seconds = MILLIS_TO_SECONDS(cmd->msec);

  Vec3 idealAngles;
  Vec3 idealRate = Vec3_Zero();

  if (combatTarget->type != AI_GOAL_ENTITY) {
    if (cl->ai->moveTarget.type == AI_GOAL_NONE) {
      idealAngles = MakeVec3(0.0, cl->ai->moveTarget.wander.angle, 0.0);
    } else {
      Vec3 aimTarget = Vec3_Zero();

      if (cl->ai->moveTarget.type == AI_GOAL_PATH) {

        // if we're trick-jumping, aim towards where we intend to land
        if (cl->ai->moveTarget.path.trickJump) {
          aimTarget = cl->ai->moveTarget.path.trickPosition;
        // if we're above ground & on-land, and our next path is bidirectional, assume it's normal
        // pathing; aim towards our *next* target to look a bit more natural.
        } else if (((ent->waterLevel < WATER_WAIST
                     && !(cl->ps.pmState.flags & PMF_ON_LADDER))
                     || !(gi.PointContents(cl->ai->moveTarget.path.pathPosition) & CONTENTS_MASK_LIQUID))
                   && cl->ai->moveTarget.path.pathIndex
                   && G_Ai_Path_IsLinked(cl->ai->moveTarget.path.path,
                                       cl->ai->moveTarget.path.pathIndex,
                                       cl->ai->moveTarget.path.pathIndex - 1)) {
          aimTarget = cl->ai->moveTarget.path.nextPathPosition;
        // otherwise, aim directly at the next position
        } else {
          aimTarget = cl->ai->moveTarget.path.pathPosition;
        }
      } else if (cl->ai->moveTarget.type == AI_GOAL_POSITION) {
        aimTarget = cl->ai->moveTarget.position.pos;
      } else if (cl->ai->moveTarget.type == AI_GOAL_ENTITY) {
        aimTarget = cl->ai->moveTarget.entity.ent->s.origin;
      } else {
        assert(false);
      }

      const Vec3 aimDirection = Vec3_Normalize(Vec3_Subtract(aimTarget, cl->entity->s.origin));
      idealAngles = Vec3_Euler(aimDirection);
      idealAngles.z = 0.f;

      // if underwater or in air we have to directly face our target, otherwise
      // just yaw us.
      // FIXME: bug in PMove prevents this from working for *all* in air situations
      // (you get less forward momentum on a jump if you are looking up/down)
      // so for now this is hardcoded to ladders
      if (cl->ps.pmState.flags & PMF_ON_LADDER) {
        if ((cl->ai->moveTarget.path.pathPosition.z - cl->entity->s.origin.z) < -Box3_Size(Pm_Bounds(&cl->ps.pmState.params, false)).z) {
          idealAngles.x = Clampf(idealAngles.x, -10.f, -180.f);
        } else {
          idealAngles.x = Clampf(idealAngles.x, 10.f, 180.f);
        }
        G_Ai_Debug("Clamping X to %f\n", idealAngles.x);
      } else if (ent->waterLevel < WATER_WAIST) {
        idealAngles.x = 0.f;
      }
    }
  } else {
    const GameEntity *enemy = combatTarget->entity.ent;
    const Vec3 eyeOrigin = Vec3_Add(cl->entity->s.origin, cl->ps.pmState.viewOffset);
    const Vec3 enemyCenter = Box3_Center(enemy->absBounds);

    const float reaction = Mixf(.25f, .12f, cl->ai->personality.skill);
    cl->ai->perceivedVelocity = Vec3_Mix(cl->ai->perceivedVelocity, enemy->velocity, 1.f - expf(-seconds / reaction));

    Vec3 aimPoint = Vec3_Fmaf(enemyCenter, reaction, Vec3_Subtract(cl->ai->perceivedVelocity, enemy->velocity));

    const GameItem *const weapon = cl->weapon;

    if (weapon->def.flags & WF_PROJECTILE) {
      const float dist = Vec3_Distance(eyeOrigin, enemyCenter);
      // skilled bots predict more accurately (tighter speed estimate range)
      const float spread = Mixf(300.f, 100.f, cl->ai->personality.skill);
      const float speed = RandomRangef(1050.f - spread, 1050.f + spread);
      aimPoint = Vec3_Fmaf(aimPoint, dist / speed, cl->ai->perceivedVelocity);
    }

    idealAngles = Vec3_Euler(Vec3_Normalize(Vec3_Subtract(aimPoint, eyeOrigin)));

    const Vec3 relativeVelocity = Vec3_Subtract(cl->ai->perceivedVelocity, ent->velocity);
    const Vec3 nextAimPoint = Vec3_Fmaf(aimPoint, AI_AIM_RATE_SECONDS, relativeVelocity);
    const Vec3 nextAngles = Vec3_Euler(Vec3_Normalize(Vec3_Subtract(nextAimPoint, eyeOrigin)));

    for (int32_t i = 0; i < 2; i++) {
      idealRate.xyz[i] = G_Ai_AngleDelta(idealAngles.xyz[i], nextAngles.xyz[i]) / AI_AIM_RATE_SECONDS;
    }

    G_Ai_UpdateAimError(cl, idealRate, seconds);

    idealAngles.x = Clampf(G_Ai_AngleDelta(0.f, idealAngles.x + cl->ai->aimError.x), -90.f, 90.f);
    idealAngles.y += cl->ai->aimError.y;
  }

  idealAngles.x = G_Ai_AngleDelta(0.f, idealAngles.x);

  if (fabsf(idealAngles.x) > 90.f) {
    idealAngles.x = 90.f;
  }

  const float stiffness = Mixf(10.f, 20.f, cl->ai->personality.skill);
  const float maxSpeed = Mixf(300.f, 900.f, cl->ai->personality.skill);

  const Vec3 viewAngles = cl->angles;
  Vec3 angles = idealAngles;

  for (int32_t i = 0; i < 2; i++) {
    const float error = G_Ai_AngleDelta(viewAngles.xyz[i], idealAngles.xyz[i]);
    float *speed = &cl->ai->viewVelocity.xyz[i];

    *speed += (stiffness * stiffness * error + 2.f * stiffness * (idealRate.xyz[i] - *speed)) * seconds;
    *speed = Clampf(*speed, -maxSpeed, maxSpeed);

    angles.xyz[i] = viewAngles.xyz[i] + *speed * seconds;
  }

  if (cl->ai->moveTarget.type == AI_GOAL_PATH && cl->ai->moveTarget.path.trickJump == TRICK_JUMP_TURNING &&
      fabsf(G_Ai_AngleDelta(angles.y, idealAngles.y)) < 2.f) {
    cl->ai->moveTarget.path.trickJump = TRICK_JUMP_NONE;
  }

  cmd->angles = Vec3_Subtract(angles, cl->ps.pmState.deltaAngles);
  return 0;
}

/**
 * @brief Clears any goal of the given AI that references the given entity.
 * Called by `G_InvalidateEntityReferences` before an entity is freed, so that
 * a bot doesn't retain a dangling reference to it (e.g. a bot targeting a
 * player who disconnects).
 */
void G_Ai_InvalidateReferences(GameAi *ai, const GameEntity *ent) {

  if (G_Ai_GoalHasEntity(&ai->combatTarget, ent)) {
    G_Ai_ClearGoal(&ai->combatTarget);
  }

  if (G_Ai_GoalHasEntity(&ai->moveTarget, ent)) {
    G_Ai_ClearGoal(&ai->moveTarget);
  }

  if (G_Ai_GoalHasEntity(&ai->backupMoveTarget, ent)) {
    G_Ai_ClearGoal(&ai->backupMoveTarget);
  }
}

/**
 * @brief Called just before an AI leaves this mortal plane.
 */
void G_Ai_Disconnect(GameClient *cl) {

  // clear any dynamic memory
  G_Ai_ClearGoal(&cl->ai->combatTarget);
  G_Ai_ClearGoal(&cl->ai->moveTarget);
  G_Ai_ClearGoal(&cl->ai->backupMoveTarget);

  gi.Free(cl->ai);
  cl->ai = NULL;
}

/**
 * @brief Long range goal picking
 */
static uint32_t G_Ai_LongRange(GameClient *cl, PMoveCmd *cmd) {

  // if we already have a long range goal, try again later.
  // TODO: we know what entity we're heading towards, so we can
  // give up a goal if the entity is gone, but it should be logical
  // (for stuff that's "close" we can give up if the item appears to 
  // be visually missing; for flags, we can give up if the flag was taken
  // so we can switch to hunting the taker, or trying to support the carrier)
  if (cl->ai->moveTarget.type != AI_GOAL_NONE) {
    return 200;
  }

  // check to be sure we're in a navicable spot
  const GameAiNodeId closest = G_Ai_Node_FindClosest(cl->entity->s.origin, 256.f, true, true);

  if (closest == AI_NODE_INVALID) {
    return 200;
  }

  Vector *goalPossibilities = $(alloc(Vector), initWithSize, sizeof(GameAiItemPick));

  G_ForEachEntity(ent, {

    if (ent->solid == SOLID_NOT || ent->solid == SOLID_DEAD) {
      continue;
    }

    if (ent->serverFlags & SVF_NO_CLIENT) {
      continue;
    }

    // TODO: atm only items can be goals.
    // in future, other players should be able to be goals (supporting flag carrier for instance)
    if (!ent->item) {
      continue;
    }

    float weight = 0.f;

    // if we're an item, check that we can pick it up
    // and check weight
    if (ent->item) {

      if (!G_Ai_CanPickup(cl, ent)) {
        continue;
      }

      // situational weighting: boost weapons when unarmed, health/armor when low
      weight = ent->item->def.priority;

      if (!G_Ai_IsArmed(cl) && ent->item->def.type == ITEM_TYPE_WEAPON) {
        weight *= 3.f;
      } else if (G_Ai_ShouldRetreat(cl) && (ent->item->def.type == ITEM_TYPE_HEALTH || ent->item->def.type == ITEM_TYPE_ARMOR)) {
        weight *= 3.f;
      }
    }

    // didn't want this
    if (!weight) {
      continue;
    }

    // randomize weights
    weight = Randomf() * weight;

    // add!!
    $(goalPossibilities, add, &(GameAiItemPick) {
      .weight = weight,
      .entity = ent
    });
  });

  // sort!
  if (goalPossibilities->count > 1) {
    $(goalPossibilities, sort, G_Ai_CompareItemsOrder);
  }

  // go down the list, high priority wins but might not be pickable
  for (uint32_t i = 0; i < goalPossibilities->count; i++) {

    const GameAiItemPick *pick = VectorElement(goalPossibilities, GameAiItemPick, i);
    const GameAiNodeId closestToItem = G_Ai_Node_FindClosest(pick->entity->s.origin, 256.f, true, true);

    Vector *path = G_Ai_Node_FindPath(cl, closest, closestToItem, G_Ai_Node_Heuristic, NULL);
    if (path) {
      G_Ai_SetPathGoal(cl, &cl->ai->moveTarget, pick->weight, path, pick->entity);
      release(path);
      break;
    }
  }

  release(goalPossibilities);

  // got one; don't try again for a while.
  return 1000;
}

/**
 * @brief Static list of func goal functions
 */
static const GameAiGoalFunc aiGoalFuncs[AI_FUNC_GOAL_TOTAL] = {
  [AI_FUNC_GOAL_LONGRANGE] = G_Ai_LongRange,
  [AI_FUNC_GOAL_HUNT] = G_Ai_Hunt,
  [AI_FUNC_GOAL_WEAPONRY] = G_Ai_Weaponry,
  [AI_FUNC_GOAL_ACROBATICS] = G_Ai_Acrobatics,
  [AI_FUNC_GOAL_FINDITEMS] = G_Ai_FindItems,
  [AI_FUNC_GOAL_TURN] = G_Ai_Turn,
  [AI_FUNC_GOAL_MOVE] = G_Ai_Move
};

#if AI_GOAL_HARDENING
/**
 * @brief Validates an `AI_GOAL_ENTITY` goal's cached `entity.ent` pointer
 * against the canonical `ge.entities` table, re-resolving by slot number
 * rather than trusting the cached pointer directly. Clears the goal if the
 * target has been freed, reused (spawnId mismatch), or if the cached pointer
 * no longer matches the canonical entity (which would indicate corruption);
 * otherwise refreshes `entity.ent` from the canonical table.
 * @details When the goal is found to be invalid, logs a warning and a
 * backtrace to aid in tracking down the root cause in the field (#960).
 * Delete this along with everything else guarded by `AI_GOAL_HARDENING` once
 * the root cause is understood and fixed.
 */
static void G_Ai_ValidateEntityGoal(const GameClient *cl, const char *field, GameAiGoal *goal) {

  if (goal->type != AI_GOAL_ENTITY) {
    return;
  }

  const GameEntity *ent = G_Ai_ResolveGoalEntity(goal->entity.number);

  if (!ent || !ent->inUse || goal->entity.spawnId != ent->s.spawnId) {

    char *backtrace = gi.Backtrace(1, 32);
    gi.Warn(__func__, "Invalidating stale %s for %s: cached ent %p, number %d, cached spawn_id %u, "
            "resolved ent %p (in_use %d, spawn_id %u)\n%s\n",
            field, etos(cl->entity), (void *) goal->entity.ent, goal->entity.number, goal->entity.spawnId,
            (void *) ent, ent ? ent->inUse : 0, ent ? ent->s.spawnId : 0,
            backtrace ? backtrace : "(no backtrace available)");
    free(backtrace);

    G_Ai_ClearGoal(goal);
    return;
  }

  goal->entity.ent = ent;
}
#endif

/**
 * @brief Called every frame for every AI.
 */
void G_Ai_Think(GameClient *cl, PMoveCmd *cmd) {

  if (cl->entity->solid == SOLID_DEAD) {
    G_Ai_ClearGoal(&cl->ai->combatTarget);
    G_Ai_ClearGoal(&cl->ai->moveTarget);
    G_Ai_ClearGoal(&cl->ai->backupMoveTarget);
  }

#if AI_GOAL_HARDENING
  // clear stale entity goals whose target has been freed, reused, or whose
  // cached `entity.ent` pointer can no longer be trusted. Re-resolve against
  // the canonical `ge.entities` table by slot number rather than dereferencing
  // the cached pointer directly, since it may have gone stale or been
  // corrupted (see #960).
  G_Ai_ValidateEntityGoal(cl, "combat_target", &cl->ai->combatTarget);
  G_Ai_ValidateEntityGoal(cl, "move_target", &cl->ai->moveTarget);
  G_Ai_ValidateEntityGoal(cl, "backup_move_target", &cl->ai->backupMoveTarget);
#endif

  // run functional goals
  for (int32_t i = 0; i < AI_FUNC_GOAL_TOTAL; i++) {

    if (cl->ai->funcGoalNextThinks[i] <= gameLevel.time) {
      const int64_t funcStart = G_Ai_Microseconds();
      const uint32_t next = aiGoalFuncs[i](cl, cmd);
      const int64_t funcUs = G_Ai_Microseconds() - funcStart;

      cl->ai->funcGoalNextThinks[i] = gameLevel.time + next;

      if (funcUs > 50000) { // > 50ms for one goal function is pathological
        G_Warn("%s goal func %d took %dms\n",
               cl->persistent.netName, i, (int32_t)(funcUs / 1000));
      }
    }
  }

  // run client think
  G_ClientThink(cl, cmd);

  // can't trick jump when we hit the ground.
  if (cl->ai->moveTarget.type == AI_GOAL_PATH && cl->entity->ground.ent && cl->ai->moveTarget.path.trickJump) {
    if (Vec3_Distance(cl->ai->moveTarget.path.trickPosition, cl->entity->s.origin) < 24.f) {
      cl->ai->moveTarget.path.trickJump = TRICK_JUMP_TURNING;
      G_Ai_Debug("Trick jump: mission accomplished\n");
    } else {
      cl->ai->moveTarget.path.trickJump = TRICK_JUMP_NONE;
      G_Ai_Debug("Trick jump: mission accomplished\n");
    }
  }
}

/**
 * @brief Called every time an AI spawns
 */
void G_Ai_Respawn(GameClient *cl) {

  G_Ai_ClearGoal(&cl->ai->combatTarget);
  G_Ai_ClearGoal(&cl->ai->moveTarget);
  G_Ai_ClearGoal(&cl->ai->backupMoveTarget);

  memset(cl->ai->funcGoalNextThinks, 0, sizeof(cl->ai->funcGoalNextThinks));

  cl->ai->weaponCheckTime = 0;
  cl->ai->reacquireTime = 0;
  cl->ai->lookaheadFrame = 0;
  cl->ai->lookaheadNoGround = false;
  cl->ai->replanTime = 0;

  cl->ai->viewVelocity = Vec3_Zero();
  cl->ai->perceivedVelocity = Vec3_Zero();
  cl->ai->aimError = Vec2_Zero();
}

/**
 * @brief Called when an AI is first spawned and is ready to go.
 */
void G_Ai_Begin(GameClient *cl) {

  const GameAiRoster *r = cl->ai->roster;

  cl->ai->personality = (GameAiPersonality) {
    .skill      = r->skill,
    .aggression = r->aggression,
    .awareness  = r->awareness,
  };

  G_Ai_Debug("%s: skill=%.2f aggression=%.2f awareness=%.2f\n",
    r->name,
    cl->ai->personality.skill,
    cl->ai->personality.aggression,
    cl->ai->personality.awareness);
}

/**
 * @brief Runs the AI think function multiple times per server frame to simulate a real client.
 */
static void G_Ai_ClientThink(GameEntity *ent) {
  const int32_t numRuns = 3;
  uint8_t msecLeft = QUETOO_TICK_MILLIS;

  for (int32_t i = 0; i < numRuns; i++) {

    if (!ent->inUse || !ent->client) {
      break;
    }

    PMoveCmd cmd = { 0 };

    cmd.msec = (i == numRuns - 1) ? msecLeft : ceilf(1000.f / QUETOO_TICK_RATE / numRuns);

    const int64_t thinkStart = G_Ai_Microseconds();
    G_Ai_Think(ent->client, &cmd);
    const int64_t thinkUs = G_Ai_Microseconds() - thinkStart;

    if (thinkUs > 100000) { // > 100ms for one think pass is pathological
      if (ent->client) {
        G_Warn("%s AI think pass %d took %dms\n",
               ent->client->persistent.netName, i, (int32_t)(thinkUs / 1000));
      }
    }

    msecLeft -= cmd.msec;
  }

  ent->nextThink = gameLevel.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief Initializes the AI client state and begins its presence in the game world.
 */
static void G_Ai_ClientBegin(GameClient *cl) {

  G_ClientBegin(cl);

  G_Ai_Begin(cl);

  G_Debug("Spawned %s at %s", cl->persistent.netName, vtos(cl->entity->s.origin));

  cl->entity->Think = G_Ai_ClientThink;
  cl->entity->nextThink = gameLevel.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief Allocates AI state and connects the bot client to the game using a generated user info string.
 */
static void G_Ai_Connect(GameClient *cl) {

  char userInfo[MAX_INFO_STRING_STRING];
  const GameAiRoster *roster = G_Ai_GetRoster(cl, userInfo);

  cl->ai = gi.Malloc(sizeof(GameAi), MEM_TAG_AI);
  cl->ai->roster = roster;

  G_ClientConnect(cl, userInfo);

  G_Ai_ClientBegin(cl);
}

/**
 * @brief Manages the bot population and runs each bot's think function every frame.
 */
void G_Ai_Frame(void) {

  if (gameLevel.intermissionTime) {
    return;
  }

  if (gameLevel.time == 1000) {
    G_Ai_NodesReady();
  }

  if (gameLevel.time % 1000 == 0) {

    int32_t humanClients = 0;
    int32_t aiClients = 0;
    G_ForEachClient(cl, {
      if (cl->ai) {
        aiClients++;
      } else if (!cl->persistent.spectator) {
        humanClients++;
      }
    });

    const int32_t activeClients = humanClients + aiClients;

    const int32_t baseMinClients = gameLevel.minClientsMap > -1 ? gameLevel.minClientsMap : sv_minClients->integer;
    const int32_t minClients = Maxi(0, baseMinClients);
    const int32_t maxClients = Maxi(0, sv_maxClients->integer);

    const int32_t desiredClients = Mini(minClients, maxClients);

    if (activeClients < desiredClients) {
      G_ForEachFreeClient(cl, {
        G_Ai_Connect(cl);
        break;
      });
    } else if (activeClients > desiredClients) {
      G_ForEachClient(cl, {
        if (cl->ai) {
          gi.Cbuf(va("kick %d\n", cl->ps.client));
          break;
        }
      });
    }
  }

  G_ForEachClient(cl, {
    if (cl->ai && cl->entity) {
      G_RunThink(cl->entity);
    }
  });
}

/**
 * @brief Console command handler to save navigation node data to disk.
 */
static void G_Ai_SaveNodes_f(void) {
  G_Ai_SaveNodes();
}

/**
 * @brief Console command handler to destroy a specific navigation node by ID.
 */
static void G_Ai_DeleteNode_f(void) {

  if (gi.Argc() != 2) {
    gi.Print("Usage: %s [node_id]\n", gi.Argv(0));
    return;
  }

  G_Ai_Node_Destroy((GameAiNodeId) atoi(gi.Argv(1)));
}

/**
 * @brief Console command handler to delete all navigation nodes.
 */
static void G_Ai_DeleteNodes_f(void) {
  G_Ai_DeleteNodes();
}

/**
 * @brief Console command handler that tests pathfinding by routing all bots through a specified path.
 */
static void G_Ai_TestPath_f(void) {

  Vector *path = G_Ai_Node_TestPath();

  if (!path) {
    return;
  }

  G_ForEachClient(cl, {
    if (cl->ai) {
      const GameAiNodeId closestToPlayer = G_Ai_Node_FindClosest(cl->entity->s.origin, 256.f, true, true);

      if (closestToPlayer == AI_NODE_INVALID) {
        G_Ai_Debug("Can't find a node near this bot\n");
        continue;
      }

      Vector *pathToStart = G_Ai_Node_FindPath(cl, closestToPlayer, VectorValue(path, GameAiNodeId, 0), G_Ai_Node_Heuristic, NULL);

      if (pathToStart == NULL) {
        G_Ai_Debug("Can't find a path to the test path\n");
        continue;
      }

      for (uint32_t i = 1; i < path->count; i++) {
        GameAiNodeId node = VectorValue(path, GameAiNodeId, i);
        $(pathToStart, add, &node);
      }
      G_Ai_SetPathGoal(cl, &cl->ai->moveTarget, 1.0, pathToStart, NULL);
      release(pathToStart);
    }
  });
}

void G_Ai_OffsetNodes_f(void);

/**
 * @brief Initializes the AI subsystem.
 */
void G_Ai_Init(void) {

  g_aiNoTarget = gi.AddCvar("g_aiNoTarget", "0", CVAR_DEVELOPER, "Disables bots targeting enemies");
  g_aiNodeDev = gi.AddCvar("g_aiNodeDev", "0", CVAR_DEVELOPER | CVAR_LATCH, "Toggles node development mode. '1' is full development mode, '2' is live debug mode.");
  
  if (g_aiNodeDev->integer) {
    gi.SetCvarInteger("g_cheats", 1);
  }

  gi.SetConfigString(CS_NAV_EDIT, g_aiNodeDev->string);

  gi.AddCmd("g_aiSaveNodes", G_Ai_SaveNodes_f, CMD_AI, "Save current node data");
  gi.AddCmd("g_aiDeleteNode", G_Ai_DeleteNode_f, CMD_AI, "Delete a node by id");
  gi.AddCmd("g_aiDeleteNodes", G_Ai_DeleteNodes_f, CMD_AI, "Delete all current node data");
  gi.AddCmd("g_aiTestPath", G_Ai_TestPath_f, CMD_AI, "Save current node data");
  gi.AddCmd("g_aiOffsetNodes", G_Ai_OffsetNodes_f, CMD_AI, "Offset the loaded nodes by the specified translation");

  G_Ai_InitSkins();
}

/**
 * @brief Loads map data for the AI subsystem.
 */
void G_Ai_Load(void) {

  G_Ai_InitNodes();
}

/**
 * @brief Shuts down the AI subsystem
 */
void G_Ai_Shutdown(void) {

  G_Ai_ShutdownSkins();

  gi.FreeTag(MEM_TAG_AI);
}

/**
 * @brief Returns true if the AI subsystem is running in full node development mode.
 */
bool G_Ai_InDeveloperMode(void) {

  return g_aiNodeDev->integer == 1;
}
