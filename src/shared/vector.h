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
 * @brief Four component signed integer vector type.
 */
typedef union {
	/**
	 * @brief Array accessor.
	 */
	int32_t xyzw[4];

	/**
	 * @brief Component accessors.
	 */
	struct {
		int32_t x, y, z, w;
	};
} vec4i_t;

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
static inline vec2s_t __attribute__ ((warn_unused_result)) Vec2s(int16_t x, int16_t y) {
	return (vec2s_t) {
		.x = x,
		.y = y
	};
}

/**
 * @return The Vector `v` scaled by `scale`.
 */
static inline vec2s_t __attribute__ ((warn_unused_result)) Vec2s_Scale(const vec2s_t v, float scale) {
	return Vec2s((int16_t) v.x * scale, (int16_t) v.y * scale);
}

/**
 * @return The vector `(0, 0)`.
 */
static inline vec2s_t __attribute__ ((warn_unused_result)) Vec2s_Zero(void) {
	return Vec2s(0, 0);
}

/**
 * @return A `s16vec3_t` with the specified components.
 */
static inline vec3s_t __attribute__ ((warn_unused_result)) Vec3s(int16_t x, int16_t y, int16_t z) {
	return (vec3s_t) {
		.x = x,
		.y = y,
		.z = z
	};
}

/**
 * @return The integer vector `v` cast to `vec3_t`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3s_CastVec3(const vec3s_t v) {
	return (vec3_t) {
		.x = (float) v.x,
		.y = (float) v.y,
		.z = (float) v.z
	};
}

/**
 * @return True if `a` and `b` are equal.
 */
static inline _Bool __attribute__ ((warn_unused_result)) Vec3s_Equal(const vec3s_t a, vec3s_t b) {
	return a.x == b.x &&
		   a.y == b.y &&
		   a.z == b.z;
}

/**
 * @return The vector `(0, 0, 0)`.
 */
static inline vec3s_t __attribute__ ((warn_unused_result)) Vec3s_Zero(void) {
	return Vec3s(0, 0, 0);
}

#pragma mark - integer vectors

/**
 * @return A `vec3i_t` with the specified components.
 */
static inline vec3i_t __attribute__ ((warn_unused_result)) Vec3i(int32_t x, int32_t y, int32_t z) {
	return (vec3i_t) {
		.x = x,
		.y = y,
		.z = z
	};
}

/**
 * @return The sum of `a + b`.
 */
static inline vec3i_t __attribute__ ((warn_unused_result)) Vec3i_Add(const vec3i_t a, const vec3i_t b) {
	return Vec3i(a.x + b.x, a.y + b.y, a.z + b.z);
}

/**
 * @return The vector `(0, 0, 0)`.
 */
static inline vec3i_t __attribute__ ((warn_unused_result)) Vec3i_Zero(void) {
	return Vec3i(0, 0, 0);
}

/**
 * @return A `vec4i_t` with the specified components.
 */
static inline vec4i_t __attribute__ ((warn_unused_result)) Vec4i(int32_t x, int32_t y, int32_t z, int32_t w) {
	return (vec4i_t) {
		.x = x,
		.y = y,
		.z = z,
		.w = w
	};
}

/**
 * @return The vector `(0, 0, 0, 0)`.
 */
static inline vec4i_t __attribute__ ((warn_unused_result)) Vec4i_Zero(void) {
	return Vec4i(0, 0, 0, 0);
}

#pragma mark - single precision

/**
 * @brief
 */
static inline float __attribute__ ((warn_unused_result)) AngleMod(float a) {
	a = fmodf(a, 360.f);

	if (a < 0) {
		return a + (((int32_t)(a / 360.f) + 1) * 360.f);
	}

	return a;
}

/**
 * @return The minimum of `a` and `b`.
 */
static inline float __attribute__ ((warn_unused_result)) Minf(float a, float b) {
	return a < b ? a : b;
}

/**
 * @return The minimum of `a` and `b`.
 */
static inline int32_t __attribute__ ((warn_unused_result)) Mini(int32_t a, int32_t b) {
	return a < b ? a : b;
}

/**
 * @return The minimum of `a` and `b`.
 */
static inline int64_t __attribute__ ((warn_unused_result)) Minui64(uint64_t a, uint64_t b) {
	return a < b ? a : b;
}

/**
 * @return The maximum of `a` and `b`.
 */
static inline float __attribute__ ((warn_unused_result)) Maxf(float a, float b) {
	return a > b ? a : b;
}

/**
 * @return The maximum of `a` and `b`.
 */
static inline int32_t __attribute__ ((warn_unused_result)) Maxi(int32_t a, int32_t b) {
	return a > b ? a : b;
}

/**
 * @return The maximum of `a` and `b`.
 */
static inline int64_t __attribute__ ((warn_unused_result)) Maxui64(uint64_t a, uint64_t b) {
	return a > b ? a : b;
}

/**
 * @return The value `f`, clamped to the specified `min` and `max`.
 */
static inline float __attribute__ ((warn_unused_result)) Clampf(float f, float min, float max) {
	return Minf(Maxf(f, min), max);
}

