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

#include <Objectively/HashTable.h>
#include <Objectively/Vector.h>

#include "tjunction.h"
#include "portal.h"
#include "qbsp.h"

static SDL_AtomicInt cTjunctions;
static Vector *faces;
static HashTable *facesSet;
static Winding **windings;

/**
 * @brief Processes a single face, inserting vertices from all other coplanar faces that lie on its edges to eliminate T-junctions.
 * @details Other faces are read from `windings`, their state before any were fixed, and only this
 * face is written, so that the result does not depend on the order in which faces are processed.
 */
static void FixTJunctions_(int32_t faceNum) {

  Face *face = VectorValue(faces, Face *, faceNum);

  const MapPlane *plane = &planes[face->brushSide->plane];

  for (size_t s = 0; s < faces->count; s++) {

    if (s == (size_t) faceNum) {
      continue;
    }

    const Winding *winding = windings[s];

    for (int32_t i = 0; i < winding->numPoints; i++) {
      const Vec3 v = winding->points[i];

      const double d = Vec3_Dot(v, plane->normal) - plane->dist;
      if (d > ON_EPSILON || d < -ON_EPSILON) {
        continue; // v is not on face's plane
      }

      // v is on face's plane, so test it against face's edges

      const Winding *faceWinding = face->w;

      for (int32_t j = 0; j < faceWinding->numPoints; j++) {

        const Vec3 v0 = faceWinding->points[(j + 0) % faceWinding->numPoints];
        const Vec3 v1 = faceWinding->points[(j + 1) % faceWinding->numPoints];

        Vec3 a;
        const float aDist = Vec3_DistanceDir(v0, v, &a);

        Vec3 b;
        const float bDist = Vec3_DistanceDir(v1, v, &b);

        if (aDist < ON_EPSILON || bDist < ON_EPSILON) {
          break; // face already includes v
        }

        const float d = Vec3_Dot(a, b);
        if (d > -1.0 + COLINEAR_EPSILON) {
          continue; // v is not on the edge v0 <-> v1
        }

        // v sits between v0 and v1, so add it to the face
        Winding *w = Winding_Alloc(faceWinding->numPoints + 1);
        w->numPoints = faceWinding->numPoints + 1;

        for (int32_t k = 0; k < w->numPoints; k++) {
          if (k <= j) {
            w->points[k] = faceWinding->points[k];
          } else if (k == j + 1) {
            w->points[k] = v;
          } else {
            w->points[k] = faceWinding->points[k - 1];
          }
        }

        Winding_Free(face->w);
        face->w = w;

        SDL_AddAtomicInt(&cTjunctions, 1);
        break;
      }
    }
  }
}

/**
 * @brief The distance within which face vertexes are one vertex.
 */
#define WELD_EPSILON .025f

typedef struct {
  Vec3 position;
  int32_t face; // the index of the face in `faces`
  int32_t point; // the index of the point in its winding
  float lattice; // the squared distance to the nearest integer grid point
  int32_t cluster; // the representative this point is welded to, or -1
} WeldPoint;

static WeldPoint *weldPoints;

static int32_t WeldPointXComparator(const void *a, const void *b) {
  const float x = weldPoints[*(const int32_t *) a].position.x;
  const float y = weldPoints[*(const int32_t *) b].position.x;
  return x < y ? -1 : x > y ? 1 : 0;
}

static int32_t WeldPointLatticeComparator(const void *a, const void *b) {
  const WeldPoint *p = weldPoints + *(const int32_t *) a;
  const WeldPoint *q = weldPoints + *(const int32_t *) b;

  if (p->lattice != q->lattice) {
    return p->lattice < q->lattice ? -1 : 1;
  }
  for (int32_t i = 0; i < 3; i++) {
    if (p->position.xyz[i] != q->position.xyz[i]) {
      return p->position.xyz[i] < q->position.xyz[i] ? -1 : 1;
    }
  }
  return (int32_t) (p - q);
}

/**
 * @brief Snaps the vertexes of all faces that are within `WELD_EPSILON` of each other to one
 * position, so that neighboring faces share their edges bit for bit, even where CSG has left
 * them a hair apart. The point closest to the integer grid is the one that the others move to, so
 * that a map built on the grid stays on it. A face never has two of its own points welded
 * together, which would collapse its winding.
 */
