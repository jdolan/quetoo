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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#pragma once

#include "common/asset.h"

/**
 * @brief Blend factors for material stage blending. Renderer-agnostic; the
 * renderer maps these to its backend equivalents (e.g. SDL_GPUBlendFactor).
 * @remarks BLEND_INVALID is deliberately 0, matching the zero-initialized
 * (never parsed a `blend` keyword) state of a fresh CmStage. This lets
 * "ensure appropriate blend function defaults" (Cm_ParseStage) distinguish
 * "never set" from an explicit, meaningful `BLEND_ZERO` factor -- e.g. a
 * stage that explicitly sets `blend one zero` for opaque overwrite rendering.
 */
typedef enum {
  BLEND_INVALID,
  BLEND_ZERO,
  BLEND_ONE,
  BLEND_SRC_COLOR,
  BLEND_ONE_MINUS_SRC_COLOR,
  BLEND_SRC_ALPHA,
  BLEND_ONE_MINUS_SRC_ALPHA,
  BLEND_DST_COLOR,
} CmBlend;

/**
 * @brief Blend function source and destination factors.
 */
typedef struct {

  /**
   * @brief The blend factors (`CmBlend`).
   */
  CmBlend src, dest;
} CmStageBlend;

/**
 * @brief Pulse animation parameters.
 */
typedef struct {

  /**
   * @brief Pulse frequency in Hz.
   */
  float hz;
} CmStagePulse;

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
} CmStageStretch;

/**
 * @brief Rotation animation parameters.
 */
typedef struct {

  /**
   * @brief Rotation frequency in Hz.
   */
  float hz;
} CmStageRotate;

/**
 * @brief Texture scrolling parameters.
 */
typedef struct {

  /**
   * @brief Scroll speed along S and T axes.
   */
  float s, t;
} CmStageScroll;

/**
 * @brief Texture scale parameters.
 */
typedef struct {

  /**
   * @brief Scale factors along S and T axes.
   */
  float s, t;
} CmStageScale;

/**
 * @brief Terrain blending parameters.
 */
typedef struct {

  /**
   * @brief World-space Z range for blending.
   */
  float floor, ceil;
} CmStageTerrain;

/**
 * @brief Dirtmap effect parameters.
 */
typedef struct {

  /**
   * @brief Dirtmap blend intensity.
   */
  float intensity;
} CmStageDirtmap;

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
} CmStageWarp;

/**
 * @brief Stage lighting parameters.
 */
typedef struct {

  /**
   * @brief Lighting intensity scalar.
   */
  float intensity;

  /**
   * @brief Lighting mode.
   */
  enum {
    STAGE_LIGHTING_MODE_MATERIAL,
    STAGE_LIGHTING_MODE_FLAT
  } mode;
} CmStageLighting;

/**
 * @brief Shell effect parameters.
 */
typedef struct {

  /**
   * @brief Shell expansion radius.
   */
  float radius;
} CmStageShell;

/**
 * @brief The default radius of a stage light.
 */
#define STAGE_LIGHT_RADIUS 300.f

/**
 * @brief The default intensity of a stage light.
 */
#define STAGE_LIGHT_INTENSITY 1.f

/**
 * @brief Stage light parameters. A stage with these emits light from the faces that use it.
 */
typedef struct {

  /**
   * @brief The light radius, compiled into the BSP. This is the maximum radius of the light.
   */
  float radius;

  /**
   * @brief The light color, compiled into the BSP. If zero, the color is resolved from the
   * brightest pixels of the stage texture.
   */
  Vec3 color;

  /**
   * @brief The light intensity, read at runtime and modulated by the stage pulse, if any.
   */
  float intensity;
} CmStageLight;

/**
 * @brief Frame animation parameters.
 */
typedef struct {

  /**
   * @brief Total number of animation frames.
   */
  int32_t numFrames;

  /**
   * @brief Resolved per-frame asset array.
   */
  Asset *frames;

  /**
   * @brief Playback rate in frames per second.
   */
  float fps;

  /**
   * @brief Optional random time offset in seconds.
   */
  float drift;
} CmStageAnimation;

