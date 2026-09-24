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

#include <Objectively/Vector.h>

#include "bsp.h"
#include "drawface.h"
#include "face.h"
#include "map.h"
#include "material.h"
#include "patch.h"
#include "portal.h"
#include "qbsp.h"

/**
 * @brief The draw faces of the model being emitted.
 */
DrawFace *drawFaces;

int32_t numDrawFaces;

/**
 * @brief For each face of the model being emitted, the `CONTENTS_BLOCK` node that holds it,
 * whether it is drawn as part of a hull, and whether its winding faces the same way as the plane
 * of its brush side.
 */
static struct {
  int32_t blockNode;
  bool hulled;
  int32_t facing;
} *faceGroups;

static const BspModel *faceGroupsModel;

/**
 * @brief The elements of the hulls of the model being emitted, which draw faces point into.
 */
static int32_t *hullElements;

static int32_t numHullElements;

/**
 * @brief A vertex that a hull is built from, with its position in the plane of the hull.
 */
typedef struct {
  Vec3 position;
  int32_t vertex;
  double x, y;
  bool used;
} HullPoint;

/**
 * @brief A vertex that lies on an edge of a hull, and its distance along that edge.
 */
typedef struct {
  double t;
  int32_t point;
} HullEdgePoint;

static Vec3 hullNormal;

/**
 * @brief Assigns each face of @p mod to the first `CONTENTS_BLOCK` node that holds its center.
 * @details Each face is given to one block only, so that the faces of a brush side in one block
 * can be joined, and so that no face is drawn twice.
 */
static void AssignFaceBlocks_r(const BspModel *mod, const BspNode *node) {

  if (node->contents == CONTENTS_BLOCK) {

    const int32_t nodeNum = (int32_t) (ptrdiff_t) (node - bspFile.nodes);

    const BspFace *face = bspFile.faces + mod->firstFace;
    for (int32_t i = 0; i < mod->numFaces; i++, face++) {
      if (faceGroups[i].blockNode == -1 && Box3_ContainsPoint(node->bounds, Box3_Center(face->bounds))) {
        faceGroups[i].blockNode = nodeNum;
      }
    }
    return;
  }

  if (node->contents == CONTENTS_NODE) {
    AssignFaceBlocks_r(mod, bspFile.nodes + node->children[0]);
    AssignFaceBlocks_r(mod, bspFile.nodes + node->children[1]);
  }
}

/**
 * @return The normal of the winding of @p face, by Newell's method, scaled by twice its area.
 */
static Vec3 FaceWindingNormal(const BspFace *face) {

  Vec3 normal = Vec3_Zero();

  const BspVertex *v = bspFile.vertexes + face->firstVertex;
  for (int32_t i = 0; i < face->numVertexes; i++) {

    const Vec3 a = v[i].position;
    const Vec3 b = v[(i + 1) % face->numVertexes].position;

    normal.x += (a.y - b.y) * (a.z + b.z);
    normal.y += (a.z - b.z) * (a.x + b.x);
    normal.z += (a.x - b.x) * (a.y + b.y);
  }

  return normal;
}

/**
 * @return True if @p face may be drawn as part of the hull of its brush side.
 * @details A hull covers area that no face covers, which is correct only where an opaque brush
 * hides that area. Two touching see-through brushes, such as a waterfall on a pool, make no face
 * where they touch, and nothing hides that area, so only opaque sides are hulled.
 */
static bool FaceIsHulled(const BspFace *face) {

  if (face->brushSide < 0) {
    return false;
  }

  const BspBrushSide *side = bspFile.brushSides + face->brushSide;

  if (!(side->contents & CONTENTS_SOLID)) {
    return false;
  }

  if (side->surface & (SURF_LIQUID | SURF_MASK_BLEND | SURF_ALPHA_TEST)) {
    return false;
  }

  return true;
}

/**
 * @brief Orders the faces of the model by block, brush side and facing, then by index.
 */
static int32_t FaceGroupCmp(const void *a, const void *b) {

  const int32_t i = *(const int32_t *) a;
  const int32_t j = *(const int32_t *) b;

  if (faceGroups[i].blockNode != faceGroups[j].blockNode) {
    return faceGroups[i].blockNode - faceGroups[j].blockNode;
  }

  const BspFace *fi = bspFile.faces + faceGroupsModel->firstFace + i;
  const BspFace *fj = bspFile.faces + faceGroupsModel->firstFace + j;

  if (fi->brushSide != fj->brushSide) {
    return fi->brushSide - fj->brushSide;
  }

  if (faceGroups[i].facing != faceGroups[j].facing) {
    return faceGroups[i].facing - faceGroups[j].facing;
  }

  return i - j;
}

