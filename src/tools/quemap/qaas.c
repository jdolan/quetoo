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

#include "bspfile.h"
#include "polylib.h"

#define AI_NODE_MIN_NORMAL 0.7

typedef struct {
	d_aas_node_t nodes[MAX_BSP_NODES];
	int32_t num_nodes;
} d_aas_t;

static d_aas_t d_aas;

/**
 * @brief Create the initial AAS nodes, which are just partial copies of the
 * BSP nodes. We'll prune these to remove leafs which are not navigable.
 */
static void CreateAASNodes(void) {
	uint16_t i;

	const bsp_node_t *in = bsp_file.nodes;
	d_aas_node_t *out = d_aas.nodes;

	for (i = 0; i < bsp_file.num_nodes; i++, in++, out++) {
		memcpy(out, in, sizeof(*in));

		out->first_path = 0;
		out->num_paths = 0;
	}
}

/**
 * @brief Returns true if the specified leaf is navigable (has an upward-facing
 * solid plane), false otherwise.
 */
static _Bool PruneAASNodes_isNavigable(const bsp_leaf_t *leaf) {
	uint16_t i;

	if ((leaf->contents & MASK_CLIP_PLAYER) == 0) {
		return false;
	}

	const uint16_t *lb = &bsp_file.leaf_brushes[leaf->first_leaf_brush];

	for (i = 0; i < leaf->num_leaf_brushes; i++, lb++) {
		const bsp_brush_t *brush = &bsp_file.brushes[*lb];

		if (brush->contents & MASK_CLIP_PLAYER) {
			const bsp_brush_side_t *bs = &bsp_file.brush_sides[brush->first_brush_side];
			int32_t j;

			for (j = 0; j < brush->num_sides; j++, bs++) {
				const bsp_plane_t *plane = &bsp_file.planes[bs->plane_num];

				if (plane->normal[2] > AI_NODE_MIN_NORMAL) {
					return true;
				}
			}
		}
	}

	return false;
}

/**
 * @brief Recurse the AAS node tree, pruning leafs which are not navigable.
 */
static void PruneAASNodes_(d_aas_node_t *node) {
	int32_t c;

	c = LittleLong(node->children[0]);
	if (c < 0) {
		if (PruneAASNodes_isNavigable(&bsp_file.leafs[-1 - c])) {

		} else {
			node->children[0] = AAS_INVALID_LEAF;
		}

		return;
	}

	PruneAASNodes_(&d_aas.nodes[c]);

	c = LittleLong(node->children[1]);
	if (c < 0) {
		if (PruneAASNodes_isNavigable(&bsp_file.leafs[-1 - c])) {

		} else {
			node->children[1] = AAS_INVALID_LEAF;
		}

		return;
	}

	PruneAASNodes_(&d_aas.nodes[c]);
}

/**
 * @brief Entry point for recursive AAS node pruning.
 */
static void PruneAASNodes(void) {
	PruneAASNodes_(d_aas.nodes);
}

/**
 * @brief Byte-swap all fields of the AAS file to LE.
 */
static void SwapAASFile(void) {
	int32_t i, j;

	d_aas_node_t *node = d_aas.nodes;
	for (i = 0; i < d_aas.num_nodes; i++, node++) {

		for (j = 0; j < 3; j++) {
			node->mins[j] = LittleShort(node->mins[j]);
			node->maxs[j] = LittleShort(node->maxs[j]);
		}
	}
}

/**
 * @brief Writes the specified lump to the AAS file.
 */
static void WriteLump(file_t *f, d_bsp_lump_t *lump, void *data, size_t len) {

	lump->file_ofs = LittleLong((int32_t) Fs_Tell(f));
	lump->file_len = LittleLong((int32_t) len);

	Fs_Write(f, data, 1, (len + 3) & ~3);
}

/**
 * @brief
 */
static void WriteAASFile(void) {

	file_t *file = Fs_OpenWrite(va("maps/%s.aas", map_base));
	if (file == NULL) {
		Com_Error(ERROR_FATAL, "Couldn't open maps/%s.aas for writing\n", map_base);
	}

	Com_Print("Writing %d AAS nodes..\n", d_aas.num_nodes);

	SwapAASFile();

	bsp_header_t header;
	memset(&header, 0, sizeof(header));

	header.ident = LittleLong(AAS_IDENT);
	header.version = LittleLong(AAS_VERSION);

	Fs_Write(file, &header, 1, sizeof(header));

	d_bsp_lump_t *lump = &header.lumps[AAS_LUMP_NODES];
	WriteLump(file, lump, d_aas.nodes, sizeof(d_aas_node_t) * d_aas.num_nodes);

	// rewrite the header with the populated lumps

	Fs_Seek(file, 0);
	Fs_Write(file, &header, 1, sizeof(header));

	Fs_Close(file);
}

/**
 * @brief Generates .aas file for AI navigation.
 */
int32_t AAS_Main(void) {

	Com_Print("\n----- AAS -----\n\n");

	const time_t start = time(NULL);

	LoadBSPFile(bsp_name, BSP_LUMPS_ALL);

	if (bsp_file.num_nodes == 0) {
		Com_Error(ERROR_FATAL, "No nodes");
	}

	memset(&d_aas, 0, sizeof(d_aas));

	CreateAASNodes();

	PruneAASNodes();

	WriteAASFile();

	const time_t end = time(NULL);
	const time_t duration = end - start;
	Com_Print("\nAAS Time: ");
	if (duration > 59) {
		Com_Print("%d Minutes ", (int32_t) (duration / 60));
	}
	Com_Print("%d Seconds\n", (int32_t) (duration % 60));

	return 0;
}
