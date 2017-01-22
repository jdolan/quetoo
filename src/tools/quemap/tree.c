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

#include "qbsp.h"

/**
 * @brief
 */
void FreeTreePortals_r(node_t *node) {
	portal_t *p, *nextp;
	int32_t s;

	// free children
	if (node->plane_num != PLANENUM_LEAF) {
		FreeTreePortals_r(node->children[0]);
		FreeTreePortals_r(node->children[1]);
	}
	// free portals
	for (p = node->portals; p; p = nextp) {
		s = (p->nodes[1] == node);
		nextp = p->next[s];

		RemovePortalFromNode(p, p->nodes[!s]);
		FreePortal(p);
	}
	node->portals = NULL;
}

/**
 * @brief
 */
void FreeTree_r(node_t *node) {
	face_t *f, *nextf;

	// free children
	if (node->plane_num != PLANENUM_LEAF) {
		FreeTree_r(node->children[0]);
		FreeTree_r(node->children[1]);
	}
	// free bspbrushes
	FreeBrushList(node->brushes);

	// free faces
	for (f = node->faces; f; f = nextf) {
		nextf = f->next;
		FreeFace(f);
	}

	// free the node
	if (node->volume) {
		FreeBrush(node->volume);
	}

	FreeNode(node);
}

/**
 * @brief
 */
void FreeTree(tree_t *tree) {

	Com_Verbose("--- FreeTree ---\n");
	FreeTreePortals_r(tree->head_node);
	FreeTree_r(tree->head_node);
	Mem_Free(tree);
	Com_Verbose("--- FreeTree complete ---\n");
}

/*
 *
 * NODES THAT DON'T SEPERATE DIFFERENT CONTENTS CAN BE PRUNED
 *
 */

static int32_t c_pruned;

/**
 * @brief
 */
void PruneNodes_r(node_t *node) {
	brush_t *b, *next;

	if (node->plane_num == PLANENUM_LEAF) {
		return;
	}
	PruneNodes_r(node->children[0]);
	PruneNodes_r(node->children[1]);

	if ((node->children[0]->contents & CONTENTS_SOLID) && (node->children[1]->contents
	        & CONTENTS_SOLID)) {
		if (node->faces) {
			Com_Error(ERROR_FATAL, "Node faces separating CONTENTS_SOLID\n");
		}
		if (node->children[0]->faces || node->children[1]->faces) {
			Com_Error(ERROR_FATAL, "Node has no faces but children do\n");
		}

		// FIXME: free stuff
		node->plane_num = PLANENUM_LEAF;
		node->contents = CONTENTS_SOLID;
		node->detail_seperator = false;

		if (node->brushes) {
			Com_Error(ERROR_FATAL, "Node still references brushes\n");
		}

		// combine brush lists
		node->brushes = node->children[1]->brushes;

		for (b = node->children[0]->brushes; b; b = next) {
			next = b->next;
			b->next = node->brushes;
			node->brushes = b;
		}

		c_pruned++;
	}
}

void PruneNodes(node_t *node) {
	Com_Verbose("--- PruneNodes ---\n");
	c_pruned = 0;
	PruneNodes_r(node);
	Com_Verbose("%5i pruned nodes\n", c_pruned);
}
