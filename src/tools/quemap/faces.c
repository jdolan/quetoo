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

#include "qbsp.h"

/*
 *
 *   some faces will be removed before saving, but still form nodes:
 *
 *   the insides of sky volumes
 *   meeting planes of different water current volumes
 *
 */

#define EQUAL_EPSILON		0.001
#define	INTEGRAL_EPSILON	0.01
#define	POINT_EPSILON		0.1
#define	OFF_EPSILON			0.5

static int32_t c_merge;
static int32_t c_subdivide;

static int32_t c_totalverts;
static int32_t c_uniqueverts;
static int32_t c_degenerate;
static int32_t c_tjunctions;
static int32_t c_faceoverflows;
static int32_t c_facecollapse;
static int32_t c_badstartverts;

#define	MAX_SUPERVERTS	512
static int32_t superverts[MAX_SUPERVERTS];
static int32_t num_superverts;

static face_t *edge_faces[MAX_BSP_EDGES][2];
int32_t first_bsp_model_edge = 1;

static int32_t c_tryedges;

static vec3_t edge_dir;
static vec3_t edge_start;

static uint32_t num_edge_verts;
static int32_t edge_verts[MAX_BSP_VERTS];

int32_t subdivide_size = 1024;

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
static int32_t FindVertex(const vec3_t in) {
	int32_t i, vnum;
	vec3_t vert;

	c_totalverts++;

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

	// emit a vertex
	if (bsp_file.num_vertexes == MAX_BSP_VERTS) {
		Com_Error(ERROR_FATAL, "MAX_BSP_VERTS\n");
	}

	VectorCopy(vert, bsp_file.vertexes[bsp_file.num_vertexes].point);

	vertex_chain[bsp_file.num_vertexes] = hash_verts[h];
	hash_verts[h] = bsp_file.num_vertexes;

	c_uniqueverts++;

	bsp_file.num_vertexes++;

	return bsp_file.num_vertexes - 1;
}

static int32_t c_faces;

/**
 * @brief
 */
static face_t *AllocFace(void) {
	face_t *f;

	f = Mem_TagMalloc(sizeof(*f), MEM_TAG_FACE);
	c_faces++;

	return f;
}

/**
 * @brief
 */
