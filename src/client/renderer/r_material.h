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

#ifndef __R_MATERIAL_H__
#define __R_MATERIAL_H__

#include "common.h"

// flags will persist on the stage structures but may also bubble
// up to the material flags to determine render eligibility
#define STAGE_TEXTURE			(1 << 0)
#define STAGE_ENVMAP			(1 << 1)
#define STAGE_BLEND				(1 << 2)
#define STAGE_COLOR				(1 << 3)
#define STAGE_PULSE				(1 << 4)
#define STAGE_STRETCH			(1 << 5)
#define STAGE_ROTATE			(1 << 6)
#define STAGE_SCROLL_S			(1 << 7)
#define STAGE_SCROLL_T			(1 << 8)
#define STAGE_SCALE_S			(1 << 9)
#define STAGE_SCALE_T			(1 << 10)
#define STAGE_TERRAIN			(1 << 11)
#define STAGE_ANIM				(1 << 12)
#define STAGE_LIGHTMAP			(1 << 13)
#define STAGE_DIRTMAP			(1 << 14)
#define STAGE_FLARE				(1 << 15)

// set on stages eligible for static, dynamic, and per-pixel lighting
#define STAGE_LIGHTING			(1 << 30)

// set on stages with valid render passes
#define STAGE_RENDER 			(1 << 31)

// composite mask for simplifying state management
#define STAGE_TEXTURE_MATRIX (\
		STAGE_STRETCH | STAGE_ROTATE | STAGE_SCROLL_S | STAGE_SCROLL_T | \
		STAGE_SCALE_S | STAGE_SCALE_T \
)

// frame based animation, lerp between consecutive images
#define MAX_ANIM_FRAMES 8

typedef struct blend_s {
	GLenum src, dest;
} blend_t;

typedef struct pulse_s {
	float hz, dhz;
} pulse_t;

typedef struct stretch_s {
	float hz, dhz;
	float amp, damp;
} stretch_t;

typedef struct rotate_s {
	float hz, deg;
} rotate_t;

typedef struct scroll_s {
	float s, t;
	float ds, dt;
} scroll_t;

typedef struct scale_s {
	float s, t;
} scale_t;

typedef struct terrain_s {
	float floor, ceil;
	float height;
} terrain_t;

typedef struct dirt_s {
	float intensity;
} dirt_t;

typedef struct anim_s {
	int num_frames;
	struct image_s *images[MAX_ANIM_FRAMES];
	float fps;
	float dtime;
	int dframe;
} anim_t;

typedef struct stage_s {
	unsigned flags;
	struct image_s *image;
	blend_t blend;
	vec3_t color;
	pulse_t pulse;
	stretch_t stretch;
	rotate_t rotate;
	scroll_t scroll;
	scale_t scale;
	terrain_t terrain;
	dirt_t dirt;
	anim_t anim;
	struct stage_s *next;
} stage_t;

#define DEFAULT_BUMP 1.0
#define DEFAULT_PARALLAX 1.0
#define DEFAULT_HARDNESS 1.0
#define DEFAULT_SPECULAR 1.0

typedef struct material_s {
	unsigned flags;
	float time;
	float bump;
	float parallax;
	float hardness;
	float specular;
	stage_t *stages;
	int num_stages;
} material_t;

#endif /*__R_MATERIAL_H__*/
