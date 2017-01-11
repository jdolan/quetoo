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

#pragma once

#include "mem.h"
#include "game/game.h"

typedef struct {
	GList *portals;
} ai_path_t;

typedef struct {
	cm_bsp_plane_t *plane;
	vec3_t mins;
	vec3_t maxs;
	GHashTable *paths;
} ai_node_t;

typedef enum {
	AI_GOAL_NONE,
	AI_GOAL_NAV,
	AI_GOAL_GHOST,
	AI_GOAL_ITEM,
	AI_GOAL_ENEMY,
	AI_GOAL_TEAMMATE
} ai_goal_type_t;

typedef struct {
	ai_goal_type_t type;
	vec_t priority;
	uint32_t time; // time this goal was set
	
	union {
		g_entity_t *ent; // for AI_GOAL_ITEM/ENEMY_TEAMMATE
		ai_node_t *node; // for AI_GOAL_NAV
	};
} ai_goal_t;

/**
 * @brief A G_AIGoalFunc can return this if the goal is finished.
 */
#define AI_GOAL_COMPLETE	0

/**
 * @brief A functional AI goal. It returns the amount of time to wait
 * until the goal should be run again.
 */
typedef uint32_t (*G_AIGoalFunc)(g_entity_t *ent, pm_cmd_t *cmd);

/**
 * @brief A functional AI goal. 
 */
typedef struct {
	G_AIGoalFunc think;
	uint32_t nextthink;
	uint32_t time; // time this funcgoal was added
} g_ai_funcgoal_t;

/**
 * @brief The total number of functional goals the AI may have at once.
 */
#define MAX_AI_FUNCGOALS	12

/**
 * @brief AI-specific locals
 */
typedef struct g_entity_ai_s {
	g_ai_funcgoal_t funcgoals[MAX_AI_FUNCGOALS];

	// the AI can have two distinct targets: one it's aiming at,
	// and one it's moving towards. These aren't pointers because
	// the priority of an item/enemy might be different depending on
	// the state of the bot.
	ai_goal_t aim_target;
	ai_goal_t move_target;

	vec_t wander_angle;
	vec3_t ghost_position;
} g_entity_ai_t;

#ifdef __AI_LOCAL_H__
#endif /* __AI_LOCAL_H__ */
