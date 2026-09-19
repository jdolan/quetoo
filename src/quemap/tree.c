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

#include <SDL3/SDL_timer.h>
#include <Objectively/Vector.h>

#include "tree.h"
#include "portal.h"
#include "qbsp.h"

static SDL_AtomicInt c_active_nodes;

/**
 * @brief Allocates a new BSP tree node with default values.
 */
Node *AllocNode(void) {

  SDL_AddAtomicInt(&c_active_nodes, 1);

  return Mem_TagMalloc(sizeof(Node), (MemTag) MEM_TAG_NODE);
}

/**
 * @brief Frees a single BSP tree node and releases its brush list.
 */
void FreeNode(Node *node) {

  SDL_AddAtomicInt(&c_active_nodes, -1);

  Mem_Free(node);
}

/**
 * @brief Allocates a new BSP tree and its head node.
 */
Tree *AllocTree(void) {

  return Mem_TagMalloc(sizeof(Tree), (MemTag) MEM_TAG_TREE);
}

/**
 * @brief Recursively frees all portals connected to the subtree rooted at node.
 */
static void FreeTreePortals_r(Node *node) {

  // free children
  if (node->plane != PLANE_LEAF) {
    FreeTreePortals_r(node->children[0]);
    FreeTreePortals_r(node->children[1]);
  }
  // free portals
  Portal *next;
  for (Portal *p = node->portals; p; p = next) {
    const int32_t s = (p->nodes[1] == node);
    next = p->next[s];

    RemovePortalFromNode(p, p->nodes[!s]);
    FreePortal(p);
  }
  node->portals = NULL;
}

void FreeTreePortals(Tree *tree) {
  FreeTreePortals_r(tree->headNode);
}

/**
 * @brief Recursively frees all nodes, faces, and brushes in the subtree rooted at node.
 */
