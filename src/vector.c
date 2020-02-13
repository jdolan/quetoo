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

#include "vector.h"

#pragma mark - short integer vectors

/**
 * @brief
 */
s16vec2_t s16vec2(int16_t x, int16_t y) {
	return (s16vec2_t) {
		.x = x,
		.y = y
	};
}

/**
 * @brief
 */
s16vec2_t s16vec2_zero(void) {
	return s16vec2(0, 0);
}

/**
 * @brief
 */
s16vec3_t s16vec3(int16_t x, int16_t y, int16_t z) {
	return (s16vec3_t) {
		.x = x,
		.y = y,
		.z = z
	};
}

/**
 * @brief.
 */
vec3_t s16vec3_cast_vec3(const s16vec3_t v) {
	return (vec3_t) {
		.x = (float) v.x,
		.y = (float) v.y,
		.z = (float) v.z
	};
}

/**
 * @brief
 */
_Bool s16vec3_equal(const s16vec3_t a, s16vec3_t b) {
	return a.x == b.x &&
		   a.y == b.y &&
		   a.z == b.z;
}

/**
 * @brief
 */
s16vec3_t s16vec3_zero(void) {
	return s16vec3(0, 0, 0);
}

#pragma mark - integer vectors

/**
 * @brief
 */
s32vec3_t s32vec3(int32_t x, int32_t y, int32_t z) {
	return (s32vec3_t) {
		.x = x,
		.y = y,
		.z = z
	};
}

/**
 * @brief
 */
