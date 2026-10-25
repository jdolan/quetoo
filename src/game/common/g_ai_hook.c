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
 * @brief The distance along its path to its objective beyond which a bot considers the hook.
 */
#define AI_HOOK_MIN_PATH 384.f

/**
 * @brief The least straight distance to a node that a bot hooks to.
 */
#define AI_HOOK_MIN_DISTANCE 128.f

/**
 * @brief The distance beyond the node within which the hook MUST catch.
 */
#define AI_HOOK_BEYOND 384.f

/**
 * @brief The least distance beyond the node at which the hook catches. The pull stops
 * `PM_HOOK_MIN_DIST` short of the hook, so a hook nearer the node than this stops the bot short of
 * the node.
 */
#define AI_HOOK_MIN_BEYOND (PM_HOOK_MIN_DIST + 16.f)

/**
 * @brief The distance that a bot moves from where it last glanced at its objective before it
 * glances again.
 */
#define AI_HOOK_GLANCE_MOVE 768.f

/**
 * @brief The interval in milliseconds between glances.
 */
#define AI_HOOK_GLANCE_INTERVAL 3000

/**
 * @brief The longest glance in milliseconds.
 */
#define AI_HOOK_GLANCE_MILLIS 400

/**
 * @brief The view error in degrees within which a glance ends early.
 */
#define AI_HOOK_GLANCE_ERROR 10.f

/**
 * @brief The least time in milliseconds between two checks of the path for the hook without a
 * glance (`G_Ai_Hook_ShouldCheck`).
 */
#define AI_HOOK_CHECK_MILLIS 500

/**
 * @brief The longest aim in milliseconds, after which the bot gives up the hook.
 */
#define AI_HOOK_AIM_MILLIS 1000

/**
 * @brief The time in milliseconds, beyond the flight of the hook, after which a hook that did not
 * catch is let go.
 */
#define AI_HOOK_FLY_MILLIS 250

/**
 * @brief The time in milliseconds, beyond the pull to the node, after which the bot lets go.
 */
#define AI_HOOK_PULL_MILLIS 500

/**
 * @brief The distance from the point at which the hook MUST catch, else the bot lets go.
 */
#define AI_HOOK_MISS_DISTANCE 64.f

/**
 * @brief The distance from the hook within which the bot lets go: the pull stops at
 * `PM_HOOK_MIN_DIST`.
 */
#define AI_HOOK_SHORT_DISTANCE (PM_HOOK_MIN_DIST + 16.f)

/**
 * @brief The height in units above the position at which the bot lets go, from which
 * `G_Ai_Hook_IsSafe` follows its flight. A pull with any upward speed lifts the bot off the floor
 * (`Pm_CheckHookJump`), and a pull has no gravity, so the bot flies on at the pull speed after it
 * lets go, until it falls back to the floor.
 */
#define AI_HOOK_RELEASE_LIFT 8.f

/**
 * @brief The longest flight in seconds after the bot lets go, that `G_Ai_Hook_IsSafe` follows.
 */
#define AI_HOOK_FLIGHT_SECONDS 1.5f

/**
 * @brief The interval in seconds of the steps of that flight.
 */
#define AI_HOOK_FLIGHT_STEP_SECONDS .05f

/**
 * @brief The interval in milliseconds between a bot's looks for an item that no path reaches.
 */
#define AI_HOOK_ITEM_INTERVAL 1000

/**
 * @brief The most items of a level that no path reaches.
 */
#define AI_HOOK_ITEMS 32

/**
 * @brief The share of the graph from which a path MUST reach an item's node, else no path reaches
 * the item.
 */
#define AI_HOOK_REACH 0.25f

static struct {
  /**
   * @brief The entity numbers of the items that no path reaches.
   */
  int32_t items[AI_HOOK_ITEMS];

  /**
   * @brief The count of `items`.
   */
  int32_t numItems;
} module;

/**
 * @return True if the hook is available to the bot: it is enabled and allowed, the bot pulls with
 * it, and it holds no hook.
 */
static bool G_Ai_Hook_IsAvailable(const GameClient *cl) {

  if (!G_Hook_Enabled() || !G_AllowHook(cl)) {
    return false;
  }

  if (cl->persistent.hookStyle != HOOK_PULL) {
    return false;
  }

  return cl->hook.entity == NULL && cl->hook.fireTime <= gameLevel.time;
}

/**
 * @return True if the bot's state lets it consider the hook: not in a fight, a climb or lava. A bot
 * with an enemy considers it only to chase or retreat from it. A bot that let go of the hook stays
 * in `AI_STATE_HOOK` until its state machine runs on the ground, so the state that the hook
 * interrupted decides whether it hooks on in the air.
 */
