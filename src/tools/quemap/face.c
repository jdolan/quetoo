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

#include "bsp.h"
#include "face.h"
#include "map.h"
#include "portal.h"
#include "qbsp.h"

/*
 *   some faces will be removed before saving, but still form nodes:
 *
 *   the insides of sky volumes
 *   meeting planes of different water current volumes
 */

#define EQUAL_EPSILON		0.001
#define	INTEGRAL_EPSILON	0.01
#define	POINT_EPSILON		0.1
#define	OFF_EPSILON			0.5

int32_t c_merge;

static face_t *edge_faces[MAX_BSP_EDGES][2];
int32_t first_bsp_model_edge = 1;

#define	HASH_SIZE 128

static int32_t vertex_chain[MAX_BSP_VERTS]; // the next vertex in a hash chain
static int32_t hash_verts[HASH_SIZE * HASH_SIZE]; // a vertex number, or 0 for no verts

/**
 * @brief
 */
static int32_t HashVertex(const vec3_t vec) {

	const int32_t x = (4096 + (int32_t) (vec[0] + 0.5)) >> 6;
	const int32_t y = (4096 + (int32_t) (vec[1] + 0.5)) >> 6;

	if (x < 0 || x >= HASH_SIZE || y < 0 || y >= HASH_SIZE) {
		Com_Error(ERROR_FATAL, "Point outside valid range\n");
	}

	return y * HASH_SIZE + x;
}

/**
 * @brief Snaps the vertex to integer coordinates if it is already very close,
 * then hashes it and searches for an existing vertex to reuse.
 */
int32_t EmitVertex(const vec3_t in) {
	int32_t i, vnum;
	vec3_t vert;

	for (i = 0; i < 3; i++) {
		const vec_t v = floor(in[i] + 0.5);

		if (fabs(in[i] - v) < INTEGRAL_EPSILON) {
			vert[i] = v;
		} else {
			vert[i] = in[i];
		}
	}

	const int32_t h = HashVertex(vert);

	for (vnum = hash_verts[h]; vnum; vnum = vertex_chain[vnum]) {
		vec3_t delta;

		// compare the points; we go out of our way to avoid using VectorLength
		VectorSubtract(bsp_file.vertexes[vnum].point, vert, delta);

		if (delta[0] < POINT_EPSILON && delta[0] > -POINT_EPSILON) {
			if (delta[1] < POINT_EPSILON && delta[1] > -POINT_EPSILON) {
				if (delta[2] < POINT_EPSILON && delta[2] > -POINT_EPSILON) {
					return vnum;
				}
			}
		}
	}

	if (bsp_file.num_vertexes == MAX_BSP_VERTS) {
		Com_Error(ERROR_FATAL, "MAX_BSP_VERTS\n");
	}

	VectorCopy(vert, bsp_file.vertexes[bsp_file.num_vertexes].point);

	vertex_chain[bsp_file.num_vertexes] = hash_verts[h];
	hash_verts[h] = bsp_file.num_vertexes;

	bsp_file.num_vertexes++;

	return bsp_file.num_vertexes - 1;
}

static int32_t c_faces;

/**
 * @brief
 */
face_t *AllocFace(void) {
	face_t *f;

	f = Mem_TagMalloc(sizeof(*f), MEM_TAG_FACE);
	c_faces++;

	return f;
}

/**
 * @brief
 */
static face_t *NewFaceFromFace(const face_t *f) {

	face_t *newf = AllocFace();
	*newf = *f;
	newf->merged = NULL;
	newf->w = NULL;
	return newf;
}

/**
 * @brief
 */
void FreeFace(face_t *f) {
	if (f->w) {
		FreeWinding(f->w);
		f->w = NULL;
	}
	Mem_Free(f);
	c_faces--;
}

/**
 * @brief Called by writebsp. Don't allow four way edges
 */
int32_t EmitEdge(int32_t v1, int32_t v2, face_t *f) {
	bsp_edge_t *edge;

	if (!no_share) {
		for (int32_t i = first_bsp_model_edge; i < bsp_file.num_edges; i++) {
			edge = &bsp_file.edges[i];
			if (v1 == edge->v[1] && v2 == edge->v[0] && edge_faces[i][0]->contents == f->contents) {
				if (edge_faces[i][1]) {
					continue;
				}
				edge_faces[i][1] = f;
				return -i;
			}
		}
	}
	// emit an edge
	if (bsp_file.num_edges >= MAX_BSP_EDGES) {
		Com_Error(ERROR_FATAL, "MAX_BSP_EDGES\n");
	}
	edge = &bsp_file.edges[bsp_file.num_edges];
	edge->v[0] = v1;
	edge->v[1] = v2;
	edge_faces[bsp_file.num_edges][0] = f;
	bsp_file.num_edges++;

	return bsp_file.num_edges - 1;
}