s32vec3_t s32vec3_add(const s32vec3_t a, const s32vec3_t b) {
	return s32vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

/**
 * @brief
 */
vec3_t s32vec3_cast_vec3(const s32vec3_t v) {
	return vec3((float) v.x, (float) v.y, (float) v.z);
}

/**
 * @brief
 */
s32vec3_t s32vec3_zero(void) {
	return s32vec3(0, 0, 0);
}

#pragma mark - single precision

#define RAD2DEG (180.f / M_PI)
#define DEG2RAD (M_PI / 180.f)

/**
 * @brief
 */
float clampf(float f, float min, float max) {
	return minf(maxf(f, min), max);
}

/**
 * @brief
 */
float degrees(float radians) {
	return radians * RAD2DEG;
}

/**
 * @brief
 */
_Bool equalf_epsilon(float a, float b, float epsilon) {
	return fabsf(a - b) <= epsilon;
}

/**
 * @brief
 */
float minf(float a, float b) {
	return a < b ? a : b;
}

/**
 * @brief
 */
float maxf(float a, float b) {
	return a > b ? a : b;
}

/**
 * @brief
 */
float radians(float degrees) {
	return degrees * DEG2RAD;
}

/**
 * @brief
 */
float random_range(float begin, float end) {
	static guint seed;
	if (seed == 0) {
		seed = (guint) time(NULL);
		g_random_set_seed(seed);
	}

	return g_random_double_range(begin, end);
}

/**
 * @brief
 */
int32_t signf(float f) {
	return (f > 0.f) - (f < 0.f);
}

/**
 * @brief
 */
float smoothf(float f, float min, float max) {
	const float s = clampf((f - min) / (max - min), 0.f, 1.f);
	return s * s * (3.f - 2.f * s);
}

#pragma mark - vec2_t

/**
 * @brief
 */
vec2_t Vec2(float x, float y) {
	return (vec2_t) { .x = x, .y = y };
}

/**
 * @brief
 */
vec2_t Vec2_Add(const vec2_t a, const vec2_t b) {
	return Vec2(a.x + b.x, a.y + b.y);
}

/**
 * @brief
 */
float Vec2_DistanceSquared(const vec2_t a, const vec2_t b) {
	return Vec2_LengthSquared(Vec2_Subtract(a, b));
}

/**
 * @brief
 */
float Vec2_Distance(const vec2_t a, const vec2_t b) {
	return Vec2_Length(Vec2_Subtract(a, b));
}

/**
 * @brief
 */
float Vec2_Dot(const vec2_t a, const vec2_t b) {
	return a.x * b.x + a.y * b.y;
}

/**
 * @brief
 */
_Bool Vec2_EqualEpsilon(const vec2_t a, const vec2_t b, float epsilon) {
	return equalf_epsilon(a.x, b.x, epsilon) &&
		   equalf_epsilon(a.y, b.y, epsilon);
}

/**
 * @brief
 */
_Bool Vec2_Equal(const vec2_t a, const vec2_t b) {
	return Vec2_EqualEpsilon(a, b, __FLT_EPSILON__);
}

/**
 * @brief
 */
float Vec2_LengthSquared(const vec2_t v) {
	return Vec2_Dot(v, v);
}

/**
 * @brief
 */
float Vec2_Length(const vec2_t v) {
	return sqrtf(Vec2_LengthSquared(v));
}

/**
 * @brief
 */
vec2_t Vec2_Maxf(const vec2_t a, const vec2_t b) {
	return Vec2(maxf(a.x, b.x), maxf(a.y, b.y));
}

/**
 * @brief
 */
vec2_t Vec2_Maxs(void) {
	return Vec2(-FLT_MAX, -FLT_MAX);
}

/**
 * @brief
 */
vec2_t Vec2_Minf(const vec2_t a, const vec2_t b) {
	return Vec2(minf(a.x, b.x), minf(a.y, b.y));
}

/**
 * @brief
 */
vec2_t Vec2_Mins(void) {
	return Vec2(FLT_MAX, FLT_MAX);
}

/**
 * @brief
 */
vec2_t Vec2_Mix(const vec2_t a, const vec2_t b, float mix) {
	return Vec2_Add(Vec2_Scale(a, 1.f - mix), Vec2_Scale(b, mix));
}

/**
 * @brief
 */
vec2_t Vec2_Scale(const vec2_t v, float scale) {
	return Vec2(v.x * scale, v.y * scale);
}

/**
 * @brief
 */
vec2_t Vec2_Subtract(const vec2_t a, const vec2_t b) {
	return Vec2(a.x - b.x, a.y - b.y);
}

/**
 * @brief
 */
vec2_t Vec2_Zero(void) {
	return Vec2(0.f, 0.f);
}

#pragma mark - vec3_t

/**
 * @brief
 */
vec3_t vec3(float x, float y, float z) {
	return (vec3_t) { .x = x, .y = y, .z = z};
}

/**
 * @brief
 */
vec3_t vec3_abs(const vec3_t v) {
	return vec3(fabsf(v.x), fabsf(v.y), fabsf(v.z));
}

/**
 * @brief
 */
vec3_t vec3_add(const vec3_t a, const vec3_t b) {
	return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

/**
 * @brief
 */
_Bool vec3_box_intersect(const vec3_t amins, const vec3_t amaxs, const vec3_t bmins, const vec3_t bmaxs) {

	if (amins.x >= bmaxs.x || amins.y >= bmaxs.y || amins.z >= bmaxs.z) {
		return false;
	}

	if (amaxs.x <= bmins.x || amaxs.y <= bmins.y || amaxs.z <= bmins.z) {
		return false;
	}

	return true;
}

/**
 * @brief
 */
dvec3_t vec3_cast_dvec3(const vec3_t v) {
	return (dvec3_t) {
		.x = (double) v.x,
		.y = (double) v.y,
		.z = (double) v.z
	};
}

/**
 * @brief
 */
s16vec3_t vec3_cast_s16vec3(const vec3_t v) {
	return (s16vec3_t) {
		.x = (int16_t) v.x,
		.y = (int16_t) v.y,
		.z = (int16_t) v.z
	};
}

/**
 * @brief
 */
s32vec3_t vec3_cast_s32vec3(const vec3_t v) {
	return (s32vec3_t) {
		.x = (int32_t) v.x,
		.y = (int32_t) v.y,
		.z = (int32_t) v.z
	};
}

/**
 * @brief
 */
vec3_t vec3_cross(const vec3_t a, const vec3_t b) {
	return vec3(a.y * b.z - a.z * b.y,
				 a.z * b.x - a.x * b.z,
				 a.x * b.y - a.y * b.x);
}

/**
 * @brief
 */
vec3_t vec3_degrees(const vec3_t radians) {
	return vec3_scale(radians, RAD2DEG);
}

/**
 * @brief
 */
float vec3_distance_dir(const vec3_t a, const vec3_t b, vec3_t *dir) {
	float length;

	*dir = vec3_normalize_length(vec3_subtract(a, b), &length);

	return length;
}

/**
 * @brief
 */
float vec3_distance_squared(const vec3_t a, const vec3_t b) {
	return vec3_length_squared(vec3_subtract(a, b));
}

/**
 * @brief
 */
float vec3_distance(const vec3_t a, const vec3_t b) {
	return vec3_length(vec3_subtract(a, b));
}

/**
 * @brief
 */
float vec3_dot(const vec3_t a, const vec3_t b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

/**
 * @brief
 */
vec3_t vec3_down(void) {
	return vec3_negate(vec3_up());
}

/**
 * @brief
 */
_Bool vec3_equal_epsilon(const vec3_t a, const vec3_t b, float epsilon) {
	return equalf_epsilon(a.x, b.x, epsilon) &&
		   equalf_epsilon(a.y, b.y, epsilon) &&
		   equalf_epsilon(a.z, b.z, epsilon);
}

/**
 * @brief
 */
_Bool vec3_equal(const vec3_t a, const vec3_t b) {
	return vec3_equal_epsilon(a, b, __FLT_EPSILON__);
}

/**
 * @brief
 */
vec3_t vec3_euler(const vec3_t dir) {
	return vec3_degrees(vec3(asinf(dir.z), atan2f(dir.y, dir.x), 0.f));
}

/**
 * @brief
 */
vec3_t vec3_forward(const vec3_t euler) {
	const vec3_t radians = vec3_radians(euler);
	return vec3(cosf(radians.x) * cosf(radians.y),
				 cosf(radians.x) * sinf(radians.y),
				 sinf(radians.x) * -1.f);
}

/**
 * @brief
 */
float vec3_length_squared(const vec3_t v) {
	return vec3_dot(v, v);
}

/**
 * @brief
 */
float vec3_length(const vec3_t v) {
	return sqrtf(vec3_length_squared(v));
}

/**
 * @brief
 */
vec3_t vec3_maxf(const vec3_t a, const vec3_t b) {
	return vec3(maxf(a.x, b.x), maxf(a.y, b.y), maxf(a.z, b.z));
}

/**
 * @brief
 */
vec3_t vec3_maxs(void) {
	return vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
}

/**
 * @brief
 */
vec3_t vec3_minf(const vec3_t a, const vec3_t b) {
	return vec3(minf(a.x, b.x), minf(a.y, b.y), minf(a.z, b.z));
}

/**
 * @brief
 */
vec3_t vec3_mins(void) {
	return vec3(FLT_MAX, FLT_MAX, FLT_MAX);
}

/**
 * @brief
 */
vec3_t vec3_mix_euler(const vec3_t a, const vec3_t b, float mix) {

	vec3_t delta = vec3_subtract(b, a);
	for (size_t i = 0; i < lengthof(delta.xyz); i++) {
		if (delta.xyz[i] > 180.f) {
			delta.xyz[i] -= 360.f;
		} else if (delta.xyz[i] < -180.f) {
			delta.xyz[i] += 360.f;
		}
	}

	return vec3_mix(a, delta, mix);
}

/**
 * @brief
 */
vec3_t vec3_mix(const vec3_t a, const vec3_t b, float mix) {
	return vec3_add(vec3_scale(a, 1.f - mix), vec3_scale(b, mix));
}

/**
 * @brief
 */
vec3_t vec3_multiply(const vec3_t a, const vec3_t b) {
	return vec3(a.x * b.x, a.y * b.y, a.z * b.z);
}

/**
 * @brief
 */
vec3_t vec3_negate(const vec3_t v) {
	return vec3_scale(v, -1.f);
}

/**
 * @brief
 */
vec3_t vec3_normalize_length(const vec3_t v, float *length) {
	*length = vec3_length(v);
	if (*length) {
		return vec3_scale(v, 1.f / *length);
	} else {
		return vec3_zero();
	}
}

/**
 * @brief
 */
vec3_t vec3_normalize(const vec3_t v) {
	float length;

	return vec3_normalize_length(v, &length);
}

/**
 * @brief
 */
vec3_t vec3_one(void) {
	return vec3(1.f, 1.f, 1.f);
}

/**
 * @brief
 */
vec3_t vec3_radians(const vec3_t degrees) {
	return vec3_scale(degrees, DEG2RAD);
}

/**
 * @brief
 */
vec3_t vec3_random_range(float begin, float end) {
	return vec3(random_range(begin, end),
				random_range(begin, end),
				random_range(begin, end));
}

/**
 * @brief
 */
vec3_t vec3_random(void) {
	return vec3_random_range(0., 1.);
}

/**
 * @brief
 */
vec3_t vec3_reflect(const vec3_t a, const vec3_t b) {
	return vec3_add(a, vec3_scale(b, -2.0 * vec3_dot(a, b)));
}

/**
 * @brief
 */
vec3_t vec3_subtract(const vec3_t a, const vec3_t b) {
	return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

/**
 * @brief
 */
vec3_t vec3_scale(const vec3_t v, float scale) {
	return vec3(v.x * scale, v.y * scale, v.z * scale);
}

/**
 * @brief
 */
void vec3_tangents(const vec3_t normal, const vec3_t sdir, const vec3_t tdir, vec3_t *tangent, vec3_t *bitangent) {

	const vec3_t s = vec3_normalize(sdir);
	const vec3_t t = vec3_normalize(tdir);

	*tangent = vec3_add(s, vec3_scale(normal, -vec3_dot(s, normal)));
	if (vec3_dot(s, *tangent) < 0.f) {
		*tangent = vec3_negate(*tangent);
	}

	*bitangent = vec3_cross(normal, *tangent);
	if (vec3_dot(t, *bitangent) < 0.f) {
		*bitangent = vec3_negate(*bitangent);
	}
}

/**
 * @brief
 */
vec4_t vec3_to_vec4(const vec3_t v, float w) {
	return vec4(v.x, v.y, v.z, w);
}

/**
 * @brief
 */
vec3_t vec3_up(void) {
	return vec3(0.f, 0.f, 1.f);
}

/**
 * @brief
 */
void vec3_vectors(const vec3_t euler, vec3_t *forward, vec3_t *right, vec3_t *up) {

	const vec3_t f = vec3_forward(euler);
	if (forward) {
		*forward = f;
	}
	const vec3_t r = vec3_cross(f, vec3_up());
	if (right) {
		*right = r;
	}
	const vec3_t u = vec3_cross(f, r);
	if (up) {
		*up = u;
	}
}

/**
 * @brief
 */
vec2_t vec3_xy(const vec3_t v) {
	return Vec2(v.x, v.y);
}

/**
 * @brief
 */
vec3_t vec3_zero(void) {
	return vec3(0.f, 0.f, 0.f);
}

#pragma mark - vec4_t

/**
 * @brief
 */
vec4_t vec4(float x, float y, float z, float w) {
	return (vec4_t) { .x = x, .y = y, .z = z, .w = w };
}

/**
 * @brief
 */
vec4_t vec4_add(const vec4_t a, const vec4_t b) {
	return vec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

/**
 * @brief
 */
_Bool vec4_equal_epsilon(const vec4_t a, const vec4_t b, float epsilon) {
	return fabsf(a.x - b.x) <= epsilon &&
		   fabsf(a.y - b.y) <= epsilon &&
		   fabsf(a.z - b.z) <= epsilon &&
		   fabsf(a.w - b.w) <= epsilon;
}

/**
 * @brief
 */
_Bool vec4_equal(const vec4_t a, const vec4_t b) {
	return vec4_equal_epsilon(a, b, __FLT_EPSILON__);
}

/**
 * @brief
 */
vec4_t vec4_mix(const vec4_t a, const vec4_t b, float mix) {
	return vec4_add(vec4_scale(a, 1.f - mix), vec4_scale(b, mix));
}

/**
 * @brief
 */
vec4_t vec4_one(void) {
	return vec4(1.f, 1.f, 1.f, 1.f);
}

/**
 * @brief
 */
vec4_t vec4_scale(const vec4_t v, float scale) {
	return vec4(v.x * scale, v.y * scale, v.z * scale, v.w * scale);
}

/**
 * @brief
 */
vec4_t vec4_subtract(const vec4_t a, const vec4_t b) {
	return vec4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

/**
 * @brief
 */
vec3_t vec4_xyz(const vec4_t v) {
	return vec3(v.x, v.y, v.z);
}

/**
 * @brief
 */
vec4_t vec4_zero(void) {
	return vec4(0.f, 0.f, 0.f, 0.f);
}

#pragma mark - double precision

/**
 * @brief
 */
_Bool equal_epsilon(double a, double b, double epsilon) {
	return fabs(a - b) <= epsilon;
}

#pragma mark - dvec3_t

/**
 * @brief
 */
dvec3_t dvec3(double x, double y, double z) {
	return (dvec3_t) {
		.x = x,
		.y = y,
		.z = z
	};
}

/**
 * @brief
 */
dvec3_t dvec3_add(const dvec3_t a, const dvec3_t b) {
	return dvec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

/**
 * @brief
 */
vec3_t dvec3_cast_vec3(const dvec3_t v) {
	return vec3(v.x, v.y, v.z);
}

/**
 * @brief
 */
dvec3_t dvec3_cross(const dvec3_t a, const dvec3_t b) {
	return dvec3(a.y * b.z - a.z * b.y,
				 a.z * b.x - a.x * b.z,
				 a.x * b.y - a.y * b.x);
}

/**
 * @brief
 */
double dvec3_distance_squared(const dvec3_t a, const dvec3_t b) {
	return dvec3_length_squared(dvec3_subtract(a, b));
}

/**
 * @brief
 */
double dvec3_distance(const dvec3_t a, const dvec3_t b) {
	return dvec3_length(dvec3_subtract(a, b));
}

/**
 * @brief
 */
double dvec3_dot(const dvec3_t a, const dvec3_t b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

/**
 * @brief
 */
_Bool dvec3_equal_epsilon(const dvec3_t a, const dvec3_t b, double epsilon) {
	return equal_epsilon(a.x, b.x, epsilon) &&
		   equal_epsilon(a.y, b.y, epsilon) &&
		   equal_epsilon(a.z, b.z, epsilon);
}

/**
 * @brief
 */
_Bool dvec3_equal(const dvec3_t a, const dvec3_t b) {
	return dvec3_equal_epsilon(a, b, __DBL_EPSILON__);
}

/**
 * @brief
 */
double dvec3_length_squared(const dvec3_t v) {
	return dvec3_dot(v, v);
}

/**
 * @brief
 */
double dvec3_length(const dvec3_t v) {
	return sqrt(dvec3_length_squared(v));
}

/**
 * @brief
 */
dvec3_t dvec3_normalize(const dvec3_t v) {
	const double length = dvec3_length(v);
	if (length) {
		return dvec3_scale(v, 1.0 / length);
	} else {
		return dvec3_zero();
	}
}

/**
 * @brief
 */
dvec3_t dvec3_scale(const dvec3_t v, double scale) {
	return dvec3(v.x * scale, v.y * scale, v.z * scale);
}

/**
 * @brief
 */
dvec3_t dvec3_subtract(const dvec3_t a, const dvec3_t b) {
	return dvec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

/**
 * @brief
 */
dvec3_t dvec3_zero(void) {
	return dvec3(0., 0., 0.);
}
