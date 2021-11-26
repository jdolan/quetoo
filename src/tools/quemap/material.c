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
 * @brief The "missing" diffusemap texture.
 */
static SDL_Surface *notex;

/**
 * @brief A hash table of all known material_t.
 */
static GHashTable *materials;

/**
 * @brief Allocates material_t for the specified collision material.
 */
static material_t *AllocMaterial(cm_material_t *cm) {

	if (bsp_file.num_materials == MAX_BSP_MATERIALS) {
		Com_Error(ERROR_FATAL, "MAX_BSP_MATERIALS\n");
	}

	material_t *material = Mem_Malloc(sizeof(*material));
	material->cm = cm;

	return material;
}

/**
 * @brief
 */
static void FreeMaterial(gpointer p) {

	material_t *material = p;

	Cm_FreeMaterial(material->cm);
	SDL_FreeSurface(material->diffusemap);

	Mem_Free(material);
}

/**
 * @brief
 */
static material_t *LoadMaterial(cm_material_t *cm) {

	material_t *material = g_hash_table_lookup(materials, cm->name);
	if (material == NULL) {

		material = AllocMaterial(cm);

		if (Cm_ResolveMaterial(cm, ASSET_CONTEXT_TEXTURES)) {
			material->diffusemap = Img_LoadSurface(cm->diffusemap.path);
			if (material->diffusemap) {
				Com_Verbose("Loaded %s\n", cm->diffusemap.path);
			} else {
				Com_Warn("Failed to load %s\n", cm->diffusemap.path);
			}
		} else {
			Com_Warn("Failed to resolve %s\n", cm->name);
		}

		material->diffusemap = material->diffusemap ?: notex;

		g_hash_table_insert(materials, cm->name, material);
	}

	return material;
}

/**
 * @brief Loads all materials defined in the specified file.
 */
void LoadMaterials(const char *path) {

	notex = Img_LoadSurface("textures/common/notex");
	assert(notex);

	materials = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, FreeMaterial);

	GList *list = NULL;
	Cm_LoadMaterials(path, &list);
	for (GList *e = list; e; e = e->next) {
		LoadMaterial(e->data);
	}

	if (list) {
		Com_Print("Loaded %d materials from %s\n", g_list_length(list), path);
	} else {
		Com_Warn("Failed to load %s\n", path);
		Com_Warn("Run `quemap -mat %s` to generate", map_base);
	}

	bsp_material_t *bsp = bsp_file.materials;
	for (int32_t i = 0; i < bsp_file.num_materials; i++, bsp++) {

		material_t *material = g_hash_table_lookup(materials, bsp->name);
		if (material == NULL) {
			if (list && !GlobMatch("common/*", bsp->name, GLOB_CASE_INSENSITIVE)) {
				Com_Warn("No material definition for %s\n", bsp->name);
			}
			material = LoadMaterial(Cm_AllocMaterial(bsp->name));
		}

		material->out = bsp;
	}

	g_list_free(list);
}

/**
 * @brief Emits a material with the specified name to the BSP.
 */
int32_t FindMaterial(const char *name) {

	material_t *material = g_hash_table_lookup(materials, name);
	if (material == NULL) {
		material = LoadMaterial(Cm_AllocMaterial(name));
	}

	if (material->out == NULL) {

		if (bsp_file.num_materials == MAX_BSP_MATERIALS) {
			Com_Error(ERROR_FATAL, "MAX_BSP_MATERIALS");
		}

		material->out = bsp_file.materials + bsp_file.num_materials;
		g_strlcpy(material->out->name, material->cm->name, sizeof(material->out->name));

		bsp_file.num_materials++;
	}

	return (int32_t) (ptrdiff_t) (material->out - bsp_file.materials);
}

/**
 * @brief
 */
material_t *GetMaterial(int32_t num) {

	assert(num < bsp_file.num_materials);

	return g_hash_table_lookup(materials, bsp_file.materials[num].name);
}

/**
 * @brief Frees all loaded materials.
 */
void FreeMaterials(void) {

	g_hash_table_destroy(materials);
	materials = NULL;

	SDL_FreeSurface(notex);
	notex = NULL;
}

/**
 * @brief Writes all materials to the specified path.
 */
ssize_t WriteMaterialsFile(const char *path) {

	GList *cm, *values = g_hash_table_get_values(materials);

	for (GList *e = values; e; e = e->next) {
		material_t *material = e->data;
		cm = g_list_prepend(cm, material->cm);
	}

	const ssize_t count = Cm_WriteMaterials(path, cm);

	g_list_free(cm);
	g_list_free(values);

	return count;
}
