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

#include <SDL_atomic.h>

#include "cm_local.h"

static SDL_atomic_t c_windings;

/**
 * @brief
 */
cm_winding_t *Cm_AllocWinding(int32_t num_points) {

	SDL_AtomicAdd(&c_windings, 1);

	return Mem_TagMalloc(sizeof(int32_t) + sizeof(vec3_t) * num_points, MEM_TAG_POLYLIB);
}

/**
 * @brief
 */
void Cm_FreeWinding(cm_winding_t *w) {

	SDL_AtomicAdd(&c_windings, -1);

	Mem_Free(w);
}

/**
 * @brief
 */
cm_winding_t *Cm_CopyWinding(const cm_winding_t *w) {

	cm_winding_t *c = Cm_AllocWinding(w->num_points);

	c->num_points = w->num_points;

	memcpy(c->points, w->points, c->num_points * sizeof(vec3_t));

	return c;
}

/**
 * @brief
 */
cm_winding_t *Cm_ReverseWinding(const cm_winding_t *w) {

	cm_winding_t *c = Cm_AllocWinding(w->num_points);

	for (int32_t i = 0; i < w->num_points; i++) {
		c->points[i] = w->points[w->num_points - 1 - i];
	}

	c->num_points = w->num_points;
	return c;
}

/**
 * @brief
 */
void Cm_WindingBounds(const cm_winding_t *w, vec3_t *mins, vec3_t *maxs) {

	*mins = Vec3_Mins();
	*maxs = Vec3_Maxs();

	for (int32_t i = 0; i < w->num_points; i++) {
		*mins = Vec3_Minf(*mins, w->points[i]);
		*maxs = Vec3_Maxf(*maxs, w->points[i]);
	}
}

/**
 * @brief
 */
vec3_t Cm_WindingCenter(const cm_winding_t *w) {

	vec3_t center = Vec3_Zero();

	for (int32_t i = 0; i < w->num_points; i++) {
		center = Vec3_Add(w->points[i], center);
	}

	return Vec3_Scale(center, 1.0 / w->num_points);
}

/**
 * @brief
 */
float Cm_WindingArea(const cm_winding_t *w) {
	float area = 0.0;

	for (int32_t i = 2; i < w->num_points; i++) {
		area += Cm_TriangleArea(w->points[0], w->points[i - 1], w->points[i]);
	}

	return area;
}

/**
 * @brief
 */
void Cm_PlaneForWinding(const cm_winding_t *w, vec3_t *normal, double *dist) {
	vec3_t v1, v2;

	v1 = Vec3_Subtract(w->points[1], w->points[0]);
	v2 = Vec3_Subtract(w->points[2], w->points[0]);
	*normal = Vec3_Cross(v2, v1);
	*normal = Vec3_Normalize(*normal);
	*dist = Vec3_Dot(w->points[0], *normal);
}

/**
 * @brief Create a massive polygon for the specified plane.
 */
cm_winding_t *Cm_WindingForPlane(const vec3_t normal, const double dist) {
	vec3_t org, right, up;

	// find the major axis
	float max = 0.0;
	int32_t x = -1;
	for (int32_t i = 0; i < 3; i++) {
		const float v = fabsf(normal.xyz[i]);
		if (v > max) {
			x = i;
			max = v;
		}
	}
	if (x == -1) {
		Com_Error(ERROR_FATAL, "No axis found\n");
	}

	switch (x) {
		case 0:
		case 1:
			right = Vec3(-normal.y, normal.x, 0.0);
			break;
		case 2:
			right = Vec3(0.0, -normal.z, normal.y);
			break;
		default:
			Com_Error(ERROR_FATAL, "Bad axis\n");
	}

	right = Vec3_Scale(right, MAX_WORLD_DIST);

	up = Vec3_Cross(normal, right);

	org = Vec3_Add(Vec3_Zero(), Vec3_Scale(normal, dist));

	// project a really big	axis aligned box onto the plane
	cm_winding_t *w = Cm_AllocWinding(4);

	w->points[0] = Vec3_Subtract(org, right);
	w->points[0] = Vec3_Add(w->points[0], up);

	w->points[1] = Vec3_Add(org, right);
	w->points[1] = Vec3_Add(w->points[1], up);

	w->points[2] = Vec3_Add(org, right);
	w->points[2] = Vec3_Subtract(w->points[2], up);

	w->points[3] = Vec3_Subtract(org, right);
	w->points[3] = Vec3_Subtract(w->points[3], up);

	w->num_points = 4;

	return w;
}

