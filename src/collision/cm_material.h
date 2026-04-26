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

#include "shared/shared.h"

/**
 * @brief Asset contexts are paths (and conventions) for locating and loading material assets.
 */
typedef enum {
  ASSET_CONTEXT_NONE,     ///< No context; paths are used as-is.
  ASSET_CONTEXT_TEXTURES, ///< Texture assets; paths rooted at textures/.
  ASSET_CONTEXT_MODELS,   ///< Model assets; paths rooted at models/.
  ASSET_CONTEXT_PLAYERS,  ///< Player skin assets; paths rooted at players/.
  ASSET_CONTEXT_SPRITES,  ///< Sprite assets; paths rooted at sprites/.
} cm_asset_context_t;

/**
 * @brief A named asset with a resolved file path.
 */
typedef struct {
  char name[MAX_QPATH]; ///< The asset name as referenced in material files.
  char path[MAX_QPATH]; ///< The resolved filesystem path, or empty if unresolved.
} cm_asset_t;

/**
 * @brief Blend function source and destination factors.
 */
typedef struct {
  uint32_t src, dest; ///< OpenGL blend function constants (GL_SRC_ALPHA, etc.).
} cm_stage_blend_t;

/**
 * @brief Pulse animation parameters.
 */
typedef struct {
  float hz; ///< Pulse frequency in Hz.
} cm_stage_pulse_t;

/**
 * @brief Stretch animation parameters.
 */
typedef struct {
  float hz;        ///< Stretch frequency in Hz.
  float amplitude; ///< Stretch amplitude.
} cm_stage_stretch_t;

/**
 * @brief Rotation animation parameters.
 */
typedef struct {
  float hz; ///< Rotation frequency in Hz.
} cm_stage_rotate_t;

/**
 * @brief Texture scrolling parameters.
 */
typedef struct {
  float s, t; ///< Scroll speed along S and T axes.
} cm_stage_scroll_t;

/**
 * @brief Texture scale parameters.
 */
typedef struct {
  float s, t; ///< Scale factors along S and T axes.
} cm_stage_scale_t;

/**
 * @brief Terrain blending parameters.
 */
typedef struct {
  float floor, ceil; ///< World-space Z range for blending.
} cm_stage_terrain_t;

/**
 * @brief Dirtmap effect parameters.
 */
typedef struct {
  float intensity; ///< Dirtmap blend intensity.
} cm_stage_dirtmap_t;

/**
 * @brief Warp (liquid) animation parameters.
 */
typedef struct {
  float hz;        ///< Warp frequency in Hz.
  float amplitude; ///< Warp amplitude.
} cm_stage_warp_t;

/**
 * @brief Stage lighting parameters.
 */
typedef struct {
  float intensity; ///< Lighting intensity scalar.
} cm_stage_lighting_t;

/**
 * @brief Shell effect parameters.
 */
typedef struct {
  float radius; ///< Shell expansion radius.
} cm_stage_shell_t;

/**
 * @brief Frame animation parameters.
 */
typedef struct {
  int32_t num_frames; ///< Total number of animation frames.
  cm_asset_t *frames; ///< Resolved per-frame asset array.
  float fps;          ///< Playback rate in frames per second.
} cm_stage_animation_t;

typedef enum {
  TINT_R,    ///< Red channel tint.
  TINT_G,    ///< Green channel tint.
  TINT_B,    ///< Blue channel tint.

  TINT_TOTAL ///< Total number of tint channels (must not exceed 3).
} cm_stage_tint_src_t;

/**
 * @brief Stage flags indicate what assets and effects a material or stage may include.
 */
typedef enum {
  STAGE_NONE      = (0),       ///< No stage flags set.
  STAGE_TEXTURE   = (1 << 0),  ///< Stage uses a texture asset.
  STAGE_BLEND     = (1 << 1),  ///< Stage uses alpha blending.
  STAGE_COLOR     = (1 << 2),  ///< Stage applies a color tint.
  STAGE_PULSE     = (1 << 3),  ///< Stage uses pulse animation.
  STAGE_STRETCH   = (1 << 4),  ///< Stage uses stretch animation.
  STAGE_ROTATE    = (1 << 5),  ///< Stage uses rotation animation.
  STAGE_SCROLL_S  = (1 << 6),  ///< Stage scrolls along the S axis.
  STAGE_SCROLL_T  = (1 << 7),  ///< Stage scrolls along the T axis.
  STAGE_SCALE_S   = (1 << 8),  ///< Stage scales along the S axis.
  STAGE_SCALE_T   = (1 << 9),  ///< Stage scales along the T axis.
  STAGE_ANIMATION = (1 << 10), ///< Stage uses frame animation.
  STAGE_TERRAIN   = (1 << 11), ///< Stage uses terrain height blending.
  STAGE_DIRTMAP   = (1 << 12), ///< Stage uses a dirtmap effect.
  STAGE_ENVMAP    = (1 << 13), ///< Stage uses environment mapping.
  STAGE_WARP      = (1 << 14), ///< Stage uses warp (liquid) animation.
  STAGE_FLARE     = (1 << 15), ///< Stage renders a lens flare sprite.
  STAGE_LIGHTING  = (1 << 16), ///< Stage applies a lighting intensity modifier.
  STAGE_SHELL     = (1 << 18), ///< Stage renders an expanding shell effect.

  STAGE_DRAW      = (1 << 30), ///< Internal flag: stage has been submitted for drawing.

} cm_stage_flags_t;

