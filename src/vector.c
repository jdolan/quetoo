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
vec2s_t Vec2s(int16_t x, int16_t y) {
	return (vec2s_t) {
		.x = x,
		.y = y
	};
}

/**
 * @brief
 */
vec2s_t Vec2s_Zero(void) {
	return Vec2s(0, 0);
}

/**
 * @brief
 */
vec3s_t Vec3s(int16_t x, int16_t y, int16_t z) {
	return (vec3s_t) {
		.x = x,
		.y = y,
		.z = z
	};
}

/**
 * @brief.
 */
vec3_t Vec3s_CastVec3(const vec3s_t v) {
	return (vec3_t) {
		.x = (float) v.x,
		.y = (float) v.y,
		.z = (float) v.z
	};
}

/**
 * @brief
 */
_Bool Vec3s_Equal(const vec3s_t a, vec3s_t b) {
	return a.x == b.x &&
		   a.y == b.y &&
		   a.z == b.z;
}

/**
 * @brief
 */
vec3s_t Vec3s_Zero(void) {
	return Vec3s(0, 0, 0);
}

#pragma mark - integer vectors

/**
 * @brief
 */
vec3i_t Vec3i(int32_t x, int32_t y, int32_t z) {
	return (vec3i_t) {
		.x = x,
		.y = y,
		.z = z
	};
}

/**
 * @brief
 */
vec3i_t Vec3i_Add(const vec3i_t a, const vec3i_t b) {
	return Vec3i(a.x + b.x, a.y + b.y, a.z + b.z);
}

/**
 * @brief
 */
vec3_t Vec3i_CastVec3(const vec3i_t v) {
	return Vec3((float) v.x, (float) v.y, (float) v.z);
}

/**
 * @brief
 */
vec3i_t Vec3i_Zero(void) {
	return Vec3i(0, 0, 0);
}

#pragma mark - single precision

#define RAD2DEG (180.f / M_PI)
#define DEG2RAD (M_PI / 180.f)

/**
 * @brief
 */
float Clampf(float f, float min, float max) {
	return Minf(Maxf(f, min), max);
}

/**
 * @brief
 */
float Degrees(float radians) {
	return radians * RAD2DEG;
}

/**
 * @brief
 */
_Bool EqualEpsilonf(float a, float b, float epsilon) {
	return fabsf(a - b) <= epsilon;
}

/**
 * @brief
 */
float Minf(float a, float b) {
	return a < b ? a : b;
}

/**
 * @brief
 */
float Maxf(float a, float b) {
	return a > b ? a : b;
}

/**
 * @brief
 */
float Radians(float degrees) {
	return degrees * DEG2RAD;
}

/**
 * @brief
 */