/**
 * @return The value `f`, clamped to 0.0 and 1.0.
 */
static inline float __attribute__ ((warn_unused_result)) Clampf01(float f) {
	return Minf(Maxf(f, 0.f), 1.f);
}

#define DegreesScalar ((float) (180.0 / M_PI))
#define RadiansScalar ((float) (M_PI / 180.0))

/**
 * @return Radians in degrees.
 */
static inline float __attribute__ ((warn_unused_result)) Degrees(float radians) {
	return radians * DegreesScalar;
}

/**
 * @return True if `fabsf(a - b) <= epsilon`.
 */
static inline _Bool __attribute__ ((warn_unused_result)) EqualEpsilonf(float a, float b, float epsilon) {
	return fabsf(a - b) <= epsilon;
}

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
static inline float __attribute__ ((warn_unused_result)) Mixf(float a, float b, float mix) {
	return a * (1.f - mix) + b * mix;
}

/**
 * @return Degrees in radians.
 */
static inline float __attribute__ ((warn_unused_result)) Radians(float degrees) {
	return degrees * RadiansScalar;
}

/**
 * @brief
 */
static inline float __attribute__ ((warn_unused_result)) SmallestAngleBetween(const float x, const float y) {
	return Minf(360.f - fabsf(x - y), fabsf(x - y));
}

/**
 * @return The angle `theta` circularly clamped.
 */
static inline float __attribute__ ((warn_unused_result)) ClampEuler(float theta) {
	while (theta >= 360.f) {
		theta -= 360.f;
	}
	while (theta < 0.f) {
		theta += 360.f;
	}
	return theta;
}


/**
 * @return A random number generator.
 */
static inline GRand *InitRandom(void) {
	static _Thread_local GRand *rand;

	if (rand == NULL) {
		rand = g_rand_new_with_seed((guint32) time(NULL));
	}

	return rand;
}

/**
 * @return A psuedo random number between `begin` and `end`.
 */
static inline float __attribute__ ((warn_unused_result)) RandomRangef(float begin, float end) {
	return (float) g_rand_double_range(InitRandom(), (gdouble) begin, (gdouble) end);
}

/**
 * @return A psuedo random integral value between `0` and `UINT_MAX`.
 */
static inline uint32_t __attribute__ ((warn_unused_result)) Randomu(void) {
	return g_rand_int(InitRandom());
}

#define INT32_TO_UINT32(x) (uint32_t) ((int64_t) ((x) - INT32_MIN))
#define UINT32_TO_INT32(x)  (int32_t) ((int64_t) ((x) + INT32_MIN))

/**
 * @return A psuedo random number between `begin` and `end`.
 */
static inline uint32_t __attribute__ ((warn_unused_result)) RandomRangeu(uint32_t begin, uint32_t end) {
	return INT32_TO_UINT32(g_rand_int_range(InitRandom(), UINT32_TO_INT32(begin), UINT32_TO_INT32(end)));
}

/**
 * @return A psuedo random integral value between `INT_MIN` and `INT_MAX`.
 */
static inline int32_t __attribute__ ((warn_unused_result)) Randomi(void) {
	return UINT32_TO_INT32(g_rand_int(InitRandom()));
}

/**
 * @return A psuedo random number between `begin` and `end`.
 */
static inline int32_t __attribute__ ((warn_unused_result)) RandomRangei(int32_t begin, int32_t end) {
	return g_rand_int_range(InitRandom(), begin, end);
}

/**
 * @return A psuedo random single precision value between `0.f` and `1.f`.
 */
static inline float __attribute__ ((warn_unused_result)) Randomf(void) {
	return (float) g_rand_double(InitRandom());
}

/**
 * @return A psuedo random boolean.
 */
static inline _Bool __attribute__ ((warn_unused_result)) Randomb(void) {
	return !!(g_rand_int(InitRandom()) & 1);
}

/**
 * @brief Returns a random number between 0 and 2 pi.
 */
static inline float __attribute__ ((warn_unused_result)) RandomRadian(void) {
	return Randomf() * (float) (M_PI * 2.0);
}

/**
 * @return The sign of the specified float.
 */
static inline int32_t __attribute__ ((warn_unused_result)) SignOf(float f) {
	return (f > 0.f) - (f < 0.f);
}

/**
 * @return The Hermite interpolation of `f`.
 */
static inline float __attribute__ ((warn_unused_result)) Smoothf(float f, float min, float max) {
	const float s = Clampf((f - min) / (max - min), 0.f, 1.f);
	return s * s;
}

#pragma mark - vec2_t

/**
 * @return A `vec2_t` with the specified components.
 */
static inline vec2_t __attribute__ ((warn_unused_result)) Vec2(float x, float y) {
	return (vec2_t) { .x = x, .y = y };
}

/**
 * @return The vector sum of `a + b`.
 */
static inline vec2_t __attribute__ ((warn_unused_result)) Vec2_Add(const vec2_t a, const vec2_t b) {
	return Vec2(a.x + b.x, a.y + b.y);
}

