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

/*
 *
 * PORTAL FILE GENERATION
 *
 * Save out name.prt for qvis to read
 */

#define	PORTALFILE	"PRT1"

static file_t *prtfile;
static int32_t num_visclusters; /* clusters the player can be in */
static int32_t num_visportals;

/**
 * @brief
 */
static void WriteFloat(file_t *f, vec_t v) {
	const vec_t r = floor(v + 0.5);

	if (fabs(v - r) < 0.001) {
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
	vec_t dist;

	// decision node
	if (node->plane_num != PLANENUM_LEAF && !node->detail_seperator) {
		WritePortalFile_r(node->children[0]);
		WritePortalFile_r(node->children[1]);
		return;
	}

	if (node->contents & CONTENTS_SOLID) {
		return;
	}

	for (portal_t *p = node->portals; p; p = p->next[s]) {
		winding_t *w = p->winding;
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
			WindingPlane(w, normal, &dist);
			if (DotProduct(p->plane.normal, normal) < 0.99) { // backwards...
				Fs_Print(prtfile, "%i %i %i ", w->num_points, p->nodes[1]->cluster,
				         p->nodes[0]->cluster);
			} else {
				Fs_Print(prtfile, "%i %i %i ", w->num_points, p->nodes[0]->cluster,
				         p->nodes[1]->cluster);
			}
			for (int32_t i = 0; i < w->num_points; i++) {
				Fs_Print(prtfile, "(");
				WriteFloat(prtfile, w->points[i][0]);
				WriteFloat(prtfile, w->points[i][1]);
				WriteFloat(prtfile, w->points[i][2]);
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
	if (node->plane_num == PLANENUM_LEAF) {
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
	portal_t *p;

	if (node->plane_num != PLANENUM_LEAF && !node->detail_seperator) { // decision node
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

	FillLeafNumbers_r(node, num_visclusters);
	num_visclusters++;

	// count the portals
	for (p = node->portals; p;) {
		if (p->nodes[0] == node) { // only write out from first leaf
			if (Portal_VisFlood(p)) {
				num_visportals++;
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
	// stop as soon as we get to a detail_seperator, which
	// means that everything below is in a single cluster
	if (node->plane_num == PLANENUM_LEAF || node->detail_seperator) {
		return;
	}

	MakeNodePortal(node);
	SplitNodePortals(node);

	CreateVisPortals_r(node->children[0]);
	CreateVisPortals_r(node->children[1]);
}

static int32_t clusterleaf;
static void SaveClusters_r(node_t *node) {
	if (node->plane_num == PLANENUM_LEAF) {
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
	node_t *head_node;

	Com_Verbose("--- WritePortalFile ---\n");

	head_node = tree->head_node;
	num_visclusters = 0;
	num_visportals = 0;

	FreeTreePortals_r(head_node);

	MakeHeadnodePortals(tree);

	CreateVisPortals_r(head_node);

	// set the cluster field in every leaf and count the total number of portals
	NumberLeafs_r(head_node);

	// write the file
	g_snprintf(filename, sizeof(filename), "maps/%s.prt", map_base);

	if (!(prtfile = Fs_OpenWrite(filename))) {
		Com_Error(ERROR_FATAL, "Error opening %s\n", filename);
	}

	Fs_Print(prtfile, "%s\n", PORTALFILE);
	Fs_Print(prtfile, "%i\n", num_visclusters);
	Fs_Print(prtfile, "%i\n", num_visportals);

	Com_Verbose("%5i visclusters\n", num_visclusters);
	Com_Verbose("%5i visportals\n", num_visportals);

	WritePortalFile_r(head_node);

	Fs_Close(prtfile);

	// we need to store the clusters out now because ordering
	// issues made us do this after writebsp...
	clusterleaf = 1;
	SaveClusters_r(head_node);

	Com_Verbose("--- WritePortalFile complete ---\n");
}