/**
 * @return True if the faces @p i and @p j of the model are drawn as one hull.
 */
static bool FaceGroupEqual(int32_t i, int32_t j) {

  const BspFace *fi = bspFile.faces + faceGroupsModel->firstFace + i;
  const BspFace *fj = bspFile.faces + faceGroupsModel->firstFace + j;

  if (!faceGroups[i].hulled || !faceGroups[j].hulled || fi->brushSide != fj->brushSide) {
    return false;
  }

  return faceGroups[i].blockNode == faceGroups[j].blockNode &&
         faceGroups[i].facing == faceGroups[j].facing;
}

/**
 * @brief Orders hull points by position, then those with a normal toward the hull first, then by
 * vertex, so that one vertex is kept for each position.
 */
static int32_t HullPointPositionCmp(const void *a, const void *b) {

  const HullPoint *pa = a;
  const HullPoint *pb = b;

  for (int32_t i = 0; i < 3; i++) {
    if (pa->position.xyz[i] != pb->position.xyz[i]) {
      return pa->position.xyz[i] < pb->position.xyz[i] ? -1 : 1;
    }
  }

  const bool aToward = Vec3_Dot(bspFile.vertexes[pa->vertex].normal, hullNormal) > 0.f;
  const bool bToward = Vec3_Dot(bspFile.vertexes[pb->vertex].normal, hullNormal) > 0.f;

  if (aToward != bToward) {
    return aToward ? -1 : 1;
  }

  return pa->vertex - pb->vertex;
}

/**
 * @brief Orders hull points by their position in the plane of the hull.
 */
static int32_t HullPointPlaneCmp(const void *a, const void *b) {

  const HullPoint *pa = a;
  const HullPoint *pb = b;

  if (pa->x != pb->x) {
    return pa->x < pb->x ? -1 : 1;
  }

  if (pa->y != pb->y) {
    return pa->y < pb->y ? -1 : 1;
  }

  return pa->vertex - pb->vertex;
}

/**
 * @brief Orders the points on one edge of a hull by their distance along it.
 */
static int32_t HullEdgePointCmp(const void *a, const void *b) {

  const HullEdgePoint *pa = a;
  const HullEdgePoint *pb = b;

  if (pa->t != pb->t) {
    return pa->t < pb->t ? -1 : 1;
  }

  return pa->point - pb->point;
}

/**
 * @brief Emits the convex hull of the faces of one brush side in one block, as one draw face.
 * @details The tree cuts a brush side into faces along every plane that it splits space with,
 * and only some of those cuts follow the brushes that hide part of the side. The draw elements do
 * not need the cuts: the hull of the faces covers each of them, and each part of the hull that no
 * face covers lies behind a brush that hides it. The faces themselves are kept for decals.
 *
 * The hull emits no vertexes of its own: its triangles point to the vertexes of its faces, which
 * decals need anyway. Vertexes closer than `ON_EPSILON` are one vertex. Vertexes that lie on an
 * edge of the hull are kept, since the faces of other brush sides may meet them there. Vertexes
 * inside the hull are not kept.
 *
 * Brushes that overlap with coplanar sides are a fault in the map: both hulls are drawn, and they
 * fight in the depth buffer.
 * @return False if the faces have no hull with three corners.
 */
