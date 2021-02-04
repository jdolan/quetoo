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

#include "cm_types.h"

cm_trace_t Cm_BoxTrace(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
                       const int32_t head_node, const int32_t contents);

/**
 * @brief Collision detection for non-world models. Rotates the specified end
 * points into the model's space, and traces down the relevant subset of the
 * BSP tree. For inline BSP models, the head node is the root of the model's
 * subtree. For mesh models, a special reserved box hull and head node are
 * used.
 *
 * @param start The trace start point, in world space.
 * @param end The trace end point, in world space.
 * @param mins The bounding box mins, in model space.
 * @param maxs The bounding box maxs, in model space.
 * @param head_node The BSP head node to recurse down.
 * @param contents The contents mask to clip to.
 * @param matrix The matrix of the entity to clip to.
 * @param inverse_matrix The inverse matrix of the entity to clip to.
 *
 * @return The trace.
 */
cm_trace_t Cm_TransformedBoxTrace(const vec3_t start, const vec3_t end,
								  const vec3_t mins, const vec3_t maxs,
								  const int32_t head_node, const int32_t contents,
                                  const mat4_t matrix, const mat4_t inverse_matrix);

void Cm_EntityBounds(const solid_t solid, const vec3_t origin, const vec3_t angles,
                     const vec3_t mins, const vec3_t maxs, vec3_t *bounds_mins, vec3_t *bounds_maxs);

void Cm_TraceBounds(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
                    vec3_t *bounds_mins, vec3_t *bounds_maxs);

#ifdef __CM_LOCAL_H__
#endif /* __CM_LOCAL_H__ */
