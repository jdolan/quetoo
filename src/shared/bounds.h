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
} bounds_t;

/**
 * @return A `bounds_t` with the specified mins and maxs.
 */
static inline bounds_t __attribute__ ((warn_unused_result)) Bounds(const vec3_t mins, const vec3_t maxs) {
	return (bounds_t) {
		.mins = mins,
		.maxs = maxs
	};
}

/**
 * @return A `bounds_t` centered around `0, 0, 0` with a size of zero.
 */
static inline bounds_t __attribute__ ((warn_unused_result)) Bounds_Zero(void) {
	return (bounds_t) {
		.mins = Vec3_Zero(),
		.maxs = Vec3_Zero()
	};
}

/**
 * @return A `bounds_t` centered around `0, 0, 0` with a size of -infinity.
 * @remarks This is often used as a seed value to add points to, as Bounds_Zero()
 * will require that `0, 0, 0` be in the box, but the points added may not be centered
 * around that point. Using this as a seed guarantees that the first point added will
 * become the new seed.
 */
static inline bounds_t __attribute__ ((warn_unused_result)) Bounds_Infinity(void) {
	return (bounds_t) {
		.mins = Vec3(INFINITY, INFINITY, INFINITY),
		.maxs = Vec3(-INFINITY, -INFINITY, -INFINITY)
	};
}

/**
 * @return A `bounds_t` that contains both the passed bounds as well as the passed
 * point.
 */
static inline bounds_t __attribute__ ((warn_unused_result)) Bounds_Append(const bounds_t bounds, const vec3_t point) {
	return (bounds_t) {
		.mins = Vec3_Minf(bounds.mins, point),
		.maxs = Vec3_Maxf(bounds.maxs, point)
	};
}

/**
 * @return A `bounds_t` that contains both of the passed bounds.
 */
static inline bounds_t __attribute__ ((warn_unused_result)) Bounds_Combine(const bounds_t a, const bounds_t b) {
	return (bounds_t) {
		.mins = Vec3_Minf(a.mins, b.mins),
		.maxs = Vec3_Maxf(a.maxs, b.maxs)
	};
}

/**
 * @return A `bounds_t` constructed from a set of points.
 */