/**
 * @brief Stages are ordered layers of visual effects rendered on top of their material.
 */
typedef struct cm_stage_s {
  cm_stage_flags_t flags;         ///< The stage flags.
  cm_asset_t asset;               ///< The stage asset.
  cm_stage_blend_t blend;         ///< The stage alpha blend function.
  color_t color;                  ///< The stage color.
  cm_stage_pulse_t pulse;         ///< The stage pulse parameters.
  cm_stage_stretch_t stretch;     ///< The stage stretch parameters.
  cm_stage_rotate_t rotate;       ///< The stage rotate parameters.
  cm_stage_scroll_t scroll;       ///< The stage scroll parameters.
  cm_stage_scale_t scale;         ///< The stage scale parameters.
  cm_stage_animation_t animation; ///< The stage animation parameters.
  cm_stage_terrain_t terrain;     ///< The stage terrain parameters.
  cm_stage_dirtmap_t dirtmap;     ///< The stage dirtmap parameters.
  cm_stage_warp_t warp;           ///< The stage warp parameters.
  cm_stage_lighting_t lighting;   ///< The stage lighting parameters.
  cm_stage_shell_t shell;         ///< The stage shell parameters.
  struct cm_stage_s *next;        ///< The next stage, or NULL.
} cm_stage_t;

#define MAX_FOOTSTEP_SAMPLES 6

/**
 * @brief Materials may optionally reference footstep samples.
 */
typedef struct {
  char name[MAX_QPATH];                     ///< The footstep name, e.g. "metal3".
  cm_asset_t samples[MAX_FOOTSTEP_SAMPLES]; ///< The footstep sample assets.
  int32_t num_samples;                      ///< The number of footstep sample assets.
} cm_footsteps_t;

#define MATERIAL_ROUGHNESS 1.f
#define MATERIAL_HARDNESS 1.f
#define MATERIAL_SPECULARITY 1.f
#define MATERIAL_PARALLAX 1.f
#define MATERIAL_SHADOW 1.f
#define MATERIAL_ALPHA_TEST 0.f

/**
 * @brief Materials define the rendering attributes of textures.
 */
typedef struct cm_material_s {
  char path[MAX_QPATH];                ///< The material file path defining this material, if any.
  char name[MAX_QPATH];                ///< The material name, as it appears in the materials file.
  char basename[MAX_QPATH];            ///< The base name of this material without any diffusemap suffix.
  cm_asset_context_t context;          ///< The asset context for this material (e.g. textures, models, players).
  cm_asset_t diffusemap;               ///< The diffusemap asset.
  cm_asset_t normalmap;                ///< The normalmap asset.
  cm_asset_t specularmap;              ///< The specularmap asset.
  cm_asset_t tintmap;                  ///< The tintmap asset.
  cm_stage_flags_t stage_flags;        ///< Flags for the material.
  cm_stage_t *stages;                  ///< The material stages, if any.
  int32_t contents;                    ///< Contents flags applied to brush sides referencing this material.
  int32_t surface;                     ///< Surface flags applied to surfaces referencing this material.
  float alpha_test;                    ///< The alpha test threshold.
  float roughness;                     ///< The roughness factor to use for the normalmap.
  float hardness;                      ///< The hardness factor to use for the specularmap.
  float specularity;                   ///< The specular factor to use for the specularmap.
  float parallax;                      ///< The parallax factor for the normalmap heightmap.
  float shadow;                        ///< The self-shadow factor for the normalmap heightmap.
  cm_footsteps_t footsteps;            ///< The footsteps to play when the player walks on this material.
  vec4_t tintmap_defaults[TINT_TOTAL]; ///< Default tint colors
  bool dirty;                          ///< True if this material has been modified and needs to be saved.
} cm_material_t;

/**
 * @brief Loads the material with the given name in the given asset context.
 * @return The loaded material, or NULL on failure.
 */
cm_material_t *Cm_LoadMaterial(const char *name, cm_asset_context_t context);

/**
 * @brief Frees the material and all its stages.
 */
void Cm_FreeMaterial(cm_material_t *material);

/**
 * @brief Resolves all asset paths referenced by the material.
 * @return true if the diffusemap was resolved successfully.
 */
bool Cm_ResolveMaterial(cm_material_t *material);

/**
 * @brief Serializes the material to its file path on disk.
 * @return true on success.
 */
bool Cm_SaveMaterial(const cm_material_t *material);

/**
 * @brief Extracts the base name from a material path, stripping any diffusemap suffix.
 */
void Cm_MaterialBasename(const char *in, char *out, size_t len);

/**
 * @brief Computes the expected .mat file path for the given material name and context.
 */
void Cm_MaterialPath(const char *name, char *path, size_t len, cm_asset_context_t context);

#if defined(__CM_LOCAL_H__)
#endif /* __CM_LOCAL_H__ */