/**
 * @brief Creates a winding for the given face, removing any collinear points.
 */
cm_winding_t *Cm_WindingForFace(const bsp_file_t *file, const bsp_face_t *face) {

	cm_winding_t *w = Cm_AllocWinding(face->num_vertexes);
	const int32_t v = face->first_vertex;

	for (int32_t i = 0; i < face->num_vertexes; i++) {

		const bsp_vertex_t *v0 = &file->vertexes[(v + (i + 0) % face->num_vertexes)];
		const bsp_vertex_t *v1 = &file->vertexes[(v + (i + 1) % face->num_vertexes)];
		const bsp_vertex_t *v2 = &file->vertexes[(v + (i + 2) % face->num_vertexes)];

		w->points[w->num_points] = v0->position;
		w->num_points++;

		vec3_t a, b;
		a = Vec3_Subtract(v1->position, v0->position);
		b = Vec3_Subtract(v2->position, v1->position);

		a = Vec3_Normalize(a);
		b = Vec3_Normalize(b);

		if (Vec3_Dot(a, b) > 1.0 - SIDE_EPSILON) { // skip v1
			i++;
		}
	}

	return w;
}

/**
 * @brief
 */
void Cm_SplitWinding(const cm_winding_t *in, const vec3_t normal, const double dist, double epsilon,
						cm_winding_t **front, cm_winding_t **back) {

	assert(in->num_points);
	const int32_t max_points = in->num_points + 4;

	double dists[max_points];
	int32_t sides[max_points];

	int32_t counts[SIDE_BOTH + 1];
	memset(counts, 0, sizeof(counts));

	// determine sides for each point
	for (int32_t i = 0; i < in->num_points; i++) {
		const double dot = Vec3_Dot(in->points[i], normal) - dist;
		dists[i] = dot;
		if (dot > epsilon) {
			sides[i] = SIDE_FRONT;
		} else if (dot < -epsilon) {
			sides[i] = SIDE_BACK;
		} else {
			sides[i] = SIDE_BOTH;
		}
		counts[sides[i]]++;
	}

	sides[in->num_points] = sides[0];
	dists[in->num_points] = dists[0];

	if (!counts[SIDE_FRONT]) {
		*front = NULL;
		*back = Cm_CopyWinding(in);
		return;
	}

	if (!counts[SIDE_BACK]) {
		*front = Cm_CopyWinding(in);
		*back = NULL;
		return;
	}

	cm_winding_t *f = Cm_AllocWinding(max_points);
	cm_winding_t *b = Cm_AllocWinding(max_points);

	for (int32_t i = 0; i < in->num_points; i++) {

		vec3_t p1, p2, mid;
		p1 = in->points[i];

		if (sides[i] == SIDE_BOTH) {
			f->points[f->num_points] = p1;
			f->num_points++;

			b->points[b->num_points] = p1;
			b->num_points++;

			continue;
		}

		if (sides[i] == SIDE_FRONT) {
			f->points[f->num_points] = p1;
			f->num_points++;
		}

		if (sides[i] == SIDE_BACK) {
			b->points[b->num_points] = p1;
			b->num_points++;
		}

		if (sides[i + 1] == SIDE_BOTH || sides[i + 1] == sides[i]) {
			continue;
		}

		// generate a split point
		p2 = in->points[(i + 1) % in->num_points];

		const double dot = dists[i] / (dists[i] - dists[i + 1]);
		for (int32_t j = 0; j < 3; j++) { // avoid round off error when possible
			if (normal.xyz[j] == 1.f) {
				mid.xyz[j] = dist;
			} else if (normal.xyz[j] == -1.f) {
				mid.xyz[j] = -dist;
			} else {
				mid.xyz[j] = p1.xyz[j] + dot * (p2.xyz[j] - p1.xyz[j]);
			}
		}

		f->points[f->num_points] = mid;
		f->num_points++;

		b->points[b->num_points] = mid;
		b->num_points++;

		if (f->num_points == max_points || b->num_points == max_points) {
			Com_Error(ERROR_FATAL, "Points exceeded estimate\n");
		}
	}

	*front = f;
	*back = b;
}

/**
 * @brief Clips the winding against the given plane.
 */
