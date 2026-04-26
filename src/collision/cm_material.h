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
  ASSET_CONTEXT_NONE,
  ASSET_CONTEXT_TEXTURES,
  ASSET_CONTEXT_MODELS,
  ASSET_CONTEXT_PLAYERS,
  ASSET_CONTEXT_SPRITES,
} cm_asset_context_t;

/**
 * @brief A named asset with a resolved file path.
 */
typedef struct {

  /**
   * @brief The asset name as referenced in material files.
   */
  char name[MAX_QPATH];

  /**
   * @brief The resolved filesystem path, or empty if unresolved.
   */
  char path[MAX_QPATH];
} cm_asset_t;

/**
 * @brief Blend function source and destination factors.
 */
typedef struct {

  /**
   * @brief OpenGL blend function constants (GL_SRC_ALPHA, etc.).
   */
  uint32_t src, dest;
} cm_stage_blend_t;

/**
 * @brief Pulse animation parameters.
 */
typedef struct {

  /**
   * @brief Pulse frequency in Hz.
   */
  float hz;
} cm_stage_pulse_t;

/**
 * @brief Stretch animation parameters.
 */
typedef struct {

  /**
   * @brief Stretch frequency in Hz.
   */
  float hz;

  /**
   * @brief Stretch amplitude.
   */
  float amplitude;
} cm_stage_stretch_t;

/**
 * @brief Rotation animation parameters.
 */
typedef struct {

  /**
   * @brief Rotation frequency in Hz.
   */
  float hz;
} cm_stage_rotate_t;

/**
 * @brief Texture scrolling parameters.
 */
typedef struct {

  /**
   * @brief Scroll speed along S and T axes.
   */
  float s, t;
} cm_stage_scroll_t;

/**
 * @brief Texture scale parameters.
 */
typedef struct {

  /**
   * @brief Scale factors along S and T axes.
   */
  float s, t;
} cm_stage_scale_t;

/**
 * @brief Terrain blending parameters.
 */
typedef struct {

  /**
   * @brief World-space Z range for blending.
   */
  float floor, ceil;
} cm_stage_terrain_t;

/**
 * @brief Dirtmap effect parameters.
 */
typedef struct {

  /**
   * @brief Dirtmap blend intensity.
   */
  float intensity;
} cm_stage_dirtmap_t;

/**
 * @brief Warp (liquid) animation parameters.
 */
typedef struct {

  /**
   * @brief Warp frequency in Hz.
   */
  float hz;

  /**
   * @brief Warp amplitude.
   */
  float amplitude;
} cm_stage_warp_t;

/**
 * @brief Stage lighting parameters.
 */
typedef struct {

  /**
   * @brief Lighting intensity scalar.
   */
  float intensity;
} cm_stage_lighting_t;

/**
 * @brief Shell effect parameters.
 */
typedef struct {

  /**
   * @brief Shell expansion radius.
   */
  float radius;
} cm_stage_shell_t;

/**
 * @brief Frame animation parameters.
 */
typedef struct {

  /**
   * @brief Total number of animation frames.
   */
  int32_t num_frames;

  /**
   * @brief Resolved per-frame asset array.
   */
  cm_asset_t *frames;

  /**
   * @brief Playback rate in frames per second.
   */
  float fps;
} cm_stage_animation_t;

typedef enum {
  TINT_R,
  TINT_G,
  TINT_B,

  TINT_TOTAL
} cm_stage_tint_src_t;

/**
 * @brief Stage flags indicate what assets and effects a material or stage may include.
 */
