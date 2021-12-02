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
 * @brief The collision materials defined in maps/my.mat.
 */
static GList *mat_file;

/**
 * @brief Allocates a material_t for the specified name.
 */
static material_t *AllocMaterial(const char *name) {

	if (bsp_file.num_materials == MAX_BSP_MATERIALS) {
		Com_Error(ERROR_FATAL, "MAX_BSP_MATERIALS\n");
	}

	material_t *material = materials + num_materials;
	num_materials++;

	for (GList *e = mat_file; e; e = e->next) {
		cm_material_t *cm = e->data;
		if (!g_strcmp0(name, cm->name)) {
			material->cm = cm;
			break;
		}
	}

	if (material->cm == NULL) {
		material->cm = Cm_AllocMaterial(name);
	}

	return material;
}

/**
 * @brief
 */
static void FreeMaterial(material_t *material) {

	Cm_FreeMaterial(material->cm);

	SDL_FreeSurface(material->diffusemap);
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
 * @brief Loads all materials defined in the specified file.
 */
void LoadMaterials(const char *path) {

	mat_file = NULL;
	Cm_LoadMaterials(path, &mat_file);
	if (mat_file) {
		Com_Print("Loaded %d materials from %s\n", g_list_length(mat_file), path);
	} else {
		Com_Warn("Failed to load %s\n", path);
		Com_Warn("Run `quemap -mat %s` to generate\n", map_base);
	}

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

	g_list_free(mat_file);
	mat_file = NULL;
}

/**
 * @brief Writes all known materials to the specified path.
 */
ssize_t WriteMaterialsFile(const char *path) {

	GList *cm = NULL;
	for (int32_t i = 0; i < num_materials; i++) {
		cm = g_list_prepend(cm, materials[i].cm);
	}

	const ssize_t count = Cm_WriteMaterials(path, cm);

	g_list_free(cm);
	return count;
}