void Cm_ClipWinding(cm_winding_t **in_out, const vec3_t normal, const double dist, const double epsilon) {

	cm_winding_t *in = *in_out;
	
	assert(in->num_points);
	const int32_t max_points = in->num_points + 4;

	double dists[max_points];
	int32_t sides[max_points];

	int32_t counts[SIDE_BOTH + 1];
	memset(counts, 0, sizeof(counts));

	// determine sides for each point
	for (int32_t i = 0; i < in->num_points; i++) {
		const double dot = Vec3_Dot(in->points[i], normal) - dist;
		dists[i] = dot;
		if (dot > epsilon) {
			sides[i] = SIDE_FRONT;
		} else if (dot < -epsilon) {
			sides[i] = SIDE_BACK;
		} else {
			sides[i] = SIDE_BOTH;
		}
		counts[sides[i]]++;
	}

	sides[in->num_points] = sides[0];
	dists[in->num_points] = dists[0];

	if (!counts[SIDE_FRONT]) {
		Cm_FreeWinding(in);
		*in_out = NULL;
		return;
	}

	if (!counts[SIDE_BACK]) {
		return;
	}

	cm_winding_t *f = Cm_AllocWinding(max_points);

	for (int32_t i = 0; i < in->num_points; i++) {

		vec3_t p1, p2, mid;
		p1 = in->points[i];

		if (sides[i] == SIDE_BOTH) {
			f->points[f->num_points] = p1;
			f->num_points++;
			continue;
		}

		if (sides[i] == SIDE_FRONT) {
			f->points[f->num_points] = p1;
			f->num_points++;
		}

		if (sides[i + 1] == SIDE_BOTH || sides[i + 1] == sides[i]) {
			continue;
		}

		// generate a split point
		p2 = in->points[(i + 1) % in->num_points];

		const double dot = dists[i] / (dists[i] - dists[i + 1]);
		for (int32_t j = 0; j < 3; j++) { // avoid round off error when possible
			if (normal.xyz[j] == 1.0) {
				mid.xyz[j] = dist;
			} else if (normal.xyz[j] == -1.0) {
				mid.xyz[j] = -dist;
			} else {
				mid.xyz[j] = p1.xyz[j] + dot * (p2.xyz[j] - p1.xyz[j]);
			}
		}

		f->points[f->num_points] = mid;
		f->num_points++;

		if (f->num_points == max_points) {
			Com_Error(ERROR_FATAL, "Points exceeded estimate\n");
		}
	}

	Cm_FreeWinding(in);
	*in_out = f;
}

/**
 * @brief If two polygons share a common edge and the edges that meet at the
 * common points are both inside the other polygons, merge them
 *
 * Returns NULL if the faces couldn't be merged, or the new face.
 * The originals will NOT be freed.
 */
