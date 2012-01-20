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

/*
 *
 * PORTAL FILE GENERATION
 *
 * Save out name.prt for qvis to read
 */

#define	PORTALFILE	"PRT1"

static FILE *pf;
int num_visclusters; /* clusters the player can be in */
int num_visportals;

/*
 * WriteFloat
 */
static void WriteFloat(FILE *f, vec_t v) {
	const float r = floor(v + 0.5);

	if (fabs(v - r) < 0.001)
		fprintf(f, "%i ", (int) r);
	else
		fprintf(f, "%f ", v);
}

/*
 * WritePortalFile_r
 */
static void WritePortalFile_r(node_t *node) {
	int i, s;
	portal_t *p;
	winding_t *w;
	vec3_t normal;
	vec_t dist;

	// decision node
	if (node->plane_num != PLANENUM_LEAF && !node->detail_seperator) {
		WritePortalFile_r(node->children[0]);
		WritePortalFile_r(node->children[1]);
		return;
	}

	if (node->contents & CONTENTS_SOLID)
		return;

	for (p = node->portals; p; p = p->next[s]) {
		w = p->winding;
		s = (p->nodes[1] == node);
		if (w && p->nodes[0] == node) {
			if (!Portal_VisFlood(p))
				continue;
			// write out to the file

			// sometimes planes get turned around when they are very near
			// the changeover point between different axis.  interpret the
			// plane the same way vis will, and flip the side orders if needed
			// FIXME: is this still relevent?
			WindingPlane(w, normal, &dist);
			if (DotProduct(p->plane.normal, normal) < 0.99) { // backwards...
				fprintf(pf, "%i %i %i ", w->numpoints, p->nodes[1]->cluster,
						p->nodes[0]->cluster);
			} else
				fprintf(pf, "%i %i %i ", w->numpoints, p->nodes[0]->cluster,
						p->nodes[1]->cluster);
			for (i = 0; i < w->numpoints; i++) {
				fprintf(pf, "(");
				WriteFloat(pf, w->p[i][0]);
				WriteFloat(pf, w->p[i][1]);
				WriteFloat(pf, w->p[i][2]);
				fprintf(pf, ") ");
			}
			fprintf(pf, "\n");
		}
	}

}

/*
 * FillLeafNumbers_r
 *
 * All of the leafs under node will have the same cluster
 */
static void FillLeafNumbers_r(node_t * node, int num) {
	if (node->plane_num == PLANENUM_LEAF) {
		if (node->contents & CONTENTS_SOLID)
			node->cluster = -1;
		else
			node->cluster = num;
		return;
	}
	node->cluster = num;
	FillLeafNumbers_r(node->children[0], num);
	FillLeafNumbers_r(node->children[1], num);
}

/*
 * NumberLeafs_r
 */
static void NumberLeafs_r(node_t * node) {
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
		if (p->nodes[0] == node) // only write out from first leaf
		{
			if (Portal_VisFlood(p))
				num_visportals++;
			p = p->next[0];
		} else
			p = p->next[1];
	}

}

/*
 * CreateVisPortals_r
 */
static void CreateVisPortals_r(node_t * node) {
	// stop as soon as we get to a detail_seperator, which
	// means that everything below is in a single cluster
	if (node->plane_num == PLANENUM_LEAF || node->detail_seperator)
		return;

	MakeNodePortal(node);
	SplitNodePortals(node);

	CreateVisPortals_r(node->children[0]);
	CreateVisPortals_r(node->children[1]);
}

static int clusterleaf;
static void SaveClusters_r(node_t * node) {
	if (node->plane_num == PLANENUM_LEAF) {
		d_bsp.leafs[clusterleaf++].cluster = node->cluster;
		return;
	}
	SaveClusters_r(node->children[0]);
	SaveClusters_r(node->children[1]);
}

/*
 * WritePortalFile
 */
void WritePortalFile(tree_t *tree) {
	char file_name[MAX_OSPATH];
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
	StripExtension(map_name, file_name);
	strcat(file_name, ".prt");

	if (Fs_OpenFile(file_name, &pf, FILE_WRITE) == -1)
		Com_Error(ERR_FATAL, "Error opening %s\n", file_name);

	fprintf(pf, "%s\n", PORTALFILE);
	fprintf(pf, "%i\n", num_visclusters);
	fprintf(pf, "%i\n", num_visportals);

	Com_Verbose("%5i visclusters\n", num_visclusters);
	Com_Verbose("%5i visportals\n", num_visportals);

	WritePortalFile_r(head_node);

	Fs_CloseFile(pf);

	// we need to store the clusters out now because ordering
	// issues made us do this after writebsp...
	clusterleaf = 1;
	SaveClusters_r(head_node);
}
