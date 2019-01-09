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

static const dvec_t MIN_EPSILON = (FLT_EPSILON * (dvec_t) 0.5);

/**
 * @brief
 */
cm_winding_t *Cm_AllocWinding(int32_t num_points) {

	SDL_AtomicAdd(&c_windings, 1);

	return Mem_Malloc(sizeof(int32_t) + sizeof(vec3_t) * num_points);
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
 * @brief Create a massive polygon for the specified plane. This will be used
 * as the basis for all clipping operations against the plane. Double precision
 * is used to produce very accurate results; this is an improvement over the
 * Quake II tools.
 */
cm_winding_t *Cm_WindingForPlane(const vec3_t normal, const vec_t dist) {
	dvec3_t org, vnormal, vright, vup;

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
			VectorSet(vright, -normal[1], normal[0], 0.0);
			break;
		case 2:
			VectorSet(vright, 0.0, -normal[2], normal[1]);
			break;
		default:
			Com_Error(ERROR_FATAL, "Bad axis\n");
	}

	VectorScale(vright, MAX_WORLD_DIST, vright);

	VectorCopy(normal, vnormal);

	// CrossProduct(vnormal, vright, vup);
	vup[0] = vnormal[1] * vright[2] - vnormal[2] * vright[1];
	vup[1] = vnormal[2] * vright[0] - vnormal[0] * vright[2];
	vup[2] = vnormal[0] * vright[1] - vnormal[1] * vright[0];

	VectorScale(vnormal, dist, org);

	// project a really big	axis aligned box onto the plane
	cm_winding_t *w = Cm_AllocWinding(4);

	VectorSubtract(org, vright, w->points[0]);
	VectorAdd(w->points[0], vup, w->points[0]);

	VectorAdd(org, vright, w->points[1]);
	VectorAdd(w->points[1], vup, w->points[1]);

	VectorAdd(org, vright, w->points[2]);
	VectorSubtract(w->points[2], vup, w->points[2]);

	VectorSubtract(org, vright, w->points[3]);
	VectorSubtract(w->points[3], vup, w->points[3]);

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
void Cm_ClipWinding(const cm_winding_t *in, const vec3_t normal, const vec_t dist, vec_t epsilon,
						cm_winding_t **front, cm_winding_t **back) {

	const int32_t max_points = in->num_points + 4;

	dvec_t dists[max_points];
	int32_t sides[max_points];

	int32_t counts[SIDE_BOTH + 1];
	memset(counts, 0, sizeof(counts));

	dvec3_t dnormal;
	VectorCopy(normal, dnormal);

	const dvec_t depsilon = (epsilon > MIN_EPSILON) ? epsilon : MIN_EPSILON;

	// determine sides for each point
	for (int32_t i = 0; i < in->num_points; i++) {
		const vec_t dot = DotProduct(in->points[i], dnormal) - dist;
		dists[i] = dot;
		if (dot > depsilon) {
			sides[i] = SIDE_FRONT;
		} else if (dot < -depsilon) {
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

		dvec3_t p1, p2, mid;
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
 * @brief
 */
void Cm_ChopWinding(cm_winding_t **in_out, const vec3_t normal, const vec_t dist, vec_t epsilon) {

	cm_winding_t *in = *in_out;
	const int32_t max_points = in->num_points + 4;

	dvec_t dists[max_points];
	int32_t sides[max_points];

	int32_t counts[SIDE_BOTH + 1];
	memset(counts, 0, sizeof(counts));

	dvec3_t dnormal;
	VectorCopy(normal, dnormal);

	const dvec_t depsilon = (epsilon > MIN_EPSILON) ? epsilon : MIN_EPSILON;

	// determine sides for each point
	for (int32_t i = 0; i < in->num_points; i++) {
		const vec_t dot = DotProduct(in->points[i], dnormal) - dist;
		dists[i] = dot;
		if (dot > depsilon) {
			sides[i] = SIDE_FRONT;
		} else if (dot < -depsilon) {
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

		dvec3_t p1, p2, mid;
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
