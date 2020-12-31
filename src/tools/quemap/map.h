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

#include "brush.h"
#include "entity.h"

/**
 * @brief The map file representation of a plane.
 */
typedef struct plane_s {
	/**
	 * @brief The plane normal vector.
	 */
	vec3_t normal;

	/**
	 * @brief The plane distance, with full double precision.
	 */
	double dist;

	/**
	 * @brief The plane type, for axial optimizations.
	 */
	int32_t type;

	/**
	 * @brief The plane hash chain, for fast plane lookups.
	 */
	struct plane_s *hash_chain;
} plane_t;

/**
 * @brief Sentinel texinfo identifier for BSP decision nodes.
 */
#define TEXINFO_NODE -1

/**
 * @brief The map file reprensetation of a brush side.
 */
typedef struct brush_side_s {
	/**
	 * @brief The diffusemap texture name.
	 */
	char texture[32];

	/**
	 * @brief The texture shift, in pixels.
	 */
	vec2_t shift;

	/**
	 * @brief The texture rotation, in Euler degrees.
	 */
	float rotate;

	/**
	 * @brief The texture scale.
	 */
	vec2_t scale;

	/**
	 * @brief The CONTENTS_* mask.
	 */
	int32_t contents;

	/**
	 * @brief The SURF_* mask.
	 */
	int32_t surf;

	/**
	 * @brief The value, for e.g. SURF_LIGHT or SURF_PHONG.
	 */
	int32_t value;

	/**
	 * @brief The BSP plane number.
	 */
	int32_t plane_num;

	/**
	 * @brief The BSP texinfo number.
	 */
	int32_t texinfo;

	/**
	 * @brief All brush sides will have a valid winding.
	 */
	cm_winding_t *winding;

	/**
	 * @brief During the BSP process, brush sides are split recursively. All split sides
	 * will point to their original brush side, to tie BSP faces back to their brushes.
	 */
	const struct brush_side_s *original;

	/**
	 * @brief If true, this side has been marked visible from within the map by a portal.
	 */
	_Bool visible;

	/**
	 * @brief Additional brush sides are allocated to provide axial clipping planes for all brushes.
	 * Bevels should not be used for BSP splitting and face generation. They are only used for
	 * collision detection.
	 */
	_Bool bevel;
} brush_side_t;

/**
 * @brief The map file representation of a brush.
 */
typedef struct brush_s {

	/**
	 * @brief The entity number within the map.
	 */
	int32_t entity_num;

	/**
	 * @brief The brush number within the entity.
	 */
	int32_t brush_num;

	/**
	 * @brief The combined CONTENTS_* mask (bitwise OR) of all sides of this brush.
	 */
	int32_t contents;

	/**
	 * @brief The brush bounds, calculated by clipping all side planes against each other.
	 */
	vec3_t mins, maxs;

	/**
	 * @brief The brush sides.
	 * @remarks This is a pointer to a statically allocated global array.
	 */
	brush_side_t *sides;

	/**
	 * @brief The number of brush sides.
	 */
	int32_t num_sides;
} brush_t;

extern int32_t num_entities;
extern entity_t entities[MAX_BSP_ENTITIES];

extern plane_t planes[MAX_BSP_PLANES];
extern int32_t num_planes;

extern int32_t num_brushes;
extern brush_t brushes[MAX_BSP_BRUSHES];

extern vec3_t map_mins, map_maxs;

int32_t FindPlane(const vec3_t normal, double dist);
void LoadMapFile(const char *filename);
