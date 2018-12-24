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

#include <float.h>

#include "quemap.h"

typedef struct {
	uint16_t num_points;
	vec3_t points[4]; // variable sized
} winding_t;

#define	MAX_POINTS_ON_WINDING	64

#define	CLIP_EPSILON 0.1

#ifndef	ON_EPSILON
	#define	ON_EPSILON	0.1
#endif

winding_t *AllocWinding(int32_t points);
vec_t WindingArea(const winding_t *w);
void WindingCenter(const winding_t *w, vec3_t center);
_Bool WindingIsTiny(const winding_t *w);
_Bool WindingIsHuge(const winding_t *w);
void ClipWindingEpsilon(const winding_t *in, vec3_t normal, vec_t dist, vec_t epsilon, winding_t **front, winding_t **back);
winding_t *CopyWinding(const winding_t *w);
winding_t *ReverseWinding(winding_t *w);
winding_t *WindingForPlane(const vec3_t normal, const vec_t dist);
winding_t *WindingForFace(const bsp_face_t *f);
void WindingPlane(const winding_t *w, vec3_t normal, vec_t *dist);
void RemoveColinearPoints(winding_t *w);
void FreeWinding(winding_t *w);
void WindingBounds(const winding_t *w, vec3_t mins, vec3_t maxs);
void ChopWindingInPlace(winding_t **w, const vec3_t normal, const vec_t dist, const vec_t epsilon);
// frees the original if clipped
