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
 * @brief The shadow atlas: one plain 2D depth texture per cube face.
 * @details SDL_gpu forbids `DEPTH_STENCIL_TARGET` on array textures, so instead
 * of a layered array, each of the six cube faces gets its own texture; a given
 * light occupies the same tile position (row/col derived from its index) in
 * all six. Each texture is a square grid of `lights_per_row` x `lights_per_row`
 * tiles.
 */
typedef struct {

  /**
   * @brief The six D16 depth textures (one per cube face), storing linear
   * distance-from-light.
   */
  Texture *textures[6];

  /**
   * @brief The comparison sampler for shadow atlas lookups (hardware PCF).
   */
  Sampler *sampler;

  /**
   * @brief Per-face texture dimensions in pixels (square: `layer_size` × `layer_size`).
   */
  int32_t layer_size;

  /**
   * @brief The tile size in pixels.
   */
  int32_t tile_size;

  /**
   * @brief The number of lights per row/column in each face texture's grid.
   */
  int32_t lights_per_row;

  /**
   * @brief The number of lights that can have a shadow this frame
   * (`lights_per_row` × `lights_per_row`).
   */
  int32_t lights_per_layer;

  /**
   * @brief Frame counter for temporal face amortization.
   */
  uint32_t frame_count;
} r_shadow_atlas_t;

extern r_shadow_atlas_t r_shadow_atlas;

void R_DrawShadows(const r_view_t *view);
void R_InitShadows(void);
void R_ShutdownShadows(void);

#endif
