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

	const size_t size = (size_t) ((cm_winding_t *) 0)->points[w->num_points];
	memcpy(c, w, size);

	return c;
}

/**
 * @brief
 */
cm_winding_t *Cm_ReverseWinding(const cm_winding_t *w) {

	cm_winding_t *c = Cm_AllocWinding(w->num_points);

	for (int32_t i = 0; i < w->num_points; i++) {
		VectorCopy(w->points[w->num_points - 1 - i], c->points[i]);
	}

	c->num_points = w->num_points;
	return c;
}

/**
 * @brief
 */
void Cm_WindingBounds(const cm_winding_t *w, vec3_t mins, vec3_t maxs) {

	ClearBounds(mins, maxs);

	for (int32_t i = 0; i < w->num_points; i++) {
		for (int32_t j = 0; j < 3; j++) {
			const vec_t v = w->points[i][j];
			if (v < mins[j]) {
				mins[j] = v;
			}
			if (v > maxs[j]) {
				maxs[j] = v;
			}
		}
	}
}

/**
 * @brief
 */
void Cm_WindingCenter(const cm_winding_t *w, vec3_t center) {

	VectorClear(center);

	for (int32_t i = 0; i < w->num_points; i++) {
		VectorAdd(w->points[i], center, center);
	}

	const vec_t scale = 1.0 / w->num_points;
	VectorScale(center, scale, center);
}

/**
 * @brief
 */
vec_t Cm_WindingArea(const cm_winding_t *w) {
	vec3_t d1, d2, cross;
	vec_t area = 0.0;

	for (int32_t i = 2; i < w->num_points; i++) {
		VectorSubtract(w->points[i - 1], w->points[0], d1);
		VectorSubtract(w->points[i], w->points[0], d2);
		CrossProduct(d1, d2, cross);
		area += 0.5 * VectorLength(cross);
	}
	return area;
}

/**
 * @brief
 */
void Cm_PlaneForWinding(const cm_winding_t *w, vec3_t normal, vec_t *dist) {
	vec3_t v1, v2;

	VectorSubtract(w->points[1], w->points[0], v1);
	VectorSubtract(w->points[2], w->points[0], v2);
	CrossProduct(v2, v1, normal);
	VectorNormalize(normal);
	*dist = DotProduct(w->points[0], normal);
}

/**
 * @brief Create a massive polygon for the specified plane.
 */
cm_winding_t *Cm_WindingForPlane(const vec3_t normal, const vec_t dist) {
	vec3_t org, right, up;

	// find the major axis
	vec_t max = 0.0;
	int32_t x = -1;
	for (int32_t i = 0; i < 3; i++) {
		const vec_t v = fabsf(normal[i]);
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
			VectorSet(right, -normal[1], normal[0], 0.0);
			break;
		case 2:
			VectorSet(right, 0.0, -normal[2], normal[1]);
			break;
		default:
			Com_Error(ERROR_FATAL, "Bad axis\n");
	}

	VectorScale(right, MAX_WORLD_DIST, right);

	CrossProduct(normal, right, up);

	VectorMA(vec3_origin, dist, normal, org);

	// project a really big	axis aligned box onto the plane
	cm_winding_t *w = Cm_AllocWinding(4);

	VectorSubtract(org, right, w->points[0]);
	VectorAdd(w->points[0], up, w->points[0]);

	VectorAdd(org, right, w->points[1]);
	VectorAdd(w->points[1], up, w->points[1]);

	VectorAdd(org, right, w->points[2]);
	VectorSubtract(w->points[2], up, w->points[2]);

	VectorSubtract(org, right, w->points[3]);
	VectorSubtract(w->points[3], up, w->points[3]);

	w->num_points = 4;

	return w;
}

/**
 * @brief Creates a winding for the given face, removing any collinear points.
 */
