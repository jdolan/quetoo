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
 * @brief Returns the distance from `point` to `plane`.
 * @return The signed distance; positive is in front, negative is behind.
 */
static inline float Cm_DistanceToPlane(const Vec3 point, const CmBspPlane *plane) {
  return Vec3_Dot(point, plane->normal) - plane->dist;
}

/**
 * @brief Returns the `PLANE_`* type constant for the given normal vector.
 */
int32_t Cm_PlaneTypeForNormal(const Vec3 normal);

/**
 * @brief Returns the sign-bits mask for the given normal vector (used for fast BSP traversal).
 */
int32_t Cm_SignBitsForNormal(const Vec3 normal);

/**
 * @brief Constructs a `CmBspPlane` from a normal and distance.
 */
CmBspPlane Cm_Plane(const Vec3 normal, float dist);

/**
 * @brief Transforms a plane by the given 4x4 matrix.
 */
CmBspPlane Cm_TransformPlane(const Mat4 matrix, const CmBspPlane plane);

/**
 * @brief Projects a point onto the plane, returning the nearest point on the plane surface.
 */
Vec3 Cm_ProjectPointToPlane(const Vec3 point, const CmBspPlane *plane);

/**
 * @brief Returns the side(s) of the plane that the bounding box intersects.
 */
int32_t Cm_BoxOnPlaneSide(const Box3 bounds, const CmBspPlane *plane);

/**
 * @brief Returns true if the point lies inside (or on) all sides of the brush.
 */
bool Cm_PointInsideBrush(const Vec3 point, const CmBspBrush *brush);

/**
 * @brief Allocates a temporary hull for the given axis-aligned bounding box.
 * @return The head node number for the box hull.
 */
int32_t Cm_SetBoxHull(const Box3 bounds, const int32_t contents);

/**
 * @brief Returns the leaf number containing the given point.
 */
int32_t Cm_PointLeafnum(const Vec3 p, int32_t head_node);

/**
 * @brief Returns the contents mask at the given point in the BSP tree.
 */
int32_t Cm_PointContents(const Vec3 p, int32_t head_node, const Mat4 inverse_matrix);

/**
 * @brief Fills list[] with leaf numbers that overlap the bounding box.
 * @return The number of leaves written to list[].
 */
size_t Cm_BoxLeafnums(const Box3 bounds, int32_t *list, size_t length, int32_t *top_node, int32_t head_node);

/**
 * @brief Returns the combined contents mask for all BSP leaves overlapping the bounding box.
 */
int32_t Cm_BoxContents(const Box3 bounds, int32_t head_node);

#if defined(__CM_LOCAL_H__)
void Cm_InitBoxHull(CmBsp *bsp);
#endif
