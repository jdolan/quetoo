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

#include <SDL3/SDL_atomic.h>

#include "cm_local.h"

static SDL_AtomicInt cWindings;

/**
 * @brief Allocates a winding for the given number of points.
 */
CmWinding *Cm_AllocWinding(int32_t numPoints) {

  SDL_AddAtomicInt(&cWindings, 1);

  return Mem_TagMalloc(sizeof(int32_t) + sizeof(Vec3) * numPoints, MEM_TAG_POLYLIB);
}

/**
 * @brief Frees the given winding.
 */
void Cm_FreeWinding(CmWinding *w) {

  SDL_AddAtomicInt(&cWindings, -1);

  Mem_Free(w);
}

/**
 * @brief Returns a copy of the given winding.
 */
CmWinding *Cm_CopyWinding(const CmWinding *w) {

  CmWinding *c = Cm_AllocWinding(w->numPoints);

  c->numPoints = w->numPoints;

  memcpy(c->points, w->points, c->numPoints * sizeof(Vec3));

  return c;
}

/**
 * @brief Returns a new winding with its points in reverse order.
 */
CmWinding *Cm_ReverseWinding(const CmWinding *w) {

  CmWinding *c = Cm_AllocWinding(w->numPoints);

  for (int32_t i = 0; i < w->numPoints; i++) {
    c->points[i] = w->points[w->numPoints - 1 - i];
  }

  c->numPoints = w->numPoints;
  return c;
}

/**
 * @brief Returns the AABB enclosing all points of the winding.
 */
Box3 Cm_WindingBounds(const CmWinding *w) {
  return Box3_FromPoints(w->points, w->numPoints);
}

/**
 * @brief Returns the centroid of the winding.
 */
Vec3 Cm_WindingCenter(const CmWinding *w) {

  Vec3 center = Vec3_Zero();

  for (int32_t i = 0; i < w->numPoints; i++) {
    center = Vec3_Add(w->points[i], center);
  }

  return Vec3_Scale(center, 1.0 / w->numPoints);
}

/**
 * @brief Returns the surface area of the winding.
 */
float Cm_WindingArea(const CmWinding *w) {
  float area = 0.0;

  for (int32_t i = 2; i < w->numPoints; i++) {
    area += Cm_TriangleArea(w->points[0], w->points[i - 1], w->points[i]);
  }

  return area;
}

/**
 * @brief Calculates the distance from `p` to `w`.
 * @see https://stackoverflow.com/questions/849211/shortest-distance-between-a-point-and-a-line-segment
 */
float Cm_DistanceToWinding(const CmWinding *w, const Vec3 p, Vec3 *dir) {

  float distance = FLT_MAX;

  for (int32_t i = 0; i < w->numPoints; i++) {

    const Vec3 a = w->points[(i + 0) % w->numPoints];
    const Vec3 b = w->points[(i + 1) % w->numPoints];

    const float distSquared = Vec3_DistanceSquared(a, b);
    if (distSquared == 0.f) {
      continue;
    }

    const Vec3 pa = Vec3_Subtract(p, a);
    const Vec3 ba = Vec3_Subtract(b, a);

    const float f = Maxf(0.f, Minf(1.f, Vec3_Dot(pa, ba) / distSquared));
    const Vec3 q = Vec3_Fmaf(a, f, ba);

    Vec3 dir0;
    const float dist = Vec3_DistanceDir(q, p, &dir0);

    if (dist < distance) {
      distance = dist;
      if (dir) {
        *dir = dir0;
      }
    }
  }

  return distance;
}

/**
 * @brief Computes the plane equation for the given winding's points.
 */
void Cm_PlaneForWinding(const CmWinding *w, Vec3 *normal, double *dist) {

  const Vec3d a = Vec3_CastVec3d(w->points[0]);
  const Vec3d b = Vec3_CastVec3d(w->points[1]);
  const Vec3d c = Vec3_CastVec3d(w->points[2]);

  const Vec3d ba = Vec3d_Subtract(b, a);
  const Vec3d ca = Vec3d_Subtract(c, a);

  const Vec3d n = Vec3d_Normalize(Vec3d_Cross(ca, ba));

  *normal = Vec3d_CastVec3(n);
  *dist = Vec3d_Dot(a, n);
}