void FreeTree_r(Node *node) {

  // free children
  if (node->plane != PLANE_LEAF) {
    FreeTree_r(node->children[0]);
    FreeTree_r(node->children[1]);
  }

  // free brushes
  FreeBrushes(node->brushes);

  // free faces
  Face *nextf;
  for (Face *f = node->faces; f; f = nextf) {
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
 * @brief Frees all portals and all nodes in the tree, then frees the tree itself.
 */
void FreeTree(Tree *tree) {

  Com_Verbose("--- FreeTree ---\n");
  FreeTreePortals_r(tree->headNode);
  FreeTree_r(tree->headNode);
  Mem_Free(tree);
  Com_Verbose("--- FreeTree complete ---\n");
}

/**
 * @brief Marks the node as a leaf, recording its contents and the brushes that generated it.
 */
static Node *LeafNode(Node *node, CsgBrush *brushes) {

  node->plane = PLANE_LEAF;
  node->contents = CONTENTS_NONE;

  for (CsgBrush *b = brushes; b; b = b->next) {
    node->contents |= b->original->contents;
  }

  node->brushes = brushes;

  Progress("Building tree", -1);

  return node;
}

/**
 * @return A heuristic value for splitting the brushes list by the given side. Higher values mean
 * that this face produces a more balanced tree while splitting as few brushes as possible.
 */
static int32_t SelectSplitSideHeuristic(const BrushSide *side, const CsgBrush *brushes) {

  if (side->surface & SURF_HINT) {
    return INT32_MAX;
  }

  const int32_t plane = side->plane & ~1;

  int32_t front = 0, back = 0, on = 0, numSplitSides = 0;

  for (const CsgBrush *brush = brushes; brush; brush = brush->next) {

    int32_t i;
    const int32_t s = BrushOnPlaneSideSplits(brush, plane, &i);

    if (s & SIDE_FRONT) {
      front++;
    }
    if (s & SIDE_BACK) {
      back++;
    }
    if (s & SIDE_ON) {
      on++;
    }

    numSplitSides += i;
  }

  // give a value estimate for using this plane

  int32_t value = 5 * on - 5 * numSplitSides - abs(front - back);

  if (AXIAL(&planes[plane])) {
    value += 5;
  }

  return value;
}

/**
 * @return The original brush side from brushes with the highest heuristic value.
 */
static const BrushSide *SelectSplitSide(Node *node, CsgBrush *brushes) {

  const BrushSide *bestSide = NULL;
  int32_t bestValue = INT32_MIN;

  Vector *cache = $(alloc(Vector), initWithSize, sizeof(intptr_t));

  bool haveStructural = false;
  for (const CsgBrush *brush = brushes; brush; brush = brush->next) {
    if (!(brush->original->contents & CONTENTS_DETAIL)) {
      if (brush->original->contents & CONTENTS_MASK_VISIBLE) {
        haveStructural = true;
        break;
      }
    }
  }

  for (const CsgBrush *brush = brushes; brush; brush = brush->next) {

    if (brush->original->contents & CONTENTS_DETAIL) {
      if (haveStructural) {
        continue;
      }
    }

    const BrushSide *side = brush->brushSides;
    for (int32_t i = 0; i < brush->numBrushSides; i++, side++) {

      if (side->surface & SURF_BEVEL) {
        continue;
      }
      if (side->surface & SURF_NODE) {
        continue;
      }

      assert(side->winding);

      const int32_t plane = side->plane ^ 1;
      bool cached = false;
      for (size_t j = 0; j < cache->count; j++) {
        if (VectorValue(cache, intptr_t, j) == plane) {
          cached = true;
          break;
        }
      }

      if (cached) {
        continue;
      }

      CsgBrush *front, *back;
      SplitBrush(node->volume, plane, &front, &back);
      const bool validSplit = (front && back);
      if (front) {
        FreeBrush(front);
      }
      if (back) {
        FreeBrush(back);
      }
      if (!validSplit) {
        continue;
      }

      const int32_t value = SelectSplitSideHeuristic(side, brushes);
      if (value > bestValue) {
        bestSide = side->original;
        bestValue = value;
      }

      intptr_t cachedPlane = plane;
      $(cache, add, &cachedPlane);
    }
  }

  release(cache);

  return bestSide;
}

/**
 * @brief Splits the brush list against the given plane into front and back sub-lists.
 */
static void SplitBrushes(CsgBrush *brushes, const Node *node, CsgBrush **front, CsgBrush **back) {

  *front = *back = NULL;

  for (const CsgBrush *brush = brushes; brush; brush = brush->next) {

    const int32_t s = BrushOnPlaneSide(brush, node->plane);
    if (s == SIDE_BOTH) {
      CsgBrush *frontBrush, *backBrush;
      SplitBrush(brush, node->plane, &frontBrush, &backBrush);
      if (frontBrush) {
        frontBrush->next = *front;
        *front = frontBrush;
      }
      if (backBrush) {
        backBrush->next = *back;
        *back = backBrush;
      }
      continue;
    }

    CsgBrush *newBrush = CopyBrush(brush);

    if (s & SIDE_FRONT) {
      newBrush->next = *front;
      *front = newBrush;
      continue;
    }
    if (s & SIDE_BACK) {
      newBrush->next = *back;
      *back = newBrush;
      continue;
    }
  }
}

/**
 * @brief Recursively split the node and filter brushes into its children. Nodes larger
 * than `BSP_BLOCK_SIZE` are split in half on their longest axis to produce a balanced tree.
 * Smaller nodes are split using a brush side heuristic to produce more optimal geometry.
 */
static Node *BuildTree_r(Node *node, CsgBrush *brushes) {

  const Vec3 size = Box3_Size(node->volume->bounds);

  int32_t axis = 0;
  float longestSide = 0.f;
  for (int32_t i = 0; i < 3; i++) {
    if (size.xyz[i] > longestSide) {
      longestSide = size.xyz[i];
      axis = i;
    }
  }

  if (longestSide > BSP_BLOCK_SIZE) {
    node->contents = CONTENTS_BLOCK;

    if (node->parent) {
      node->parent->contents = CONTENTS_NODE;
    }

    Vec3 normal = Vec3_Zero();
    normal.xyz[axis] = 1.f;

    const int32_t dist = Box3_Center(node->volume->bounds).xyz[axis];
    node->plane = FindPlane(normal, dist) & ~1;

  } else {

    if (node->parent == NULL) {
      node->contents = CONTENTS_BLOCK;
    } else {
      node->contents = CONTENTS_NODE;
    }

    node->splitSide = SelectSplitSide(node, brushes);
    if (!node->splitSide) {
      return LeafNode(node, brushes);
    }

    node->plane = node->splitSide->plane & ~1;
  }

  node->children[0] = AllocNode();
  node->children[0]->parent = node;

  node->children[1] = AllocNode();
  node->children[1]->parent = node;

  SplitBrush(node->volume, node->plane, &node->children[0]->volume, &node->children[1]->volume);

  CsgBrush *front, *back;
  SplitBrushes(brushes, node, &front, &back);

  FreeBrushes(brushes);

  BuildTree_r(node->children[0], front);
  BuildTree_r(node->children[1], back);

  return node;
}

/**
 * @brief Recursively partitions the brush list into a BSP tree, selecting the best split plane at each step.
 * @remark The incoming list will be freed before exiting
 */
Tree *BuildTree(CsgBrush *brushes) {

  assert(brushes);

  Com_Debug(DEBUG_ALL, "--- BuildTree ---\n");

  const uint32_t start = (uint32_t) SDL_GetTicks();

  Tree *tree = AllocTree();

  tree->bounds = Box3_Null();

  int32_t numBrushes = 0;
  int32_t numBrushSides = 0;

  for (CsgBrush *b = brushes; b; b = b->next) {
    numBrushes++;

    const float volume = BrushVolume(b);
    if (volume < micro_volume) {
      Com_Warn("Entity %d brush %d produced microvolume\n", b->original->entity, b->original->brush);
    }

    const BrushSide *s = b->brushSides;
    for (int32_t i = 0; i < b->numBrushSides; i++, s++) {
      if (s->surface & SURF_BEVEL) {
        continue;
      }
      if (s->surface & SURF_NODE) {
        continue;
      }
      numBrushSides++;
    }

    tree->bounds = Box3_Union(tree->bounds, b->bounds);
  }

  assert(numBrushes);
  assert(numBrushSides);

  Com_Debug(DEBUG_ALL, "%5i brushes\n", numBrushes);
  Com_Debug(DEBUG_ALL, "%5i brush sides\n", numBrushSides);

  tree->headNode = AllocNode();
  tree->headNode->volume = BrushFromBounds(Box3_Expand(tree->bounds, 1.f));

  BuildTree_r(tree->headNode, brushes);

  Com_Print("\r%-24s [100%%] %d ms\n", "Building tree", (uint32_t) SDL_GetTicks() - start);

  return tree;
}

static int32_t c_merged_faces;

/**
 * @brief Recursively merges coplanar, co-material faces in the subtree rooted at node.
 */
static void MergeFaces_r(Node *node) {

  if (node->plane == PLANE_LEAF) {
    return;
  }

again:
  for (Face *a = node->faces; a; a = a->next) {
    if (a->merged) {
      continue;
    }
    for (Face *b = node->faces; b; b = b->next) {
      if (a == b) {
        continue;
      }

      if (b->merged) {
        continue;
      }

      Face *merged = MergeFaces(a, b);
      if (!merged) {
        continue;
      }

      c_merged_faces++;

      merged->next = node->faces;
      node->faces = merged;

      goto again;
    }
  }

  MergeFaces_r(node->children[0]);
  MergeFaces_r(node->children[1]);
}

/**
 * @brief Recursively computes the visible bounds of each node from its children's visible bounds.
 */
static Box3 CalcNodeVisibleBounds_r(Node *node) {

  if (node->plane == PLANE_LEAF) {
    return Box3_Null();
  }

  const Box3 a = CalcNodeVisibleBounds_r(node->children[0]);
  const Box3 b = CalcNodeVisibleBounds_r(node->children[1]);

  node->visibleBounds = Box3_Union(a, b);

  for (Face *face = node->faces; face; face = face->next) {

    Face *f = face;
    while (f->merged) {
      f = f->merged;
    }

    assert(f->w);
    node->visibleBounds = Box3_Union(node->visibleBounds, Cm_WindingBounds(f->w));
  }

  return node->visibleBounds;
}


/**
 * @brief Merges coplanar faces across the entire tree, then recalculates each node's visible bounds.
 */
void MergeTreeFaces(Tree *tree) {
  Com_Verbose("--- MergeTreeFaces ---\n");
  c_merged_faces = 0;
  MergeFaces_r(tree->headNode);
  CalcNodeVisibleBounds_r(tree->headNode);
  Com_Verbose("%5i merged faces\n", c_merged_faces);
}
