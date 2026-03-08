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
static material_t *AllocMaterial(const char *name) {

  if (bsp_file.num_materials == MAX_BSP_MATERIALS) {
    Com_Error(ERROR_FATAL, "MAX_BSP_MATERIALS\n");
  }

  material_t *material = materials + num_materials;
  num_materials++;

  char path[MAX_QPATH];
  g_snprintf(path, sizeof(path), "textures/%s.mat", name);

  GList *list = NULL;
  if (Cm_LoadMaterials(path, &list) > 0) {
    material->cm = list->data;
    g_list_free(list);
  } else {
    material->cm = Cm_AllocMaterial(name);
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

  if (Cm_ResolveMaterial(material->cm, ASSET_CONTEXT_TEXTURES)) {
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

  material_t *material = NULL;
  for (int32_t i = 0; i < num_materials; i++) {
    if (!g_strcmp0(name, materials[i].cm->name)) {
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

/**
 * @brief Writes per-texture .mat files for all known materials.
 * @details Non-destructive: skips any material whose .mat file already exists.
 * Only writes newly discovered textures that lack an authored .mat file.
 */
ssize_t WriteMaterialsFile(void) {

  ssize_t count = 0;
  material_t *mat = materials;
  for (int32_t i = 0; i < num_materials; i++, mat++) {
    char path[MAX_QPATH];
    g_snprintf(path, sizeof(path), "textures/%s.mat", mat->cm->name);

    if (Fs_Exists(path)) {
      Com_Debug(DEBUG_ALL, "Skipping existing %s\n", path);
      continue;
    }

    g_strlcpy(mat->cm->path, path, sizeof(mat->cm->path));
    GList *single = g_list_prepend(NULL, mat->cm);
    if (Cm_WriteMaterials(path, single) > 0) {
      count++;
    }
    g_list_free(single);
  }
  return count;
}