/**
 * @brief Create a massive polygon for the specified plane.
 */
CmWinding *Cm_WindingForPlane(const Vec3 normal, double dist) {

  const Vec3d norm = Vec3d_Normalize(MakeVec3d(normal.x, normal.y, normal.z));

  // find the major axis
  double max = 0.0;
  int32_t x = -1;
  for (int32_t i = 0; i < 3; i++) {
    const double v = fabs(norm.xyz[i]);
    if (v > max) {
      x = i;
      max = v;
    }
  }
  if (x == -1) {
    Com_Error(ERROR_FATAL, "No axis found\n");
  }

  Vec3d up, right;
  switch (x) {
    case 0:
    case 1:
      right = MakeVec3d(-norm.y, norm.x, 0.0);
      break;
    case 2:
      right = MakeVec3d(0.0, -norm.z, norm.y);
      break;
    default:
      Com_Error(ERROR_FATAL, "Bad axis\n");
  }


  right = Vec3d_Normalize(right);
  up = Vec3d_Normalize(Vec3d_Cross(norm, right));

  const Vec3d r = Vec3d_Scale(right, MAX_WORLD_AXIAL);
  const Vec3d u = Vec3d_Scale(up, MAX_WORLD_AXIAL);

  const Vec3d org = Vec3d_Fma(Vec3d_Zero(), dist, norm);

  // project a really big quad onto the plane
  Vec3d points[4];

  points[0] = Vec3d_Subtract(org, r);
  points[0] = Vec3d_Add(points[0], u);

  points[1] = Vec3d_Add(org, r);
  points[1] = Vec3d_Add(points[1], u);

  points[2] = Vec3d_Add(org, r);
  points[2] = Vec3d_Subtract(points[2], u);

  points[3] = Vec3d_Subtract(org, r);
  points[3] = Vec3d_Subtract(points[3], u);

  CmWinding *w = Cm_AllocWinding(4);

  w->points[0] = Vec3d_CastVec3(points[0]);
  w->points[1] = Vec3d_CastVec3(points[1]);
  w->points[2] = Vec3d_CastVec3(points[2]);
  w->points[3] = Vec3d_CastVec3(points[3]);

  w->numPoints = 4;

  return w;
}

/**
 * @brief Creates a winding for the given face, removing any collinear points.
 */
CmWinding *Cm_WindingForFace(const BspFile *file, const BspFace *face) {

  CmWinding *w = Cm_AllocWinding(face->numVertexes);
  const int32_t v = face->firstVertex;

  for (int32_t i = 0; i < face->numVertexes; i++) {

    const BspVertex *v0 = &file->vertexes[(v + (i + 0) % face->numVertexes)];
    const BspVertex *v1 = &file->vertexes[(v + (i + 1) % face->numVertexes)];
    const BspVertex *v2 = &file->vertexes[(v + (i + 2) % face->numVertexes)];

    w->points[w->numPoints] = v0->position;
    w->numPoints++;

    Vec3 a, b;
    a = Vec3_Subtract(v1->position, v0->position);
    b = Vec3_Subtract(v2->position, v1->position);

    a = Vec3_Normalize(a);
    b = Vec3_Normalize(b);

    if (Vec3_Dot(a, b) > 1.0f - COLINEAR_EPSILON) { // skip v1
      i++;
    }
  }

  return w;
}

/**
 * @brief Creates a winding for the given brush side, clipped to its brush.
 */
