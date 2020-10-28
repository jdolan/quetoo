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
 * @brief Add the specified goal function to the bot, and run it at the specified
 * time offset.
 */
void Ai_AddFuncGoal(g_entity_t *ent, Ai_GoalFunc func, uint32_t time_offset) {
	ai_locals_t *ai = Ai_GetLocals(ent);

	for (int32_t i = 0; i < MAX_AI_FUNCGOALS; i++) {
		ai_funcgoal_t *funcgoal = &ai->funcgoals[i];

		if (funcgoal->think) {
			continue;
		}

		funcgoal->think = func;
		funcgoal->nextthink = ai_level.time + time_offset;
		funcgoal->time = ai_level.time;
		return;
	}

	aim.gi->Warn("Bot ran out of empty goal slots\n");
}

/**
 * @brief Remove the specified goal function from the bot.
 */
void Ai_RemoveFuncGoal(g_entity_t *ent, Ai_GoalFunc func) {
	ai_locals_t *ai = Ai_GetLocals(ent);

	for (int32_t i = 0; i < MAX_AI_FUNCGOALS; i++) {
		ai_funcgoal_t *funcgoal = &ai->funcgoals[i];

		if (funcgoal->think != func) {
			continue;
		}

		funcgoal->think = NULL;
		funcgoal->nextthink = 0;
		return;
	}
}

/**
 * @brief Setup entity goal for the specified target.
 */
void Ai_SetEntityGoal(ai_goal_t *goal, ai_goal_type_t type, float priority, const g_entity_t *entity) {

	Ai_ClearGoal(goal);

	aim.gi->Debug("New goal: %s (%f priority)\n", etos(entity), priority);

	goal->type = type;
	goal->time = ai_level.time;
	goal->priority = priority;
	goal->ent = entity;
	goal->ent_id = entity->spawn_id;
}

/**
 * @brief Setup entity goal for the specified target.
 */
void Ai_SetPathGoal(ai_goal_t *goal, ai_goal_type_t type, float priority, GArray *path) {

	Ai_ClearGoal(goal);

	aim.gi->Debug("New goal: path from %u -> %u (%f priority)\n", g_array_index(path, ai_node_id_t, 0), g_array_index(path, ai_node_id_t, path->len - 1), priority);

	goal->type = type;
	goal->time = ai_level.time;
	goal->priority = priority;
	goal->path = g_array_ref(path);
	goal->path_index = 0;
	goal->path_position = Ai_Node_GetPosition(g_array_index(path, ai_node_id_t, goal->path_index));
}

/**
 * @brief Check if the goal references the same entity still
 */
_Bool Ai_GoalHasEntity(const ai_goal_t *goal, const g_entity_t *ent) {

	return goal->ent == ent && goal->ent_id == ent->spawn_id;
}

/**
 * @brief Copy a goal from one target to another
 */
void Ai_CopyGoal(const ai_goal_t *from, ai_goal_t *to) {

	Ai_ClearGoal(to);

	memcpy(to, from, sizeof(ai_goal_t));
	to->time = ai_level.time;

	if (from->path) {
		to->path = g_array_ref(from->path);
	}
}

/**
 * @brief Clear a goal
 */
void Ai_ClearGoal(ai_goal_t *goal) {
	
	if (goal->path) {
		g_array_unref(goal->path);
	}

	memset(goal, 0, sizeof(ai_goal_t));
	goal->time = ai_level.time;
	goal->path_index = NODE_INVALID;
}
