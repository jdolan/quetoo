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

#include "map.h"
#include "bsp.h"

bsp_file_t bsp_file;

/**
 * @brief Dumps info about current file
 */
static void PrintBSPFileSizes(void) {

	Com_Verbose("--- Wrote %s ---\n", bsp_name);

	Com_Verbose("%5i entities      %7i bytes\n", num_entities,
				bsp_file.entity_string_size);

	Com_Verbose("%5i texinfo       %7i bytes\n", bsp_file.num_texinfo,
				(int32_t) (bsp_file.num_texinfo * sizeof(bsp_texinfo_t)));

	Com_Verbose("%5i planes        %7i bytes\n", bsp_file.num_planes,
				(int32_t) (bsp_file.num_planes * sizeof(bsp_plane_t)));

	Com_Verbose("%5i brush_sides   %7i bytes\n", bsp_file.num_brush_sides,
				(int32_t) (bsp_file.num_brush_sides * sizeof(bsp_brush_side_t)));

	Com_Verbose("%5i brushes       %7i bytes\n", bsp_file.num_brushes,
				(int32_t) (bsp_file.num_brushes * sizeof(bsp_brush_t)));

	Com_Verbose("%5i vertexes      %7i bytes\n", bsp_file.num_vertexes,
				(int32_t) (bsp_file.num_vertexes * sizeof(bsp_vertex_t)));

	Com_Verbose("%5i elements      %7i bytes\n", bsp_file.num_elements,
				(int32_t) (bsp_file.num_elements * sizeof(int32_t)));

	Com_Verbose("%5i faces         %7i bytes\n", bsp_file.num_faces,
				(int32_t) (bsp_file.num_faces * sizeof(bsp_face_t)));

	Com_Verbose("%5i nodes         %7i bytes\n", bsp_file.num_nodes,
				(int32_t) (bsp_file.num_nodes * sizeof(bsp_node_t)));

	Com_Verbose("%5i leaf_faces    %7i bytes\n", bsp_file.num_leaf_faces,
				(int32_t) (bsp_file.num_leaf_faces * sizeof(bsp_file.leaf_faces[0])));

	Com_Verbose("%5i leaf_brushes  %7i bytes\n", bsp_file.num_leaf_brushes,
				(int32_t) (bsp_file.num_leaf_brushes * sizeof(bsp_file.leaf_brushes[0])));

	Com_Verbose("%5i leafs         %7i bytes\n", bsp_file.num_leafs,
				(int32_t) (bsp_file.num_leafs * sizeof(bsp_leaf_t)));

	Com_Verbose("%5i models        %7i bytes\n", bsp_file.num_models,
				(int32_t) (bsp_file.num_models * sizeof(bsp_model_t)));

	Com_Verbose("%5i area_portals  %7i bytes\n", bsp_file.num_area_portals,
				(int32_t) (bsp_file.num_area_portals * sizeof(bsp_area_portal_t)));

	Com_Verbose("%5i areas         %7i bytes\n", bsp_file.num_areas,
				(int32_t) (bsp_file.num_areas * sizeof(bsp_area_t)));

	Com_Verbose("      vis         %7i bytes\n", bsp_file.vis_size);

	Com_Verbose("%5i lightmaps     %7i bytes\n", bsp_file.num_lightmaps,
				(int32_t) (bsp_file.num_lightmaps * sizeof(bsp_lightmap_t)));

	Com_Verbose("      lightgrid   %7i bytes\n", bsp_file.lightgrid_size);
}

/**
 * @brief
 */
void LoadBSPFile(const char *filename, const bsp_lump_id_t lumps) {

	memset(&bsp_file, 0, sizeof(bsp_file));

	bsp_header_t *file;

	if (Fs_Load(filename, (void **) &file) == -1) {
		Com_Error(ERROR_FATAL, "Failed to load %s\n", filename);
	}

	if (Bsp_Verify(file) == -1) {
		Fs_Free(file);
		Com_Error(ERROR_FATAL, "Failed to verify %s\n", filename);
	}

	Bsp_LoadLumps(file, &bsp_file, lumps);
	Fs_Free(file);
}

/**
 * @brief
 */
void WriteBSPFile(const char *filename) {

	file_t *file = Fs_OpenWrite(filename);

	Bsp_Write(file, &bsp_file);

	Fs_Close(file);

	if (verbose) {
		PrintBSPFileSizes();
	}
}