CmWinding *Cm_WindingForBrushSide(const BspFile *file, const BspBrushSide *brushSide) {

  const BspPlane *plane = file->planes + brushSide->plane;
  CmWinding *winding = Cm_WindingForPlane(plane->normal, plane->dist);

  const int32_t side = (int32_t) (brushSide - file->brushSides);

  const BspBrush *brush = file->brushes;
  for (int32_t i = 0; i < file->numBrushes; i++, brush++) {

    if (side >= brush->firstBrushSide
      && side < brush->firstBrushSide + brush->numBrushSides) {
      break;
    }
  }

  const BspBrushSide *s = file->brushSides + brush->firstBrushSide;
  for (int32_t i = 0; i < brush->numBrushSides; i++, s++) {
    if (s == brushSide) {
      continue;
    }
    if (s->surface & SURF_BEVEL) {
      continue;
    }
    const BspPlane *p = &file->planes[s->plane ^ 1];
    Cm_ClipWinding(&winding, p->normal, p->dist, SIDE_EPSILON);

    if (winding == NULL) {
      break;
    }
  }

  return winding;
}

/**
 * @brief Removes duplicate adjacent points from the winding, in place.
 * @return False if fewer than three points remain, leaving `w` degenerate.
 */
static bool Cm_CompactWinding(CmWinding *w) {

  for (int32_t i = 0; i < w->numPoints; i++) {
    const Vec3 a = w->points[(i + 0) % w->numPoints];
    const Vec3 b = w->points[(i + 1) % w->numPoints];

    if (Vec3_EqualEpsilon(a, b, FLT_EPSILON)) {

      for (int32_t j = i + 1; j < w->numPoints; j++) {
        w->points[j] = w->points[(j + 1) % w->numPoints];
      }

      w->numPoints--;
    }
  }

  return w->numPoints >= 3;
}

/**
 * @brief Removes duplicate points from the winding, freeing it if fewer than
 * three points remain.
 */
static CmWinding *Cm_FixWinding(CmWinding *w) {

  if (!Cm_CompactWinding(w)) {
    Cm_FreeWinding(w);
    return NULL;
  }

  return w;
}

/**
 * @brief Splits the winding by the given plane into front and back components.
 */
void Cm_SplitWinding(const CmWinding *in, const Vec3 normal, double dist, double epsilon,
            CmWinding **front, CmWinding **back) {

  assert(in->numPoints);
  const int32_t maxPoints = in->numPoints + 4;

  CmClipPoint clipPoints[maxPoints];
  memset(&clipPoints, 0, maxPoints * sizeof(CmClipPoint));

  int32_t sideFront = 0, sideBack = 0;

  CmClipPoint *c = clipPoints;
  for (int32_t i = 0; i < in->numPoints; i++, c++) {
    c->point = in->points[i];
    c->dist = (double) Vec3_Dot(c->point, normal) - dist;
    if (c->dist > epsilon) {
      c->side = SIDE_FRONT;
      sideFront++;
    } else if (c->dist < -epsilon) {
      c->side = SIDE_BACK;
      sideBack++;
    } else {
      c->side = SIDE_ON;
    }
  }

  if (sideFront == 0) {
    *front = NULL;
    *back = Cm_CopyWinding(in);
    return;
  }

  if (sideBack == 0) {
    *front = Cm_CopyWinding(in);
    *back = NULL;
    return;
  }

  CmWinding *f = Cm_AllocWinding(maxPoints);
  CmWinding *b = Cm_AllocWinding(maxPoints);

  for (int32_t i = 0; i < in->numPoints; i++) {
    const CmClipPoint *c = clipPoints + i;

    if (c->side == SIDE_ON) {
      f->points[f->numPoints] = c->point;
      f->numPoints++;

      b->points[b->numPoints] = c->point;
      b->numPoints++;

      continue;
    }

    if (c->side == SIDE_FRONT) {
      f->points[f->numPoints] = c->point;
      f->numPoints++;
    }

    if (c->side == SIDE_BACK) {
      b->points[b->numPoints] = c->point;
      b->numPoints++;
    }

    const CmClipPoint *d = clipPoints + ((i + 1) % in->numPoints);

    if (d->side == SIDE_ON || d->side == c->side) {
      continue;
    }

    const double denom = c->dist - d->dist;
    if (fabs(denom) < 1e-10) {
      continue; // Points too close, skip interpolation
    }

    Vec3d mid = Vec3d_Zero();
    const double dot = c->dist / denom;
    for (int32_t j = 0; j < 3; j++) { // avoid round off error when possible
      if (normal.xyz[j] > 1.f - FLT_EPSILON) {
        mid.xyz[j] = dist;
      } else if (normal.xyz[j] < -1.f + FLT_EPSILON) {
        mid.xyz[j] = -dist;
      } else {
        mid.xyz[j] = (double) c->point.xyz[j] + dot * (double) (d->point.xyz[j] - c->point.xyz[j]);
      }
    }

    f->points[f->numPoints] = Vec3d_CastVec3(mid);
    f->numPoints++;

    b->points[b->numPoints] = Vec3d_CastVec3(mid);
    b->numPoints++;

    if (f->numPoints == maxPoints || b->numPoints == maxPoints) {
      Com_Error(ERROR_FATAL, "Points exceeded estimate\n");
    }
  }

  *front = Cm_FixWinding(f);
  *back = Cm_FixWinding(b);
}

