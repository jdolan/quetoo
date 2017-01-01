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

#include "cm_types.h"

typedef struct {
	uint32_t src, dest;
} cm_stage_blend_t;

typedef struct {
	vec_t hz;
} cm_stage_pulse_t;

typedef struct {
	vec_t hz;
	vec_t amp;
} cm_stage_stretch_t;

typedef struct {
	vec_t hz;
} cm_stage_rotate_t;

typedef struct {
	vec_t s, t;
} cm_stage_scroll_t;

typedef struct {
	vec_t s, t;
} cm_stage_scale_t;

typedef struct {
	vec_t floor, ceil;
	vec_t height;
} cm_stage_terrain_t;

typedef struct {
	vec_t intensity;
} cm_stage_dirt_t;

// frame based material animation, lerp between consecutive images
typedef struct {
	uint16_t num_frames;
	vec_t fps;
} cm_stage_anim_t;

typedef struct cm_stage_s {
	uint32_t flags;
	char image[MAX_QPATH];
	int32_t image_index;
	struct cm_material_s *material;
	cm_stage_blend_t blend;
	vec3_t color;
	cm_stage_pulse_t pulse;
	cm_stage_stretch_t stretch;
	cm_stage_rotate_t rotate;
	cm_stage_scroll_t scroll;
	cm_stage_scale_t scale;
	cm_stage_terrain_t terrain;
	cm_stage_dirt_t dirt;
	cm_stage_anim_t anim;
	struct cm_stage_s *next;
} cm_stage_t;

// stage flags will persist on the stage structures but may also bubble
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
#define STAGE_FOG				(1 << 16)

// set on stages eligible for static, dynamic, and per-pixel lighting
#define STAGE_LIGHTING			(1 << 30)

// set on stages with valid render passes
#define STAGE_DIFFUSE 			(1u << 31)

// composite mask for simplifying state management
#define STAGE_TEXTURE_MATRIX (\
                              STAGE_STRETCH | STAGE_ROTATE | STAGE_SCROLL_S | STAGE_SCROLL_T | STAGE_SCALE_S | STAGE_SCALE_T \
                             )

#define DEFAULT_BUMP 1.0
#define DEFAULT_PARALLAX 1.0
#define DEFAULT_HARDNESS 1.0
#define DEFAULT_SPECULAR 1.0

typedef struct cm_material_s {
	uint32_t ref_count; // refs for the cm system, do not touch
	char base[MAX_QPATH];
	char key[MAX_QPATH];

	char full_name[MAX_QPATH]; // the original full name of the material (#..., etc)
	char material_file[MAX_QPATH]; // the material file we got loaded from

	char diffuse[MAX_QPATH];
	char normalmap[MAX_QPATH];
	char specularmap[MAX_QPATH];
	uint32_t flags;
	vec_t bump;
	vec_t parallax;
	vec_t hardness;
	vec_t specular;
	cm_stage_t *stages;
	uint16_t num_stages;
} cm_material_t;

cm_material_t *Cm_FindMaterial(const char *diffuse);

_Bool Cm_UnrefMaterial(cm_material_t *mat);
void Cm_RefMaterial(cm_material_t *mat);

cm_material_t *Cm_LoadMaterial(const char *diffuse);
GArray *Cm_LoadMaterials(const char *path);
void Cm_UnloadMaterials(GArray *materials);

void Cm_WriteMaterials(void);

#ifdef __CM_LOCAL_H__
void Cm_ShutdownMaterials(void);
#endif /* __CM_LOCAL_H__ */
