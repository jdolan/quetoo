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
 * @brief Adjust inputs so that mins and maxs are always symetric, which
 * avoids some complications with plane expanding of rotated bmodels.
 * @param start 
 * @param end 
 * @param mins 
 * @param maxs 
*/
static inline void Cm_AdjustTraceSymmetry(vec3_t *start, vec3_t *end, vec3_t *mins, vec3_t *maxs) {

	const vec3_t offset = Vec3_Scale(Vec3_Add(*mins, *maxs), .5f);

	*mins = Vec3_Subtract(*mins, offset);
	*maxs = Vec3_Subtract(*maxs, offset);

	*start = Vec3_Add(*start, offset);
	*end = Vec3_Add(*end, offset);
}

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
static inline cm_trace_t Cm_TransformedBoxTrace(const vec3_t start, const vec3_t end,
								  const vec3_t mins, const vec3_t maxs,
								  const int32_t head_node, const int32_t contents,
                                  const mat4_t matrix, const mat4_t inverse_matrix) {
	
	vec3_t start0 = start, end0 = end, mins0 = mins, maxs0 = maxs;

	Cm_AdjustTraceSymmetry(&start0, &end0, &mins0, &maxs0);

	const vec3_t start1 = Mat4_Transform(inverse_matrix, start0);
	const vec3_t end1 = Mat4_Transform(inverse_matrix, end0);

	// sweep the box through the model
	cm_trace_t trace = Cm_BoxTrace(start1, end1, mins0, maxs0, head_node, contents);

	if (trace.fraction < 1.0f) { // transform the impacted plane
		trace.plane = Cm_TransformPlane(matrix, &trace.plane);
	}

	// and calculate the final end point
	trace.end = Vec3_Mix(start, end, trace.fraction);

	return trace;
}

void Cm_EntityBounds(const solid_t solid, const vec3_t origin, const vec3_t angles,
                     const vec3_t mins, const vec3_t maxs, vec3_t *bounds_mins, vec3_t *bounds_maxs);

void Cm_TraceBounds(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
                    vec3_t *bounds_mins, vec3_t *bounds_maxs);

#ifdef __CM_LOCAL_H__
#endif /* __CM_LOCAL_H__ */