/**
 * @brief Classifies each point of the winding against the given plane.
 * @param clip_points Receives one entry per point of `in`.
 */
static void Cm_ClassifyWindingPoints(const CmWinding *in, const Vec3 normal, double dist,
                                     double epsilon, CmClipPoint *clipPoints,
                                     int32_t *sideFront, int32_t *sideBack) {

  *sideFront = *sideBack = 0;

  CmClipPoint *c = clipPoints;
  for (int32_t i = 0; i < in->numPoints; i++, c++) {
    c->point = in->points[i];
    c->dist = (double) Vec3_Dot(c->point, normal) - dist;
    if (c->dist > epsilon) {
      c->side = SIDE_FRONT;
      (*sideFront)++;
    } else if (c->dist < -epsilon) {
      c->side = SIDE_BACK;
      (*sideBack)++;
    } else {
      c->side = SIDE_BOTH;
    }
  }
}

/**
 * @brief Emits the front-side portion of `in` into `out`.
 * @param capacity The number of points `out` can hold.
 * @remarks Neither winding is allocated or freed, and they MUST NOT alias.
 */
static void Cm_EmitClippedWinding(const CmWinding *in, const CmClipPoint *clipPoints,
                                  const Vec3 normal, double dist, CmWinding *out,
                                  int32_t capacity) {

  out->numPoints = 0;

  for (int32_t i = 0; i < in->numPoints; i++) {
    const CmClipPoint *c = clipPoints + i;

    if (c->side == SIDE_BOTH) {
      out->points[out->numPoints] = c->point;
      out->numPoints++;
      continue;
    }

    if (c->side == SIDE_FRONT) {
      out->points[out->numPoints] = c->point;
      out->numPoints++;
    }

    const CmClipPoint *d = clipPoints + ((i + 1) % in->numPoints);

    if (d->side == SIDE_BOTH || d->side == c->side) {
      continue;
    }

    const double denom = c->dist - d->dist;
    if (fabs(denom) < 1e-10) {
      continue; // Points too close, skip interpolation
    }

    Vec3d mid = Vec3d_Zero();
    const double dot = c->dist / denom;
    for (int32_t j = 0; j < 3; j++) { // avoid round off error when possible
      if (normal.xyz[j] > 1.f - FLT_EPSILON) {
        mid.xyz[j] = dist;
      } else if (normal.xyz[j] < -1.f + FLT_EPSILON) {
        mid.xyz[j] = -dist;
      } else {
        mid.xyz[j] = (double) c->point.xyz[j] + dot * (double) (d->point.xyz[j] - c->point.xyz[j]);
      }
    }

    out->points[out->numPoints] = Vec3d_CastVec3(mid);
    out->numPoints++;

    if (out->numPoints == capacity) {
      Com_Error(ERROR_FATAL, "Points exceeded estimate\n");
    }
  }
}

/**
 * @brief Clips the winding against the given plane.
 */
