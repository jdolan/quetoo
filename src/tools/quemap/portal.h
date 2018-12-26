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

#include "tree.h"

typedef struct portal_s {
	plane_t plane;
	node_t *on_node; // NULL = outside box
	node_t *nodes[2]; // [0] = front side of plane
	struct portal_s *next[2];
	winding_t *winding;

	_Bool side_found; // false if ->side hasn't been checked
	brush_side_t *side; // NULL = non-visible
	face_t *face[2]; // output face in bsp file
} portal_t;

int32_t VisibleContents(int32_t contents);

void MakeHeadnodePortals(tree_t *tree);
void MakeNodePortal(node_t *node);
void SplitNodePortals(node_t *node);

_Bool Portal_VisFlood(const portal_t *p);
void RemovePortalFromNode(portal_t *portal, node_t *l);

_Bool FloodEntities(tree_t *tree);
void FillOutside(node_t *head_node);
void FloodAreas(tree_t *tree);
void MarkVisibleSides(tree_t *tree, int32_t start, int32_t end);
void FreePortal(portal_t *p);
void EmitAreaPortals(void);

void MakeTreePortals(tree_t *tree);
void MakeTreeFaces(tree_t *tree);
