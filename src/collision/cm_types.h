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

#ifndef __CM_TYPES_H__
#define __CM_TYPES_H__

#include "files.h"
#include "filesystem.h"
#include "matrix.h"

/*
 * @brief Inline BSP models are segments of the collision model that may move.
 * They are treated as their own sub-trees and recursed separately.
 */
typedef struct {
	vec3_t mins, maxs;
	vec3_t origin; // for sounds or lights
	int32_t head_node;
} cm_bsp_model_t;

#ifdef __CM_LOCAL_H__

typedef struct {
	cm_bsp_plane_t *plane;
	int32_t children[2]; // negative numbers are leafs
} cm_bsp_node_t;

typedef struct {
	cm_bsp_plane_t *plane;
	cm_bsp_surface_t *surface;
} cm_bsp_brush_side_t;

typedef struct {
	int32_t contents;
	int32_t cluster;
	int32_t area;
	uint16_t first_leaf_brush;
	uint16_t num_leaf_brushes;
} cm_bsp_leaf_t;

typedef struct {
	int32_t contents;
	int32_t num_sides;
	int32_t first_brush_side;
	vec3_t mins, maxs;
} cm_bsp_brush_t;

typedef struct {
	int32_t num_area_portals;
	int32_t first_area_portal;
	int32_t flood_num; // if two areas have equal flood_nums, they are connected
	int32_t flood_valid;
} cm_bsp_area_t;

typedef struct {
	char name[MAX_QPATH];
	byte *base;

	int32_t entity_string_len;
	char entity_string[MAX_BSP_ENT_STRING];

	int32_t num_planes;
	cm_bsp_plane_t planes[MAX_BSP_PLANES + 12]; // extra for box hull

	int32_t num_nodes;
	cm_bsp_node_t nodes[MAX_BSP_NODES + 6]; // extra for box hull

	int32_t num_surfaces;
	cm_bsp_surface_t surfaces[MAX_BSP_TEXINFO];

	int32_t num_leafs;
	cm_bsp_leaf_t leafs[MAX_BSP_LEAFS + 1]; // extra for box hull
	int32_t empty_leaf, solid_leaf;

	int32_t num_leaf_brushes;
	uint16_t leaf_brushes[MAX_BSP_LEAF_BRUSHES + 1]; // extra for box hull

	int32_t num_models;
	cm_bsp_model_t models[MAX_BSP_MODELS];

	int32_t num_brushes;
	cm_bsp_brush_t brushes[MAX_BSP_BRUSHES + 1]; // extra for box hull

	int32_t num_brush_sides;
	cm_bsp_brush_side_t brush_sides[MAX_BSP_BRUSH_SIDES + 6]; // extra for box hull

	int32_t num_visibility;
	byte visibility[MAX_BSP_VISIBILITY];

	int32_t num_areas;
	cm_bsp_area_t areas[MAX_BSP_AREAS];

	int32_t num_area_portals;
	d_bsp_area_portal_t area_portals[MAX_BSP_AREA_PORTALS];

	_Bool portal_open[MAX_BSP_AREA_PORTALS];
	int32_t flood_valid;
} cm_bsp_t;

typedef d_bsp_vis_t cm_vis_t;

#endif /* __CM_LOCAL_H__ */

#endif /* __CM_TYPES_H__ */