static bool G_Ai_Hook_IsStateAllowed(const GameClient *cl) {

  const GameAiStateId state = cl->ai->state == AI_STATE_HOOK && !G_Ai_Hook_IsCommitted(cl)
    ? cl->ai->interruptedState
    : cl->ai->state;

  switch (state) {
    case AI_STATE_FIGHT:
    case AI_STATE_CLIMB:
    case AI_STATE_ESCAPE:
    case AI_STATE_HOOK:
      return false;
    case AI_STATE_CHASE:
    case AI_STATE_RETREAT:
      return true;
    default:
      return cl->ai->combatTarget.type != AI_GOAL_ENTITY;
  }
}

/**
 * @return The distance along the bot's path, from the bot to the node at `index`.
 */
static float G_Ai_Hook_PathDistance(const GameClient *cl, const uint32_t index) {

  const GameAiGoal *goal = &cl->ai->moveTarget;
  const Vector *path = goal->path.path;

  Vec3 from = cl->entity->s.origin;
  float distance = 0.f;

  for (uint32_t i = goal->path.pathIndex; i <= index; i++) {
    const Vec3 to = G_Ai_Node_GetPosition(VectorValue(path, GameAiNodeId, i));
    distance += Vec3_Distance(from, to);
    from = to;
  }

  return distance;
}

/**
 * @return The bot's eye.
 */
static Vec3 G_Ai_Hook_Eye(const GameClient *cl) {
  return Vec3_Add(cl->entity->s.origin, cl->ps.pmState.viewOffset);
}

/**
 * @brief Finds where the hook catches, to pull the bot to the position: the bot's box MUST pass
 * from the bot to the position, and a structural surface MUST lie beyond it along the same line,
 * within `AI_HOOK_BEYOND`, but not nearer than `AI_HOOK_MIN_BEYOND`. The surface is the world or
 * a model that does not move, not a player or a mover, which can move before the hook gets there.
 * Then the pull, which runs from the bot to the hook, takes the bot through the position. The hook MUST reach the point from the
 * bot's eye, within `g_hookDistance`.
 * @return NULL if the hook can pull the bot to the position, else why not, for the profiler:
 * `range`, `body` (the box does not pass), `beyond` (nothing to catch), `surface` (sky, a player,
 * a mover, or not solid) or `sight` (the hook does not reach the point).
 */
static const char *G_Ai_Hook_Point(const GameClient *cl, const Vec3 dest, Vec3 *point) {

  const GameEntity *ent = cl->entity;

  Vec3 dir;
  const float distance = Vec3_DistanceDir(dest, ent->s.origin, &dir);

  if (distance < AI_HOOK_MIN_DISTANCE || distance + AI_HOOK_MIN_BEYOND > g_hookDistance->value) {
    return "range";
  }

  const CollisionTrace path = gi.Trace(ent->s.origin, dest, ent->bounds, ent, ent->clipMask);

  if (path.startSolid || path.fraction < 1.f) {
    return "body";
  }

  const Vec3 end = Vec3_Fmaf(dest, AI_HOOK_BEYOND, dir);
  const CollisionTrace beyond = gi.Trace(dest, end, Box3_Zero(), ent, CONTENTS_MASK_CLIP_PROJECTILE);

  if (beyond.fraction == 1.f || beyond.fraction * AI_HOOK_BEYOND < AI_HOOK_MIN_BEYOND) {
    return "beyond";
  }

  const GameEntity *surface = beyond.ent;

  if (!(beyond.contents & (CONTENTS_SOLID | CONTENTS_WINDOW)) || !surface ||
      surface->solid != SOLID_BSP || surface->moveType == MOVE_TYPE_PUSH) {
    return "surface";
  }

  if (!G_IsStructural(&beyond) && !(G_IsSky(&beyond) && g_hookSky->integer)) {
    return "surface";
  }

  const Vec3 eye = G_Ai_Hook_Eye(cl);

  if (Vec3_Distance(eye, beyond.end) > g_hookDistance->value) {
    return "range";
  }

  const CollisionTrace sight = gi.Trace(eye, beyond.end, Box3_Zero(), ent,
                                        CONTENTS_MASK_CLIP_PROJECTILE);

  if (Vec3_Distance(sight.end, beyond.end) > 8.f) {
    return "sight";
  }

  *point = beyond.end;
  return NULL;
}

/**
 * @return True if the bot's box, moved from `start` to `end`, touches lava or slime, a
 * `trigger_hurt`, or a `trigger_push`, which would throw the bot off its pull.
 */
static bool G_Ai_Hook_IsHazard(const GameEntity *ent, const Vec3 start, const Vec3 end) {

  const CollisionTrace liquid = gi.Trace(start, end, ent->bounds, ent, CONTENTS_LAVA | CONTENTS_SLIME);

  if (liquid.startSolid || liquid.fraction < 1.f) {
    return true;
  }

  return G_Ai_CrossesTrigger(start, end, true);
}

/**
 * @return True if the pull from the bot to the position, and the bot's flight after it lets go
 * there, keep out of lava, slime, `trigger_hurt` and `trigger_push`. The bot lets go at the pull
 * speed, toward the hook, `AI_HOOK_RELEASE_LIFT` above the position, and slides on a floor until
 * friction, about as `Pm_Friction` applies it, slows it to its ground speed, at which it runs
 * under its own control. The flight is also safe after `AI_HOOK_FLIGHT_SECONDS`.
 */