/**
 * @return The difference of `a - b`.
 */
static inline vec2_t __attribute__ ((warn_unused_result)) Vec2_Subtract(const vec2_t a, const vec2_t b) {
	return Vec2(a.x - b.x, a.y - b.y);
}

/**
 * @return The dot product of `a · b`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec2_Dot(const vec2_t a, const vec2_t b) {
	return a.x * b.x + a.y * b.y;
}

/**
 * @return The squared length (magnitude) of `v`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec2_LengthSquared(const vec2_t v) {
	return Vec2_Dot(v, v);
}

/**
 * @return The length (magnitude) of `v`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec2_Length(const vec2_t v) {
	return sqrtf(Vec2_LengthSquared(v));
}

/**
 * @return The length squared of the vector `a - b`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec2_DistanceSquared(const vec2_t a, const vec2_t b) {
	return Vec2_LengthSquared(Vec2_Subtract(a, b));
}

/**
 * @return The length of the vector `a - b`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec2_Distance(const vec2_t a, const vec2_t b) {
	return Vec2_Length(Vec2_Subtract(a, b));
}

/**
 * @return True if `a` and `b` are equal, using the specified epsilon.
 */
static inline _Bool __attribute__ ((warn_unused_result)) Vec2_EqualEpsilon(const vec2_t a, const vec2_t b, float epsilon) {
	return EqualEpsilonf(a.x, b.x, epsilon) &&
		   EqualEpsilonf(a.y, b.y, epsilon);
}

/**
 * @return True if `a` and `b` are equal.
 */
static inline _Bool __attribute__ ((warn_unused_result)) Vec2_Equal(const vec2_t a, const vec2_t b) {
	return Vec2_EqualEpsilon(a, b, __FLT_EPSILON__);
}

/**
 * @return A vector containing the max components of `a` and `b`.
 */
static inline vec2_t __attribute__ ((warn_unused_result)) Vec2_Maxf(const vec2_t a, const vec2_t b) {
	return Vec2(Maxf(a.x, b.x), Maxf(a.y, b.y));
}

/**
 * @return The vector `(-FLT_MAX, -FLT_MAX)`.
 */
static inline vec2_t __attribute__ ((warn_unused_result)) Vec2_Maxs(void) {
	return Vec2(-FLT_MAX, -FLT_MAX);
}

/**
 * @return A vector containing the min components of `a` and `b`.
 */
static inline vec2_t __attribute__ ((warn_unused_result)) Vec2_Minf(const vec2_t a, const vec2_t b) {
	return Vec2(Minf(a.x, b.x), Minf(a.y, b.y));
}

/**
 * @return The vector `(FLT_MAX, FLT_MAX)`.
 */
static inline vec2_t __attribute__ ((warn_unused_result)) Vec2_Mins(void) {
	return Vec2(FLT_MAX, FLT_MAX);
}

/**
 * @return The vector `v` scaled by `scale`.
 */
static inline vec2_t __attribute__ ((warn_unused_result)) Vec2_Scale(const vec2_t v, float scale) {
	return Vec2(v.x * scale, v.y * scale);
}

/**
 * @return The vector `v` + (`add` * `multiply`).
 */
static inline vec2_t __attribute__ ((warn_unused_result)) Vec2_Fmaf(const vec2_t v, float multiply, const vec2_t add) {
	return Vec2(fmaf(add.x, multiply, v.x), fmaf(add.y, multiply, v.y));
}

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
static inline vec2_t __attribute__ ((warn_unused_result)) Vec2_Mix(const vec2_t a, const vec2_t b, float mix) {
	return Vec2_Add(Vec2_Scale(a, 1.f - mix), Vec2_Scale(b, mix));
}

/**
 * @return The zero vector.
 */
static inline vec2_t __attribute__ ((warn_unused_result)) Vec2_Zero(void) {
	return Vec2(0.f, 0.f);
}

#pragma mark - vec3_t

/**
 * @return A `vec3_t` with the specified components.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3(float x, float y, float z) {
	return (vec3_t) { .x = x, .y = y, .z = z };
}

/**
 * @return A `vec3_t` comprised of the specified `vec2_t` and `z`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec2_ToVec3(const vec2_t v, float z) {
	return Vec3(v.x, v.y, z);
}

/**
 * @return The vector `(0.f, 0.f, 0.f)`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Zero(void) {
	return Vec3(0.f, 0.f, 0.f);
}

/**
 * @return The vector `v` cast to `vec3_t`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3i_CastVec3(const vec3i_t v) {
	return Vec3((float) v.x, (float) v.y, (float) v.z);
}

/**
 * @return The vector sum of `a + b`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Add(const vec3_t a, const vec3_t b) {
	return Vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

/**
 * @return The difference of `a - b`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Subtract(const vec3_t a, const vec3_t b) {
	return Vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

/**
 * @return The vector `v` cast to `vec3d_t`.
 */
static inline vec3d_t __attribute__ ((warn_unused_result)) Vec3_CastVec3d(const vec3_t v) {
	return (vec3d_t) {
		.x = (double) v.x,
		.y = (double) v.y,
		.z = (double) v.z
	};
}

