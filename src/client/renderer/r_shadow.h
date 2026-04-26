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
 * @brief The shadow atlas.
 * @details Uses a layered 2D array texture (GL_TEXTURE_2D_ARRAY) with square layers.
 * Each layer has the same dimensions and tile layout. Lights are assigned to layers
 * sequentially: layer = light_index / lights_per_layer.
 */
typedef struct {
  /**
   * @brief The 2D array depth texture atlas.
   */
  GLuint texture;
  /**
   * @brief The depth pass framebuffer.
   */
  GLuint framebuffer;
  /**
   * @brief Per-layer dimensions in pixels (square: layer_size × layer_size).
   */
  GLsizei layer_size;
  /**
   * @brief The tile size in pixels.
   */
  GLsizei tile_size;
  /**
   * @brief The number of lights per row in each layer's grid.
   */
  int32_t lights_per_row;
  /**
   * @brief The number of lights per column in each layer's grid.
   */
  int32_t lights_per_col;
  /**
   * @brief The number of lights per layer (lights_per_row × lights_per_col).
   */
  int32_t lights_per_layer;
  /**
   * @brief The number of layers in the array texture.
   */
  int32_t num_layers;
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
