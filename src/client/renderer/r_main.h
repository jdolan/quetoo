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

#include "r_types.h"

extern Cvar *r_ambient;
extern Cvar *r_ambient_occlusion;
extern Cvar *r_anisotropy;
extern Cvar *r_antialias;
extern Cvar *r_bloom;
extern Cvar *r_bloom_iterations;
extern Cvar *r_bloom_threshold;
extern Cvar *r_caustics;
extern Cvar *r_framebuffer_scale;
extern Cvar *r_fullscreen;
extern Cvar *r_fullscreen_width;
extern Cvar *r_fullscreen_height;
extern Cvar *r_gpu_driver;
extern Cvar *r_hardness;
extern Cvar *r_lighting_distance;
extern Cvar *r_modulate;
extern Cvar *r_modulate_mesh;
extern Cvar *r_saturation;
extern Cvar *r_parallax;
extern Cvar *r_parallax_shadow;
extern Cvar *r_roughness;
extern Cvar *r_screenshot_format;
extern Cvar *r_shadows;
extern Cvar *r_shadow_tile_size;
extern Cvar *r_specularity;
extern Cvar *r_swap_interval;
extern Cvar *r_window_height;
extern Cvar *r_window_width;

extern SDL_GPUSampleCount r_scene_samples;
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

extern RenderViewStats *r_stats;

/**
 * @brief Renderer driver information.
 */
typedef struct {

  /**
   * @brief Renderer name.
   */
  const char *renderer;

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

extern RenderConfig r_config;

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
extern RenderUniforms r_uniforms;
extern Cvar *r_alpha_test;
extern Cvar *r_cull;
extern Cvar *r_depth_pass;
extern Cvar *r_draw_bsp_blocks;
extern Cvar *r_draw_occlusion_queries;
extern Cvar *r_draw_bsp_normals;
extern Cvar *r_draw_bsp_voxels;
extern Cvar *r_draw_entity_bounds;
extern Cvar *r_draw_light_bounds;
extern Cvar *r_draw_material_stages;
extern Cvar *r_occlude;
extern Cvar *r_portals;

#endif
