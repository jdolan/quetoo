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

#include <SDL3/SDL_timer.h>
#include <Objectively/Vector.h>

#include "material.h"
#include "tree.h"
#include "portal.h"
#include "qbsp.h"

static SDL_AtomicInt cActiveNodes;

/**
 * @brief Allocates a new BSP tree node with default values.
 */
Node *AllocNode(void) {

  SDL_AddAtomicInt(&cActiveNodes, 1);

  return Mem_TagMalloc(sizeof(Node), (MemTag) MEM_TAG_NODE);
}

/**
 * @brief Frees a single BSP tree node and releases its brush list.
 */
void FreeNode(Node *node) {

  SDL_AddAtomicInt(&cActiveNodes, -1);

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
    if (volume < microVolume) {
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

/**
 * @return The single content bit of the strongest visible content present.
 */
static int32_t VisibleContents(int32_t contents) {

  for (int32_t i = 1; i <= LAST_VISIBLE_CONTENTS; i <<= 1) {
    if (contents & i) {
      return i;
    }
  }

  return 0;
}

/**
 * @return True if every point of @p w lies within `ON_EPSILON` of @p plane.
 */
static bool WindingOnPlane(const CmWinding *w, const Plane *plane) {

  for (int32_t i = 0; i < w->numPoints; i++) {
    if (fabs(Vec3_Dot(w->points[i], plane->normal) - plane->dist) > ON_EPSILON) {
      return false;
    }
  }

  return true;
}

/**
 * @return True if the leaf @p node holds a fragment of @p brush.
 */
static bool LeafHoldsBrush(const Node *node, const Brush *brush) {

  for (const CsgBrush *b = node->brushes; b; b = b->next) {
    if (b->original == brush) {
      return true;
    }
  }

  return false;
}

static Node *faceHeadNode;

/**
 * @return The leaf of the tree being faced that holds @p point.
 */
static Node *PointInLeaf(const Vec3 point) {

  Node *node = faceHeadNode;
  while (node->plane != PLANE_LEAF) {
    const Plane *plane = &planes[node->plane];
    node = node->children[Vec3_Dot(point, plane->normal) - plane->dist >= 0.0 ? 0 : 1];
  }

  return node;
}

/**
 * @return True if the piece @p w of @p side is visible from the first leaf in front of it that
 * does not hold @p brush.
 * @details The leaf is found at points in front of the center of the piece, up to 4 units away.
 * If each of them is still in a leaf that holds the brush, the piece is not visible.
 */
static bool SideVisibleInFront(const CmWinding *w, const CsgBrush *brush, const BrushSide *side) {

  static const float distances[] = { .5f, 1.f, 2.f, 4.f };

  const Vec3 center = Cm_WindingCenter(w);
  const Vec3 normal = planes[side->plane].normal;
  const int32_t contents = brush->original->contents;

  for (size_t i = 0; i < lengthof(distances); i++) {

    const Node *leaf = PointInLeaf(Vec3_Fmaf(center, distances[i], normal));
    if (LeafHoldsBrush(leaf, brush->original)) {
      continue;
    }

    return !(leaf->contents & CONTENTS_SOLID) && (VisibleContents(leaf->contents ^ contents) & contents);
  }

  return false;
}

static int32_t cFaces;

/**
 * @brief Pushes a piece of the brush side @p side of @p brush down the tree, and makes a face of
 * each piece that ends in a leaf where the side is visible.
 * @details At a node on the plane of the side, or within `ON_EPSILON` of it everywhere, the piece
 * goes to the child that the side faces. The last such node on the path holds the faces made from
 * it. Everywhere else the piece is split. Points within `ON_EPSILON` of a plane are on it, so that
 * a piece which only touches a plane is not cut into a sliver with no area.
 *
 * A piece can reach a leaf without a node on its plane, and the parent of the leaf then holds the
 * face, since its space holds the piece. This occurs where two brushes nearly meet, and the leaf
 * between them is bounded by the plane of the other brush. It also occurs where the space in
 * front of a side is too thin to split, less than a unit of volume, so the tree keeps it in the
 * leaf of the brush. The piece then ends inside its own brush, and `SideVisibleInFront` decides.
 * Such a face does not lie on the plane of its node, and the decal walk can miss it.

 *
 * The side is visible in a leaf that is not solid, if the strongest visible content that differs
 * between the leaf and the brush belongs to the brush: solid shows in empty and in water, water
 * shows in empty, and water does not show in water. A brush that is not solid can be seen from
 * inside, so each visible piece of it also makes a face that looks into the brush: the surface of
 * water, seen from under it.
 */
static void ClipSideIntoTree_r(Node *node, CmWinding *w, const CsgBrush *brush, const BrushSide *side, Node *onNode) {

  if (node->plane == PLANE_LEAF) {

    const int32_t contents = brush->original->contents;

    bool visible = !(node->contents & CONTENTS_SOLID) && (VisibleContents(node->contents ^ contents) & contents);

    if (!visible && !onNode && LeafHoldsBrush(node, brush->original)) {
      visible = SideVisibleInFront(w, brush, side);
    }

    if (!visible) {
      Cm_FreeWinding(w);
      return;
    }

    if (!onNode) {
      Com_Verbose("Brush side %s @ %s is not on a node plane\n",
                  materials[side->original->material].cm->name, vtos(Cm_WindingCenter(w)));
      onNode = node->parent;
    }

    if (!(contents & CONTENTS_SOLID)) {

      Face *inside = AllocFace();

      inside->brushSide = side->original;
      inside->plane = side->plane ^ 1;
      inside->w = Cm_ReverseWinding(w);

      inside->next = onNode->faces;
      onNode->faces = inside;

      cFaces++;
    }

    Face *face = AllocFace();

    face->brushSide = side->original;
    face->plane = side->plane;
    face->w = w;

    face->next = onNode->faces;
    onNode->faces = face;

    cFaces++;
    return;
  }

  if ((side->plane & ~1) == node->plane) {
    ClipSideIntoTree_r(node->children[side->plane & 1], w, brush, side, node);
    return;
  }

  const Plane *plane = &planes[node->plane];

  if (WindingOnPlane(w, plane)) {
    const bool facing = Vec3_Dot(planes[side->plane].normal, plane->normal) > 0.f;
    ClipSideIntoTree_r(node->children[facing ? 0 : 1], w, brush, side, node);
    return;
  }

  CmWinding *front, *back;
  Cm_SplitWinding(w, plane->normal, plane->dist, ON_EPSILON, &front, &back);
  Cm_FreeWinding(w);

  if (front) {
    ClipSideIntoTree_r(node->children[0], front, brush, side, onNode);
  }

  if (back) {
    ClipSideIntoTree_r(node->children[1], back, brush, side, onNode);
  }
}

/**
 * @brief A visible side of a brush that the tree was built from, and its order in the list of
 * brushes.
 */
typedef struct {
  const CsgBrush *brush;
  const BrushSide *side;
  int32_t order;
} TreeFaceSide;

/**
 * @brief Orders tree face sides by plane, then by brush, then by their order in the list.
 */
static int32_t TreeFaceSideCmp(const void *a, const void *b) {

  const TreeFaceSide *sa = a;
  const TreeFaceSide *sb = b;

  if (sa->side->plane != sb->side->plane) {
    return sa->side->plane - sb->side->plane;
  }

  if (sa->brush->original != sb->brush->original) {
    return (int32_t) (sa->brush->original - sb->brush->original);
  }

  return sa->order - sb->order;
}

/**
 * @brief Removes the part of each winding in @p pieces that the convex winding @p clip covers.
 * @details Each piece is split along the edges of @p clip, on the plane with @p normal. The parts
 * outside an edge are kept, and the part inside every edge is freed. A piece that @p clip does not
 * cover is kept whole, since the lines of the edges would cut it for nothing.
 * @return The pieces that remain, which replace @p pieces.
 */
static Vector *SubtractWinding(Vector *pieces, const CmWinding *clip, const Vec3 normal) {

  Vector *out = $(alloc(Vector), initWithSize, sizeof(CmWinding *));

  const Vec3 center = Cm_WindingCenter(clip);
  const Box3 bounds = Box3_Expand(Cm_WindingBounds(clip), ON_EPSILON);

  for (size_t i = 0; i < pieces->count; i++) {

    CmWinding *piece = VectorValue(pieces, CmWinding *, i);

    if (!Box3_Intersects(Cm_WindingBounds(piece), bounds)) {
      $(out, add, &piece);
      continue;
    }

    const size_t count = out->count;

    CmWinding *w = Cm_CopyWinding(piece);

    for (int32_t j = 0; j < clip->numPoints && w; j++) {

      const Vec3 a = clip->points[j];
      const Vec3 b = clip->points[(j + 1) % clip->numPoints];

      const Vec3 edge = Vec3_Subtract(b, a);
      if (Vec3_Length(edge) < ON_EPSILON) {
        continue;
      }

      Vec3 outward = Vec3_Normalize(Vec3_Cross(edge, normal));
      if (Vec3_Dot(Vec3_Subtract(center, a), outward) > 0.f) {
        outward = Vec3_Negate(outward);
      }

      CmWinding *front, *back;
      Cm_SplitWinding(w, outward, Vec3_Dot(a, outward), ON_EPSILON, &front, &back);
      Cm_FreeWinding(w);

      if (front) {
        $(out, add, &front);
      }

      w = back;
    }

    if (w && Cm_WindingArea(w) > ON_EPSILON) {
      Cm_FreeWinding(w);
      Cm_FreeWinding(piece);
      continue;
    }

    if (w) {
      Cm_FreeWinding(w);
    }

    while (out->count > count) {
      Cm_FreeWinding(VectorValue(out, CmWinding *, out->count - 1));
      $(out, removeAt, out->count - 1);
    }

    $(out, add, &piece);
  }

  release(pieces);
  return out;
}

/**
 * @brief Makes the faces of the tree from the sides of @p brushes, the brushes that the tree was
 * built from.
 * @details Each face is made from the brush side that it shows, so that no step has to find the
 * side of a face again. CSG removes most parts of each side that lie inside a brush that wins over
 * it, and the tree removes the parts that face into solid or unreachable leafs.
 *
 * CSG leaves some brushes overlapping: see-through brushes never cut each other, and two solids
 * are left alone if cutting either one would split it. Their coplanar sides would both make faces,
 * which blend twice or fight in the depth buffer. So before a side is clipped into the tree, the
 * sides of earlier brushes on the same plane, facing the same way, with the same visible contents,
 * are subtracted from it.
 */
void MakeTreeFaces(Tree *tree, const CsgBrush *brushes) {

  Com_Verbose("--- MakeTreeFaces ---\n");

  cFaces = 0;
  faceHeadNode = tree->headNode;

  int32_t numSides = 0;
  for (const CsgBrush *brush = brushes; brush; brush = brush->next) {
    numSides += brush->numBrushSides;
  }

  TreeFaceSide *sides = Mem_Malloc(numSides * sizeof(TreeFaceSide));
  numSides = 0;

  for (const CsgBrush *brush = brushes; brush; brush = brush->next) {

    if (!VisibleContents(brush->original->contents)) {
      continue;
    }

    const BrushSide *side = brush->brushSides;
    for (int32_t i = 0; i < brush->numBrushSides; i++, side++) {

      if (!side->original || !side->winding) {
        continue;
      }

      if (side->surface & (SURF_NODE | SURF_BEVEL)) {
        continue;
      }

      if (side->original->surface & (SURF_NO_DRAW | SURF_SKIP)) {
        continue;
      }

      sides[numSides] = (TreeFaceSide) {
        .brush = brush,
        .side = side,
        .order = numSides
      };
      numSides++;
    }
  }

  qsort(sides, numSides, sizeof(TreeFaceSide), TreeFaceSideCmp);

  for (int32_t i = 0; i < numSides; i++) {

    const TreeFaceSide *s = &sides[i];
    const int32_t contents = VisibleContents(s->brush->original->contents);
    const Vec3 normal = planes[s->side->plane].normal;

    Vector *pieces = $(alloc(Vector), initWithSize, sizeof(CmWinding *));

    CmWinding *w = Cm_CopyWinding(s->side->winding);
    $(pieces, add, &w);

    const Box3 bounds = Box3_Expand(Cm_WindingBounds(s->side->winding), ON_EPSILON);

    for (int32_t j = i - 1; j >= 0 && sides[j].side->plane == s->side->plane && pieces->count; j--) {

      const TreeFaceSide *t = &sides[j];

      if (!Box3_Intersects(Cm_WindingBounds(t->side->winding), bounds)) {
        continue;
      }

      if (t->brush->original == s->brush->original) {
        continue;
      }

      if (VisibleContents(t->brush->original->contents) != contents) {
        continue;
      }

      pieces = SubtractWinding(pieces, t->side->winding, normal);
    }

    for (size_t j = 0; j < pieces->count; j++) {
      ClipSideIntoTree_r(tree->headNode, VectorValue(pieces, CmWinding *, j), s->brush, s->side, NULL);
    }

    release(pieces);
  }

  Mem_Free(sides);

  Com_Verbose("%5i faces\n", cFaces);
}

static int32_t cMergedFaces;

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

      cMergedFaces++;

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
  cMergedFaces = 0;
  MergeFaces_r(tree->headNode);
  CalcNodeVisibleBounds_r(tree->headNode);
  Com_Verbose("%5i merged faces\n", cMergedFaces);
}
