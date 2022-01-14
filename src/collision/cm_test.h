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

/**
 * @return The distance from `point` to `plane`.
 */
static inline float Cm_DistanceToPlane(const vec3_t point, const cm_bsp_plane_t *plane) {
	return Vec3_Dot(point, plane->normal) - plane->dist;
}

int32_t Cm_PlaneTypeForNormal(const vec3_t normal);
int32_t Cm_SignBitsForNormal(const vec3_t normal);

cm_bsp_plane_t Cm_Plane(const vec3_t normal, float dist);
cm_bsp_plane_t Cm_TransformPlane(const mat4_t matrix, const cm_bsp_plane_t plane);

int32_t Cm_BoxOnPlaneSide(const box3_t bounds, const cm_bsp_plane_t *plane);
_Bool Cm_PointInsideBrush(const vec3_t point, const cm_bsp_brush_t *brush);

int32_t Cm_SetBoxHull(const box3_t bounds, const int32_t contents);

int32_t Cm_PointLeafnum(const vec3_t p, int32_t head_node);
int32_t Cm_PointContents(const vec3_t p, int32_t head_node, const mat4_t inverse_matrix);

size_t Cm_BoxLeafnums(const box3_t bounds, int32_t *list, size_t length, int32_t *top_node, int32_t head_node, const mat4_t matrix);
int32_t Cm_BoxContents(const box3_t bounds, int32_t head_node, const mat4_t matrix);

#ifdef __CM_LOCAL_H__
void Cm_InitBoxHull(cm_bsp_t *bsp);
#endif /* __CM_LOCAL_H__ */
