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

#pragma once

#include "cm_bsp.h"

/**
 * @brief An ordered collection of coplanar points describing a convex volume.
 */
typedef struct {
  int32_t num_points; ///< The number of points in the winding.
  vec3_t points[0];   ///< The actual number of points will vary.
} cm_winding_t;

/**
 * @brief A winding point, clipped against a specific plane.
 */
typedef struct {
  vec3_t point; ///< The clipped point.
  double dist;  ///< The distance from the plane.
  int32_t side; ///< The plane side.
} cm_clip_point_t;

/**
 * @brief Allocates a winding with space for num_points points.
 */
cm_winding_t *Cm_AllocWinding(int32_t num_points);

/**
 * @brief Frees the winding.
 */
void Cm_FreeWinding(cm_winding_t *w);

/**
 * @brief Returns a deep copy of the winding.
 */
cm_winding_t *Cm_CopyWinding(const cm_winding_t *w);

/**
 * @brief Returns a new winding with points in the reverse order.
 */
cm_winding_t *Cm_ReverseWinding(const cm_winding_t *w);

/**
 * @brief Returns the axis-aligned bounding box enclosing the winding.
 */
box3_t Cm_WindingBounds(const cm_winding_t *w);

/**
 * @brief Returns the centroid of the winding.
 */
vec3_t Cm_WindingCenter(const cm_winding_t *w);

/**
 * @brief Returns the surface area of the winding.
 */
float Cm_WindingArea(const cm_winding_t *w);

/**
 * @brief Returns the minimum distance from point p to the winding boundary.
 * @param dir If non-NULL, receives the direction from p to the nearest point.
 */
float Cm_DistanceToWinding(const cm_winding_t *w, const vec3_t p, vec3_t *dir);

/**
 * @brief Creates a large axially-aligned winding for the given plane.
 */
cm_winding_t *Cm_WindingForPlane(const vec3_t normal, double dist);

/**
 * @brief Creates a winding from the vertex loop of a BSP face.
 */
cm_winding_t *Cm_WindingForFace(const bsp_file_t *file, const bsp_face_t *face);

/**
 * @brief Creates a winding from the vertex loop of a BSP brush side.
 */
cm_winding_t *Cm_WindingForBrushSide(const bsp_file_t *file, const bsp_brush_side_t *brush_side);

/**
 * @brief Computes the plane normal and distance from a winding's points.
 */
void Cm_PlaneForWinding(const cm_winding_t *w, vec3_t *normal, double *dist);

/**
 * @brief Splits the winding by the plane, producing front and back halves.
 */
void Cm_SplitWinding(const cm_winding_t *w, const vec3_t normal, double dist, double epsilon, cm_winding_t **front, cm_winding_t **back);

/**
 * @brief Clips the winding to the front half-space of the plane, freeing the back.
 */
void Cm_ClipWinding(cm_winding_t **w, const vec3_t normal, double dist, double epsilon);

/**
 * @brief Clips winding in against the clip winding's plane, returning the front fragment.
 */
cm_winding_t *Cm_ClipWindingToWinding(const cm_winding_t *in, const cm_winding_t *clip, const vec3_t normal, double epsilon);

/**
 * @brief Merges two coplanar windings into a single winding, if possible.
 * @return The merged winding, or NULL if the windings could not be merged.
 */
cm_winding_t *Cm_MergeWindings(const cm_winding_t *a, const cm_winding_t *b, const vec3_t normal);

/**
 * @brief Fills elements[] with triangle indices for the winding (fan triangulation).
 * @return The number of indices written.
 */
int32_t Cm_ElementsForWinding(const cm_winding_t *w, int32_t *elements);

/**
 * @brief Returns the area of the triangle formed by the three vertices.
 */
float Cm_TriangleArea(const vec3_t a, const vec3_t b, const vec3_t c);

/**
 * @brief Computes barycentric coordinates of point p in triangle abc.
 * @param out If non-NULL, receives the barycentric weights as a vec3_t.
 * @return The interpolated scalar value at p.
 */
float Cm_Barycentric(const vec3_t a, const vec3_t b, const vec3_t c, const vec3_t p, vec3_t *out);

/**
 * @brief Clips the axis-aligned bounding box by the given plane, returning the clipped box.
 */
box3_t Cm_ClipBox(const box3_t in, const vec4_t plane);

/**
 * @brief A UV mapped vertex primitive.
 */
typedef struct {
  vec3_t *position;  ///< The vertex position.
  vec3_t *normal;    ///< The vertex normal.
  vec3_t *tangent;   ///< The vertex tangent.
  vec3_t *bitangent; ///< The vertex bitangent.
  vec2_t *st;        ///< The vertex texture coordinate.
  int32_t num_tris;  ///< The number of triangles referencing this vertex.
} cm_vertex_t;

/**
 * @brief Computes and accumulates tangent and bitangent vectors for the given vertex range.
 */
void Cm_Tangents(cm_vertex_t *vertexes, int32_t base_vertex, int32_t num_vertexes, const int32_t *elements, int32_t num_elements);
