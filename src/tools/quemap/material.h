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

#include "collision/cm_material.h"
#include "common/image.h"

#include "quemap.h"

typedef struct {
	bsp_material_t *out;
	cm_material_t *cm;
	SDL_Surface *diffusemap;
} material_t;

ssize_t LoadMaterials(const char *path);
int32_t EmitMaterial(const char *name);
material_t *GetMaterial(int32_t num);
ssize_t WriteMaterialsFile(const char *path);
void FreeMaterials(void);
