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
extern cvar_t *r_anisotropy;
extern cvar_t *r_bloom;
extern cvar_t *r_bloom_iterations;
extern cvar_t *r_bloom_threshold;
extern cvar_t *r_caustics;
extern cvar_t *r_draw_scale;
extern cvar_t *r_finish;
extern cvar_t *r_framebuffer_scale;
extern cvar_t *r_fullscreen;
extern cvar_t *r_hardness;
extern cvar_t *r_lighting_distance;
extern cvar_t *r_modulate;
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
 * @brief OpenGL driver information.
 */
typedef struct {
  const char *renderer; ///< The renderer string reported by the GL driver.
  const char *vendor; ///< The vendor string reported by the GL driver.
  const char *version; ///< The version string reported by the GL driver.

  GLint max_texunits; ///< The maximum number of simultaneous texture units.
  GLint max_texture_size; ///< The maximum 2D texture dimension in texels.
  GLint max_3d_texture_size; ///< The maximum 3D texture dimension in texels.
  GLint max_uniform_block_size; ///< The maximum uniform block size in bytes.
} r_config_t;

extern r_config_t r_config;

/**
 * @brief The voxel uniform struct.
 * @remarks This struct is vec4 aligned.
 */
typedef struct {
  vec4_t mins; ///< The voxel grid minimum corner in world space (xyz, w unused).
  vec4_t maxs; ///< The voxel grid maximum corner in world space (xyz, w unused).
  vec4_t view_coordinate; ///< The view origin expressed in voxel-space coordinates (xyz, w unused).
  vec4_t size; ///< The voxel grid dimensions in voxels (xyz, w unused).
} r_voxels_t;

/**
 * @brief The uniforms block type.
 */
typedef struct {
  GLuint buffer; ///< The uniform buffer object name.

  /**
   * @brief The uniform block struct.
   * @remarks This struct is vec4 aligned.
   */
  struct r_uniform_block_t {
    vec4i_t viewport; ///< The viewport (x, y, w, h) in device pixels.
    mat4_t projection3D; ///< The 3D projection matrix.
    mat4_t view; ///< The view matrix.
    mat4_t sky_projection; ///< The projection matrix for environment cubemaps.
    mat4_t light_projection; ///< The projection matrix for point light shadow passes.
    r_voxels_t voxels; ///< The voxel uniforms.
    vec2_t depth_range; ///< The depth range (near, far) in world units.
    int32_t view_type; ///< The view type, e.g. `VIEW_MAIN`.
    int32_t ticks; ///< The renderer time in milliseconds.
    float ambient; ///< The ambient scalar.
    float modulate; ///< The light modulation scalar.
    float caustics; ///< The caustics intensity scalar.
    float lighting_distance; ///< Distance threshold beyond which vertex lighting is used.
    int editor; ///< Non-zero when the in-game editor is active.
    int developer; ///< Non-zero when developer mode is enabled.
    int wireframe; ///< Non-zero when wireframe rendering is enabled.
  } block;

} r_uniforms_t;

/**
 * @brief The uniform variables block, updated once per frame and common to all programs.
 */
extern r_uniforms_t r_uniforms;

// developer tools
extern cvar_t *r_alpha_test;
extern cvar_t *r_cull;
extern cvar_t *r_depth_pass;
extern cvar_t *r_draw_occlusion_queries;
extern cvar_t *r_draw_bsp_blocks;
extern cvar_t *r_draw_bsp_normals;
extern cvar_t *r_draw_bsp_voxels;
extern cvar_t *r_draw_entity_bounds;
extern cvar_t *r_draw_light_bounds;
extern cvar_t *r_draw_material_stages;
extern cvar_t *r_draw_wireframe;
extern cvar_t *r_get_error;
extern cvar_t *r_occlude;

void R_GetError_(const char *function, const char *msg);

#define R_GetError(msg) { \
  if (r_get_error->integer) { \
    R_GetError_(__func__, msg); \
  } \
}

#endif
