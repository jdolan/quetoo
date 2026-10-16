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
 * @brief Setup base entity goal for the specified target.
 */
static inline void G_Ai_InitGoal(const GameClient *cl, GameAiGoal *goal, GameAiGoalType type, float priority) {

  G_Ai_ClearGoal(goal);

  goal->type = type;
  goal->priority = priority;
}

/**
 * @brief Setup entity goal for the specified target.
 */
void G_Ai_SetPositionGoal(const GameClient *cl, GameAiGoal *goal, float priority, const Vec3 pos) {

  G_Ai_InitGoal(cl, goal, AI_GOAL_POSITION, priority);

  goal->position.pos = pos;

  G_Ai_Debug("New goal: %s (%f priority)\n", vtos(pos), priority);
}

/**
 * @brief Resolves the entity at the given slot number against the canonical,
 * always-valid `ge.entities` table, rather than trusting a cached `GameEntity *`
 * that may have gone stale or been corrupted. Returns `NULL` if `number` is out
 * of range.
 */
#if AI_GOAL_HARDENING
const GameEntity *G_Ai_ResolveGoalEntity(int32_t number) {

  if (number < 0 || number >= sv_maxEntities->integer) {
    return NULL;
  }

  return ge.entities[number];
}
#endif

/**
 * @brief Setup entity goal for the specified target.
 */
void G_Ai_SetEntityGoal(const GameClient *cl, GameAiGoal *goal, float priority, const GameEntity *entity) {

  G_Ai_InitGoal(cl, goal, AI_GOAL_ENTITY, priority);
  
  goal->entity.ent = entity;
#if AI_GOAL_HARDENING
  goal->entity.number = entity->s.number;
#endif
  goal->entity.spawnId = entity->s.spawnId;

  G_Ai_Debug("New goal: %s (%f priority)\n", etos(entity), priority);
}

/**
 * @return True if the bot is on a ladder, on the ladder link of its move target's path.
 * @param bottom The node at the start of the climb.
 * @param top The node at its top.
 */
static bool G_Ai_IsOnClimb(const GameClient *cl, GameAiNodeId *bottom, GameAiNodeId *top) {

  const GameAiGoal *goal = &cl->ai->moveTarget;

  if (!(cl->ps.pmState.flags & PMF_ON_LADDER) || goal->type != AI_GOAL_PATH || !goal->path.path ||
      goal->path.pathIndex == 0 || goal->path.pathIndex >= goal->path.path->count) {
    return false;
  }

  *bottom = VectorValue(goal->path.path, GameAiNodeId, goal->path.pathIndex - 1);
  *top = VectorValue(goal->path.path, GameAiNodeId, goal->path.pathIndex);

  const GameAiLink *link = G_Ai_PathLink(cl);

  return link && link->travelType == NAV_TRAVEL_LADDER;
}

/**
 * @see g_ai_goal.h
 */
GameAiNodeId G_Ai_PathStart(const GameClient *cl, const float maxDistance) {

  GameAiNodeId bottom, top;

  if (G_Ai_IsOnClimb(cl, &bottom, &top)) {
    return top;
  }

  return G_Ai_Node_FindClosest(cl->entity->s.origin, maxDistance, true, true);
}

/**
 * @brief Setup entity goal for the specified target. A bot that climbs a ladder keeps its climb in
 * front of a new path that starts at the climb's top (`G_Ai_PathStart`), so that it does not let
 * go of the ladder for a path from the node nearest it, as one in a pipe that the ladder passes.
 * @remarks It clears the bot's strafe look-ahead, which `G_Ai_StrafeTarget` keys by the path's
 * address, because the new path can reuse the address of one that was freed, and the strafe jump
 * that `G_Ai_StrafeJumpIsSafe` found safe.
 */