void Cm_ClipWinding(CmWinding **inOut, const Vec3 normal, double dist, double epsilon) {

  CmWinding *in = *inOut;

  assert(in->numPoints);
  const int32_t maxPoints = in->numPoints + 4;

  CmClipPoint clipPoints[maxPoints];
  memset(clipPoints, 0, maxPoints * sizeof(CmClipPoint));

  int32_t sideFront, sideBack;
  Cm_ClassifyWindingPoints(in, normal, dist, epsilon, clipPoints, &sideFront, &sideBack);

  if (sideFront == 0) {
    Cm_FreeWinding(in);
    *inOut = NULL;
    return;
  }

  if (sideBack == 0) {
    return;
  }

  CmWinding *out = Cm_AllocWinding(maxPoints);

  Cm_EmitClippedWinding(in, clipPoints, normal, dist, out, maxPoints);

  Cm_FreeWinding(in);
  *inOut = Cm_FixWinding(out);
}

/**
 * @brief Clips a winding against all edges of another winding using Sutherland-Hodgman algorithm.
 * @param in The winding to be clipped.
 * @param clip The winding whose edges define the clipping region.
 * @param normal The shared plane normal (must match for both windings).
 * @param epsilon The epsilon for plane distance tests.
 * @return The clipped winding, or `NULL` if fully clipped away.
 * @remarks The input winding is NOT freed. The returned winding must be freed by caller.
 */
CmWinding *Cm_ClipWindingToWinding(const CmWinding *in, const CmWinding *clip, const Vec3 normal, double epsilon) {

  assert(in);
  assert(clip);
  assert(in->numPoints >= 3);
  assert(clip->numPoints >= 3);
  
  CmWinding *current = Cm_CopyWinding(in);
  
  // Clip against each edge of the clipping winding
  for (int32_t edge = 0; edge < clip->numPoints && current != NULL; edge++) {
    
    const Vec3 edgeStart = clip->points[edge];
    const Vec3 edgeEnd = clip->points[(edge + 1) % clip->numPoints];
    
    // Build edge plane (perpendicular to edge, in the winding plane)
    const Vec3 edgeDir = Vec3_Normalize(Vec3_Subtract(edgeEnd, edgeStart));
    const Vec3 edgeNormal = Vec3_Cross(edgeDir, normal);
    const double edgeDist = Vec3_Dot(edgeNormal, edgeStart);
    
    // Clip against this edge plane (keep front side)
    Cm_ClipWinding(&current, edgeNormal, edgeDist, epsilon);
  }

  return current;
}

/**
 * @brief Clips a winding against all edges of another winding, alternating
 * between the caller-supplied scratch windings `a` and `b`.
 * @param in The winding to be clipped, which is never modified.
 * @param clip The winding whose edges define the clipping region.
 * @param normal The shared plane normal (must match for both windings).
 * @param epsilon The epsilon for plane distance tests.
 * @param a Scratch winding with capacity for `capacity` points.
 * @param b Scratch winding with capacity for `capacity` points.
 * @param capacity The number of points `a` and `b` can each hold, which MUST be
 * at least `in->num_points + 4 * clip->num_points`.
 * @return `in` if no edge clipped it, otherwise `a` or `b`, or `NULL` if it was
 * clipped away entirely.
 * @remarks Nothing is allocated or freed. Prefer this over
 * `Cm_ClipWindingToWinding` on hot paths, and note the result is only valid
 * until the next call reusing the same scratch windings.
 */