static bool G_Ai_Hook_IsSafe(const GameClient *cl, const Vec3 dest, const Vec3 point) {

  const GameEntity *ent = cl->entity;
  const PMoveParams *params = &cl->ps.pmState.params;

  if (G_Ai_Hook_IsHazard(ent, ent->s.origin, dest)) {
    return false;
  }

  const Vec3 lift = Vec3_Add(dest, MakeVec3(0.f, 0.f, AI_HOOK_RELEASE_LIFT));
  Vec3 origin = gi.Trace(dest, lift, ent->bounds, ent, ent->clipMask).end;
  Vec3 velocity = Vec3_Scale(Vec3_Normalize(Vec3_Subtract(point, ent->s.origin)),
                             g_hookPullSpeed->value);

  const float gravity = G_LevelGravity();
  const float dt = AI_HOOK_FLIGHT_STEP_SECONDS;

  bool ground = false, slick = false;

  for (float time = 0.f; time < AI_HOOK_FLIGHT_SECONDS; time += dt) {

    if (ground) {
      const float speed = Vec3_Length(MakeVec3(velocity.x, velocity.y, 0.f));

      if (speed <= Maxf(1.f, params->speedGround)) {
        return true;
      }

      const float friction = Maxf(0.f, slick ? params->frictionGroundSlick
                                             : params->frictionGround);
      const float control = Maxf(params->speedStop, speed);
      const float scale = Maxf(0.f, speed - friction * control * dt) / speed;

      velocity = MakeVec3(velocity.x * scale, velocity.y * scale, 0.f);
    } else {
      velocity.z -= gravity * dt;
    }

    const Vec3 end = Vec3_Fmaf(origin, dt, velocity);
    const CollisionTrace tr = gi.Trace(origin, end, ent->bounds, ent, ent->clipMask);

    if (tr.startSolid) {
      return true;
    }

    if (G_Ai_Hook_IsHazard(ent, origin, tr.end)) {
      return false;
    }

    if (tr.fraction < 1.f) {

      if (tr.surface & SURF_SKY) {
        return false;
      }

      if (tr.plane.normal.z >= PM_STEP_NORMAL) {
        ground = true;
        slick = tr.surface & SURF_SLICK;
      }

      velocity = Vec3_Subtract(velocity,
                               Vec3_Scale(tr.plane.normal, Vec3_Dot(velocity, tr.plane.normal)));
    }

    origin = tr.end;

    if (ground) {
      const Vec3 down = Vec3_Add(origin, MakeVec3(0.f, 0.f, -PM_STEP_HEIGHT));
      const CollisionTrace floor = gi.Trace(origin, down, ent->bounds, ent, ent->clipMask);

      ground = floor.fraction < 1.f && floor.plane.normal.z >= PM_STEP_NORMAL;

      if (ground) {
        if (G_Ai_Hook_IsHazard(ent, origin, floor.end)) {
          return false;
        }
        origin = floor.end;
        slick = floor.surface & SURF_SLICK;
      }
    }
  }

  return true;
}

/**
 * @brief The heights above a node at which the bot MAY let go, lowest first. A pull up to a ledge
 * clips the lip of the ledge on the straight line to the node, but passes over it to a point above
 * the node, from which the bot drops onto the node.
 */
static const float raises[] = { 0.f, 32.f, 64.f };

/**
 * @brief Finds the lowest point above the node at which the hook can pull the bot to let go
 * (`G_Ai_Hook_Point`), safely.
 * @return NULL if the bot can hook to the node, and the pull and the flight after it are safe
 * (`G_Ai_Hook_IsSafe`), else why not at the node itself, or `hazard`.
 */
static const char *G_Ai_Hook_Reach(const GameClient *cl, const Vec3 node, Vec3 *dest, Vec3 *point) {

  const char *reason = NULL;

  for (size_t i = 0; i < lengthof(raises); i++) {
    const Vec3 raised = Vec3_Add(node, MakeVec3(0.f, 0.f, raises[i]));
    const char *r = G_Ai_Hook_Point(cl, raised, point);

    if (r == NULL) {
      if (G_Ai_Hook_IsSafe(cl, raised, *point)) {
        *dest = raised;
        return NULL;
      }
      r = "hazard";
    }

    reason = reason ?: r;
  }

  return reason;
}

/**
 * @brief Looks along the bot's path, from its objective back toward the bot, for the farthest node
 * to which the hook can pull it (`G_Ai_Hook_Point`), and which is at least `AI_HOOK_MIN_PATH` along
 * the path.
 * @param reason If not NULL, why most of the nodes failed (`G_Ai_Hook_Reach`), or `near` if no
 * node is far enough along the path, for the profiler.
 * @return True if the bot has a node to hook to.
 */
