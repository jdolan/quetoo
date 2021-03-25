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

#include "vector.h"

/**
 * @brief Represents a bounding box constructed from two points.
 */
typedef struct {
	/**
	 * @brief The mins of the bbox.
	 */
	vec3_t mins;

	/**
	 * @brief The maxs of the bbox.
	 */
	vec3_t maxs;
} box_t;

/**
 * @return A `box_t` with the specified mins and maxs.
 */
static inline box_t __attribute__ ((warn_unused_result)) Box_MinsMaxs(const vec3_t mins, const vec3_t maxs) {
	return (box_t) {
		.mins = mins,
		.maxs = maxs
	};
}

/**
 * @return A bounding box constructed from a 3d size parameter. The box is constructed
 * such that its origin is zero, and its size matches `size`.
 */
static inline box_t __attribute__ ((warn_unused_result)) Box3fv(const vec3_t size) {
	const vec3_t half_size = Vec3_Scale(size, .5f);
	return Box_MinsMaxs(
		Vec3_Negate(half_size),
		half_size
	);
}

/**
 * @return A bounding box constructed from a single size value. The box is constructed
 * such that its origin is zero, and its size matches { `distance`, `distance`, `distance` }.
 */
static inline box_t __attribute__ ((warn_unused_result)) Boxf(const float distance) {
	return Box3fv(Vec3(distance, distance, distance));
}

/**
 * @return A `box_t` centered around `0, 0, 0` with a size of zero.
 */
static inline box_t __attribute__ ((warn_unused_result)) Box_Zero(void) {
	return (box_t) {
		.mins = Vec3_Zero(),
		.maxs = Vec3_Zero()
	};
}

/**
 * @return A `box_t` centered around `0, 0, 0` with a size of -infinity.
 * @remarks This is often used as a seed value to add points to, as Box_Zero()
 * will require that `0, 0, 0` be in the box, but the points added may not be centered
 * around that point. Using this as a seed guarantees that the first point added will
 * become the new seed.
 */
static inline box_t __attribute__ ((warn_unused_result)) Box_Null(void) {
	return (box_t) {
		.mins = Vec3(INFINITY, INFINITY, INFINITY),
		.maxs = Vec3(-INFINITY, -INFINITY, -INFINITY)
	};
}

/**
 * @return A `box_t` that contains both the passed bounds as well as the passed
 * point.
 */
static inline box_t __attribute__ ((warn_unused_result)) Box_Append(const box_t bounds, const vec3_t point) {
	return (box_t) {
		.mins = Vec3_Minf(bounds.mins, point),
		.maxs = Vec3_Maxf(bounds.maxs, point)
	};
}

/**
 * @return A `box_t` that unions both of the passed bounds.
 */
static inline box_t __attribute__ ((warn_unused_result)) Box_Union(const box_t a, const box_t b) {
	return (box_t) {
		.mins = Vec3_Minf(a.mins, b.mins),
		.maxs = Vec3_Maxf(a.maxs, b.maxs)
	};
}

/**
 * @return A `box_t` constructed from a set of points.
 */
static inline box_t __attribute__ ((warn_unused_result)) Box_Points(const vec3_t *points, const size_t num_points) {
	box_t bounds = Box_Null();

	for (size_t i = 0; i < num_points; i++, points++) {
		bounds = Box_Append(bounds, *points);
	}

	return bounds;
}

/**
 * @brief Writes the eight corner points of the bounding box to "points".
 * It must be at least 8 `vec3_t` wide. The output of the points are in
 * axis order - assuming bitflags of 1 2 4 = X Y Z - where a bit unset is
 * mins and a bit set is maxs. This ordering should not be changed, as
 * a few places in our code requires it to be this order.
 */
static inline void Box_ToPoints(const box_t bounds, vec3_t *points) {

	for (int32_t i = 0; i < 8; i++) {
		points[i] = Vec3(
			((i & 1) ? bounds.maxs : bounds.mins).x,
			((i & 2) ? bounds.maxs : bounds.mins).y,
			((i & 4) ? bounds.maxs : bounds.mins).z
		);
	}
}

/**
 * @return `true` if `a` intersects the bounds `b`, `false` otherwise.
*/
static inline _Bool __attribute__ ((warn_unused_result)) Box_Intersects(const box_t a, const box_t b) {

	return Vec3_BoxIntersect(a.mins, a.maxs, b.mins, b.maxs);
}

/**
 * @return `true` if `a` contains the point `b`, `false` otherwise.
*/
static inline _Bool __attribute__ ((warn_unused_result)) Box_ContainsPoint(const box_t a, const vec3_t b) {

	return Vec3_BoxIntersect(b, b, a.mins, a.maxs);
}

/**
 * @return `true` if `b` is fully contained within `a`, `false` otherwise.
 */
static inline _Bool __attribute__ ((warn_unused_result)) Box_ContainsBounds(const box_t a, const box_t b) {

	for (int32_t j = 0; j < 3; j++) {
		if (b.mins.xyz[j] < a.mins.xyz[j] || b.maxs.xyz[j] > a.maxs.xyz[j]) {
			return false;
		}
	}

	return true;
}