static bool EmitHull(const BspModel *mod, const int32_t *group, int32_t count, DrawFace *out) {

  const BspFace *first = bspFile.faces + mod->firstFace + group[0];
  const BspBrushSide *side = bspFile.brushSides + first->brushSide;

  hullNormal = bspFile.planes[side->plane].normal;
  if (!faceGroups[group[0]].facing) {
    hullNormal = Vec3_Negate(hullNormal);
  }

  int32_t numPoints = 0;
  for (int32_t i = 0; i < count; i++) {
    numPoints += bspFile.faces[mod->firstFace + group[i]].numVertexes;
  }

  HullPoint *points = Mem_Malloc(numPoints * sizeof(HullPoint));

  numPoints = 0;
  out->bounds = Box3_Null();

  for (int32_t i = 0; i < count; i++) {
    const BspFace *face = bspFile.faces + mod->firstFace + group[i];
    for (int32_t j = 0; j < face->numVertexes; j++) {
      points[numPoints++] = (HullPoint) {
        .position = bspFile.vertexes[face->firstVertex + j].position,
        .vertex = face->firstVertex + j
      };
    }
    out->bounds = Box3_Union(out->bounds, face->bounds);
  }

  qsort(points, numPoints, sizeof(HullPoint), HullPointPositionCmp);

  int32_t numUnique = 0;
  for (int32_t i = 0; i < numPoints; i++) {

    int32_t j;
    for (j = 0; j < numUnique; j++) {
      if (Vec3_DistanceSquared(points[j].position, points[i].position) < ON_EPSILON * ON_EPSILON) {
        break;
      }
    }

    if (j == numUnique) {
      points[numUnique++] = points[i];
    }
  }
  numPoints = numUnique;

  const Vec3 n = hullNormal;
  const Vec3 ref = fabsf(n.x) <= fabsf(n.y) && fabsf(n.x) <= fabsf(n.z) ? MakeVec3(1.f, 0.f, 0.f) :
                   fabsf(n.y) <= fabsf(n.z) ? MakeVec3(0.f, 1.f, 0.f) : MakeVec3(0.f, 0.f, 1.f);

  const Vec3 u = Vec3_Normalize(Vec3_Cross(ref, n));
  const Vec3 v = Vec3_Cross(n, u);

  for (int32_t i = 0; i < numPoints; i++) {
    const Vec3 p = points[i].position;
    points[i].x = (double) p.x * u.x + (double) p.y * u.y + (double) p.z * u.z;
    points[i].y = (double) p.x * v.x + (double) p.y * v.y + (double) p.z * v.z;
  }

  int32_t start = 0;
  for (int32_t i = 1; i < numPoints; i++) {
    if (HullPointPlaneCmp(&points[i], &points[start]) < 0) {
      start = i;
    }
  }

  int32_t *corners = Mem_Malloc((numPoints + 1) * sizeof(int32_t));
  int32_t numCorners = 0;

  int32_t *visit = Mem_Malloc(numPoints * sizeof(int32_t));
  for (int32_t i = 0; i < numPoints; i++) {
    visit[i] = -1;
  }

  for (int32_t p = start; visit[p] == -1;) {

    visit[p] = numCorners;
    corners[numCorners++] = p;

    int32_t best = -1;
    for (int32_t q = 0; q < numPoints; q++) {

      if (q == p) {
        continue;
      }

      if (best == -1) {
        best = q;
        continue;
      }

      const HullPoint *o = &points[p], *b = &points[best], *c = &points[q];

      const double bx = b->x - o->x, by = b->y - o->y;
      const double cx = c->x - o->x, cy = c->y - o->y;
      const double cross = bx * cy - by * cx;

      if (cross < -ON_EPSILON * sqrt(bx * bx + by * by)) {
        best = q;
      } else if (cross <= ON_EPSILON * sqrt(bx * bx + by * by) && cx * cx + cy * cy > bx * bx + by * by) {
        best = q;
      }
    }

    if (best == -1) {
      break;
    }

    p = best;
    if (visit[p] != -1) {
      memmove(corners, corners + visit[p], (numCorners - visit[p]) * sizeof(int32_t));
      numCorners -= visit[p];
      break;
    }
  }

  Mem_Free(visit);

  corners[numCorners] = corners[0];

  if (numCorners < 3) {
    Com_Warn("Brush side %s @ %s has no hull, drawing its %d faces\n",
             bspFile.materials[side->material].name, vtos(Box3_Center(out->bounds)), count);
    Mem_Free(corners);
    Mem_Free(points);
    return false;
  }

  for (int32_t i = 0; i < numCorners; i++) {
    points[corners[i]].used = true;
  }

  CmWinding *w = Cm_AllocWinding(numPoints);
  int32_t *source = Mem_Malloc(numPoints * sizeof(int32_t));
  HullEdgePoint *edgePoints = Mem_Malloc(numPoints * sizeof(HullEdgePoint));

  for (int32_t i = 0; i < numCorners; i++) {

    const HullPoint *p = &points[corners[i]];
    const HullPoint *q = &points[corners[i + 1]];

    source[w->numPoints] = p->vertex;
    w->points[w->numPoints++] = p->position;

    const double dx = q->x - p->x, dy = q->y - p->y;
    const double length = sqrt(dx * dx + dy * dy);

    int32_t numEdgePoints = 0;
    for (int32_t j = 0; j < numPoints; j++) {

      if (points[j].used) {
        continue;
      }

      const double rx = points[j].x - p->x, ry = points[j].y - p->y;
      if (fabs(rx * dy - ry * dx) > ON_EPSILON * length) {
        continue;
      }

      const double t = (rx * dx + ry * dy) / (length * length);
      if (t * length <= ON_EPSILON || (1.0 - t) * length <= ON_EPSILON) {
        continue;
      }

      edgePoints[numEdgePoints++] = (HullEdgePoint) { .t = t, .point = j };
    }

    qsort(edgePoints, numEdgePoints, sizeof(HullEdgePoint), HullEdgePointCmp);

    for (int32_t j = 0; j < numEdgePoints; j++) {
      HullPoint *r = &points[edgePoints[j].point];
      r->used = true;

      source[w->numPoints] = r->vertex;
      w->points[w->numPoints++] = r->position;
    }
  }

  int32_t *elements = hullElements + numHullElements;
  const int32_t numElements = Cm_ElementsForWinding(w, elements);

  for (int32_t i = 0; i < numElements; i++) {
    elements[i] = source[elements[i]];
  }

  numHullElements += numElements;

  out->face = first;
  out->blockNode = faceGroups[group[0]].blockNode;
  out->elements = elements;
  out->numElements = numElements;

  Mem_Free(edgePoints);
  Mem_Free(source);
  Cm_FreeWinding(w);
  Mem_Free(corners);
  Mem_Free(points);

  return numElements > 0;
}

