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

#include "quetoo.h"

/**
 * @brief Two component signed short vector type.
 */
typedef union {
	/**
	 * @brief Array accessor.
	 */
	int16_t xy[2];

	/**
	 * @brief Component accessors.
	 */
	struct {
		int16_t x, y;
	};
} s16vec2_t;

/**
 * @brief Three component signed short vector type.
 */
typedef union {
	/**
	 * @brief Array accessor.
	 */
	int16_t xyz[3];

	/**
	 * @brief Component accessors.
	 */
	struct {
		int16_t x, y, z;
	};
} s16vec3_t;

/**
 * @brief Three component signed integer vector type.
 */
typedef union {
	/**
	 * @brief Array accessor.
	 */
	int32_t xyz[3];

	/**
	 * @brief Component accessors.
	 */
	struct {
		int32_t x, y, z;
	};
} s32vec3_t;

/**
 * @brief Two component single precision vector type.
 */
typedef union {
	/**
	 * @brief Array accessor.
	 */
	float xy[2];

	/**
	 * @brief Component accessors.
	 */
	struct {
		float x, y;
	};
} vec2_t;

/**
 * @brief Three component single precision vector type.
 */
typedef union {
	/**
	 * @brief Array accessor.
	 */
	float xyz[3];

	/**
	 * @brief Component accessors.
	 */
	struct {
		float x, y, z;
	};
} vec3_t;

/**
 * @brief Four component single precision vector type.
 */
typedef union {
	/**
	 * @brief Array accessor.
	 */
	float xyzw[4];

	/**
	 * @brief Component accessors.
	 */
	struct {
		float x, y, z, w;
	};
} vec4_t;

/**
 * @brief Three component double precision vector type.
 */
typedef union {
	/**
	 * @brief Array accessor.
	 */
	double xyz[3];

	/**
	 * @brief Component accessors.
	 */
	struct {
		double x, y, z;
	};
} dvec3_t;

#pragma mark - short integer vectors

/**
 * @return A `s16vec2_t` with the specified components.
 */
s16vec2_t s16vec2(int16_t x, int16_t y);

/**
 * @return The vector `(0, 0)`.
 */
s16vec2_t s16vec2_zero(void);

/**
 * @return A `s16vec3_t` with the specified components.
 */
s16vec3_t s16vec3(int16_t x, int16_t y, int16_t z);

/**
 * @return The integer vector `v` cast to `vec3_t`.
 */
vec3_t s16vec3_cast_vec3(const s16vec3_t v);

/**
 * @return True if `a` and `b` are equal.
 */
_Bool s16vec3_equal(const s16vec3_t a, s16vec3_t b);

/**
 * @return The vector `(0, 0, 0)`.
 */
s16vec3_t s16vec3_zero(void);

#pragma mark - integer vectors

/**
 * @return A `s32vec3_t` with the specified components.
 */
s32vec3_t s32vec3(int32_t x, int32_t y, int32_t z);

/**
 * @return The sum of `a + b`.
 */
s32vec3_t s32vec3_add(const s32vec3_t a, const s32vec3_t b);

/**
 * @return The vector `v` cast to `vec3_t`.
 */
vec3_t s32vec3_cast_vec3(const s32vec3_t v);

/**
 * @return The vector `(0, 0, 0)`.
 */
s32vec3_t s32vec3_zero(void);

#pragma mark - single precision

/**
 * @return The value `f`, clamped to the specified `min` and `max`.
 */
float clampf(float f, float min, float max) __attribute__ ((warn_unused_result));

/**
 * @return Radians in degrees.
 */
float degrees(float radians) __attribute__ ((warn_unused_result));

/**
 * @return True if `fabsf(a - b) <= epsilon`.
 */
_Bool equalf_epsilon(float a, float b, float epsilon) __attribute__ ((warn_unused_result));

/**
 * @return The minimim of `a` and `b`.
 */
float minf(float a, float b) __attribute__ ((warn_unused_result));

/**
 * @return The maximum of `a` and `b`.
 */
float maxf(float a, float b) __attribute__ ((warn_unused_result));

/**
 * @return Degrees in radians.
 */
float radians(float degrees) __attribute__ ((warn_unused_result));

/**
 * @return A psuedo random number between `begin` and `end`.
 */
