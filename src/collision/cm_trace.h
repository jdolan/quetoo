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
                       const int32_t head_node, const int32_t contents, const mat4_t *matrix, const mat4_t *inverse_matrix);

void Cm_EntityBounds(const solid_t solid, const vec3_t origin, const vec3_t angles, const mat4_t matrix,
                     const vec3_t mins, const vec3_t maxs, vec3_t *bounds_mins, vec3_t *bounds_maxs);

void Cm_TraceBounds(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
                    vec3_t *bounds_mins, vec3_t *bounds_maxs);

/**
 * @brief Rotate a bounding box by the specified matrix.
 * @param m 
 * @param mins 
 * @param maxs 
*/
static inline void Cm_TransformBounds(const mat4_t m, vec3_t *mins, vec3_t *maxs) {
	
	vec3_t points[8] = {
		*mins,
		{ .x = maxs->x, .y = mins->y, .z = mins->z },
		{ .x = mins->x, .y = maxs->y, .z = mins->z },
		{ .x = maxs->x, .y = maxs->y, .z = mins->z },
		{ .x = mins->x, .y = mins->y, .z = maxs->z },
		{ .x = maxs->x, .y = mins->y, .z = maxs->z },
		{ .x = maxs->x, .y = maxs->y, .z = maxs->z },
		*maxs
	};

	*mins = Vec3_Mins();
	*maxs = Vec3_Maxs();

	for (size_t i = 0; i < lengthof(points); i++) {
		vec3_t p = Mat4_Transform(m, points[i]);
		*mins = Vec3_Minf(*mins, p);
		*maxs = Vec3_Maxf(*maxs, p);
	}
}

#ifdef __CM_LOCAL_H__
#endif /* __CM_LOCAL_H__ */
