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

extern int c_nodes;

/*
 * FreeTreePortals_r
 */
void FreeTreePortals_r(node_t * node){
	portal_t *p, *nextp;
	int s;

	// free children
	if(node->plane_num != PLANENUM_LEAF){
		FreeTreePortals_r(node->children[0]);
		FreeTreePortals_r(node->children[1]);
	}
	// free portals
	for(p = node->portals; p; p = nextp){
		s = (p->nodes[1] == node);
		nextp = p->next[s];

		RemovePortalFromNode(p, p->nodes[!s]);
		FreePortal(p);
	}
	node->portals = NULL;
}


/*
 * FreeTree_r
 */
void FreeTree_r(node_t * node){
	face_t *f, *nextf;

	// free children
	if(node->plane_num != PLANENUM_LEAF){
		FreeTree_r(node->children[0]);
		FreeTree_r(node->children[1]);
	}
	// free bspbrushes
	FreeBrushList(node->brushes);

	// free faces
	for(f = node->faces; f; f = nextf){
		nextf = f->next;
		FreeFace(f);
	}

	// free the node
	if(node->volume)
		FreeBrush(node->volume);

	if(!threads->integer)
		c_nodes--;
	Z_Free(node);
}


/*
 * FreeTree
 */
void FreeTree(tree_t * tree){
	FreeTreePortals_r(tree->head_node);
	FreeTree_r(tree->head_node);
	Z_Free(tree);
}

/*
 *
 * NODES THAT DON'T SEPERATE DIFFERENT CONTENTS CAN BE PRUNED
 *
 */

int c_pruned;

/*
 * PruneNodes_r
 */
void PruneNodes_r(node_t * node){
	bsp_brush_t *b, *next;

	if(node->plane_num == PLANENUM_LEAF)
		return;
	PruneNodes_r(node->children[0]);
	PruneNodes_r(node->children[1]);

	if((node->children[0]->contents & CONTENTS_SOLID)
	        && (node->children[1]->contents & CONTENTS_SOLID)){
		if(node->faces)
			Com_Error(ERR_FATAL, "node->faces seperating CONTENTS_SOLID\n");
		if(node->children[0]->faces || node->children[1]->faces)
			Com_Error(ERR_FATAL, "!node->faces with children\n");

		// FIXME: free stuff
		node->plane_num = PLANENUM_LEAF;
		node->contents = CONTENTS_SOLID;
		node->detail_seperator = false;

		if(node->brushes)
			Com_Error(ERR_FATAL, "PruneNodes: node->brushlist\n");

		// combine brush lists
		node->brushes = node->children[1]->brushes;

		for(b = node->children[0]->brushes; b; b = next){
			next = b->next;
			b->next = node->brushes;
			node->brushes = b;
		}

		c_pruned++;
	}
}


void PruneNodes(node_t * node){
	Com_Verbose("--- PruneNodes ---\n");
	c_pruned = 0;
	PruneNodes_r(node);
	Com_Verbose("%5i pruned nodes\n", c_pruned);
}