float RandomRangef(float begin, float end) {
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
float Randomf(void) {
	return g_random_double();
}

/**
 * @brief
 */
int32_t SignOf(float f) {
	return (f > 0.f) - (f < 0.f);
}

/**
 * @brief
 */
float Smoothf(float f, float min, float max) {
	const float s = Clampf((f - min) / (max - min), 0.f, 1.f);
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
	return EqualEpsilonf(a.x, b.x, epsilon) &&
		   EqualEpsilonf(a.y, b.y, epsilon);
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
	return Vec2(Maxf(a.x, b.x), Maxf(a.y, b.y));
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
	return Vec2(Minf(a.x, b.x), Minf(a.y, b.y));
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
vec3_t Vec3(float x, float y, float z) {
	return (vec3_t) { .x = x, .y = y, .z = z};
}

/**
 * @brief
 */
vec3_t Vec3_Absf(const vec3_t v) {
	return Vec3(fabsf(v.x), fabsf(v.y), fabsf(v.z));
}

/**
 * @brief
 */
vec3_t Vec3_Add(const vec3_t a, const vec3_t b) {
	return Vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

/**
 * @brief
 */
_Bool Vec3_BoxIntersect(const vec3_t amins, const vec3_t amaxs, const vec3_t bmins, const vec3_t bmaxs) {

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
vec3d_t Vec3_CastDVec3(const vec3_t v) {
	return (vec3d_t) {
		.x = (double) v.x,
		.y = (double) v.y,
		.z = (double) v.z
	};
}

/**
 * @brief
 */
vec3s_t Vec3_CastS16Vec3(const vec3_t v) {
	return (vec3s_t) {
		.x = (int16_t) v.x,
		.y = (int16_t) v.y,
		.z = (int16_t) v.z
	};
}

/**
 * @brief
 */
vec3i_t Vec3_CastS32Vec3(const vec3_t v) {
	return (vec3i_t) {
		.x = (int32_t) v.x,
		.y = (int32_t) v.y,
		.z = (int32_t) v.z
	};
}

/**
 * @brief
 */
vec3_t Vec3_Cross(const vec3_t a, const vec3_t b) {
	return Vec3(a.y * b.z - a.z * b.y,
				 a.z * b.x - a.x * b.z,
				 a.x * b.y - a.y * b.x);
}

/**
 * @brief
 */
vec3_t Vec3_Degrees(const vec3_t radians) {
	return Vec3_Scale(radians, RAD2DEG);
}

/**
 * @brief
 */
float Vec3_DistanceDir(const vec3_t a, const vec3_t b, vec3_t *dir) {
	float length;

	*dir = Vec3_NormalizeLength(Vec3_Subtract(a, b), &length);

	return length;
}

/**
 * @brief
 */
float Vec3_DistanceSquared(const vec3_t a, const vec3_t b) {
	return Vec3_LengthSquared(Vec3_Subtract(a, b));
}

/**
 * @brief
 */
float Vec3_Distance(const vec3_t a, const vec3_t b) {
	return Vec3_Length(Vec3_Subtract(a, b));
}

/**
 * @brief
 */
float Vec3_Dot(const vec3_t a, const vec3_t b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

/**
 * @brief
 */
vec3_t Vec3_Down(void) {
	return Vec3_Negate(Vec3_Up());
}

/**
 * @brief
 */
_Bool Vec3_EqualEpsilon(const vec3_t a, const vec3_t b, float epsilon) {
	return EqualEpsilonf(a.x, b.x, epsilon) &&
		   EqualEpsilonf(a.y, b.y, epsilon) &&
		   EqualEpsilonf(a.z, b.z, epsilon);
}

/**
 * @brief
 */
_Bool Vec3_Equal(const vec3_t a, const vec3_t b) {
	return Vec3_EqualEpsilon(a, b, __FLT_EPSILON__);
}

/**
 * @brief
 */
vec3_t Vec3_Euler(const vec3_t dir) {
	return Vec3_Degrees(Vec3(asinf(dir.z), atan2f(dir.y, dir.x), 0.f));
}

/**
 * @brief
 */
vec3_t Vec3_Forward(const vec3_t euler) {
	const vec3_t radians = Vec3_Radians(euler);
	return Vec3(cosf(radians.x) * cosf(radians.y),
				 cosf(radians.x) * sinf(radians.y),
				 sinf(radians.x) * -1.f);
}

/**
 * @brief
 */
float Vec3_LengthSquared(const vec3_t v) {
	return Vec3_Dot(v, v);
}

/**
 * @brief
 */
float Vec3_Length(const vec3_t v) {
	return sqrtf(Vec3_LengthSquared(v));
}

/**
 * @brief
 */
vec3_t Vec3_Maxf(const vec3_t a, const vec3_t b) {
	return Vec3(Maxf(a.x, b.x), Maxf(a.y, b.y), Maxf(a.z, b.z));
}

/**
 * @brief
 */
vec3_t Vec3_Maxs(void) {
	return Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
}

/**
 * @brief
 */
vec3_t Vec3_Minf(const vec3_t a, const vec3_t b) {
	return Vec3(Minf(a.x, b.x), Minf(a.y, b.y), Minf(a.z, b.z));
}

/**
 * @brief
 */
vec3_t Vec3_Mins(void) {
	return Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
}

/**
 * @brief
 */
vec3_t Vec3_MixEuler(const vec3_t a, const vec3_t b, float mix) {

	vec3_t delta = Vec3_Subtract(b, a);
	for (size_t i = 0; i < lengthof(delta.xyz); i++) {
		if (delta.xyz[i] > 180.f) {
			delta.xyz[i] -= 360.f;
		} else if (delta.xyz[i] < -180.f) {
			delta.xyz[i] += 360.f;
		}
	}

	return Vec3_Mix(a, delta, mix);
}

/**
 * @brief
 */
vec3_t Vec3_Mix(const vec3_t a, const vec3_t b, float mix) {
	return Vec3_Add(Vec3_Scale(a, 1.f - mix), Vec3_Scale(b, mix));
}

/**
 * @brief
 */
vec3_t Vec3_Multiply(const vec3_t a, const vec3_t b) {
	return Vec3(a.x * b.x, a.y * b.y, a.z * b.z);
}

/**
 * @brief
 */
vec3_t Vec3_Negate(const vec3_t v) {
	return Vec3_Scale(v, -1.f);
}

/**
 * @brief
 */
vec3_t Vec3_NormalizeLength(const vec3_t v, float *length) {
	*length = Vec3_Length(v);
	if (*length) {
		return Vec3_Scale(v, 1.f / *length);
	} else {
		return Vec3_Zero();
	}
}

/**
 * @brief
 */
vec3_t vec3_normalize(const vec3_t v) {
	float length;

	return Vec3_NormalizeLength(v, &length);
}

/**
 * @brief
 */
vec3_t Vec3_One(void) {
	return Vec3(1.f, 1.f, 1.f);
}

/**
 * @brief
 */
vec3_t Vec3_Radians(const vec3_t degrees) {
	return Vec3_Scale(degrees, DEG2RAD);
}

/**
 * @brief
 */
vec3_t Vec3_RandomRange(float begin, float end) {
	return Vec3(RandomRangef(begin, end),
				RandomRangef(begin, end),
				RandomRangef(begin, end));
}

/**
 * @brief
 */
vec3_t Vec3_Random(void) {
	return Vec3_RandomRange(0., 1.);
}

/**
 * @brief
 */
vec3_t vec3_reflect(const vec3_t a, const vec3_t b) {
	return Vec3_Add(a, Vec3_Scale(b, -2.0 * Vec3_Dot(a, b)));
}

/**
 * @brief
 */
vec3_t Vec3_Subtract(const vec3_t a, const vec3_t b) {
	return Vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

/**
 * @brief
 */
vec3_t Vec3_Scale(const vec3_t v, float scale) {
	return Vec3(v.x * scale, v.y * scale, v.z * scale);
}

/**
 * @brief
 */
void Vec3_Tangents(const vec3_t normal, const vec3_t sdir, const vec3_t tdir, vec3_t *tangent, vec3_t *bitangent) {

	const vec3_t s = vec3_normalize(sdir);
	const vec3_t t = vec3_normalize(tdir);

	*tangent = Vec3_Add(s, Vec3_Scale(normal, -Vec3_Dot(s, normal)));
	if (Vec3_Dot(s, *tangent) < 0.f) {
		*tangent = Vec3_Negate(*tangent);
	}

	*bitangent = Vec3_Cross(normal, *tangent);
	if (Vec3_Dot(t, *bitangent) < 0.f) {
		*bitangent = Vec3_Negate(*bitangent);
	}
}

/**
 * @brief
 */
vec4_t Vec3_ToVec4(const vec3_t v, float w) {
	return Vec4(v.x, v.y, v.z, w);
}

/**
 * @brief
 */
vec3_t Vec3_Up(void) {
	return Vec3(0.f, 0.f, 1.f);
}

/**
 * @brief
 */
void Vec3_Vectors(const vec3_t euler, vec3_t *forward, vec3_t *right, vec3_t *up) {

	const vec3_t f = Vec3_Forward(euler);
	if (forward) {
		*forward = f;
	}
	const vec3_t r = Vec3_Cross(f, Vec3_Up());
	if (right) {
		*right = r;
	}
	const vec3_t u = Vec3_Cross(f, r);
	if (up) {
		*up = u;
	}
}

/**
 * @brief
 */
vec2_t Vec3_XY(const vec3_t v) {
	return Vec2(v.x, v.y);
}

/**
 * @brief
 */
vec3_t Vec3_Zero(void) {
	return Vec3(0.f, 0.f, 0.f);
}

#pragma mark - vec4_t

/**
 * @brief
 */
vec4_t Vec4(float x, float y, float z, float w) {
	return (vec4_t) { .x = x, .y = y, .z = z, .w = w };
}

/**
 * @brief
 */
vec4_t Vec4_Add(const vec4_t a, const vec4_t b) {
	return Vec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

/**
 * @brief
 */
_Bool Vec4_EqualEpsilon(const vec4_t a, const vec4_t b, float epsilon) {
	return fabsf(a.x - b.x) <= epsilon &&
		   fabsf(a.y - b.y) <= epsilon &&
		   fabsf(a.z - b.z) <= epsilon &&
		   fabsf(a.w - b.w) <= epsilon;
}

/**
 * @brief
 */
_Bool Vec4_Equal(const vec4_t a, const vec4_t b) {
	return Vec4_EqualEpsilon(a, b, __FLT_EPSILON__);
}

/**
 * @brief
 */
vec4_t Vec4_Mix(const vec4_t a, const vec4_t b, float mix) {
	return Vec4_Add(Vec4_Scale(a, 1.f - mix), Vec4_Scale(b, mix));
}

/**
 * @brief
 */
vec4_t Vec4_One(void) {
	return Vec4(1.f, 1.f, 1.f, 1.f);
}

/**
 * @brief
 */
vec4_t Vec4_Scale(const vec4_t v, float scale) {
	return Vec4(v.x * scale, v.y * scale, v.z * scale, v.w * scale);
}

/**
 * @brief
 */
vec4_t Vec4_Subtract(const vec4_t a, const vec4_t b) {
	return Vec4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

/**
 * @brief
 */
vec3_t Vec4_XYZ(const vec4_t v) {
	return Vec3(v.x, v.y, v.z);
}

/**
 * @brief
 */
vec4_t Vec4_Zero(void) {
	return Vec4(0.f, 0.f, 0.f, 0.f);
}

#pragma mark - double precision

/**
 * @brief
 */
_Bool EqualEpsilon(double a, double b, double epsilon) {
	return fabs(a - b) <= epsilon;
}

#pragma mark - vec3d_t

/**
 * @brief
 */
vec3d_t Vec3d(double x, double y, double z) {
	return (vec3d_t) {
		.x = x,
		.y = y,
		.z = z
	};
}

/**
 * @brief
 */
vec3d_t Vec3d_Add(const vec3d_t a, const vec3d_t b) {
	return Vec3d(a.x + b.x, a.y + b.y, a.z + b.z);
}

/**
 * @brief
 */
vec3_t Vec3d_CastVec3(const vec3d_t v) {
	return Vec3(v.x, v.y, v.z);
}

/**
 * @brief
 */
vec3d_t Vec3d_Cross(const vec3d_t a, const vec3d_t b) {
	return Vec3d(a.y * b.z - a.z * b.y,
				 a.z * b.x - a.x * b.z,
				 a.x * b.y - a.y * b.x);
}

/**
 * @brief
 */
double Vec3d_DistanceSquared(const vec3d_t a, const vec3d_t b) {
	return Vec3d_LengthSquared(Vec3d_Subtract(a, b));
}

/**
 * @brief
 */
double Vec3d_Distance(const vec3d_t a, const vec3d_t b) {
	return Vec3d_Length(Vec3d_Subtract(a, b));
}

/**
 * @brief
 */
double Vec3d_Dot(const vec3d_t a, const vec3d_t b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

/**
 * @brief
 */
_Bool Vec3d_EqualEpsilon(const vec3d_t a, const vec3d_t b, double epsilon) {
	return EqualEpsilon(a.x, b.x, epsilon) &&
		   EqualEpsilon(a.y, b.y, epsilon) &&
		   EqualEpsilon(a.z, b.z, epsilon);
}

/**
 * @brief
 */
_Bool Vec3d_Equal(const vec3d_t a, const vec3d_t b) {
	return Vec3d_EqualEpsilon(a, b, __DBL_EPSILON__);
}

/**
 * @brief
 */
double Vec3d_LengthSquared(const vec3d_t v) {
	return Vec3d_Dot(v, v);
}

/**
 * @brief
 */
double Vec3d_Length(const vec3d_t v) {
	return sqrt(Vec3d_LengthSquared(v));
}

/**
 * @brief
 */
vec3d_t Vec3d_Normalize(const vec3d_t v) {
	const double length = Vec3d_Length(v);
	if (length) {
		return Vec3d_Scale(v, 1.0 / length);
	} else {
		return Vec3d_Zero();
	}
}

/**
 * @brief
 */
vec3d_t Vec3d_Scale(const vec3d_t v, double scale) {
	return Vec3d(v.x * scale, v.y * scale, v.z * scale);
}

/**
 * @brief
 */
vec3d_t Vec3d_Subtract(const vec3d_t a, const vec3d_t b) {
	return Vec3d(a.x - b.x, a.y - b.y, a.z - b.z);
}

/**
 * @brief
 */
vec3d_t Vec3d_Zero(void) {
	return Vec3d(0., 0., 0.);
}