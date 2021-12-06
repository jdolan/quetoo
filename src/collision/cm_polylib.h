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
	/**
	 * @brief The number of points in the winding.
	 */
	int32_t num_points;

	/**
	 * @brief The actual number of points will vary.
	 */
	vec3_t points[0];
} cm_winding_t;

/**
 * @brief A winding point, clipped against a specific plane.
 */
typedef struct {
	/**
	 * @brief The clipped point.
	 */
	vec3_t point;

	/**
	 * @brief The distance from the plane.
	 */
	double dist;

	/**
	 * @brief The plane side.
	 */
	int32_t side;
} cm_clip_point_t;

cm_winding_t *Cm_AllocWinding(int32_t num_points);
void Cm_FreeWinding(cm_winding_t *w);
cm_winding_t *Cm_CopyWinding(const cm_winding_t *w);
cm_winding_t *Cm_ReverseWinding(const cm_winding_t *w);
box3_t Cm_WindingBounds(const cm_winding_t *w);
vec3_t Cm_WindingCenter(const cm_winding_t *w);
float Cm_WindingArea(const cm_winding_t *w);
cm_winding_t *Cm_WindingForPlane(const vec3_t normal, double dist);
cm_winding_t *Cm_WindingForFace(const bsp_file_t *file, const bsp_face_t *face);
void Cm_PlaneForWinding(const cm_winding_t *w, vec3_t *normal, double *dist);
void Cm_SplitWinding(const cm_winding_t *w, const vec3_t normal, double dist, double epsilon, cm_winding_t **front, cm_winding_t **back);
void Cm_ClipWinding(cm_winding_t **w, const vec3_t normal, double dist, double epsilon);
int32_t Cm_ElementsForWinding(const cm_winding_t *w, int32_t *elements);
float Cm_TriangleArea(const vec3_t a, const vec3_t b, const vec3_t c);
float Cm_Barycentric(const vec3_t a, const vec3_t b, const vec3_t c, const vec3_t p, vec3_t *out);

/**
 * @brief A UV mapped vertex primitive.
 */
typedef struct {
	/**
	 * @brief The vertex position.
	 */
	vec3_t *position;

	/**
	 * @brief The vertex normal.
	 */
	vec3_t *normal;

	/**
	 * @brief The vertex tangent.
	 */
	vec3_t *tangent;

	/**
	 * @brief The vertex bitangent.
	 */
	vec3_t *bitangent;

	/**
	 * @brief The vertex texture coordinate.
	 */
	vec2_t *st;
} cm_vertex_t;


void Cm_Tangents(cm_vertex_t *vertexes, int32_t num_vertexes, const int32_t *elements, int32_t num_elements);
