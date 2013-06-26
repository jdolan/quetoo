/*
 * Copyright(c) 1997-2001 Id Software, Inc.
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

#include "ai_local.h"

typedef struct {
	GList *goals;
} ai_goal_state_t;

static ai_goal_state_t ai_goal_state;

/*
 * @brief Utility function for instantiating ai_goal_t.
 */
ai_goal_t *Ai_AllocGoal(const ai_goal_type_t type, g_edict_t *ent) {
	ai_goal_t *goal = Z_TagMalloc(sizeof(*goal), Z_TAG_AI);

	goal->type = type;
	goal->ent = ent;

	// append the goal to the list
	ai_goal_state.goals = g_list_prepend(ai_goal_state.goals, goal);

	return goal;
}

/*
 * @brief GDestroyNotify for ai_goal_t.
 */
static void Ai_FreeGoal(gpointer data) {
	ai_goal_t *goal = (ai_goal_t *) data;

	Z_Free(goal);
}

/*
 * @brief Frees all ai_goal_t.
 */
void Ai_FreeGoals(void) {

	g_list_free_full(ai_goal_state.goals, Ai_FreeGoal);

	memset(&ai_goal_state, 0, sizeof(ai_goal_state));
}
