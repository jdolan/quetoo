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
 * @brief If true, BSP area culling is skipped.
 */
_Bool cm_no_areas = false;

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

	const bsp_vis_t *vis = cm_bsp.bsp.vis;
	const byte *in = ((byte *) vis) + vis->bit_offsets[cluster][set];

	return Bsp_DecompressVis(&cm_bsp.bsp, in, out);
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
 * @brief Recurse over the area portals, marking adjacent ones as flooded.
 */
static void Cm_FloodArea(cm_bsp_area_t *area, int32_t flood_num) {

	if (area->flood_valid == cm_bsp.flood_valid) {
		if (area->flood_num == flood_num) {
			return;
		}

		Com_Error(ERROR_DROP, "Re-flooded\n");
	}

	area->flood_num = flood_num;
	area->flood_valid = cm_bsp.flood_valid;

	const bsp_area_portal_t *p = &cm_bsp.bsp.area_portals[area->first_area_portal];

	for (int32_t i = 0; i < area->num_area_portals; i++, p++) {
		if (cm_bsp.area_portals[p->portal_num]) {
			Cm_FloodArea(&cm_bsp.areas[p->other_area], flood_num);
		}
	}
}

/**
 * @brief
 */
void Cm_FloodAreas(void) {
	int32_t flood_num;

	// all current floods are now invalid
	cm_bsp.flood_valid++;

	// area 0 is not used
	for (int32_t i = flood_num = 1; i < cm_bsp.bsp.num_areas; i++) {
		cm_bsp_area_t *area = &cm_bsp.areas[i];

		if (area->flood_valid == cm_bsp.flood_valid) {
			continue;  // already flooded into
		}

		Cm_FloodArea(area, flood_num++);
	}
}

/**
 * @brief Sets the state of the specified area portal and re-floods all area
 * connections, updating their flood counts such that Cm_WriteAreaBits
 * will return the correct information.
 */
void Cm_SetAreaPortalState(const int32_t portal_num, const _Bool open) {

	if (portal_num > cm_bsp.bsp.num_area_portals) {
		Com_Error(ERROR_DROP, "Portal %d > num_area_portals", portal_num);
	}

	cm_bsp.area_portals[portal_num] = open;
	Cm_FloodAreas();
}

/**
 * @brief Returns true if the specified areas are connected.
 */
_Bool Cm_AreasConnected(const int32_t area1, const int32_t area2) {

	if (cm_no_areas) {
		return true;
	}

	if (area1 > cm_bsp.bsp.num_areas || area2 > cm_bsp.bsp.num_areas) {
		Com_Error(ERROR_DROP, "Area %d > cm.num_areas\n", area1 > area2 ? area1 : area2);
	}

	if (cm_bsp.areas[area1].flood_num == cm_bsp.areas[area2].flood_num) {
		return true;
	}

	return false;
}

/**
 * @brief Writes a bit vector of all the areas that are in the same flood as the
 * specified area. Returns the length of the bit vector in bytes.
 *
 * This is used by the client view to cull visibility.
 */
int32_t Cm_WriteAreaBits(const int32_t area, byte *out) {

	const int32_t bytes = (cm_bsp.bsp.num_areas + 7) >> 3;

	if (cm_no_areas) { // for debugging, send everything
		memset(out, 0xff, bytes);
	} else {
		const int32_t flood_num = cm_bsp.areas[area].flood_num;
		memset(out, 0, bytes);

		for (int32_t i = 0; i < cm_bsp.bsp.num_areas; i++) {
			if (cm_bsp.areas[i].flood_num == flood_num || !area) {
				out[i >> 3] |= 1 << (i & 7);
			}
		}
	}

	return bytes;
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

		if (cluster == -1) {
			return false;
		}

		if (vis[cluster >> 3] & (1 << (cluster & 7))) {
			return true;
		}

		return false;
	}

	node = &cm_bsp.nodes[node_num];

	if (Cm_HeadnodeVisible(node->children[0], vis)) {
		return true;
	}

	return Cm_HeadnodeVisible(node->children[1], vis);
}