float random_range(float begin, float end) __attribute__ ((warn_unused_result));

/**
 * @return The sign of the specified float.
 */
int32_t signf(float f) __attribute__ ((warn_unused_result));

/**
 * @return The Hermite interpolation of `f`.
 */
float smoothf(float f, float min, float max) __attribute__ ((warn_unused_result));

#pragma mark - vec2_t

/**
 * @return A `vec2_t` with the specified components.
 */
vec2_t vec2(float x, float y) __attribute__ ((warn_unused_result));

/**
 * @return The vector sum of `a + b`.
 */
vec2_t vec2_add(const vec2_t a, const vec2_t b) __attribute__ ((warn_unused_result));

/**
 * @return The length squared of the vector `a - b`.
 */
float vec2_distance_squared(const vec2_t a, const vec2_t b) __attribute__ ((warn_unused_result));

/**
 * @return The length of the vector `a - b`.
 */
float vec2_distance(const vec2_t a, const vec2_t b) __attribute__ ((warn_unused_result));

/**
 * @return The dot product of `a · b`.
 */
float vec2_dot(const vec2_t a, const vec2_t b) __attribute__ ((warn_unused_result));

/**
 * @return True if `a` and `b` are equal, using the specified epsilon.
 */
_Bool vec2_equal_epsilon(const vec2_t a, const vec2_t b, float epsilon) __attribute__ ((warn_unused_result));

/**
 * @return True if `a` and `b` are equal.
 */
_Bool vec2_equal(const vec2_t a, const vec2_t b) __attribute__ ((warn_unused_result));

/**
 * @return The squared length (magnitude) of `v`.
 */
float vec2_length_squared(const vec2_t v) __attribute__ ((warn_unused_result));

/**
 * @return The length (magnitude) of `v`.
 */
float vec2_length(const vec2_t v) __attribute__ ((warn_unused_result));

/**
 * @return A vector containing the max components of `a` and `b`.
 */
vec2_t vec2_maxf(const vec2_t a, const vec2_t b) __attribute__ ((warn_unused_result));

/**
 * @return The vector `(-FLT_MAX, -FLT_MAX)`.
 */
vec2_t vec2_maxs(void) __attribute__ ((warn_unused_result));

/**
 * @return A vector containing the min components of `a` and `b`.
 */
vec2_t vec2_minf(const vec2_t a, const vec2_t b) __attribute__ ((warn_unused_result));

/**
 * @return The vector `(FLT_MAX, FLT_MAX)`.
 */
vec2_t vec2_mins(void) __attribute__ ((warn_unused_result));

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
vec2_t vec2_mix(const vec2_t a, const vec2_t b, float mix) __attribute__ ((warn_unused_result));

/**
 * @return The vector `v` scaled by `scale`.
 */
vec2_t vec2_scale(const vec2_t v, float scale) __attribute__ ((warn_unused_result));

/**
 * @return The difference of `a - b`.
 */
vec2_t vec2_subtract(const vec2_t a, const vec2_t b) __attribute__ ((warn_unused_result));

/**
 * @return The zero vector.
 */
vec2_t vec2_zero(void) __attribute__ ((warn_unused_result));

#pragma mark - vec3_t

/**
 * @return A `vec3_t` with the specified components.
 */
vec3_t vec3(float x, float y, float z) __attribute__ ((warn_unused_result));

/**
 * @return A vector containing the absolute values of `v`.
 */
