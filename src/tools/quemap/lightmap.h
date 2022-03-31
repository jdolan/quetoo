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

#include "luxel.h"

/**
 * @brief Lightmaps are per-face textures of lighting information.
 */
typedef struct {
	bsp_face_t *face;
	const bsp_node_t *node;
	const bsp_model_t *model;
	const bsp_brush_side_t *brush_side;
	const bsp_plane_t *plane;
	mat4_t matrix;
	mat4_t inverse_matrix;
	vec2_t st_mins, st_maxs;
	int16_t w, h;
	int16_t s, t;
	luxel_t *luxels;
	size_t num_luxels;
	SDL_Surface *ambient;
	SDL_Surface *diffuse;
	SDL_Surface *direction;
	SDL_Surface *caustics;
} lightmap_t;

extern lightmap_t *lightmaps;

void BuildLightmaps(void);
void BuildVertexNormals(void);
void DirectLightmap(int32_t face_num);
void IndirectLightmap(int32_t face_num);
void CausticsLightmap(int32_t face_num);
void FinalizeLightmap(int32_t face_num);
void EmitLightmap(void);
void EmitLightmapTexcoords(void);