typedef enum {
  TINT_R,
  TINT_G,
  TINT_B,

  TINT_TOTAL
} CmStageTintSrc;

/**
 * @brief Stage flags indicate what assets and effects a material or stage may include.
 */
typedef enum {
  STAGE_NONE          = (0),
  STAGE_TEXTURE       = (1 << 0),
  STAGE_BLEND         = (1 << 1),
  STAGE_COLOR         = (1 << 2),

  STAGE_SCROLL_S      = (1 << 3),
  STAGE_SCROLL_T      = (1 << 4),
  STAGE_SCALE_S       = (1 << 5),
  STAGE_SCALE_T       = (1 << 6),
  STAGE_ROTATE        = (1 << 7),
  STAGE_STRETCH       = (1 << 8),
  STAGE_PULSE         = (1 << 9),

  STAGE_ANIMATION     = (1 << 10),
  STAGE_ANIM_LERP     = (1 << 11),

  STAGE_TERRAIN       = (1 << 12),
  STAGE_DIRTMAP       = (1 << 13),
  STAGE_ENVMAP        = (1 << 14),
  STAGE_WARP          = (1 << 15),

  STAGE_LIGHTING      = (1 << 16),
  STAGE_LIGHTING_FLAT = (1 << 17),
  STAGE_EMISSIVE      = (1 << 18),

  STAGE_FLARE         = (1 << 19),
  STAGE_SHELL         = (1 << 20),

  /**
   * @brief A stage that draws the portal its face shows, in place of a texture.
   */
  STAGE_PORTAL        = (1 << 21),

  /**
   * @brief A stage that draws the reflection its face shows, in place of a texture.
   */
  STAGE_REFLECTION    = (1 << 22),

  /**
   * @brief A stage that emits light from the faces that use it.
   */
  STAGE_LIGHT         = (1 << 23),

  STAGE_DRAW          = (1 << 30),
} CmStageFlags;

/**
 * @brief The stage keywords that draw a subview, rather than an asset of their own.
 */
#define STAGE_MASK_SUBVIEW (STAGE_PORTAL | STAGE_REFLECTION)

/**
 * @brief Stages are ordered layers of visual effects rendered on top of their material.
 */
typedef struct CmStage {

  /**
   * @brief The stage flags.
   */
  CmStageFlags flags;

  /**
   * @brief The stage asset.
   */
  Asset asset;

  /**
   * @brief The stage alpha blend function.
   */
  CmStageBlend blend;

  /**
   * @brief The stage color.
   */
  Color color;

  /**
   * @brief The stage pulse parameters.
   */
  CmStagePulse pulse;

  /**
   * @brief The stage stretch parameters.
   */
  CmStageStretch stretch;

  /**
   * @brief The stage rotate parameters.
   */
  CmStageRotate rotate;

  /**
   * @brief The stage scroll parameters.
   */
  CmStageScroll scroll;

  /**
   * @brief The stage scale parameters.
   */
  CmStageScale scale;

  /**
   * @brief The stage animation parameters.
   */
  CmStageAnimation animation;

  /**
   * @brief The stage terrain parameters.
   */
  CmStageTerrain terrain;

  /**
   * @brief The stage dirtmap parameters.
   */
  CmStageDirtmap dirtmap;

  /**
   * @brief The stage warp parameters.
   */
  CmStageWarp warp;

  /**
   * @brief The stage lighting parameters.
   */
  CmStageLighting lighting;

  /**
   * @brief The stage shell parameters.
   */
  CmStageShell shell;

  /**
   * @brief The stage emissive intensity [0, 1]. Adds unlit stage color to output.
   */
  float emissive;

  /**
   * @brief The stage light parameters.
   */
  CmStageLight light;

  /**
   * @brief The next stage, or `NULL`.
   */
  struct CmStage *next;
} CmStage;

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
  Asset samples[MAX_FOOTSTEP_SAMPLES];

  /**
   * @brief The number of footstep sample assets.
   */
  int32_t numSamples;
} CmFootsteps;