static void WeldFaces(void) {

  int32_t numPoints = 0;
  for (size_t i = 0; i < faces->count; i++) {
    const Face *face = VectorValue(faces, Face *, i);
    numPoints += face->w->numPoints;
  }

  weldPoints = Mem_Malloc(numPoints * sizeof(WeldPoint));

  numPoints = 0;
  for (size_t i = 0; i < faces->count; i++) {
    const Face *face = VectorValue(faces, Face *, i);
    const Winding *w = face->w;
    for (int32_t j = 0; j < w->numPoints; j++) {

      const Vec3 p = w->points[j];
      const Vec3 r = Vec3_Subtract(p, MakeVec3(roundf(p.x), roundf(p.y), roundf(p.z)));

      weldPoints[numPoints++] = (WeldPoint) {
        .position = p,
        .face = (int32_t) i,
        .point = j,
        .lattice = Vec3_Dot(r, r),
        .cluster = -1
      };
    }
  }

  // by x, so that the points near one are found by a binary search and a short scan
  int32_t *byX = Mem_Malloc(numPoints * sizeof(int32_t));
  int32_t *byLattice = Mem_Malloc(numPoints * sizeof(int32_t));
  for (int32_t i = 0; i < numPoints; i++) {
    byX[i] = byLattice[i] = i;
  }

  qsort(byX, numPoints, sizeof(int32_t), WeldPointXComparator);
  qsort(byLattice, numPoints, sizeof(int32_t), WeldPointLatticeComparator);

  for (int32_t i = 0; i < numPoints; i++) {

    WeldPoint *rep = weldPoints + byLattice[i];
    if (rep->cluster != -1) {
      continue;
    }

    rep->cluster = byLattice[i];

    int32_t clusterFaces[64], numClusterFaces = 0;
    clusterFaces[numClusterFaces++] = rep->face;

    int32_t lo = 0, hi = numPoints;
    while (lo < hi) {
      const int32_t mid = (lo + hi) / 2;
      if (weldPoints[byX[mid]].position.x < rep->position.x - WELD_EPSILON) {
        lo = mid + 1;
      } else {
        hi = mid;
      }
    }

    for (int32_t j = lo; j < numPoints; j++) {

      WeldPoint *p = weldPoints + byX[j];
      if (p->position.x > rep->position.x + WELD_EPSILON) {
        break;
      }

      if (p->cluster != -1) {
        continue;
      }

      if (Vec3_DistanceSquared(p->position, rep->position) > WELD_EPSILON * WELD_EPSILON) {
        continue;
      }

      int32_t k;
      for (k = 0; k < numClusterFaces; k++) {
        if (clusterFaces[k] == p->face) {
          break;
        }
      }
      if (k < numClusterFaces || numClusterFaces == lengthof(clusterFaces)) {
        continue;
      }

      clusterFaces[numClusterFaces++] = p->face;
      p->cluster = byLattice[i];
    }
  }

  int32_t numWelded = 0;
  for (int32_t i = 0; i < numPoints; i++) {

    const WeldPoint *p = weldPoints + i;
    const WeldPoint *rep = weldPoints + p->cluster;

    if (rep != p) {
      const Face *face = VectorValue(faces, Face *, p->face);
      face->w->points[p->point] = rep->position;
      numWelded++;
    }
  }

  Com_Verbose("%5i welded vertexes\n", numWelded);

  Mem_Free(byX);
  Mem_Free(byLattice);
  Mem_Free(weldPoints);
}

/**
 * @brief Recursively traverses the tree and collects all unmerged faces into the faces array.
 */
static void FixTJunctions_r(Node *node) {

  if (node->plane != PLANE_LEAF) {
    FixTJunctions_r(node->children[0]);
    FixTJunctions_r(node->children[1]);
  }

  for (Face *face = node->faces; face; face = face->next) {

    if (face->merged) {
      continue;
    }
    
    if ($(facesSet, get, face) != NULL) {
      continue;
    }
    
    $(faces, add, &face);
    $(facesSet, set, face, face);
  }
}

/**
 * @brief Fixes all T-junctions in the tree by inserting missing vertices into face windings along shared edges.
 */
void FixTJunctions(Tree *tree) {

  Com_Verbose("--- FixTJunctions ---\n");
  SDL_SetAtomicInt(&cTjunctions, 0);

  faces = $(alloc(Vector), initWithSize, sizeof(Face *));
  facesSet = $(alloc(HashTable), init, HashTableHashDirect, HashTableEqualDirect);
  FixTJunctions_r(tree->headNode);
  facesSet = release(facesSet);

  WeldFaces();

  windings = Mem_Malloc(sizeof(Winding *) * faces->count);
  for (size_t i = 0; i < faces->count; i++) {
    const Face *face = VectorValue(faces, Face *, i);
    windings[i] = Winding_Copy(face->w);
  }

  Work("Fixing t-junctions", FixTJunctions_, (int32_t) faces->count);

  Com_Verbose("%5i fixed tjunctions\n", SDL_GetAtomicInt(&cTjunctions));

  for (size_t i = 0; i < faces->count; i++) {
    Winding_Free(windings[i]);
  }
  Mem_Free(windings);

  release(faces);
}