/**
 * @return The vector `v` cast to `s16vec3_t`.
 */
static inline vec3s_t __attribute__ ((warn_unused_result)) Vec3_CastVec3s(const vec3_t v) {
	return (vec3s_t) {
		.x = (int16_t) v.x,
		.y = (int16_t) v.y,
		.z = (int16_t) v.z
	};
}

/**
 * @return The vector `v` cast to `s32vec3_t`.
 */
static inline vec3i_t __attribute__ ((warn_unused_result)) Vec3_CastVec3i(const vec3_t v) {
	return (vec3i_t) {
		.x = (int32_t) v.x,
		.y = (int32_t) v.y,
		.z = (int32_t) v.z
	};
}

/**
 * @brief A vector containing the components of `v`, rounded to the nearest higher integer.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Ceilf(const vec3_t v) {
	return Vec3(ceilf(v.x),
				ceilf(v.y),
				ceilf(v.z));
}

/**
 * @return The specified Euler angles circularly clamped to `0.f - 360.f`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_ClampEuler(const vec3_t euler) {
	return Vec3(ClampEuler(euler.x),
				ClampEuler(euler.y),
				ClampEuler(euler.z));
}

/**
 * @return The cross product of `a ✕ b`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Cross(const vec3_t a, const vec3_t b) {
	return Vec3(a.y * b.z - a.z * b.y,
				 a.z * b.x - a.x * b.z,
				 a.x * b.y - a.y * b.x);
}

/**
 * @return The vector `v` scaled by `scale`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Scale(const vec3_t v, float scale) {
	return Vec3(v.x * scale, v.y * scale, v.z * scale);
}

/**
 * @return The negated vector `v`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Negate(const vec3_t v) {
	return Vec3_Scale(v, -1.f);
}

/**
 * @return The dot product of `a · b`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec3_Dot(const vec3_t a, const vec3_t b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

/**
 * @return The squared length (magnitude) of `v`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec3_LengthSquared(const vec3_t v) {
	return Vec3_Dot(v, v);
}

/**
 * @return The length (magnitude) of `v`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec3_Length(const vec3_t v) {
	return sqrtf(Vec3_LengthSquared(v));
}

/**
 * @return The normalized vector `v`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_NormalizeLength(const vec3_t v, float *length) {
	*length = Vec3_Length(v);
	if (*length > 0.f) {
		return Vec3_Scale(v, 1.f / *length);
	} else {
		return Vec3_Zero();
	}
}

/**
 * @return The normalized vector `v`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Normalize(const vec3_t v) {
	float length;
	return Vec3_NormalizeLength(v, &length);
}

/**
 * @return The length of `a - b` as well as the normalized directional vector.
 */
static inline float __attribute__ ((warn_unused_result)) Vec3_DistanceDir(const vec3_t a, const vec3_t b, vec3_t *dir) {
	float length;

	*dir = Vec3_NormalizeLength(Vec3_Subtract(a, b), &length);

	return length;
}

/**
 * @return The direction vector between points a and b.
 */
static inline vec3_t __attribute__((warn_unused_result)) Vec3_Direction(const vec3_t a, const vec3_t b) {
	return Vec3_Normalize(Vec3_Subtract(a, b));
}

/**
 * @return The squared length of the vector `a - b`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec3_DistanceSquared(const vec3_t a, const vec3_t b) {
	return Vec3_LengthSquared(Vec3_Subtract(a, b));
}

/**
 * @return The length of the vector `a - b`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec3_Distance(const vec3_t a, const vec3_t b) {
	return Vec3_Length(Vec3_Subtract(a, b));
}

/**
 * @return The quotient of `a / b`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Divide(const vec3_t a, const vec3_t b) {
	return Vec3(a.x / b.x, a.y / b.y, a.z / b.z);
}

/**
 * @return The up vector `(0.f, 0.f, 1.f)`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Up(void) {
	return Vec3(0.f, 0.f, 1.f);
}

/**
 * @return The down vector `(0.f, 0.f, -1.f)`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Down(void) {
	return Vec3_Negate(Vec3_Up());
}

/**
 * @return True if `a` and `b` are equal using the specified epsilon.
 */
static inline _Bool __attribute__ ((warn_unused_result)) Vec3_EqualEpsilon(const vec3_t a, const vec3_t b, float epsilon) {
	return EqualEpsilonf(a.x, b.x, epsilon) &&
		   EqualEpsilonf(a.y, b.y, epsilon) &&
		   EqualEpsilonf(a.z, b.z, epsilon);
}

/**
 * @return True if `a` and `b` are equal.
 */
static inline _Bool __attribute__ ((warn_unused_result)) Vec3_Equal(const vec3_t a, const vec3_t b) {
	return Vec3_EqualEpsilon(a, b, __FLT_EPSILON__);
}