static bool G_Ai_Hook_FindNode(GameClient *cl, const char **reason) {

  const GameAiGoal *goal = &cl->ai->moveTarget;
  const Vector *path = goal->path.path;
  GameAiHook *hook = &cl->ai->hook;

  static const char *reasons[] = { "range", "body", "beyond", "surface", "sight", "hazard" };
  uint32_t counts[lengthof(reasons)] = { 0 };

  float pathDistance = G_Ai_Hook_PathDistance(cl, (uint32_t) path->count - 1);

  for (uint32_t i = (uint32_t) path->count - 1; i > goal->path.pathIndex; i--) {

    if (pathDistance < AI_HOOK_MIN_PATH) {
      break;
    }

    const Vec3 node = G_Ai_Node_GetPosition(VectorValue(path, GameAiNodeId, i));
    const Vec3 prev = G_Ai_Node_GetPosition(VectorValue(path, GameAiNodeId, i - 1));

    Vec3 dest;
    const char *r = G_Ai_Hook_Reach(cl, node, &dest, &hook->point);
    if (!r) {
      hook->dest = dest;
      hook->node = VectorValue(path, GameAiNodeId, i);
      hook->index = i;
      hook->pathDistance = pathDistance;
      hook->distance = Vec3_Distance(cl->entity->s.origin, dest);
      return true;
    }

    for (size_t j = 0; j < lengthof(reasons); j++) {
      if (!strcmp(r, reasons[j])) {
        counts[j]++;
      }
    }

    pathDistance -= Vec3_Distance(prev, node);
  }

  if (reason) {
    *reason = "near";
    uint32_t most = 0;
    for (size_t j = 0; j < lengthof(reasons); j++) {
      if (counts[j] > most) {
        most = counts[j];
        *reason = reasons[j];
      }
    }
  }

  return false;
}

/**
 * @brief Finds the hook's node in the bot's path, at or after the node that the bot moves to, as
 * the state machine can plan the path again while the bot aims or the hook flies. For an item, it
 * checks that the item is still there.
 * @return True if the node is in the path, or the item is there.
 */
static bool G_Ai_Hook_Track(GameClient *cl) {

  const GameAiGoal *goal = &cl->ai->moveTarget;
  GameAiHook *hook = &cl->ai->hook;

  if (hook->item) {
    return hook->item->inUse && hook->item->s.spawnId == hook->itemSpawnId &&
           hook->item->solid == SOLID_TRIGGER;
  }

  if (goal->type != AI_GOAL_PATH || !goal->path.path) {
    return false;
  }

  const Vector *path = goal->path.path;

  for (uint32_t i = goal->path.pathIndex; i < path->count; i++) {
    if (VectorValue(path, GameAiNodeId, i) == hook->node) {
      hook->index = i;
      return true;
    }
  }

  return false;
}

/**
 * @brief Enters the phase.
 */
static void G_Ai_Hook_Enter(GameClient *cl, const GameAiHookPhase phase) {

  cl->ai->hook.phase = phase;
  cl->ai->hook.phaseTime = gameLevel.time;
}

/**
 * @brief Lets go of the hook, and records why. The bot MAY glance again at once, to hook on toward
 * its objective.
 */
static void G_Ai_Hook_Release(GameClient *cl, const char *reason) {

  G_Ai_Profile_Hook(cl, "release", reason);

  G_Ai_Hook_Enter(cl, AI_HOOK_NONE);

  cl->ai->hook.item = NULL;
  cl->ai->hook.glanceNode = AI_NODE_INVALID;
  cl->ai->hook.glanceTime = 0;
}

/**
 * @brief Gives up the hook before the bot fires it.
 */
static void G_Ai_Hook_Abandon(GameClient *cl) {

  G_Ai_Hook_Enter(cl, AI_HOOK_NONE);

  cl->ai->hook.item = NULL;
}

/**
 * @brief Looks for an item that no path reaches, that the bot sees within reach of the hook and
 * would value, and starts a glance at it.
 * @return True if the bot glances at such an item.
 */
static bool G_Ai_Hook_FindItem(GameClient *cl) {

  GameAiHook *hook = &cl->ai->hook;

  if (hook->itemTime > gameLevel.time || !module.numItems) {
    return false;
  }

  hook->itemTime = gameLevel.time + AI_HOOK_ITEM_INTERVAL;

  const Vec3 eye = G_Ai_Hook_Eye(cl);

  for (int32_t i = 0; i < module.numItems; i++) {
    const GameEntity *ent = ge.entities[module.items[i]];

    if (!ent->inUse || !ent->item || ent->solid != SOLID_TRIGGER || ent->node == AI_NODE_INVALID) {
      continue;
    }

    const float distance = Vec3_Distance(eye, ent->s.origin);

    if (distance + AI_HOOK_MIN_BEYOND > g_hookDistance->value) {
      continue;
    }

    const uint32_t eta = SECONDS_TO_MILLIS(distance / g_hookPullSpeed->value);

    if (G_Ai_State_ItemValue(cl, ent, eta) <= 0.f || !G_Ai_HasLineOfSight(cl, ent)) {
      continue;
    }

    hook->item = ent;
    hook->itemSpawnId = ent->s.spawnId;
    hook->node = AI_NODE_INVALID;
    hook->index = 0;
    hook->glance = ent->s.origin;
    hook->dest = G_Ai_Node_GetPosition(ent->node);
    hook->glanceTime = gameLevel.time + AI_HOOK_GLANCE_INTERVAL;

    G_Ai_Hook_Enter(cl, AI_HOOK_GLANCE);
    return true;
  }

  return false;
}