const CmWinding *Cm_ClipWindingToWindingInto(const CmWinding *in, const CmWinding *clip,
                                                const Vec3 normal, double epsilon,
                                                CmWinding *a, CmWinding *b,
                                                int32_t capacity) {

  assert(in);
  assert(clip);
  assert(in->numPoints >= 3);
  assert(clip->numPoints >= 3);
  assert(a);
  assert(b);
  assert(capacity >= in->numPoints + 4 * clip->numPoints);

  const CmWinding *current = in;
  CmWinding *spare = a;

  for (int32_t edge = 0; edge < clip->numPoints; edge++) {

    const Vec3 edgeStart = clip->points[edge];
    const Vec3 edgeEnd = clip->points[(edge + 1) % clip->numPoints];

    const Vec3 edgeDir = Vec3_Normalize(Vec3_Subtract(edgeEnd, edgeStart));
    const Vec3 edgeNormal = Vec3_Cross(edgeDir, normal);
    const double edgeDist = Vec3_Dot(edgeNormal, edgeStart);

    CmClipPoint clipPoints[current->numPoints];
    memset(clipPoints, 0, current->numPoints * sizeof(CmClipPoint));

    int32_t sideFront, sideBack;
    Cm_ClassifyWindingPoints(current, edgeNormal, edgeDist, epsilon, clipPoints,
                             &sideFront, &sideBack);

    if (sideFront == 0) {
      return NULL;
    }

    if (sideBack == 0) {
      continue;
    }

    Cm_EmitClippedWinding(current, clipPoints, edgeNormal, edgeDist, spare, capacity);

    if (!Cm_CompactWinding(spare)) {
      return NULL;
    }

    current = spare;
    spare = (spare == a) ? b : a;
  }

  return current;
}

/**
 * @brief If two polygons share a common edge and the edges that meet at the
 * common points are both inside the other polygons, merge them
 *
 * Returns `NULL` if the faces couldn't be merged, or the new face.
 * The originals will NOT be freed.
 */
CmWinding *Cm_MergeWindings(const CmWinding *a, const CmWinding *b, const Vec3 normal) {
  Vec3 p1, p2, back;
  int32_t i, j, k, l;
  Vec3 cross, delta;
  float dot;

  // find a common edge
  p1 = p2 = Vec3_Zero();
  j = 0;

  for (i = 0; i < a->numPoints; i++) {
    p1 = a->points[i];
    p2 = a->points[(i + 1) % a->numPoints];
    for (j = 0; j < b->numPoints; j++) {
      Vec3 p3 = b->points[j];
      Vec3 p4 = b->points[(j + 1) % b->numPoints];
      for (k = 0; k < 3; k++) {
        if (fabsf(p1.xyz[k] - p4.xyz[k]) > ON_EPSILON) {
          break;
        }
        if (fabsf(p2.xyz[k] - p3.xyz[k]) > ON_EPSILON) {
          break;
        }
      }
      if (k == 3) {
        break;
      }
    }
    if (j < b->numPoints) {
      break;
    }
  }

  if (i == a->numPoints) {
    return NULL; // no matching edges
  }

  // if the slopes are colinear, the point can be removed
  back = a->points[(i + a->numPoints - 1) % a->numPoints];
  delta = Vec3_Subtract(p1, back);
  cross = Vec3_Cross(normal, delta);
  cross = Vec3_Normalize(cross);

  back = b->points[(j + 2) % b->numPoints];
  delta = Vec3_Subtract(back, p1);
  dot = Vec3_Dot(delta, cross);
  if (dot > COLINEAR_EPSILON) {
    return NULL; // not a convex polygon
  }
  const bool keep1 = dot < -COLINEAR_EPSILON;

  back = a->points[(i + 2) % a->numPoints];
  delta = Vec3_Subtract(back, p2);
  cross = Vec3_Cross(normal, delta);
  cross = Vec3_Normalize(cross);

  back = b->points[(j + b->numPoints - 1) % b->numPoints];
  delta = Vec3_Subtract(back, p2);
  dot = Vec3_Dot(delta, cross);
  if (dot > COLINEAR_EPSILON) {
    return NULL; // not a convex polygon
  }
  const bool keep2 = dot < -COLINEAR_EPSILON;

  // build the new polygon
  CmWinding *merged = Cm_AllocWinding(a->numPoints + b->numPoints);

  // copy first polygon
  for (k = (i + 1) % a->numPoints; k != i; k = (k + 1) % a->numPoints) {
    if (k == (i + 1) % a->numPoints && !keep2) {
      continue;
    }

    merged->points[merged->numPoints] = a->points[k];
    merged->numPoints++;
  }

  // copy second polygon
  for (l = (j + 1) % b->numPoints; l != j; l = (l + 1) % b->numPoints) {
    if (l == (j + 1) % b->numPoints && !keep1) {
      continue;
    }
    merged->points[merged->numPoints] = b->points[l];
    merged->numPoints++;
  }

  return Cm_FixWinding(merged);
}