static face_t *NewFaceFromFace(const face_t *f) {
	face_t *newf;

	newf = AllocFace();
	*newf = *f;
	newf->merged = NULL;
	newf->split[0] = newf->split[1] = NULL;
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

/**
 * @brief The faces vertexes have been added to the superverts[] array,
 * and there may be more there than can be held in a face (MAXEDGES).
 *
 * If less, the faces vertexnums[] will be filled in, otherwise
 * face will reference a tree of split[] faces until all of the
 * vertexnums can be added.
 *
 * superverts[base] will become face->vertexnums[0], and the others
 * will be circularly filled in.
 */
static void FaceFromSuperverts(node_t *node, face_t *f, int32_t base) {
	face_t *newf;
	int32_t remaining;
	int32_t i;

	remaining = num_superverts;
	while (remaining > MAXEDGES) { // must split into two faces, because of vertex overload
		c_faceoverflows++;

		newf = f->split[0] = NewFaceFromFace(f);
		newf = f->split[0];
		newf->next = node->faces;
		node->faces = newf;

		newf->num_points = MAXEDGES;
		for (i = 0; i < MAXEDGES; i++) {
			newf->vertexnums[i] = superverts[(i + base) % num_superverts];
		}

		f->split[1] = NewFaceFromFace(f);
		f = f->split[1];
		f->next = node->faces;
		node->faces = f;

		remaining -= (MAXEDGES - 2);
		base = (base + MAXEDGES - 1) % num_superverts;
	}

	// copy the vertexes back to the face
	f->num_points = remaining;
	for (i = 0; i < remaining; i++) {
		f->vertexnums[i] = superverts[(i + base) % num_superverts];
	}
}

/**
 * @brief
 */
static void EmitFaceVertexes(node_t *node, face_t *f) {
	winding_t *w;
	int32_t i;

	if (f->merged || f->split[0] || f->split[1]) {
		return;
	}

	w = f->w;
	for (i = 0; i < w->num_points; i++) {
		if (noweld) { // make every point unique
			if (bsp_file.num_vertexes == MAX_BSP_VERTS) {
				Com_Error(ERROR_FATAL, "MAX_BSP_VERTS\n");
			}
			superverts[i] = bsp_file.num_vertexes;
			VectorCopy(w->points[i], bsp_file.vertexes[bsp_file.num_vertexes].point);
			bsp_file.num_vertexes++;
			c_uniqueverts++;
			c_totalverts++;
		} else {
			superverts[i] = FindVertex(w->points[i]);
		}
	}
	num_superverts = w->num_points;

	// this may fragment the face if > MAXEDGES
	FaceFromSuperverts(node, f, 0);
}

/**
 * @brief
 */
static void EmitVertexes_r(node_t *node) {
	int32_t i;
	face_t *f;

	if (node->plane_num == PLANENUM_LEAF) {
		return;
	}

	for (f = node->faces; f; f = f->next) {
		EmitFaceVertexes(node, f);
	}

	for (i = 0; i < 2; i++) {
		EmitVertexes_r(node->children[i]);
	}
}

/**
 * @brief Forced a dumb check of everything
 */
static void FindEdgeVertexes(vec3_t v1, vec3_t v2) {

	int32_t x1 = (4096 + (int32_t) (v1[0] + 0.5)) >> 6;
	int32_t y1 = (4096 + (int32_t) (v1[1] + 0.5)) >> 6;
	int32_t x2 = (4096 + (int32_t) (v2[0] + 0.5)) >> 6;
	int32_t y2 = (4096 + (int32_t) (v2[1] + 0.5)) >> 6;

	int32_t tmp;
	if (x1 > x2) {
		tmp = x1;
		x1 = x2;
		x2 = tmp;
	}
	if (y1 > y2) {
		tmp = y1;
		y1 = y2;
		y2 = tmp;
	}

	num_edge_verts = 0;

	int32_t x, y;
	for (x = x1; x <= x2; x++) {
		for (y = y1; y <= y2; y++) {
			int32_t vnum;
			for (vnum = hash_verts[y * HASH_SIZE + x]; vnum; vnum = vertex_chain[vnum]) {
				edge_verts[num_edge_verts++] = vnum;
			}
		}
	}
}

/**
 * @brief Can be recursively reentered
 */
static void TestEdge(vec_t start, vec_t end, int32_t p1, int32_t p2, int32_t startvert) {
	vec_t dist;
	vec3_t delta;
	vec3_t exact;
	vec3_t off;
	vec_t error;
	vec3_t p;

	if (p1 == p2) {
		c_degenerate++;
		return; // degenerate edge
	}

	for (uint32_t k = startvert; k < num_edge_verts; k++) {
		const int32_t j = edge_verts[k];
		if (j == p1 || j == p2) {
			continue;
		}

		VectorCopy(bsp_file.vertexes[j].point, p);

		VectorSubtract(p, edge_start, delta);
		dist = DotProduct(delta, edge_dir);
		if (dist <= start || dist >= end) {
			continue;    // off an end
		}

		VectorMA(edge_start, dist, edge_dir, exact);
		VectorSubtract(p, exact, off);
		error = VectorLength(off);

		if (fabs(error) > OFF_EPSILON) {
			continue;    // not on the edge
		}

		// break the edge
		c_tjunctions++;
		TestEdge(start, dist, p1, j, k + 1);
		TestEdge(dist, end, j, p2, k + 1);
		return;
	}

	// the edge p1 to p2 is now free of tjunctions
	if (num_superverts >= MAX_SUPERVERTS) {
		Com_Error(ERROR_FATAL, "MAX_SUPERVERTS\n");
	}
	superverts[num_superverts] = p1;
	num_superverts++;
}

/**
 * @brief
 */
static void FixFaceEdges(node_t *node, face_t *f) {
	int32_t i;
	vec3_t e2;
	vec_t len;
	int32_t count[MAX_SUPERVERTS], start[MAX_SUPERVERTS];
	int32_t base;

	if (f->merged || f->split[0] || f->split[1]) {
		return;
	}

	num_superverts = 0;

	for (i = 0; i < f->num_points; i++) {
		const int32_t p1 = f->vertexnums[i];
		const int32_t p2 = f->vertexnums[(i + 1) % f->num_points];

		VectorCopy(bsp_file.vertexes[p1].point, edge_start);
		VectorCopy(bsp_file.vertexes[p2].point, e2);

		FindEdgeVertexes(edge_start, e2);

		VectorSubtract(e2, edge_start, edge_dir);
		len = VectorNormalize(edge_dir);

		start[i] = num_superverts;
		TestEdge(0, len, p1, p2, 0);

		count[i] = num_superverts - start[i];
	}

	if (num_superverts < 3) { // entire face collapsed
		f->num_points = 0;
		c_facecollapse++;
		return;
	}
	// we want to pick a vertex that doesn't have tjunctions
	// on either side, which can cause artifacts on trifans,
	// especially underwater
	for (i = 0; i < f->num_points; i++) {
		if (count[i] == 1 && count[(i + f->num_points - 1) % f->num_points] == 1) {
			break;
		}
	}
	if (i == f->num_points) {
		c_badstartverts++;
		base = 0;
	} else { // rotate the vertex order
		base = start[i];
	}

	// this may fragment the face if > MAXEDGES
	FaceFromSuperverts(node, f, base);
}

/**
 * @brief
 */
static void FixEdges_r(node_t *node) {
	int32_t i;
	face_t *f;

	if (node->plane_num == PLANENUM_LEAF) {
		return;
	}

	for (f = node->faces; f; f = f->next) {
		FixFaceEdges(node, f);
	}

	for (i = 0; i < 2; i++) {
		FixEdges_r(node->children[i]);
	}
}

/**
 * @brief
 */
void FixTjuncs(node_t *head_node) {
	// snap and merge all vertexes
	Com_Verbose("---- Fixing T Junctions ----\n");
	memset(hash_verts, 0, sizeof(hash_verts));
	c_totalverts = 0;
	c_uniqueverts = 0;
	c_faceoverflows = 0;
	EmitVertexes_r(head_node);
	Com_Verbose("%i unique from %i\n", c_uniqueverts, c_totalverts);

	// break edges on tjunctions
	Com_Verbose("---- tjunc ----\n");
	c_tryedges = 0;
	c_degenerate = 0;
	c_facecollapse = 0;
	c_tjunctions = 0;
	if (!notjunc) {
		FixEdges_r(head_node);
	}
	Com_Verbose("%5i edges degenerated\n", c_degenerate);
	Com_Verbose("%5i faces degenerated\n", c_facecollapse);
	Com_Verbose("%5i edges added by tjunctions\n", c_tjunctions);
	Com_Verbose("%5i faces added by tjunctions\n", c_faceoverflows);
	Com_Verbose("%5i bad start verts\n", c_badstartverts);
}

/*
 * GetEdge2
 *
 * Called by writebsp. Don't allow four way edges
 */
int32_t GetEdge2(int32_t v1, int32_t v2, face_t *f) {
	bsp_edge_t *edge;
	int32_t i;

	c_tryedges++;

	if (!noshare) {
		for (i = first_bsp_model_edge; i < bsp_file.num_edges; i++) {
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

/*
 *
 * FACE MERGING
 *
 */

#define	CONTINUOUS_EPSILON	0.001

/**
 * @brief If two polygons share a common edge and the edges that meet at the
 * common points are both inside the other polygons, merge them
 *
 * Returns NULL if the faces couldn't be merged, or the new face.
 * The originals will NOT be freed.
 */
static winding_t *TryMergeWinding(winding_t *f1, winding_t *f2, const vec3_t planenormal) {
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
	CrossProduct(planenormal, delta, normal);
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
	CrossProduct(planenormal, delta, normal);
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
 * Returns NULL if the faces couldn't be merged, or the new face.
 * The originals will NOT be freed.
 */
static face_t *TryMerge(face_t *f1, face_t *f2, const vec3_t planenormal) {
	face_t *newf;
	winding_t *nw;

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

	nw = TryMergeWinding(f1->w, f2->w, planenormal);
	if (!nw) {
		return NULL;
	}

	c_merge++;
	newf = NewFaceFromFace(f1);
	newf->w = nw;

	f1->merged = newf;
	f2->merged = newf;

	return newf;
}

/**
 * @brief
 */
static void MergeNodeFaces(node_t *node) {
	face_t *f1, *f2, *end;
	face_t *merged;
	const map_plane_t *plane;

	plane = &map_planes[node->plane_num];
	merged = NULL;

	for (f1 = node->faces; f1; f1 = f1->next) {
		if (f1->merged || f1->split[0] || f1->split[1]) {
			continue;
		}
		for (f2 = node->faces; f2 != f1; f2 = f2->next) {
			if (f2->merged || f2->split[0] || f2->split[1]) {
				continue;
			}
			merged = TryMerge(f1, f2, plane->normal);
			if (!merged) {
				continue;
			}

			// add merged to the end of the node face list
			// so it will be checked against all the faces again
			for (end = node->faces; end->next; end = end->next)
				;
			merged->next = NULL;
			end->next = merged;
			break;
		}
	}
}

/**
 * @brief Chop up faces that are larger than we want in the surface cache
 */
static void SubdivideFace(node_t *node, face_t *f) {
	vec_t mins, maxs;
	vec_t v;
	int32_t axis, i;
	const bsp_texinfo_t *tex;
	vec3_t temp;
	vec_t dist;
	winding_t *w, *frontw, *backw;

	if (f->merged) {
		return;
	}

	// special (non-surface cached) faces don't need subdivision
	tex = &bsp_file.texinfo[f->texinfo];

	if (tex->flags & (SURF_SKY | SURF_WARP)) {
		return;
	}

	for (axis = 0; axis < 2; axis++) {
		while (true) {
			mins = 999999;
			maxs = -999999;

			VectorCopy(tex->vecs[axis], temp);
			w = f->w;
			for (i = 0; i < w->num_points; i++) {
				v = DotProduct(w->points[i], temp);
				if (v < mins) {
					mins = v;
				}
				if (v > maxs) {
					maxs = v;
				}
			}

			if (maxs - mins <= subdivide_size) {
				break;
			}

			// split it
			c_subdivide++;

			v = VectorNormalize(temp);

			dist = (mins + subdivide_size - 16.0) / v;

			ClipWindingEpsilon(w, temp, dist, ON_EPSILON, &frontw, &backw);
			if (!frontw || !backw) {
				Mon_SendWinding(MON_ERROR, (const vec3_t *) w->points, w->num_points, "Failed to split polygon");
			}

			f->split[0] = NewFaceFromFace(f);
			f->split[0]->w = frontw;
			f->split[0]->next = node->faces;
			node->faces = f->split[0];

			f->split[1] = NewFaceFromFace(f);
			f->split[1]->w = backw;
			f->split[1]->next = node->faces;
			node->faces = f->split[1];

			SubdivideFace(node, f->split[0]);
			SubdivideFace(node, f->split[1]);
			return;
		}
	}
}

/**
 * @brief
 */
static void SubdivideNodeFaces(node_t *node) {
	face_t *f;

	for (f = node->faces; f; f = f->next) {
		SubdivideFace(node, f);
	}
}

static int32_t c_nodefaces;

/**
 * @brief
 */
static face_t *FaceFromPortal(portal_t *p, int32_t pside) {
	face_t *f;
	side_t *side;

	side = p->side;
	if (!side) {
		return NULL;    // portal does not bridge different visible contents
	}

	if ((side->surf & SURF_NO_DRAW) // Knightmare- nodraw faces
	        && !(side->surf & (SURF_SKY | SURF_HINT | SURF_SKIP))) {
		return NULL;
	}

	f = AllocFace();

	f->texinfo = side->texinfo;
	f->plane_num = (side->plane_num & ~1) | pside;
	f->portal = p;

	if ((p->nodes[pside]->contents & CONTENTS_WINDOW) && VisibleContents(
	            p->nodes[!pside]->contents ^ p->nodes[pside]->contents) == CONTENTS_WINDOW) {
		return NULL;    // don't show insides of windows
	}

	if (pside) {
		f->w = ReverseWinding(p->winding);
		f->contents = p->nodes[1]->contents;
	} else {
		f->w = CopyWinding(p->winding);
		f->contents = p->nodes[0]->contents;
	}
	return f;
}

/**
 * @brief If a portal will make a visible face, mark the side that originally created it.
 *
 *   solid / empty : solid
 *   solid / water : solid
 *   water / empty : water
 *   water / water : none
 */
static void MakeFaces_r(node_t *node) {
	portal_t *p;
	int32_t s;

	// recurse down to leafs
	if (node->plane_num != PLANENUM_LEAF) {
		MakeFaces_r(node->children[0]);
		MakeFaces_r(node->children[1]);

		// merge together all visible faces on the node
		if (!nomerge) {
			MergeNodeFaces(node);
		}
		if (!nosubdivide) {
			SubdivideNodeFaces(node);
		}

		return;
	}
	// solid leafs never have visible faces
	if (node->contents & CONTENTS_SOLID) {
		return;
	}

	// see which portals are valid
	for (p = node->portals; p; p = p->next[s]) {
		s = (p->nodes[1] == node);

		p->face[s] = FaceFromPortal(p, s);
		if (p->face[s]) {
			c_nodefaces++;
			p->face[s]->next = p->onnode->faces;
			p->onnode->faces = p->face[s];
		}
	}
}

/**
 * @brief
 */
void MakeFaces(node_t *node) {
	Com_Verbose("--- MakeFaces ---\n");
	c_merge = 0;
	c_subdivide = 0;
	c_nodefaces = 0;

	MakeFaces_r(node);

	Com_Verbose("%5i node faces\n", c_nodefaces);
	Com_Verbose("%5i merged\n", c_merge);
	Com_Verbose("%5i subdivided\n", c_subdivide);
}
