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

typedef struct {
	box3_t stu_bounds;
	vec3i_t size;

	mat4_t matrix;
	mat4_t inverse_matrix;

	size_t num_luxels;
	luxel_t *luxels;
} lightgrid_t;

extern lightgrid_t lg;

size_t BuildLightgrid(void);
void DirectLightgrid(int32_t luxel_num);
void IndirectLightgrid(int32_t luxel_num);
void CausticsLightgrid(int32_t luxel_num);
void FogLightgrid(int32_t luxel_num);
void FinalizeLightgrid(int32_t luxel_num);
void EmitLightgrid(void);
