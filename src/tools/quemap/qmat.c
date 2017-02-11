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

#include "qmat.h"

static GPtrArray *materials;

/**
 * @brief Loads all materials defined in the material file.
 */
void LoadMaterials(void) {

	size_t count;
	cm_material_t **mats = Cm_LoadMaterials(mat_name, &count);
	cm_material_t **material = mats;

	materials = g_ptr_array_new_with_free_func((GDestroyNotify) Cm_FreeMaterial);
	assert(materials);

	for (size_t i = 0; i < count; i++, material++) {
		g_ptr_array_add(materials, *material);
	}
}

/**
 * @brief Frees all loaded materials.
 */
void FreeMaterials(void) {
	g_ptr_array_free(materials, true);
	materials = NULL;
}

/**
 * @brief Loads the material with the specified diffuse name.
 */
cm_material_t *LoadMaterial(const char *diffuse) {

	Com_Print("Searching for material %s\n", diffuse);

	for (guint i = 0; i < materials->len; i++) {
		cm_material_t *material = g_ptr_array_index(materials, i);
		if (!g_strcmp0(material->diffuse, diffuse)) {
			Com_Print("Resolved material %s\n", material->diffuse);
			return material;
		}
	}

	cm_material_t *material = Cm_LoadMaterial(diffuse);
	g_ptr_array_add(materials, material);

	Com_Print("Created material %s\n", material->diffuse);

	return material;
}

/**
 * @brief Loads the specified BSP file, resolves all materials referenced by it,
 * and generates a "stub" materials file.
 */
int32_t MAT_Main(void) {

	Com_Print("\n----- MAT -----\n\n");

	const time_t start = time(NULL);

	LoadMaterials();

	LoadBSPFile(bsp_name, (1 << BSP_LUMP_TEXINFO));

	for (int32_t i = 0; i < bsp_file.num_texinfo; i++) {
		LoadMaterial(bsp_file.texinfo[i].texture);
	}

	Cm_WriteMaterials(mat_name, (const cm_material_t **) materials->pdata, materials->len);

	Com_Print("Generated %d materials\n", materials->len);

	FreeMaterials();

	const time_t end = time(NULL);
	const time_t duration = end - start;
	Com_Print("\nMaterials time: ");
	if (duration > 59) {
		Com_Print("%d Minutes ", (int32_t) (duration / 60));
	}
	Com_Print("%d Seconds\n", (int32_t) (duration % 60));

	return 0;
}
