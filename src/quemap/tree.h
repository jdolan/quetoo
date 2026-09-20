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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#pragma once

#include "brush.h"
#include "face.h"
#include "map.h"

#define  PLANE_LEAF -1

typedef struct Node {
  // both leafs and nodes
  struct Node *parent;
  int32_t plane; // -1 = leaf node
  Box3 bounds; // valid after portalization
  Box3 visibleBounds; // valid after face merging
  CsgBrush *volume; // one for each leaf/node
  int32_t contents; // OR of all brush contents, or CONTENTS_NODE, CONTENTS_BLOCK

  // nodes only
  const BrushSide *splitSide; // the side that created the node
  struct Node *children[2];
  Face *faces;
  struct PatchFace *patchFaces;

  // leafs only
  CsgBrush *brushes; // fragments of all brushes in this leaf
  int32_t occupied; // 1 or greater can reach entity
  const Entity *occupant; // for leak file testing
  struct Portal *portals; // also on nodes during construction
} Node;

Node *AllocNode(void);
void FreeNode(Node *node);

typedef struct {
  Node *headNode;
  Node outsideNode;
  Box3 bounds;
} Tree;

Tree *AllocTree(void);
void FreeTree(Tree *tree);
void FreeTreePortals(Tree *tree);
void MergeTreeFaces(Tree *tree);

Tree *BuildTree(CsgBrush *brushes);
