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
} box3_t;

/**
 * @return A `box_t` with the specified mins and maxs.
 */
static inline box3_t __attribute__ ((warn_unused_result)) Box3(const vec3_t mins, const vec3_t maxs) {
	return (box3_t) {
		.mins = mins,
		.maxs = maxs
	};
}

/**
 * @return A bounding box constructed from a 3d size parameter. The box is constructed
 * such that its center is zero, and its size matches `size`.
 */
static inline box3_t __attribute__ ((warn_unused_result)) Box3fv(const vec3_t size) {
	return Box3(Vec3_Scale(size, -.5f), Vec3_Scale(size, .5f));
}

/**
 * @return A bounding box constructed from size values. The box is constructed
 * such that its center is zero, and its size matches { `x`, `y`, `z` }.
 */
static inline box3_t __attribute__ ((warn_unused_result)) Box3f(float x, float y, float z) {
	return Box3fv(Vec3(x, y, z));
}

/**
 * @return A `box_t` centered around `0, 0, 0` with a size of zero.
 */
static inline box3_t __attribute__ ((warn_unused_result)) Box3_Zero(void) {
	return (box3_t) {
		.mins = Vec3_Zero(),
		.maxs = Vec3_Zero()
	};
}

/**
 * @return A `box_t` centered around `0, 0, 0` with a size of -infinity.
 * @remarks This is often used as a seed value to add points to, as Box3_Zero()
 * will require that `0, 0, 0` be in the box, but the points added may not be centered
 * around that point. Using this as a seed guarantees that the first point added will
 * become the new seed.
 */
static inline box3_t __attribute__ ((warn_unused_result)) Box3_Null(void) {
	return (box3_t) {
		.mins = Vec3(INFINITY, INFINITY, INFINITY),
		.maxs = Vec3(-INFINITY, -INFINITY, -INFINITY)
	};
}

/**
 * @return A `box_t` that contains both the passed bounds as well as the passed
 * point.
 */
static inline box3_t __attribute__ ((warn_unused_result)) Box3_Append(const box3_t bounds, const vec3_t point) {
	return (box3_t) {
		.mins = Vec3_Minf(bounds.mins, point),
		.maxs = Vec3_Maxf(bounds.maxs, point)
	};
}

/**
 * @return A `box_t` that unions both of the passed bounds.
 */
static inline box3_t __attribute__ ((warn_unused_result)) Box3_Union(const box3_t a, const box3_t b) {
	return (box3_t) {
		.mins = Vec3_Minf(a.mins, b.mins),
		.maxs = Vec3_Maxf(a.maxs, b.maxs)
	};
}

/**
 * @return A `box_t` constructed from a set of points.
 */