/**
 * @brief Builds the draw faces of @p mod: one hull for the faces of each brush side in each
 * block, and one draw face for each patch face and each face that is not drawn.
 */
void EmitDrawFaces(const BspModel *mod) {

  faceGroupsModel = mod;
  faceGroups = Mem_Malloc(mod->numFaces * sizeof(*faceGroups));

  drawFaces = Mem_Malloc(mod->numFaces * sizeof(DrawFace));
  numDrawFaces = 0;

  int32_t numVertexes = 0;

  const BspFace *face = bspFile.faces + mod->firstFace;
  for (int32_t i = 0; i < mod->numFaces; i++, face++) {

    faceGroups[i].blockNode = -1;

    if (FaceIsHulled(face)) {
      const BspBrushSide *side = bspFile.brushSides + face->brushSide;
      faceGroups[i].hulled = true;
      faceGroups[i].facing = Vec3_Dot(FaceWindingNormal(face), bspFile.planes[side->plane].normal) > 0.f;
    }

    numVertexes += face->numVertexes;
  }

  hullElements = Mem_Malloc(3 * numVertexes * sizeof(int32_t));
  numHullElements = 0;

  AssignFaceBlocks_r(mod, bspFile.nodes + mod->headNode);

  int32_t *order = Mem_Malloc(mod->numFaces * sizeof(int32_t));
  for (int32_t i = 0; i < mod->numFaces; i++) {
    order[i] = i;
  }

  qsort(order, mod->numFaces, sizeof(int32_t), FaceGroupCmp);

  for (int32_t i = 0; i < mod->numFaces;) {

    int32_t count = 1;
    while (i + count < mod->numFaces && FaceGroupEqual(order[i], order[i + count])) {
      count++;
    }

    const BspFace *first = bspFile.faces + mod->firstFace + order[i];

    if (count > 1 && !(FaceSurface(first) & SURF_MASK_NO_DRAW_ELEMENTS) &&
        EmitHull(mod, order + i, count, &drawFaces[numDrawFaces])) {
      numDrawFaces++;
    } else {
      for (int32_t j = 0; j < count; j++) {
        const BspFace *f = bspFile.faces + mod->firstFace + order[i + j];
        drawFaces[numDrawFaces++] = (DrawFace) {
          .face = f,
          .blockNode = faceGroups[order[i + j]].blockNode,
          .elements = bspFile.elements + f->firstElement,
          .numElements = f->numElements,
          .bounds = f->bounds
        };
      }
    }

    i += count;
  }

  Mem_Free(order);
}

/**
 * @return The `CONTENTS_BLOCK` node that holds the face @p face of the model being emitted, or -1.
 */
int32_t DrawFaceBlockNode(int32_t face) {
  return faceGroups[face].blockNode;
}

/**
 * @brief Frees the draw faces of the model that was emitted.
 */
void FreeDrawFaces(void) {

  Mem_Free(hullElements);
  Mem_Free(drawFaces);
  Mem_Free(faceGroups);

  hullElements = NULL;
  numHullElements = 0;

  drawFaces = NULL;
  numDrawFaces = 0;

  faceGroups = NULL;
  faceGroupsModel = NULL;
}
