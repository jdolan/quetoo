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

#include "bspfile.h"

/**
 * @brief Face texture extents.
 */
typedef struct {
	const bsp_face_t *face;
	const bsp_texinfo_t *texinfo;
	vec3_t offset;
	vec3_t mins, maxs;
	vec3_t center;
	vec3_t normal;
	vec2_t st_mins, st_maxs;
	vec3_t st_origin;
	vec3_t st_normal;
	vec3_t st_tangent;
	vec3_t st_bitangent;
	s16vec2_t lm_mins, lm_maxs, lm_size;
	size_t num_luxels;
	vec_t *origins;
	vec_t *normals;
	vec_t *direct;
	vec_t *directions;
	vec_t *indirect;
} face_lighting_t;

extern face_lighting_t face_lighting[MAX_BSP_FACES];

void BuildFaceLighting(void);
void BuildVertexNormals(void);
void DirectLighting(int32_t face_num);
void IndirectLighting(int32_t face_num);
void FinalizeLighting(int32_t face_num);
