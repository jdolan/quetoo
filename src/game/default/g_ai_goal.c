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
#include "ai/ai.h"

/**
 * @brief Add the specified goal function to the bot, and run it at the specified
 * time offset.
 */
void G_Ai_AddFuncGoal(g_entity_t *ent, G_AIGoalFunc func, uint32_t time_offset) {

	for (int32_t i = 0; i < MAX_AI_FUNCGOALS; i++) {
		g_ai_funcgoal_t *funcgoal = &ent->locals.ai_locals->funcgoals[i];
		
		if (funcgoal->think) {
			continue;
		}

		funcgoal->think = func;
		funcgoal->nextthink = g_level.time + time_offset;
		funcgoal->time = g_level.time;
		return;
	}

	gi.Warn("Bot ran out of empty goal slots\n");
}

/**
 * @brief Remove the specified goal function from the bot.
 */
void G_Ai_RemoveFuncGoal(g_entity_t *ent, G_AIGoalFunc func) {
	
	for (int32_t i = 0; i < MAX_AI_FUNCGOALS; i++) {
		g_ai_funcgoal_t *funcgoal = &ent->locals.ai_locals->funcgoals[i];
		
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
void G_Ai_SetEntityGoal(ai_goal_t *goal, ai_goal_type_t type, vec_t priority, g_entity_t *entity) {

	goal->type = type;
	goal->time = g_level.time;
	goal->priority = priority;
	goal->ent = entity;
}

/**
 * @brief Copy a goal from one target to another
 */
void G_Ai_CopyGoal(const ai_goal_t *from, ai_goal_t *to) {

	memcpy(to, from, sizeof(ai_goal_t));
	to->time = g_level.time;
}

/**
 * @brief Clear a goal
 */
void G_Ai_ClearGoal(ai_goal_t *goal) {

	memset(goal, 0, sizeof(ai_goal_t));
	goal->time = g_level.time;
}

/**
 * @brief Allocs a g_ai_node_t for the specified item entity.
 */
/*static ai_goal_t *G_Ai_AllocGoal_Item(g_entity_t *ent) {
	ai_goal_t *goal = Ai_AllocGoal(AI_GOAL_ITEM, ent);

	goal->priority = ent->locals.item->priority;

	return goal;
}*/

/**
 * @brief Allocs a g_ai_node_t for the specified player spawn entity.
 */
/*static ai_goal_t *G_Ai_AllocGoal_Spawn(g_entity_t *ent) {
	return Ai_AllocGoal(AI_GOAL_NAV, ent);
}*/

/**
 * @brief Allocs the node list and path table for the current level.
 */
/*void G_Ai_AllocGoals(void) {
	g_entity_t *ent;

	int32_t i = sv_max_clients->integer + 1;
	for (ent = &g_game.entities[i]; i < g_max_entities->integer; i++, ent++) {

		if (!ent->in_use) {
			continue;
		}

		if (ent->locals.item) {
			G_Ai_AllocGoal_Item(ent);
		}

		if (g_str_has_prefix(ent->class_name, "info_player_")) {
			G_Ai_AllocGoal_Spawn(ent);
		}
	}
}*/