/**
 * @brief Creates a vertex element array of triangles for the given winding.
 * @details This function uses an ear-clipping algorithm to clip triangles from
 * the given winding. Invalid triangles due to colinear points are skipped over.
 * @param w The winding.
 * @param elements The output array, which must be `>= (w->num_points - 2) * 3` in length.
 * @return The number of vertex elements written to tris.
 */
int32_t Cm_ElementsForWinding(const CmWinding *w, int32_t *elements) {

  int32_t *out = elements;

  typedef struct {
    Vec3 position;
    int32_t index;
    int32_t corner;
  } Point;

  int32_t numPoints = w->numPoints;
  Point points[numPoints];

  for (int32_t i = 0; i < numPoints; i++) {
    points[i].position = w->points[i];
    points[i].index = i;
  }

  while (numPoints > 2) {

    // find the corners, or points without collinear neighbors

    int32_t numCorners = 0;
    for (int32_t i = 0; i < numPoints; i++) {

      Point *a = &points[(i + 0) % numPoints];
      Point *b = &points[(i + 1) % numPoints];
      Point *c = &points[(i + 2) % numPoints];

      const Vec3 ba = Vec3_Direction(b->position, a->position);
      const Vec3 cb = Vec3_Direction(c->position, b->position);

      const float dot = Vec3_Dot(ba, cb);
      if (dot > 1.f - COLINEAR_EPSILON) {
        b->corner = 0;
      } else {
        b->corner = ++numCorners;
      }
    }

    // if we don't find 3 corners, this is a degenerate winding (a line segment)

    if (numCorners < 3) {
      Com_Warn("Invalid winding: %d corners found in %d points\n", numCorners, numPoints);
      break;
    }

    // chip away at edges with colinear points first

    const Point *clip = NULL;
    if (numCorners < numPoints) {
      float best = FLT_MAX;

      for (int32_t i = 0; i < numPoints; i++) {
        const Point *a = &points[(i + 0) % numPoints];
        const Point *b = &points[(i + 1) % numPoints];
        const Point *c = &points[(i + 2) % numPoints];

        if (!a->corner && b->corner) {
          const float area = Cm_TriangleArea(a->position, b->position, c->position);
          if (area < best) {
            best = area;
            clip = b;
          }
        }
      }
      assert(clip);
    } else {
      clip = points + 1;
    }

    const int32_t i = (int32_t) (ptrdiff_t) (clip - points);
    const int32_t j = (i - 1 + numPoints) % numPoints;

    for (int32_t k = 0; k < 3; k++) {
      *out++ = points[(j + k) % numPoints].index;
    }

    for (int32_t k = i; k < numPoints - 1; k++) {
      points[k] = points[k + 1];
    }

    numPoints--;
  }

  return (int32_t) (ptrdiff_t) (out - elements);
}

/**
* @return The area of the triangle defined by a, b and c.
*/
float Cm_TriangleArea(const Vec3 a, const Vec3 b, const Vec3 c) {

   const Vec3 ba = Vec3_Subtract(b, a);
   const Vec3 ca = Vec3_Subtract(c, a);
   const Vec3 cross = Vec3_Cross(ba, ca);

   return Vec3_Length(cross) * 0.5f;
}

