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
 * @brief A bot is outgunned by an enemy whose weapon priority exceeds its own best by this much.
 * Unskilled and aggressive bots need a larger margin.
 */
#define AI_OUTGUNNED_PRIORITY .3f

/**
 * @brief A bot is outgunned by an enemy with quad damage if its best weapon is below this priority.
 * Unskilled and aggressive bots need a lower priority.
 */
#define AI_OUTGUNNED_QUAD_PRIORITY .5f

/**
 * @return The lowest weapon priority at which the AI considers itself armed. Aggressive bots settle
 * for less.
 */
static float G_Ai_ArmedPriority(const GameClient *cl) {
  return AI_ARMED_PRIORITY * Mixf(1.25f, .6f, cl->ai->personality.aggression);
}

/**
 * @return True if the bot has at least a Shotgun or better.
 * Blaster-only bots are considered unarmed and will prioritize weapon pickups.
 */
bool G_Ai_IsArmed(const GameClient *cl) {

  const float threshold = G_Ai_ArmedPriority(cl);

  for (GameItemTag t = WEAPON_FIRST; t < WEAPON_LAST; t++) {
    const GameItem *it = &gameItems[t];
    if (cl->inventory[t] && it->def.priority >= threshold) {
      return true;
    }
  }

  return false;
}

/**
 * @see g_ai_main.h
 */
bool G_Ai_WouldArm(const GameClient *cl, const GameItem *item) {

  if (item->def.type != ITEM_TYPE_WEAPON || cl->inventory[item->def.tag]) {
    return false;
  }

  return item->def.priority >= G_Ai_ArmedPriority(cl) && !G_Ai_IsArmed(cl);
}

/**
 * @return The highest priority of the weapons that the bot holds with enough ammo to fire.
 */
static float G_Ai_BestWeaponPriority(const GameClient *cl) {

  float best = 0.f;

  for (GameItemTag t = WEAPON_FIRST; t < WEAPON_LAST; t++) {
    const GameItem *it = &gameItems[t];

    if (!cl->inventory[t]) {
      continue;
    }

    if (it->def.ammo && cl->inventory[it->def.ammo] < it->def.quantity) {
      continue;
    }

    best = Maxf(best, it->def.priority);
  }

  return best;
}

/**
 * @return True if the enemy is invulnerable, or seriously outguns the bot with its weapon or quad
 * damage. Unskilled and aggressive bots misjudge or ignore the odds, so they pick worse fights.
 */
bool G_Ai_IsOutgunned(const GameClient *cl, const GameEntity *enemy) {

  if (enemy->s.effects & EF_INVULNERABILITY) {
    return true;
  }

  const float best = G_Ai_BestWeaponPriority(cl);
  const float recklessness = Mixf(1.5f, 1.f, cl->ai->personality.skill) * Mixf(.7f, 1.3f, cl->ai->personality.aggression);

  if ((enemy->s.effects & EF_QUAD) && best < AI_OUTGUNNED_QUAD_PRIORITY / recklessness) {
    return true;
  }

  if (enemy->client && enemy->client->weapon) {
    return enemy->client->weapon->def.priority - best >= AI_OUTGUNNED_PRIORITY * recklessness;
  }

  return false;
}

/**
 * @see g_ai_main.h
 */
int32_t G_Ai_RetreatHealth(const GameClient *cl) {
  return (int32_t) (AI_RETREAT_HEALTH * Mixf(1.5f, .5f, cl->ai->personality.aggression));
}

/**
 * @return True if the bot should disengage from combat to seek health/armor.
 * Aggressive bots retreat at lower health; cautious bots retreat earlier.
 * Bots also flee from a combat target that outguns them.
 */
bool G_Ai_ShouldRetreat(const GameClient *cl) {
  if (cl->entity->health < G_Ai_RetreatHealth(cl)) {
    return true;
  }

  if (cl->ai->combatTarget.type == AI_GOAL_ENTITY) {
    return G_Ai_IsOutgunned(cl, cl->ai->combatTarget.entity.ent);
  }

  return false;
}

/**
 * @see g_ai_main.h
 */