cm_winding_t *Cm_WindingForFace(const bsp_file_t *file, const bsp_face_t *face) {

	cm_winding_t *w = Cm_AllocWinding(face->num_vertexes);

	const int32_t *fv = file->face_vertexes + face->vertex;

	for (int32_t i = 0; i < face->num_vertexes; i++) {

		const bsp_vertex_t *v0 = &file->vertexes[*(fv + (i + 0) % face->num_vertexes)];
		const bsp_vertex_t *v1 = &file->vertexes[*(fv + (i + 1) % face->num_vertexes)];
		const bsp_vertex_t *v2 = &file->vertexes[*(fv + (i + 2) % face->num_vertexes)];

		VectorCopy(v0->position, w->points[w->num_points]);
		w->num_points++;

		vec3_t a, b;
		VectorSubtract(v0->position, v1->position, a);
		VectorSubtract(v0->position, v2->position, b);

		VectorNormalize(a);
		VectorNormalize(b);

		if (DotProduct(a, b) >= 1.0 - SIDE_EPSILON) { // skip v1
			i++;
		}
	}

	return w;
}

/**
 * @brief
 */
void Cm_SplitWinding(const cm_winding_t *in, const vec3_t normal, const vec_t dist, vec_t epsilon,
						cm_winding_t **front, cm_winding_t **back) {

	const int32_t max_points = in->num_points + 4;

	vec_t dists[max_points];
	int32_t sides[max_points];

	int32_t counts[SIDE_BOTH + 1];
	memset(counts, 0, sizeof(counts));

	// determine sides for each point
	for (int32_t i = 0; i < in->num_points; i++) {
		const vec_t dot = DotProduct(in->points[i], normal) - dist;
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
		VectorCopy(in->points[i], p1);

		if (sides[i] == SIDE_BOTH) {
			VectorCopy(p1, f->points[f->num_points]);
			f->num_points++;

			VectorCopy(p1, b->points[b->num_points]);
			b->num_points++;

			continue;
		}

		if (sides[i] == SIDE_FRONT) {
			VectorCopy(p1, f->points[f->num_points]);
			f->num_points++;
		}

		if (sides[i] == SIDE_BACK) {
			VectorCopy(p1, b->points[b->num_points]);
			b->num_points++;
		}

		if (sides[i + 1] == SIDE_BOTH || sides[i + 1] == sides[i]) {
			continue;
		}

		// generate a split point
		VectorCopy(in->points[(i + 1) % in->num_points], p2);

		const vec_t dot = dists[i] / (dists[i] - dists[i + 1]);
		for (int32_t j = 0; j < 3; j++) { // avoid round off error when possible
			if (normal[j] == 1) {
				mid[j] = dist;
			} else if (normal[j] == -1) {
				mid[j] = -dist;
			} else {
				mid[j] = p1[j] + dot * (p2[j] - p1[j]);
			}
		}

		VectorCopy(mid, f->points[f->num_points]);
		f->num_points++;

		VectorCopy(mid, b->points[b->num_points]);
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
void Cm_ClipWinding(cm_winding_t **in_out, const vec3_t normal, const vec_t dist, vec_t epsilon) {

	cm_winding_t *in = *in_out;
	const int32_t max_points = in->num_points + 4;

	vec_t dists[max_points];
	int32_t sides[max_points];

	int32_t counts[SIDE_BOTH + 1];
	memset(counts, 0, sizeof(counts));

	// determine sides for each point
	for (int32_t i = 0; i < in->num_points; i++) {
		const vec_t dot = DotProduct(in->points[i], normal) - dist;
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
		VectorCopy(in->points[i], p1);

		if (sides[i] == SIDE_BOTH) {
			VectorCopy(p1, f->points[f->num_points]);
			f->num_points++;
			continue;
		}

		if (sides[i] == SIDE_FRONT) {
			VectorCopy(p1, f->points[f->num_points]);
			f->num_points++;
		}

		if (sides[i + 1] == SIDE_BOTH || sides[i + 1] == sides[i]) {
			continue;
		}

		// generate a split point
		VectorCopy(in->points[(i + 1) % in->num_points], p2);

		const vec_t dot = dists[i] / (dists[i] - dists[i + 1]);
		for (int32_t j = 0; j < 3; j++) { // avoid round off error when possible
			if (normal[j] == 1) {
				mid[j] = dist;
			} else if (normal[j] == -1) {
				mid[j] = -dist;
			} else {
				mid[j] = p1[j] + dot * (p2[j] - p1[j]);
			}
		}

		VectorCopy(mid, f->points[f->num_points]);
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
	const vec_t *p1, *p2, *back;
	int32_t i, j, k, l;
	vec3_t cross, delta;
	vec_t dot;

	// find a common edge
	p1 = p2 = NULL;

	for (i = 0; i < a->num_points; i++) {
		p1 = a->points[i];
		p2 = a->points[(i + 1) % a->num_points];
		for (j = 0; j < b->num_points; j++) {
			const vec_t *p3 = b->points[j];
			const vec_t *p4 = b->points[(j + 1) % b->num_points];
			for (k = 0; k < 3; k++) {
				if (fabsf(p1[k] - p4[k]) > ON_EPSILON) {
					break;
				}
				if (fabsf(p2[k] - p3[k]) > ON_EPSILON) {
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
	VectorSubtract(p1, back, delta);
	CrossProduct(normal, delta, cross);
	VectorNormalize(cross);

	back = b->points[(j + 2) % b->num_points];
	VectorSubtract(back, p1, delta);
	dot = DotProduct(delta, cross);
	if (dot > SIDE_EPSILON) {
		return NULL; // not a convex polygon
	}
	const _Bool keep1 = dot < -SIDE_EPSILON;

	back = a->points[(i + 2) % a->num_points];
	VectorSubtract(back, p2, delta);
	CrossProduct(normal, delta, cross);
	VectorNormalize(cross);

	back = b->points[(j + b->num_points - 1) % b->num_points];
	VectorSubtract(back, p2, delta);
	dot = DotProduct(delta, cross);
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

		VectorCopy(a->points[k], merged->points[merged->num_points]);
		merged->num_points++;
	}

	// copy second polygon
	for (l = (j + 1) % b->num_points; l != j; l = (l + 1) % b->num_points) {
		if (l == (j + 1) % b->num_points && !keep1) {
			continue;
		}
		VectorCopy(b->points[l], merged->points[merged->num_points]);
		merged->num_points++;
	}

	return merged;
}

/**
 * @brief Creates a vertex element array of triangles for the given winding.
 * @details This function uses an ear-clipping algorithm to clip triangles from
 * the given winding. Invalid triangles due to colinear points are skipped over.
 * The returned vertex element array should be freed with Mem_Free.
 * @return The number of vertex elements written to tris.
 */
int32_t Cm_TrianglesForWinding(const cm_winding_t *w, int32_t **tris) {

	const int32_t num_vertexes = (w->num_points - 2) * 3;
	int32_t *out = *tris = Mem_Malloc(num_vertexes * sizeof(int32_t));

	GPtrArray *points = g_ptr_array_new();

	for (int32_t i = 0; i < w->num_points; i++) {
		g_ptr_array_add(points, (gpointer) w->points[i]);
	}

	while (points->len > 2) {

		int32_t i;
		const vec_t *a, *b, *c;

		for (i = 0; i < (int32_t) points->len; i++) {
			a = g_ptr_array_index(points, (i + 0) % points->len);
			b = g_ptr_array_index(points, (i + 1) % points->len);
			c = g_ptr_array_index(points, (i + 2) % points->len);

			vec3_t ba, cb, cross;
			VectorSubtract(b, a, ba);
			VectorSubtract(c, b, cb);
			CrossProduct(ba, cb, cross);

			if (VectorLength(cross)) {
				 break;
			}
		}

		*out++ = (int32_t) (a - (vec_t *) w->points) / 3;
		*out++ = (int32_t) (b - (vec_t *) w->points) / 3;
		*out++ = (int32_t) (c - (vec_t *) w->points) / 3;

		g_ptr_array_remove_index(points, (i + 1) % points->len);
	}

	g_ptr_array_free(points, false);

	return num_vertexes;
}
