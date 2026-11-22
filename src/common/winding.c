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

#include <SDL3/SDL_atomic.h>

#include "common.h"
#include "winding.h"

static SDL_AtomicInt cWindings;

/**
 * @brief Allocates a winding for the given number of points.
 */
Winding *Winding_Alloc(int32_t numPoints) {

  SDL_AddAtomicInt(&cWindings, 1);

  return Mem_TagMalloc(sizeof(int32_t) + sizeof(Vec3) * numPoints, MEM_TAG_POLYLIB);
}

/**
 * @brief Frees the given winding.
 */
void Winding_Free(Winding *w) {

  SDL_AddAtomicInt(&cWindings, -1);

  Mem_Free(w);
}

/**
 * @brief Returns a copy of the given winding.
 */
Winding *Winding_Copy(const Winding *w) {

  Winding *c = Winding_Alloc(w->numPoints);

  c->numPoints = w->numPoints;

  memcpy(c->points, w->points, c->numPoints * sizeof(Vec3));

  return c;
}

/**
 * @brief Returns a new winding with its points in reverse order.
 */
Winding *Winding_Reverse(const Winding *w) {

  Winding *c = Winding_Alloc(w->numPoints);

  for (int32_t i = 0; i < w->numPoints; i++) {
    c->points[i] = w->points[w->numPoints - 1 - i];
  }

  c->numPoints = w->numPoints;
  return c;
}

/**
 * @brief Returns the AABB enclosing all points of the winding.
 */
Box3 Winding_Bounds(const Winding *w) {
  return Box3_FromPoints(w->points, w->numPoints);
}

/**
 * @brief Returns the centroid of the winding.
 */
Vec3 Winding_Center(const Winding *w) {

  Vec3 center = Vec3_Zero();

  for (int32_t i = 0; i < w->numPoints; i++) {
    center = Vec3_Add(w->points[i], center);
  }

  return Vec3_Scale(center, 1.0 / w->numPoints);
}

/**
 * @brief Returns the surface area of the winding.
 */
float Winding_Area(const Winding *w) {
  float area = 0.0;

  for (int32_t i = 2; i < w->numPoints; i++) {
    area += Vec3_TriangleArea(w->points[0], w->points[i - 1], w->points[i]);
  }

  return area;
}

/**
 * @brief Calculates the distance from `p` to `w`.
 * @see https://stackoverflow.com/questions/849211/shortest-distance-between-a-point-and-a-line-segment
 */
