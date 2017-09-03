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

#include "collision/cm_types.h"

/**
 * @brief The origin (0, 0, 0).
 */
extern const vec3_t vec3_origin;

/**
 * @brief Up (0, 0, 1).
 */
extern const vec3_t vec3_up;

/**
 * @brief Down (0, 0, -1).
 */
extern const vec3_t vec3_down;

/**
 * @brief Forward (0, 1, 0).
 */
extern const vec3_t vec3_forward;

/**
 * @brief Math library.
 */
/**
 * @return The value, clamped to the bounds.
 */
#define Clamp(val, min, max) \
	({ \
		typeof(val) _val = (val); typeof(min) _min = (min); typeof(max) _max = (max); \
		_val < _min ? _min : _val > _max ? _max : _val; \
	})

/**
 * @return The maximum of the two values.
 */
#ifndef Max
	#define Max(a, b) ({ typeof (a) _a = (a); typeof (b) _b = (b); _a > _b ? _a : _b; })
#endif

/**
 * @return The minimum of the two values.
 */
#ifndef Min
	#define Min(a, b) ({ typeof (a) _a = (a); typeof (b) _b = (b); _a < _b ? _a : _b; })
#endif

/**
 * @return The sign (-1, 0 or 1) of the value.
 */
#ifndef Sign
	#define Sign(a) ({ typeof (a) _a = (a); ((_a > 0) - (_a < 0)); })
#endif

#define DotProduct(x, y)			(x[0] * y[0] + x[1] * y[1] + x[2] * y[2])
#define VectorCompare(a, b)			(a[0] == b[0] && a[1] == b[1] && a[2] == b[2])
#define Vector4Compare(a, b)		(a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3])
#define VectorAdd(a,b,c)			(c[0] = a[0] + b[0], c[1] = a[1] + b[1], c[2] = a[2] + b[2])
#define VectorSubtract(a, b, c)		(c[0] = a[0] - b[0], c[1] = a[1] - b[1], c[2] = a[2] - b[2])
#define VectorScale(a, s, b)		(b[0] = a[0] * (s), b[1] = a[1] * (s), b[2] = a[2] * (s))
#define Vector2Copy(a, b)			((b)[0] = (a)[0], (b)[1] = (a)[1])
#define Vector2Set(v, x, y)         (v[0] = (x), v[1] = (y))
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

#define NearestMultiple(n, align)	((n) == 0 ? 0 : ((n) - 1 - ((n) - 1) % (align) + (align)))

/*
 * @brief Z origin offset for sounds; has a 4x multiplier at sound spatialization time
 *  Uses the upper 4 bits of the attenuation byte
 */
#define S_GET_Z_ORIGIN_OFFSET(atten) (((atten & 0xf0) >> 4) - 8)
#define S_SET_Z_ORIGIN_OFFSET(offset) ((offset + 8) << 4)

/**
 * @brief Math and trigonometry functions.
 */
int32_t Random(void); // 0 to (2^32)-1
vec_t Randomf(void); // 0.0 to 1.0
vec_t Randomc(void); // -1.0 to 1.0
vec_t Randomfr(const vec_t min, const vec_t max); // min to max
int32_t Randomr(const int32_t min, const int32_t max); // min to max

void ClearBounds(vec3_t mins, vec3_t maxs);
void AddPointToBounds(const vec3_t v, vec3_t mins, vec3_t maxs);
vec_t RadiusFromBounds(const vec3_t mins, const vec3_t maxs);

vec_t VectorNormalize(vec3_t v);
vec_t VectorNormalize2(const vec3_t in, vec3_t out);
vec_t VectorLengthSquared(const vec3_t v);
vec_t VectorLength(const vec3_t v);
vec_t VectorDistanceSquared(const vec3_t a, const vec3_t b);
vec_t VectorDistance(const vec3_t a, const vec3_t b);
void VectorMix(const vec3_t v1, const vec3_t v2, const vec_t mix, vec3_t out);
void VectorMA(const vec3_t veca, const vec_t scale, const vec3_t vecb, vec3_t vecc);
void CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross);

