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
 * @brief Allocates a material_t for the specified name.
 * @details Attempts to seed the material from its per-texture .mat file if
 * one exists, otherwise allocates a fresh material for suffix-based resolution.
 */
static const char *StripTexturesPrefix(const char *name) {
  return g_str_has_prefix(name, "textures/") ? name + strlen("textures/") : name;
}

static material_t *AllocMaterial(const char *name) {

  if (num_materials == MAX_BSP_MATERIALS) {
    Com_Error(ERROR_FATAL, "MAX_BSP_MATERIALS\n");
  }

  name = StripTexturesPrefix(name);

  material_t *material = materials + num_materials;
  num_materials++;

  g_strlcpy(material->name, name, sizeof(material->name));

  char path[MAX_QPATH];
  g_snprintf(path, sizeof(path), "textures/%s.mat", name);

  material->cm = Cm_LoadMaterial(path, ASSET_CONTEXT_TEXTURES);
  if (material->cm == NULL) {
    material->cm = Cm_AllocMaterial(name, ASSET_CONTEXT_TEXTURES);
  }

  return material;
}

/**
 * @brief
 */
static void FreeMaterial(material_t *material) {

  Cm_FreeMaterial(material->cm);

  SDL_DestroySurface(material->diffusemap);
}

/**
 * @brief
 */
static material_t *LoadMaterial(const char *name) {

  material_t *material = AllocMaterial(name);

  if (Cm_ResolveMaterial(material->cm)) {
    material->diffusemap = Img_LoadSurface(material->cm->diffusemap.path);
    if (material->diffusemap) {
      Com_Verbose("Loaded %s\n", material->cm->diffusemap.path);
      material->ambient = Img_Color(material->diffusemap).vec3;
    } else {
      Com_Warn("Failed to load %s\n", material->cm->diffusemap.path);
    }
  } else {
    Com_Warn("Failed to resolve %s\n", name);
  }

  material->diffusemap = material->diffusemap ?: Img_LoadSurface("textures/common/notex");

  return material;
}

/**
 * @brief Loads all BSP materials, seeding each from its per-texture .mat file
 * if one exists, then falling back to suffix-based asset resolution.
 */
void LoadMaterials(void) {

  const bsp_material_t *bsp = bsp_file.materials;
  for (int32_t i = 0; i < bsp_file.num_materials; i++, bsp++) {
    LoadMaterial(bsp->name);
  }
}

/**
 * @brief Finds the material with the specified name, allocating a new one if necessary.
 */
int32_t FindMaterial(const char *name) {

  name = StripTexturesPrefix(name);

  material_t *material = NULL;
  for (int32_t i = 0; i < num_materials; i++) {
    if (!g_strcmp0(name, materials[i].name)) {
      material = &materials[i];
      break;
    }
  }

  if (material == NULL) {
    material = LoadMaterial(name);
  }

  return (int32_t) (ptrdiff_t) (material - materials);
}

/**
 * @brief Frees all loaded materials.
 */
void FreeMaterials(void) {

  for (int32_t i = 0; i < num_materials; i++) {
    FreeMaterial(materials + i);
  }

  num_materials = 0;
  memset(materials, 0, sizeof(materials));
}
