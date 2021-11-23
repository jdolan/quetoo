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

#include "bsp.h"
#include "map.h"
#include "material.h"
#include "qmat.h"

/**
 * @brief Parses the materials and map files, merging all known materials, and 
 * serializes them back to the materials file for the current map.
 */
int32_t MAT_Main(void) {
	char path[MAX_QPATH];

	g_snprintf(path, sizeof(path), "maps/%s.mat", map_base);

	Com_Print("\n------------------------------------------\n");
	Com_Print("\nGenerating %s for %s\n\n", path, map_name);

	const uint32_t start = SDL_GetTicks();

	// clear the whole bsp structure
	memset(&bsp_file, 0, sizeof(bsp_file));

	Bsp_AllocLump(&bsp_file, BSP_LUMP_MATERIALS, MAX_BSP_MATERIALS);

	LoadMaterials(path, ASSET_CONTEXT_TEXTURES, NULL);

	LoadMapFile(map_name);

	for (int32_t i = 0; i < bsp_file.num_materials; i++) {
		LoadMaterial(bsp_file.materials[i].name, ASSET_CONTEXT_TEXTURES);
	}

	WriteMaterialsFile(path);

	FreeMaterials();

	const uint32_t end = SDL_GetTicks();
	Com_Print("\nGenerated materials in %d ms\n", end - start);

	return 0;
}