typedef enum {
  STAGE_NONE      = (0),
  STAGE_TEXTURE   = (1 << 0),
  STAGE_BLEND     = (1 << 1),
  STAGE_COLOR     = (1 << 2),
  STAGE_PULSE     = (1 << 3),
  STAGE_STRETCH   = (1 << 4),
  STAGE_ROTATE    = (1 << 5),
  STAGE_SCROLL_S  = (1 << 6),
  STAGE_SCROLL_T  = (1 << 7),
  STAGE_SCALE_S   = (1 << 8),
  STAGE_SCALE_T   = (1 << 9),
  STAGE_ANIMATION = (1 << 10),
  STAGE_TERRAIN   = (1 << 11),
  STAGE_DIRTMAP   = (1 << 12),
  STAGE_ENVMAP    = (1 << 13),
  STAGE_WARP      = (1 << 14),
  STAGE_FLARE     = (1 << 15),
  STAGE_LIGHTING  = (1 << 16),
  STAGE_SHELL     = (1 << 18),

  STAGE_DRAW      = (1 << 30),

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
   * @brief The stage pulse parameters.
   */
  cm_stage_pulse_t pulse;

  /**
   * @brief The stage stretch parameters.
   */
  cm_stage_stretch_t stretch;

  /**
   * @brief The stage rotate parameters.
   */
  cm_stage_rotate_t rotate;

  /**
   * @brief The stage scroll parameters.
   */
  cm_stage_scroll_t scroll;

  /**
   * @brief The stage scale parameters.
   */
  cm_stage_scale_t scale;

  /**
   * @brief The stage animation parameters.
   */
  cm_stage_animation_t animation;

  /**
   * @brief The stage terrain parameters.
   */
  cm_stage_terrain_t terrain;

  /**
   * @brief The stage dirtmap parameters.
   */
  cm_stage_dirtmap_t dirtmap;

  /**
   * @brief The stage warp parameters.
   */
  cm_stage_warp_t warp;

  /**
   * @brief The stage lighting parameters.
   */
  cm_stage_lighting_t lighting;

  /**
   * @brief The stage shell parameters.
   */
  cm_stage_shell_t shell;

  /**
   * @brief The next stage, or NULL.
   */
  struct cm_stage_s *next;
} cm_stage_t;

#define MAX_FOOTSTEP_SAMPLES 6

/**
 * @brief Materials may optionally reference footstep samples.
 */
typedef struct {

  /**
   * @brief The footstep name, e.g. "metal3".
   */
  char name[MAX_QPATH];

  /**
   * @brief The footstep sample assets.
   */
  cm_asset_t samples[MAX_FOOTSTEP_SAMPLES];

  /**
   * @brief The number of footstep sample assets.
   */
  int32_t num_samples;
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

  /**
   * @brief The material file path defining this material, if any.
   */
  char path[MAX_QPATH];

  /**
   * @brief The material name, as it appears in the materials file.
   */
  char name[MAX_QPATH];

  /**
   * @brief The base name of this material without any diffusemap suffix.
   */
  char basename[MAX_QPATH];

  /**
   * @brief The asset context for this material (e.g. textures, models, players).
   */
  cm_asset_context_t context;

  /**
   * @brief The diffusemap asset.
   */
  cm_asset_t diffusemap;

  /**
   * @brief The normalmap asset.
   */
  cm_asset_t normalmap;

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
  cm_stage_flags_t stage_flags;

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
   * @brief The alpha test threshold.
   */
  float alpha_test;

  /**
   * @brief The roughness factor to use for the normalmap.
   */
  float roughness;

  /**
   * @brief The hardness factor to use for the specularmap.
   */
  float hardness;

  /**
   * @brief The specular factor to use for the specularmap.
   */
  float specularity;

  /**
   * @brief The parallax factor for the normalmap heightmap.
   */
  float parallax;

  /**
   * @brief The self-shadow factor for the normalmap heightmap.
   */
  float shadow;

  /**
   * @brief The footsteps to play when the player walks on this material.
   */
  cm_footsteps_t footsteps;

  /**
   * @brief Default tint colors
   */
  vec4_t tintmap_defaults[TINT_TOTAL];

  /**
   * @brief True if this material has been modified and needs to be saved.
   */
  bool dirty;
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
