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
 * @brief Setup base entity goal for the specified target.
 */
static inline void G_Ai_InitGoal(const GameClient *cl, AiGoal *goal, AiGoalType type, float priority) {

  G_Ai_ClearGoal(goal);

  goal->type = type;
  goal->priority = priority;
}

/**
 * @brief Setup entity goal for the specified target.
 */
void G_Ai_SetPositionGoal(const GameClient *cl, AiGoal *goal, float priority, const Vec3 pos) {

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

  if (number < 0 || number >= sv_max_entities->integer) {
    return NULL;
  }

  return ge.entities[number];
}
#endif

/**
 * @brief Setup entity goal for the specified target.
 */
void G_Ai_SetEntityGoal(const GameClient *cl, AiGoal *goal, float priority, const GameEntity *entity) {

  G_Ai_InitGoal(cl, goal, AI_GOAL_ENTITY, priority);
  
  goal->entity.ent = entity;
#if AI_GOAL_HARDENING
  goal->entity.number = entity->s.number;
#endif
  goal->entity.spawnId = entity->s.spawnId;

  G_Ai_Debug("New goal: %s (%f priority)\n", etos(entity), priority);
}

/**
 * @brief Setup entity goal for the specified target.
 */
void G_Ai_SetPathGoal(const GameClient *cl, AiGoal *goal, float priority, Vector *path, const GameEntity *pathTarget) {

  G_Ai_InitGoal(cl, goal, AI_GOAL_PATH, priority);
  
  goal->path.path = retain(path);
  goal->path.pathIndex = 0;

  const AiNodeId node = VectorValue(path, AiNodeId, 0);
  const AiNodeId next = VectorValue(path, AiNodeId, Minz(path->count - 1, 1));
  goal->path.pathPosition = G_Ai_Node_GetPosition(node);
  goal->path.nextPathPosition = G_Ai_Node_GetPosition(next);
  goal->path.pathTarget = pathTarget;

#if AI_GOAL_HARDENING
  if (pathTarget) {
    goal->path.pathTargetNumber = pathTarget->s.number;
    goal->path.pathTargetSpawnId = pathTarget->s.spawnId;
  } else {
    goal->path.pathTargetNumber = -1;
  }
#else
  if (path_target) {
    goal->path.path_target_spawn_id = path_target->s.spawn_id;
  }
#endif

  G_Ai_Debug("New goal: path from %u -> %u (%f priority, heading for %s)\n", VectorValue(path, AiNodeId, 0), VectorValue(path, AiNodeId, path->count - 1), priority, etos(pathTarget));
}

/**
 * @brief Check if the goal references the same entity still
 */
bool G_Ai_GoalHasEntity(const AiGoal *goal, const GameEntity *ent) {

  return (goal->type == AI_GOAL_ENTITY && goal->entity.ent == ent && goal->entity.spawnId == ent->s.spawnId) ||
    (goal->type == AI_GOAL_PATH && goal->path.pathTarget == ent && goal->path.pathTargetSpawnId == ent->s.spawnId);
}

/**
 * @brief Copy a goal from one target to another, resetting time-dependent state.
 */
void G_Ai_CopyGoal(const AiGoal *from, AiGoal *to) {

  G_Ai_ClearGoal(to);

  to->type = from->type;
  to->priority = from->priority;
  to->time = g_level.time;
  to->lastDistance = FLT_MAX;

  switch (from->type) {
    case AI_GOAL_NONE:
      to->wander.angle = from->wander.angle;
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
void G_Ai_ClearGoal(AiGoal *goal) {
  
  if (goal->type == AI_GOAL_PATH) {
    release(goal->path.path);
  }

  memset(goal, 0, sizeof(AiGoal));
  goal->time = g_level.time;
  goal->lastDistance = FLT_MAX;
}
