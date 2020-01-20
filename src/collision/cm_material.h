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

#include "cm_types.h"

typedef enum {
	ASSET_CONTEXT_NONE,
	ASSET_CONTEXT_TEXTURES,
	ASSET_CONTEXT_MODELS,
	ASSET_CONTEXT_PLAYERS,
	ASSET_CONTEXT_ENVMAPS,
	ASSET_CONTEXT_FLARES,
} cm_asset_context_t;

typedef struct {
	char name[MAX_QPATH];
	char path[MAX_QPATH];
	int32_t index;
} cm_asset_t;

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
	cm_asset_t *frames;
	vec_t fps;
} cm_stage_anim_t;

typedef enum {
	TINT_R,
	TINT_G,
	TINT_B,

	TINT_TOTAL // cannot be increased past 3
} cm_stage_tint_src_t;

typedef struct cm_stage_s {
	uint32_t flags;
	cm_asset_t asset;
	cm_stage_blend_t blend;
	vec3_t color;

	/**
	 * @brief Whether the stage will pull color from the mesh model entity being rendered.
	 */
	_Bool mesh_color;

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
typedef enum {
	STAGE_TEXTURE			= (1 << 0),
	STAGE_ENVMAP			= (1 << 1),
	STAGE_BLEND				= (1 << 2),
	STAGE_COLOR				= (1 << 3),
	STAGE_PULSE				= (1 << 4),
	STAGE_STRETCH			= (1 << 5),
	STAGE_ROTATE			= (1 << 6),
	STAGE_SCROLL_S			= (1 << 7),
	STAGE_SCROLL_T			= (1 << 8),
	STAGE_SCALE_S			= (1 << 9),
	STAGE_SCALE_T			= (1 << 10),
	STAGE_TERRAIN			= (1 << 11),
	STAGE_ANIM				= (1 << 12),
	STAGE_LIGHTMAP			= (1 << 13),
	STAGE_DIRTMAP			= (1 << 14),
	STAGE_FLARE				= (1 << 15),
	STAGE_FOG				= (1 << 16),

	// set on stages eligible for static, dynamic, and per-pixel lighting
	STAGE_LIGHTING			= (1 << 30),

	// set on stages with valid render passes
	STAGE_DIFFUSE 			= (int32_t) (1u << 31),

	// composite mask for simplifying state management
	STAGE_TEXTURE_MATRIX = (STAGE_STRETCH | STAGE_ROTATE | STAGE_SCROLL_S | STAGE_SCROLL_T | STAGE_SCALE_S | STAGE_SCALE_T)
} cm_material_flags_t;

#define DEFAULT_BUMP 1.0
#define DEFAULT_PARALLAX 1.0
#define DEFAULT_HARDNESS 1.0
#define DEFAULT_SPECULAR 1.0
#define DEFAULT_LIGHT 300.0

typedef struct cm_material_s {

	/**
	 * @brief The material name, as it appears in the materials file.
	 */
	char name[MAX_QPATH];

	/**
	 * @brief The base name of this material without any diffuse suffix.
	 */
	char basename[MAX_QPATH];

	/**
	 * @brief The diffuse asset.
	 */
	cm_asset_t diffuse;

	/**
	 * @brief The normalmap asset.
	 */
	cm_asset_t normalmap;

	/**
	 * @brief The heightmap asset.
	 */
	cm_asset_t heightmap;

	/**
	 * @brief The glossmap asset.
	 */
	cm_asset_t glossmap;

	/**
	 * @brief The tintmap asset.
	 */
	cm_asset_t tintmap;

	/**
	 * @brief Flags for the material.
	 */
	cm_material_flags_t flags;

	/**
	 * @brief Contents flags applied to brush sides referencing this material.
	 */
	uint32_t contents;

	/**
	 * @brief Surface flags applied to surfaces referencing this material.
	 */
	uint32_t surface;

	/**
	 * @brief Light emission applied to surfaces referencing this material.
	 */
	vec_t light;

	/**
	 * @brief The bump factor to use for the normal map.
	 */
	vec_t bump;

	/**
	 * @brief The hardness factor to use for the normal map.
	 */
	vec_t hardness;

	/**
	 * @brief The specular factor to use for the specular map.
	 */
	vec_t specular;

	/**
	 * @brief The parallel factor to use for the normal map.
	 */
	vec_t parallax;

	/**
	 * @brief The name for the footstep sounds to query on this surface
	 */
	char footsteps[MAX_QPATH];

	/**
	 * @brief Whether only stages should be rendered or the diffuse should too
	 */
	_Bool only_stages;

	/**
	 * @brief Default tint colors
	 */
	vec4_t tintmap_defaults[TINT_TOTAL];

	/**
	 * @brief Pointer to the first stage in the stage list. NOT an array;
	 * be sure to use ->next to traverse.
	 */
	cm_stage_t *stages;

	/**
	 * @brief The total number of stages in the "stages" list.
	 */
	uint16_t num_stages;
} cm_material_t;

cm_material_t *Cm_AllocMaterial(const char *name);
void Cm_FreeMaterial(cm_material_t *material);
void Cm_FreeMaterials(GList *materials, _Bool full);
ssize_t Cm_LoadMaterials(const char *path, GList **materials);
_Bool Cm_ResolveMaterial(cm_material_t *material, cm_asset_context_t context);
ssize_t Cm_WriteMaterials(const char *path, GList *materials);

void Cm_MaterialBasename(const char *in, char *out, size_t len);

#ifdef __CM_LOCAL_H__
#endif /* __CM_LOCAL_H__ */