#define MATERIAL_ROUGHNESS 1.f
#define MATERIAL_HARDNESS 1.f
#define MATERIAL_SPECULARITY 1.f
#define MATERIAL_PARALLAX 1.f
#define MATERIAL_SHADOW 1.f
#define MATERIAL_ALPHA_TEST 0.f

/**
 * @brief Materials define the rendering attributes of textures.
 */
typedef struct CmMaterial {

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
  AssetContext context;

  /**
   * @brief The diffusemap asset.
   */
  Asset diffusemap;

  /**
   * @brief The normalmap asset.
   */
  Asset normalmap;

  /**
   * @brief The specularmap asset.
   */
  Asset specularmap;

  /**
   * @brief The tintmap asset.
   */
  Asset tintmap;

  /**
   * @brief Flags for the material.
   */
  CmStageFlags stageFlags;

  /**
   * @brief The material stages, if any.
   */
  CmStage *stages;

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
  float alphaTest;

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
  CmFootsteps footsteps;

  /**
   * @brief Default tint colors
   */
  Vec4 tintmapDefaults[TINT_TOTAL];

  /**
   * @brief True if this material has been modified and needs to be saved.
   */
  bool dirty;
} CmMaterial;

/**
 * @brief Loads the material with the given name in the given asset context.
 * @return The loaded material, or `NULL` on failure.
 */
CmMaterial *Cm_LoadMaterial(const char *name, AssetContext context);

/**
 * @brief Frees the material and all its stages.
 */
void Cm_FreeMaterial(CmMaterial *material);

/**
 * @brief Resolves all asset paths referenced by the material.
 * @return true if the diffusemap was resolved successfully.
 */
bool Cm_ResolveMaterial(CmMaterial *material);

/**
 * @brief Applies the implied flags and defaults of a stage after its keywords are set.
 * @remarks The parser calls this at the end of each stage. The editor MUST call it after it
 * changes the flags of a stage.
 */
void Cm_FinalizeStage(CmStage *stage);

/**
 * @brief Recomputes the aggregate stage flags of the material from its stages.
 */
void Cm_ResolveStageFlags(CmMaterial *material);

/**
 * @brief Returns the first `STAGE_LIGHT` stage of the material, or `NULL`.
 */
CmStage *Cm_MaterialLightStage(const CmMaterial *material);

/**
 * @brief Finalizes the stage after an edit, and resolves its assets and the material stage flags.
 * @remarks The editor MUST call this after it changes the flags or the asset of a stage. It marks
 * the material dirty.
 * @return True if the stage assets were resolved.
 */
bool Cm_ResolveStage(CmMaterial *material, CmStage *stage);

/**
 * @brief Appends a new stage that draws the material diffusemap, and marks the material dirty.
 * @return The new stage.
 */
CmStage *Cm_AddStage(CmMaterial *material);

/**
 * @brief Removes and frees the stage, and marks the material dirty.
 * @remarks Any pointer to the stage, such as a `RenderStage`, is invalid after this. The renderer
 * stages of the material MUST be reloaded.
 */
void Cm_RemoveStage(CmMaterial *material, CmStage *stage);

/**
 * @brief Serializes the material to its file path on disk.
 * @return true on success.
 */
bool Cm_SaveMaterial(const CmMaterial *material);

/**
 * @brief Extracts the base name from a material path, stripping any diffusemap suffix.
 */
void Cm_MaterialBasename(const char *in, char *out, size_t len);

/**
 * @brief Computes the expected .mat file path for the given material name and context.
 */
void Cm_MaterialPath(const char *name, char *path, size_t len, AssetContext context);

#if defined(__CM_LOCAL_H__)
#endif
