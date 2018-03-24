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

#include "bspfile.h"
#include "polylib.h"

static uint32_t c_peak_windings;

#define	BOGUS_RANGE	MAX_WORLD_DIST

static const dvec_t MIN_EPSILON = (FLT_EPSILON * (dvec_t) 0.5);

/**
 * @brief
 */
winding_t *AllocWinding(int32_t points) {

	if (debug) {
		SDL_SemPost(semaphores.active_windings);
		uint32_t active_windings = SDL_SemValue(semaphores.active_windings);

		if (active_windings > c_peak_windings) {
			c_peak_windings = active_windings;
		}
	}

	return Mem_TagMalloc(sizeof(int32_t) + sizeof(vec3_t) * points, MEM_TAG_WINDING);
}

/**
 * @brief
 */
void FreeWinding(winding_t *w) {

	if (debug) {
		SDL_SemWait(semaphores.active_windings);
	}

	Mem_Free(w);
}

/**
 * @brief
 */
void RemoveColinearPoints(winding_t *w) {
	int32_t i;
	vec3_t v1, v2;
	int32_t nump;
	vec3_t p[MAX_POINTS_ON_WINDING];

	nump = 0;
	for (i = 0; i < w->num_points; i++) {
		const int32_t j = (i + 1) % w->num_points;
		const int32_t k = (i + w->num_points - 1) % w->num_points;
		VectorSubtract(w->points[j], w->points[i], v1);
		VectorSubtract(w->points[i], w->points[k], v2);
		VectorNormalize(v1);
		VectorNormalize(v2);
		if (DotProduct(v1, v2) < 0.999) {
			VectorCopy(w->points[i], p[nump]);
			nump++;
		}
	}

	if (nump == w->num_points) {
		return;
	}

	if (debug) {
		const int32_t j = w->num_points - nump;
		for (i = 0; i < j; i++) {
			SDL_SemPost(semaphores.removed_points);
		}
	}

	w->num_points = nump;
	memcpy(w->points, p, nump * sizeof(p[0]));
}

/**
 * @brief
 */
void WindingPlane(const winding_t *w, vec3_t normal, vec_t *dist) {
	vec3_t v1, v2;

	VectorSubtract(w->points[1], w->points[0], v1);
	VectorSubtract(w->points[2], w->points[0], v2);
	CrossProduct(v2, v1, normal);
	VectorNormalize(normal);
	*dist = DotProduct(w->points[0], normal);
}

/**
 * @brief
 */
vec_t WindingArea(const winding_t *w) {
	int32_t i;
	vec3_t d1, d2, cross;
	vec_t total;

	total = 0;
	for (i = 2; i < w->num_points; i++) {
		VectorSubtract(w->points[i - 1], w->points[0], d1);
		VectorSubtract(w->points[i], w->points[0], d2);
		CrossProduct(d1, d2, cross);
		total += 0.5 * VectorLength(cross);
	}
	return total;
}

/**
 * @brief
 */
