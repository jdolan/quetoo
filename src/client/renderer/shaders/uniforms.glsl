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

#ifndef _UNIFORMS_GLSL_
#define _UNIFORMS_GLSL_

/**
 * @file uniforms.glsl
 * @brief Declares shared descriptor bindings and renderer uniform blocks.
 * @remarks Define VERTEX_SHADER or FRAGMENT_SHADER before including this file.
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

#define BINDING_UNIFORMS 0
#define BINDING_LOCALS   1

#define VIEW_UNKNOWN        0
#define VIEW_MAIN           1
#define VIEW_PLAYER_MODEL   2
#define VIEW_SUBVIEW         3

#define BSP_VOXEL_SIZE      32.0

#define MAX_BSP_LIGHTS 512
#define MAX_DYNAMIC_LIGHTS 512
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
 * @brief Voxel grid bounds and dimensions.
 */
struct Voxels {
  /**
   * @brief World-space voxel minimums.
   */
  vec4 mins;

  /**
   * @brief World-space voxel maximums.
   */
  vec4 maxs;

  /**
   * @brief View origin in voxel space.
   */
  vec4 viewCoordinate;

  /**
   * @brief Voxel grid size.
   */
  vec4 size;
};

/**
 * @brief Per-frame renderer uniforms shared by all shaders.
 */
layout (std140, set = UNIFORM_SET, binding = BINDING_UNIFORMS) uniform uniformsBlock {
  /**
   * @brief Viewport rectangle in device pixels.
   */
  ivec4 viewport;

  /**
   * @brief 3D projection matrix.
   */
  mat4 projection3D;

  /**
   * @brief View matrix.
   */
  mat4 view;

  /**
   * @brief Sky projection matrix.
   */
  mat4 skyProjection;

  /**
   * @brief Light projection matrix.
   */
  mat4 lightProjection;

  /**
   * @brief Voxel grid parameters.
   */
  Voxels voxels;

  /**
   * @brief View depth range in world units.
   */
  vec2 depthRange;

  /**
   * @brief Active view identifier.
   */
  int viewType;

  /**
   * @brief Renderer time in milliseconds.
   */
  int ticks;

  /**
   * @brief Ambient lighting modulation, per channel.
   */
  vec3 ambient;

  /**
   * @brief Modulation scalar.
   */
  float modulate;

  /**
   * @brief Saturation scalar.
   */
  float saturation;

  /**
   * @brief Caustics scalar.
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
   * @brief Editor debug flags.
   */
  int editor;

  /**
   * @brief Developer debug flags.
   */
  int developer;

  /**
   * @brief Pads the block to a multiple of vec4, as std140 requires.
   */
  vec2 padding;
};

#endif // _UNIFORMS_GLSL_
