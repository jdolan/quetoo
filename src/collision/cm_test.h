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

cm_bsp_plane_t Cm_Plane(const vec3_t normal, float dist);
float Cm_DistanceToPlane(const vec3_t point, const cm_bsp_plane_t *plane);
float Cm_DistanceToFace(const vec3_t point, const cm_bsp_plane_t *plane, int32_t plane_side);
int32_t Cm_PlaneTypeForNormal(const vec3_t normal);
int32_t Cm_SignBitsForNormal(const vec3_t normal);
int32_t Cm_BoxOnPlaneSide(const vec3_t mins, const vec3_t maxs, const cm_bsp_plane_t *plane);
int32_t Cm_SetBoxHull(const vec3_t mins, const vec3_t maxs, const int32_t contents);
int32_t Cm_PointLeafnum(const vec3_t p, int32_t head_node);
int32_t Cm_PointContents(const vec3_t p, int32_t head_node);
int32_t Cm_TransformedPointContents(const vec3_t p, int32_t head_node, const mat4_t *inverse_matrix);
size_t Cm_BoxLeafnums(const vec3_t mins, const vec3_t maxs, int32_t *list, size_t len, int32_t *top_node, int32_t head_node);

#ifdef __CM_LOCAL_H__
void Cm_InitBoxHull(void);
#endif /* __CM_LOCAL_H__ */
