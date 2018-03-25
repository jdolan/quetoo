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
#include "polylib.h"

#define DEFAULT_PATCH_SIZE 8.0

typedef struct patch_s {
	const bsp_face_t *face;
	winding_t *winding;
	struct patch_s *next;  // next in face
} patch_t;

extern patch_t *face_patches[MAX_BSP_FACES];
extern vec3_t face_offsets[MAX_BSP_FACES];

void BuildTextureColors(void);
void GetTextureColor(const char *name, vec3_t color);
void FreeTextureColors(void);
void BuildPatches(void);
void SubdividePatches(void);
