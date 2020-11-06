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

#include "ai_local.h"

/**
 * @brief Setup base entity goal for the specified target.
 */
static inline void Ai_SetGoalBase(const g_entity_t *self, ai_goal_t *goal, ai_goal_type_t type, float priority) {

	Ai_ClearGoal(goal);

	goal->type = type;
	goal->priority = priority;
}

/**
 * @brief Setup entity goal for the specified target.
 */
void Ai_SetPositionalGoal(const g_entity_t *self, ai_goal_t *goal, float priority, const vec3_t pos) {

	Ai_SetGoalBase(self, goal, AI_GOAL_POSITION, priority);

	goal->position.pos = pos;

	aim.gi->Debug("New goal: %s (%f priority)\n", vtos(pos), priority);
}

/**
 * @brief Setup entity goal for the specified target.
 */
void Ai_SetEntityGoal(const g_entity_t *self, ai_goal_t *goal, float priority, const g_entity_t *entity) {

	Ai_SetGoalBase(self, goal, AI_GOAL_ENTITY, priority);
	
	goal->entity.ent = entity;
	goal->entity.spawn_id = entity->spawn_id;

	aim.gi->Debug("New goal: %s (%f priority)\n", etos(entity), priority);
}

/**
 * @brief Setup entity goal for the specified target.
 */
void Ai_SetPathGoal(const g_entity_t *self, ai_goal_t *goal, float priority, GArray *path, const g_entity_t *path_target) {

	Ai_SetGoalBase(self, goal, AI_GOAL_PATH, priority);
	
	goal->path.path = g_array_ref(path);
	goal->path.path_index = 0;
	goal->path.path_position = Ai_Node_GetPosition(g_array_index(path, ai_node_id_t, goal->path.path_index));
	goal->path.next_path_position = Ai_Node_GetPosition(g_array_index(path, ai_node_id_t, Mini(path->len - 1, goal->path.path_index + 1)));
	goal->path.path_target = path_target;

	if (path_target) {
		goal->path.path_target_spawn_id = path_target->spawn_id;
	}

	aim.gi->Debug("New goal: path from %u -> %u (%f priority, heading for %s)\n", g_array_index(path, ai_node_id_t, 0), g_array_index(path, ai_node_id_t, path->len - 1), priority, etos(path_target));
}

/**
 * @brief Check if the goal references the same entity still
 */
_Bool Ai_GoalHasEntity(const ai_goal_t *goal, const g_entity_t *ent) {

	return (goal->type == AI_GOAL_ENTITY && goal->entity.ent == ent && goal->entity.spawn_id == ent->spawn_id) ||
		(goal->type == AI_GOAL_PATH && goal->path.path_target == ent && goal->path.path_target_spawn_id == ent->spawn_id);;
}

/**
 * @brief Copy a goal from one target to another
 */
void Ai_CopyGoal(const ai_goal_t *from, ai_goal_t *to) {

	Ai_ClearGoal(to);

	memcpy(to, from, sizeof(ai_goal_t));

	// reset state-dependent objects
	to->time = ai_level.time;
	to->last_distance = FLT_MAX;
	to->distress = 0;
	to->distress_extension = false;

	if (from->type == AI_GOAL_PATH) {
		to->path.path = g_array_ref(from->path.path);
	}
}

/**
 * @brief Clear a goal
 */
void Ai_ClearGoal(ai_goal_t *goal) {
	
	if (goal->type == AI_GOAL_PATH) {
		g_array_unref(goal->path.path);
	}

	memset(goal, 0, sizeof(ai_goal_t));
	goal->time = ai_level.time;
	goal->last_distance = FLT_MAX;
}
