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
#include "materials.h"
#include "polylib.h"
#include "collision/cmodel.h"

#define PATCH_SIZE 4.0

typedef enum {
	LIGHT_POINT,
	LIGHT_SPOT,
	LIGHT_FACE,
} light_type_t;

typedef struct patch_s {
	bsp_face_t *face;
	winding_t *winding;

	vec3_t origin;
	vec3_t normal;

	vec_t light; // light radius in units
	vec3_t color; // emissive light color from surface texture

	struct patch_s *next;  // next in face
} patch_t;

extern patch_t *face_patches[MAX_BSP_FACES];
extern vec3_t face_offset[MAX_BSP_FACES]; // for rotating bmodels

extern int32_t indirect_bounces;
extern int32_t indirect_bounce;

// lightmap.c
void BuildLights(void);
void BuildIndirectLights(void);
void BuildFaceExtents(void);
void BuildVertexNormals(void);
void DirectLighting(int32_t face_num);
void IndirectLighting(int32_t face_num);
void FinalizeLighting(int32_t face_num);

// patches.c
void BuildTextureColors(void);
void GetTextureColor(const char *name, vec3_t color);
void FreeTextureColors(void);
void BuildPatches(void);
void SubdividePatches(void);
void FreePatches(void);

// qlight.c
_Bool Light_PointPVS(const vec3_t org, byte *pvs);
_Bool Light_InPVS(const vec3_t point1, const vec3_t point2);
int32_t Light_PointLeafnum(const vec3_t point);
void Light_Trace(cm_trace_t *trace, const vec3_t start, const vec3_t end, int32_t mask);
vec3_t *Light_AverageTextureColor(const char *name);
