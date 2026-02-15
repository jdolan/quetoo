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

void R_AddDecal(r_view_t *view, const r_decal_t *decal);

#ifdef __R_LOCAL_H__

#define DECAL_TEXTURE_SIZE 256
#define DECAL_TEXTURE_LAYERS 64

/**
 * @brief BSP decal vertex structure.
 */
typedef struct {
  /**
   * @brief The position.
   */
  vec3_t position;

  /**
   * @brief The diffusemap texture coordinate.
   */
  vec2_t texcoord;

  /**
   * @brief The color.
   */
  color32_t color;

  /**
   * @brief The decal creation time, for GPU decal expiration.
   */
  uint32_t time;

  /**
   * @brief The decal lifetime, for GPU decal expiration.
   */
  uint32_t lifetime;
} r_decal_vertex_t;

/**
 * @brief The decal triangle type.
 */
typedef struct {
  /**
   * @brief The vertexes.
   */
  r_decal_vertex_t vertexes[3];
} r_decal_triangle_t;

void R_UpdateDecals(r_view_t *view);
void R_DrawDecals(const r_view_t *view);
void R_InitDecals(void);
void R_ShutdownDecals(void);
#endif
