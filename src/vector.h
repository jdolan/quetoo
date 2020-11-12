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
} vec2s_t;

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
} vec3s_t;

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
} vec3i_t;

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
} vec3d_t;

#pragma mark - short vectors

/**
 * @return A `vec2s_t` with the specified components.
 */
static inline vec2s_t Vec2s(int16_t x, int16_t y) __attribute__ ((warn_unused_result));

/**
 * @return The Vector `v` scaled by `scale`.
 */
static inline vec2s_t Vec2s_Scale(const vec2s_t v, float scale) __attribute__ ((warn_unused_result));

/**
 * @return The vector `(0, 0)`.
 */
static inline vec2s_t Vec2s_Zero(void) __attribute__ ((warn_unused_result));

/**
 * @return A `s16vec3_t` with the specified components.
 */
static inline vec3s_t Vec3s(int16_t x, int16_t y, int16_t z) __attribute__ ((warn_unused_result));

/**
 * @return The integer vector `v` cast to `vec3_t`.
 */
static inline vec3_t Vec3s_CastVec3(const vec3s_t v) __attribute__ ((warn_unused_result));

/**
 * @return True if `a` and `b` are equal.
 */
static inline _Bool Vec3s_Equal(const vec3s_t a, vec3s_t b) __attribute__ ((warn_unused_result));

/**
 * @return The vector `(0, 0, 0)`.
 */
static inline vec3s_t Vec3s_Zero(void) __attribute__ ((warn_unused_result));

#pragma mark - integer vectors

/**
 * @return A `s32vec3_t` with the specified components.
 */
static inline vec3i_t Vec3i(int32_t x, int32_t y, int32_t z) __attribute__ ((warn_unused_result));

/**
 * @return The sum of `a + b`.
 */
static inline vec3i_t Vec3i_Add(const vec3i_t a, const vec3i_t b) __attribute__ ((warn_unused_result));

/**
 * @return The vector `v` cast to `vec3_t`.
 */
static inline vec3_t Vec3i_CastVec3(const vec3i_t v) __attribute__ ((warn_unused_result));

/**
 * @return The vector `(0, 0, 0)`.
 */
static inline vec3i_t Vec3i_Zero(void) __attribute__ ((warn_unused_result));

#pragma mark - single precision

/**
 * @return The angle `theta` circularly clamped.
 */
static inline float ClampEuler(float theta) __attribute__ ((warn_unused_result));

/**
 * @return The value `f`, clamped to the specified `min` and `max`.
 */
static inline float Clampf(float f, float min, float max) __attribute__ ((warn_unused_result));

/**
 * @return The value `f`, clamped to 0.0 and 1.0.
 */
static inline float Clampf01(float f) __attribute__ ((warn_unused_result));

/**
 * @return Radians in degrees.
 */
static inline float Degrees(float radians) __attribute__ ((warn_unused_result));

/**
 * @return True if `fabsf(a - b) <= epsilon`.
 */
static inline _Bool EqualEpsilonf(float a, float b, float epsilon) __attribute__ ((warn_unused_result));

/**
 * @return The minimim of `a` and `b`.
 */
static inline float Minf(float a, float b) __attribute__ ((warn_unused_result));

/**
 * @return The minimim of `a` and `b`.
 */
static inline int32_t Mini(int32_t a, int32_t b) __attribute__ ((warn_unused_result));

/**
 * @return The minimum of `a`, `b`, and `c`.
 */
static inline float Minf3(float a, float b, float c) __attribute__ ((warn_unused_result));

/**
 * @return The minimum of `a`, `b`, `c`, and `d`.
 */
static inline float Minf4(float a, float b, float c, float d) __attribute__ ((warn_unused_result));

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
static inline float Mixf(float a, float b, float mix) __attribute__ ((warn_unused_result));

/**
 * @return The maximum of `a` and `b`.
 */
static inline float Maxf(float a, float b) __attribute__ ((warn_unused_result));

/**
 * @return The maximum of `a` and `b`.
 */
static inline int32_t Maxi(int32_t a, int32_t b) __attribute__ ((warn_unused_result));

/**
 * @return The maximum of `a`, `b`, and `c`.
 */
static inline float Maxf3(float a, float b, float c) __attribute__ ((warn_unused_result));

/**
 * @return The maximum of `a`, `b`, `c`, and `d`.
 */
static inline float Maxf4(float a, float b, float c, float d) __attribute__ ((warn_unused_result));

/**
 * @return Degrees in radians.
 */
static inline float Radians(float degrees) __attribute__ ((warn_unused_result));

