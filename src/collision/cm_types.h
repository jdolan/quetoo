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
 * @brief Plane side epsilon.
 */
#define SIDE_EPSILON			FLT_EPSILON

/**
 * @brief Winding clipping and splitting epsilon.
 */
#define CLIP_EPSILON			0.001f

/**
 * @brief Colinear points dot product epsilon.
 */
#define COLINEAR_EPSILON		.00001f

/**
 * @brief Point equality epsilon.
 */
#define ON_EPSILON				.1f

/**
 * @brief Vertex equality epsilon.
 */
#define VERTEX_EPSILON			.375f

/**
 * @brief Trace collision epsilon.
 */
#define TRACE_EPSILON			.125f

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
 * Quake stores planes in front and back facing pairs, but most references to
 * planes in the game prefer the "positive planes," or the ones with a normal
 * vector such that all components are >= 0.
 */
typedef struct {
	vec3_t normal;
	float dist;
	int32_t type; // for fast side tests
	int32_t sign_bits; // sign_x + (sign_y << 1) + (sign_z << 2)
} cm_bsp_plane_t;

/**
 * @brief Returns true if the specified plane is axially aligned.
 */
#define AXIAL(p) ((p)->type < PLANE_ANY_X)

/**
 * @brief Texinfos describe a material applied to a winding or plane.
 */
typedef struct {
	/**
	 * @brief The texture name.
	 */
	char name[32];

	/**
	 * @brief SURF_* flags.
	 */
	int32_t flags;

	/**
	 * @brief The surface value (e.g. light radius).
	 */
	int32_t value;

	/**
	 * @brief The material definition.
	 */
	struct cm_material_s *material;
} cm_bsp_texinfo_t;

/**
 * @brief Inline BSP models are segments of the collision model that may move.
 * They are treated as their own sub-trees and recursed separately.
 */
typedef struct {
	/**
	 * @brief The index of the head node in the BSP file.
	 */
	int32_t head_node;

	/**
	 * @brief The model bounds.
	 */
	vec3_t mins, maxs;
} cm_bsp_model_t;

/**
 * @brief The maximum length of an entity key-value key, in characters.
 */
#define MAX_BSP_ENTITY_KEY		32

/**
 * @brief The maximum length of an entity key-value value, in characters.
 */
#define MAX_BSP_ENTITY_VALUE	256

/**
 * @brief Entities are, essentially, linked lists of key-value pairs.
 */
typedef struct cm_entity_s {
	char key[MAX_BSP_ENTITY_KEY];
	char value[MAX_BSP_ENTITY_VALUE];
	struct cm_entity_s *next;
} cm_entity_t;

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
	float fraction;

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

/**
 * @brief Brush sides refefence the planes and textures of a given brush.
 * @remarks Brush sides are not clipped windings, or faces. They are used for collision,
 * not for rendering.
 */
typedef struct {
	cm_bsp_plane_t *plane;
	cm_bsp_texinfo_t *surface;
} cm_bsp_brush_side_t;

/**
 * @brief Brushes are convex volumes defined by the clipping planes of their sides.
 */
typedef struct {
	/**
	 * @brief The brush contents (CONTENTS_*).
	 */
	int32_t contents;

	/**
	 * @brief The brush sides.
	 */
	cm_bsp_brush_side_t *sides;

	/**
	 * @brief The number of sides.
	 */
	int32_t num_sides;

	/**
	 * @brief The brush bounds.
	 */
	vec3_t mins, maxs;
} cm_bsp_brush_t;

/**
 * @brief Leafs are the terminating nodes of the BSP tree.
 * @details When a node has been partitioned until no brush sides occupy its bounds,
 * it becomes a leaf. The leaf brushes are those brushes which bound the leaf. Leafs
 * with non-solid contents comprise the parts of the world the player may occupy.
 */
typedef struct {
	/**
	 * @brief The leaf CONTENTS_*.
	 */
	int32_t contents;

	/**
	 * @brief The visibility cluster. Sibling leafs, and their parent node, share the
	 * same visibility cluster.
	 */
	int32_t cluster;

	/**
	 * @brief The index of the first leaf-brush reference.
	 */
	int32_t first_leaf_brush;

	/**
	 * @brief The number of leaf-brush references for this leaf.
	 */
	int32_t num_leaf_brushes;
} cm_bsp_leaf_t;

/**
 * @brief The BSP node structure.
 */
typedef struct {
	/**
	 * @brief The positive plane that separates this node's children.
	 */
	cm_bsp_plane_t *plane;

	/**
	 * @brief The child node indexes, where positive values are nodes, and negative are leafs.
	 * @remarks Because 0 can not be negated, the BSP is padded with an empty first leaf.
	 */
	int32_t children[2];
} cm_bsp_node_t;
