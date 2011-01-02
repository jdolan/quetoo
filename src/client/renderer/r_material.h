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

typedef struct r_stage_blend_s {
	GLenum src, dest;
} r_stage_blend_t;

typedef struct r_stage_pulse_s {
	float hz, dhz;
} r_stage_pulse_t;

typedef struct r_stage_stretch_s {
	float hz, dhz;
	float amp, damp;
} r_stage_stretch_t;

typedef struct r_stage_rotate_s {
	float hz, deg;
} r_stage_rotate_t;

typedef struct r_stage_scroll_s {
	float s, t;
	float ds, dt;
} r_stage_scroll_t;

typedef struct r_stage_scale_s {
	float s, t;
} r_stage_scale_t;

typedef struct r_stage_terrain_s {
	float floor, ceil;
	float height;
} r_stage_terrain_t;

typedef struct r_stage_dirt_s {
	float intensity;
} r_stage_dirt_t;

typedef struct r_stage_anim_s {
	int num_frames;
	struct r_image_s *images[MAX_ANIM_FRAMES];
	float fps;
	float dtime;
	int dframe;
} r_stage_anim_t;

typedef struct r_stage_s {
	unsigned flags;
	struct r_image_s *image;
	r_stage_blend_t blend;
	vec3_t color;
	r_stage_pulse_t pulse;
	r_stage_stretch_t stretch;
	r_stage_rotate_t rotate;
	r_stage_scroll_t scroll;
	r_stage_scale_t scale;
	r_stage_terrain_t terrain;
	r_stage_dirt_t dirt;
	r_stage_anim_t anim;
	struct r_stage_s *next;
} r_stage_t;

#define DEFAULT_BUMP 1.0
#define DEFAULT_PARALLAX 1.0
#define DEFAULT_HARDNESS 1.0
#define DEFAULT_SPECULAR 1.0

typedef struct r_material_s {
	unsigned flags;
	float time;
	float bump;
	float parallax;
	float hardness;
	float specular;
	r_stage_t *stages;
	int num_stages;
} r_material_t;

#endif /*__R_MATERIAL_H__*/
