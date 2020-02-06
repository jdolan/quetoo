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

#include <math.h>

#define RAD2DEG (180.f / M_PI)
#define DEG2RAD (M_PI / 180.f)

/**
 * @brief Two component floating point vector type.
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
} vec2;

/**
 * @brief Three component floating point vector type.
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
} vec3;

/**
 * @brief Four component floating point vector type.
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
} vec4;

static inline float minf(float a, float b);
static inline float maxf(float a, float b);
static inline float clampf(float f, float min, float max);
static inline float smoothf(float f, float min, float max);

static inline vec2 vec2f(float x, float y);
static inline vec2 vec2_add(const vec2 a, const vec2 b);
static inline float vec2_distance(const vec2 a, const vec2 b);
static inline float vec2_dot(const vec2 a, const vec2 b);
static inline _Bool vec2_equal(const vec2 a, const vec2 b, float epsilon);
static inline vec2 vec2_subtract(const vec2 a, const vec2 b);
static inline float vec2_length(const vec2 v);
static inline vec2 vec2_zero(void);

static inline vec3 vec3f(float x, float y, float z);
static inline vec3 vec3_add(const vec3 a, const vec3 b);
static inline vec3 vec3_cross(const vec3 a, const vec3 b);
static inline vec3 vec3_degrees(const vec3 radians);
static inline float vec3_distance(const vec3 a, const vec3 b);
static inline float vec3_dot(const vec3 a, const vec3 b);
static inline vec3 vec3_down(void);
static inline _Bool vec3_equal(const vec3 a, const vec3 b, float epsilon);
static inline vec3 vec3_euler(const vec3 dir);
static inline vec3 vec3_forward(const vec3 euler);
static inline float vec3_length(const vec3 v);
static inline vec3 vec3_negate(const vec3 v);
static inline vec3 vec3_normalize(const vec3 v);
static inline vec3 vec3_one(void);
static inline vec3 vec3_radians(const vec3 degrees);
static inline vec3 vec3_scale(const vec3 v, float scale);
static inline vec3 vec3_subtract(const vec3 a, const vec3 b);
static inline vec3 vec3_up(void);
static inline vec3 vec3_zero(void);

//static vec4 vec4f(float x, float y, float z, float w);

#pragma mark - float

static float clampf(float f, float min, float max) {
	return minf(maxf(f, min), max);
}

static float minf(float a, float b) {
	return a < b ? a : b;
}

static float maxf(float a, float b) {
	return a > b ? a : b;
}

static float smoothf(float f, float min, float max) {
	const float s = clampf((f - min) / (max - min), 0.f, 1.f);
	return s * s * (3.f - 2.f * s);
}

#pragma mark - vec2

static vec2 vec2f(float x, float y) {
	return (vec2) { .x = x, .y = y };
}

static vec2 vec2_add(const vec2 a, const vec2 b) {
	return vec2f(a.x + b.x, a.y + b.y);
}

static float vec2_distance(const vec2 a, const vec2 b) {
	return vec2_length(vec2_subtract(a, b));
}

static float vec2_dot(const vec2 a, const vec2 b) {
	return a.x * b.x + a.y * b.y;
}

static _Bool vec2_equal(const vec2 a, const vec2 b, float epsilon) {
	return fabsf(a.x - b.x) <= epsilon &&
		   fabsf(a.y - b.y) <= epsilon;
}

static vec2 vec2_subtract(const vec2 a, const vec2 b) {
	return vec2f(a.x - b.x, a.y - b.y);
}

static float vec2_length(const vec2 v) {
	return sqrtf(v.x * v.x + v.y * v.y);
}

static vec2 vec2_zero(void) {
	return vec2f(0.f, 0.f);
}

#pragma mark - vec3

static vec3 vec3f(float x, float y, float z) {
	return (vec3) { .x = x, .y = y, .z = z};
}

static vec3 vec3_add(const vec3 a, const vec3 b) {
	return vec3f(a.x + b.x, a.y + b.y, a.z + b.z);
}

static vec3 vec3_cross(const vec3 a, const vec3 b) {
	return vec3f(a.y * b.z - a.z * b.y,
				 a.z * b.x - a.x * b.z,
				 a.x * b.y - a.y * b.x);
}

static vec3 vec3_degrees(const vec3 radians) {
	return vec3_scale(radians, RAD2DEG);
}

static float vec3_distance(const vec3 a, const vec3 b) {
	return vec3_length(vec3_subtract(a, b));
}

static float vec3_dot(const vec3 a, const vec3 b) {
	return a.x * b.x + a.y * b.y + a.z + b.z;
}

static vec3 vec3_down(void) {
	return vec3_negate(vec3_up());
}

static _Bool vec3_equal(const vec3 a, const vec3 b, float epsilon) {
	return fabsf(a.x - b.x) <= epsilon &&
		   fabsf(a.y - b.y) <= epsilon &&
		   fabsf(a.z - b.z) <= epsilon;
}

static vec3 vec3_euler(const vec3 dir) {
	return vec3f(asinf(dir.z), atan2f(dir.y, dir.x), 0.f);
}

static vec3 vec3_forward(const vec3 euler) {
	return vec3f(cosf(euler.x) * cosf(euler.y),
				 cosf(euler.x) * sinf(euler.y),
				 sinf(euler.x) * -1.f);
}

static float vec3_length(const vec3 v) {
	return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static vec3 vec3_negate(const vec3 v) {
	return vec3_scale(v, -1.f);
}

static vec3 vec3_normalize(const vec3 v) {
	const float length = vec3_length(v);
	if (length) {
		return vec3_scale(v, 1.f / length);
	}
	return vec3_zero();
}

static vec3 vec3_one(void) {
	return vec3f(1.f, 1.f, 1.f);
}

static vec3 vec3_radians(const vec3 degrees) {
	return vec3_scale(degrees, DEG2RAD);
}

static vec3 vec3_subtract(const vec3 a, const vec3 b) {
	return vec3f(a.x - b.x, a.y - b.y, a.z - b.z);
}

static vec3 vec3_scale(const vec3 v, float scale) {
	return vec3f(v.x * scale, v.y * scale, v.z * scale);
}

static vec3 vec3_up(void) {
	return vec3f(0.f, 0.f, 1.f);
}

static vec3 vec3_zero(void) {
	return vec3f(0.f, 0.f, 0.f);
}

//static vec4 vec4f(float x, float y, float z, float w) {
//	return (vec4) { .x = x, .y = y, .z = z, .w = w };
//}


