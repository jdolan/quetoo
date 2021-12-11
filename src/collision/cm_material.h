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

/**
 * @brief Asset contexts are paths (and conventions) for locating and loading material assets.
 */
typedef enum {
	ASSET_CONTEXT_NONE,
	ASSET_CONTEXT_TEXTURES,
	ASSET_CONTEXT_MODELS,
	ASSET_CONTEXT_PLAYERS,
	ASSET_CONTEXT_SPRITES,
} cm_asset_context_t;

typedef struct {
	char name[MAX_QPATH];
	char path[MAX_QPATH];
} cm_asset_t;

typedef struct {
	uint32_t src, dest;
} cm_stage_blend_t;

typedef struct {
	float hz;
} cm_stage_pulse_t;

typedef struct {
	float hz;
	float amp;
} cm_stage_stretch_t;

typedef struct {
	float hz;
} cm_stage_rotate_t;

typedef struct {
	float s, t;
} cm_stage_scroll_t;

typedef struct {
	float s, t;
} cm_stage_scale_t;

typedef struct {
	float floor, ceil;
} cm_stage_terrain_t;

typedef struct {
	float intensity;
} cm_stage_dirtmap_t;

typedef struct {
	float hz;
	float amplitude;
} cm_stage_warp_t;

typedef struct {
	float radius;
} cm_stage_shell_t;

// frame based material animation, lerp between consecutive images
typedef struct {
	int32_t num_frames;
	cm_asset_t *frames;
	float fps;
} cm_stage_animation_t;

typedef enum {
	TINT_R,
	TINT_G,
	TINT_B,

	TINT_TOTAL // cannot be increased past 3
} cm_stage_tint_src_t;

/**
 * @brief Stage flags indicate what assets and effects a material or stage may include.
 */
typedef enum {
	STAGE_TEXTURE			= (1 << 0),
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
	STAGE_ANIMATION			= (1 << 12),
	STAGE_LIGHTMAP			= (1 << 13),
	STAGE_DIRTMAP			= (1 << 14),
	STAGE_ENVMAP            = (1 << 15),
	STAGE_WARP				= (1 << 16),
	STAGE_FLARE				= (1 << 17),
	STAGE_FOG				= (1 << 18),
	STAGE_SHELL				= (1 << 19),

	STAGE_DRAW 				= (1 << 28),
	STAGE_MATERIAL			= (1 << 29),

} cm_stage_flags_t;

/**
 * @brief Stages are ordered layers of visual effects rendered on top of their material.
 */
typedef struct cm_stage_s {
	/**
	 * @brief The stage flags.
	 */
	cm_stage_flags_t flags;

	/**
	 * @brief The stage asset.
	 */
	cm_asset_t asset;

	/**
	 * @brief The stage alpha blend function.
	 */
	cm_stage_blend_t blend;

	/**
	 * @brief The stage color.
	 */
	color_t color;

	/**
	 * @brief The stage pulse effect.
	 */
	cm_stage_pulse_t pulse;

	/**
	 * @brief The stage stretch effect.
	 */
	cm_stage_stretch_t stretch;

	/**
	 * @brief The stage rotate effect.
	 */
	cm_stage_rotate_t rotate;

	/**
	 * @brief The stage scroll effect.
	 */
	cm_stage_scroll_t scroll;

	/**
	 * @brief The stage scale effect.
	 */
	cm_stage_scale_t scale;

	/**
	 * @brief The stage terrain effect.
	 */
	cm_stage_terrain_t terrain;

	/**
	 * @brief The stage dirtmap effect.
	 */
	cm_stage_dirtmap_t dirtmap;

	/**
	 * @brief The stage warp effect.
	 */
	cm_stage_warp_t warp;

	/**
	 * @brief The stage shell effect.
	 */
	cm_stage_shell_t shell;

	/**
	 * @brief The stage animation effect.
	 */
	cm_stage_animation_t animation;

	/**
	 * @brief The next stage, or NULL.
	 */
	struct cm_stage_s *next;
} cm_stage_t;

#define DEFAULT_ROUGHNESS 1.0
#define DEFAULT_PARALLAX 1.0
#define DEFAULT_HARDNESS 1.0
#define DEFAULT_SPECULARITY 1.0
#define DEFAULT_BLOOM 1.0
#define DEFAULT_LIGHT 300.0
#define DEFAULT_PATCH_SIZE 64

/**
 * @brief Materials define the rendering attributes of textures.
 */
typedef struct cm_material_s {

	/**
	 * @brief The material name, as it appears in the materials file.
	 */
	char name[MAX_QPATH];

	/**
	 * @brief The base name of this material without any diffusemap suffix.
	 */
	char basename[MAX_QPATH];

	/**
	 * @brief The diffusemap asset.
	 */
	cm_asset_t diffusemap;

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
	 * @brief The specularmap asset.
	 */
	cm_asset_t specularmap;

	/**
	 * @brief The tintmap asset.
	 */
	cm_asset_t tintmap;

	/**
	 * @brief Flags for the material.
	 */
	cm_stage_flags_t flags;

	/**
	 * @brief The material stages, if any.
	 */
	cm_stage_t *stages;

	/**
	 * @brief Contents flags applied to brush sides referencing this material.
	 */
	int32_t contents;

	/**
	 * @brief Surface flags applied to surfaces referencing this material.
	 */
	int32_t surface;

	/**
	 * @brief Light emission applied to surfaces referencing this material.
	 */
	float light;

	/**
	 * @brief The roughness factor to use for the normalmap.
	 */
	float roughness;

	/**
	 * @brief The hardness factor to use for the normalmap.
	 */
	float hardness;

	/**
	 * @brief The specular factor to use for the specularmap.
	 */
	float specularity;

	/**
	 * @brief The parallel factor to use for the normalmap.
	 */
	float parallax;

	/**
	 * @brief The bloom factor to apply to the diffusemap.
	 */
	float bloom;

	/**
	 * @brief The per-material patch size, for light emission.
	 */
	float patch_size;

	/**
	 * @brief The name for the footstep sounds to query on this surface
	 */
	char footsteps[MAX_QPATH];

	/**
	 * @brief Default tint colors
	 */
	vec4_t tintmap_defaults[TINT_TOTAL];

} cm_material_t;

cm_material_t *Cm_AllocMaterial(const char *name);
void Cm_FreeMaterial(cm_material_t *material);
void Cm_FreeMaterials(GList *materials);
ssize_t Cm_LoadMaterials(const char *path, GList **materials);
_Bool Cm_ResolveMaterial(cm_material_t *material, cm_asset_context_t context);
ssize_t Cm_WriteMaterials(const char *path, GList *materials);
void Cm_MaterialBasename(const char *in, char *out, size_t len);

#ifdef __CM_LOCAL_H__
#endif /* __CM_LOCAL_H__ */
