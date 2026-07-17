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
 * @brief Vec4-aligned light uniform.
 */
typedef struct {

  /**
   * @brief Light origin and radius.
   */
  alignas(16) vec4_t origin;

  /**
   * @brief Light color and intensity.
   */
  vec4_t color;

  /**
   * @brief Shadow atlas tile origin, or (-1, -1) if the light has no shadow.
   */
  vec2_t shadow;
} r_light_uniform_t;

/**
 * @brief Static BSP light uniform block.
 */
typedef struct {

  /**
   * @brief Number of BSP lights.
   */
  int32_t num_lights;

  /**
   * @brief BSP lights indexed by BSP lump index.
   */
  alignas(16) r_light_uniform_t lights[MAX_BSP_LIGHTS];
} r_bsp_lights_uniform_block_t;

/**
 * @brief Per-frame dynamic light uniform block.
 */
typedef struct {

  /**
   * @brief Number of dynamic lights.
   */
  int32_t num_lights;

  /**
   * @brief Dynamic lights in view order.
   */
  alignas(16) r_light_uniform_t lights[MAX_DYNAMIC_LIGHTS];
} r_dynamic_lights_uniform_block_t;

/**
 * @brief Per-frame light storage buffers and mirrored uniform blocks.
 */
typedef struct {

  /**
   * @brief GPU buffer for `bsp_block`.
   */
  Buffer *bsp_buffer;

  /**
   * @brief CPU copy of the BSP light block.
   */
  r_bsp_lights_uniform_block_t bsp_block;

  /**
   * @brief GPU buffer for `dynamic_block`.
   */
  Buffer *dynamic_buffer;

  /**
   * @brief CPU copy of the dynamic light block.
   */
  r_dynamic_lights_uniform_block_t dynamic_block;
} r_lights_t;

/**
 * @brief Per-frame light storage.
 */
extern r_lights_t r_lights;

void R_ActiveDynamicLights(const r_view_t *view, const box3_t bounds, r_active_dynamic_lights_t *out);
void R_UpdateLights(r_view_t *view, CopyPass *copyPass);
void R_InitLights(void);
void R_ShutdownLights(void);
#endif