#define	CONTINUOUS_EPSILON	0.001

/**
 * @brief If two polygons share a common edge and the edges that meet at the
 * common points are both inside the other polygons, merge them
 *
 * Returns NULL if the faces couldn't be merged, or the new face.
 * The originals will NOT be freed.
 */
static winding_t *MergeWindings(winding_t *f1, winding_t *f2, const vec3_t plane_normal) {
	vec_t *p1, *p2, *back;
	winding_t *newf;
	int32_t i, j, k, l;
	vec3_t normal, delta;
	vec_t dot;
	_Bool keep1, keep2;

	// find a common edge
	p1 = p2 = NULL;
	j = 0;

	for (i = 0; i < f1->num_points; i++) {
		p1 = f1->points[i];
		p2 = f1->points[(i + 1) % f1->num_points];
		for (j = 0; j < f2->num_points; j++) {
			const vec_t *p3 = f2->points[j];
			const vec_t *p4 = f2->points[(j + 1) % f2->num_points];
			for (k = 0; k < 3; k++) {
				if (fabs(p1[k] - p4[k]) > EQUAL_EPSILON) {
					break;
				}
				if (fabs(p2[k] - p3[k]) > EQUAL_EPSILON) {
					break;
				}
			}
			if (k == 3) {
				break;
			}
		}
		if (j < f2->num_points) {
			break;
		}
	}

	if (i == f1->num_points) {
		return NULL;    // no matching edges
	}

	// check slope of connected lines
	// if the slopes are colinear, the point can be removed
	back = f1->points[(i + f1->num_points - 1) % f1->num_points];
	VectorSubtract(p1, back, delta);
	CrossProduct(plane_normal, delta, normal);
	VectorNormalize(normal);

	back = f2->points[(j + 2) % f2->num_points];
	VectorSubtract(back, p1, delta);
	dot = DotProduct(delta, normal);
	if (dot > CONTINUOUS_EPSILON) {
		return NULL;    // not a convex polygon
	}
	keep1 = (_Bool) (dot < -CONTINUOUS_EPSILON);

	back = f1->points[(i + 2) % f1->num_points];
	VectorSubtract(back, p2, delta);
	CrossProduct(plane_normal, delta, normal);
	VectorNormalize(normal);

	back = f2->points[(j + f2->num_points - 1) % f2->num_points];
	VectorSubtract(back, p2, delta);
	dot = DotProduct(delta, normal);
	if (dot > CONTINUOUS_EPSILON) {
		return NULL;    // not a convex polygon
	}
	keep2 = (_Bool) (dot < -CONTINUOUS_EPSILON);

	// build the new polygon
	newf = AllocWinding(f1->num_points + f2->num_points);

	// copy first polygon
	for (k = (i + 1) % f1->num_points; k != i; k = (k + 1) % f1->num_points) {
		if (k == (i + 1) % f1->num_points && !keep2) {
			continue;
		}

		VectorCopy(f1->points[k], newf->points[newf->num_points]);
		newf->num_points++;
	}

	// copy second polygon
	for (l = (j + 1) % f2->num_points; l != j; l = (l + 1) % f2->num_points) {
		if (l == (j + 1) % f2->num_points && !keep1) {
			continue;
		}
		VectorCopy(f2->points[l], newf->points[newf->num_points]);
		newf->num_points++;
	}

	return newf;
}

/**
 * @brief If two polygons share a common edge and the edges that meet at the
 * common points are both inside the other polygons, merge them
 *
 * @return NULL if the faces couldn't be merged, or the new face.
 * @remark The originals will NOT be freed.
 */
face_t *MergeFaces(face_t *f1, face_t *f2, const vec3_t normal) {

	if (!f1->w || !f2->w) {
		return NULL;
	}
	if (f1->texinfo != f2->texinfo) {
		return NULL;
	}
	if (f1->plane_num != f2->plane_num) { // on front and back sides
		return NULL;
	}
	if (f1->contents != f2->contents) {
		return NULL;
	}

	winding_t *nw = MergeWindings(f1->w, f2->w, normal);
	if (!nw) {
		return NULL;
	}

	c_merge++;
	face_t *newf = NewFaceFromFace(f1);
	newf->w = nw;

	f1->merged = newf;
	f2->merged = newf;

	return newf;
}
