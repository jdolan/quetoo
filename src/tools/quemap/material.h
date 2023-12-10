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

/**
 * @brief The quemap representation of materials.
 */
typedef struct {
	/**
	 * @brief The collision material backing this material.
	 */
	cm_material_t *cm;

	/**
	 * @brief The diffusemap texture.
	 */
	SDL_Surface *diffusemap;

	/**
	 * @brief The ambient emissive color for patch lights.
	 * @remarks This is mulitiplied by the direct diffuse lightmap value to reflect indirect light.
	 */
	vec3_t ambient;

	/**
	 * @brief The diffuse color for face lights.
	 */
	vec3_t diffuse;

} material_t;

extern int32_t num_materials;
extern material_t materials[MAX_BSP_MATERIALS];

void LoadMaterials(const char *path);
int32_t FindMaterial(const char *name);
ssize_t WriteMaterialsFile(const char *path);
void FreeMaterials(void);
