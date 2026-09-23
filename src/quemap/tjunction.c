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
static CmWinding **windings;

/**
 * @brief Processes a single face, inserting vertices from all other coplanar faces that lie on its edges to eliminate T-junctions.
 * @details Other faces are read from `windings`, their state before any were fixed, and only this
 * face is written, so that the result does not depend on the order in which faces are processed.
 */
static void FixTJunctions_(int32_t faceNum) {

  Face *face = VectorValue(faces, Face *, faceNum);

  const Plane *plane = &planes[face->brushSide->plane];

  for (size_t s = 0; s < faces->count; s++) {

    if (s == (size_t) faceNum) {
      continue;
    }

    const CmWinding *f_winding = windings[s];

    for (int32_t i = 0; i < f_winding->numPoints; i++) {
      const Vec3 v = f_winding->points[i];

      const double d = Vec3_Dot(v, plane->normal) - plane->dist;
      if (d > ON_EPSILON || d < -ON_EPSILON) {
        continue; // v is not on face's plane
      }

      // v is on face's plane, so test it against face's edges

      const CmWinding *face_winding = face->w;

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

        Cm_FreeWinding(face->w);
        face->w = w;

        SDL_AddAtomicInt(&cTjunctions, 1);
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

  windings = Mem_Malloc(sizeof(CmWinding *) * faces->count);
  for (size_t i = 0; i < faces->count; i++) {
    const Face *face = VectorValue(faces, Face *, i);
    windings[i] = Cm_CopyWinding(face->w);
  }

  Work("Fixing t-junctions", FixTJunctions_, (int32_t) faces->count);

  Com_Verbose("%5i fixed tjunctions\n", SDL_GetAtomicInt(&cTjunctions));

  for (size_t i = 0; i < faces->count; i++) {
    Cm_FreeWinding(windings[i]);
  }
  Mem_Free(windings);

  release(faces);
}
