/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#include "quake2world.h"

// math and trigonometry functions
float frand(void);  // 0 to 1
float crand(void);  // -1 to 1

void ClearBounds(vec3_t mins, vec3_t maxs);
void AddPointToBounds(const vec3_t v, vec3_t mins, vec3_t maxs);

boolean_t VectorCompare(const vec3_t v1, const vec3_t v2);
boolean_t VectorNearer(const vec3_t v1, const vec3_t v2, const vec3_t comp);

vec_t VectorNormalize(vec3_t v);  // returns vector length
vec_t VectorLength(const vec3_t v);
void VectorMix(const vec3_t v1, const vec3_t v2, const float mix, vec3_t out);
void VectorMA(const vec3_t veca, const vec_t scale, const vec3_t vecb, vec3_t vecc);
void CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross);

void ConcatRotations(vec3_t in1[3], vec3_t in2[3], vec3_t out[3]);

void VectorAngles(const vec3_t vector, vec3_t angles);
void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);

void VectorLerp(const vec3_t from, const vec3_t to, const vec_t frac, vec3_t out);
void AngleLerp(const vec3_t from, const vec3_t to, const vec_t frac, vec3_t out);

byte SignBitsForPlane(const c_bsp_plane_t *plane);
int BoxOnPlaneSide(const vec3_t emins, const vec3_t emaxs, const c_bsp_plane_t *plane);

void ProjectPointOnPlane(vec3_t dst, const vec3_t p, const vec3_t normal);
void PerpendicularVector(vec3_t dst, const vec3_t src);
void TangentVectors(const vec3_t normal, const vec3_t sdir, const vec3_t tdir, vec4_t tangent, vec3_t bitangent);
void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, float degrees);

// color functions
vec_t ColorNormalize(const vec3_t in, vec3_t out);
void ColorFilter(const vec3_t in, vec3_t out, float brightness, float saturation, float contrast);
int ColorByName(const char *s, int def);

// string functions
boolean_t GlobMatch(const char *pattern, const char *text);
boolean_t MixedCase(const char *s);
char *CommonPrefix(const char *words[], unsigned int nwords);
char *Lowercase(char *s);
char *Trim(char *s);
const char *Basename(const char *path);
void Dirname(const char *in, char *out);
void StripExtension(const char *in, char *out);
void StripColor(const char *in, char *out);
int StrColorCmp(const char *s1, const char *s2);
char *ParseToken(const char **data_p);
char *va(const char *format, ...) __attribute__((format(printf, 1, 2)));

// key / value info strings
#define MAX_USER_INFO_KEY		64
#define MAX_USER_INFO_VALUE		64
#define MAX_USER_INFO_STRING	512

char *GetUserInfo(const char *s, const char *key);
void DeleteUserInfo(char *s, const char *key);
void SetUserInfo(char *s, const char *key, const char *value);
boolean_t ValidateUserInfo(const char *s);

#endif /* __SHARED_H__ */
