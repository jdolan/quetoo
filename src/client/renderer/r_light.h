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

#include <stdalign.h>

#include "r_types.h"

void R_AddLight(r_view_t *view, const r_light_t *l);

#if defined(__R_LOCAL_H__)

/**
 * @brief The light uniform type.
 * @remarks This struct is vec4 aligned.
 */
typedef struct {

  /**
   * @brief The light origin in model space (xyz) and radius (w).
   * @details `alignas` pads the struct to a 16-byte multiple now that
   * `shadow` is a trailing vec2, matching std430's forced array stride.
   */
  alignas(16) vec4_t origin;

  /**
   * @brief The light color (xyz) and intensity (w).
   */
  vec4_t color;

  /**
   * @brief This light's shadow atlas tile origin, in pixels, or (-1, -1) if
   * this light casts no shadow. The tile is square and the same in all six
   * face textures; its size is derived in-shader from
   * `textureSize() / SHADOW_ATLAS_LIGHTS_PER_ROW` rather than carried here.
   */
  vec2_t shadow;
} r_light_uniform_t;

/**
 * @brief The BSP lights uniform block: static, voxel-gridded light sources
 * addressed by their fixed BSP lump index, mirroring the std430
 * `bsp_lights_block` in light.glsl.
 */
typedef struct {

  /**
   * @brief The number of light sources in `bsp_lights`.
   */
  int32_t num_bsp_lights;

  /**
   * @brief The static BSP light sources, indexed by BSP lump index.
   */
  alignas(16) r_light_uniform_t bsp_lights[MAX_BSP_LIGHTS];
} r_bsp_lights_uniform_block_t;

/**
 * @brief The dynamic lights uniform block: the per-frame dynamic tail
 * (trails, explosions, animated lights, ...), mirroring the std430
 * `dynamic_lights_block` in light.glsl.
 */
typedef struct {

  /**
   * @brief The number of light sources in `dynamic_lights`.
   */
  int32_t num_dynamic_lights;

  /**
   * @brief The dynamic light sources for the current frame, in view order.
   */
  alignas(16) r_light_uniform_t dynamic_lights[MAX_DYNAMIC_LIGHTS];
} r_dynamic_lights_uniform_block_t;

/**
 * @brief The lights uniform blocks type: two separate GPU storage buffers, one
 * per light class, so that shaders needing only dynamic lights (e.g. sprites)
 * don't have to reason about a combined index space.
 */
typedef struct {

  /**
   * @brief The GPU storage buffer backing `bsp_block`.
   */
  Buffer *bsp_buffer;

  /**
   * @brief The BSP lights block, mirrored to `bsp_buffer` each frame.
   */
  r_bsp_lights_uniform_block_t bsp_block;

  /**
   * @brief The GPU storage buffer backing `dynamic_block`.
   */
  Buffer *dynamic_buffer;

  /**
   * @brief The dynamic lights block, mirrored to `dynamic_buffer` each frame.
   */
  r_dynamic_lights_uniform_block_t dynamic_block;
} r_lights_t;

/**
 * @brief The lights uniform block, updated once per frame.
 */
extern r_lights_t r_lights;

void R_UpdateLights(r_view_t *view);
void R_ActiveDynamicLights(const r_view_t *view, const box3_t bounds, uint32_t mask[MAX_DYNAMIC_LIGHTS / 32]);
void R_InitLights(void);
void R_ShutdownLights(void);
#endif