/**
 * @return The relative size of all three axis of the bounds. This also works as
 * a vector between the two points of the box.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Box_Size(const box_t a) {
	return Vec3_Subtract(a.maxs, a.mins);
}

/**
 * @return The distance between the two corners of the bounds.
 */
static inline float __attribute__ ((warn_unused_result)) Box_Distance(const box_t a) {
	return Vec3_Distance(a.maxs, a.mins);
}

/**
 * @return The radius of the bounds (equivalent to Box_Distance(a) / 2). This is
 * a sphere that contains the whole box, including its corners.
 */
static inline float __attribute__ ((warn_unused_result)) Box_Radius(const box_t a) {
	return Box_Distance(a) / 2.f;
}

/**
 * @return The origin of the bounding box.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Box_Origin(const box_t a) {
	return Vec3_Mix(a.mins, a.maxs, .5f);
}

/**
 * @return A bounding box centered around `a` with a size of zero.
 */
static inline box_t __attribute__ ((warn_unused_result)) Box_FromOrigin(const vec3_t a) {
	return Box_MinsMaxs(a, a);
}

/**
 * @return A bounding box constructed from a 3d size parameter. The box is constructed
 * such that its origin is `origin`, and its size matches `size`.
 */
static inline box_t __attribute__ ((warn_unused_result)) Box_FromOriginSize(const vec3_t origin, const vec3_t size) {
	const vec3_t half_size = Vec3_Scale(size, .5f);
	return Box_MinsMaxs(
		Vec3_Subtract(origin, half_size),
		Vec3_Add(origin, half_size)
	);
}

/**
 * @return A bounding box constructed from the distance between the extent of each axis,
 * translated by `origin`.
 */
static inline box_t __attribute__ ((warn_unused_result)) Box_FromOriginDistance(const vec3_t origin, const float distance) {
	return Box_FromOriginSize(origin, Vec3(distance, distance, distance));
}

/**
 * @return A bounding box translated by the specified offset.
 */
static inline box_t __attribute__ ((warn_unused_result)) Box_Translate(const box_t bounds, const vec3_t translation) {
	return Box_MinsMaxs(
		Vec3_Add(bounds.mins, translation),
		Vec3_Add(bounds.maxs, translation)
	);
}

/**
 * @return A bounding box expanded (or shrunk, if a value is negative) by the
 * specified expansion values on all six axis.
 */
static inline box_t __attribute__ ((warn_unused_result)) Box_Expand3(const box_t bounds, const vec3_t expansion) {
	return Box_MinsMaxs(
		Vec3_Subtract(bounds.mins, expansion),
		Vec3_Add(bounds.maxs, expansion)
	);
}

/**
 * @return A bounding box expanded (or shrunk, if a value is negative) by the
 * specified expansion value on all six axis.
 */
static inline box_t __attribute__ ((warn_unused_result)) Box_Expand(const box_t bounds, const float expansion) {
	return Box_Expand3(bounds, Vec3(expansion, expansion, expansion));
}

/**
 * @return The bounding box `a` expanded by the bounding box `b`.
 */
static inline box_t __attribute__ ((warn_unused_result)) Box_ExpandBox(const box_t a, const box_t b) {
	return Box_MinsMaxs(
		Vec3_Add(a.mins, b.mins),
		Vec3_Add(a.maxs, b.maxs)
	);
}

/**
 * @return The point `p` clamped by the bounding box `b`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Box_ClampPoint(const box_t b, const vec3_t p) {
	return Vec3_Clamp(p, b.mins, b.maxs);
}

/**
 * @return The bounds `b` clamped by the bounding box `a`.
 */
static inline box_t __attribute__ ((warn_unused_result)) Box_ClampBounds(const box_t a, const box_t b) {
	return Box_MinsMaxs(
		Vec3_Clamp(b.mins, a.mins, a.maxs),
		Vec3_Clamp(b.maxs, a.mins, a.maxs)
	);
}

/**
 * @return A random point within the bounding box `b`, edges inclusive.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Box_RandomPoint(const box_t b) {
	return Vec3_Mix3(b.mins, b.maxs, Vec3_Random());
}

/**
 * @return Whether `a` and `b` are equal or not.
 */
static inline _Bool __attribute__ ((warn_unused_result)) Box_Equal(const box_t a, const box_t b) {
	return Vec3_Equal(a.mins, b.mins) && Vec3_Equal(a.maxs, b.maxs);
}

/**
 * @return The symetrical extents of `bounds`
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Box_Symetrical(const box_t bounds) {
	return Vec3_Maxf(Vec3_Fabsf(bounds.mins), Vec3_Fabsf(bounds.maxs));
}

/**
 * @return The `bounds` scaled by `scale`.
 */
static inline box_t __attribute__ ((warn_unused_result)) Box_Scale(const box_t bounds, const float scale) {
	return Box_MinsMaxs(
		Vec3_Scale(bounds.mins, scale),
		Vec3_Scale(bounds.maxs, scale)
	);
}