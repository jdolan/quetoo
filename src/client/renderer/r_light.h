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

void R_AddLight(r_view_t *view, const r_light_t *l);

#if defined(__R_LOCAL_H__)

/**
 * @brief The light uniform type.
 * @remarks This struct is vec4 aligned.
 */
typedef struct {

  /**
   * @brief The light origin in model space (xyz) and radius (w).
   */
  vec4_t origin;

  /**
   * @brief The light color (xyz) and intensity (w).
   */
  vec4_t color;

  /**
   * @brief Shadow atlas tile: xy = base UV origin, z = tile UV size, w = layer index.
   */
  vec4_t shadow;
} r_light_uniform_t;

/**
 * @brief The lights uniform block struct.
 * @remarks This struct is vec4 aligned. The counts live here, with their data,
 * mirroring the std430 `lights_block` in light.glsl (two ints then 8 bytes of
 * padding so `lights` begins at its vec4-aligned offset).
 */
typedef struct {

  /**
   * @brief The number of light sources in `lights`.
   */
  int32_t num_lights;

  /**
   * @brief The number of leading static BSP lights (lit via voxels); lights at
   * [num_bsp_lights, num_lights) are the dynamic tail, lit directly.
   */
  int32_t num_bsp_lights;

  /**
   * @brief Padding to align `lights` to 16 bytes (std430).
   */
  int32_t _pad[2];

  /**
   * @brief The light sources for the current frame.
   */
  r_light_uniform_t lights[MAX_LIGHTS];
} r_light_uniform_block_t;

/**
 * @brief The lights uniform block type.
 */
typedef struct {

  /**
   * @brief The GPU storage buffer backing the lights block.
   */
  Buffer *buffer;

  /**
   * @brief The lights block, mirrored to `buffer` each frame.
   */
  r_light_uniform_block_t block;
} r_lights_t;

/**
 * @brief The lights uniform block, updated once per frame.
 */
extern r_lights_t r_lights;

void R_UpdateLights(r_view_t *view);
void R_ActiveLights(const r_view_t *view, const box3_t bounds, uint32_t mask[4]);
void R_InitLights(void);
void R_ShutdownLights(void);
#endif
