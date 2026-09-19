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

#include <Objectively/HashTable.h>
#include <Objectively/Vector.h>

#include "tjunction.h"
#include "portal.h"
#include "qbsp.h"

static SDL_AtomicInt c_tjunctions;
static Vector *faces;
static HashTable *faces_set;
static SDL_SpinLock *faces_locks;
static int32_t largest_winding = 0;

/**
 * @brief Processes a single face, inserting vertices from all other coplanar faces that lie on its edges to eliminate T-junctions.
 */
static void FixTJunctions_(int32_t faceNum) {
  static _Thread_local CmWinding *face_winding, *f_winding;

  if (!face_winding) {
    face_winding = Cm_AllocWinding(largest_winding);
    f_winding = Cm_AllocWinding(largest_winding);
  }

  Face *face = VectorValue(faces, Face *, faceNum);

  SDL_SpinLock *faceLock = &faces_locks[faceNum];

  const Plane *plane = &planes[face->brushSide->plane];

  // Make a copy of face->w for testing
  SDL_LockSpinlock(faceLock);
  memcpy(face_winding, face->w, sizeof(CmWinding) + (face->w->numPoints * sizeof(Vec3)));
  SDL_UnlockSpinlock(faceLock);

  for (size_t s = 0; s < faces->count; s++) {

    const Face *f = VectorValue(faces, Face *, s);
    if (face == f) {
      continue;
    }
    
    SDL_SpinLock *fLock = &faces_locks[s];

    SDL_LockSpinlock(fLock);
    memcpy(f_winding, f->w, sizeof(CmWinding) + (f->w->numPoints * sizeof(Vec3)));
    SDL_UnlockSpinlock(fLock);

    for (int32_t i = 0; i < f_winding->numPoints; i++) {
      const Vec3 v = f_winding->points[i];

      const double d = Vec3_Dot(v, plane->normal) - plane->dist;
      if (d > ON_EPSILON || d < -ON_EPSILON) {
        continue; // v is not on face's plane
      }

      // v is on face's plane, so test it against face's edges

      for (int32_t j = 0; j < face_winding->numPoints; j++) {

        const Vec3 v0 = face_winding->points[(j + 0) % face_winding->numPoints];
        const Vec3 v1 = face_winding->points[(j + 1) % face_winding->numPoints];

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
        CmWinding *w = Cm_AllocWinding(face_winding->numPoints + 1);
        w->numPoints = face_winding->numPoints + 1;

        for (int32_t k = 0; k < w->numPoints; k++) {
          if (k <= j) {
            w->points[k] = face_winding->points[k];
          } else if (k == j + 1) {
            w->points[k] = v;
          } else {
            w->points[k] = face_winding->points[k - 1];
          }
        }

        SDL_LockSpinlock(faceLock);

        // Copy back to face, and copy to temp winding
        Cm_FreeWinding(face->w);
        face->w = w;
        memcpy(face_winding, face->w, sizeof(CmWinding) + (face->w->numPoints * sizeof(Vec3)));

        SDL_UnlockSpinlock(faceLock);

        SDL_AddAtomicInt(&c_tjunctions, 1);
        break;
      }
    }
  }
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
    
    if ($(faces_set, get, face) != NULL) {
      continue;
    }
    
    $(faces, add, &face);
    $(faces_set, set, face, face);

    largest_winding = Maxi(largest_winding, face->w->numPoints);
  }
}

/**
 * @brief Fixes all T-junctions in the tree by inserting missing vertices into face windings along shared edges.
 */
void FixTJunctions(Tree *tree) {

  Com_Verbose("--- FixTJunctions ---\n");
  SDL_SetAtomicInt(&c_tjunctions, 0);

  faces = $(alloc(Vector), initWithSize, sizeof(Face *));
  faces_set = $(alloc(HashTable), init, HashTableHashDirect, HashTableEqualDirect);
  FixTJunctions_r(tree->headNode);
  faces_set = release(faces_set);

  const int32_t largestPointCount = largest_winding;
  largest_winding = sizeof(CmWinding) + (sizeof(Vec3) * largestPointCount);

  faces_locks = Mem_Malloc(sizeof(SDL_SpinLock) * faces->count);

  Work("Fixing t-junctions", FixTJunctions_, (int32_t) faces->count);

  Com_Verbose("%5i fixed tjunctions\n", SDL_GetAtomicInt(&c_tjunctions));

  Mem_Free(faces_locks);
  release(faces);
}