/**
* @brief Calculates barycentric coordinates for p in the triangle defined by a, b and c.
* @remarks The `max_area` checks ensure that p is (approximately) inside the triangle abc.
* @see https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/barycentric-coordinates
*/
float Cm_Barycentric(const Vec3 a, const Vec3 b, const Vec3 c, const Vec3 p, Vec3 *out) {

  const float abc = Cm_TriangleArea(a, b, c);
  if (abc) {
    const float maxArea = abc * 1.f;

    const float bcp = Cm_TriangleArea(b, c, p);
    if (bcp > maxArea) {
      return FLT_MAX;
    }

    const float cap = Cm_TriangleArea(c, a, p);
    if (cap > maxArea) {
      return FLT_MAX;
    }

    const float abp = Cm_TriangleArea(a, b, p);
    if (abp > maxArea) {
      return FLT_MAX;
    }

    out->x = bcp / abc;
    out->y = cap / abc;
    out->z = abp / abc;

    return out->x + out->y + out->z;
  } else {
     *out = Vec3_Zero();
  }

  return FLT_MAX;
}

/**
 * @brief Calculates the tangent vectors for the given vertexes and triangle elements.
 * @see http://foundationsofgameenginedev.com/FGED2-sample.pdf
 */
void Cm_Tangents(CmVertex *vertexes, int32_t baseVertex, int32_t numVertexes, const int32_t *elements, int32_t numElements) {

  for (int32_t i = 0; i < numElements; i += 3) {

    const int32_t i0 = *(elements + i + 0) - baseVertex;
    const int32_t i1 = *(elements + i + 1) - baseVertex;
    const int32_t i2 = *(elements + i + 2) - baseVertex;

    CmVertex *v0 = vertexes + i0;
    CmVertex *v1 = vertexes + i1;
    CmVertex *v2 = vertexes + i2;

    const Vec3 e1 = Vec3_Subtract(*v1->position, *v0->position);
    const Vec3 e2 = Vec3_Subtract(*v2->position, *v0->position);

    const double x1 = v1->st->x - v0->st->x;
    const double x2 = v2->st->x - v0->st->x;

    const double y1 = v1->st->y - v0->st->y;
    const double y2 = v2->st->y - v0->st->y;

    const double r = 1.f / (x1 * y2 - x2 * y1);

    if (r == INFINITY || r == -INFINITY) {
      continue;
    }

    const Vec3 t = Vec3_Scale(Vec3_Subtract(Vec3_Scale(e1, y2), Vec3_Scale(e2, y1)), r);
    const Vec3 b = Vec3_Scale(Vec3_Subtract(Vec3_Scale(e2, x1), Vec3_Scale(e1, x2)), r);

    *v0->tangent = Vec3_Add(*v0->tangent, t);
    *v1->tangent = Vec3_Add(*v1->tangent, t);
    *v2->tangent = Vec3_Add(*v2->tangent, t);

    *v0->bitangent = Vec3_Add(*v0->bitangent, b);
    *v1->bitangent = Vec3_Add(*v1->bitangent, b);
    *v2->bitangent = Vec3_Add(*v2->bitangent, b);

    v0->numTris++;
    v1->numTris++;
    v2->numTris++;
  }

  CmVertex *v = vertexes;
  for (int32_t i = 0; i < numVertexes; i++, v++) {

    const Vec3 sdir = *v->tangent;
    const Vec3 tdir = *v->bitangent;

    Vec3_Tangents(*v->normal, sdir, tdir, v->tangent, v->bitangent);
  }
}

/**
 * @brief Clips an AABB to the positive half-space of the given plane.
 */
Box3 Cm_ClipBox(const Box3 in, const Vec4 plane) {
  Box3 out = Box3_Null();

  Vec3 corners[8];
  Box3_ToPoints(in, corners);

  // There are 8 corners in the AABB
  for (size_t i = 0; i < lengthof(corners); i++) {
    const Vec3 corner = corners[i];

    // If the corner is on the positive side of the plane, include it
    const float dist = Vec3_Dot(plane.xyz, corner) - plane.w;
    if (dist >= 0.f) {
      out.mins = Vec3_Minf(out.mins, corner);
      out.maxs = Vec3_Maxf(out.maxs, corner);
    } else {
      // Otherwise, project the corner onto the plane and include it
      const Vec3 point = Vec3_Subtract(corner, Vec3_Scale(plane.xyz, dist));
      out.mins = Vec3_Minf(out.mins, point);
      out.maxs = Vec3_Maxf(out.maxs, point);
    }
  }

  return out;
}