vec3_t vec3_abs(const vec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return The vector sum of `a + b`.
 */
vec3_t vec3_add(const vec3_t a, const vec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return True if the specified boxes intersect, false otherwise.
 */
_Bool vec3_box_intersect(const vec3_t amins, const vec3_t amaxs, const vec3_t bmins, const vec3_t bmaxs) __attribute__ ((warn_unused_result));

/**
 * @return The vector `v` cast to `dvec3_t`.
 */
dvec3_t vec3_cast_dvec3(const vec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return The vector `v` cast to `s16vec3_t`.
 */
s16vec3_t vec3_cast_s16vec3(const vec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return The vector `v` cast to `s32vec3_t`.
 */
s32vec3_t vec3_cast_s32vec3(const vec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return The cross product of `a ✕ b`.
 */
vec3_t vec3_cross(const vec3_t a, const vec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The vector `radians` converted to degrees.
 */
vec3_t vec3_degrees(const vec3_t radians) __attribute__ ((warn_unused_result));

/**
 * @return The length of `a - b` as well as the normalized directional vector.
 */
float vec3_distance_dir(const vec3_t a, const vec3_t b, vec3_t *dir) __attribute__ ((warn_unused_result));

/**
 * @return The squared length of the vector `a - b`.
 */
float vec3_distance_squared(const vec3_t a, const vec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The length of the vector `a - b`.
 */
float vec3_distance(const vec3_t a, const vec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The dot product of `a · b`.
 */
float vec3_dot(const vec3_t a, const vec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The down vector `(0.f, 0.f, -1.f)`.
 */
vec3_t vec3_down(void) __attribute__ ((warn_unused_result));

/**
 * @return True if `a` and `b` are equal using the specified epsilon.
 */
_Bool vec3_equal_epsilon(const vec3_t a, const vec3_t b, float epsilon) __attribute__ ((warn_unused_result));

/**
 * @return True if `a` and `b` are equal.
 */
_Bool vec3_equal(const vec3_t a, const vec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The euler angles, in radians, for the directional vector `dir`.
 */
vec3_t vec3_euler(const vec3_t dir) __attribute__ ((warn_unused_result));

/**
 * @return The forward directional vector for the euler angles, in radians.
 */
vec3_t vec3_forward(const vec3_t euler) __attribute__ ((warn_unused_result));

/**
 * @return The squared length (magnitude) of `v`.
 */
float vec3_length_squared(const vec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return The length (magnitude) of `v`.
 */
float vec3_length(const vec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return A vector containing the max components of `a` and `b`.
 */
vec3_t vec3_maxf(const vec3_t a, const vec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The vector `(-FLT_MAX, -FLT_MAX)`.
 */
vec3_t vec3_maxs(void) __attribute__ ((warn_unused_result));

/**
 * @return A vector containing the min components of `a` and `b`.
 */
vec3_t vec3_minf(const vec3_t a, const vec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The vector `(FLT_MAX, FLT_MAX)`.
 */
vec3_t vec3_mins(void) __attribute__ ((warn_unused_result));

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
vec3_t vec3_mix_euler(const vec3_t a, const vec3_t b, float mix) __attribute__ ((warn_unused_result));

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
vec3_t vec3_mix(const vec3_t a, const vec3_t b, float mix) __attribute__ ((warn_unused_result));

/**
 * @return The product `a * b`.
 */
vec3_t vec3_multiply(const vec3_t a, const vec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The negated vector `v`.
 */
vec3_t vec3_negate(const vec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return The normalized vector `v`.
 */
vec3_t vec3_normalize_length(const vec3_t v, float *length) __attribute__ ((warn_unused_result));

/**
 * @return The normalized vector `v`.
 */
vec3_t vec3_normalize(const vec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return The vector `(1.f, 1.f, 1.f)`.
 */
vec3_t vec3_one(void) __attribute__ ((warn_unused_result));

/**
 * @return The vector `degrees` in radians.
 */
vec3_t vec3_radians(const vec3_t degrees) __attribute__ ((warn_unused_result));

/**
 * @return A vector with random values between `begin` and `end`.
 */
vec3_t vec3_random_range(float begin, float end);

/**
 * @return A vector with random values between `0` and `1`.
 */
vec3_t vec3_random(void);

/**
 * @return The vector `v` scaled by `scale`.
 */
vec3_t vec3_scale(const vec3_t v, float scale) __attribute__ ((warn_unused_result));

/**
 * @return The difference of `a - b`.
 */
vec3_t vec3_subtract(const vec3_t a, const vec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The tangent and bitangent vectors for the given normal and texture directional vectors.
 */
void vec3_tangents(const vec3_t normal, const vec3_t sdir, const vec3_t tdir, vec3_t *tangent, vec3_t *bitangent);

/**
 * @return A `vec4_t` comprised of the specified `vec3_t` and `w`.
 */
vec4_t vec3_to_vec4(const vec3_t v, float w);

/**
 * @return The up vector `(0.f, 0.f, 1.f)`.
 */
vec3_t vec3_up(void) __attribute__ ((warn_unused_result));

/**
 * @return The forward, right and up vectors for the euler angles in radians.
 */
void vec3_vectors(const vec3_t euler, vec3_t *forward, vec3_t *right, vec3_t *up);

/**
 * @return The `xy` swizzle of `v`.
 */
vec2_t vec3_xy(const vec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return The vector `(0.f, 0.f, 0.f)`.
 */
vec3_t vec3_zero(void) __attribute__ ((warn_unused_result));

#pragma mark - vec4_t

/**
 * @return A `vec4_t` with the specified components.
 */
vec4_t vec4(float x, float y, float z, float w) __attribute__ ((warn_unused_result));

/**
 * @return The sub of `a + b`.
 */
vec4_t vec4_add(const vec4_t a, const vec4_t b) __attribute__ ((warn_unused_result));

/**
 * @return True if `a` and `b` are equal using the specified epsilon.
 */
_Bool vec4_equal_epsilon(const vec4_t a, const vec4_t b, float epsilon) __attribute__ ((warn_unused_result));

/**
 * @return True if `a` and `b` are equal.
 */
_Bool vec4_equal(const vec4_t a, const vec4_t b) __attribute__ ((warn_unused_result));

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
vec4_t vec4_mix(const vec4_t a, const vec4_t b, float mix) __attribute__ ((warn_unused_result));

/**
 * @return The vector `(1.f, 1f., 1.f)`.
 */
vec4_t vec4_one(void) __attribute__ ((warn_unused_result));

/**
 * @return The vector `v` scaled by `scale`.
 */
vec4_t vec4_scale(const vec4_t v, float scale) __attribute__ ((warn_unused_result));

/**
 * @return The difference of `a - b`.
 */
vec4_t vec4_subtract(const vec4_t a, const vec4_t b) __attribute__ ((warn_unused_result));

/**
 * @return The xyz swizzle of `v`.
 */
vec3_t vec4_xyz(const vec4_t v) __attribute__ ((warn_unused_result));

/**
 * @return The vector `(0.f, 0.f, 0.f, 0.f)`.
 */
vec4_t vec4_zero(void) __attribute__ ((warn_unused_result));

#pragma mark - double precision

/**
 * @return True if `fabs(a - b) <= epsilon`.
 */
_Bool equal_epsilon(double a, double b, double epsilon) __attribute__ ((warn_unused_result));

#pragma mark - dvec3_t

/**
 * @return A `dvec3_t` with the specified components.
 */
dvec3_t dvec3(double x, double y, double z) __attribute__ ((warn_unused_result));

/**
 * @return The vector `v` cast to single precision.
 */
vec3_t dvec3_cast_vec3(const dvec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return The vector sum of `a + b`.
 */
dvec3_t dvec3_add(const dvec3_t a, const dvec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The cross product of `a ✕ b`.
 */
dvec3_t dvec3_cross(const dvec3_t a, const dvec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The squared length of the vector `a - b`.
 */
double dvec3_distance_squared(const dvec3_t a, const dvec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The length of the vector `a - b`.
 */
double dvec3_distance(const dvec3_t a, const dvec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The dot product of `a · b`.
 */
double dvec3_dot(const dvec3_t a, const dvec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return True if `a` and `b` are equal using the specified epsilon.
 */
_Bool dvec3_equal_epsilon(const dvec3_t a, const dvec3_t b, double epsilon) __attribute__ ((warn_unused_result));

/**
 * @return True if `a` and `b` are equal.
 */
_Bool dvec3_equal(const dvec3_t a, const dvec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The squared length (magnitude) of `v`.
 */
double dvec3_length_squared(const dvec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return The length (magnitude) of `v`.
 */
double dvec3_length(const dvec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return The normalized vector `v`.
 */
dvec3_t dvec3_normalize(const dvec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return The vector `v` scaled by `scale`.
 */
dvec3_t dvec3_scale(const dvec3_t v, double scale) __attribute__ ((warn_unused_result));

/**
 * @return The difference of `a - b`.
 */
dvec3_t dvec3_subtract(const dvec3_t a, const dvec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The vector `(0., 0., 0.)`.
 */
dvec3_t dvec3_zero(void) __attribute__ ((warn_unused_result));
