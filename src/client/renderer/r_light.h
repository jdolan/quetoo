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

void R_AddLight(RenderView *view, const RenderLight *l);

#if defined(__R_LOCAL_H__)

/**
 * @brief Vec4-aligned light uniform.
 */
typedef struct {

  /**
   * @brief Light origin and radius.
   */
  alignas(16) Vec4 origin;

  /**
   * @brief Light color and intensity.
   */
  Vec4 color;

  /**
   * @brief Shadow atlas tile origin, or (-1, -1) if the light has no shadow.
   */
  Vec2 tile;
} RenderLightUniform;

/**
 * @brief Static BSP light uniform block.
 */
typedef struct {

  /**
   * @brief Number of BSP lights.
   */
  int32_t numLights;

  /**
   * @brief BSP lights indexed by BSP lump index.
   */
  alignas(16) RenderLightUniform lights[MAX_BSP_LIGHTS];
} RenderBspLightsUniformBlock;

/**
 * @brief Per-frame dynamic light uniform block.
 */
typedef struct {

  /**
   * @brief Number of dynamic lights.
   */
  int32_t numLights;

  /**
   * @brief Dynamic lights in view order.
   */
  alignas(16) RenderLightUniform lights[MAX_DYNAMIC_LIGHTS];
} RenderDynamicLightsUniformBlock;

/**
 * @brief Per-frame light storage buffers and mirrored uniform blocks.
 */
typedef struct {
  /**
   * @brief GPU buffer for `bspBlock`.
   */
  Buffer *bspBuffer;

  /**
   * @brief CPU copy of the BSP light block.
   */
  RenderBspLightsUniformBlock bspBlock;

  /**
   * @brief GPU buffer for `dynamicBlock`.
   */
  Buffer *dynamicBuffer;

  /**
   * @brief CPU copy of the dynamic light block.
   */
  RenderDynamicLightsUniformBlock dynamicBlock;

  /**
   * @brief The transfer buffer sourcing both blocks' uploads, held for the renderer's
   * lifetime because they are uploaded every frame.
   */
  TransferBuffer *transferBuffer;

  /**
   * @brief One voxel with no lights, bound where a level has no clustered light
   * data, or a view has no level.
   */
  Buffer *voxelFallbackBuffer;
} RenderLights;

/**
 * @brief Per-frame light storage.
 */
extern RenderLights rLights;

void R_ActiveDynamicLights(const RenderView *view, const Box3 bounds, RenderActiveDynamicLights *out);
void R_UpdateLights(RenderView *view, CopyPass *copyPass);
void R_InitLights(void);
void R_ShutdownLights(void);
#endif