/**
 * @return The euler angles, in radians, for the directional vector `dir`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Euler(const vec3_t dir) {
	float pitch, yaw;

	if (dir.y == 0.f && dir.x == 0.f) {
		yaw = 0.f;
		if (dir.z > 0.f) {
			pitch = 90.f;
		} else {
			pitch = 270.f;
		}
	} else {
		if (dir.x) {
			yaw = Degrees(atan2f(dir.y, dir.x));
		} else if (dir.y > 0.f) {
			yaw = 90.f;
		} else {
			yaw = 270.f;
		}

		if (yaw < 0.f) {
			yaw += 360.f;
		}

		const float forward = sqrtf(dir.x * dir.x + dir.y * dir.y);
		pitch = Degrees(atan2f(dir.z, forward));

		if (pitch < 0.f) {
			pitch += 360.f;
		}
	}

	return Vec3(-pitch, yaw, 0);
}

/**
 * @return A vector containing the absolute values of `v`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Fabsf(const vec3_t v) {
	return Vec3(fabsf(v.x), fabsf(v.y), fabsf(v.z));
}

/**
 * @brief A vector containing the components of `v`, rounded to the nearest lower integer.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Floorf(const vec3_t v) {
	return Vec3(floorf(v.x),
				floorf(v.y),
				floorf(v.z));
}

/**
 * @return The vector `v` + (`add` * `multiply`).
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Fmaf(const vec3_t v, float multiply, const vec3_t add) {
	return Vec3(fmaf(add.x, multiply, v.x), fmaf(add.y, multiply, v.y), fmaf(add.z, multiply, v.z));
}

/**
 * @return The vector containing the floating point modulo of `a / b`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Fmodf(const vec3_t a, const vec3_t b) {
	return Vec3(fmodf(a.x, b.x), fmodf(a.y, b.y), fmodf(a.z, b.z));
}

/**
 * @return A vector containing the max component of `v`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec3_Hmaxf(const vec3_t v) {
	return Maxf(Maxf(v.x, v.y), v.z);
}

/**
 * @return A vector containing the min component of `v`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec3_Hminf(const vec3_t v) {
	return Minf(Minf(v.x, v.y), v.z);
}

/**
 * @return A vector containing the max components of `a` and `b`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Maxf(const vec3_t a, const vec3_t b) {
	return Vec3(Maxf(a.x, b.x), Maxf(a.y, b.y), Maxf(a.z, b.z));
}

/**
 * @return The vector `(-FLT_MAX, -FLT_MAX, -FLT_MAX)`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Maxs(void) {
	return Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
}

/**
 * @return A vector containing the min components of `a` and `b`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Minf(const vec3_t a, const vec3_t b) {
	return Vec3(Minf(a.x, b.x), Minf(a.y, b.y), Minf(a.z, b.z));
}

/**
 * @return The vector `(FLT_MAX, FLT_MAX, FLT_MAX)`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Mins(void) {
	return Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
}

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Mix(const vec3_t a, const vec3_t b, float mix) {
	return Vec3_Add(Vec3_Scale(a, 1.f - mix), Vec3_Scale(b, mix));
}

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_MixEuler(const vec3_t a, const vec3_t b, float mix) {

	vec3_t _a = a;
	vec3_t _b = b;

	for (size_t i = 0; i < 3; i++) {
		if (_b.xyz[i] - _a.xyz[i] >= 180.f) {
			_a.xyz[i] += 360.f;
		} else if (_b.xyz[i] - _a.xyz[i] <= -180.f) {
			_b.xyz[i] += 360.f;
		}
	}

	return Vec3_Mix(_a, _b, mix);
}

/**
 * @return The product `a * b`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Multiply(const vec3_t a, const vec3_t b) {
	return Vec3(a.x * b.x, a.y * b.y, a.z * b.z);
}

/**
 * @return The vector `(1.f, 1.f, 1.f)`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_One(void) {
	return Vec3(1.f, 1.f, 1.f);
}

/**
 * @return The linear interpolation of `a` and `b` using the specified fractions.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Mix3(const vec3_t a, const vec3_t b, const vec3_t mix) {
	return Vec3_Add(Vec3_Multiply(a, Vec3_Subtract(Vec3_One(), mix)), Vec3_Multiply(b, mix));
}

/**
 * @return The vector `a` raised tht exponent `exp`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Pow(const vec3_t a, float exp) {
	return Vec3(powf(a.x, exp), powf(a.y, exp), powf(a.z, exp));
}

/**
 * @return The vector `degrees` in radians.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Radians(const vec3_t degrees) {
	return Vec3_Scale(degrees, RadiansScalar);
}

/**
 * @return A vector with random values between the respective ranges.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_RandomRanges(float x_begin, float x_end,
																			float y_begin, float y_end,
																			float z_begin, float z_end) {
	return Vec3(RandomRangef(x_begin, x_end),
				RandomRangef(y_begin, y_end),
				RandomRangef(z_begin, z_end));
}

/**
 * @return A vector with random values between `begin` and `end`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_RandomRange(float begin, float end) {
	return Vec3_RandomRanges(begin, end, begin, end, begin, end);
}

/**
 * @return A vector with random values between `0` and `1`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Random(void) {
	return Vec3_RandomRange(0.f, 1.f);
}

/**
 * @return Returns a random vector (positive or negative) with a length of 1.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_RandomDir(void) {
	return Vec3_Normalize(Vec3_RandomRange(-1.f, 1.f));
}

/**
 * @return Takes a vector and randomizes its direction where a `random` of 0 yields the original vector and 1 yields a completely random direction.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_RandomizeDir(const vec3_t dir, const float randomization) {
	float length;
	vec3_t direction = Vec3_NormalizeLength(dir, &length);
	vec3_t result = Vec3_Mix(direction, Vec3_RandomDir(), Clampf(randomization, 0.f, 1.f));
	return Vec3_Scale(Vec3_Normalize(result), length);
}

/**
 * @return The vector `v` rounded to the nearest integer values.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Roundf(const vec3_t v) {
	return Vec3(roundf(v.x), roundf(v.y), roundf(v.z));
}

/**
 * @return
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Clamp(const vec3_t v, vec3_t min, vec3_t max) {
	return Vec3(
		Clampf(v.x, min.x, max.x),
		Clampf(v.y, min.y, max.y),
		Clampf(v.z, min.z, max.z));
}

/**
 * @return
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Clampf(const vec3_t v, float min, float max) {
	return Vec3(
		Clampf(v.x, min, max),
		Clampf(v.y, min, max),
		Clampf(v.z, min, max));
}

/**
 * @return
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Clamp01(const vec3_t v) {
	return Vec3(
		Clampf(v.x, 0.0, 1.0),
		Clampf(v.y, 0.0, 1.0),
		Clampf(v.z, 0.0, 1.0));
}

/**
 * @return The vector `a` reflected by the vector `b`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3_Reflect(const vec3_t a, const vec3_t b) {
	return Vec3_Add(a, Vec3_Scale(b, -2.f * Vec3_Dot(a, b)));
}

/**
 * @return The tangent and bitangent vectors for the given normal and texture directional vectors.
 */
