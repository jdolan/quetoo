/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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
#define	POINT_EPSILON		0.5
#define	OFF_EPSILON			0.5

static int c_merge;
static int c_subdivide;

static int c_totalverts;
static int c_uniqueverts;
static int c_degenerate;
static int c_tjunctions;
static int c_faceoverflows;
static int c_facecollapse;
static int c_badstartverts;

#define	MAX_SUPERVERTS	512
static int superverts[MAX_SUPERVERTS];
static int num_superverts;

static face_t *edge_faces[MAX_BSP_EDGES][2];
int first_bsp_model_edge = 1;

static int c_tryedges;

static vec3_t edge_dir;
static vec3_t edge_start;

static int num_edge_verts;
static int edge_verts[MAX_BSP_VERTS];

int subdivide_size = 1024;

#define	HASH_SIZE	64

static int vertex_chain[MAX_BSP_VERTS]; // the next vertex in a hash chain
static int hash_verts[HASH_SIZE * HASH_SIZE]; // a vertex number, or 0 for no verts


/*
 * HashVec
 */
static unsigned HashVec(const vec3_t vec) {
	const int x = (4096 + (int) (vec[0] + 0.5)) >> 7;
	const int y = (4096 + (int) (vec[1] + 0.5)) >> 7;

	if (x < 0 || x >= HASH_SIZE || y < 0 || y >= HASH_SIZE)
		Com_Error(ERR_FATAL, "HashVec: point outside valid range\n");

	return y * HASH_SIZE + x;
}

/*
 * GetVertex
 *
 * Uses hashing
 */
static int GetVertexnum(const vec3_t in) {
	int h;
	int i;
	float *p;
	vec3_t vert;
	int vnum;

	c_totalverts++;

	for (i = 0; i < 3; i++) {
		const float f = floor(in[i] + 0.5);

		if (fabs(in[i] - f) < INTEGRAL_EPSILON)
			vert[i] = f;
		else
			vert[i] = in[i];
	}

	h = HashVec(vert);

	for (vnum = hash_verts[h]; vnum; vnum = vertex_chain[vnum]) {
		p = d_bsp.vertexes[vnum].point;
		if (fabs(p[0] - vert[0]) < POINT_EPSILON && fabs(p[1] - vert[1])
				< POINT_EPSILON && fabs(p[2] - vert[2]) < POINT_EPSILON)
			return vnum;
	}

	// emit a vertex
	if (d_bsp.num_vertexes == MAX_BSP_VERTS)
		Com_Error(ERR_FATAL, "num_vertexes == MAX_BSP_VERTS\n");

	d_bsp.vertexes[d_bsp.num_vertexes].point[0] = vert[0];
	d_bsp.vertexes[d_bsp.num_vertexes].point[1] = vert[1];
	d_bsp.vertexes[d_bsp.num_vertexes].point[2] = vert[2];

	vertex_chain[d_bsp.num_vertexes] = hash_verts[h];
	hash_verts[h] = d_bsp.num_vertexes;

	c_uniqueverts++;

	d_bsp.num_vertexes++;

	return d_bsp.num_vertexes - 1;
}

static int c_faces;

/*
 * AllocFace
 */
static face_t *AllocFace(void) {
	face_t *f;

	f = Z_Malloc(sizeof(*f));
	memset(f, 0, sizeof(*f));
	c_faces++;

	return f;
}

/*
 * NewFaceFromFace
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

/*
 * FreeFace
 */
void FreeFace(face_t *f) {
	if (f->w)
		FreeWinding(f->w);
	Z_Free(f);
	c_faces--;
}

/*
 * FaceFromSuperverts
 *
 * The faces vertexes have beeb added to the superverts[] array,
 * and there may be more there than can be held in a face (MAXEDGES).
 *
 * If less, the faces vertexnums[] will be filled in, otherwise
 * face will reference a tree of split[] faces until all of the
 * vertexnums can be added.
 *
 * superverts[base] will become face->vertexnums[0], and the others
 * will be circularly filled in.
 */
