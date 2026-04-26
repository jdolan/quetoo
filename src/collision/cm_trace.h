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

/** @brief Performs a box sweep trace through the BSP tree from start to end.
 * @param start The trace start point.
 * @param end The trace end point.
 * @param bounds The AABB of the moving object (zero-sized for a point trace).
 * @param head_node The BSP head node to trace against.
 * @param contents The contents mask; only brush sides with matching contents are tested.
 * @return The cm_trace_t result; check `fraction` (1.0 = no hit) and `surface`.
 */
__attribute__ ((warn_unused_result))
cm_trace_t Cm_BoxTrace(const vec3_t start, const vec3_t end, const box3_t bounds, int32_t head_node,
             int32_t contents);

/** @brief Like Cm_BoxTrace but applies a model transform to start, end and planes.
 * @param matrix The forward transform of the entity being traced against.
 * @param inverse_matrix The inverse transform, used to bring the ray into model space.
 */
__attribute__ ((warn_unused_result))
cm_trace_t Cm_TransformedBoxTrace(const vec3_t start, const vec3_t end, const box3_t bounds, int32_t head_node,
                        int32_t contents, const mat4_t matrix, const mat4_t inverse_matrix);

/** @brief Returns the world-space AABB for the given solid type and model transform.
 * @param solid The solid type constant.
 * @param matrix The entity's current world transform.
 * @param bounds The entity's local-space bounds.
 */
__attribute__ ((warn_unused_result))
box3_t Cm_EntityBounds(const solid_t solid, const mat4_t matrix, const box3_t bounds);

/** @brief Returns the AABB enclosing the entire swept path from start to end.
 * @param bounds The moving object's AABB; used to expand the swept volume.
 */
__attribute__ ((warn_unused_result))
box3_t Cm_TraceBounds(const vec3_t start, const vec3_t end, const box3_t bounds);

#if defined(__CM_LOCAL_H__)
#endif /* __CM_LOCAL_H__ */
