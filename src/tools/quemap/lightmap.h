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

#include "quemap.h"
#include "atlas.h"
#include "matrix.h"

typedef struct {
	int32_t s, t;
	vec3_t origin;
	vec3_t normal;
	vec3_t direct;
	vec3_t direction;
	vec3_t indirect;
} luxel_t;

typedef struct {
	bsp_face_t *face;
	const bsp_leaf_t *leaf;
	const bsp_plane_t *plane;
	const bsp_texinfo_t *texinfo;
	const cm_material_t *material;
	matrix4x4_t matrix;
	matrix4x4_t inverse_matrix;
	vec2_t st_mins, st_maxs;
	int16_t w, h;
	int16_t s, t;
	luxel_t *luxels;
	size_t num_luxels;
	SDL_Surface *lightmap;
	SDL_Surface *deluxemap;
} lightmap_t;

extern lightmap_t lightmaps[MAX_BSP_FACES];

void BuildLightmaps(void);
void BuildVertexNormals(void);
void DirectLighting(int32_t face_num);
void IndirectLighting(int32_t face_num);
void FinalizeLighting(int32_t face_num);
void EmitLightmaps(void);
void EmitLightmapTexcoords(void);
