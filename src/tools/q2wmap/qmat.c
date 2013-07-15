/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#include "qbsp.h"

static GList *materials;

/*
 * @brief
 */
static void AddMaterial(const char *name) {

	if (!name || !g_strcmp0(name, "NULL"))
		return;

	if (!g_list_find_custom(materials, name, (GCompareFunc) g_ascii_strcasecmp)) {
		materials = g_list_insert_sorted(materials, (gpointer) name, (GCompareFunc) g_ascii_strcasecmp);
	}
}

/*
 * @brief Loads the specified BSP file, resolves all materials referenced by it,
 * and generates a "stub" materials file.
 */
int32_t MAT_Main(void) {
	char path[MAX_QPATH];
	file_t *f;
	int32_t i;

	Com_Print("\n----- MAT -----\n\n");

	const time_t start = time(NULL);

	g_snprintf(path, sizeof(path), "materials/%s", Basename(bsp_name));
	g_strlcpy(path + strlen(path) - 3, "mat", sizeof(path));

	if (Fs_Exists(path)) {
		Com_Print("Materials file %s exists, skipping...\n", path);
	} else { // do it

		LoadBSPFileTexinfo(bsp_name);

		if (!(f = Fs_OpenWrite(path)))
			Com_Error(ERR_FATAL, "Couldn't open %s for writing\n", path);

		for (i = 0; i < d_bsp.num_texinfo; i++) // resolve the materials
			AddMaterial(d_bsp.texinfo[i].texture);

		GList *material = materials;
		while (material) { // write the .mat definition

			Fs_Print(f, "{\n"
				"\tmaterial %s\n"
				"\tbump 1.0\n"
				"\thardness 1.0\n"
				"\tparallax 1.0\n"
				"\tspecular 1.0\n"
				"}\n", (char *) material->data);

			material = material->next;
		}

		Fs_Close(f);

		Com_Print("Generated %d materials\n", g_list_length(materials));

		g_list_free(materials);
	}

	const time_t end = time(NULL);
	const time_t duration = end - start;
	Com_Print("\nMaterials time: ");
	if (duration > 59)
		Com_Print("%d Minutes ", (int32_t) (duration / 60));
	Com_Print("%d Seconds\n", (int32_t) (duration % 60));

	return 0;
}
