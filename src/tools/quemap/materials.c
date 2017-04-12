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

#include "quemap.h"
#include "materials.h"

static GList *materials;

/**
 * @brief Loads all materials defined in the material file.
 */
void LoadMaterials(void) {

	char path[MAX_QPATH];
	g_snprintf(path, sizeof(path), "materials/%s.mat", map_base);

	materials = NULL;
	Cm_LoadMaterials(path, &materials);
}

/**
 * @brief Frees all loaded materials.
 */
void FreeMaterials(void) {

	g_list_free_full(materials, (GDestroyNotify) Cm_FreeMaterial);
	materials = NULL;
}

/**
 * @brief Loads the material with the specified name.
 */
cm_material_t *LoadMaterial(const char *name) {

	for (GList *list = materials; list; list = list->next) {
		if (!g_strcmp0(((cm_material_t *) list->data)->name, name)) {
			return list->data;
		}
	}

	cm_material_t *material = Cm_AllocMaterial(name);
	materials = g_list_prepend(materials, material);

	Com_Debug(DEBUG_ALL, "Loaded material %s\n", name);

	return material;
}

/**
 * @brief Writes the materials to the specified file.
 */
void WriteMaterialsFile(const char *filename) {

	Cm_WriteMaterials(filename, materials);
}
