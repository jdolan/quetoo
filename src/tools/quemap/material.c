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

static GList *materials;
static GHashTable *assets;

/**
 * @brief Loads all materials defined in the material file.
 */
ssize_t LoadMaterials(const char *path, cm_asset_context_t context, GList **result) {

	const ssize_t count = Cm_LoadMaterials(path, &materials);

	GList *e = materials;
	for (ssize_t i = 0; i < count; i++, e = e->next) {
		Cm_ResolveMaterial((cm_material_t *) e->data, context);
	}

	if (result) {
		GList *e = materials;
		for (ssize_t i = 0; i < count; i++, e = e->next) {
			*result = g_list_append(*result, e->data);
		}
	}

	return count;
}

/**
 * @brief Loads the material with the specified name.
 */
cm_material_t *LoadMaterial(const char *name, cm_asset_context_t context) {

	for (GList *list = materials; list; list = list->next) {
		if (!g_strcmp0(((cm_material_t *) list->data)->name, name)) {
			return list->data;
		}
	}

	cm_material_t *material = Cm_AllocMaterial(name);
	Cm_ResolveMaterial(material, context);

	materials = g_list_prepend(materials, material);

	Com_Debug(DEBUG_ALL, "Loaded material %s\n", name);

	return material;
}

/**
 * @brief
 */
SDL_Surface *LoadAsset(const cm_asset_t *asset) {

	if (assets == NULL) {
		assets = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, (GDestroyNotify) SDL_FreeSurface);
	}

	SDL_Surface *surf = g_hash_table_lookup(assets, (void *) asset->path);
	if (surf == NULL) {
		surf = Img_LoadSurface(asset->path);
		if (surf) {
			g_hash_table_insert(assets, (void *) asset->path, surf);
		}
	}

	return surf;
}

/**
 * @brief
 */
SDL_Surface *LoadDiffuseTexture(const char *name) {
	return LoadAsset(&LoadMaterial(name, ASSET_CONTEXT_TEXTURES)->diffusemap);
}

/**
 * @brief Frees all loaded materials.
 */
void FreeMaterials(void) {

	if (materials != NULL) {
		g_list_free_full(materials, (GDestroyNotify) Cm_FreeMaterial);
		materials = NULL;
	}

	if (assets != NULL) {
		g_hash_table_destroy(assets);
		assets = NULL;
	}
}

/**
 * @brief Writes the materials to the specified file.
 */
ssize_t WriteMaterialsFile(const char *filename) {
	
	return Cm_WriteMaterials(filename, materials);
}
