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

#include "quetoo.h"

/**
 * @brief Plane side epsilon value.
 */
#define	SIDE_EPSILON			0.001

/**
 * @brief Plane side constants used for BSP recursion.
 */
#define	SIDE_FRONT				1
#define	SIDE_BACK				2
#define	SIDE_BOTH				3
#define	SIDE_FACING				4

/**
 * @brief Plane type constants for axial plane optimizations.
 */
#define PLANE_X					0
#define PLANE_Y					1
#define PLANE_Z					2
#define PLANE_ANY_X				3
#define PLANE_ANY_Y				4
#define PLANE_ANY_Z				5

/**
 * @brief BSP planes are essential to collision detection as well as rendering.
 * Quake uses "positive planes," where the plane distances are represented as
 * negative offsets from the origin.
 */
typedef struct {
	vec3_t normal;
	vec_t dist;
	uint16_t type; // for fast side tests
	uint16_t sign_bits; // sign_x + (sign_y << 1) + (sign_z << 2)
	uint16_t num; // the plane number, shared by both instances (sides) of a given plane
} cm_bsp_plane_t;

/**
 * @brief Returns true if the specified plane is axially aligned.
 */
#define AXIAL(p) ((p)->type < PLANE_ANY_X)

/**
 * @brief BSP surfaces describe a material applied to a plane.
 */
typedef struct {
	char name[32];
	int32_t flags;
	int32_t value;
	struct cm_material_s *material;
} cm_bsp_texinfo_t;

/**
 * @brief Inline BSP models are segments of the collision model that may move.
 * They are treated as their own sub-trees and recursed separately.
 */
typedef struct {
	vec3_t mins, maxs;
	vec3_t origin; // for sounds or lights
	int32_t head_node;
} cm_bsp_model_t;

/**
 * @brief The maximum length of an entity key-value key, in characters.
 */
#define MAX_BSP_ENTITY_KEY		32

/**
 * @brief The maximum length of an entity key-value value, in characters.
 */
#define MAX_BSP_ENTITY_VALUE	1024

/**
 * @brief Traces are discrete movements through world space, clipped to the
 * BSP planes they intersect. This is the basis for all collision detection
 * within Quake.
 */
typedef struct {
	/**
	 * @brief If true, the trace started and ended within the same solid.
	 */
	_Bool all_solid;

	/**
	 * @brief If true, the trace started within a solid, but exited it.
	 */
	_Bool start_solid;

	/**
	 * @brief The fraction of the desired distance traveled (0.0 - 1.0). If
	 * 1.0, no plane was impacted.
	 */
	vec_t fraction;

	/**
	 * @brief The destination position.
	 */
	vec3_t end;

	/**
	 * @brief The impacted plane, or empty. Note that a copy of the plane is
	 * returned, rather than a pointer. This is because the plane may belong to
	 * an inline BSP or the box hull of a solid entity, in which case it must
	 * be transformed by the entity's current position.
	 */
	cm_bsp_plane_t plane;

	/**
	 * @brief The impacted surface, or `NULL`.
	 */
	cm_bsp_texinfo_t *surface;

	/**
	 * @brief The contents mask of the impacted brush, or 0.
	 */
	int32_t contents;

	/**
	 * @brief The impacted entity, or `NULL`.
	 */
	struct g_entity_s *ent; // not set by Cm_*() functions
} cm_trace_t;

typedef struct {
	cm_bsp_plane_t *plane;
	int32_t children[2]; // negative numbers are leafs
} cm_bsp_node_t;

typedef struct {
	cm_bsp_plane_t *plane;
	cm_bsp_texinfo_t *surface;
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