void VectorAngles(const vec3_t vector, vec3_t angles);
void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);

#define Lerp(from, to, frac) ((from) + (frac) * ((to) - (from)))

void VectorLerp(const vec3_t from, const vec3_t to, const vec_t frac, vec3_t out);
void Vector4Lerp(const vec4_t from, const vec4_t to, const vec_t frac, vec4_t out);

vec_t AngleLerp(vec_t from, vec_t to, const vec_t frac);
void AnglesLerp(const vec3_t from, const vec3_t to, const vec_t frac, vec3_t out);

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
vec_t UnclampAngle(vec_t angle);
void ClampAngles(vec3_t angles);
void PackBounds(const vec3_t mins, const vec3_t maxs, uint32_t *out);
void UnpackBounds(const uint32_t in, vec3_t mins, vec3_t maxs);
u16vec_t PackTexcoord(const vec_t in);
void PackTexcoords(const vec2_t in, u16vec2_t out);

/**
 * @brief A 32-bit RGBA color
 */
typedef union {
	struct {
		byte r, g, b, a;
	}; // as separate components

	byte bytes[4]; // as four bytes, for loopage

	uint32_t u32; // as a full uint32_t
} color_t;

#define ColorFromRGBA(rr, gg, bb, aa) ({ color_t _c; _c.r = rr; _c.g = gg; _c.b = bb; _c.a = aa; _c; })
#define ColorFromRGB(r, g, b) ColorFromRGBA(r, g, b, 255)
#define ColorFromU32(v) { color_t _c; _c.u32 = v; _c; }

/**
 * @brief Color manipulating.
 */
vec_t ColorNormalize(const vec3_t in, vec3_t out);
void ColorFilter(const vec3_t in, vec3_t out, vec_t brightness, vec_t saturation, vec_t contrast);
void ColorDecompose(const vec4_t in, u8vec4_t out);
void ColorDecompose3(const vec3_t in, u8vec3_t out);

/**
 * @brief String manipulation functions.
 */
typedef enum {
	GLOB_FLAGS_NONE = 0,
	GLOB_CASE_INSENSITIVE = (1 << 0)
} glob_flags_t;

_Bool GlobMatch(const char *pattern, const char *text, const glob_flags_t flags);
const char *Basename(const char *path);
void Dirname(const char *in, char *out);
void StripNewline(const char *in, char *out);
void StripExtension(const char *in, char *out);
void StripColors(const char *in, char *out);
size_t StrColorLen(const char *s);
int32_t StrColorCmp(const char *s1, const char *s2);
int32_t StrColor(const char *s);
int32_t StrrColor(const char *s);
char *va(const char *format, ...) __attribute__((format(printf, 1, 2)));
char *vtos(const vec3_t v);

// a cute little hack for printing g_entity_t
#define etos(e) (e ? va("%u: %s @ %s", e->s.number, e->class_name, vtos(e->s.origin)) : "null")

// key / value info strings
#define MAX_USER_INFO_KEY		64
#define MAX_USER_INFO_VALUE		64
#define MAX_USER_INFO_STRING	512

// max name of a team
#define MAX_TEAM_NAME			32

char *GetUserInfo(const char *s, const char *key);
void DeleteUserInfo(char *s, const char *key);
void SetUserInfo(char *s, const char *key, const char *value);
_Bool ValidateUserInfo(const char *s);

/**
 * @brief Color manipulation functions
 */
_Bool ColorParseHex(const char *s, color_t *color);
_Bool ColorToHex(const color_t color, char *s, const size_t s_len);
void ColorToVec3(const color_t color, vec3_t vec);
void ColorToVec4(const color_t color, vec4_t vec);
void ColorFromVec3(const vec3_t vec, color_t *color);
void ColorFromVec4(const vec4_t vec, color_t *color);
color_t ColorFromHSV(const vec3_t hsv);

// max length of a color string is "rrggbbaa\0" - "default" fits in here
#define COLOR_MAX_LENGTH		9

gboolean g_stri_equal (gconstpointer v1, gconstpointer v2);
guint g_stri_hash (gconstpointer v);