void WindingBounds(const winding_t *w, vec3_t mins, vec3_t maxs) {

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
void WindingCenter(const winding_t *w, vec3_t center) {

	VectorClear(center);

	for (int32_t i = 0; i < w->num_points; i++) {
		VectorAdd(w->points[i], center, center);
	}

	const vec_t scale = 1.0 / w->num_points;
	VectorScale(center, scale, center);
}

/**
 * @brief Create a massive polygon for the specified plane. This will be used
 * as the basis for all clipping operations against the plane. Double precision
 * is used to produce very accurate results; this is an improvement over the
 * Quake II tools.
 */
winding_t *WindingForPlane(const vec3_t normal, const vec_t dist) {
	dvec3_t org, vnormal, vright, vup;
	int32_t i;

	// find the major axis
	vec_t max = -BOGUS_RANGE;
	int32_t x = -1;
	for (i = 0; i < 3; i++) {
		const vec_t v = fabs(normal[i]);
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

	VectorScale(vright, MAX_WORLD_COORD * 8.0, vright);

	VectorCopy(normal, vnormal);

	// CrossProduct(vnormal, vright, vup);
	vup[0] = vnormal[1] * vright[2] - vnormal[2] * vright[1];
	vup[1] = vnormal[2] * vright[0] - vnormal[0] * vright[2];
	vup[2] = vnormal[0] * vright[1] - vnormal[1] * vright[0];

	VectorScale(vnormal, dist, org);

	// project a really big	axis aligned box onto the plane
	winding_t *w = AllocWinding(4);

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
 * @brief
 */
winding_t *WindingForFace(const bsp_face_t *f) {
	int32_t i;
	bsp_vertex_t *dv;
	int32_t v;
	winding_t *w;

	w = AllocWinding(f->num_edges);
	w->num_points = f->num_edges;

	for (i = 0; i < f->num_edges; i++) {
		const int32_t se = bsp_file.face_edges[f->first_edge + i];
		if (se < 0) {
			v = bsp_file.edges[-se].v[1];
		} else {
			v = bsp_file.edges[se].v[0];
		}

		dv = &bsp_file.vertexes[v];
		VectorCopy(dv->point, w->points[i]);
	}

	RemoveColinearPoints(w);

	return w;
}

/**
 * @brief
 */
winding_t *CopyWinding(const winding_t *w) {
	size_t size;
	winding_t *c;

	c = AllocWinding(w->num_points);
	size = (size_t) ((winding_t *) 0)->points[w->num_points];
	memcpy(c, w, size);
	return c;
}

/**
 * @brief
 */
winding_t *ReverseWinding(winding_t *w) {
	int32_t i;
	winding_t *c;

	c = AllocWinding(w->num_points);
	for (i = 0; i < w->num_points; i++) {
		VectorCopy(w->points[w->num_points - 1 - i], c->points[i]);
	}
	c->num_points = w->num_points;
	return c;
}

/**
 * @brief
 */
void ClipWindingEpsilon(const winding_t *in, vec3_t normal, vec_t dist, vec_t epsilon,
                        winding_t **front, winding_t **back) {
	dvec_t dists[MAX_POINTS_ON_WINDING + 4];
	int32_t sides[MAX_POINTS_ON_WINDING + 4];
	int32_t counts[SIDE_BOTH + 1];
	dvec3_t dnormal;
	dvec_t dot;
	int32_t i, j;
	dvec3_t p1, p2;
	dvec3_t mid;
	winding_t *f, *b;
	int32_t maxpts;

	memset(counts, 0, sizeof(counts));

	VectorCopy(normal, dnormal);

	const dvec_t depsilon = (epsilon > MIN_EPSILON) ? epsilon : MIN_EPSILON;

	// determine sides for each point
	for (i = 0; i < in->num_points; i++) {
		dot = DotProduct(in->points[i], dnormal);
		dot -= dist;
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

	sides[i] = sides[0];
	dists[i] = dists[0];

	*front = *back = NULL;

	if (!counts[SIDE_FRONT]) {
		*back = CopyWinding(in);
		return;
	}
	if (!counts[SIDE_BACK]) {
		*front = CopyWinding(in);
		return;
	}

	maxpts = in->num_points + 4;

	*front = f = AllocWinding(maxpts);
	*back = b = AllocWinding(maxpts);

	for (i = 0; i < in->num_points; i++) {
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

		dot = dists[i] / (dists[i] - dists[i + 1]);
		for (j = 0; j < 3; j++) { // avoid round off error when possible
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
	}

	if (f->num_points > maxpts || b->num_points > maxpts) {
		Com_Error(ERROR_FATAL, "Points exceeded estimate\n");
	}
	if (f->num_points > MAX_POINTS_ON_WINDING || b->num_points > MAX_POINTS_ON_WINDING) {
		Com_Error(ERROR_FATAL, "MAX_POINTS_ON_WINDING\n");
	}
}

/**
 * @brief
 */
void ChopWindingInPlace(winding_t **inout, const vec3_t normal, const vec_t dist,
                        const vec_t epsilon) {
	winding_t *in;
	dvec_t dists[MAX_POINTS_ON_WINDING + 4];
	int32_t sides[MAX_POINTS_ON_WINDING + 4];
	int32_t counts[SIDE_BOTH + 1];
	dvec3_t dnormal;
	dvec_t dot;
	int32_t i, j;
	dvec3_t p1, p2;
	dvec3_t mid;
	winding_t *f;
	int32_t maxpts;

	in = *inout;

	memset(counts, 0, sizeof(counts));

	VectorCopy(normal, dnormal);

	const dvec_t depsilon = (epsilon > MIN_EPSILON) ? epsilon : MIN_EPSILON;

	// determine sides for each point
	for (i = 0; i < in->num_points; i++) {
		dot = DotProduct(in->points[i], dnormal);
		dot -= dist;
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

	sides[i] = sides[0];
	dists[i] = dists[0];

	if (!counts[SIDE_FRONT]) {
		FreeWinding(in);
		*inout = NULL;
		return;
	}
	if (!counts[SIDE_BACK]) {
		return;    // inout stays the same
	}

	maxpts = in->num_points + 4;

	f = AllocWinding(maxpts);

	for (i = 0; i < in->num_points; i++) {
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

		dot = dists[i] / (dists[i] - dists[i + 1]);
		for (j = 0; j < 3; j++) { // avoid round off error when possible
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
	}

	if (f->num_points > maxpts) {
		Com_Error(ERROR_FATAL, "Points exceeded estimate\n");
	}
	if (f->num_points > MAX_POINTS_ON_WINDING) {
		Com_Error(ERROR_FATAL, "MAX_POINTS_ON_WINDING\n");
	}

	FreeWinding(in);
	*inout = f;
}
