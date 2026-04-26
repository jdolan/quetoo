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
  vec4_t origin; ///< The light origin in model space (xyz) and radius (w).
  vec4_t color;  ///< The light color (xyz) and intensity (w).
  vec4_t shadow; ///< Shadow atlas tile: xy = base UV origin, z = tile UV size, w = layer index.
} r_light_uniform_t;

/**
 * @brief The lights uniform block struct.
 * @remarks This struct is vec4 aligned.
 */
typedef struct {
  r_light_uniform_t lights[MAX_LIGHTS]; ///< The light sources for the current frame.
} r_light_uniform_block_t;

/**
 * @brief The lights uniform block type.
 */
typedef struct {
  GLuint buffer;                 ///< The uniform buffer name.
  r_light_uniform_block_t block; ///< The uniform buffer interface block.
} r_lights_t;

/**
 * @brief The lights uniform block, updated once per frame.
 */
extern r_lights_t r_lights;

void R_UpdateLights(r_view_t *view);
void R_ActiveLights(const r_view_t *view, const box3_t bounds, GLint name);
void R_InitLights(void);
void R_ShutdownLights(void);
#endif
