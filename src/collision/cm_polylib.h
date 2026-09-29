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

#include "cm_bsp.h"

/**
 * @brief An ordered collection of coplanar points describing a convex volume.
 */
typedef struct {

  /**
   * @brief The number of points in the winding.
   */
  int32_t numPoints;

  /**
   * @brief The actual number of points will vary.
   */
  Vec3 points[0];
} Winding;

/**
 * @brief A winding point, clipped against a specific plane.
 */
typedef struct {

  /**
   * @brief The clipped point.
   */
  Vec3 point;

  /**
   * @brief The distance from the plane.
   */
  double dist;

  /**
   * @brief The plane side.
   */
  int32_t side;
} WindingClipPoint;

/**
 * @brief Allocates a winding with space for `numPoints` points.
 */
Winding *Cm_AllocWinding(int32_t numPoints);

/**
 * @brief Frees the winding.
 */
void Cm_FreeWinding(Winding *w);

/**
 * @brief Returns a deep copy of the winding.
 */
Winding *Cm_CopyWinding(const Winding *w);

/**
 * @brief Returns a new winding with points in the reverse order.
 */
Winding *Cm_ReverseWinding(const Winding *w);

/**
 * @brief Returns the axis-aligned bounding box enclosing the winding.
 */
Box3 Cm_WindingBounds(const Winding *w);

/**
 * @brief Returns the centroid of the winding.
 */
Vec3 Cm_WindingCenter(const Winding *w);

/**
 * @brief Returns the surface area of the winding.
 */
float Cm_WindingArea(const Winding *w);

/**
 * @brief Returns the minimum distance from point p to the winding boundary.
 * @param dir If non-`NULL`, receives the direction from p to the nearest point.
 */
float Cm_DistanceToWinding(const Winding *w, const Vec3 p, Vec3 *dir);

/**
 * @brief Creates a large axially-aligned winding for the given plane.
 */
Winding *Cm_WindingForPlane(const Vec3 normal, double dist);

/**
 * @brief Creates a winding from the vertex loop of a BSP face.
 */
Winding *Cm_WindingForFace(const BspFile *file, const BspFace *face);

/**
 * @brief Creates a winding from the vertex loop of a BSP brush side.
 */
Winding *Cm_WindingForBrushSide(const BspFile *file, const BspBrushSide *brushSide);

/**
 * @brief Computes the plane normal and distance from a winding's points.
 */
void Cm_PlaneForWinding(const Winding *w, Vec3 *normal, double *dist);

/**
 * @brief Splits the winding by the plane, producing front and back halves.
 */
void Cm_SplitWinding(const Winding *w, const Vec3 normal, double dist, double epsilon, Winding **front, Winding **back);

/**
 * @brief Clips the winding to the front half-space of the plane, freeing the back.
 */
void Cm_ClipWinding(Winding **w, const Vec3 normal, double dist, double epsilon);

/**
 * @brief Clips winding in against the clip winding's plane, returning the front fragment.
 */
Winding *Cm_ClipWindingToWinding(const Winding *in, const Winding *clip, const Vec3 normal, double epsilon);

/**
 * @brief Clips `in` against every edge of `clip` without allocating, using the
 * caller-supplied scratch windings `a` and `b`.
 */
const Winding *Cm_ClipWindingToWindingInto(const Winding *in, const Winding *clip, const Vec3 normal, double epsilon, Winding *a, Winding *b, int32_t capacity);

/**
 * @brief Merges two coplanar windings into a single winding, if possible.
 * @return The merged winding, or `NULL` if the windings could not be merged.
 */
Winding *Cm_MergeWindings(const Winding *a, const Winding *b, const Vec3 normal);

/**
 * @brief Fills elements[] with triangle indices for the winding (fan triangulation).
 * @return The number of indices written.
 */
int32_t Cm_ElementsForWinding(const Winding *w, int32_t *elements);

/**
 * @brief Returns the area of the triangle formed by the three vertices.
 */
float Cm_TriangleArea(const Vec3 a, const Vec3 b, const Vec3 c);

/**
 * @brief Computes barycentric coordinates of point p in triangle abc.
 * @param out If non-`NULL`, receives the barycentric weights as a `Vec3`.
 * @return The interpolated scalar value at p.
 */
float Cm_Barycentric(const Vec3 a, const Vec3 b, const Vec3 c, const Vec3 p, Vec3 *out);

/**
 * @brief Clips the axis-aligned bounding box by the given plane, returning the clipped box.
 */
Box3 Cm_ClipBox(const Box3 in, const Vec4 plane);

/**
 * @brief A UV mapped vertex primitive.
 */
typedef struct {

  /**
   * @brief The vertex position.
   */
  Vec3 *position;

  /**
   * @brief The vertex normal.
   */
  Vec3 *normal;

  /**
   * @brief The vertex tangent.
   */
  Vec3 *tangent;

  /**
   * @brief The vertex bitangent.
   */
  Vec3 *bitangent;

  /**
   * @brief The vertex texture coordinate.
   */
  Vec2 *st;

  /**
   * @brief The number of triangles referencing this vertex.
   */
  int32_t numTris;
} WindingVertex;

/**
 * @brief Computes and accumulates tangent and bitangent vectors for the given vertex range.
 */
void Cm_Tangents(WindingVertex *vertexes, int32_t baseVertex, int32_t numVertexes, const int32_t *elements, int32_t numElements);
