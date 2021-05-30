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

#pragma once

#include "brush.h"
#include "face.h"
#include "map.h"

#define	PLANE_LEAF -1

typedef struct node_s {
	// both leafs and nodes
	struct node_s *parent;
	int32_t plane; // -1 = leaf node
	box3_t bounds; // valid after portalization
	csg_brush_t *volume; // one for each leaf/node

	// nodes only
	const brush_side_t *split_side; // the side that created the node
	struct node_s *children[2];
	face_t *faces;

	// leafs only
	csg_brush_t *brushes; // fragments of all brushes in this leaf
	int32_t contents; // OR of all brush contents
	int32_t occupied; // 1 or greater can reach entity
	const entity_t *occupant; // for leak file testing
	int32_t cluster; // for portal file writing
	struct portal_s *portals; // also on nodes during construction
} node_t;

node_t *AllocNode(void);
void FreeNode(node_t *node);

typedef struct {
	node_t *head_node;
	node_t outside_node;
	box3_t bounds;
} tree_t;

tree_t *AllocTree(void);
void FreeTree(tree_t *tree);
void FreeTree_r(node_t *node);
void FreeTreePortals_r(node_t *node);
void PruneNodes_r(node_t *node);
void PruneNodes(node_t *node);

tree_t *BuildTree(csg_brush_t *brushes);
void MergeNodeFaces(node_t *node);