/**
 * @return A psuedo random number between `begin` and `end`.
 */
static inline float RandomRangef(float begin, float end) __attribute__ ((warn_unused_result));

/**
 * @return A psuedo random integral value between `0` and `UINT_MAX`.
 */
static inline uint32_t Randomu(void) __attribute__ ((warn_unused_result));

/**
 * @return A psuedo random number between `begin` and `end`.
 */
static inline uint32_t RandomRangeu(uint32_t begin, uint32_t end) __attribute__ ((warn_unused_result));

/**
 * @return A psuedo random integral value between `INT_MIN` and `INT_MAX`.
 */
static inline int32_t Randomi(void) __attribute__ ((warn_unused_result));

/**
 * @return A psuedo random number between `begin` and `end`.
 */
static inline int32_t RandomRangei(int32_t begin, int32_t end) __attribute__ ((warn_unused_result));

/**
 * @return A psuedo random single precision value between `0.f` and `1.f`.
 */
static inline float Randomf(void) __attribute__ ((warn_unused_result));

/**
 * @return A psuedo random boolean.
 */
static inline _Bool Randomb(void) __attribute__ ((warn_unused_result));

/**
 * @brief Returns a random number between 0 and 2 pi.
 */
static inline float RandomRadian(void) __attribute__ ((warn_unused_result));

/**
 * @return The sign of the specified float.
 */
static inline int32_t SignOf(float f) __attribute__ ((warn_unused_result));

/**
 * @return The Hermite interpolation of `f`.
 */
static inline float Smoothf(float f, float min, float max) __attribute__ ((warn_unused_result));

#pragma mark - vec2_t

/**
 * @return A `vec2_t` with the specified components.
 */
static inline vec2_t Vec2(float x, float y) __attribute__ ((warn_unused_result));

/**
 * @return The vector sum of `a + b`.
 */
static inline vec2_t Vec2_Add(const vec2_t a, const vec2_t b) __attribute__ ((warn_unused_result));

/**
 * @return The length squared of the vector `a - b`.
 */
static inline float Vec2_DistanceSquared(const vec2_t a, const vec2_t b) __attribute__ ((warn_unused_result));

/**
 * @return The length of the vector `a - b`.
 */
static inline float Vec2_Distance(const vec2_t a, const vec2_t b) __attribute__ ((warn_unused_result));

/**
 * @return The dot product of `a · b`.
 */
static inline float Vec2_Dot(const vec2_t a, const vec2_t b) __attribute__ ((warn_unused_result));

/**
 * @return True if `a` and `b` are equal, using the specified epsilon.
 */
static inline _Bool Vec2_EqualEpsilon(const vec2_t a, const vec2_t b, float epsilon) __attribute__ ((warn_unused_result));

/**
 * @return True if `a` and `b` are equal.
 */
static inline _Bool Vec2_Equal(const vec2_t a, const vec2_t b) __attribute__ ((warn_unused_result));

/**
 * @return The squared length (magnitude) of `v`.
 */
static inline float Vec2_LengthSquared(const vec2_t v) __attribute__ ((warn_unused_result));

/**
 * @return The length (magnitude) of `v`.
 */
static inline float Vec2_Length(const vec2_t v) __attribute__ ((warn_unused_result));

/**
 * @return A vector containing the max components of `a` and `b`.
 */
static inline vec2_t Vec2_Maxf(const vec2_t a, const vec2_t b) __attribute__ ((warn_unused_result));

/**
 * @return The vector `(-FLT_MAX, -FLT_MAX)`.
 */
static inline vec2_t Vec2_Maxs(void) __attribute__ ((warn_unused_result));

/**
 * @return A vector containing the min components of `a` and `b`.
 */
static inline vec2_t Vec2_Minf(const vec2_t a, const vec2_t b) __attribute__ ((warn_unused_result));

/**
 * @return The vector `(FLT_MAX, FLT_MAX)`.
 */
static inline vec2_t Vec2_Mins(void) __attribute__ ((warn_unused_result));

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
static inline vec2_t Vec2_Mix(const vec2_t a, const vec2_t b, float mix) __attribute__ ((warn_unused_result));

/**
 * @return The vector `v` scaled by `scale`.
 */
static inline vec2_t Vec2_Scale(const vec2_t v, float scale) __attribute__ ((warn_unused_result));

/**
 * @return The difference of `a - b`.
 */
static inline vec2_t Vec2_Subtract(const vec2_t a, const vec2_t b) __attribute__ ((warn_unused_result));

/**
 * @return The zero vector.
 */