void G_Ai_SetPathGoal(const GameClient *cl, GameAiGoal *goal, float priority, Vector *path, const GameEntity *pathTarget) {

  GameAiNodeId bottom, top;
  const bool climb = goal == &cl->ai->moveTarget && G_Ai_IsOnClimb(cl, &bottom, &top) &&
                     path->count && VectorValue(path, GameAiNodeId, 0) == top;

  G_Ai_InitGoal(cl, goal, AI_GOAL_PATH, priority);

  cl->ai->strafeLookTime = 0;
  cl->ai->strafeSafeTime = 0;

  if (climb) {
    $(path, insert, &bottom, 0);
  }

  goal->path.path = retain(path);
  goal->path.pathIndex = climb ? 1 : 0;

  const GameAiNodeId node = VectorValue(path, GameAiNodeId, goal->path.pathIndex);
  const GameAiNodeId next = VectorValue(path, GameAiNodeId, Minz(path->count - 1, goal->path.pathIndex + 1));
  goal->path.pathPosition = G_Ai_Node_GetPosition(node);
  goal->path.nextPathPosition = G_Ai_Node_GetPosition(next);
  goal->path.pathTarget = pathTarget;

  if (pathTarget) {
    goal->path.pathTargetSpawnId = pathTarget->s.spawnId;
  }

#if AI_GOAL_HARDENING
  goal->path.pathTargetNumber = pathTarget ? pathTarget->s.number : -1;
#endif

  G_Ai_Debug("New goal: path from %u -> %u (%f priority, heading for %s)\n", VectorValue(path, GameAiNodeId, 0), VectorValue(path, GameAiNodeId, path->count - 1), priority, etos(pathTarget));
}

/**
 * @brief Check if the goal references the same entity still
 */
bool G_Ai_GoalHasEntity(const GameAiGoal *goal, const GameEntity *ent) {

  return (goal->type == AI_GOAL_ENTITY && goal->entity.ent == ent && goal->entity.spawnId == ent->s.spawnId) ||
    (goal->type == AI_GOAL_PATH && goal->path.pathTarget == ent && goal->path.pathTargetSpawnId == ent->s.spawnId);
}

/**
 * @brief Copy a goal from one target to another, resetting time-dependent state.
 */
void G_Ai_CopyGoal(const GameAiGoal *from, GameAiGoal *to) {

  G_Ai_ClearGoal(to);

  to->type = from->type;
  to->priority = from->priority;
  to->time = gameLevel.time;
  to->lastDistance = FLT_MAX;

  switch (from->type) {
    case AI_GOAL_NONE:
      break;
    case AI_GOAL_POSITION:
      to->position.pos = from->position.pos;
      break;
    case AI_GOAL_ENTITY:
      to->entity.ent = from->entity.ent;
#if AI_GOAL_HARDENING
      to->entity.number = from->entity.number;
#endif
      to->entity.spawnId = from->entity.spawnId;
      to->entity.combatType = from->entity.combatType;
      to->entity.lockOnTime = from->entity.lockOnTime;
      to->entity.flankAngle = from->entity.flankAngle;
      break;
    case AI_GOAL_PATH:
      to->path.path = retain(from->path.path);
      to->path.pathIndex = from->path.pathIndex;
      to->path.pathPosition = from->path.pathPosition;
      to->path.nextPathPosition = from->path.nextPathPosition;
      to->path.trickJump = from->path.trickJump;
      to->path.trickPosition = from->path.trickPosition;
      to->path.pathTarget = from->path.pathTarget;
#if AI_GOAL_HARDENING
      to->path.pathTargetNumber = from->path.pathTargetNumber;
#endif
      to->path.pathTargetSpawnId = from->path.pathTargetSpawnId;
      break;
  }
}

/**
 * @brief Clear a goal
 */
void G_Ai_ClearGoal(GameAiGoal *goal) {
  
  if (goal->type == AI_GOAL_PATH) {
    release(goal->path.path);
  }

  memset(goal, 0, sizeof(GameAiGoal));
  goal->time = gameLevel.time;
  goal->lastDistance = FLT_MAX;
}