static void FaceFromSuperverts(node_t *node, face_t *f, int base) {
	face_t *newf;
	int remaining;
	int i;

	remaining = num_superverts;
	while (remaining > MAXEDGES) { // must split into two faces, because of vertex overload
		c_faceoverflows++;

		newf = f->split[0] = NewFaceFromFace(f);
		newf = f->split[0];
		newf->next = node->faces;
		node->faces = newf;

		newf->num_points = MAXEDGES;
		for (i = 0; i < MAXEDGES; i++)
			newf->vertexnums[i] = superverts[(i + base) % num_superverts];

		f->split[1] = NewFaceFromFace(f);
		f = f->split[1];
		f->next = node->faces;
		node->faces = f;

		remaining -= (MAXEDGES - 2);
		base = (base + MAXEDGES - 1) % num_superverts;
	}

	// copy the vertexes back to the face
	f->num_points = remaining;
	for (i = 0; i < remaining; i++)
		f->vertexnums[i] = superverts[(i + base) % num_superverts];
}

/*
 * EmitFaceVertexes
 */
static void EmitFaceVertexes(node_t *node, face_t *f) {
	winding_t *w;
	int i;

	if (f->merged || f->split[0] || f->split[1])
		return;

	w = f->w;
	for (i = 0; i < w->numpoints; i++) {
		if (noweld) { // make every point unique
			if (d_bsp.num_vertexes == MAX_BSP_VERTS)
				Com_Error(ERR_FATAL, "MAX_BSP_VERTS\n");
			superverts[i] = d_bsp.num_vertexes;
			VectorCopy(w->p[i], d_bsp.vertexes[d_bsp.num_vertexes].point);
			d_bsp.num_vertexes++;
			c_uniqueverts++;
			c_totalverts++;
		} else
			superverts[i] = GetVertexnum(w->p[i]);
	}
	num_superverts = w->numpoints;

	// this may fragment the face if > MAXEDGES
	FaceFromSuperverts(node, f, 0);
}

/*
 * EmitVertexes_r
 */
static void EmitVertexes_r(node_t *node) {
	int i;
	face_t *f;

	if (node->plane_num == PLANENUM_LEAF)
		return;

	for (f = node->faces; f; f = f->next) {
		EmitFaceVertexes(node, f);
	}

	for (i = 0; i < 2; i++)
		EmitVertexes_r(node->children[i]);
}

/*
 * FindEdgeVerts
 *
 * Forced a dumb check of everything
 */
static void FindEdgeVerts(vec3_t v1, vec3_t v2) {
	int i;

	num_edge_verts = d_bsp.num_vertexes - 1;
	for (i = 0; i < num_edge_verts; i++)
		edge_verts[i] = i + 1;
}

/*
 * TestEdge
 *
 * Can be recursively reentered
 */
static void TestEdge(vec_t start, vec_t end, int p1, int p2, int startvert) {
	int k;
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

	for (k = startvert; k < num_edge_verts; k++) {
		const int j = edge_verts[k];
		if (j == p1 || j == p2)
			continue;

		VectorCopy(d_bsp.vertexes[j].point, p);

		VectorSubtract(p, edge_start, delta);
		dist = DotProduct(delta, edge_dir);
		if (dist <= start || dist >= end)
			continue; // off an end

		VectorMA(edge_start, dist, edge_dir, exact);
		VectorSubtract(p, exact, off);
		error = VectorLength(off);

		if (fabs(error) > OFF_EPSILON)
			continue; // not on the edge

		// break the edge
		c_tjunctions++;
		TestEdge(start, dist, p1, j, k + 1);
		TestEdge(dist, end, j, p2, k + 1);
		return;
	}

	// the edge p1 to p2 is now free of tjunctions
	if (num_superverts >= MAX_SUPERVERTS)
		Com_Error(ERR_FATAL, "MAX_SUPERVERTS\n");
	superverts[num_superverts] = p1;
	num_superverts++;
}

