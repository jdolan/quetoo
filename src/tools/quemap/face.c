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

int32_t c_merge;
static int32_t c_faces;

/**
 * @brief
 */
face_t *AllocFace(void) {

	face_t *f = Mem_TagMalloc(sizeof(*f), MEM_TAG_FACE);
	c_faces++;

	f->num = -1;

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
	}
	Mem_Free(f);
	c_faces--;
}

#define	CONTINUOUS_EPSILON	0.001
#define EQUAL_EPSILON		0.001

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
		return NULL; // no matching edges
	}

	// if the slopes are colinear, the point can be removed
	back = f1->points[(i + f1->num_points - 1) % f1->num_points];
	VectorSubtract(p1, back, delta);
	CrossProduct(plane_normal, delta, normal);
	VectorNormalize(normal);

	back = f2->points[(j + 2) % f2->num_points];
	VectorSubtract(back, p1, delta);
	dot = DotProduct(delta, normal);
	if (dot > CONTINUOUS_EPSILON) {
		return NULL; // not a convex polygon
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
		return NULL; // not a convex polygon
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

#define SNAP_FLOAT_TO_INT   8
#define SNAP_INT_TO_FLOAT   (1.0 / SNAP_FLOAT_TO_INT)

/**
 * @brief Emit vertexes and vertex indices for the given face.
 */
static int32_t EmitFaceVertexes(const face_t *face) {

	const bsp_texinfo_t *texinfo = &bsp_file.texinfo[face->texinfo];

	const vec_t *sdir = texinfo->vecs[0];
	const vec_t *tdir = texinfo->vecs[1];

	for (int32_t i = 0; i < face->w->num_points; i++) {

		if (bsp_file.num_vertexes == MAX_BSP_VERTEXES) {
			Com_Error(ERROR_FATAL, "MAX_BSP_VERTEXES");
		}

		bsp_vertex_t v = {
			.texinfo = face->texinfo
		};

		VectorCopy(face->w->points[i], v.position);

		if (!(texinfo->flags & SURF_NO_WELD)) {
			for (int32_t i = 0; i < 3; i++) {
				v.position[i] = SNAP_INT_TO_FLOAT * floorf(v.position[i] * SNAP_FLOAT_TO_INT + 0.5);
			}
		}

		VectorCopy(planes[face->plane_num].normal, v.normal);
		TangentVectors(v.normal, sdir, tdir, v.tangent, v.bitangent);

		int32_t j;
		for (j = 0; j < bsp_file.num_vertexes; j++) {
			const bsp_vertex_t *v1 = &bsp_file.vertexes[j];
			if (VectorCompare(v1->position, v.position) && v1->texinfo == v.texinfo) {
				break;
			}
		}

		if (j == bsp_file.num_vertexes) {
			bsp_file.vertexes[bsp_file.num_vertexes] = v;
			bsp_file.num_vertexes++;
		}

		bsp_file.face_vertexes[bsp_file.num_face_vertexes] = j;
		bsp_file.num_face_vertexes++;
	}

	return bsp_file.num_face_vertexes - face->w->num_points;
}

/**
 * @brief
 */
void EmitFace(face_t *face) {

	assert(face->w->num_points > 2);

	if (face->merged) {
		return; // not a final face
	}
	// save output number so leaffaces can use
	face->num = bsp_file.num_faces;

	if (bsp_file.num_faces >= MAX_BSP_FACES) {
		Com_Error(ERROR_FATAL, "MAX_BSP_FACES\n");
	}

	bsp_face_t *df = &bsp_file.faces[bsp_file.num_faces];
	bsp_file.num_faces++;

	df->plane_num = face->plane_num;
	df->texinfo = face->texinfo;

	df->vertex = EmitFaceVertexes(face);
	df->num_vertexes = face->w->num_points;

	df->lightmap = -1;
}

#define MAX_PHONG_FACES 256

/**
 * @brief Populate Phong faces with indexes of all Phong shaded faces referencing the vertex.
 * @return The number of Phong shaded bsp_face_t's referencing the vertex.
 */
static size_t PhongFacesForVertex(const bsp_vertex_t *vertex, const bsp_face_t **phong_faces) {

	size_t count = 0;

	const bsp_face_t *face = bsp_file.faces;
	for (int32_t i = 0; i < bsp_file.num_faces; i++, face++) {

		if (!(bsp_file.texinfo[face->texinfo].flags & SURF_PHONG)) {
			continue;
		}

		const int32_t *fv = bsp_file.face_vertexes + face->vertex;
		for (uint16_t j = 0; j < face->num_vertexes; j++, fv++) {

			const bsp_vertex_t *v = &bsp_file.vertexes[*fv];

			if (VectorCompare(vertex->position, v->position)) {
				phong_faces[count++] = face;
				if (count == MAX_PHONG_FACES) {
					Mon_SendPoint(MON_ERROR, vertex->position, "MAX_PHONG_FACES");
				}
				break;
			}
		}
	}

	return count;
}

/**
 * @brief Calculate per-vertex (instead of per-plane) normal vectors. This is done by finding all of
 * the faces which share a given vertex, and calculating a weighted average of their normals.
 */
void PhongVertexes(void) {
	const bsp_face_t *phong_faces[MAX_PHONG_FACES];

	bsp_vertex_t *v = bsp_file.vertexes;
	for (int32_t i = 0; i < bsp_file.num_vertexes; i++, v++) {

		const size_t count = PhongFacesForVertex(v, phong_faces);
		if (count) {

			VectorClear(v->normal);

			const bsp_face_t **pf = phong_faces;
			for (size_t j = 0; j < count; j++, pf++) {

				const plane_t *plane = &planes[(*pf)->plane_num];

				winding_t *w = WindingForFace(*pf);
				VectorMA(v->normal, WindingArea(w), plane->normal, v->normal);
				FreeWinding(w);
			}

			VectorNormalize(v->normal);

			const bsp_texinfo_t *texinfo = &bsp_file.texinfo[v->texinfo];

			const vec_t *sdir = texinfo->vecs[0];
			const vec_t *tdir = texinfo->vecs[1];

			TangentVectors(v->normal, sdir, tdir, v->tangent, v->bitangent);
		}
	}
}
