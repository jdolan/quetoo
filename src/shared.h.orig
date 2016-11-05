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

#ifndef __SHARED_H__
#define __SHARED_H__

#include "collision/cm_types.h"

/**
 * @brief The origin (0, 0, 0).
 */
extern vec3_t vec3_origin;

/**
 * @brief Up (0, 0, 1).
 */
extern vec3_t vec3_up;

/**
 * @brief Down (0, 0, -1).
 */
extern vec3_t vec3_down;

/**
 * @brief Forward (0, 1, 0).
 */
extern vec3_t vec3_forward;

/**
 * @brief Math library.
 */
#define Clamp(x, y, z)				(x < y ? y : x > z ? z : x)
#define DotProduct(x, y)			(x[0] * y[0] + x[1] * y[1] + x[2] * y[2])
#define VectorCompare(a, b)			(a[0] == b[0] && a[1] == b[1] && a[2] == b[2])
#define Vector4Compare(a, b)		(a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3])
#define VectorAdd(a,b,c)			(c[0] = a[0] + b[0], c[1] = a[1] + b[1], c[2] = a[2] + b[2])
#define VectorSubtract(a, b, c)		(c[0] = a[0] - b[0], c[1] = a[1] - b[1], c[2] = a[2] - b[2])
#define VectorScale(a, s, b)		(b[0] = a[0] * (s), b[1] = a[1] * (s), b[2] = a[2] * (s))
#define Vector2Copy(a, b)			((b)[0] = (a)[0], (b)[1] = (a)[1])
#define VectorCopy(a, b)			((b)[0] = (a)[0], (b)[1] = (a)[1], (b)[2] = (a)[2])
#define Vector4Copy(a, b)			((b)[0] = (a)[0], (b)[1] = (a)[1], (b)[2] = (a)[2], (b)[3] = (a)[3])
#define VectorClear(a)				(a[0] = a[1] = a[2] = 0.0)
#define Vector4Clear(a)				(a[0] = a[1] = a[2] = a[3] = 0.0)
#define VectorNegate(a, b)			(b[0] = -a[0], b[1] = -a[1], b[2] = -a[2])
#define VectorSet(v, x, y, z)		(v[0] = (x), v[1] = (y), v[2] = (z))
#define Vector4Set(v, x, y, z, w) 	(v[0] = (x), v[1] = (y), v[2] = (z), v[3] = (w))
#define VectorSum(a)				(a[0] + a[1] + a[2])
#define Radians(d) 					((d) * 0.01745329251) // * M_PI / 180.0
#define Degrees(r)					((r) * 57.2957795131) // * 180.0 / M_PI

/**
 * @brief Math and trigonometry functions.
 */
int32_t Random(void); // 0 to (2^32)-1
vec_t Randomf(void); // 0.0 to 1.0
vec_t Randomc(void); // -1.0 to 1.0

void ClearBounds(vec3_t mins, vec3_t maxs);
void AddPointToBounds(const vec3_t v, vec3_t mins, vec3_t maxs);
vec_t RadiusFromBounds(const vec3_t mins, const vec3_t maxs);

vec_t VectorNormalize(vec3_t v);
vec_t VectorNormalize2(const vec3_t in, vec3_t out);
vec_t VectorLengthSquared(const vec3_t v);
vec_t VectorLength(const vec3_t v);
void VectorMix(const vec3_t v1, const vec3_t v2, const vec_t mix, vec3_t out);
void VectorMA(const vec3_t veca, const vec_t scale, const vec3_t vecb, vec3_t vecc);
void CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross);

void VectorAngles(const vec3_t vector, vec3_t angles);
void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);

void VectorLerp(const vec3_t from, const vec3_t to, const vec_t frac, vec3_t out);
void AngleLerp(const vec3_t from, const vec3_t to, const vec_t frac, vec3_t out);

_Bool BoxIntersect(const vec3_t mins0, const vec3_t maxs0, const vec3_t mins1, const vec3_t maxs1);
void ProjectPointOnPlane(const vec3_t p, const vec3_t normal, vec3_t out);
void PerpendicularVector(const vec3_t in, vec3_t out);
void TangentVectors(const vec3_t normal, const vec3_t sdir, const vec3_t tdir, vec4_t tangent,
		vec3_t bitangent);
void RotatePointAroundVector(const vec3_t p, const vec3_t dir, const vec_t degrees, vec3_t out);

/**
 * @brief A table of approximate normal vectors is used to save bandwidth when
 * transmitting entity angles, which would otherwise require 12 bytes.
 */
#define NUM_APPROXIMATE_NORMALS 162
extern const vec3_t approximate_normals[NUM_APPROXIMATE_NORMALS];

/**
 * @brief Serialization helpers.
 */
void PackVector(const vec3_t in, int16_t *out);
void UnpackVector(const int16_t *in, vec3_t out);
uint16_t PackAngle(const vec_t a);
void PackAngles(const vec3_t in, uint16_t *out);
vec_t UnpackAngle(const uint16_t a);
void UnpackAngles(const uint16_t *in, vec3_t out);
vec_t ClampAngle(vec_t angle);
void ClampAngles(vec3_t angles);
void PackBounds(const vec3_t mins, const vec3_t maxs, uint16_t *out);
void UnpackBounds(const uint16_t in, vec3_t mins, vec3_t maxs);

/**
 * @brief Color manipulating.
 */
vec_t ColorNormalize(const vec3_t in, vec3_t out);
void ColorFilter(const vec3_t in, vec3_t out, vec_t brightness, vec_t saturation, vec_t contrast);

/**
 * @brief String manipulation functions.
 */
_Bool GlobMatch(const char *pattern, const char *text);
char *CommonPrefix(GList *words);
const char *Basename(const char *path);
void Dirname(const char *in, char *out);
void StripExtension(const char *in, char *out);
void StripColors(const char *in, char *out);
size_t StrColorLen(const char *s);
int32_t StrColorCmp(const char *s1, const char *s2);
int32_t StrColor(const char *s);
int32_t StrrColor(const char *s);
char *ParseToken(const char **data_p);
char *va(const char *format, ...) __attribute__((format(printf, 1, 2)));
char *vtos(const vec3_t v);

// a cute little hack for printing g_entity_t
#define etos(e) (e ? va("%u: %s @ %s", e->s.number, e->class_name, vtos(e->s.origin)) : "null")

// key / value info strings
#define MAX_USER_INFO_KEY		64
#define MAX_USER_INFO_VALUE		64
#define MAX_USER_INFO_STRING	512

char *GetUserInfo(const char *s, const char *key);
void DeleteUserInfo(char *s, const char *key);
void SetUserInfo(char *s, const char *key, const char *value);
_Bool ValidateUserInfo(const char *s);

#endif /* __SHARED_H__ */