bool G_Ai_HasLineOfSight(const GameClient *cl, const GameEntity *other) {

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
bool G_Ai_CanSee(const GameClient *cl, const GameEntity *other) {

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
 * @brief Comparison function for sorting item pick candidates by descending weight.
 */
int32_t G_Ai_CompareItems(const void *a, const void *b) {

  const GameAiItemPick *w0 = (const GameAiItemPick *) a;
  const GameAiItemPick *w1 = (const GameAiItemPick *) b;

  return SignOf(w1->weight - w0->weight);
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
 * @see g_ai_main.h
 */
bool G_Ai_IsCovetedItem(const GameClient *cl, const GameEntity *ent) {

  if (!ent->item) {
    return false;
  }

  if (G_Ai_IsMapControlItem(ent->item)) {
    return G_Ai_IsArmed(cl);
  }

  return (ent->spawnFlags & SF_ITEM_DROPPED) && ent->item->def.type == ITEM_TYPE_WEAPON && !cl->inventory[ent->item->def.tag];
}

/**
 * @brief The chance that a bot of no awareness notes when a taken item returns.
 */
#define AI_ITEM_NOTICE_CHANCE_MIN .3f

/**
 * @brief The most error in milliseconds, at skill 0, in the time that a bot remembers for an item's
 * return.
 */
#define AI_ITEM_TIMING_ERROR_MAX 10000.f

/**
 * @see g_ai_main.h
 */
void G_Ai_ItemTaken(const GameEntity *item, const GameEntity *taker) {

  if (taker->client && taker->client->ai) {
    G_Ai_Avoid(taker->client, item);
  }

  if (!item->item || !G_Ai_IsMapControlItem(item->item) || item->solid != SOLID_NOT ||
      item->nextThink <= gameLevel.time) {
    return;
  }

  G_ForEachClient(cl, {
    if (!cl->ai || !cl->entity || cl->entity->dead) {
      continue;
    }

    if (Vec3_Distance(cl->entity->s.origin, item->s.origin) > AI_HEAR_PICKUP_DISTANCE && !G_Ai_CanSee(cl, item)) {
      continue;
    }

    if (Randomf() > Mixf(AI_ITEM_NOTICE_CHANCE_MIN, 1.f, cl->ai->personality.awareness)) {
      continue;
    }

    GameAiItemTiming *slot = &cl->ai->itemTimings[0];
    for (int32_t i = 0; i < AI_ITEM_TIMINGS; i++) {
      GameAiItemTiming *timing = &cl->ai->itemTimings[i];
      if (timing->number == item->s.number) {
        slot = timing;
        break;
      }
      if (timing->respawnTime < slot->respawnTime) {
        slot = timing;
      }
    }

    const float error = Mixf(AI_ITEM_TIMING_ERROR_MAX, 0.f, cl->ai->personality.skill) * RandomRangef(-1.f, 1.f);

    slot->number = item->s.number;
    slot->respawnTime = (uint32_t) Maxf(gameLevel.time + 1.f, item->nextThink + error);
  });
}

/**
 * @brief The time in milliseconds that a bot of skill 0 allows itself to wait at a timed item.
 */
#define AI_ITEM_WAIT_MIN 1000.f

/**
 * @brief The time in milliseconds that a bot of skill 1 allows itself to wait at a timed item.
 */
#define AI_ITEM_WAIT_MAX 10000.f

/**
 * @see g_ai_main.h
 */
uint32_t G_Ai_ItemWaitMillis(const GameClient *cl, const GameEntity *item, const uint32_t eta) {

  if (item->solid == SOLID_TRIGGER) {
    return 0;
  }

  for (int32_t i = 0; i < AI_ITEM_TIMINGS; i++) {
    const GameAiItemTiming *timing = &cl->ai->itemTimings[i];

    if (timing->number == item->s.number && timing->respawnTime > gameLevel.time + eta) {
      return timing->respawnTime - (gameLevel.time + eta);
    }
  }

  return 0;
}

/**
 * @see g_ai_main.h
 */
bool G_Ai_IsTimedItem(const GameClient *cl, const GameEntity *item, const uint32_t eta) {

  for (int32_t i = 0; i < AI_ITEM_TIMINGS; i++) {
    const GameAiItemTiming *timing = &cl->ai->itemTimings[i];

    if (timing->number != item->s.number || timing->respawnTime <= gameLevel.time) {
      continue;
    }

    const uint32_t wait = (uint32_t) Mixf(AI_ITEM_WAIT_MIN, AI_ITEM_WAIT_MAX, cl->ai->personality.skill);

    return timing->respawnTime <= gameLevel.time + eta + wait;
  }

  return false;
}

/**
 * @brief Within this distance, a bot notices a dropped item in any direction.
 */
#define AI_ITEM_NOTICE_DISTANCE 384.f

/**
 * @see g_ai_main.h
 */
bool G_Ai_IsItemKnown(const GameClient *cl, const GameEntity *ent, const bool coveted) {

  if (!(ent->spawnFlags & SF_ITEM_DROPPED)) {
    return true;
  }

  if (coveted || Vec3_Distance(cl->entity->s.origin, ent->s.origin) < AI_ITEM_NOTICE_DISTANCE) {
    return G_Ai_HasLineOfSight(cl, ent);
  }

  return G_Ai_CanSee(cl, ent);
}

/**
 * @see g_ai_main.h
 */
void G_Ai_EndPath(GameClient *cl) {

  G_Ai_ClearGoal(&cl->ai->moveTarget);
  cl->ai->funcGoalNextThinks[AI_FUNC_GOAL_STATE] = 0;
}

/**
 * @brief The minimum interval in milliseconds between wander turns.
 */
#define AI_WANDER_TURN_MILLIS 300

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
  if (!blocked && ent->waterLevel < WATER_WAIST) {
    const Vec3 dropStart = end;
    const Vec3 dropEnd = Vec3_Subtract(dropStart, MakeVec3(0, 0, PM_STEP_HEIGHT * 4.f));
    const CollisionTrace groundTr = gi.Trace(dropStart, dropEnd, Box3_Zero(), ent, CONTENTS_MASK_SOLID);
    blocked = groundTr.fraction >= 1.0f;
  }

  if (blocked && cl->ai->wanderTurnTime <= gameLevel.time) {
    cl->ai->wanderTurnTime = gameLevel.time + AI_WANDER_TURN_MILLIS;

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

/**
 * @brief The preferred distance to a target, by the range that the weapon is made for.
 */
#define AI_RANGE_SHORT 160.f
#define AI_RANGE_MEDIUM 384.f
#define AI_RANGE_LONG 640.f

/**
 * @brief The interval in milliseconds after which a strafing bot may reverse, for the least and
 * the most skilled bots.
 */
#define AI_STRAFE_MILLIS_MAX 1000
#define AI_STRAFE_MILLIS_MIN 400

/**
 * @see g_ai_main.h
 */
float G_Ai_PreferredRange(const GameClient *cl) {

  const uint32_t flags = cl->weapon ? cl->weapon->def.flags : 0;

  if (flags & WF_SHORT_RANGE) {
    return AI_RANGE_SHORT;
  } else if (flags & WF_LONG_RANGE) {
    return AI_RANGE_LONG;
  }

  return AI_RANGE_MEDIUM;
}

/**
 * @return True if the bot can move one body width toward `dir` without hitting a wall or walking
 * off a ledge. The wall test sweeps the bot's box, raised by a step, because the center of the box
 * can pass a corner or a gap that the box does not.
 */
static bool G_Ai_CanStep(const GameEntity *ent, const Vec3 dir) {

  const Vec3 end = Vec3_Fmaf(ent->s.origin, Box3_Size(ent->bounds).x * 2.f, dir);

  Box3 bounds = ent->bounds;
  bounds.mins.z += PM_STEP_HEIGHT;

  if (gi.Trace(ent->s.origin, end, bounds, ent, CONTENTS_MASK_CLIP_PLAYER).fraction < 1.f) {
    return false;
  }

  if (ent->waterLevel >= WATER_WAIST) {
    return true;
  }

  const Vec3 below = Vec3_Subtract(end, MakeVec3(0.f, 0.f, PM_STEP_HEIGHT * 4.f));
  return gi.Trace(end, below, Box3_Zero(), ent, CONTENTS_MASK_SOLID).fraction < 1.f;
}

/**
 * @brief Strafes around the combat target: sideways, plus toward or away from it to hold the
 * weapon's preferred range. The bot reverses at intervals that shorten with skill, and when the
 * way ahead is blocked or ends at a ledge.
 * @return The point to move toward.
 */
static Vec3 G_Ai_Strafe(GameClient *cl, const GameEntity *enemy) {

  const GameEntity *ent = cl->entity;
  float *side = &cl->ai->combatTarget.entity.flankAngle;

  if (*side == 0.f) {
    *side = Randomb() ? -90.f : 90.f;
  }

  const Vec3 delta = MakeVec3(enemy->s.origin.x - ent->s.origin.x, enemy->s.origin.y - ent->s.origin.y, 0.f);
  const float distance = Vec3_Length(delta);
  const Vec3 toEnemy = distance > 1.f ? Vec3_Scale(delta, 1.f / distance) : MakeVec3(1.f, 0.f, 0.f);

  const float range = G_Ai_PreferredRange(cl);
  const float radial = Clampf((distance - range) / range, -1.f, 1.f);

  Vec3 dir = Vec3_Normalize(Vec3_Fmaf(MakeVec3(-toEnemy.y * SignOf(*side), toEnemy.x * SignOf(*side), 0.f), radial, toEnemy));

  const bool blocked = !G_Ai_CanStep(ent, dir);

  if (blocked || cl->ai->strafeTime <= gameLevel.time) {
    const uint32_t millis = (uint32_t) Mixf(AI_STRAFE_MILLIS_MAX, AI_STRAFE_MILLIS_MIN, cl->ai->personality.skill);
    cl->ai->strafeTime = gameLevel.time + RandomRangeu(millis / 2, millis);

    if (blocked || Randomb()) {
      *side = -*side;
      dir = Vec3_Normalize(Vec3_Fmaf(MakeVec3(-toEnemy.y * SignOf(*side), toEnemy.x * SignOf(*side), 0.f), radial, toEnemy));
    }
  }

  return Vec3_Fmaf(ent->s.origin, AI_RANGE_SHORT, dir);
}

static GameEntity *aiCurrentEntity;

/**
 * @brief Ignore ourselves, clipping to the correct mask based on our status.
 */
static CollisionTrace G_Ai_MoveTrace(const Vec3 start, const Vec3 end, const Box3 bounds) {

  const GameEntity *ent= aiCurrentEntity;

  if (G_IsMeat(ent) && ent->dead) {
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
 * @brief Plans a new path from the node nearest the AI to the current path's destination.
 * @return True if the AI has a new path.
 */
static bool G_Ai_ReplanPath(GameClient *cl) {

  GameAiGoal *goal = &cl->ai->moveTarget;

  const GameAiNodeId src = G_Ai_Node_FindClosest(cl->entity->s.origin, 512.f, true, true);
  const GameAiNodeId dest = VectorValue(goal->path.path, GameAiNodeId, goal->path.path->count - 1);

  if (src == AI_NODE_INVALID) {
    return false;
  }

  Vector *path = G_Ai_Node_FindPath(cl, src, dest, G_Ai_Node_Heuristic, 0.f, NULL);

  if (!path) {
    return false;
  }

  G_Ai_SetPathGoal(cl, goal, goal->priority, path, goal->path.pathTarget);
  release(path);

  return true;
}

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

  if (!G_Ai_ReplanPath(cl)) {
    return false;
  }

  if (goal->path.path->count > 1 && G_Ai_Node_RaisedPlatform(goal->path.pathPosition)) {
    G_Ai_AdvancePath(cl, goal);
  }

  G_Ai_Debug("Pathing around a raised plat\n");
  return true;
}

/**
 * @brief The shortest path link in units that a bot takes for a crossing on a jump pad or a
 * teleporter.
 */
#define AI_PUSH_LINK_DISTANCE 400.f

/**
 * @brief The distance in units from the start of a long path link within which a bot looks for
 * the jump pad or teleporter that makes the crossing.
 */
#define AI_PUSH_LINK_RADIUS 128.f

/**
 * @brief The least cosine between the push of a `trigger_push` and a long path link, for the bot
 * to take the push for the crossing of that link.
 */
#define AI_PUSH_LINK_ALIGNMENT .7f

/**
 * @brief The distance in units by which a path link MAY miss a jump pad or teleporter, and still
 * cross on it: the half width of a player.
 */
#define AI_PUSH_LINK_MARGIN 16.f

/**
 * @return True if the horizontal line of the path link from `a` to `b` passes through the given
 * bounds, widened by the half width of a player.
 */
static bool G_Ai_IsOnLink(const Box3 bounds, const Vec3 a, const Vec3 b) {

  const Vec2 center = Vec3_XY(Box3_Center(bounds));
  const Vec2 start = Vec3_XY(a);
  const Vec2 delta = Vec2_Subtract(Vec3_XY(b), start);

  const float t = Clampf(Vec2_Dot(Vec2_Subtract(center, start), delta) / Vec2_Dot(delta, delta), 0.f, 1.f);
  const Vec2 closest = Vec2_Add(start, Vec2_Scale(delta, t));

  return closest.x >= bounds.mins.x - AI_PUSH_LINK_MARGIN && closest.x <= bounds.maxs.x + AI_PUSH_LINK_MARGIN &&
         closest.y >= bounds.mins.y - AI_PUSH_LINK_MARGIN && closest.y <= bounds.maxs.y + AI_PUSH_LINK_MARGIN;
}

/**
 * @return The center of the jump pad or teleporter at the start of the AI's current path link, if
 * the link is too long to walk, and the trigger lies on it. Else, the given destination.
 * @remarks A path heads for the node at the far end of the link, and so it can run past the side
 * of a small jump pad, and off the edge.
 */
static Vec3 G_Ai_PushDestination(const GameClient *cl, const Vec3 dest) {

  const GameAiGoal *goal = &cl->ai->moveTarget;

  if (goal->path.pathIndex == 0) {
    return dest;
  }

  const Vec3 from = G_Ai_Node_GetPosition(VectorValue(goal->path.path, GameAiNodeId, goal->path.pathIndex - 1));

  if (Vec2_Distance(Vec3_XY(from), Vec3_XY(goal->path.pathPosition)) < AI_PUSH_LINK_DISTANCE) {
    return dest;
  }

  GameEntity *ents[32];
  const size_t len = gi.BoxEntities(Box3_FromCenterRadius(from, AI_PUSH_LINK_RADIUS), ents, lengthof(ents), BOX_OCCUPY);

  Vec3 link = Vec3_Subtract(goal->path.pathPosition, from);
  link.z = 0.f;

  for (size_t i = 0; i < len; i++) {

    const GameEntity *ent = ents[i];

    if (!ent->classname) {
      continue;
    }

    if (Str_Compare(ent->classname, "trigger_push") == 0) {
      Vec3 push = ent->moveDir;
      push.z = 0.f;

      if (Vec3_Length(push) > .1f && Vec3_Dot(Vec3_Normalize(push), Vec3_Normalize(link)) < AI_PUSH_LINK_ALIGNMENT) {
        continue;
      }
    } else if (Str_Compare(ent->classname, "misc_teleporter") && Str_Compare(ent->classname, "trigger_teleporter")) {
      continue;
    }

    if (!G_Ai_IsOnLink(ent->absBounds, from, goal->path.pathPosition)) {
      continue;
    }

    const Vec3 center = Box3_Center(ent->absBounds);
    return MakeVec3(center.x, center.y, from.z);
  }

  return dest;
}

/**
 * @brief The longest fall in seconds that `G_Ai_FallsToVoid` follows.
 */
#define AI_FALL_MAX_SECONDS 3.f

/**
 * @brief The time step in seconds with which `G_Ai_FallsToVoid` follows a fall.
 */
#define AI_FALL_STEP_SECONDS .1f

/**
 * @brief The time in seconds for which a bot slides on after it lands, in which it MUST still find
 * a floor below it.
 */
#define AI_FALL_SLIDE_SECONDS .2f

/**
 * @brief The depth in units below the end of the slide within which a bot MUST find a floor, deep
 * enough for a flight of stairs.
 */
#define AI_FALL_SLIDE_DEPTH 128.f

/**
 * @return True if a `trigger_hurt` occupies the given bounds.
 */
static bool G_Ai_IsHurtful(const Box3 bounds) {

  GameEntity *ents[32];
  const size_t len = gi.BoxEntities(bounds, ents, lengthof(ents), BOX_OCCUPY);

  for (size_t i = 0; i < len; i++) {
    if (ents[i]->classname && Str_Compare(ents[i]->classname, "trigger_hurt") == 0) {
      return true;
    }
  }

  return false;
}

/**
 * @brief The spacing in units of the boxes that `G_Ai_CrossesTrigger` tests along a line.
 */
#define AI_TRIGGER_SPACING 16.f

/**
 * @return True if the player's box, moved along the line from `start` to `end`, touches a
 * `trigger_hurt` or a `trigger_push`. Walk links keep out of both, but a bot that cuts a corner of
 * its path does not, and a jump pad beside the path throws it off.
 */
static bool G_Ai_CrossesTrigger(const Vec3 start, const Vec3 end) {

  const float length = Vec3_Distance(start, end);
  const Box3 bounds = G_PlayerBounds();

  for (float d = 0.f; d <= length + AI_TRIGGER_SPACING; d += AI_TRIGGER_SPACING) {

    const Vec3 point = Vec3_Mix(start, end, Minf(1.f, length > 0.f ? d / length : 1.f));

    GameEntity *ents[32];
    const size_t len = gi.BoxEntities(Box3_Translate(bounds, point), ents, lengthof(ents), BOX_OCCUPY);

    for (size_t i = 0; i < len; i++) {
      if (ents[i]->classname && (!Str_Compare(ents[i]->classname, "trigger_hurt") ||
                                 !Str_Compare(ents[i]->classname, "trigger_push"))) {
        return true;
      }
    }
  }

  return false;
}

/**
 * @brief Follows the arc of the AI from the given origin and velocity, with no input, until it
 * lands.
 * @return True if the AI lands on no floor, on the sky, in lava or slime, or in a `trigger_hurt`, or
 * if it slides off the floor on which it lands.
 */
bool G_Ai_FallsToVoid(const GameClient *cl, Vec3 origin, Vec3 velocity) {

  const GameEntity *ent = cl->entity;
  const float gravity = G_LevelGravity();

  for (float time = 0.f; time < AI_FALL_MAX_SECONDS; time += AI_FALL_STEP_SECONDS) {

    const Vec3 end = Vec3_Fmaf(origin, AI_FALL_STEP_SECONDS, velocity);
    velocity.z -= gravity * AI_FALL_STEP_SECONDS;

    const CollisionTrace tr = gi.Trace(origin, end, ent->bounds, ent, CONTENTS_MASK_CLIP_PLAYER);

    if (tr.startSolid) {
      return false;
    }

    if (G_Ai_IsHurtful(Box3_Union(Box3_Translate(ent->bounds, origin), Box3_Translate(ent->bounds, tr.end)))) {
      return true;
    }

    if (gi.PointContents(tr.end) & (CONTENTS_LAVA | CONTENTS_SLIME)) {
      return true;
    }

    if (tr.fraction < 1.f) {

      if (tr.surface & SURF_SKY) {
        return true;
      }

      if (tr.plane.normal.z >= PM_STEP_NORMAL) {
        const Vec3 slide = Vec3_Fmaf(tr.end, AI_FALL_SLIDE_SECONDS, MakeVec3(velocity.x, velocity.y, 0.f));
        const CollisionTrace wall = gi.Trace(tr.end, slide, ent->bounds, ent, CONTENTS_MASK_CLIP_PLAYER);

        if (wall.fraction < 1.f) {
          return false;
        }

        const Vec3 below = Vec3_Fmaf(slide, AI_FALL_SLIDE_DEPTH, Vec3_Down());
        return gi.Trace(slide, below, ent->bounds, ent, CONTENTS_MASK_CLIP_PLAYER).fraction == 1.f;
      }

      velocity = Vec3_Subtract(velocity, Vec3_Scale(tr.plane.normal, Vec3_Dot(velocity, tr.plane.normal)));
    }

    origin = tr.end;
  }

  return true;
}

/**
 * @return The velocity with which the AI leaves the ground, if it jumps now.
 */
Vec3 G_Ai_JumpVelocity(const GameClient *cl) {
  return MakeVec3(cl->entity->velocity.x, cl->entity->velocity.y, cl->ps.pmState.params.speedJump);
}

/**
 * @brief The horizontal speed, as a fraction of the run speed, above which a bot no longer
 * controls where it goes, as after a jump pad or a knockback.
 */
#define AI_OVERSPEED_SCALE 1.2f

/**
 * @return True if the entity moves faster than it can run. A path link was walked by a player, who
 * steered in the air, so a fall at run speed MAY look short with no input, and still land.
 */
static bool G_Ai_IsOverspeed(const GameEntity *ent) {
  return Vec2_Length(Vec3_XY(ent->velocity)) > PM_SPEED_RUN * AI_OVERSPEED_SCALE;
}

/**
 * @return The yaw of the view that the command turns to. `G_Ai_Turn` sets the command's angles
 * before `G_Ai_Move` runs, and the player move applies the move relative to them, not to the view
 * before the command. In combat the two differ by several degrees each frame.
 */
static inline float G_Ai_CommandYaw(const GameClient *cl, const PMoveCmd *cmd) {
  return cmd->angles.y + cl->ps.pmState.deltaAngles.y;
}

/**
 * @brief Steers the AI back toward the place at which it left the ground, if it is in the air
 * with no path to follow, and its fall ends in the void. A path plans its own jumps and drops.
 */
static void G_Ai_AvoidVoid(GameClient *cl, PMoveCmd *cmd) {

  const GameEntity *ent = cl->entity;

  if (ent->ground.ent ||
      cl->ai->moveTarget.type == AI_GOAL_PATH ||
      ent->waterLevel >= WATER_WAIST ||
      (cl->ps.pmState.flags & PMF_ON_LADDER)) {
    return;
  }

  if (cl->ai->fallFrame != gameLevel.frameNum) {
    cl->ai->fallFrame = gameLevel.frameNum;
    cl->ai->fallsToVoid = G_Ai_FallsToVoid(cl, ent->s.origin, ent->velocity);
  }

  if (!cl->ai->fallsToVoid) {
    return;
  }

  Vec3 back = Vec3_Subtract(cl->ai->groundPosition, ent->s.origin);
  back.z = 0.f;

  if (Vec3_Length(back) < 1.f) {
    back = Vec3_Negate(ent->velocity);
    back.z = 0.f;
  }

  Vec3 dir;
  Vec3_Vectors(MakeVec3(0.f, G_Ai_CommandYaw(cl, cmd) - Vec3_Euler(Vec3_Normalize(back)).y, 0.f), &dir, NULL, NULL);

  cmd->forward = dir.x * PM_SPEED_RUN;
  cmd->right = dir.y * PM_SPEED_RUN;
  cmd->up = 0;
}

/**
 * @brief The longest hold in milliseconds from the first launch of a `trigger_push`, after which
 * the AI moves again, so that it cannot stay still where it finds no ground, or inside a push.
 */
#define AI_PUSH_MAX_MILLIS 5000

/**
 * @brief Holds the AI still from the time that a `trigger_push` launches it until it lands, because
 * any move input in the air steers it off the arc of the jump pad. On landing, the AI plans a new
 * path, because the flight can pass over the nodes of the old one.
 * @remarks The flight lasts at least as long as `PMF_TIME_PUSHED` is set, because some movement
 * modes keep the ground of the launch for that time.
 * @return True if the AI is in flight from a `trigger_push`.
 */
static bool G_Ai_FollowPush(GameClient *cl) {

  const GameEntity *ent = cl->entity;

  if (!cl->ai->pushTime) {
    return false;
  }

  const bool held = ent->waterLevel < WATER_WAIST && !(cl->ps.pmState.flags & PMF_ON_LADDER);

  if (held && gameLevel.time - cl->ai->pushTime < AI_PUSH_MAX_MILLIS) {

    if (cl->ps.pmState.flags & PMF_TIME_PUSHED) {
      return true;
    }

    if (!ent->ground.ent) {
      return true;
    }
  }

  cl->ai->pushTime = 0;

  if (cl->ai->moveTarget.type == AI_GOAL_PATH) {
    G_Ai_ReplanPath(cl);
  }

  return false;
}

/**
 * @return The command direction that takes the AI out from under the given plat, which its trigger
 * would otherwise hold up, or zero if the AI is already clear of it.
 */
static Vec3 G_Ai_LeavePlatform(const GameClient *cl, const PMoveCmd *cmd, const GameEntity *plat) {

  const Box3 footprint = Box3_Expand3(plat->absBounds, MakeVec3(-16.f, -16.f, 0.f));
  const Box3 bounds = cl->entity->absBounds;

  if (bounds.maxs.x < footprint.mins.x || bounds.mins.x > footprint.maxs.x ||
      bounds.maxs.y < footprint.mins.y || bounds.mins.y > footprint.maxs.y) {
    return Vec3_Zero();
  }

  Vec3 away = Vec3_Subtract(cl->entity->s.origin, Box3_Center(plat->absBounds));
  away.z = 0.f;

  Vec3 dir;
  Vec3_Vectors(MakeVec3(0.f, G_Ai_CommandYaw(cl, cmd) - Vec3_Euler(Vec3_Normalize(away)).y, 0.f), &dir, NULL, NULL);

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
 * @return True if the node lies within the horizontal extent of the given bounds.
 */
static bool G_Ai_IsWithinXY(const Box3 bounds, const Vec3 node) {
  return node.x >= bounds.mins.x && node.x <= bounds.maxs.x &&
         node.y >= bounds.mins.y && node.y <= bounds.maxs.y;
}

/**
 * @return True if a rising mover, such as a plat, has lifted the AI off the node, which lies on the
 * mover's floor where it started.
 */
static bool G_Ai_IsLiftedOffNode(const GameClient *cl, const Box3 bounds, const Vec3 node) {

  const GameEntity *ground = cl->entity->ground.ent;

  if (!ground || ground->s.number == 0 || ground->velocity.z <= 0.f) {
    return false;
  }

  return G_Ai_IsWithinXY(bounds, node) && G_Ai_IsWithinXY(ground->absBounds, node) && node.z < bounds.mins.z;
}

/**
 * @return True if a jump pad has launched the AI off the node, which starts a jump pad link on its
 * path. A vertical pad launches the AI before its box reaches the node, and the AI must steer for
 * the ledge that the link leads to, not back over the pad.
 */
static bool G_Ai_IsPushedOffNode(const GameClient *cl, const GameAiGoal *goal, const Box3 bounds) {

  const Vector *path = goal->path.path;
  const uint32_t index = goal->path.pathIndex;

  if (cl->entity->ground.ent || index + 1 >= path->count) {
    return false;
  }

  const GameAiLink *link = G_Ai_Node_GetLink(VectorValue(path, GameAiNodeId, index),
                                             VectorValue(path, GameAiNodeId, index + 1));

  if (!link || link->travelType != NAV_TRAVEL_JUMP_PAD) {
    return false;
  }

  return G_Ai_IsWithinXY(bounds, goal->path.pathPosition) && goal->path.pathPosition.z < bounds.mins.z;
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

  const Box3 bounds = Box3_Expand3(cl->entity->absBounds, padding);

  // if we're touching our nav goal, we can go next
  if (Box3_ContainsPoint(bounds, goal->path.pathPosition)) {
    return G_Ai_AdvancePath(cl, goal);
  }

  if (G_Ai_IsLiftedOffNode(cl, bounds, goal->path.pathPosition)) {
    return G_Ai_AdvancePath(cl, goal);
  }

  if (G_Ai_IsPushedOffNode(cl, goal, bounds)) {
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
 * @brief The horizontal distance from a barrier jump's target at which a bot jumps.
 */
#define AI_BARRIER_JUMP_DISTANCE 48.f

/**
 * @return True if the bot is on the ground, within `AI_BARRIER_JUMP_DISTANCE` of the target of a
 * barrier jump link on its path.
 */
static bool G_Ai_ShouldBarrierJump(const GameClient *cl) {

  if (!cl->entity->ground.ent || cl->ai->moveTarget.path.pathIndex == 0) {
    return false;
  }

  const Vector *path = cl->ai->moveTarget.path.path;
  const uint32_t index = cl->ai->moveTarget.path.pathIndex;

  const GameAiLink *link = G_Ai_Node_GetLink(VectorValue(path, GameAiNodeId, index - 1),
                                             VectorValue(path, GameAiNodeId, index));

  if (!link || link->travelType != NAV_TRAVEL_BARRIER_JUMP) {
    return false;
  }

  const Vec2 delta = Vec2_Subtract(Vec3_XY(cl->ai->moveTarget.path.pathPosition), Vec3_XY(cl->entity->s.origin));

  return Vec2_Length(delta) <= AI_BARRIER_JUMP_DISTANCE;
}

/**
 * @brief The horizontal distance from the line of its path link beyond which a bot on the ground is
 * off its path, as after knockback, and plans a new one.
 */
#define AI_OFF_PATH_DISTANCE 128.f

/**
 * @brief The height below both ends of its path link at which a bot on the ground has fallen off
 * its path, and plans a new one.
 */
#define AI_OFF_PATH_DROP 64.f

/**
 * @brief The horizontal distance from the line of a walk-off-ledge link within which a bot at the
 * edge walks off it. Elsewhere, the fall MAY miss the floor that the link drops to.
 */
#define AI_LEDGE_DISTANCE 48.f

/**
 * @brief The fraction of the run speed at which a bot walks a walk-off-ledge link, so that its fall
 * carries it no farther than the floor below.
 */
#define AI_LEDGE_SPEED .5f

/**
 * @return The link into the node that the bot moves to, or NULL at the start of its path.
 */
static const GameAiLink *G_Ai_PathLink(const GameClient *cl) {

  const GameAiGoal *goal = &cl->ai->moveTarget;

  if (goal->type != AI_GOAL_PATH || goal->path.pathIndex == 0) {
    return NULL;
  }

  return G_Ai_Node_GetLink(VectorValue(goal->path.path, GameAiNodeId, goal->path.pathIndex - 1),
                           VectorValue(goal->path.path, GameAiNodeId, goal->path.pathIndex));
}

/**
 * @return The horizontal distance from the bot to the line of the link into the node that it moves
 * to.
 */
static float G_Ai_PathLinkDistance(const GameClient *cl) {

  const GameAiGoal *goal = &cl->ai->moveTarget;

  const Vec2 a = Vec3_XY(G_Ai_Node_GetPosition(VectorValue(goal->path.path, GameAiNodeId, goal->path.pathIndex - 1)));
  const Vec2 b = Vec3_XY(goal->path.pathPosition);
  const Vec2 p = Vec3_XY(cl->entity->s.origin);

  const Vec2 ab = Vec2_Subtract(b, a);
  const float length = Vec2_LengthSquared(ab);
  const float t = length > 0.f ? Clampf(Vec2_Dot(Vec2_Subtract(p, a), ab) / length, 0.f, 1.f) : 0.f;

  return Vec2_Length(Vec2_Subtract(p, Vec2_Add(a, Vec2_Scale(ab, t))));
}

/**
 * @return True if the bot is on the ground or in water to the waist, far from the walk, crouch,
 * ledge or jump link that it follows, or below both of its ends, so that the link no longer leads
 * where it goes. A bot that falls into water off its path plans a new one, which swims out, and a
 * bot that falls short of a jump plans one from where it lands.
 */
static bool G_Ai_IsOffPath(const GameClient *cl) {

  if ((!cl->entity->ground.ent && cl->entity->waterLevel < WATER_WAIST) || cl->ai->strafeJumping) {
    return false;
  }

  const GameAiLink *link = G_Ai_PathLink(cl);

  if (!link || ((link->travelType < NAV_TRAVEL_WALK || link->travelType > NAV_TRAVEL_BARRIER_JUMP) &&
                link->travelType != NAV_TRAVEL_JUMP)) {
    return false;
  }

  const GameAiGoal *goal = &cl->ai->moveTarget;
  const Vec3 a = G_Ai_Node_GetPosition(VectorValue(goal->path.path, GameAiNodeId, goal->path.pathIndex - 1));
  const float z = Minf(a.z, goal->path.pathPosition.z);

  return G_Ai_PathLinkDistance(cl) > AI_OFF_PATH_DISTANCE || cl->entity->s.origin.z < z - AI_OFF_PATH_DROP;
}

/**
 * @brief The distance ahead of the bot, on the line of a jump link, to which it steers.
 */
#define AI_JUMP_LOOK_DISTANCE 32.f

/**
 * @brief The farthest that a bot may be from the line of a jump link, to jump.
 */
#define AI_JUMP_LINE_DISTANCE 16.f

/**
 * @return True if the bot is on the line from the source node of its jump link to the target, which
 * the navigation build simulated the jump along: within `AI_JUMP_LINE_DISTANCE` of it, between the
 * source and the target, and on the source's level. Off that line, the jump can hit a wall or fall
 * short, and past the target, or below the source after a fall, it MUST NOT jump again.
 * @param along The distance along the line from the source node, or NULL.
 */
static bool G_Ai_IsOnJumpLine(const GameClient *cl, float *along) {

  const GameAiGoal *goal = &cl->ai->moveTarget;

  const Vec3 source = G_Ai_Node_GetPosition(VectorValue(goal->path.path, GameAiNodeId, goal->path.pathIndex - 1));

  if (fabsf(cl->entity->s.origin.z - source.z) > PM_STEP_HEIGHT * 2.f) {
    return false;
  }

  const Vec2 a = Vec3_XY(source);
  const Vec2 ab = Vec2_Subtract(Vec3_XY(goal->path.pathPosition), a);
  const Vec2 ap = Vec2_Subtract(Vec3_XY(cl->entity->s.origin), a);

  const float length = Vec2_Length(ab);
  if (length < 1.f) {
    return false;
  }

  const float distance = Vec2_Dot(ap, ab) / length;
  const float lateral = fabsf(ab.x * ap.y - ab.y * ap.x) / length;

  if (along) {
    *along = distance;
  }

  return lateral <= AI_JUMP_LINE_DISTANCE && distance >= -AI_JUMP_LINE_DISTANCE && distance < length;
}

/**
 * @return The point to which a bot on a jump link steers: `AI_JUMP_LOOK_DISTANCE` ahead of it on
 * the line of the jump. Off that line, it steers to the target if it stands on the target's level,
 * nearer the target than the source, as after a jump that lands past the target, and otherwise to
 * the source.
 */
static Vec3 G_Ai_JumpDestination(const GameClient *cl) {

  const GameAiGoal *goal = &cl->ai->moveTarget;

  const Vec3 a = G_Ai_Node_GetPosition(VectorValue(goal->path.path, GameAiNodeId, goal->path.pathIndex - 1));
  const Vec3 b = goal->path.pathPosition;

  float along;
  if (!G_Ai_IsOnJumpLine(cl, &along)) {
    const Vec3 origin = cl->entity->s.origin;
    if (fabsf(origin.z - b.z) <= PM_STEP_HEIGHT && Vec3_Distance(origin, b) < Vec3_Distance(origin, a)) {
      return b;
    }
    return a;
  }

  const float length = Vec2_Distance(Vec3_XY(a), Vec3_XY(b));

  return Vec3_Mix(a, b, Clampf((along + AI_JUMP_LOOK_DISTANCE) / length, 0.f, 1.f));
}

/**
 * @brief Plans a new path for a bot that is off its path, at most once per `AI_REPLAN_MILLIS`.
 * @return True if the bot has a new path.
 */
static bool G_Ai_ReturnToPath(GameClient *cl) {

  if (cl->ai->replanTime > gameLevel.time) {
    return false;
  }

  cl->ai->replanTime = gameLevel.time + AI_REPLAN_MILLIS;

  if (!G_Ai_ReplanPath(cl)) {
    return false;
  }

  G_Ai_Debug("Off the path; planned a new one\n");
  return true;
}

/**
 * @brief The least skill at which a bot strafe jumps along its path.
 */
#define AI_STRAFE_JUMP_SKILL .25f

/**
 * @brief The most path nodes that a strafe jumping bot looks ahead.
 */
#define AI_STRAFE_LOOK_NODES 12

/**
 * @brief The sharpest turn in degrees, at `AI_STRAFE_JUMP_SKILL`, that a bot takes in a strafe jump.
 */
#define AI_STRAFE_TURN_MIN 15.f

/**
 * @brief The sharpest turn in degrees, at skill 1, that a bot takes in a strafe jump.
 */
#define AI_STRAFE_TURN_MAX 60.f

/**
 * @brief The distance within which a strafe jumping bot ignores the turns of its path, which are
 * the steps of the grid: one and a half cells.
 */
#define AI_STRAFE_NEAR_DISTANCE 96.f

/**
 * @brief The most error in degrees, at skill 0, in the wish direction of a strafe jumping bot.
 */
#define AI_STRAFE_ERROR 20.f

/**
 * @brief The spacing in units of the floor samples under the line to the node that a bot steers to.
 */
#define AI_STRAFE_FLOOR_SPACING 32.f

/**
 * @brief The interval in milliseconds at which a strafe jumping bot looks ahead again, unless it
 * reaches a node first.
 */
#define AI_STRAFE_LOOK_MILLIS 100

/**
 * @return The speed toward which the level's movement accelerates a player in the air, or 0 if
 * bots do not strafe jump in it.
 */
static float G_Ai_AirWishSpeed(const GameClient *cl) {

  switch (gameLevel.movement) {
    case PM_MOVEMENT_QUETOO:
      return cl->ps.pmState.params.speedAir;
    case PM_MOVEMENT_QUAKE2:
    case PM_MOVEMENT_RACE:
      return cl->ps.pmState.params.speedGround;
    default:
      return 0.f;
  }
}

/**
 * @return True if the bot can strafe jump in its current state, on its current path.
 */
static bool G_Ai_CanStrafeJump(const GameClient *cl) {

  const GameEntity *ent = cl->entity;
  const GameAiGoal *goal = &cl->ai->moveTarget;

  if (cl->ai->personality.skill < AI_STRAFE_JUMP_SKILL || G_Ai_AirWishSpeed(cl) <= 0.f) {
    return false;
  }

  if (goal->type != AI_GOAL_PATH || goal->path.trickJump) {
    return false;
  }

  if (ent->waterLevel > WATER_FEET || G_Ai_IsRidingMover(ent)) {
    return false;
  }

  const uint32_t flags = PMF_DUCKED | PMF_ON_LADDER | PMF_TIME_PUSHED | PMF_TIME_TELEPORT | PMF_TIME_WATER_JUMP;
  if (cl->ps.pmState.flags & flags) {
    return false;
  }

  return G_Ai_Path_CanPathTo(goal->path.path, goal->path.pathIndex);
}

/**
 * @return True if the path link into the node at `index` is one that a bot can run along. Untyped
 * links, from nodes that the game adds, qualify, because the line and floor checks of
 * `G_Ai_StrafeTarget` decide for them.
 */
static bool G_Ai_IsRunLink(const Vector *path, const uint32_t index) {

  if (index == 0) {
    return true;
  }

  const GameAiLink *link = G_Ai_Node_GetLink(VectorValue(path, GameAiNodeId, index - 1),
                                             VectorValue(path, GameAiNodeId, index));

  return link && (link->travelType == 0 || link->travelType == NAV_TRAVEL_WALK);
}

/**
 * @return True if there is floor, and no liquid, under the line from `a` to `b`, which are player
 * origins.
 */
static bool G_Ai_HasFloor(const GameEntity *ent, const Vec3 a, const Vec3 b) {

  const float length = Vec2_Length(Vec2_Subtract(Vec3_XY(b), Vec3_XY(a)));
  const float depth = -PM_BOUNDS.mins.z + PM_STEP_HEIGHT * 2.f;

  for (float d = AI_STRAFE_FLOOR_SPACING; d < length; d += AI_STRAFE_FLOOR_SPACING) {

    const Vec3 point = Vec3_Mix(a, b, d / length);
    const Vec3 start = Vec3_Add(point, MakeVec3(0.f, 0.f, PM_STEP_HEIGHT));
    const Vec3 end = Vec3_Subtract(point, MakeVec3(0.f, 0.f, depth));

    const CollisionTrace tr = gi.Trace(start, end, Box3_Zero(), ent, CONTENTS_MASK_SOLID | CONTENTS_MASK_LIQUID);

    if (tr.fraction == 1.f || (tr.contents & CONTENTS_MASK_LIQUID)) {
      return false;
    }
  }

  return true;
}

/**
 * @return The sharpest turn in degrees that the bot takes in a strafe jump, which grows with skill.
 */
static float G_Ai_StrafeTurn(const GameClient *cl) {

  const float t = (cl->ai->personality.skill - AI_STRAFE_JUMP_SKILL) / (1.f - AI_STRAFE_JUMP_SKILL);

  return Mixf(AI_STRAFE_TURN_MIN, AI_STRAFE_TURN_MAX, Clampf(t, 0.f, 1.f));
}

/**
 * @brief Finds the node that a strafe jumping bot steers to: the farthest node ahead on its path,
 * over run links, before the path turns more than `G_Ai_StrafeTurn` from the bot's heading. The
 * heading is the bot's velocity, or if it moves slower than half the run speed, the direction to the
 * first node beyond `AI_STRAFE_NEAR_DISTANCE`. Nodes within that distance are passed. The line to
 * the node MUST be clear for the player's box, touch no `trigger_hurt` or `trigger_push`, and have
 * floor under it, with no liquid, because the bot cuts the corners of its path on the way. The result is kept for `AI_STRAFE_LOOK_MILLIS`, and
 * `G_Ai_SetPathGoal` clears it.
 * @param run The distance along the path to that node.
 * @return The index of that node in the path, or -1 if no node passes: the path turns, its next
 * link is not a run link, or the line to its next node is blocked.
 */
static int32_t G_Ai_StrafeTarget(GameClient *cl, float *run) {

  GameAi *ai = cl->ai;
  const GameEntity *ent = cl->entity;
  const GameAiGoal *goal = &ai->moveTarget;
  const Vector *path = goal->path.path;
  const Vec3 origin = MakeVec3(ent->s.origin.x, ent->s.origin.y, 0.f);
  const Vec3 floor = MakeVec3(ent->s.origin.x, ent->s.origin.y, ent->ground.ent ? ent->s.origin.z : ai->groundPosition.z);

  Box3 bounds = Pm_Bounds(&cl->ps.pmState.params, false);
  bounds.mins.z += PM_STEP_HEIGHT;

  if (ai->strafeLookTime > gameLevel.time && ai->strafeLookPath == path &&
      ai->strafeLookPathIndex == goal->path.pathIndex && ai->strafeLookIndex < (int32_t) path->count) {
    *run = ai->strafeLookRun;
    return ai->strafeLookIndex;
  }

  const Vec3 velocity = MakeVec3(ent->velocity.x, ent->velocity.y, 0.f);
  Vec3 heading = Vec3_Length(velocity) >= PM_SPEED_RUN * .5f ? Vec3_Normalize(velocity) : Vec3_Zero();

  const float limit = cosf(Radians(G_Ai_StrafeTurn(cl)));

  int32_t target = -1;
  float walked = 0.f;
  Vec3 previous = origin;

  *run = 0.f;

  for (uint32_t i = goal->path.pathIndex; i < path->count && i < goal->path.pathIndex + AI_STRAFE_LOOK_NODES; i++) {

    if (!G_Ai_IsRunLink(path, i)) {
      break;
    }

    const Vec3 node = G_Ai_Node_GetPosition(VectorValue(path, GameAiNodeId, i));
    const Vec3 position = MakeVec3(node.x, node.y, 0.f);

    walked += Vec3_Distance(position, previous);
    previous = position;

    float distance;
    const Vec3 to = Vec3_NormalizeLength(Vec3_Subtract(position, origin), &distance);

    if (distance > AI_STRAFE_NEAR_DISTANCE) {
      if (Vec3_Equal(heading, Vec3_Zero())) {
        heading = to;
      } else if (Vec3_Dot(to, heading) < limit) {
        break;
      }
    }

    const CollisionTrace tr = gi.Trace(ent->s.origin, node, bounds, ent, CONTENTS_MASK_CLIP_PLAYER);

    if (tr.startSolid || tr.fraction < 1.f || !G_Ai_HasFloor(ent, floor, node) || G_Ai_CrossesTrigger(ent->s.origin, node)) {
      break;
    }

    target = (int32_t) i;
    *run = walked;
  }

  ai->strafeLookTime = gameLevel.time + AI_STRAFE_LOOK_MILLIS;
  ai->strafeLookPath = path;
  ai->strafeLookPathIndex = goal->path.pathIndex;
  ai->strafeLookIndex = target;
  ai->strafeLookRun = *run;

  return target;
}

/**
 * @return True if the bot should jump, now or on landing: the path ahead of it runs within its turn
 * for longer than one jump.
 * @param run The distance along the path to the node that the bot steers to.
 */
static bool G_Ai_ShouldStrafeJump(const GameClient *cl, const float run) {

  const PMoveParams *params = &cl->ps.pmState.params;
  const float speed = Vec2_Length(Vec3_XY(cl->entity->velocity));

  const float airTime = 2.f * params->speedJump / Maxf(1.f, params->gravity);

  return run >= Maxf(speed, PM_SPEED_RUN) * airTime;
}

/**
 * @return The horizontal direction in which a strafe jumping bot at `origin`, moving at `velocity`,
 * should wish to move, to gain speed in the air while it turns toward the target. Air acceleration
 * adds speed only while the velocity's projection on the wish direction is below the wish speed, so
 * the wish direction that gains the most lies at the angle whose cosine is `(cap - gain) / speed`
 * from the velocity. A target farther off than that angle turns the wish direction toward it, up to
 * a right angle.
 * @param error The most error in degrees to add to the angle.
 */
static Vec3 G_Ai_StrafeWish(const GameClient *cl, const Vec3 origin, Vec3 velocity, const Vec3 target,
                            const uint8_t msec, const float error) {

  Vec3 to = Vec3_Subtract(target, origin);
  to.z = 0.f;
  to = Vec3_Normalize(to);

  velocity.z = 0.f;

  float speed;
  const Vec3 dir = Vec3_NormalizeLength(velocity, &speed);

  const float cap = G_Ai_AirWishSpeed(cl);
  const float gain = cl->ps.pmState.params.accelAir * cap * MILLIS_TO_SECONDS(msec);

  if (speed <= cap - gain) {
    return to;
  }

  const float optimal = acosf(Clampf((cap - gain) / speed, -1.f, 1.f));
  const float off = atan2f(dir.x * to.y - dir.y * to.x, Vec3_Dot(dir, to));

  float angle = Clampf(fabsf(off), optimal, (float) M_PI_2);
  angle += Radians(RandomRangef(-1.f, 1.f) * error);
  angle = copysignf(angle, off);

  const float c = cosf(angle), s = sinf(angle);

  return MakeVec3(dir.x * c - dir.y * s, dir.x * s + dir.y * c, 0.f);
}

/**
 * @brief The longest drop that `G_Ai_SimulateDrop` follows, in milliseconds.
 */
#define AI_DROP_MILLIS 2000

/**
 * @brief Simulates the bot's walk off a ledge with `Pm_Move`. The bot steers straight at the
 * target, as `G_Ai_Move` does in the air.
 * @return True if the drop lands on the floor no lower than the target, or in water to the waist,
 * from where the bot swims to the target, and touches no `trigger_hurt`, lava or slime on the way.
 */
static bool G_Ai_SimulateDrop(GameClient *cl, const Vec3 target) {

  GameEntity *ent = cl->entity;

  PMove pm;
  memset(&pm, 0, sizeof(pm));

  pm.s = cl->ps.pmState;
  pm.s.origin = ent->s.origin;
  pm.s.velocity = ent->velocity;
  pm.s.type = PM_NORMAL;
  pm.ground = ent->ground;

  pm.PointContents = gi.PointContents;
  pm.BoxContents = gi.BoxContents;
  pm.Trace = G_Ai_MoveTrace;

  pm.Debug = gi.Debug;
  pm.DebugMask = gi.DebugMask;
  pm.debugMask = DEBUG_PMOVE_SERVER;

  aiCurrentEntity = ent;

  const float floor = target.z - PM_STEP_HEIGHT * 2.f;
  const uint8_t msec = (uint8_t) ceilf(QUETOO_TICK_MILLIS / 3.f);

  bool dropped = false;

  for (uint32_t millis = 0; millis < AI_DROP_MILLIS; millis += msec) {

    float speed;
    Vec3 wish = Vec3_Subtract(target, pm.s.origin);
    wish.z = 0.f;
    wish = Vec3_NormalizeLength(wish, &speed);
    speed = pm.ground.ent ? PM_SPEED_RUN * AI_LEDGE_SPEED : Clampf(speed, 10.f, PM_SPEED_RUN);

    pm.cmd = (PMoveCmd) {
      .msec = msec,
      .angles = MakeVec3(0.f, Vec3_Euler(wish).y, 0.f),
      .forward = (int16_t) speed,
    };

    pm.cmd.angles = Vec3_Subtract(pm.cmd.angles, pm.s.deltaAngles);

    Pm_Move(&pm);

    dropped |= !pm.ground.ent;

    const Box3 bounds = Box3_Translate(Pm_Bounds(&pm.s.params, false), pm.s.origin);

    if (G_Ai_IsHurtful(bounds) || (gi.PointContents(pm.s.origin) & (CONTENTS_LAVA | CONTENTS_SLIME))) {
      return false;
    }

    if (dropped && pm.waterLevel >= WATER_WAIST) {
      return true;
    }

    if (dropped && pm.ground.ent) {
      return pm.s.origin.z >= floor;
    }
  }

  return false;
}

/**
 * @brief Strafe jumps along the bot's path, as skilled players do to gain speed. On the ground the
 * bot jumps. In the air it releases jump while it rises, and holds it while it falls, so that it
 * jumps again on landing, before friction slows it. It steers at the optimal strafe angle toward
 * the farthest node of `G_Ai_StrafeTarget`. Before a turn sharper than `G_Ai_StrafeTurn`, it lands
 * and runs, to turn on the ground.
 * @param dir The move direction relative to the view, as `G_Ai_Move` builds it.
 * @return True if the bot strafe jumps, and `dir` and `cmd->up` are set.
 */
static bool G_Ai_StrafeJump(GameClient *cl, PMoveCmd *cmd, Vec3 *dir) {

  GameAi *ai = cl->ai;
  const GameEntity *ent = cl->entity;

  if (!G_Ai_CanStrafeJump(cl)) {
    ai->strafeJumping = false;
    return false;
  }

  float run;
  const int32_t steer = G_Ai_StrafeTarget(cl, &run);

  if (steer < 0) {
    ai->strafeJumping = false;
    return false;
  }

  if (ent->ground.ent) {
    if (!G_Ai_ShouldStrafeJump(cl, run)) {
      ai->strafeJumping = false;
      return false;
    }

    ai->strafeJumping = true;
    cmd->up = (cl->ps.pmState.flags & PMF_JUMP_HELD) ? 0 : PM_SPEED_JUMP;
  } else if (ai->strafeJumping) {
    cmd->up = (ent->velocity.z <= 0.f && G_Ai_ShouldStrafeJump(cl, run)) ? PM_SPEED_JUMP : 0;
  } else {
    return false;
  }

  while (ai->moveTarget.path.pathIndex < (uint32_t) steer) {
    if (!G_Ai_AdvancePath(cl, &ai->moveTarget)) {
      break;
    }
  }

  const Vec3 target = G_Ai_Node_GetPosition(VectorValue(ai->moveTarget.path.path, GameAiNodeId, steer));
  const Vec3 wish = G_Ai_StrafeWish(cl, ent->s.origin, ent->velocity, target, cmd->msec,
                                    AI_STRAFE_ERROR * (1.f - ai->personality.skill));
  const float deltaYaw = G_Ai_CommandYaw(cl, cmd) - Vec3_Euler(wish).y;

  Vec3_Vectors(MakeVec3(0.f, deltaYaw, 0.f), dir, NULL, NULL);
  *dir = Vec3_Scale(*dir, Maxf(G_Ai_AirWishSpeed(cl), PM_SPEED_RUN));

  return true;
}

/**
 * @brief Updates the positions of the bot's next nodes, which move if they stand on a model that
 * moves, and plans a new path at once if the link into its next node is gone, as a link to a node
 * that moves is once the model moves on. If no path comes back, the bot drops its path, so that it
 * does not follow a node that the model carries away, over a gap or a lava pit.
 */
static void G_Ai_RefreshPath(GameClient *cl) {

  GameAiGoal *goal = &cl->ai->moveTarget;

  if (goal->type != AI_GOAL_PATH || !goal->path.path || goal->path.pathIndex >= goal->path.path->count) {
    return;
  }

  const Vector *path = goal->path.path;
  const uint32_t index = goal->path.pathIndex;

  goal->path.pathPosition = G_Ai_Node_GetPosition(VectorValue(path, GameAiNodeId, index));
  goal->path.nextPathPosition = G_Ai_Node_GetPosition(VectorValue(path, GameAiNodeId, Minz(index + 1, path->count - 1)));

  if (index > 0 && !G_Ai_Node_IsLinked(VectorValue(path, GameAiNodeId, index - 1), VectorValue(path, GameAiNodeId, index))) {
    if (!G_Ai_ReplanPath(cl)) {
      G_Ai_ClearGoal(goal);
    }
  }
}

/**
 * @brief Move towards our current target
 */
static uint32_t G_Ai_Move(GameClient *cl, PMoveCmd *cmd) {

  GameEntity *ent = cl->entity;

  G_Ai_RefreshPath(cl);

  if (ent->ground.ent) {
    cl->ai->groundPosition = ent->s.origin;
  }

  if (cl->ai->moveTarget.type != AI_GOAL_PATH) {
    cl->ai->strafeJumping = false;
  }

  if (G_Ai_FollowPush(cl)) {
    cmd->forward = 0;
    cmd->right = 0;
    cmd->up = 0;
    return 0;
  }

  if (cl->ai->moveTarget.type == AI_GOAL_NONE && G_Ai_IsWaiting(cl)) {
    cmd->forward = 0;
    cmd->right = 0;
    cmd->up = 0;
    return 0;
  }

  bool targetEnemy = false;
  bool toPush = false;

  Vec3 dir, angles, dest = Vec3_Zero();
  bool moveWander = false;

  // Resolve the move goal iteratively. The original code used tail-recursive calls
  // when the path check or distress check failed, which could overflow the stack since
  // G_Ai_Move has a large frame (~6.5 KB for two PMove locals). Three tries matches
  // the maximum depth of the original recursion.
  bool goalReady = false;
  for (int32_t tries = 0; tries < 3 && !goalReady; tries++) {

    targetEnemy = false;
    toPush = false;
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
          dest = G_Ai_Strafe(cl, cl->ai->moveTarget.entity.ent);
          targetEnemy = true;
          break;
        case AI_COMBAT_WANDER:
          moveWander = true;
          targetEnemy = true;
          break;
        }
        break;
      case AI_GOAL_PATH:
        if (!G_Ai_CheckNav(cl, &cl->ai->moveTarget)) {
          if (!G_Ai_ApproachPathTarget(cl)) {
            G_Ai_EndPath(cl);
          }
          continue;
        }

        if (G_Ai_AvoidRaisedPlatform(cl)) {
          continue;
        }

        if (G_Ai_IsOffPath(cl) && G_Ai_ReturnToPath(cl)) {
          continue;
        }

        if (cl->ai->moveTarget.path.trickJump) {
          dest = cl->ai->moveTarget.path.trickPosition;
        } else if (G_Ai_PathLink(cl) && G_Ai_PathLink(cl)->travelType == NAV_TRAVEL_JUMP && ent->ground.ent) {
          dest = G_Ai_JumpDestination(cl);
        } else {
          dest = G_Ai_PushDestination(cl, cl->ai->moveTarget.path.pathPosition);
          toPush = !Vec3_Equal(dest, cl->ai->moveTarget.path.pathPosition);
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

  const float deltaYaw = G_Ai_CommandYaw(cl, cmd) - angles.y;
  Vec3_Vectors(MakeVec3(0.0, deltaYaw, 0.0), &dir, NULL, NULL);

  const bool swimming = ent->waterLevel >= WATER_WAIST;
  bool waitPolitely = false;

  if (cl->ai->moveTarget.type == AI_GOAL_PATH) {
    // the next step(s) will be onto a mover, so if we can't move yet, we wait.
    if (!(gi.PointContents(ent->s.origin) & CONTENTS_MASK_LIQUID) && !G_Ai_Path_CanPathTo(cl->ai->moveTarget.path.path, cl->ai->moveTarget.path.pathIndex)) {
      const GameEntity *plat = G_Ai_Node_RaisedPlatform(cl->ai->moveTarget.path.pathPosition);
      dir = plat ? G_Ai_LeavePlatform(cl, cmd, plat) : Vec3_Scale(dir, PM_SPEED_RUN);
      waitPolitely = true;
      cl->ai->moveTarget.distressExtension = true;
    // trick-jumping requires a bit of finesse
    } else if (cl->ai->moveTarget.path.trickJump) {
      if (cl->ai->moveTarget.path.trickJump == TRICK_JUMP_TURNING) {
        dir = Vec3_Zero();
      } else {
        dir = Vec3_Scale(dir, Clampf(len * 2.f, 10.f, PM_SPEED_RUN));
      }
    } else if (G_Ai_StrafeJump(cl, cmd, &dir)) {
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
               ((G_Ai_PathLink(cl) && G_Ai_PathLink(cl)->travelType == NAV_TRAVEL_WALK_OFF_LEDGE) ||
               G_Ai_ShouldSlowDrop(
                 VectorValue(cl->ai->moveTarget.path.path, GameAiNodeId, cl->ai->moveTarget.path.pathIndex - 1),
                 VectorValue(cl->ai->moveTarget.path.path, GameAiNodeId, cl->ai->moveTarget.path.pathIndex)))) {
      dir = Vec3_Scale(dir, PM_SPEED_RUN * AI_LEDGE_SPEED);
    // run full speed towards the target
    } else {
      dir = Vec3_Scale(dir, PM_SPEED_RUN);
    }

    // if we're swimming, swim up or down to the node
    if (swimming) {
      const float rise = cl->ai->moveTarget.path.pathPosition.z - ent->s.origin.z;

      if (rise > PM_STEP_HEIGHT || (rise > 0.f && ent->ground.ent)) {
        cmd->up = PM_SPEED_JUMP;
      } else if (rise < -PM_STEP_HEIGHT) {
        cmd->up = -PM_SPEED_RUN;
      }
    // if we're on a ladder and the node is a bbox below us, crouch to get down,
    // otherwise hold jump
    } else if (cl->ps.pmState.flags & PMF_ON_LADDER) {
      if ((cl->ai->moveTarget.path.pathPosition.z - ent->s.origin.z) < -Box3_Size(Pm_Bounds(&cl->ps.pmState.params, false)).z) {
        cmd->up = -PM_SPEED_DUCKED;
      } else {
        cmd->up = PM_SPEED_JUMP;
      }
    } else if (G_Ai_ShouldBarrierJump(cl)) {
      cmd->up = PM_SPEED_JUMP;
      G_Ai_Debug("Barrier jump to %s\n", vtos(cl->ai->moveTarget.path.pathPosition));
    } else if (G_Ai_PathLink(cl) && G_Ai_PathLink(cl)->travelType == NAV_TRAVEL_CROUCH) {
      cmd->up = -PM_SPEED_DUCKED;
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
    ahead.cmd.msec = cl->ai->moveTarget.type == AI_GOAL_PATH ? 100 : 250;
    Pm_Move(&ahead);
    cl->ai->lookaheadFrame = gameLevel.frameNum;
    cl->ai->lookaheadNoGround = !ahead.ground.ent;
  }

  const bool atEdge = ent->ground.ent && (cl->ai->lookaheadNoGround || !pm.ground.ent);

  // predicted ground is gone
  if (atEdge) {

    if (cl->ai->moveTarget.type == AI_GOAL_PATH) {

      // path goals: only apply edge logic when not intentionally jumping
      if (cmd->up <= 0) {
        const float xyDist = Vec2_Distance(Vec3_XY(cl->ai->moveTarget.path.pathPosition), Vec3_XY(ent->s.origin));
        const GameAiLink *link = G_Ai_PathLink(cl);
        const int32_t travelType = link ? link->travelType : 0;

        // we're most likely on or going to a mover; if we'll be falling in a few frames, stop us early
        if (!(gi.PointContents(ent->s.origin) & CONTENTS_MASK_LIQUID) && !G_Ai_Path_CanPathTo(cl->ai->moveTarget.path.path, cl->ai->moveTarget.path.pathIndex)) {
          cmd->forward = -cmd->forward;
          cmd->right = -cmd->right; // stop for now
          G_Ai_Debug("Stopping early to prevent mover issues\n");
        // a ledge link drops from one place, if the fall lands where the link does
        } else if (travelType == NAV_TRAVEL_WALK_OFF_LEDGE && G_Ai_PathLinkDistance(cl) <= AI_LEDGE_DISTANCE &&
                   G_Ai_SimulateDrop(cl, cl->ai->moveTarget.path.pathPosition)) {
          G_Ai_Debug("Walking off a ledge to %s\n", vtos(cl->ai->moveTarget.path.pathPosition));
        } else if (travelType == NAV_TRAVEL_NONE && !toPush &&
                   cl->ai->moveTarget.path.pathPosition.z < ent->s.origin.z - PM_STEP_HEIGHT &&
                   G_Ai_SimulateDrop(cl, cl->ai->moveTarget.path.pathPosition)) {
          G_Ai_Debug("Dropping to %s\n", vtos(cl->ai->moveTarget.path.pathPosition));
        } else if (travelType == NAV_TRAVEL_JUMP && !toPush && G_Ai_IsOnJumpLine(cl, NULL)) {
          if (!pm.ground.ent) {
            cmd->up = PM_SPEED_JUMP;
            G_Ai_Debug("Jumping across a gap to %s\n", vtos(cl->ai->moveTarget.path.pathPosition));
          }
        // walk links, and the links that the game adds to them, have floor all the way, and a jump
        // link leaves it only on its line, so an edge anywhere else means that we are off the path
        } else if (!toPush && (travelType <= NAV_TRAVEL_BARRIER_JUMP || travelType == NAV_TRAVEL_JUMP)) {
          cmd->forward = -cmd->forward;
          cmd->right = -cmd->right;
          G_Ai_ReturnToPath(cl);
        // if the node is above us step-wise OR it's not far below us & across a big distance, we gotta jump
        } else if (((cl->ai->moveTarget.path.pathPosition.z - ent->s.origin.z) > -PM_STEP_HEIGHT ||
          (xyDist > fabsf(cl->ai->moveTarget.path.pathPosition.z - ent->s.origin.z) && (xyDist >= PM_STEP_HEIGHT * 6.f))) && !pm.ground.ent && !toPush) {
          if (G_Ai_IsOverspeed(ent) && G_Ai_FallsToVoid(cl, ent->s.origin, G_Ai_JumpVelocity(cl))) {
            cmd->forward = -cmd->forward;
            cmd->right = -cmd->right;
          } else {
            cmd->up = PM_SPEED_JUMP;
          }
        } else if (G_Ai_IsOverspeed(ent) && G_Ai_FallsToVoid(cl, ent->s.origin, ent->velocity)) {
          cmd->forward = -cmd->forward;
          cmd->right = -cmd->right;
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
        G_Ai_EndPath(cl);
      }
    }
  // if we're not waiting to turn...
  // and not waiting politely...
  // and not riding a mover...
  } else if (((cl->ai->moveTarget.type == AI_GOAL_PATH
               && cl->ai->moveTarget.path.trickJump != TRICK_JUMP_TURNING)
               || cl->ai->moveTarget.type != AI_GOAL_PATH)
               && !waitPolitely
               && !atEdge
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
        } else if (cl->ai->moveTarget.distress > 12.f &&
                   !G_Ai_FallsToVoid(cl, ent->s.origin, G_Ai_JumpVelocity(cl))) {
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

  G_Ai_AvoidVoid(cl, cmd);
  return 0;
}

/**
 * @brief The horizontal distance within which a path node is too near to look at, because the
 * direction to it swings as the bot passes it.
 */
#define AI_LOOK_MIN_DISTANCE 96.f

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
      bool holdYaw = false;

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

        const GameAiGoal *path = &cl->ai->moveTarget;

        if (!path->path.trickJump && !(cl->ps.pmState.flags & PMF_ON_LADDER) && ent->waterLevel < WATER_WAIST &&
            path->path.pathPosition.z - ent->s.origin.z <= PM_STEP_HEIGHT) {

          if (Vec2_Distance(Vec3_XY(aimTarget), Vec3_XY(ent->s.origin)) < AI_LOOK_MIN_DISTANCE) {
            aimTarget = path->path.nextPathPosition;
          }

          holdYaw = Vec2_Distance(Vec3_XY(aimTarget), Vec3_XY(ent->s.origin)) < AI_LOOK_MIN_DISTANCE;
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

      if (holdYaw) {
        idealAngles.y = cl->angles.y;
      }

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

    if (G_Ai_IsAlerted(cl)) {
      const Vec3 eye = Vec3_Add(ent->s.origin, cl->ps.pmState.viewOffset);
      idealAngles = Vec3_Euler(Vec3_Normalize(Vec3_Subtract(cl->ai->alertPosition, eye)));
      idealAngles.z = 0.f;
    }

    G_Ai_SpamViewAngles(cl, &idealAngles);
  } else {
    G_Ai_CombatAngles(cl, seconds, &idealAngles, &idealRate);
  }

  idealAngles.x = G_Ai_AngleDelta(0.f, idealAngles.x);

  if (fabsf(idealAngles.x) > 90.f) {
    idealAngles.x = 90.f;
  }

  const float stiffness = combatTarget->type == AI_GOAL_ENTITY
    ? Mixf(10.f, 20.f, cl->ai->personality.skill)
    : Mixf(6.f, 10.f, cl->ai->personality.skill);

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

}

/**
 * @brief Called just before an AI leaves this mortal plane.
 */
void G_Ai_Disconnect(GameClient *cl) {

  // clear any dynamic memory
  G_Ai_ClearGoal(&cl->ai->combatTarget);
  G_Ai_ClearGoal(&cl->ai->moveTarget);

  gi.Free(cl->ai);
  cl->ai = NULL;
}

/**
 * @brief Static list of func goal functions
 */
static const GameAiGoalFunc aiGoalFuncs[AI_FUNC_GOAL_TOTAL] = {
  [AI_FUNC_GOAL_TARGET] = G_Ai_Target,
  [AI_FUNC_GOAL_STATE] = G_Ai_State,
  [AI_FUNC_GOAL_WEAPONRY] = G_Ai_Weaponry,
  [AI_FUNC_GOAL_ACROBATICS] = G_Ai_Acrobatics,
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

  if (G_IsMeat(cl->entity) && cl->entity->dead) {
    G_Ai_ClearGoal(&cl->ai->combatTarget);
    G_Ai_ClearGoal(&cl->ai->moveTarget);
  }

#if AI_GOAL_HARDENING
  // clear stale entity goals whose target has been freed, reused, or whose
  // cached `entity.ent` pointer can no longer be trusted. Re-resolve against
  // the canonical `ge.entities` table by slot number rather than dereferencing
  // the cached pointer directly, since it may have gone stale or been
  // corrupted (see #960).
  G_Ai_ValidateEntityGoal(cl, "combat_target", &cl->ai->combatTarget);
  G_Ai_ValidateEntityGoal(cl, "move_target", &cl->ai->moveTarget);
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

  memset(cl->ai->funcGoalNextThinks, 0, sizeof(cl->ai->funcGoalNextThinks));

  G_Ai_State_Reset(cl);

  cl->ai->weaponCheckTime = 0;
  cl->ai->reacquireTime = 0;
  cl->ai->lookaheadFrame = 0;
  cl->ai->lookaheadNoGround = false;
  cl->ai->replanTime = 0;
  cl->ai->pushTime = 0;
  cl->ai->groundPosition = cl->entity->s.origin;
  cl->ai->fallFrame = 0;
  cl->ai->wanderTurnTime = 0;
  cl->ai->strafeJumping = false;
  cl->ai->strafeLookTime = 0;

  cl->ai->viewVelocity = Vec3_Zero();
  cl->ai->perceivedVelocity = Vec3_Zero();
  cl->ai->aimError = Vec2_Zero();
  cl->ai->leadError = 0.f;
  cl->ai->spamAmmo = 0;
  cl->ai->spamTime = 0;
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
 * @param roster The roster entry to connect, or NULL for the next entry in the shuffled order.
 */
static void G_Ai_Connect(GameClient *cl, const GameAiRoster *roster) {

  char userInfo[MAX_INFO_STRING_STRING];

  if (roster) {
    G_Ai_GetRosterInfo(cl, roster, userInfo);
  } else {
    roster = G_Ai_GetRoster(cl, userInfo);
  }

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

  G_Ai_Node_UpdateMovers();

  if (gameLevel.time % 1000 == 0) {

    int32_t humanClients = 0;
    int32_t aiClients = 0;
    G_ForEachClient(cl, {
      if (cl->ai) {
        aiClients += cl->ai->added ? 0 : 1;
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
        G_Ai_Connect(cl, NULL);
        break;
      });
    } else if (activeClients > desiredClients) {
      G_ForEachClient(cl, {
        if (cl->ai && !cl->ai->added) {
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
 * @brief Console command handler to add a bot from the roster, with optional traits.
 */
static void G_Ai_Add_f(void) {

  if (gi.Argc() < 2 || gi.Argc() > 5) {
    gi.Print("Usage: %s <name> [skill] [aggression] [awareness]\n", gi.Argv(0));
    G_Ai_PrintRoster();
    return;
  }

  const GameAiRoster *roster = G_Ai_FindRoster(gi.Argv(1));
  if (!roster) {
    gi.Print("Unknown bot: %s\n", gi.Argv(1));
    G_Ai_PrintRoster();
    return;
  }

  float traits[] = { roster->skill, roster->aggression, roster->awareness };

  for (int32_t i = 2; i < gi.Argc(); i++) {
    char *end;
    const float value = strtof(gi.Argv(i), &end);

    if (end == gi.Argv(i) || *end || !(value >= 0.f && value <= 1.f)) {
      gi.Print("Traits must be numbers from 0 to 1: %s\n", gi.Argv(i));
      return;
    }

    traits[i - 2] = value;
  }

  G_ForEachFreeClient(cl, {
    G_Ai_Connect(cl, roster);

    cl->ai->added = true;
    cl->ai->personality.skill = traits[0];
    cl->ai->personality.aggression = traits[1];
    cl->ai->personality.awareness = traits[2];

    gi.Print("Added %s: skill=%.2f aggression=%.2f awareness=%.2f\n", cl->persistent.netName, traits[0], traits[1], traits[2]);
    return;
  });

  gi.Print("No free client slot for %s\n", roster->name);
}

/**
 * @brief Initializes the AI subsystem.
 */
void G_Ai_Init(void) {

  g_aiNoTarget = gi.AddCvar("g_aiNoTarget", "0", CVAR_DEVELOPER, "Disables bots targeting enemies");

  gi.AddCmd("addAi", G_Ai_Add_f, CMD_GAME, "Add a bot from the roster, with optional skill, aggression and awareness");

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
  G_Ai_State_Shutdown();
  G_Ai_ShutdownNodes();

  gi.FreeTag(MEM_TAG_AI);
}

/**
 * @brief The time in milliseconds for which a bot attends to an alert.
 */
#define AI_ALERT_MILLIS 1500

/**
 * @brief The time in milliseconds for which the last enemy to hurt a bot is a preferred target.
 */
#define AI_ATTACKER_MILLIS 2000

/**
 * @brief Alerts the bot to the given position, unless it is fighting an enemy that it can see.
 */
static void G_Ai_Alert(GameClient *cl, const Vec3 position) {

  if (cl->ai->combatTarget.type == AI_GOAL_ENTITY && cl->ai->combatTargetSeenTime == gameLevel.time) {
    return;
  }

  cl->ai->alertPosition = position;
  cl->ai->alertTime = gameLevel.time;
  cl->ai->alertInvestigated = false;
}

/**
 * @see g_ai_main.h
 */
void G_Ai_Hear(const GameEntity *source, const float distance) {

  if (!source->client) {
    return;
  }

  G_ForEachClient(cl, {
    if (!cl->ai || !cl->entity || cl->entity == source || cl->entity->dead) {
      continue;
    }

    if (G_OnSameTeam(cl, source->client)) {
      continue;
    }

    if (Vec3_Distance(cl->entity->s.origin, source->s.origin) > distance) {
      continue;
    }

    G_Ai_Alert(cl, source->s.origin);
  });
}

/**
 * @see g_ai_main.h
 */
void G_Ai_Damaged(const GameEntity *target, const GameEntity *attacker) {

  GameClient *cl = target->client;

  if (!cl || !cl->ai || !attacker->client || attacker == target || G_OnSameTeam(cl, attacker->client)) {
    return;
  }

  cl->ai->attacker = attacker->s.number;
  cl->ai->attackedTime = gameLevel.time;

  G_Ai_Alert(cl, attacker->s.origin);
}

/**
 * @see g_ai_main.h
 */
bool G_Ai_IsAlerted(const GameClient *cl) {
  return cl->ai->alertTime && gameLevel.time - cl->ai->alertTime < AI_ALERT_MILLIS;
}

/**
 * @return True if `ent` hurt the bot within `AI_ATTACKER_MILLIS`.
 */
bool G_Ai_IsAttacker(const GameClient *cl, const GameEntity *ent) {
  return cl->ai->attackedTime && cl->ai->attacker == ent->s.number &&
         gameLevel.time - cl->ai->attackedTime < AI_ATTACKER_MILLIS;
}
