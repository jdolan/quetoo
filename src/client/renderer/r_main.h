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

extern cvar_t *r_ambient;
extern cvar_t *r_ambient_occlusion;
extern cvar_t *r_anisotropy;
extern cvar_t *r_antialias;
extern cvar_t *r_bloom;
extern cvar_t *r_bloom_iterations;
extern cvar_t *r_bloom_threshold;
extern cvar_t *r_caustics;
extern cvar_t *r_draw_scale;
extern cvar_t *r_framebuffer_scale;
extern cvar_t *r_fullscreen;
extern cvar_t *r_fullscreen_width;
extern cvar_t *r_fullscreen_height;
extern cvar_t *r_gpu_driver;
extern cvar_t *r_hardness;
extern cvar_t *r_lighting_distance;
extern cvar_t *r_modulate;
extern cvar_t *r_saturation;
extern cvar_t *r_parallax;
extern cvar_t *r_parallax_shadow;
extern cvar_t *r_roughness;
extern cvar_t *r_screenshot_format;
extern cvar_t *r_shadows;
extern cvar_t *r_shadow_tile_size;
extern cvar_t *r_shadow_distance;
extern cvar_t *r_specularity;
extern cvar_t *r_swap_interval;
extern cvar_t *r_window_height;
extern cvar_t *r_window_width;

extern cvar_t *r_draw_stats;

extern r_stats_t r_stats;

extern SDL_GPUSampleCount r_scene_samples;
SDL_GPUSampleCount R_SampleCount(void);

void R_Init(void);
void R_Shutdown(void);
void R_BeginFrame(void);
void R_InitView(r_view_t *view);
void R_DrawViewDepth(r_view_t *view);
void R_DrawMainView(r_view_t *view);
void R_DrawPlayerModelView(r_view_t *view);
void R_EndFrame(void);
void R_UpdateUniforms(const r_view_t *view);

#if defined(__R_LOCAL_H__)

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
  int32_t max_texunits;

  /**
   * @brief Maximum 2D texture size.
   */
  int32_t max_texture_size;

  /**
   * @brief Maximum 3D texture size.
   */
  int32_t max_3d_texture_size;

  /**
   * @brief Maximum uniform block size.
   */
  int32_t max_uniform_block_size;
} r_config_t;

extern r_config_t r_config;

/**
 * @brief Vec4-aligned voxel uniforms.
 */
typedef struct {

  /**
   * @brief Voxel grid minimum corner.
   */
  vec4_t mins;

  /**
   * @brief Voxel grid maximum corner.
   */
  vec4_t maxs;

  /**
   * @brief View origin in voxel-space coordinates.
   */
  vec4_t view_coordinate;

  /**
   * @brief Voxel grid dimensions.
   */
  vec4_t size;
} r_voxels_t;

/**
 * @brief The uniforms block type.
 */
typedef struct {

  /**
   * @brief Vec4-aligned global uniform block.
   */
  struct r_uniform_block_t {

    /**
     * @brief The viewport (x, y, w, h) in device pixels.
     */
    vec4i_t viewport;

    /**
     * @brief The 3D projection matrix.
     */
    mat4_t projection3D;

    /**
     * @brief The view matrix.
     */
    mat4_t view;

    /**
     * @brief The projection matrix for environment cubemaps.
     */
    mat4_t sky_projection;

    /**
     * @brief The projection matrix for point light shadow passes.
     */
    mat4_t light_projection;

    /**
     * @brief The voxel uniforms.
     */
    r_voxels_t voxels;

    /**
     * @brief The depth range (near, far) in world units.
     */
    vec2_t depth_range;

    /**
     * @brief The view type, e.g. `VIEW_MAIN`.
     */
    int32_t view_type;

    /**
     * @brief The renderer time in milliseconds.
     */
    int32_t ticks;

    /**
     * @brief The ambient scalar.
     */
    float ambient;

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
    float ambient_occlusion;

    /**
     * @brief Vertex-lighting distance threshold.
     */
    float lighting_distance;

    /**
     * @brief Non-zero when the in-game editor is active.
     */
    int editor;

    /**
     * @brief Non-zero when developer mode is enabled.
     */
    int developer;
  } block;

} r_uniforms_t;

/**
 * @brief Per-frame global uniforms.
 */
extern r_uniforms_t r_uniforms;
extern cvar_t *r_alpha_test;
extern cvar_t *r_cull;
extern cvar_t *r_depth_pass;
extern cvar_t *r_draw_bsp_blocks;
extern cvar_t *r_draw_occlusion_queries;
extern cvar_t *r_draw_bsp_normals;
extern cvar_t *r_draw_bsp_voxels;
extern cvar_t *r_draw_entity_bounds;
extern cvar_t *r_draw_light_bounds;
extern cvar_t *r_draw_material_stages;
extern cvar_t *r_occlude;

#endif
