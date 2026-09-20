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

#include "r_types.h"

extern Cvar *r_ambient;
extern Cvar *r_ambientOcclusion;
extern Cvar *r_anisotropy;
extern Cvar *r_antialias;
extern Cvar *r_bloom;
extern Cvar *r_bloomIterations;
extern Cvar *r_bloomThreshold;
extern Cvar *r_caustics;
extern Cvar *r_framebufferScale;
extern Cvar *r_fullscreen;
extern Cvar *r_fullscreenWidth;
extern Cvar *r_fullscreenHeight;
extern Cvar *r_gpuDriver;
extern Cvar *r_hardness;
extern Cvar *r_lightingDistance;
extern Cvar *r_modulate;
extern Cvar *r_modulateMesh;
extern Cvar *r_saturation;
extern Cvar *r_parallax;
extern Cvar *r_parallaxShadow;
extern Cvar *r_roughness;
extern Cvar *r_screenshotFormat;
extern Cvar *r_shadows;
extern Cvar *r_shadowTileSize;
extern Cvar *r_specularity;
extern Cvar *r_swapInterval;
extern Cvar *r_windowHeight;
extern Cvar *r_windowWidth;

extern SDL_GPUSampleCount rSceneSamples;
SDL_GPUSampleCount R_SampleCount(void);

void R_Init(void);
void R_Shutdown(void);
void R_BeginFrame(void);
void R_InitView(RenderView *view);
void R_DrawViewDepth(RenderView *view);
void R_DrawMainView(RenderView *view);
void R_DrawPlayerModelView(RenderView *view);
void R_EndFrame(void);
void R_UpdateUniforms(const RenderView *view);

#if defined(__R_LOCAL_H__)

extern RenderViewStats *rStats;

/**
 * @brief Renderer driver information.
 */
typedef struct {

  /**
   * @brief Renderer name.
   */
  const char *renderer;

  /**
   * @brief GPU device name, e.g. `Apple M3 Max`.
   */
  const char *device;

  /**
   * @brief Vendor name.
   */
  const char *vendor;

  /**
   * @brief Driver version string.
   */
  const char *version;

  /**
   * @brief Maximum texture unit count.
   */
  int32_t maxTexunits;

  /**
   * @brief Maximum 2D texture size.
   */
  int32_t maxTextureSize;

  /**
   * @brief Maximum 3D texture size.
   */
  int32_t max3dTextureSize;

  /**
   * @brief Maximum uniform block size.
   */
  int32_t maxUniformBlockSize;
} RenderConfig;

extern RenderConfig rConfig;

/**
 * @brief Vec4-aligned voxel uniforms.
 */
typedef struct {

  /**
   * @brief Voxel grid minimum corner.
   */
  Vec4 mins;

  /**
   * @brief Voxel grid maximum corner.
   */
  Vec4 maxs;

  /**
   * @brief View origin in voxel-space coordinates.
   */
  Vec4 viewCoordinate;

  /**
   * @brief Voxel grid dimensions.
   */
  Vec4 size;
} RenderVoxels;

/**
 * @brief The uniforms block type.
 */
typedef struct {

  /**
   * @brief Vec4-aligned global uniform block.
   */
  struct RenderUniformBlock {

    /**
     * @brief The viewport (x, y, w, h) in device pixels.
     */
    Vec4i viewport;

    /**
     * @brief The 3D projection matrix.
     */
    Mat4 projection3D;

    /**
     * @brief The view matrix.
     */
    Mat4 view;

    /**
     * @brief The projection matrix for environment cubemaps.
     */
    Mat4 skyProjection;

    /**
     * @brief The projection matrix for point light shadow passes.
     */
    Mat4 lightProjection;

    /**
     * @brief The voxel uniforms.
     */
    RenderVoxels voxels;

    /**
     * @brief The depth range (near, far) in world units.
     */
    Vec2 depthRange;

    /**
     * @brief The view type, e.g. `VIEW_MAIN`.
     */
    int32_t viewType;

    /**
     * @brief The renderer time in milliseconds.
     */
    int32_t ticks;

    /**
     * @brief The ambient modulation, per channel.
     */
    Vec3 ambient;

    /**
     * @brief The light modulation scalar.
     */
    float modulate;

    /**
     * @brief The saturation scalar.
     */
    float saturation;

    /**
     * @brief The caustics intensity scalar.
     */
    float caustics;

    /**
     * @brief Ambient occlusion scalar.
     */
    float ambientOcclusion;

    /**
     * @brief Vertex-lighting distance threshold.
     */
    float lightingDistance;

    /**
     * @brief Non-zero when the in-game editor is active.
     */
    int editor;

    /**
     * @brief Non-zero when developer mode is enabled.
     */
    int developer;

    /**
     * @brief Pads the block to a multiple of vec4, as std140 requires.
     */
    Vec2 padding;
  } block;

} RenderUniforms;

/**
 * @brief Per-frame global uniforms.
 */
extern RenderUniforms rUniforms;
extern Cvar *r_alphaTest;
extern Cvar *r_cull;
extern Cvar *r_depthPass;
extern Cvar *r_drawBspBlocks;
extern Cvar *r_drawOcclusionQueries;
extern Cvar *r_drawBspNormals;
extern Cvar *r_drawBspVoxels;
extern Cvar *r_drawEntityBounds;
extern Cvar *r_drawLightBounds;
extern Cvar *r_drawMaterialStages;
extern Cvar *r_occlude;
extern Cvar *r_portals;

#endif