/*
 * FixFaceEdges
 */
static void FixFaceEdges(node_t *node, face_t *f) {
	int i;
	vec3_t e2;
	vec_t len;
	int count[MAX_SUPERVERTS], start[MAX_SUPERVERTS];
	int base;

	if (f->merged || f->split[0] || f->split[1])
		return;

	num_superverts = 0;

	for (i = 0; i < f->num_points; i++) {
		const int p1 = f->vertexnums[i];
		const int p2 = f->vertexnums[(i + 1) % f->num_points];

		VectorCopy(d_bsp.vertexes[p1].point, edge_start);
		VectorCopy(d_bsp.vertexes[p2].point, e2);

		FindEdgeVerts(edge_start, e2);

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
		if (count[i] == 1 && count[(i + f->num_points - 1) % f->num_points]
				== 1)
			break;
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

/*
 * FixEdges_r
 */
static void FixEdges_r(node_t *node) {
	int i;
	face_t *f;

	if (node->plane_num == PLANENUM_LEAF)
		return;

	for (f = node->faces; f; f = f->next)
		FixFaceEdges(node, f);

	for (i = 0; i < 2; i++)
		FixEdges_r(node->children[i]);
}

/*
 * FixTjuncs
 */
void FixTjuncs(node_t *head_node) {
	// snap and merge all vertexes
	Com_Verbose("---- snap verts ----\n");
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
	if (!notjunc)
		FixEdges_r(head_node);
	Com_Verbose("%5i edges degenerated\n", c_degenerate);
	Com_Verbose("%5i faces degenerated\n", c_facecollapse);
	Com_Verbose("%5i edges added by tjunctions\n", c_tjunctions);
	Com_Verbose("%5i faces added by tjunctions\n", c_faceoverflows);
	Com_Verbose("%5i bad start verts\n", c_badstartverts);
}

/*
 * GetEdge2
 *
 * Called by writebsp.  Don't allow four way edges
 */
int GetEdge2(int v1, int v2, face_t * f) {
	d_bsp_edge_t *edge;
	int i;

	c_tryedges++;

	if (!noshare) {
		for (i = first_bsp_model_edge; i < d_bsp.num_edges; i++) {
			edge = &d_bsp.edges[i];
			if (v1 == edge->v[1] && v2 == edge->v[0]
					&& edge_faces[i][0]->contents == f->contents) {
				if (edge_faces[i][1])
					continue;
				edge_faces[i][1] = f;
				return -i;
			}
		}
	}
	// emit an edge
	if (d_bsp.num_edges >= MAX_BSP_EDGES)
		Com_Error(ERR_FATAL, "num_edges == MAX_BSP_EDGES\n");
	edge = &d_bsp.edges[d_bsp.num_edges];
	edge->v[0] = v1;
	edge->v[1] = v2;
	edge_faces[d_bsp.num_edges][0] = f;
	d_bsp.num_edges++;

	return d_bsp.num_edges - 1;
}

/*
 *
 * FACE MERGING
 *
 */

#define	CONTINUOUS_EPSILON	0.001

/*
 * TryMergeWinding
 *
 * If two polygons share a common edge and the edges that meet at the
 * common points are both inside the other polygons, merge them
 *
 * Returns NULL if the faces couldn't be merged, or the new face.
 * The originals will NOT be freed.
 */
static winding_t *TryMergeWinding(winding_t * f1, winding_t * f2,
		const vec3_t planenormal) {
	vec_t *p1, *p2, *back;
	winding_t *newf;
	int i, j, k, l;
	vec3_t normal, delta;
	vec_t dot;
	boolean_t keep1, keep2;

	// find a common edge
	p1 = p2 = NULL;
	j = 0;

	for (i = 0; i < f1->numpoints; i++) {
		p1 = f1->p[i];
		p2 = f1->p[(i + 1) % f1->numpoints];
		for (j = 0; j < f2->numpoints; j++) {
			const vec_t *p3 = f2->p[j];
			const vec_t *p4 = f2->p[(j + 1) % f2->numpoints];
			for (k = 0; k < 3; k++) {
				if (fabs(p1[k] - p4[k]) > EQUAL_EPSILON)
					break;
				if (fabs(p2[k] - p3[k]) > EQUAL_EPSILON)
					break;
			}
			if (k == 3)
				break;
		}
		if (j < f2->numpoints)
			break;
	}

	if (i == f1->numpoints)
		return NULL; // no matching edges

	// check slope of connected lines
	// if the slopes are colinear, the point can be removed
	back = f1->p[(i + f1->numpoints - 1) % f1->numpoints];
	VectorSubtract(p1, back, delta);
	CrossProduct(planenormal, delta, normal);
	VectorNormalize(normal);

	back = f2->p[(j + 2) % f2->numpoints];
	VectorSubtract(back, p1, delta);
	dot = DotProduct(delta, normal);
	if (dot > CONTINUOUS_EPSILON)
		return NULL; // not a convex polygon
	keep1 = (boolean_t) (dot < -CONTINUOUS_EPSILON);

	back = f1->p[(i + 2) % f1->numpoints];
	VectorSubtract(back, p2, delta);
	CrossProduct(planenormal, delta, normal);
	VectorNormalize(normal);

	back = f2->p[(j + f2->numpoints - 1) % f2->numpoints];
	VectorSubtract(back, p2, delta);
	dot = DotProduct(delta, normal);
	if (dot > CONTINUOUS_EPSILON)
		return NULL; // not a convex polygon
	keep2 = (boolean_t) (dot < -CONTINUOUS_EPSILON);

	// build the new polygon
	newf = AllocWinding(f1->numpoints + f2->numpoints);

	// copy first polygon
	for (k = (i + 1) % f1->numpoints; k != i; k = (k + 1) % f1->numpoints) {
		if (k == (i + 1) % f1->numpoints && !keep2)
			continue;

		VectorCopy(f1->p[k], newf->p[newf->numpoints]);
		newf->numpoints++;
	}

	// copy second polygon
	for (l = (j + 1) % f2->numpoints; l != j; l = (l + 1) % f2->numpoints) {
		if (l == (j + 1) % f2->numpoints && !keep1)
			continue;
		VectorCopy(f2->p[l], newf->p[newf->numpoints]);
		newf->numpoints++;
	}

	return newf;
}

/*
 * TryMerge
 *
 * If two polygons share a common edge and the edges that meet at the
 * common points are both inside the other polygons, merge them
 *
 * Returns NULL if the faces couldn't be merged, or the new face.
 * The originals will NOT be freed.
 */
static face_t *TryMerge(face_t * f1, face_t * f2, const vec3_t planenormal) {
	face_t *newf;
	winding_t *nw;

	if (!f1->w || !f2->w)
		return NULL;
	if (f1->texinfo != f2->texinfo)
		return NULL;
	if (f1->plane_num != f2->plane_num) // on front and back sides
		return NULL;
	if (f1->contents != f2->contents)
		return NULL;

	nw = TryMergeWinding(f1->w, f2->w, planenormal);
	if (!nw)
		return NULL;

	c_merge++;
	newf = NewFaceFromFace(f1);
	newf->w = nw;

	f1->merged = newf;
	f2->merged = newf;

	return newf;
}

/*
 * MergeNodeFaces
 */
static void MergeNodeFaces(node_t * node) {
	face_t *f1, *f2, *end;
	face_t *merged;
	const map_plane_t *plane;

	plane = &map_planes[node->plane_num];
	merged = NULL;

	for (f1 = node->faces; f1; f1 = f1->next) {
		if (f1->merged || f1->split[0] || f1->split[1])
			continue;
		for (f2 = node->faces; f2 != f1; f2 = f2->next) {
			if (f2->merged || f2->split[0] || f2->split[1])
				continue;
			merged = TryMerge(f1, f2, plane->normal);
			if (!merged)
				continue;

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

/*
 * SubdivideFace
 *
 * Chop up faces that are larger than we want in the surface cache
 */
static void SubdivideFace(node_t *node, face_t * f) {
	float mins, maxs;
	vec_t v;
	int axis, i;
	const d_bsp_texinfo_t *tex;
	vec3_t temp;
	vec_t dist;
	winding_t *w, *frontw, *backw;

	if (f->merged)
		return;

	// special (non-surface cached) faces don't need subdivision
	tex = &d_bsp.texinfo[f->texinfo];

	if (tex->flags & (SURF_SKY | SURF_WARP))
		return;

	for (axis = 0; axis < 2; axis++) {
		while (true) {
			mins = 999999;
			maxs = -999999;

			VectorCopy(tex->vecs[axis], temp);
			w = f->w;
			for (i = 0; i < w->numpoints; i++) {
				v = DotProduct(w->p[i], temp);
				if (v < mins)
					mins = v;
				if (v > maxs)
					maxs = v;
			}

			if (maxs - mins <= subdivide_size)
				break;

			// split it
			c_subdivide++;

			v = VectorNormalize(temp);

			dist = (mins + subdivide_size - 16.0) / v;

			ClipWindingEpsilon(w, temp, dist, ON_EPSILON, &frontw, &backw);
			if (!frontw || !backw)
				Com_Error(ERR_FATAL,
						"SubdivideFace: didn't split the polygon\n");

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

/*
 * SubdivideNodeFaces
 */
static void SubdivideNodeFaces(node_t * node) {
	face_t *f;

	for (f = node->faces; f; f = f->next) {
		SubdivideFace(node, f);
	}
}

static int c_nodefaces;

/*
 * FaceFromPortal
 */
static face_t *FaceFromPortal(portal_t * p, int pside) {
	face_t *f;
	side_t *side;

	side = p->side;
	if (!side)
		return NULL; // portal does not bridge different visible contents

	if ((side->surf & SURF_NO_DRAW) // Knightmare- nodraw faces
			&& !(side->surf & (SURF_SKY | SURF_HINT | SURF_SKIP)))
		return NULL;

	f = AllocFace();

	f->texinfo = side->texinfo;
	f->plane_num = (side->plane_num & ~1) | pside;
	f->portal = p;

	if ((p->nodes[pside]->contents & CONTENTS_WINDOW) && VisibleContents(
			p->nodes[!pside]->contents ^ p->nodes[pside]->contents)
			== CONTENTS_WINDOW)
		return NULL; // don't show insides of windows

	if (pside) {
		f->w = ReverseWinding(p->winding);
		f->contents = p->nodes[1]->contents;
	} else {
		f->w = CopyWinding(p->winding);
		f->contents = p->nodes[0]->contents;
	}
	return f;
}

/*
 * MakeFaces_r
 *
 * If a portal will make a visible face, mark the side that originally created it.
 *
 *   solid / empty : solid
 *   solid / water : solid
 *   water / empty : water
 *   water / water : none
 */
static void MakeFaces_r(node_t * node) {
	portal_t *p;
	int s;

	// recurse down to leafs
	if (node->plane_num != PLANENUM_LEAF) {
		MakeFaces_r(node->children[0]);
		MakeFaces_r(node->children[1]);

		// merge together all visible faces on the node
		if (!nomerge)
			MergeNodeFaces(node);
		if (!nosubdivide)
			SubdivideNodeFaces(node);

		return;
	}
	// solid leafs never have visible faces
	if (node->contents & CONTENTS_SOLID)
		return;

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

/*
 * MakeFaces
 */
void MakeFaces(node_t * node) {
	Com_Verbose("--- MakeFaces ---\n");
	c_merge = 0;
	c_subdivide = 0;
	c_nodefaces = 0;

	MakeFaces_r(node);

	Com_Verbose("%5i node faces\n", c_nodefaces);
	Com_Verbose("%5i merged\n", c_merge);
	Com_Verbose("%5i subdivided\n", c_subdivide);
}
