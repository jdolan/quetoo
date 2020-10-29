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

#include "cm_local.h"

/**
 * @brief If true, PVS culling is skipped.
 */
_Bool cm_no_vis = false;

/**
 * @remarks `pvs` must be at least `MAX_BSP_LEAFS >> 3` in length.
 */
static int32_t Cm_DecompressVis(int32_t cluster, byte *out, int32_t set) {

	if (Cm_NumClusters() == 0 || cm_no_vis) {
		memset(out, 0xff, MAX_BSP_LEAFS >> 3);
		return MAX_BSP_LEAFS >> 3;
	}

	memset(out, 0, MAX_BSP_LEAFS >> 3);
	
	if (cluster == -1) {
		return MAX_BSP_LEAFS >> 3;
	}

	const bsp_vis_t *vis = cm_bsp.file.vis;
	const byte *in = ((byte *) vis) + vis->bit_offsets[cluster][set];

	return Bsp_DecompressVis(&cm_bsp.file, in, out);
}

/**
 * @brief
 */
int32_t Cm_ClusterPVS(const int32_t cluster, byte *pvs) {
	return Cm_DecompressVis(cluster, pvs, VIS_PVS);
}

/**
 * @brief
 */
int32_t Cm_ClusterPHS(const int32_t cluster, byte *phs) {
	return Cm_DecompressVis(cluster, phs, VIS_PHS);
}

/**
 * @return The unique clusters of all leafs that intersect with the given bounding box.
 */
static size_t Cm_BoxClusters(const vec3_t mins, const vec3_t maxs, int32_t *clusters, size_t len) {

	int32_t leafs[cm_bsp.file.num_leafs];
	size_t count = Cm_BoxLeafnums(mins, maxs, leafs, cm_bsp.file.num_leafs, NULL, 0);

	size_t num_clusters = 0;
	for (size_t i = 0; i < count; i++) {

		const int32_t cluster = Cm_LeafCluster(leafs[i]);
		if (cluster == -1) {
			continue;
		}

		size_t c;
		for (c = 0; c < num_clusters; c++) {
			if (clusters[c] == cluster) {
				break;
			}
		}

		if (c < num_clusters) {
			continue;
		}

		clusters[num_clusters++] = cluster;
	}

	return num_clusters;
}

/**
 * @breif
 */
static int32_t Cm_BoxVis(const vec3_t mins, const vec3_t maxs, byte *out, int32_t set) {

	memset(out, 0, MAX_BSP_LEAFS >> 3);

	int32_t clusters[cm_bsp.file.num_leafs];
	int32_t box_len = 0;

	const size_t count = Cm_BoxClusters(mins, maxs, clusters, cm_bsp.file.num_leafs);
	for (size_t i = 0; i < count; i++) {

		byte custer_out[MAX_BSP_LEAFS >> 3];
		const int32_t cluster_len = Cm_DecompressVis(clusters[i], custer_out, set);

		for (int32_t j = 0; j < cluster_len; j++) {
			out[j] |= custer_out[j];
		}

		box_len = MAX(box_len, cluster_len);
	}

	return box_len;
}

/**
 * @brief
 */
int32_t Cm_BoxPVS(const vec3_t mins, const vec3_t maxs, byte *pvs) {
	return Cm_BoxVis(mins, maxs, pvs, VIS_PVS);
}

/**
 * @brief
 */
int32_t Cm_BoxPHS(const vec3_t mins, const vec3_t maxs, byte *phs) {
	return Cm_BoxVis(mins, maxs, phs, VIS_PHS);
}


/**
 * @return True if the specified cluster is marked visible in the given visibilty vector.
 */
_Bool Cm_ClusterVisible(const int32_t cluster, const byte *vis) {

	if (cluster == -1) {
		return false;
	}

	return vis[cluster >> 3] & (1 << (cluster & 7));
}

/**
 * @brief Returns true if any leaf under head_node has a cluster that
 * is potentially visible.
 */
_Bool Cm_HeadnodeVisible(const int32_t node_num, const byte *vis) {
	const cm_bsp_node_t *node;

	if (node_num < 0) { // at a leaf, check it
		const int32_t leaf_num = -1 - node_num;
		const int32_t cluster = cm_bsp.leafs[leaf_num].cluster;

		return Cm_ClusterVisible(cluster, vis);

		return false;
	}

	node = &cm_bsp.nodes[node_num];

	if (Cm_HeadnodeVisible(node->children[0], vis)) {
		return true;
	}

	return Cm_HeadnodeVisible(node->children[1], vis);
}
