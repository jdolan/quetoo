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

static struct {
	/**
	 * @brief A list of all known cm_material_t.
	 */
	GList *cm;

	/**
	 * @brief A hash table of all known material_t.
	 */
	GHashTable *hash;
} materials;

/**
 * @brief Allocates material_t for the specified collision material.
 */
static material_t *AllocMaterial(cm_material_t *cm) {

	if (bsp_file.num_materials == MAX_BSP_MATERIALS) {
		Com_Error(ERROR_FATAL, "MAX_BSP_MATERIALS\n");
	}

	material_t *material = Mem_Malloc(sizeof(*material));
	material->cm = cm;

	if (Cm_ResolveMaterial(material->cm, ASSET_CONTEXT_TEXTURES)) {
		material->diffusemap = Img_LoadSurface(material->cm->diffusemap.path);
		if (material->diffusemap) {
			Com_Verbose("Loaded %s\n", material->cm->diffusemap.path);
		} else {
			Com_Warn("Failed to load %s\n", material->cm->diffusemap.path);
		}
	} else {
		Com_Warn("Failed to resolve %s\n", material->cm->name);
	}

	return material;
}

/**
 * @brief
 */
static void FreeMaterial(gpointer p) {

	material_t *material = p;

	SDL_FreeSurface(material->diffusemap);

	Mem_Free(material);
}

/**
 * @brief Loads all materials defined in the specified file.
 */
ssize_t LoadMaterials(const char *path) {

	assert(materials.cm == NULL);
	assert(materials.hash == NULL);

	materials.hash = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, FreeMaterial);

	const ssize_t count = Cm_LoadMaterials(path, &materials.cm);
	for (GList *e = materials.cm; e; e = e->next) {

		material_t *material = AllocMaterial(e->data);

		g_hash_table_insert(materials.hash, material->cm->name, material);
	}

	return count;
}

/**
 * @brief Finds the specified material by name, writing it to the BSP file.
 */
static material_t *FindMaterial(const char *name) {

	material_t *material = g_hash_table_lookup(materials.hash, name);
	if (material == NULL) {

		cm_material_t *cm = Cm_AllocMaterial(name);
		materials.cm = g_list_prepend(materials.cm, cm);

		material = AllocMaterial(cm);
		g_hash_table_insert(materials.hash, material->cm->name, material);
	}

	Com_Debug(DEBUG_ALL, "Loaded material %s\n", name);
	return material;
}

/**
 * @brief Emits a material with the specified name to the BSP.
 */
int32_t EmitMaterial(const char *name) {

	material_t *material = FindMaterial(name);
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

	return FindMaterial(bsp_file.materials[num].name);
}

/**
 * @brief Frees all loaded materials.
 */
void FreeMaterials(void) {

	Cm_FreeMaterials(materials.cm);

	g_hash_table_destroy(materials.hash);

	memset(&materials, 0, sizeof(materials));
}

/**
 * @brief Writes the materials to the specified path.
 */
ssize_t WriteMaterialsFile(const char *path) {
	return Cm_WriteMaterials(path, materials.cm);
}
