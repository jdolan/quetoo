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

#ifndef __AI_TYPES_H__
#define __AI_TYPES_H__

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
	AI_GOAL_NAV,
	AI_GOAL_ITEM,
	AI_GOAL_ENEMY,
	AI_GOAL_TEAMMATE
} ai_goal_type_t;

typedef struct {
	ai_goal_type_t type;
	g_edict_t *ent;
	vec_t priority;
	ai_node_t *node;
} ai_goal_t;

#endif /* __AI_TYPES_H__ */
