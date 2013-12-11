/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

/*
 * @brief Allocs a g_ai_node_t for the specified item entity.
 */
static ai_goal_t *G_Ai_AllocGoal_Item(g_edict_t *ent) {
	/*ai_goal_t *goal = Ai_AllocGoal(AI_GOAL_ITEM, ent);

	goal->priority = ent->locals.item->priority;

	return goal;*/return NULL;
}

/*
 * @brief Allocs a g_ai_node_t for the specified player spawn entity.
 */
static ai_goal_t *G_Ai_AllocGoal_Spawn(g_edict_t *ent) {
	//return Ai_AllocGoal(AI_GOAL_NAV, ent);
	return NULL;
}

/*
 * @brief Allocs the node list and path table for the current level.
 */
void G_Ai_AllocGoals(void) {
	g_edict_t *ent;

	int32_t i = sv_max_clients->integer + 1;
	for (ent = &g_game.edicts[i]; i < g_max_entities->integer; i++, ent++) {

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
}