/**
 * @return The last node of the bot's path, its objective. The bot MUST follow a path.
 */
static GameAiNodeId G_Ai_Hook_Objective(const GameClient *cl) {

  const Vector *path = cl->ai->moveTarget.path.path;

  return VectorValue(path, GameAiNodeId, path->count - 1);
}

/**
 * @return True if the bot follows a path long enough to consider the hook, on foot.
 */
static bool G_Ai_Hook_IsPathReady(const GameClient *cl) {

  const GameAiGoal *goal = &cl->ai->moveTarget;

  if (goal->type != AI_GOAL_PATH || !goal->path.path ||
      goal->path.pathIndex + 1 >= goal->path.path->count) {
    return false;
  }

  if (goal->path.trickJump || (cl->ps.pmState.flags & PMF_ON_LADDER) || G_Ai_IsClimbing(cl)) {
    return false;
  }

  if (cl->ps.pmState.flags & PMF_TIME_PUSHED) {
    return false;
  }

  return G_Ai_Hook_PathDistance(cl, (uint32_t) goal->path.path->count - 1) >= AI_HOOK_MIN_PATH;
}

/**
 * @return True if the bot did not glance at its objective lately from about here, and its path is
 * ready for the hook.
 */
static bool G_Ai_Hook_ShouldGlance(const GameClient *cl) {

  const GameAiHook *hook = &cl->ai->hook;

  if (hook->glanceTime > gameLevel.time || !G_Ai_Hook_IsPathReady(cl)) {
    return false;
  }

  if (G_Ai_Hook_Objective(cl) == hook->glanceNode &&
      Vec3_Distance(cl->entity->s.origin, hook->glanceOrigin) < AI_HOOK_GLANCE_MOVE) {
    return false;
  }

  return true;
}

/**
 * @return True if the bot moved on to another node of its path, or has a new path, since it last
 * checked its path for the hook, at most each `AI_HOOK_CHECK_MILLIS`, and its path is ready for
 * the hook. The view along the path opens as the bot turns a corner.
 */
static bool G_Ai_Hook_ShouldCheck(const GameClient *cl) {

  const GameAiHook *hook = &cl->ai->hook;

  if (hook->checkTime > gameLevel.time || !G_Ai_Hook_IsPathReady(cl)) {
    return false;
  }

  return cl->ai->moveTarget.path.pathIndex != hook->checkIndex ||
         G_Ai_Hook_Objective(cl) != hook->checkNode;
}

/**
 * @brief Records the node of the path to which the bot moves, and its objective, so that the bot
 * checks its path again only after it moves on, or for a new path.
 */
static void G_Ai_Hook_Checked(GameClient *cl) {

  GameAiHook *hook = &cl->ai->hook;

  hook->checkIndex = cl->ai->moveTarget.path.pathIndex;
  hook->checkNode = G_Ai_Hook_Objective(cl);
  hook->checkTime = gameLevel.time + AI_HOOK_CHECK_MILLIS;
}

/**
 * @brief Starts a glance at the objective of the bot's path.
 */
static void G_Ai_Hook_Glance(GameClient *cl) {

  const GameAiGoal *goal = &cl->ai->moveTarget;
  GameAiHook *hook = &cl->ai->hook;

  const uint32_t last = (uint32_t) goal->path.path->count - 1;

  hook->glanceNode = VectorValue(goal->path.path, GameAiNodeId, last);
  hook->glanceOrigin = cl->entity->s.origin;
  hook->glanceTime = gameLevel.time + AI_HOOK_GLANCE_INTERVAL;

  hook->glance = goal->path.pathTarget ? goal->path.pathTarget->s.origin
                                       : G_Ai_Node_GetPosition(hook->glanceNode);

  G_Ai_Hook_Checked(cl);
  G_Ai_Hook_Enter(cl, AI_HOOK_GLANCE);
}

/**
 * @return The angle in degrees between the bot's view and the direction from its eye to the point.
 */
static float G_Ai_Hook_ViewError(const GameClient *cl, const Vec3 point) {

  const Vec3 dir = Vec3_Normalize(Vec3_Subtract(point, G_Ai_Hook_Eye(cl)));

  return Degrees(acosf(Clampf(Vec3_Dot(dir, cl->forward), -1.f, 1.f)));
}

/**
 * @return True if the hook, fired now along the bot's view from where the game launches it
 * (`G_ClientProjectile`), would catch the world or a model near the hook point, within a miss that
 * a skilled bot keeps smaller. The hook leaves below and ahead of the eye, so at a lip or an edge
 * its line can miss the surface that the eye sees.
 */
