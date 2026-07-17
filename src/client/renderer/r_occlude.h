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
  r_occlusion_query_t queries[MAX_OCCLUSION_QUERIES];

  /**
   * @brief Number of allocated queries.
   */
  int32_t num_queries;

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
  Buffer *instance_buffer;

  /**
   * @brief Unit-cube vertex buffer.
   */
  Buffer *vertex_buffer;

  /**
   * @brief Unit-cube index buffer.
   */
  Buffer *elements_buffer;

  /**
   * @brief Occlusion query pipeline.
   */
  GraphicsPipeline *pipeline;

  /**
   * @brief Query result transfer buffer.
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
