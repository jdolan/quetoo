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

#ifndef __AI_TYPES_H__
#define __AI_TYPES_H__

#include "files.h"
#include "mem.h"

typedef enum {
	AI_NODE_NAV,
	AI_NODE_ITEM,
	AI_NODE_AI,
	AI_NODE_CLIENT
} ai_node_type_t;

typedef struct {
	ai_node_type_t type;
	int32_t leaf;
	int32_t cluster;
	vec3_t mins, maxs;
	vec3_t center;
	GHashTable *links;
	vec_t priority;
} ai_node_t;

typedef struct {
	ai_node_t *to;
	vec_t dist;
} ai_node_link_t;

#endif /* __AI_TYPES_H__ */