static inline box3_t __attribute__ ((warn_unused_result)) Box3_FromPoints(const vec3_t *points, const size_t num_points) {
	box3_t bounds = Box3_Null();

	for (size_t i = 0; i < num_points; i++, points++) {
		bounds = Box3_Append(bounds, *points);
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
static inline void Box3_ToPoints(const box3_t bounds, vec3_t *points) {

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
static inline _Bool __attribute__ ((warn_unused_result)) Box3_Intersects(const box3_t a, const box3_t b) {

	return Vec3_BoxIntersect(a.mins, a.maxs, b.mins, b.maxs);
}

/**
 * @return `true` if `a` contains the point `b`, `false` otherwise.
*/
static inline _Bool __attribute__ ((warn_unused_result)) Box3_ContainsPoint(const box3_t a, const vec3_t b) {

	return Vec3_BoxIntersect(b, b, a.mins, a.maxs);
}

/**
 * @return `true` if `b` is fully contained within `a`, `false` otherwise.
 */
static inline _Bool __attribute__ ((warn_unused_result)) Box3_Contains(const box3_t a, const box3_t b) {

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
static inline vec3_t __attribute__ ((warn_unused_result)) Box3_Size(const box3_t a) {
	return Vec3_Subtract(a.maxs, a.mins);
}

/**
 * @return The box extents, or the half size.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Box3_Extents(const box3_t b) {
	return Vec3_Scale(Box3_Size(b), 0.5f);
}

/**
 * @return The distance between the two corners of the bounds.
 */
static inline float __attribute__ ((warn_unused_result)) Box3_Distance(const box3_t a) {
	return Vec3_Distance(a.maxs, a.mins);
}

/**
 * @return The radius of the bounds (equivalent to Box3_Distance(a) / 2). This is
 * a sphere that contains the whole box, including its corners.
 */
static inline float __attribute__ ((warn_unused_result)) Box3_Radius(const box3_t a) {
	return Box3_Distance(a) / 2.f;
}

/**
 * @return The center of the bounding box.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Box3_Center(const box3_t b) {
	return Vec3_Mix(b.mins, b.maxs, .5f);
}

/**
 * @return A bounding box centered around `center` with a size of zero.
 */
static inline box3_t __attribute__ ((warn_unused_result)) Box3_FromCenter(const vec3_t center) {
	return Box3(center, center);
}

/**
 * @return A bounding box constructed from a 3d size parameter. The box is constructed
 * such that its center is `center`, and its size matches `size`.
 */
static inline box3_t __attribute__ ((warn_unused_result)) Box3_FromCenterSize(const vec3_t center, const vec3_t size) {
	const vec3_t half_size = Vec3_Scale(size, .5f);
	return Box3(
		Vec3_Subtract(center, half_size),
		Vec3_Add(center, half_size)
	);
}

/**
 * @return A bounding box constructed from the spherical radius, translated by `center`.
 */
static inline box3_t __attribute__ ((warn_unused_result)) Box3_FromCenterRadius(const vec3_t center, float radius) {
	const vec3_t delta = Vec3(radius, radius, radius);
	return Box3(
		Vec3_Subtract(center, delta),
		Vec3_Add(center, delta)
	);
}

/**
 * @return A bounding box translated by the specified offset.
 */
static inline box3_t __attribute__ ((warn_unused_result)) Box3_Translate(const box3_t bounds, const vec3_t translation) {
	return Box3(
		Vec3_Add(bounds.mins, translation),
		Vec3_Add(bounds.maxs, translation)
	);
}

/**
 * @return A bounding box expanded (or shrunk, if a value is negative) by the
 * specified expansion values on all six axis.
 */
static inline box3_t __attribute__ ((warn_unused_result)) Box3_Expand3(const box3_t bounds, const vec3_t expansion) {
	return Box3(
		Vec3_Subtract(bounds.mins, expansion),
		Vec3_Add(bounds.maxs, expansion)
	);
}

/**
 * @return A bounding box expanded (or shrunk, if a value is negative) by the
 * specified expansion value on all six axis.
 */
static inline box3_t __attribute__ ((warn_unused_result)) Box3_Expand(const box3_t bounds, float expansion) {
	return Box3_Expand3(bounds, Vec3(expansion, expansion, expansion));
}

/**
 * @return The bounding box `a` expanded by the bounding box `b`.
 */
static inline box3_t __attribute__ ((warn_unused_result)) Box3_ExpandBox(const box3_t a, const box3_t b) {
	return Box3(
		Vec3_Add(a.mins, b.mins),
		Vec3_Add(a.maxs, b.maxs)
	);
}

/**
 * @return The point `p` clamped by the bounding box `b`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Box3_ClampPoint(const box3_t b, const vec3_t p) {
	return Vec3_Clamp(p, b.mins, b.maxs);
}

/**
 * @return The bounds `b` clamped by the bounding box `a`.
 */
static inline box3_t __attribute__ ((warn_unused_result)) Box3_ClampBounds(const box3_t a, const box3_t b) {
	return Box3(
		Vec3_Clamp(b.mins, a.mins, a.maxs),
		Vec3_Clamp(b.maxs, a.mins, a.maxs)
	);
}

/**
 * @return A random point within the bounding box `b`, edges inclusive.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Box3_RandomPoint(const box3_t b) {
	return Vec3_Mix3(b.mins, b.maxs, Vec3_Random());
}

/**
 * @return Whether `a` and `b` are equal or not.
 */
static inline _Bool __attribute__ ((warn_unused_result)) Box3_Equal(const box3_t a, const box3_t b) {
	return Vec3_Equal(a.mins, b.mins) && Vec3_Equal(a.maxs, b.maxs);
}

/**
 * @return The symetrical extents of `bounds`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Box3_Symetrical(const box3_t bounds) {
	return Vec3_Maxf(Vec3_Fabsf(bounds.mins), Vec3_Fabsf(bounds.maxs));
}

/**
 * @return The `bounds` scaled by `scale`.
 */
static inline box3_t __attribute__ ((warn_unused_result)) Box3_Scale(const box3_t bounds, float scale) {
	return Box3(
		Vec3_Scale(bounds.mins, scale),
		Vec3_Scale(bounds.maxs, scale)
	);
}