static inline vec2_t Vec2_Zero(void) __attribute__ ((warn_unused_result));

#pragma mark - vec3_t

/**
 * @return A `vec3_t` with the specified components.
 */
static inline vec3_t Vec3(float x, float y, float z) __attribute__ ((warn_unused_result));

/**
 * @return A vector containing the absolute values of `v`.
 */
static inline vec3_t Vec3_Absf(const vec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return The vector sum of `a + b`.
 */
static inline vec3_t Vec3_Add(const vec3_t a, const vec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return True if the specified boxes intersect, false otherwise.
 */
static inline int32_t Vec3_BoxIntersect(const vec3_t amins, const vec3_t amaxs, const vec3_t bmins, const vec3_t bmaxs) __attribute__ ((warn_unused_result));

/**
 * @return The vector `v` cast to `vec3d_t`.
 */
static inline vec3d_t Vec3_CastVec3d(const vec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return The vector `v` cast to `s16vec3_t`.
 */
static inline vec3s_t Vec3_CastVec3s(const vec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return The vector `v` cast to `s32vec3_t`.
 */
static inline vec3i_t Vec3_CastVec3i(const vec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return The specified Euler angles circularly clamped to `0.f - 360.f`.
 */
static inline vec3_t Vec3_ClampEuler(const vec3_t euler) __attribute__ ((warn_unused_result));

/**
 * @return The cross product of `a ✕ b`.
 */
static inline vec3_t Vec3_Cross(const vec3_t a, const vec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The vector `radians` converted to degrees.
 */
static inline vec3_t Vec3_Degrees(const vec3_t radians) __attribute__ ((warn_unused_result));

/**
 * @return The length of `a - b` as well as the normalized directional vector.
 */
static inline float Vec3_DistanceDir(const vec3_t a, const vec3_t b, vec3_t *dir) __attribute__ ((warn_unused_result));

/**
 * @return The squared length of the vector `a - b`.
 */
static inline float Vec3_DistanceSquared(const vec3_t a, const vec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The length of the vector `a - b`.
 */
static inline float Vec3_Distance(const vec3_t a, const vec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The dot product of `a · b`.
 */
static inline float Vec3_Dot(const vec3_t a, const vec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The down vector `(0.f, 0.f, -1.f)`.
 */
static inline vec3_t Vec3_Down(void) __attribute__ ((warn_unused_result));

/**
 * @return True if `a` and `b` are equal using the specified epsilon.
 */
static inline _Bool Vec3_EqualEpsilon(const vec3_t a, const vec3_t b, float epsilon) __attribute__ ((warn_unused_result));

/**
 * @return True if `a` and `b` are equal.
 */
static inline _Bool Vec3_Equal(const vec3_t a, const vec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The euler angles, in radians, for the directional vector `dir`.
 */
static inline vec3_t Vec3_Euler(const vec3_t dir) __attribute__ ((warn_unused_result));

/**
 * @return The squared length (magnitude) of `v`.
 */
static inline float Vec3_LengthSquared(const vec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return The length (magnitude) of `v`.
 */
static inline float Vec3_Length(const vec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return A vector containing the max components of `a` and `b`.
 */
static inline vec3_t Vec3_Maxf(const vec3_t a, const vec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The vector `(-FLT_MAX, -FLT_MAX)`.
 */
static inline vec3_t Vec3_Maxs(void) __attribute__ ((warn_unused_result));

/**
 * @return A vector containing the min components of `a` and `b`.
 */
static inline vec3_t Vec3_Minf(const vec3_t a, const vec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The vector `(FLT_MAX, FLT_MAX)`.
 */
static inline vec3_t Vec3_Mins(void) __attribute__ ((warn_unused_result));

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
static inline vec3_t Vec3_MixEuler(const vec3_t a, const vec3_t b, float mix) __attribute__ ((warn_unused_result));

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
static inline vec3_t Vec3_Mix(const vec3_t a, const vec3_t b, float mix) __attribute__ ((warn_unused_result));

/**
 * @return The linear interpolation of `a` and `b` using the specified fractions.
 */
static inline vec3_t Vec3_Mix3(const vec3_t a, const vec3_t b, const vec3_t mix) __attribute__ ((warn_unused_result));

/**
 * @return The product `a * b`.
 */
static inline vec3_t Vec3_Multiply(const vec3_t a, const vec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The negated vector `v`.
 */
static inline vec3_t Vec3_Negate(const vec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return The normalized vector `v`.
 */
static inline vec3_t Vec3_NormalizeLength(const vec3_t v, float *length) __attribute__ ((warn_unused_result));

/**
 * @return The normalized vector `v`.
 */
static inline vec3_t Vec3_Normalize(const vec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return The vector `(1.f, 1.f, 1.f)`.
 */
static inline vec3_t Vec3_One(void) __attribute__ ((warn_unused_result));

/**
 * @return The vector `degrees` in radians.
 */
static inline vec3_t Vec3_Radians(const vec3_t degrees) __attribute__ ((warn_unused_result));

/**
 * @return A vector with random values between `0` and `1`.
 */
static inline vec3_t Vec3_Random(void) __attribute__ ((warn_unused_result));

/**
 * @return A vector with random values between `begin` and `end`.
 */
static inline vec3_t Vec3_RandomRange(float begin, float end) __attribute__ ((warn_unused_result));

/**
 * @return Returns a random vector (positive or negative) with a length of 1.
 */
static inline vec3_t Vec3_RandomDir(void) __attribute__ ((warn_unused_result));

/**
 * @return Takes a vector and randomizes its direction where a `random` of 0 yields the original vector and 1 yields a completely random direction.
 */
static inline vec3_t Vec3_RandomizeDir(const vec3_t vector, const float random) __attribute__ ((warn_unused_result));

/**
 * @return The vector `v` rounded to the nearest integer values.
 */
static inline vec3_t Vec3_Roundf(const vec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return The vector `v` scaled by `scale`.
 */
static inline vec3_t Vec3_Scale(const vec3_t v, float scale) __attribute__ ((warn_unused_result));

/**
 * @return
 */
static inline vec3_t Vec3_Clamp(const vec3_t v, vec3_t min, vec3_t max) __attribute__ ((warn_unused_result));

/**
 * @return
 */
static inline vec3_t Vec3_Clampf(const vec3_t v, float min, float max) __attribute__ ((warn_unused_result));

/**
 * @return
 */
static inline vec3_t Vec3_Clamp01(const vec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return Reflected vector
 */
static inline vec3_t Vec3_Reflect(const vec3_t a, const vec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The difference of `a - b`.
 */
static inline vec3_t Vec3_Subtract(const vec3_t a, const vec3_t b) __attribute__ ((warn_unused_result));

/**
 * @return The tangent and bitangent vectors for the given normal and texture directional vectors.
 */
static inline void Vec3_Tangents(const vec3_t normal, const vec3_t sdir, const vec3_t tdir, vec3_t *tangent, vec3_t *bitangent);

/**
 * @return A `vec4_t` comprised of the specified `vec3_t` and `w`.
 */
static inline vec4_t Vec3_ToVec4(const vec3_t v, float w) __attribute__ ((warn_unused_result));

/**
 * @return The up vector `(0.f, 0.f, 1.f)`.
 */
static inline vec3_t Vec3_Up(void) __attribute__ ((warn_unused_result));

/**
 * @return The forward, right and up vectors for the euler angles in radians.
 */
static inline void Vec3_Vectors(const vec3_t euler, vec3_t *forward, vec3_t *right, vec3_t *up);

/**
 * @return The `xy` swizzle of `v`.
 */
static inline vec2_t Vec3_XY(const vec3_t v) __attribute__ ((warn_unused_result));

/**
 * @return The vector `(0.f, 0.f, 0.f)`.
 */
static inline vec3_t Vec3_Zero(void) __attribute__ ((warn_unused_result));

#pragma mark - vec4_t

/**
 * @return A `vec4_t` with the specified components.
 */
static inline vec4_t Vec4(float x, float y, float z, float w) __attribute__ ((warn_unused_result));

/**
 * @return The sub of `a + b`.
 */
static inline vec4_t Vec4_Add(const vec4_t a, const vec4_t b) __attribute__ ((warn_unused_result));

/**
 * @return The sub of `a + b`.
 */
static inline vec4_t Vec4_Subtract(const vec4_t a, const vec4_t b) __attribute__ ((warn_unused_result));

/**
 * @return The sub of `a + b`.
 */
static inline vec4_t Vec4_Multiply(const vec4_t a, const vec4_t b) __attribute__ ((warn_unused_result));

/**
 * @return The negated vector `v`.
 */
static inline vec4_t Vec4_Negate(const vec4_t v) __attribute__ ((warn_unused_result));

/**
 * @return True if `a` and `b` are equal using the specified epsilon.
 */
static inline _Bool Vec4_EqualEpsilon(const vec4_t a, const vec4_t b, float epsilon) __attribute__ ((warn_unused_result));

/**
 * @return True if `a` and `b` are equal.
 */
static inline _Bool Vec4_Equal(const vec4_t a, const vec4_t b) __attribute__ ((warn_unused_result));

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
static inline vec4_t Vec4_Mix(const vec4_t a, const vec4_t b, float mix) __attribute__ ((warn_unused_result));

/**
 * @return The vector `(1.f, 1f., 1.f)`.
 */
static inline vec4_t Vec4_One(void) __attribute__ ((warn_unused_result));

/**
 * @return A vector with random values between `begin` and `end`.
 */
static inline vec4_t Vec4_RandomRange(float begin, float end) __attribute__ ((warn_unused_result));

/**
 * @return A vevtor with random values between `0` and `1`.
 */
static inline vec4_t Vec4_Random(void) __attribute__ ((warn_unused_result));

/**
 * @return The vector `v` scaled by `scale`.
 */
static inline vec4_t Vec4_Scale(const vec4_t v, float scale) __attribute__ ((warn_unused_result));

/**
 * @return The difference of `a - b`.
 */
static inline vec4_t Vec4_Subtract(const vec4_t a, const vec4_t b) __attribute__ ((warn_unused_result));

/**
 * @return The xyz swizzle of `v`.
 */
static inline vec3_t Vec4_XYZ(const vec4_t v) __attribute__ ((warn_unused_result));

/**
 * @return The vector `(0.f, 0.f, 0.f, 0.f)`.
 */
static inline vec4_t Vec4_Zero(void) __attribute__ ((warn_unused_result));

#pragma mark - double precision

/**
 * @return True if `fabs(a - b) <= epsilon`.
 */
static inline _Bool EqualEpsilon(double a, double b, double epsilon) __attribute__ ((warn_unused_result));

#pragma mark - vec3d_t

/**
 * @return A `vec3d_t` with the specified components.
 */
static inline vec3d_t Vec3d(double x, double y, double z) __attribute__ ((warn_unused_result));

/**
 * @return The vector `v` cast to single precision.
 */
static inline vec3_t Vec3d_CastVec3(const vec3d_t v) __attribute__ ((warn_unused_result));

/**
 * @return The vector sum of `a + b`.
 */
static inline vec3d_t Vec3d_Add(const vec3d_t a, const vec3d_t b) __attribute__ ((warn_unused_result));

/**
 * @return The cross product of `a ✕ b`.
 */
static inline vec3d_t Vec3d_Cross(const vec3d_t a, const vec3d_t b) __attribute__ ((warn_unused_result));

/**
 * @return The squared length of the vector `a - b`.
 */
static inline double Vec3d_DistanceSquared(const vec3d_t a, const vec3d_t b) __attribute__ ((warn_unused_result));

/**
 * @return The length of the vector `a - b`.
 */
static inline double Vec3d_Distance(const vec3d_t a, const vec3d_t b) __attribute__ ((warn_unused_result));

/**
 * @return The dot product of `a · b`.
 */
static inline double Vec3d_Dot(const vec3d_t a, const vec3d_t b) __attribute__ ((warn_unused_result));

/**
 * @return True if `a` and `b` are equal using the specified epsilon.
 */
static inline _Bool Vec3d_EqualEpsilon(const vec3d_t a, const vec3d_t b, double epsilon) __attribute__ ((warn_unused_result));

/**
 * @return True if `a` and `b` are equal.
 */
static inline _Bool Vec3d_Equal(const vec3d_t a, const vec3d_t b) __attribute__ ((warn_unused_result));

/**
 * @return The squared length (magnitude) of `v`.
 */
static inline double Vec3d_LengthSquared(const vec3d_t v) __attribute__ ((warn_unused_result));

/**
 * @return The length (magnitude) of `v`.
 */
static inline double Vec3d_Length(const vec3d_t v) __attribute__ ((warn_unused_result));

/**
 * @return The normalized vector `v`.
 */
static inline vec3d_t Vec3d_Normalize(const vec3d_t v) __attribute__ ((warn_unused_result));

/**
 * @return The vector `v` scaled by `scale`.
 */
static inline vec3d_t Vec3d_Scale(const vec3d_t v, double scale) __attribute__ ((warn_unused_result));

/**
 * @return The difference of `a - b`.
 */
static inline vec3d_t Vec3d_Subtract(const vec3d_t a, const vec3d_t b) __attribute__ ((warn_unused_result));

/**
 * @return The vector `(0., 0., 0.)`.
 */
static inline vec3d_t Vec3d_Zero(void) __attribute__ ((warn_unused_result));

#include "vector.c"