static inline void Vec3_Tangents(const vec3_t normal, const vec3_t sdir, const vec3_t tdir, vec3_t *tangent, vec3_t *bitangent) {
	const vec3_t t = sdir;
	const vec3_t b = tdir;
	const vec3_t n = normal;

	*tangent = Vec3_Normalize(Vec3_Subtract(t, Vec3_Scale(n, Vec3_Dot(t, n))));
	*bitangent = Vec3_Normalize(Vec3_Cross(n, *tangent));

	if (Vec3_Dot(*bitangent, b) < 0.f) {
		*bitangent = Vec3_Negate(*bitangent);
	}
}

/**
 * @brief
 */
static inline void SinCosf(const float rad, float *s, float *c) {
	*s = sinf(rad);
	*c = cosf(rad);
}

/**
 * @return The forward, right and up vectors for the euler angles in radians.
 */
static inline void Vec3_Vectors(const vec3_t euler, vec3_t *forward, vec3_t *right, vec3_t *up) {
	float sr, sp, sy, cr, cp, cy;

	SinCosf(Radians(euler.x), &sp, &cp);
	SinCosf(Radians(euler.y), &sy, &cy);
	SinCosf(Radians(euler.z), &sr, &cr);

	if (forward) {
		forward->x = cp * cy;
		forward->y = cp * sy;
		forward->z = -sp;
	}

	if (right) {
		right->x = (-1.f * sr * sp * cy + -1.f * cr * -sy);
		right->y = (-1.f * sr * sp * sy + -1.f * cr * cy);
		right->z =  -1.f * sr * cp;
	}

	if (up) {
		up->x = (cr * sp * cy + -sr * -sy);
		up->y = (cr * sp * sy + -sr * cy);
		up->z = cr * cp;
	}
}

/**
 * @return The `xy` swizzle of `v`.
 */
static inline vec2_t __attribute__ ((warn_unused_result)) Vec3_XY(const vec3_t v) {
	return Vec2(v.x, v.y);
}

#pragma mark - vec4_t

/**
 * @return A `vec4_t` with the specified components.
 */
static inline vec4_t __attribute__ ((warn_unused_result)) Vec4(float x, float y, float z, float w) {
	return (vec4_t) { .x = x, .y = y, .z = z, .w = w };
}

/**
 * @return A `vec4_t` from the encoded normalized bytes.
 */
static inline vec4_t __attribute__ ((warn_unused_result)) Vec4bv(const uint32_t xyzw) {

	union {
		struct {
			byte x, y, z, w;
		};
		uint32_t integer;
	} in;

	in.integer = xyzw;

	return Vec4(
		((float) in.x / 255.f) * 2.f - 1.f,
		((float) in.y / 255.f) * 2.f - 1.f,
		((float) in.z / 255.f) * 2.f - 1.f,
		((float) in.w / 255.f) * 2.f - 1.f);
}

/**
 * @return A `vec4_t` comprised of the specified `vec3_t` and `w`.
 */
static inline vec4_t __attribute__ ((warn_unused_result)) Vec3_ToVec4(const vec3_t v, float w) {
	return Vec4(v.x, v.y, v.z, w);
}