float Winding_Distance(const Winding *w, const Vec3 p, Vec3 *dir) {

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
void Winding_Plane(const Winding *w, Vec3 *normal, double *dist) {

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
Winding *Winding_ForPlane(const Vec3 normal, double dist) {

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

  Winding *w = Winding_Alloc(4);

  w->points[0] = Vec3d_CastVec3(points[0]);
  w->points[1] = Vec3d_CastVec3(points[1]);
  w->points[2] = Vec3d_CastVec3(points[2]);
  w->points[3] = Vec3d_CastVec3(points[3]);

  w->numPoints = 4;

  return w;
}

/**
 * @brief Removes duplicate adjacent points from the winding, in place.
 * @return False if fewer than three points remain, leaving `w` degenerate.
 */
static bool Winding_Compact(Winding *w) {

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
static Winding *Winding_Fix(Winding *w) {

  if (!Winding_Compact(w)) {
    Winding_Free(w);
    return NULL;
  }

  return w;
}

/**
 * @brief Splits the winding by the given plane into front and back components.
 */
void Winding_Split(const Winding *in, const Vec3 normal, double dist, double epsilon,
            Winding **front, Winding **back) {

  assert(in->numPoints);
  const int32_t maxPoints = in->numPoints + 4;

  WindingClipPoint clipPoints[maxPoints];
  memset(&clipPoints, 0, maxPoints * sizeof(WindingClipPoint));

  int32_t sideFront = 0, sideBack = 0;

  WindingClipPoint *c = clipPoints;
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
    *back = Winding_Copy(in);
    return;
  }

  if (sideBack == 0) {
    *front = Winding_Copy(in);
    *back = NULL;
    return;
  }

  Winding *f = Winding_Alloc(maxPoints);
  Winding *b = Winding_Alloc(maxPoints);

  for (int32_t i = 0; i < in->numPoints; i++) {
    const WindingClipPoint *c = clipPoints + i;

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

    const WindingClipPoint *d = clipPoints + ((i + 1) % in->numPoints);

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

  *front = Winding_Fix(f);
  *back = Winding_Fix(b);
}

/**
 * @brief Classifies each point of the winding against the given plane.
 * @param clipPoints Receives one entry per point of `in`.
 */
static void Winding_ClassifyPoints(const Winding *in, const Vec3 normal, double dist,
                                     double epsilon, WindingClipPoint *clipPoints,
                                     int32_t *sideFront, int32_t *sideBack) {

  *sideFront = *sideBack = 0;

  WindingClipPoint *c = clipPoints;
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
static void Winding_EmitClipped(const Winding *in, const WindingClipPoint *clipPoints,
                                  const Vec3 normal, double dist, Winding *out,
                                  int32_t capacity) {

  out->numPoints = 0;

  for (int32_t i = 0; i < in->numPoints; i++) {
    const WindingClipPoint *c = clipPoints + i;

    if (c->side == SIDE_BOTH) {
      out->points[out->numPoints] = c->point;
      out->numPoints++;
      continue;
    }

    if (c->side == SIDE_FRONT) {
      out->points[out->numPoints] = c->point;
      out->numPoints++;
    }

    const WindingClipPoint *d = clipPoints + ((i + 1) % in->numPoints);

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
void Winding_Clip(Winding **inOut, const Vec3 normal, double dist, double epsilon) {

  Winding *in = *inOut;

  assert(in->numPoints);
  const int32_t maxPoints = in->numPoints + 4;

  WindingClipPoint clipPoints[maxPoints];
  memset(clipPoints, 0, maxPoints * sizeof(WindingClipPoint));

  int32_t sideFront, sideBack;
  Winding_ClassifyPoints(in, normal, dist, epsilon, clipPoints, &sideFront, &sideBack);

  if (sideFront == 0) {
    Winding_Free(in);
    *inOut = NULL;
    return;
  }

  if (sideBack == 0) {
    return;
  }

  Winding *out = Winding_Alloc(maxPoints);

  Winding_EmitClipped(in, clipPoints, normal, dist, out, maxPoints);

  Winding_Free(in);
  *inOut = Winding_Fix(out);
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
Winding *Winding_ClipToWinding(const Winding *in, const Winding *clip, const Vec3 normal, double epsilon) {

  assert(in);
  assert(clip);
  assert(in->numPoints >= 3);
  assert(clip->numPoints >= 3);
  
  Winding *current = Winding_Copy(in);
  
  // Clip against each edge of the clipping winding
  for (int32_t edge = 0; edge < clip->numPoints && current != NULL; edge++) {
    
    const Vec3 edgeStart = clip->points[edge];
    const Vec3 edgeEnd = clip->points[(edge + 1) % clip->numPoints];
    
    // Build edge plane (perpendicular to edge, in the winding plane)
    const Vec3 edgeDir = Vec3_Normalize(Vec3_Subtract(edgeEnd, edgeStart));
    const Vec3 edgeNormal = Vec3_Cross(edgeDir, normal);
    const double edgeDist = Vec3_Dot(edgeNormal, edgeStart);
    
    // Clip against this edge plane (keep front side)
    Winding_Clip(&current, edgeNormal, edgeDist, epsilon);
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
 * at least `in->numPoints + 4 * clip->numPoints`.
 * @return `in` if no edge clipped it, otherwise `a` or `b`, or `NULL` if it was
 * clipped away entirely.
 * @remarks Nothing is allocated or freed. Prefer this over
 * `Winding_ClipToWinding` on hot paths, and note the result is only valid
 * until the next call reusing the same scratch windings.
 */
const Winding *Winding_ClipToWindingInto(const Winding *in, const Winding *clip,
                                                const Vec3 normal, double epsilon,
                                                Winding *a, Winding *b,
                                                int32_t capacity) {

  assert(in);
  assert(clip);
  assert(in->numPoints >= 3);
  assert(clip->numPoints >= 3);
  assert(a);
  assert(b);
  assert(capacity >= in->numPoints + 4 * clip->numPoints);

  const Winding *current = in;
  Winding *spare = a;

  for (int32_t edge = 0; edge < clip->numPoints; edge++) {

    const Vec3 edgeStart = clip->points[edge];
    const Vec3 edgeEnd = clip->points[(edge + 1) % clip->numPoints];

    const Vec3 edgeDir = Vec3_Normalize(Vec3_Subtract(edgeEnd, edgeStart));
    const Vec3 edgeNormal = Vec3_Cross(edgeDir, normal);
    const double edgeDist = Vec3_Dot(edgeNormal, edgeStart);

    WindingClipPoint clipPoints[current->numPoints];
    memset(clipPoints, 0, current->numPoints * sizeof(WindingClipPoint));

    int32_t sideFront, sideBack;
    Winding_ClassifyPoints(current, edgeNormal, edgeDist, epsilon, clipPoints,
                             &sideFront, &sideBack);

    if (sideFront == 0) {
      return NULL;
    }

    if (sideBack == 0) {
      continue;
    }

    Winding_EmitClipped(current, clipPoints, edgeNormal, edgeDist, spare, capacity);

    if (!Winding_Compact(spare)) {
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
Winding *Winding_Merge(const Winding *a, const Winding *b, const Vec3 normal) {
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

    // an edge of no length is no seam; merging across one pinches the polygon
    if (Vec3_Distance(p1, p2) < ON_EPSILON * 2.f) {
      continue;
    }

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
  Winding *merged = Winding_Alloc(a->numPoints + b->numPoints);

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

  // the merged polygon must cover exactly what the two did, or it folded over itself
  const float area = Winding_Area(a) + Winding_Area(b);
  Vec3 sum = Vec3_Zero();
  for (k = 0; k < merged->numPoints; k++) {
    sum = Vec3_Add(sum, Vec3_Cross(merged->points[k], merged->points[(k + 1) % merged->numPoints]));
  }
  if (fabsf(fabsf(Vec3_Dot(sum, normal)) * .5f - area) > Maxf(area * .001f, .05f)) {
    Winding_Free(merged);
    return NULL;
  }

  return Winding_Fix(merged);
}

/**
 * @return The distance of `b` from the line through `a` and `c`, positive if `b` is convex with
 * respect to the winding that has the given `normal`.
 */
static float Winding_PointHeight(Vec3 a, Vec3 b, Vec3 c, Vec3 normal) {

  const Vec3 ac = Vec3_Subtract(c, a);

  const float length = Vec3_Length(ac);
  if (length <= FLT_EPSILON) {
    return 0.f;
  }

  const Vec3 cross = Vec3_Cross(Vec3_Subtract(b, a), ac);
  const float height = Vec3_Length(cross) / length;

  return Vec3_Dot(cross, normal) >= 0.f ? height : -height;
}

/**
 * @brief Creates a vertex element array of triangles for the given winding.
 * @details This function uses an ear-clipping algorithm to clip triangles from the given winding.
 * A point is colinear if it lies within `COLINEAR_DISTANCE` of the line through its neighbors, and
 * is not a corner. Colinear points are kept as vertexes of the triangles beside them, so that the
 * triangles meet the faces that share their edge. A winding that is a sliver, with fewer than
 * three corners, is clipped at its widest point instead, so that it still gets the triangles it
 * has the area for. Points that no triangle has the area for are skipped over.
 * @param w The winding.
 * @param elements The output array, which must be `>= (w->numPoints - 2) * 3` in length.
 * @return The number of vertex elements written to tris.
 */
static int32_t Winding_ElementsClip(const Winding *w, int32_t *elements) {

  int32_t *out = elements;

  typedef struct {
    Vec3 position;
    int32_t index;
    int32_t corner;
    float height;
  } Point;

  int32_t numPoints = w->numPoints;
  Point points[numPoints];

  Vec3 normal = Vec3_Zero();

  for (int32_t i = 0; i < numPoints; i++) {
    points[i].position = w->points[i];
    points[i].index = i;

    const Vec3 p0 = Vec3_Subtract(w->points[i], w->points[0]);
    const Vec3 p1 = Vec3_Subtract(w->points[(i + 1) % numPoints], w->points[0]);

    normal = Vec3_Add(normal, Vec3_Cross(p0, p1));
  }

  while (numPoints > 2) {

    // find the corners, or points without collinear neighbors; the height of a point is its
    // distance from the line through its neighbors, and is positive if the point is convex

    int32_t numCorners = 0;
    for (int32_t i = 0; i < numPoints; i++) {

      const Point *a = &points[(i + 0) % numPoints];
      Point *b = &points[(i + 1) % numPoints];
      const Point *c = &points[(i + 2) % numPoints];

      // The angle at b is no use for this: a point a few tenths from its neighbor has an
      // arbitrary angle, and a long sliver has a tiny one, however much area it covers.

      b->height = Winding_PointHeight(a->position, b->position, c->position, normal);

      if (fabsf(b->height) < COLINEAR_DISTANCE) {
        b->corner = 0;
      } else {
        b->corner = ++numCorners;
      }
    }

    const Point *clip = NULL;
    if (numCorners >= 3 && numCorners < numPoints) {

      // chip away at edges with colinear points first

      float best = FLT_MAX;

      for (int32_t i = 0; i < numPoints; i++) {
        const Point *a = &points[(i + 0) % numPoints];
        const Point *b = &points[(i + 1) % numPoints];
        const Point *c = &points[(i + 2) % numPoints];

        if (!a->corner && b->height > 0.f) {
          const float area = Vec3_TriangleArea(a->position, b->position, c->position);
          if (area < best) {
            best = area;
            clip = b;
          }
        }
      }

    } else if (numCorners == numPoints && points[1].height > 0.f) {
      clip = points + 1;
    }

    if (!clip) {

      // clip the widest point, unless its triangle would leave a neighbor folded back over the
      // rest of the winding, which loses the triangle that the remainder has the area for

      float best = COLINEAR_DISTANCE, widest = COLINEAR_DISTANCE;
      const Point *fallback = NULL;

      for (int32_t i = 0; i < numPoints; i++) {
        const Point *b = &points[(i + 1) % numPoints];

        if (b->height <= COLINEAR_DISTANCE) {
          continue;
        }

        if (b->height > widest) {
          widest = b->height;
          fallback = b;
        }

        if (numPoints > 3) {
          const Vec3 z = points[(i + numPoints - 1) % numPoints].position;
          const Vec3 a = points[i].position;
          const Vec3 c = points[(i + 2) % numPoints].position;
          const Vec3 d = points[(i + 3) % numPoints].position;

          if (Winding_PointHeight(z, a, c, normal) < -COLINEAR_DISTANCE ||
              Winding_PointHeight(a, c, d, normal) < -COLINEAR_DISTANCE) {
            continue;
          }
        }

        if (b->height > best) {
          best = b->height;
          clip = b;
        }
      }

      if (!clip) {
        clip = fallback;
      }

      if (!clip) {
        break;
      }
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
 * @brief Clips ears from the winding with no tolerance for colinearity, taking the largest ear
 * that no other point lies in. This is for the slivers that `Winding_ElementsClip` leaves area
 * behind in: it has no regard for the edges a winding shares with its neighbors.
 */
static int32_t Winding_ElementsStrict(const Winding *w, int32_t *elements) {

  int32_t *out = elements;

  int32_t numPoints = w->numPoints;
  int32_t indexes[numPoints];

  Vec3 normal = Vec3_Zero();

  for (int32_t i = 0; i < numPoints; i++) {
    indexes[i] = i;
    normal = Vec3_Add(normal, Vec3_Cross(w->points[i], w->points[(i + 1) % numPoints]));
  }

  while (numPoints > 2) {

    int32_t clip = -1;
    float best = 0.f;

    for (int32_t pass = 0; pass < 2 && clip == -1; pass++) {
      for (int32_t i = 0; i < numPoints; i++) {

        const Vec3 a = w->points[indexes[(i + numPoints - 1) % numPoints]];
        const Vec3 b = w->points[indexes[i]];
        const Vec3 c = w->points[indexes[(i + 1) % numPoints]];

        const Vec3 cross = Vec3_Cross(Vec3_Subtract(b, a), Vec3_Subtract(c, a));
        if (Vec3_Dot(cross, normal) <= 0.f) {
          continue;
        }

        const float area = Vec3_Length(cross) * .5f;
        if (area <= best) {
          continue;
        }

        // on the first pass, no other point may lie inside the ear
        bool inside = false;
        for (int32_t j = 0; pass == 0 && j < numPoints && !inside; j++) {

          const Vec3 p = w->points[indexes[j]];
          if (Vec3_Equal(p, a) || Vec3_Equal(p, b) || Vec3_Equal(p, c)) {
            continue;
          }

          // a point just outside the ear counts: the diagonal then runs outside the winding
          const Vec3 n = Vec3_Normalize(normal);
          const Vec3 corners[] = { a, b, c };

          inside = true;
          for (int32_t k = 0; k < 3 && inside; k++) {
            const Vec3 from = corners[k];
            const Vec3 edge = Vec3_Subtract(corners[(k + 1) % 3], from);
            const float length = Vec3_Length(edge);
            inside = length > FLT_EPSILON &&
                     Vec3_Dot(Vec3_Cross(edge, Vec3_Subtract(p, from)), n) / length > -COLINEAR_DISTANCE;
          }
        }

        if (!inside) {
          best = area;
          clip = i;
        }
      }
    }

    if (clip == -1) {
      break;
    }

    *out++ = indexes[(clip + numPoints - 1) % numPoints];
    *out++ = indexes[clip];
    *out++ = indexes[(clip + 1) % numPoints];

    for (int32_t k = clip; k < numPoints - 1; k++) {
      indexes[k] = indexes[k + 1];
    }

    numPoints--;
  }

  return (int32_t) (ptrdiff_t) (out - elements);
}

/**
 * @return The area that the given triangle elements cover, in total.
 */
float Winding_ElementsArea(const Winding *w, const int32_t *elements, int32_t numElements) {

  float area = 0.f;

  for (int32_t i = 0; i < numElements; i += 3) {
    area += Vec3_TriangleArea(w->points[elements[i]], w->points[elements[i + 1]], w->points[elements[i + 2]]);
  }

  return area;
}

/**
 * @brief Creates a vertex element array of triangles for the given winding.
 * @details The winding is triangulated by `Winding_ElementsClip`, which keeps its colinear points.
 * Where that leaves a part of a sliver winding uncovered, the triangulation that comes closest
 * to covering the winding exactly is used instead.
 * @param w The winding.
 * @param elements The output array, which must be `>= (w->numPoints - 2) * 3` in length.
 * @return The number of vertex elements written to elements.
 */
int32_t Winding_Elements(const Winding *w, int32_t *elements) {

  int32_t numElements = Winding_ElementsClip(w, elements);

  if (w->numPoints > 3) {

    Vec3 sum = Vec3_Zero();
    for (int32_t i = 0; i < w->numPoints; i++) {
      sum = Vec3_Add(sum, Vec3_Cross(w->points[i], w->points[(i + 1) % w->numPoints]));
    }
    const float area = Vec3_Length(sum) * .5f;

    const float error = fabsf(Winding_ElementsArea(w, elements, numElements) - area);
    if (error > Maxf(area * .001f, .001f)) {

      int32_t strict[(w->numPoints - 2) * 3];
      const int32_t numStrict = Winding_ElementsStrict(w, strict);

      if (fabsf(Winding_ElementsArea(w, strict, numStrict) - area) < error) {
        memcpy(elements, strict, sizeof(int32_t) * numStrict);
        numElements = numStrict;
      }
    }
  }

  // a winding with no triangles is only a loss if it had area to cover
  if (numElements == 0 && w->numPoints > 2 && Winding_Area(w) > 1.f) {
    Com_Warn("Invalid winding: %d points have no area\n", w->numPoints);
  }

  return numElements;
}

/**
 * @brief Calculates the tangent vectors for the given vertexes and triangle elements.
 * @see http://foundationsofgameenginedev.com/FGED2-sample.pdf
 */
void Winding_Tangents(WindingVertex *vertexes, int32_t baseVertex, int32_t numVertexes, const int32_t *elements, int32_t numElements) {

  for (int32_t i = 0; i < numElements; i += 3) {

    const int32_t i0 = *(elements + i + 0) - baseVertex;
    const int32_t i1 = *(elements + i + 1) - baseVertex;
    const int32_t i2 = *(elements + i + 2) - baseVertex;

    WindingVertex *v0 = vertexes + i0;
    WindingVertex *v1 = vertexes + i1;
    WindingVertex *v2 = vertexes + i2;

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

  WindingVertex *v = vertexes;
  for (int32_t i = 0; i < numVertexes; i++, v++) {

    const Vec3 sdir = *v->tangent;
    const Vec3 tdir = *v->bitangent;

    Vec3_Tangents(*v->normal, sdir, tdir, v->tangent, v->bitangent);
  }
}
