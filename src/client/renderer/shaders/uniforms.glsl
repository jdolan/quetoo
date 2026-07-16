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

#ifndef _UNIFORMS_GLSL_
#define _UNIFORMS_GLSL_

/*
 * SDL_gpu descriptor set / binding slot map.
 *
 * The offline build (Makefile.am) defines exactly one of VERTEX_SHADER or
 * FRAGMENT_SHADER via glslc -D, selecting the descriptor sets SDL_gpu expects:
 *
 *   Stage     | Sampled textures / storage | Uniform buffers
 *   ----------+----------------------------+----------------
 *   vertex    | set 0                      | set 1
 *   fragment  | set 2                      | set 3
 *
 * Within a set, bindings are assigned in declaration order. BINDING_UNIFORMS /
 * BINDING_LOCALS below are the only bindings genuinely shared by every program
 * (the globals block and the per-draw locals block); every other resource's
 * binding is local to the program that uses it, defined by that program's own
 * .glsl file before it includes this one -- see e.g. bsp_fs.glsl.
 */
#if defined(VERTEX_SHADER)
  #define SAMPLER_SET 0
  #define UNIFORM_SET 1
#elif defined(FRAGMENT_SHADER)
  #define SAMPLER_SET 2
  #define UNIFORM_SET 3
#else
  #error "Define VERTEX_SHADER or FRAGMENT_SHADER when compiling shaders."
#endif

/*
 * Uniform-buffer bindings (within UNIFORM_SET). Binding 0 is the per-frame
 * globals block, present in every program. Per-program per-draw blocks
 * ("locals": model matrix, lerp, colors, ...) use BINDING_LOCALS.
 */
#define BINDING_UNIFORMS 0
#define BINDING_LOCALS   1

#define VIEW_UNKNOWN        0
#define VIEW_MAIN           1
#define VIEW_PLAYER_MODEL   2

#define BSP_VOXEL_SIZE      32.0

#define MAX_BSP_LIGHTS 768
#define MAX_DYNAMIC_LIGHTS 256
#define MAX_LIGHTS (MAX_BSP_LIGHTS + MAX_DYNAMIC_LIGHTS)

#define CONTENTS_NONE       0x0
#define CONTENTS_SOLID      0x1
#define CONTENTS_WINDOW     0x2
#define CONTENTS_DECORATION 0x4
#define CONTENTS_LAVA       0x8
#define CONTENTS_SLIME      0x10
#define CONTENTS_WATER      0x20
#define CONTENTS_MIST       0x40

#define CONTENTS_MASK_SOLID  (CONTENTS_SOLID | CONTENTS_WINDOW)
#define CONTENTS_MASK_LIQUID (CONTENTS_WATER | CONTENTS_LAVA | CONTENTS_SLIME)

/**
 * @brief The voxels struct.
 */
struct voxels_t {
  /**
   * @brief The voxels mins, in world space.
   */
  vec4 mins;

  /**
   * @brief The voxels maxs, in world space.
   */
  vec4 maxs;

  /**
   * @brief The view origin, in voxel space.
   */
  vec4 view_coordinate;

  /**
   * @brief The grid size, in voxels.
   */
  vec4 size;
};

/**
 * @brief The uniforms block (per-frame globals, shared by all programs).
 */
layout (std140, set = UNIFORM_SET, binding = BINDING_UNIFORMS) uniform uniforms_block {
  /**
   * @brief The viewport (x, y, w, h) in device pixels.
   */
  ivec4 viewport;

  /**
   * @brief The 3D projection matrix.
   */
  mat4 projection3D;

  /**
   * @brief The view matrix.
   */
  mat4 view;

  /**
   * @brief The projection matrix for environment cubemaps.
   */
  mat4 sky_projection;

  /**
   * @brief The projection matrix for shadow projection.
   */
  mat4 light_projection;

  /**
   * @brief The voxels.
   */
  voxels_t voxels;

  /**
   * @brief The depth range, in world units.
   */
  vec2 depth_range;

  /**
   * @brief The view type, e.g. VIEW_MAIN.
   */
  int view_type;

  /**
   * @brief The renderer time, in milliseconds.
   */
  int ticks;

  /**
   * @brief The ambient scalar.
   */
  float ambient;

  /**
   * @brief The modulate scalar.
   */
  float modulate;

  /**
   * @brief The saturation scalar.
   */
  float saturation;

  /**
   * @brief The caustics scalar.
   */
  float caustics;

  /**
   * @brief The ambient occlusion scalar (0 = disabled, 1 = full).
   */
  float ambient_occlusion;

  /**
   * @brief Distance threshold for switching to vertex lighting.
   */
  float lighting_distance;

  /**
   * @brief The editor flags, used for in-game lighting and more.
   */
  int editor;

  /**
   * @brief The developer flags, used for shader development tweaking.
   */
  int developer;
};

#endif // _UNIFORMS_GLSL_