static bool G_Ai_Hook_IsAimed(const GameClient *cl) {

  const GameAiHook *hook = &cl->ai->hook;

  Vec3 forward, right, up, org;
  G_ClientProjectile(cl, &forward, &right, &up, &org, -1.f);

  const float distance = Vec3_Distance(org, hook->point) + AI_HOOK_BEYOND;
  const Vec3 end = Vec3_Fmaf(org, distance, forward);
  const CollisionTrace tr = gi.Trace(org, end, Box3_Zero(), cl->entity,
                                     CONTENTS_MASK_CLIP_PROJECTILE);

  if (tr.fraction == 1.f || !tr.ent || ((const GameEntity *) tr.ent)->solid != SOLID_BSP) {
    return false;
  }

  return Vec3_Distance(tr.end, hook->point) < Mixf(48.f, 16.f, cl->ai->personality.skill);
}

/**
 * @return True if the bot passed the position at which it lets go, along the pull, or holds it.
 */
static bool G_Ai_Hook_IsAtDest(const GameClient *cl) {

  const GameEntity *ent = cl->entity;
  const GameAiHook *hook = &cl->ai->hook;

  const Box3 bounds = Box3_Expand3(ent->absBounds, MakeVec3(8.f, 8.f, 0.f));

  if (Box3_ContainsPoint(bounds, hook->dest)) {
    return true;
  }

  const Vec3 pull = Vec3_Subtract(cl->ps.pmState.hookPosition, ent->s.origin);

  return Vec3_Dot(Vec3_Subtract(hook->dest, ent->s.origin), pull) <= 0.f;
}

/**
 * @brief Runs the pull: lets go at the node, or for any reason that the pull cannot get it there.
 * @return True if the hook still pulls the bot.
 */
static bool G_Ai_Hook_Pull(GameClient *cl, PMoveCmd *cmd) {

  const GameEntity *ent = cl->entity;
  GameAiHook *hook = &cl->ai->hook;

  if (!cl->hook.entity) {
    G_Ai_Hook_Release(cl, "detach");
    return false;
  }

  if (!G_Ai_Hook_Track(cl)) {
    G_Ai_Hook_Release(cl, hook->item ? "taken" : "path");
    return false;
  }

  if (G_Ai_Hook_IsAtDest(cl)) {
    G_Ai_Hook_Release(cl, "reached");
    return false;
  }

  if (Vec3_Distance(ent->s.origin, cl->ps.pmState.hookPosition) < AI_HOOK_SHORT_DISTANCE) {
    G_Ai_Hook_Release(cl, "short");
    return false;
  }

  const float distance = Vec3_Distance(cl->ps.pmState.hookPosition, hook->dest) + hook->distance;
  const uint32_t millis = SECONDS_TO_MILLIS(distance / g_hookPullSpeed->value) + AI_HOOK_PULL_MILLIS;

  if (gameLevel.time > hook->phaseTime + millis) {
    G_Ai_Hook_Release(cl, "timeout");
    return false;
  }

  cmd->buttons |= BUTTON_HOOK;
  return true;
}

/**
 * @brief Holds the hook while it flies, until it catches near its point, and the pull begins. The
 * bot then moves along its path to the node at which it lets go.
 * @return True if the hook pulls the bot.
 */
static bool G_Ai_Hook_Fly(GameClient *cl, PMoveCmd *cmd) {

  GameAiHook *hook = &cl->ai->hook;

  if (!cl->hook.entity) {
    if (hook->launched) {
      G_Ai_Hook_Release(cl, "detach");
      return false;
    }

    if (gameLevel.time > hook->phaseTime + AI_HOOK_FLY_MILLIS) {
      cl->latchedButtons &= ~BUTTON_HOOK;
      G_Ai_Hook_Release(cl, "unfired");
      return false;
    }

    cmd->buttons |= BUTTON_HOOK;
    return false;
  }

  if (!hook->launched) {
    hook->launched = true;
    hook->phaseTime = gameLevel.time;
  }

  if (cl->hook.pull) {

    if (Vec3_Distance(cl->ps.pmState.hookPosition, hook->point) > AI_HOOK_MISS_DISTANCE) {
      G_Ai_Hook_Release(cl, "miss");
      return false;
    }

    if (!G_Ai_Hook_Track(cl)) {
      G_Ai_Hook_Release(cl, hook->item ? "item" : "path");
      return false;
    }

    if (!hook->item) {
      G_Ai_SkipPath(cl, hook->index);
    }

    G_Ai_Profile_Hook(cl, "attach", NULL);
    G_Ai_Hook_Enter(cl, AI_HOOK_PULL);

    return G_Ai_Hook_Pull(cl, cmd);
  }

  const float distance = Vec3_Distance(G_Ai_Hook_Eye(cl), hook->point);
  const uint32_t millis = SECONDS_TO_MILLIS(distance / Maxf(g_hookSpeed->value, 1.f)) + AI_HOOK_FLY_MILLIS;

  if (gameLevel.time > hook->phaseTime + millis) {
    G_Ai_Hook_Release(cl, "late");
    return false;
  }

  cmd->buttons |= BUTTON_HOOK;
  return false;
}

