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

bool R_CulludeBox(const RenderView *view, const Box3 bounds);
bool R_CulludeSphere(const RenderView *view, const Vec3 point, const float radius);
bool R_OccludeBox(const RenderView *view, const Box3 bounds);
bool R_OccludeSphere(const RenderView *view, const Vec3 origin, float radius);

#if defined(__R_LOCAL_H__)

/**
 * @brief The maximum number of occlusion queries.
 */
#define MAX_OCCLUSION_QUERIES (MAX_BSP_BLOCKS + MAX_BSP_LIGHTS)

/**
 * @brief Occlusion query state.
 */
typedef struct {
  /**
   * @brief Allocated occlusion queries.
   */
  RenderOcclusionQuery queries[MAX_OCCLUSION_QUERIES];

  /**
   * @brief Number of allocated queries.
   */
  int32_t numQueries;

  /**
   * @brief The bounds of this frame's occluded world block queries, compacted so
   * that `R_OccludeBox` need only visit the blocks relevant to each of its passes.
   */
  Box3 occludedBounds[MAX_BSP_BLOCKS];
  int32_t numOccludedBounds;

  /**
   * @brief The bounds of this frame's visible world block queries, compacted as
   * above.
   */
  Box3 visibleBounds[MAX_BSP_BLOCKS];
  int32_t numVisibleBounds;

  /**
   * @brief Per-instance occlusion box bounds.
   */
  Vector *boxes;

  /**
   * @brief Shared occlusion query pool.
   */
  QueryPool *pool;

  /**
   * @brief Per-instance box buffer.
   */
  Buffer *instanceBuffer;

  /**
   * @brief Unit-cube vertex buffer.
   */
  Buffer *vertexBuffer;

  /**
   * @brief Unit-cube index buffer.
   */
  Buffer *elementsBuffer;

  /**
   * @brief Occlusion query pipeline.
   */
  GraphicsPipeline *pipeline;

  /**
   * @brief Query result transfer buffer.
   */
  TransferBuffer *transfer;
} RenderOcclusion;

extern RenderOcclusion r_occlusion;

RenderOcclusionQuery *R_AllocOcclusionQuery(const Box3 bounds);
void R_AppendOcclusionQueryBox(RenderOcclusionQuery *query, Box3 bounds);
void R_FreeOcclusionQueries(void);
void R_LoadOcclusionQueries(void);
void R_DrawOcclusionQueries(const RenderView *view, CommandBuffer *commands);
void R_InitOcclusionQueries(void);
void R_ShutdownOcclusionQueries(void);

#endif