/**
 * @return The sub of `a + b`.
 */
static inline vec4_t __attribute__ ((warn_unused_result)) Vec4_Add(const vec4_t a, const vec4_t b) {
	return Vec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

/**
 * @return The difference of `a - b`.
 */
static inline vec4_t __attribute__ ((warn_unused_result)) Vec4_Subtract(const vec4_t a, const vec4_t b) {
	return Vec4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

/**
 * @return The sub of `a + b`.
 */
static inline vec4_t __attribute__ ((warn_unused_result)) Vec4_Multiply(const vec4_t a, const vec4_t b) {
	return Vec4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}

/**
 * @return The negated vector `v`.
 */
static inline vec4_t __attribute__ ((warn_unused_result)) Vec4_Negate(const vec4_t v) {
	return Vec4(-v.x, -v.y, -v.z, -v.w);
}

/**
 * @return True if `a` and `b` are equal using the specified epsilon.
 */
static inline _Bool __attribute__ ((warn_unused_result)) Vec4_EqualEpsilon(const vec4_t a, const vec4_t b, float epsilon) {
	return fabsf(a.x - b.x) <= epsilon &&
		   fabsf(a.y - b.y) <= epsilon &&
		   fabsf(a.z - b.z) <= epsilon &&
		   fabsf(a.w - b.w) <= epsilon;
}

/**
 * @return True if `a` and `b` are equal.
 */
static inline _Bool __attribute__ ((warn_unused_result)) Vec4_Equal(const vec4_t a, const vec4_t b) {
	return Vec4_EqualEpsilon(a, b, __FLT_EPSILON__);
}

/**
 * @return The vector `v` scaled by `scale`.
 */
static inline vec4_t __attribute__ ((warn_unused_result)) Vec4_Scale(const vec4_t v, float scale) {
	return Vec4(v.x * scale, v.y * scale, v.z * scale, v.w * scale);
}

/**
 * @return The vector `v` + (`add` * `multiply`).
 */
static inline vec4_t __attribute__ ((warn_unused_result)) Vec4_Fmaf(const vec4_t v, float multiply, const vec4_t add) {
	return Vec4(fmaf(add.x, multiply, v.x), fmaf(add.y, multiply, v.y), fmaf(add.z, multiply, v.z), fmaf(add.w, multiply, v.w));
}

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
static inline vec4_t __attribute__ ((warn_unused_result)) Vec4_Mix(const vec4_t a, const vec4_t b, float mix) {
	return Vec4_Add(Vec4_Scale(a, 1.f - mix), Vec4_Scale(b, mix));
}

/**
 * @return The vector `(1.f, 1f., 1.f)`.
 */
static inline vec4_t __attribute__ ((warn_unused_result)) Vec4_One(void) {
	return Vec4(1.f, 1.f, 1.f, 1.f);
}

/**
 * @return The vector `a` raised tht exponent `exp`.
 */
static inline vec4_t __attribute__ ((warn_unused_result)) Vec4_Pow(const vec4_t a, float exp) {
	return Vec4(powf(a.x, exp), powf(a.y, exp), powf(a.z, exp), powf(a.w, exp));
}

/**
 * @return The vector `a` raised tht exponent `exp`.
 */
static inline vec4_t __attribute__ ((warn_unused_result)) Vec4_Pow3(const vec4_t a, const vec3_t exp) {
	return Vec4(powf(a.x, exp.x), powf(a.y, exp.y), powf(a.z, exp.z), a.w);
}

/**
 * @return A vector with random values between `begin` and `end`.
 */
static inline vec4_t __attribute__ ((warn_unused_result)) Vec4_RandomRange(float begin, float end) {
	return Vec4(RandomRangef(begin, end),
				RandomRangef(begin, end),
				RandomRangef(begin, end),
				RandomRangef(begin, end));
}

/**
 * @return A vevtor with random values between `0` and `1`.
 */
static inline vec4_t __attribute__ ((warn_unused_result)) Vec4_Random(void) {
	return Vec4_RandomRange(0.f, 1.f);
}

/**
 * @return A byte encoded representation of the normalized vector `v`.
 * @details Floating point -1.0 to 1.0 are packed to bytes, where -1.0 -> 0 and 1.0 -> 255.
 */
static inline uint32_t __attribute__ ((warn_unused_result)) Vec4_Bytes(const vec4_t v) {

	union {
		struct {
			byte x, y, z, w;
		};
		uint32_t integer;
	} out;

	out.x = (byte) Clampf((v.x + 1.f) * 0.5f * 255.f, 0.f, 255.f);
	out.y = (byte) Clampf((v.y + 1.f) * 0.5f * 255.f, 0.f, 255.f);
	out.z = (byte) Clampf((v.z + 1.f) * 0.5f * 255.f, 0.f, 255.f);
	out.w = (byte) Clampf((v.w + 1.f) * 0.5f * 255.f, 0.f, 255.f);

	return out.integer;
}

/**
 * @return A byte encoded representation of the normalized vector `v`.
 */
static inline int32_t __attribute__ ((warn_unused_result)) Vec3_Bytes(const vec3_t v) {
	return Vec4_Bytes(Vec3_ToVec4(v, 1.f));
}

/**
 * @return The xyz swizzle of `v`.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec4_XYZ(const vec4_t v) {
	return Vec3(v.x, v.y, v.z);
}

/**
 * @return The vector `(0.f, 0.f, 0.f, 0.f)`.
 */
static inline vec4_t __attribute__ ((warn_unused_result)) Vec4_Zero(void) {
	return Vec4(0.f, 0.f, 0.f, 0.f);
}

#pragma mark - double precision

/**
 * @return True if `fabs(a - b) <= epsilon`.
 */
static inline _Bool __attribute__ ((warn_unused_result)) EqualEpsilon(double a, double b, double epsilon) {
	return fabs(a - b) <= epsilon;
}

#pragma mark - vec3d_t

/**
 * @return A `vec3d_t` with the specified components.
 */
static inline vec3d_t __attribute__ ((warn_unused_result)) Vec3d(double x, double y, double z) {
	return (vec3d_t) {
		.x = x,
		.y = y,
		.z = z
	};
}

/**
 * @return The vector `v` cast to single precision.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Vec3d_CastVec3(const vec3d_t v) {
	return Vec3(v.x, v.y, v.z);
}

/**
 * @return The vector sum of `a + b`.
 */
static inline vec3d_t __attribute__ ((warn_unused_result)) Vec3d_Add(const vec3d_t a, const vec3d_t b) {
	return Vec3d(a.x + b.x, a.y + b.y, a.z + b.z);
}

/**
 * @return The cross product of `a ✕ b`.
 */
static inline vec3d_t __attribute__ ((warn_unused_result)) Vec3d_Cross(const vec3d_t a, const vec3d_t b) {
	return Vec3d(a.y * b.z - a.z * b.y,
				 a.z * b.x - a.x * b.z,
				 a.x * b.y - a.y * b.x);
}

/**
 * @return The difference of `a - b`.
 */
static inline vec3d_t __attribute__ ((warn_unused_result)) Vec3d_Subtract(const vec3d_t a, const vec3d_t b) {
	return Vec3d(a.x - b.x, a.y - b.y, a.z - b.z);
}

/**
 * @return The dot product of `a · b`.
 */
static inline double __attribute__ ((warn_unused_result)) Vec3d_Dot(const vec3d_t a, const vec3d_t b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

/**
 * @return The squared length (magnitude) of `v`.
 */
static inline double __attribute__ ((warn_unused_result)) Vec3d_LengthSquared(const vec3d_t v) {
	return Vec3d_Dot(v, v);
}

/**
 * @return The length (magnitude) of `v`.
 */
static inline double __attribute__ ((warn_unused_result)) Vec3d_Length(const vec3d_t v) {
	return sqrt(Vec3d_LengthSquared(v));
}

/**
 * @return The squared length of the vector `a - b`.
 */
static inline double __attribute__ ((warn_unused_result)) Vec3d_DistanceSquared(const vec3d_t a, const vec3d_t b) {
	return Vec3d_LengthSquared(Vec3d_Subtract(a, b));
}

/**
 * @return The length of the vector `a - b`.
 */
static inline double __attribute__ ((warn_unused_result)) Vec3d_Distance(const vec3d_t a, const vec3d_t b) {
	return Vec3d_Length(Vec3d_Subtract(a, b));
}

/**
 * @return True if `a` and `b` are equal using the specified epsilon.
 */
static inline _Bool __attribute__ ((warn_unused_result)) Vec3d_EqualEpsilon(const vec3d_t a, const vec3d_t b, double epsilon) {
	return EqualEpsilon(a.x, b.x, epsilon) &&
		   EqualEpsilon(a.y, b.y, epsilon) &&
		   EqualEpsilon(a.z, b.z, epsilon);
}

/**
 * @return True if `a` and `b` are equal.
 */
static inline _Bool __attribute__ ((warn_unused_result)) Vec3d_Equal(const vec3d_t a, const vec3d_t b) {
	return Vec3d_EqualEpsilon(a, b, __DBL_EPSILON__);
}

/**
 * @return The vector `v` scaled by `scale`.
 */
static inline vec3d_t __attribute__ ((warn_unused_result)) Vec3d_Scale(const vec3d_t v, double scale) {
	return Vec3d(v.x * scale, v.y * scale, v.z * scale);
}

/**
 * @return The vector `(0., 0., 0.)`.
 */
static inline vec3d_t __attribute__ ((warn_unused_result)) Vec3d_Zero(void) {
	return Vec3d(0., 0., 0.);
}

/**
 * @return The normalized vector `v`.
 */
static inline vec3d_t __attribute__ ((warn_unused_result)) Vec3d_Normalize(const vec3d_t v) {
	const double length = Vec3d_Length(v);
	if (length) {
		return Vec3d_Scale(v, 1.0 / length);
	} else {
		return Vec3d_Zero();
	}
}
