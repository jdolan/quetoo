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

#include "material.h"
#include "bsp.h"

/**
 * @brief
 */
int32_t num_materials;
material_t materials[MAX_BSP_MATERIALS];

/**
 * @brief Finds the material with the specified name, allocating a new one if necessary.
 */
int32_t LoadMaterial(const char *name) {

  material_t *m = materials;
  for (int32_t i = 0; i < num_materials; i++, m++) {
    if (!g_strcmp0(name, m->cm->name)) {
      return i;
    }
  }

  if (num_materials == MAX_BSP_MATERIALS) {
    Com_Error(ERROR_FATAL, "MAX_BSP_MATERIALS\n");
  }

  m = materials + num_materials;
  num_materials++;

  m->cm = Cm_LoadMaterial(name, ASSET_CONTEXT_TEXTURES);

  m->diffusemap = Img_LoadSurface(m->cm->diffusemap.path);
  if (m->diffusemap) {
    Com_Verbose("Loaded %s\n", m->cm->diffusemap.path);
  } else {
    Com_Warn("Failed to resolve %s\n", name);
    m->diffusemap = Img_LoadSurface("textures/common/notex");
  }

  return (int32_t) (ptrdiff_t) (m - materials);
}

/**
 * @brief Frees all loaded materials.
 */
void FreeMaterials(void) {

  material_t *m = materials;
  for (int32_t i = 0; i < num_materials; i++, m++) {
    Cm_FreeMaterial(m->cm);
    SDL_DestroySurface(m->diffusemap);
  }

  num_materials = 0;
  memset(materials, 0, sizeof(materials));
}
