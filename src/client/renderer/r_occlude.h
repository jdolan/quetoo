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

bool R_CulludeBox(const r_view_t *view, const box3_t bounds);
bool R_CulludeSphere(const r_view_t *view, const vec3_t point, const float radius);
bool R_OccludeBox(const r_view_t *view, const box3_t bounds);
bool R_OccludeSphere(const r_view_t *view, const vec3_t origin, float radius);

#if defined(__R_LOCAL_H__)

/**
 * @brief The maximum number of occlusion queries: one per BSP block plus one per BSP light.
 */
#define MAX_OCCLUSION_QUERIES (MAX_BSP_BLOCKS + MAX_BSP_LIGHTS)

/**
 * @brief The occlusion query accounting structure.
 */
typedef struct {
  /**
   * @brief The occlusion queries. A query's position in this array is also its index into `query_pool`.
   */
  r_occlusion_query_t queries[MAX_OCCLUSION_QUERIES];

  /**
   * @brief The number of allocated queries.
   */
  int32_t num_queries;

  /**
   * @brief The per-instance box bounds appended by `R_AppendOcclusionQueryBox`, flattened
   * into `instance_buffer` by `R_LoadOcclusionQueries`. Reset by `R_FreeOcclusionQueries`.
   */
  Vector *boxes;

  /**
   * @brief The shared occlusion query pool.
   */
  QueryPool *pool;

  /**
   * @brief The per-instance box bounds buffer (INSTANCE rate, slot 1), built from `boxes`
   * by `R_LoadOcclusionQueries` when loading completes.
   */
  Buffer *instanceBuffer;

  /**
   * @brief The constant unit-cube vertex buffer (VERTEX rate, slot 0), shared by every
   * occlusion box instance. Built once by `R_InitOcclusionQueries`.
   */
  Buffer *vertexBuffer;

  /**
   * @brief The shared index buffer for the unit cube (reused for every instance).
   */
  Buffer *elementsBuffer;

  /**
   * @brief The position-only, depth-only pipeline used to draw occlusion boxes.
   */
  GraphicsPipeline *pipeline;

  /**
   * @brief The download target for query results.
   */
  TransferBuffer *transfer;
} r_occlusion_t;

extern r_occlusion_t r_occlusion;

r_occlusion_query_t *R_AllocOcclusionQuery(const box3_t bounds);
void R_AppendOcclusionQueryBox(r_occlusion_query_t *query, box3_t bounds);
void R_FreeOcclusionQueries(void);
void R_LoadOcclusionQueries(void);
void R_DrawOcclusionQueries(const r_view_t *view, CommandBuffer *commands);
void R_InitOcclusionQueries(void);
void R_ShutdownOcclusionQueries(void);

#endif
