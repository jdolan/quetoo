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

#if defined(__R_LOCAL_H__)

/**
 * @brief The number of shadow-atlas tiles per row in each face.
 */
#define SHADOW_ATLAS_LIGHTS_PER_ROW 32

/**
 * @brief Shadow atlas resources.
 */
typedef struct {

  /**
   * @brief The per-face shadow atlas textures.
   */
  Texture *textures[6];

  /**
   * @brief The comparison sampler for shadow atlas lookups (hardware PCF).
   */
  Sampler *sampler;

  /**
   * @brief The tile size in pixels.
   */
  int32_t tile_size;
} r_shadow_atlas_t;

extern r_shadow_atlas_t r_shadow_atlas;

void R_UpdateShadows(r_view_t *view);
void R_ClearShadows(const r_view_t *view);
void R_DrawShadows(const r_view_t *view);
void R_InitShadows(void);
void R_ShutdownShadows(void);

#endif
