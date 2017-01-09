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
} cm_bsp_surface_t;

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
	cm_bsp_surface_t *surface;

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
	uint32_t src, dest;
} cm_stage_blend_t;

typedef struct {
	vec_t hz;
} cm_stage_pulse_t;

typedef struct {
	vec_t hz;
	vec_t amp;
} cm_stage_stretch_t;

typedef struct {
	vec_t hz;
} cm_stage_rotate_t;

typedef struct {
	vec_t s, t;
} cm_stage_scroll_t;

typedef struct {
	vec_t s, t;
} cm_stage_scale_t;

typedef struct {
	vec_t floor, ceil;
	vec_t height;
} cm_stage_terrain_t;

typedef struct {
	vec_t intensity;
} cm_stage_dirt_t;

// frame based material animation, lerp between consecutive images
typedef struct {
	uint16_t num_frames;
	vec_t fps;
} cm_stage_anim_t;

typedef struct cm_stage_s {
	uint32_t flags;
	char image[MAX_QPATH];
	int32_t image_index;
	struct cm_material_s *material;
	cm_stage_blend_t blend;
	vec3_t color;
	cm_stage_pulse_t pulse;
	cm_stage_stretch_t stretch;
	cm_stage_rotate_t rotate;
	cm_stage_scroll_t scroll;
	cm_stage_scale_t scale;
	cm_stage_terrain_t terrain;
	cm_stage_dirt_t dirt;
	cm_stage_anim_t anim;
	struct cm_stage_s *next;
} cm_stage_t;

// stage flags will persist on the stage structures but may also bubble
// up to the material flags to determine render eligibility
#define STAGE_TEXTURE			(1 << 0)
#define STAGE_ENVMAP			(1 << 1)
#define STAGE_BLEND				(1 << 2)
#define STAGE_COLOR				(1 << 3)
#define STAGE_PULSE				(1 << 4)
#define STAGE_STRETCH			(1 << 5)
#define STAGE_ROTATE			(1 << 6)
#define STAGE_SCROLL_S			(1 << 7)
#define STAGE_SCROLL_T			(1 << 8)
#define STAGE_SCALE_S			(1 << 9)
#define STAGE_SCALE_T			(1 << 10)
#define STAGE_TERRAIN			(1 << 11)
#define STAGE_ANIM				(1 << 12)
#define STAGE_LIGHTMAP			(1 << 13)
#define STAGE_DIRTMAP			(1 << 14)
#define STAGE_FLARE				(1 << 15)
#define STAGE_FOG				(1 << 16)

// set on stages eligible for static, dynamic, and per-pixel lighting
#define STAGE_LIGHTING			(1 << 30)

// set on stages with valid render passes
#define STAGE_DIFFUSE 			(1u << 31)

// composite mask for simplifying state management
#define STAGE_TEXTURE_MATRIX (\
                              STAGE_STRETCH | STAGE_ROTATE | STAGE_SCROLL_S | STAGE_SCROLL_T | STAGE_SCALE_S | STAGE_SCALE_T \
                             )

#define DEFAULT_BUMP 1.0
#define DEFAULT_PARALLAX 1.0
#define DEFAULT_HARDNESS 1.0
#define DEFAULT_SPECULAR 1.0

/**
 * @brief A material is applied to BSP surfaces and contains data about how it is
 * rendered and how other objects interact with the surface.
 */
typedef struct cm_material_s {
	uint32_t ref_count; // refs for the cm system, do not touch
	char base[MAX_QPATH];
	char key[MAX_QPATH];

	char full_name[MAX_QPATH]; // the original full name of the material (#..., etc)
	char material_file[MAX_QPATH]; // the material file we got loaded from

	char diffuse[MAX_QPATH];
	char normalmap[MAX_QPATH];
	char specularmap[MAX_QPATH];
	uint32_t flags;
	vec_t bump;
	vec_t parallax;
	vec_t hardness;
	vec_t specular;
	char footsteps[MAX_QPATH];
	cm_stage_t *stages;
	uint16_t num_stages;
} cm_material_t;

typedef void (*EnumerateMaterialsCallback) (cm_material_t *material);

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

#endif /* __CM_LOCAL_H__ */