/**
 * @see g_ai_hook.h
 */
bool G_Ai_Hook_Move(GameClient *cl, PMoveCmd *cmd) {

  GameAiHook *hook = &cl->ai->hook;

  switch (hook->phase) {
    case AI_HOOK_NONE:
      if (G_Ai_Hook_IsAvailable(cl) && G_Ai_Hook_IsStateAllowed(cl)) {
        if (G_Ai_Hook_FindItem(cl)) {
          return false;
        }

        if (G_Ai_Hook_ShouldGlance(cl)) {
          G_Ai_Hook_Glance(cl);
        } else if (G_Ai_Hook_ShouldCheck(cl)) {
          G_Ai_Hook_Checked(cl);

          const char *reason;
          if (G_Ai_Hook_FindNode(cl, &reason)) {
            G_Ai_Profile_Hook(cl, "check", "hook");
            G_Ai_Hook_Enter(cl, AI_HOOK_AIM);
          } else {
            G_Ai_Profile_Hook(cl, "check", va("none-%s", reason));
          }
        }
      }
      return false;

    case AI_HOOK_GLANCE: {
      if (!G_Ai_Hook_IsAvailable(cl) || !G_Ai_Hook_IsStateAllowed(cl) ||
          (hook->item ? !G_Ai_Hook_Track(cl) : cl->ai->moveTarget.type != AI_GOAL_PATH)) {
        G_Ai_Hook_Abandon(cl);
        return false;
      }

      if (G_Ai_Hook_ViewError(cl, hook->glance) > AI_HOOK_GLANCE_ERROR &&
          gameLevel.time < hook->phaseTime + AI_HOOK_GLANCE_MILLIS) {
        return false;
      }

      bool found;
      if (hook->item) {
        const Vec3 node = G_Ai_Node_GetPosition(hook->item->node);
        const char *reason = G_Ai_Hook_Reach(cl, node, &hook->dest, &hook->point);
        hook->pathDistance = hook->distance = Vec3_Distance(cl->entity->s.origin, hook->dest);
        found = reason == NULL;
        G_Ai_Profile_Hook(cl, "glance", found ? "item" : va("noitem-%s", reason));
      } else {
        const char *reason;
        found = G_Ai_Hook_FindNode(cl, &reason);
        G_Ai_Profile_Hook(cl, "glance", found ? "hook" : va("none-%s", reason));
      }

      if (found) {
        G_Ai_Hook_Enter(cl, AI_HOOK_AIM);
      } else {
        G_Ai_Hook_Abandon(cl);
      }
      return false;
    }

    case AI_HOOK_AIM: {
      const char *lost = NULL;

      if (!G_Ai_Hook_IsAvailable(cl)) {
        lost = "unavailable";
      } else if (!G_Ai_Hook_IsStateAllowed(cl)) {
        lost = "state";
      } else if (!G_Ai_Hook_Track(cl)) {
        lost = hook->item ? "item" : "path";
      } else if (!hook->item && cl->ai->moveTarget.path.pathIndex >= hook->index) {
        lost = "passed";
      }

      if (lost) {
        G_Ai_Profile_Hook(cl, "aim", lost);
        G_Ai_Hook_Abandon(cl);
        return false;
      }

      if (gameLevel.time > hook->phaseTime + AI_HOOK_AIM_MILLIS) {
        G_Ai_Profile_Hook(cl, "aim", "timeout");
        G_Ai_Hook_Abandon(cl);
        return false;
      }

      if (G_Ai_Hook_IsAimed(cl)) {
        hook->pathDistance = hook->item ? Vec3_Distance(cl->entity->s.origin, hook->dest)
                                        : G_Ai_Hook_PathDistance(cl, hook->index);
        hook->distance = Vec3_Distance(cl->entity->s.origin, hook->dest);
        hook->fireTime = gameLevel.time;

        hook->launched = false;

        G_Ai_Profile_Hook(cl, "fire", NULL);
        G_Ai_Hook_Enter(cl, AI_HOOK_FIRED);

        cmd->buttons |= BUTTON_HOOK;
      }
      return false;
    }

    case AI_HOOK_FIRED:
      return G_Ai_Hook_Fly(cl, cmd);

    case AI_HOOK_PULL:
      return G_Ai_Hook_Pull(cl, cmd);
  }

  return false;
}

/**
 * @see g_ai_hook.h
 */