cm_winding_t *Cm_MergeWindings(const cm_winding_t *a, const cm_winding_t *b, const vec3_t normal) {
	vec3_t p1, p2, back;
	int32_t i, j, k, l;
	vec3_t cross, delta;
	float dot;

	// find a common edge
	p1 = p2 = Vec3_Zero();
	j = 0;

	for (i = 0; i < a->num_points; i++) {
		p1 = a->points[i];
		p2 = a->points[(i + 1) % a->num_points];
		for (j = 0; j < b->num_points; j++) {
			vec3_t p3 = b->points[j];
			vec3_t p4 = b->points[(j + 1) % b->num_points];
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
		if (j < b->num_points) {
			break;
		}
	}

	if (i == a->num_points) {
		return NULL; // no matching edges
	}

	// if the slopes are colinear, the point can be removed
	back = a->points[(i + a->num_points - 1) % a->num_points];
	delta = Vec3_Subtract(p1, back);
	cross = Vec3_Cross(normal, delta);
	cross = Vec3_Normalize(cross);

	back = b->points[(j + 2) % b->num_points];
	delta = Vec3_Subtract(back, p1);
	dot = Vec3_Dot(delta, cross);
	if (dot > SIDE_EPSILON) {
		return NULL; // not a convex polygon
	}
	const _Bool keep1 = dot < -SIDE_EPSILON;

	back = a->points[(i + 2) % a->num_points];
	delta = Vec3_Subtract(back, p2);
	cross = Vec3_Cross(normal, delta);
	cross = Vec3_Normalize(cross);

	back = b->points[(j + b->num_points - 1) % b->num_points];
	delta = Vec3_Subtract(back, p2);
	dot = Vec3_Dot(delta, cross);
	if (dot > SIDE_EPSILON) {
		return NULL; // not a convex polygon
	}
	const _Bool keep2 = dot < -SIDE_EPSILON;

	// build the new polygon
	cm_winding_t *merged = Cm_AllocWinding(a->num_points + b->num_points);

	// copy first polygon
	for (k = (i + 1) % a->num_points; k != i; k = (k + 1) % a->num_points) {
		if (k == (i + 1) % a->num_points && !keep2) {
			continue;
		}

		merged->points[merged->num_points] = a->points[k];
		merged->num_points++;
	}

	// copy second polygon
	for (l = (j + 1) % b->num_points; l != j; l = (l + 1) % b->num_points) {
		if (l == (j + 1) % b->num_points && !keep1) {
			continue;
		}
		merged->points[merged->num_points] = b->points[l];
		merged->num_points++;
	}

	return merged;
}

/**
 * @brief Creates a vertex element array of triangles for the given winding.
 * @details This function uses an ear-clipping algorithm to clip triangles from
 * the given winding. Invalid triangles due to colinear points are skipped over.
 * @param w The winding.
 * @param elements The output array, which must be `>= (w->num_points - 2) * 3` in length.
 * @return The number of vertex elements written to tris.
 */
int32_t Cm_ElementsForWinding(const cm_winding_t *w, int32_t *elements) {

	int32_t *out = elements;

	typedef struct {
		vec3_t position;
		int32_t index;
		int32_t corner;
	} point_t;

	int32_t num_points = w->num_points;
	point_t points[num_points];

	for (int32_t i = 0; i < num_points; i++) {
		points[i].position = w->points[i];
		points[i].index = i;
	}

	while (num_points > 2) {

		// find the corners, or points without collinear neighbors

		int32_t num_corners = 0;
		for (int32_t i = 0; i < num_points; i++) {

			point_t *a = &points[(i + 0) % num_points];
			point_t *b = &points[(i + 1) % num_points];
			point_t *c = &points[(i + 2) % num_points];

			vec3_t ba, cb;
			ba = Vec3_Subtract(b->position, a->position);
			cb = Vec3_Subtract(c->position, b->position);
			ba = Vec3_Normalize(ba);
			cb = Vec3_Normalize(cb);

			if (Vec3_Dot(ba, cb) > 1.0 - SIDE_EPSILON) {
				b->corner = 0;
			} else {
				b->corner = ++num_corners;
			}
		}

		//assert(num_corners > 2);

		// chip away at edges with collinear points first

		const point_t *clip = NULL;
		if (num_corners < num_points) {
			float best = FLT_MAX;

			for (int32_t i = 0; i < num_points; i++) {
				const point_t *a = &points[(i + 0) % num_points];
				const point_t *b = &points[(i + 1) % num_points];
				const point_t *c = &points[(i + 2) % num_points];

				if (!a->corner && b->corner) {

					vec3_t ba, cb, cross;
					ba = Vec3_Subtract(b->position, a->position);
					cb = Vec3_Subtract(c->position, b->position);
					cross = Vec3_Cross(ba, cb);

					const float area = 0.5 * Vec3_Length(cross);
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
		const int32_t j = (i - 1 + num_points) % num_points;

		for (int32_t k = 0; k < 3; k++) {
			*out++ = points[(j + k) % num_points].index;
		}

		for (int32_t k = i; k < num_points - 1; k++) {
			points[k] = points[k + 1];
		}

		num_points--;
	}

	return (int32_t) (ptrdiff_t) (out - elements);
}

/**
* @return The area of the triangle defined by a, b and c.
*/
float Cm_TriangleArea(const vec3_t a, const vec3_t b, const vec3_t c) {
   vec3_t ba;
   vec3_t ca;
   vec3_t cross;

   ba = Vec3_Subtract(b, a);
   ca = Vec3_Subtract(c, a);

   cross = Vec3_Cross(ba, ca);
   return Vec3_Length(cross) * 0.5;
}

/**
* @brief Calculates barycentric coordinates for p in the triangle defined by a, b and c.
* @see https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/barycentric-coordinates
*/
float Cm_Barycentric(const vec3_t a, const vec3_t b, const vec3_t c, const vec3_t p, vec3_t *out) {

   const float abc = Cm_TriangleArea(a, b, c);
   if (abc) {
	   const float bcp = Cm_TriangleArea(b, c, p);
	   out->x = bcp / abc;

	   const float cap = Cm_TriangleArea(c, a, p);
	   out->y = cap / abc;

	   const float abp = Cm_TriangleArea(a, b, p);
	   out->z = abp / abc;

	   return out->x + out->y + out->z;
   } else {
	   *out = Vec3_Zero();
   }

   return FLT_MAX;
}
