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
#include "portal.h"
#include "prtfile.h"

static file_t *prtfile;
static int32_t num_vis_clusters; /* clusters the player can be in */
static int32_t num_vis_portals;

/**
 * @brief
 */
static void WriteFloat(file_t *f, float v) {
	const float r = floorf(v + 0.5);

	if (fabsf(v - r) < SIDE_EPSILON) {
		Fs_Print(f, "%i ", (int32_t) r);
	} else {
		Fs_Print(f, "%f ", v);
	}
}

/**
 * @brief
 */
static void WritePortalFile_r(node_t *node) {
	int32_t s;
	vec3_t normal;
	double dist;

	// decision node
	if (node->plane != PLANE_LEAF && !(node->split_side->contents & CONTENTS_DETAIL)) {
		WritePortalFile_r(node->children[0]);
		WritePortalFile_r(node->children[1]);
		return;
	}

	if (node->contents & CONTENTS_SOLID) {
		return;
	}

	for (portal_t *p = node->portals; p; p = p->next[s]) {
		cm_winding_t *w = p->winding;
		s = (p->nodes[1] == node);
		if (w && p->nodes[0] == node) {
			if (!Portal_VisFlood(p)) {
				continue;
			}
			// write out to the file

			// sometimes planes get turned around when they are very near
			// the changeover point between different axis. interpret the
			// plane the same way vis will, and flip the side orders if needed
			// FIXME: is this still relevent? Yes. jgothic.
			Cm_PlaneForWinding(w, &normal, &dist);
			if (Vec3_Dot(p->plane.normal, normal) < 0.99f) { // backwards...
				Fs_Print(prtfile, "%i %i %i ", w->num_points, p->nodes[1]->cluster,
				         p->nodes[0]->cluster);
			} else {
				Fs_Print(prtfile, "%i %i %i ", w->num_points, p->nodes[0]->cluster,
				         p->nodes[1]->cluster);
			}
			for (int32_t i = 0; i < w->num_points; i++) {
				Fs_Print(prtfile, "(");
				WriteFloat(prtfile, w->points[i].x);
				WriteFloat(prtfile, w->points[i].y);
				WriteFloat(prtfile, w->points[i].z);
				Fs_Print(prtfile, ") ");
			}
			Fs_Print(prtfile, "\n");
		}
	}
}

/**
 * @brief All of the leafs under node will have the same cluster
 */
static void FillLeafNumbers_r(node_t *node, int32_t num) {
	if (node->plane == PLANE_LEAF) {
		if (node->contents & CONTENTS_SOLID) {
			node->cluster = -1;
		} else {
			node->cluster = num;
		}
		return;
	}
	node->cluster = num;
	FillLeafNumbers_r(node->children[0], num);
	FillLeafNumbers_r(node->children[1], num);
}

/**
 * @brief
 */
static void NumberLeafs_r(node_t *node) {

	if (node->plane != PLANE_LEAF && !(node->split_side->contents & CONTENTS_DETAIL)) { // decision node
		node->cluster = -99;
		NumberLeafs_r(node->children[0]);
		NumberLeafs_r(node->children[1]);
		return;
	}

	// either a leaf or a detail cluster
	if (node->contents & CONTENTS_SOLID) { // solid block, viewpoint never inside
		node->cluster = -1;
		return;
	}

	FillLeafNumbers_r(node, num_vis_clusters);
	num_vis_clusters++;

	// count the portals
	for (const portal_t *p = node->portals; p;) {
		if (p->nodes[0] == node) { // only write out from first leaf
			if (Portal_VisFlood(p)) {
				num_vis_portals++;
			}
			p = p->next[0];
		} else {
			p = p->next[1];
		}
	}
}

/**
 * @brief
 */
static void CreateVisPortals_r(node_t *node) {
	// stop as soon as we get to a leaf or detail node, which
	// means that everything below is in a single cluster
	if (node->plane == PLANE_LEAF || (node->split_side->contents & CONTENTS_DETAIL)) {
		return;
	}

	MakeNodePortal(node);
	SplitNodePortals(node);

	CreateVisPortals_r(node->children[0]);
	CreateVisPortals_r(node->children[1]);
}

/**
 * @brief
 */
static void SaveClusters_r(node_t *node) {
	static int32_t clusterleaf = 1;

	if (node->plane == PLANE_LEAF) {
		bsp_file.leafs[clusterleaf++].cluster = node->cluster;
		return;
	}

	SaveClusters_r(node->children[0]);
	SaveClusters_r(node->children[1]);
}

/**
 * @brief
 */
void WritePortalFile(tree_t *tree) {
	char filename[MAX_OS_PATH];

	Com_Verbose("--- WritePortalFile ---\n");

	num_vis_clusters = 0;
	num_vis_portals = 0;

	FreeTreePortals_r(tree->head_node);

	MakeHeadnodePortals(tree);

	CreateVisPortals_r(tree->head_node);

	// set the cluster field in every leaf and count the total number of portals
	NumberLeafs_r(tree->head_node);

	// write the file
	g_snprintf(filename, sizeof(filename), "maps/%s.prt", map_base);

	if (!(prtfile = Fs_OpenWrite(filename))) {
		Com_Error(ERROR_FATAL, "Error opening %s\n", filename);
	}

	Fs_Print(prtfile, "%s\n", PORTALFILE);
	Fs_Print(prtfile, "%i\n", num_vis_clusters);
	Fs_Print(prtfile, "%i\n", num_vis_portals);

	Com_Verbose("%5i vis clusters\n", num_vis_clusters);
	Com_Verbose("%5i vis portals\n", num_vis_portals);

	WritePortalFile_r(tree->head_node);

	Fs_Close(prtfile);

	// we need to store the clusters out now because ordering
	// issues made us do this after writebsp...
	SaveClusters_r(tree->head_node);

	Com_Verbose("--- WritePortalFile complete ---\n");
}