bool G_Ai_Hook_Turn(const GameClient *cl, Vec3 *angles) {

  const GameAiHook *hook = &cl->ai->hook;

  Vec3 point;

  switch (hook->phase) {
    case AI_HOOK_GLANCE:
      point = hook->glance;
      break;
    case AI_HOOK_AIM:
      point = hook->point;
      break;
    case AI_HOOK_FIRED:
      if (hook->launched) {
        return false;
      }
      point = hook->point;
      break;
    default:
      return false;
  }

  *angles = Vec3_Euler(Vec3_Normalize(Vec3_Subtract(point, G_Ai_Hook_Eye(cl))));
  angles->z = 0.f;

  return true;
}

/**
 * @see g_ai_hook.h
 */
bool G_Ai_Hook_IsCommitted(const GameClient *cl) {
  return cl->ai->hook.phase == AI_HOOK_FIRED || cl->ai->hook.phase == AI_HOOK_PULL;
}

/**
 * @see g_ai_hook.h
 */
void G_Ai_Hook_Reset(GameClient *cl) {

  cl->ai->hook = (GameAiHook) {
    .phase = AI_HOOK_NONE,
    .node = AI_NODE_INVALID,
    .glanceNode = AI_NODE_INVALID,
    .checkNode = AI_NODE_INVALID,
  };
}

/**
 * @brief Counts the nodes that a path reaches from the node, or with `offsets` and `sources`, the
 * reverse graph, the nodes from which a path reaches the node, up to `limit`.
 * @return The count, at most a little over `limit`.
 */
static uint32_t G_Ai_Hook_Load_reach(const GameAiNodeId node, const uint32_t *offsets,
                                     const GameAiNodeId *sources, GameAiNodeId *queue,
                                     uint32_t *seen, const uint32_t mark, const uint32_t limit) {

  uint32_t head = 0;
  uint32_t tail = 0;

  queue[tail++] = node;
  seen[node] = mark;

  while (head < tail && tail < limit) {
    const GameAiNodeId a = queue[head++];

    if (offsets) {
      for (uint32_t i = offsets[a]; i < offsets[a + 1]; i++) {
        if (seen[sources[i]] != mark) {
          seen[sources[i]] = mark;
          queue[tail++] = sources[i];
        }
      }
    } else {
      const Vector *links = G_Ai_Node_GetLinks(a);
      for (size_t i = 0; links && i < links->count; i++) {
        const GameAiNodeId b = VectorElement(links, GameAiLink, i)->id;
        if (seen[b] != mark) {
          seen[b] = mark;
          queue[tail++] = b;
        }
      }
    }
  }

  return tail;
}

/**
 * @see g_ai_hook.h
 */
void G_Ai_Hook_Load(void) {

  module.numItems = 0;

  const uint32_t count = G_Ai_Node_Count();

  if (!count || !G_Hook_Enabled()) {
    return;
  }

  uint32_t *offsets = calloc(count + 1, sizeof(uint32_t));
  if (!offsets) {
    G_Error("Failed to allocate %u offsets\n", count + 1);
  }

  for (GameAiNodeId a = 0; a < count; a++) {
    const Vector *links = G_Ai_Node_GetLinks(a);
    for (size_t i = 0; links && i < links->count; i++) {
      offsets[VectorElement(links, GameAiLink, i)->id + 1]++;
    }
  }

  for (uint32_t i = 0; i < count; i++) {
    offsets[i + 1] += offsets[i];
  }

  GameAiNodeId *sources = malloc(Maxz(offsets[count], 1) * sizeof(GameAiNodeId));
  uint32_t *fill = malloc(count * sizeof(uint32_t));
  GameAiNodeId *queue = malloc(count * sizeof(GameAiNodeId));
  uint32_t *seen = calloc(count, sizeof(uint32_t));

  if (!sources || !fill || !queue || !seen) {
    G_Error("Failed to allocate the reverse graph of %u nodes\n", count);
  }

  memcpy(fill, offsets, count * sizeof(uint32_t));

  for (GameAiNodeId a = 0; a < count; a++) {
    const Vector *links = G_Ai_Node_GetLinks(a);
    for (size_t i = 0; links && i < links->count; i++) {
      sources[fill[VectorElement(links, GameAiLink, i)->id]++] = a;
    }
  }

  uint32_t mark = 0;

  G_ForEachEntity(ent, {
    if (!ent->item || ent->node == AI_NODE_INVALID || ent->node >= count) {
      continue;
    }

    if (module.numItems == AI_HOOK_ITEMS) {
      break;
    }

    const uint32_t reach = count * AI_HOOK_REACH;

    if (G_Ai_Hook_Load_reach(ent->node, offsets, sources, queue, seen, ++mark, reach) >= reach) {
      continue;
    }

    if (G_Ai_Hook_Load_reach(ent->node, NULL, NULL, queue, seen, ++mark, reach) < reach) {
      G_Ai_Debug("%s at %s: no path leaves it\n", ent->classname, vtos(ent->s.origin));
      continue;
    }

    G_Ai_Debug("%s at %s: no path reaches it\n", ent->classname, vtos(ent->s.origin));
    module.items[module.numItems++] = ent->s.number;
  });

  free(offsets);
  free(sources);
  free(fill);
  free(queue);
  free(seen);
}
