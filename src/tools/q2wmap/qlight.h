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

#ifndef __QLIGHT_H__
#define __QLIGHT_H__

#include "bspfile.h"
#include "polylib.h"
#include "cmodel.h"

typedef enum {
    emit_surface,
    emit_point,
    emit_spotlight
} emittype_t;

typedef struct patch_s {
	d_bsp_face_t *face;
	winding_t *winding;

	vec3_t origin;
	vec3_t normal;

	float area;
	vec3_t light;  // emissive surface light

	struct patch_s *next;  // next in face
} patch_t;

extern patch_t *face_patches[MAX_BSP_FACES];
extern vec3_t face_offset[MAX_BSP_FACES];  // for rotating bmodels

extern float brightness;
extern float saturation;
extern float contrast;

extern float surface_scale;
extern float entity_scale;

extern vec3_t ambient;

extern boolean_t extra_samples;

void BuildLightmaps(void);

void BuildVertexNormals(void);

void BuildFacelights(int facenum);

void FinalLightFace(int facenum);

boolean_t PvsForOrigin(const vec3_t org, byte *pvs);

void BuildLights(void);

d_bsp_leaf_t *Light_PointInLeaf(const vec3_t point);

void Light_Trace(c_trace_t *trace, const vec3_t start, const vec3_t end, int mask);

// patches.c
void CalcTextureReflectivity(void);
void BuildPatches(void);
void SubdividePatches(void);
void FreePatches(void);

#endif /* __QLIGHT_H__ */