static inline bounds_t __attribute__ ((warn_unused_result)) Bounds_Points(const vec3_t *points, const size_t num_points) {
	bounds_t bounds = Bounds_Infinity();

	for (size_t i = 0; i < num_points; i++, points++) {
		bounds = Bounds_Append(bounds, *points);
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
static inline void Bounds_ToPoints(const bounds_t bounds, vec3_t *points) {

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
static inline _Bool Bounds_Intersect(const bounds_t a, const bounds_t b) {

	return Vec3_BoxIntersect(a.mins, a.maxs, b.mins, b.maxs);
}

/**
 * @return `true` if `a` contains (intersects) the point `b`, `false` otherwise.
*/
static inline _Bool Bounds_Contains(const bounds_t a, const vec3_t b) {

	return Vec3_BoxIntersect(b, b, a.mins, a.maxs);
}

/**
 * @return `true` if `a` intersects *or* touches `b`, `false` otherwise.
 */
static inline _Bool __attribute__ ((warn_unused_result)) Bounds_Touching(const bounds_t a, const bounds_t b) {

	if (a.mins.x > b.maxs.x || a.mins.y > b.maxs.y || a.mins.z > b.maxs.z) {
		return false;
	}

	if (a.maxs.x < b.mins.x || a.maxs.y < b.mins.y || a.maxs.z < b.mins.z) {
		return false;
	}

	return true;
}

/**
 * @return The width (x axis) of the bounds.
 */
static inline float __attribute__ ((warn_unused_result)) Bounds_Width(const bounds_t a) {
	return a.maxs.x - a.mins.x;
}

/**
 * @return The length (y axis) of the bounds.
 */
static inline float __attribute__ ((warn_unused_result)) Bounds_Length(const bounds_t a) {
	return a.maxs.y - a.mins.y;
}

/**
 * @return The height (z axis) of the bounds.
 */
static inline float __attribute__ ((warn_unused_result)) Bounds_Height(const bounds_t a) {
	return a.maxs.z - a.mins.z;
}

/**
 * @return The relative size of all three axis of the bounds. This also works as
 * a vector between the two points of the box.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Bounds_Size(const bounds_t a) {
	return Vec3_Subtract(a.maxs, a.mins);
}

/**
 * @return A bounding box constructed from a 3d size parameter. The box is constructed
 * such that its origin is zero, and its size matches `size`.
 */
static inline bounds_t __attribute__ ((warn_unused_result)) Bounds_FromSize(const vec3_t size) {
	const vec3_t half_size = Vec3_Scale(size, .5f);
	return Bounds(
		Vec3_Negate(half_size),
		half_size
	);
}

/**
 * @return A bounding box constructed from a 3d size parameter. The box is constructed
 * such that its origin is zero, and its size equals `size` * 2.0.
 */
static inline bounds_t __attribute__ ((warn_unused_result)) Bounds_FromAbsoluteSize(const vec3_t size) {
	return Bounds(
		Vec3_Negate(size),
		size
	);
}

/**
 * @return A bounding box constructed from the distance between the extent of each axis. Equivalent to
 * Bounds_FromSize(distance, distance, distance).
 */
static inline bounds_t __attribute__ ((warn_unused_result)) Bounds_FromDistance(const float distance) {
	return Bounds_FromSize(Vec3(distance, distance, distance));
}

/**
 * @return A bounding box constructed from the distance between the center and each axis. Equivalent to
 * Bounds_FromAbsoluteSize(distance, distance, distance).
 */
static inline bounds_t __attribute__ ((warn_unused_result)) Bounds_FromAbsoluteDistance(const float distance) {
	return Bounds_FromAbsoluteSize(Vec3(distance, distance, distance));
}

/**
 * @return The distance between the two corners of the bounds.
 */
static inline float __attribute__ ((warn_unused_result)) Bounds_Distance(const bounds_t a) {
	return Vec3_Distance(a.maxs, a.mins);
}

/**
 * @return The radius of the bounds (equivalent to Bounds_Distance(a) / 2).
 */
static inline float __attribute__ ((warn_unused_result)) Bounds_Radius(const bounds_t a) {
	return Bounds_Distance(a) / 2.f;
}

/**
 * @return The origin of the bounding box.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Bounds_Origin(const bounds_t a) {
	return Vec3_Mix(a.mins, a.maxs, .5f);
}

/**
 * @return A bounding box centered around `a` with a size of zero.
 */
static inline bounds_t __attribute__ ((warn_unused_result)) Bounds_FromOrigin(const vec3_t a) {
	return Bounds(a, a);
}

/**
 * @return A bounding box constructed from a 3d size parameter. The box is constructed
 * such that its origin is `origin`, and its size matches `size`.
 */
static inline bounds_t __attribute__ ((warn_unused_result)) Bounds_FromOriginSize(const vec3_t origin, const vec3_t size) {
	const vec3_t half_size = Vec3_Scale(size, .5f);
	return Bounds(
		Vec3_Subtract(origin, half_size),
		Vec3_Add(origin, half_size)
	);
}


/**
 * @return A bounding box constructed from a 3d size parameter. The box is constructed
 * such that its origin is `origin`, and its size matches `size` * 2.
 */
static inline bounds_t __attribute__ ((warn_unused_result)) Bounds_FromOriginAbsoluteSize(const vec3_t origin, const vec3_t size) {
	return Bounds(
		Vec3_Subtract(origin, size),
		Vec3_Add(origin, size)
	);
}

/**
 * @return A bounding box constructed from the distance between the extent of each axis,
 * translated by `origin`.
 */
static inline bounds_t __attribute__ ((warn_unused_result)) Bounds_FromOriginDistance(const vec3_t origin, const float distance) {
	return Bounds_FromOriginSize(origin, Vec3(distance, distance, distance));
}

/**
 * @return A bounding box constructed from the distance between the center and each axis,
 * translated by `origin`.
 */
static inline bounds_t __attribute__ ((warn_unused_result)) Bounds_FromOriginAbsoluteDistance(const vec3_t origin, const float distance) {
	return Bounds_FromOriginAbsoluteSize(origin, Vec3(distance, distance, distance));
}

/**
 * @return A bounding box translated by the specified offset.
 */
static inline bounds_t __attribute__ ((warn_unused_result)) Bounds_Translate(const bounds_t bounds, const vec3_t translation) {
	return Bounds(
		Vec3_Add(bounds.mins, translation),
		Vec3_Add(bounds.maxs, translation)
	);
}

/**
 * @return A bounding box expanded (or shrunk, if a value is negative) by the
 * specified expansion values on all six axis.
 */
static inline bounds_t __attribute__ ((warn_unused_result)) Bounds_Expand3(const bounds_t bounds, const vec3_t expansion) {
	return Bounds(
		Vec3_Subtract(bounds.mins, expansion),
		Vec3_Add(bounds.maxs, expansion)
	);
}

/**
 * @return A bounding box expanded (or shrunk, if a value is negative) by the
 * specified expansion value on all six axis.
 */
static inline bounds_t __attribute__ ((warn_unused_result)) Bounds_Expand(const bounds_t bounds, const float expansion) {
	return Bounds_Expand3(bounds, Vec3(expansion, expansion, expansion));
}

/**
 * @return The bounding box `a` expanded by the bounding box `b`.
 */
static inline bounds_t __attribute__ ((warn_unused_result)) Bounds_ExpandBounds(const bounds_t a, const bounds_t b) {
	return Bounds(
		Vec3_Add(a.mins, b.mins),
		Vec3_Add(a.maxs, b.maxs)
	);
}

/**
 * @return The point `p` clamped by the bounding box `b`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Bounds_ClampPoint(const bounds_t b, const vec3_t p) {
	return Vec3_Clamp(p, b.mins, b.maxs);
}

/**
 * @return The bounds `b` clamped by the bounding box `a`.
 */
static inline bounds_t __attribute__ ((warn_unused_result)) Bounds_ClampBounds(const bounds_t a, const bounds_t b) {
	return Bounds(
		Vec3_Clamp(b.mins, a.mins, a.maxs),
		Vec3_Clamp(b.maxs, a.mins, a.maxs)
	);
}

/**
 * @return A random point within the bounding box `b`, edges inclusive.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Bounds_RandomPoint(const bounds_t b) {
	return Vec3_Mix3(b.mins, b.maxs, Vec3_Random());
}

/**
 * @return Whether `a` and `b` are equal or not.
 */
static inline _Bool __attribute__ ((warn_unused_result)) Bounds_Equal(const bounds_t a, const bounds_t b) {
	return Vec3_Equal(a.mins, b.mins) && Vec3_Equal(a.maxs, b.maxs);
}

/**
 * @return The symetrical extents of `bounds`
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Bounds_Extents(const bounds_t bounds) {
	return Vec3_Maxf(Vec3_Fabsf(bounds.mins), Vec3_Fabsf(bounds.maxs));
}

/**
 * @return The `bounds` scaled by `scale`.
 */
static inline bounds_t __attribute__ ((warn_unused_result)) Bounds_Scale(const bounds_t bounds, const float scale) {
	return Bounds(
		Vec3_Scale(bounds.mins, scale),
		Vec3_Scale(bounds.maxs, scale)
	);
}