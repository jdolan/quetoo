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

#ifndef __POLYLIB_H__
#define __POLYLIB_H__

typedef struct {
	int numpoints;
	vec3_t p[4];  // variable sized
} winding_t;

#define	MAX_POINTS_ON_WINDING	64

#ifndef	ON_EPSILON
#define	ON_EPSILON	0.1
#endif

winding_t *AllocWinding(int points);
vec_t WindingArea(const winding_t *w);
void WindingCenter(const winding_t *w, vec3_t center);
void ClipWindingEpsilon(const winding_t *in, vec3_t normal, vec_t dist,
		vec_t epsilon, winding_t **front, winding_t **back);
winding_t *ChopWinding(winding_t *in, vec3_t normal, vec_t dist);
winding_t *CopyWinding(const winding_t *w);
winding_t *ReverseWinding(winding_t *w);
winding_t *BaseWindingForPlane(const vec3_t normal, const vec_t dist);
void WindingPlane(const winding_t *w, vec3_t normal, vec_t *dist);
void RemoveColinearPoints(winding_t *w);
void FreeWinding(winding_t *w);
void WindingBounds(const winding_t *w, vec3_t mins, vec3_t maxs);

void ChopWindingInPlace(winding_t **w, const vec3_t normal, const vec_t dist, const vec_t epsilon);
// frees the original if clipped

#endif /* __POLYLIB_H__ */
