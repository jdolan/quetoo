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

/*
 * @brief Create the initial AAS nodes, which are just partial copies of the
 * BSP nodes. We'll prune these to remove leafs which are not navigable.
 */
static void CreateAASNodes(void) {
	uint16_t i;

	const d_bsp_node_t *in = d_bsp.nodes;
	d_aas_node_t *out = d_aas.nodes;

	for (i = 0; i < d_bsp.num_nodes; i++, in++, out++) {
		memcpy(out, in, sizeof(*in));

		out->first_path = 0;
		out->num_paths = 0;
	}
}

/*
 * @brief Returns true if the specified leaf is navigable (has an upward-facing
 * solid plane), false otherwise.
 */
static _Bool PruneAASNodes_isNavigable(const d_bsp_leaf_t *leaf) {
	uint16_t i;

	if ((leaf->contents & MASK_CLIP_PLAYER) == 0) {
		return false;
	}

	const uint16_t *lb = &d_bsp.leaf_brushes[leaf->first_leaf_brush];

	for (i = 0; i < leaf->num_leaf_brushes; i++, lb++) {
		const d_bsp_brush_t *brush = &d_bsp.brushes[*lb];

		if (brush->contents & MASK_CLIP_PLAYER) {
			const d_bsp_brush_side_t *bs = &d_bsp.brush_sides[brush->first_side];
			int32_t j;

			for (j = 0; j < brush->num_sides; j++, bs++) {
				const d_bsp_plane_t *plane = &d_bsp.planes[bs->plane_num];

				if (plane->normal[2] > AI_NODE_MIN_NORMAL) {
					return true;
				}
			}
		}
	}

	return false;
}

/*
 * @brief Recurse the AAS node tree, pruning leafs which are not navigable.
 */
static void PruneAASNodes_(d_aas_node_t *node) {
	int32_t c;

	c = LittleLong(node->children[0]);
	if (c < 0) {
		if (PruneAASNodes_isNavigable(&d_bsp.leafs[-1 - c])) {

		} else {
			node->children[0] = AAS_INVALID_LEAF;
		}

		return;
	}

	PruneAASNodes_(&d_aas.nodes[c]);

	c = LittleLong(node->children[1]);
	if (c < 0) {
		if (PruneAASNodes_isNavigable(&d_bsp.leafs[-1 - c])) {

		} else {
			node->children[1] = AAS_INVALID_LEAF;
		}

		return;
	}

	PruneAASNodes_(&d_aas.nodes[c]);
}

/*
 * @brief Entry point for recursive AAS node pruning.
 */
static void PruneAASNodes(void) {
	PruneAASNodes_(d_aas.nodes);
}

/*
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

/*
 * @brief Writes the specified lump to the AAS file.
 */
static void WriteLump(file_t *f, d_bsp_lump_t *lump, void *data, size_t len) {

	lump->file_ofs = LittleLong((int32_t) Fs_Tell(f));
	lump->file_len = LittleLong((int32_t) len);

	Fs_Write(f, data, 1, (len + 3) & ~3);
}

/*
 * @brief
 */
void WriteAASFile(void) {
	char path[MAX_QPATH];
	file_t *f;

	StripExtension(bsp_name, path);
	g_strlcat(path, ".aas", sizeof(path));

	if (!(f = Fs_OpenWrite(path))) {
		Com_Error(ERR_FATAL, "Couldn't open %s for writing\n", path);
	}

	Com_Print("Writing %d AAS nodes..\n", d_aas.num_nodes);

	SwapAASFile();

	d_bsp_header_t header;
	memset(&header, 0, sizeof(header));

	header.ident = LittleLong(AAS_IDENT);
	header.version = LittleLong(AAS_VERSION);

	Fs_Write(f, &header, 1, sizeof(header));

	d_bsp_lump_t *lump = &header.lumps[AAS_LUMP_NODES];
	WriteLump(f, lump, d_aas.nodes, sizeof(d_aas_node_t) * d_aas.num_nodes);

	// rewrite the header with the populated lumps

	Fs_Seek(f, 0);
	Fs_Write(f, &header, 1, sizeof(header));

	Fs_Close(f);
}

/*
 * @brief Generates ${bsp_name}.aas for AI navigation.
 */
int32_t AAS_Main(void) {

	Com_Print("\n----- AAS -----\n\n");

	const time_t start = time(NULL);

	LoadBSPFile(bsp_name);

	if (d_bsp.num_nodes == 0) {
		Com_Error(ERR_FATAL, "No nodes");
	}

	memset(&d_aas, 0, sizeof(d_aas));

	CreateAASNodes();

	PruneAASNodes();

	WriteAASFile();

	const time_t end = time(NULL);
	const time_t duration = end - start;
	Com_Print("\nAAS Time: ");
	if (duration > 59)
		Com_Print("%d Minutes ", (int32_t) (duration / 60));
	Com_Print("%d Seconds\n", (int32_t) (duration % 60));

	return 0;
}
