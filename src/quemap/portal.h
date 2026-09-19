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

typedef struct Portal {
  Plane plane;
  Node *on_node; // NULL = outside box
  Node *nodes[2]; // [0] = front side of plane
  struct Portal *next[2];
  CmWinding *winding;
  BrushSide *side; // NULL = non-visible
  Face *face[2]; // output face in bsp file
} Portal;

void MakeHeadnodePortals(Tree *tree);
void MakeNodePortal(Node *node);
void SplitNodePortals(Node *node);

bool Portal_VisFlood(const Portal *p);
void RemovePortalFromNode(Portal *portal, Node *l);

bool FloodEntities(Tree *tree);
void FillOutside(Tree *tree);
void FindPortalBrushSides(Tree *tree);
void FreePortal(Portal *p);

void MakeTreePortals(Tree *tree);
void MakeTreeFaces(Tree *tree);
