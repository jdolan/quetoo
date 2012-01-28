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

int c_nofaces;
int c_facenodes;


/*
 * EmitPlanes
 *
 * There is no opportunity to discard planes, because all of the original
 * brushes will be saved in the map.
 */
static void EmitPlanes(void){
	int i;
	d_bsp_plane_t *dp;
	map_plane_t *mp;

	mp = map_planes;
	for(i = 0; i < num_map_planes; i++, mp++){
		dp = &d_bsp.planes[d_bsp.num_planes];
		VectorCopy(mp->normal, dp->normal);
		dp->dist = mp->dist;
		dp->type = mp->type;
		d_bsp.num_planes++;
	}
}


/*
 * EmitLeafFace
 */
static void EmitLeafFace(d_bsp_leaf_t *leaf_p, face_t *f){
	int i;
	int face_num;

	while(f->merged)
		f = f->merged;

	if(f->split[0]){
		EmitLeafFace(leaf_p, f->split[0]);
		EmitLeafFace(leaf_p, f->split[1]);
		return;
	}

	face_num = f->output_number;
	if(face_num == -1)
		return;  // degenerate face

	if(face_num < 0 || face_num >= d_bsp.num_faces)
		Com_Error(ERR_FATAL, "Bad leaf_face\n");

	for(i = leaf_p->first_leaf_face; i < d_bsp.num_leaf_faces; i++)
		if(d_bsp.leaf_faces[i] == face_num)
			break;  // merged out face

	if(i == d_bsp.num_leaf_faces){
		if(d_bsp.num_leaf_faces >= MAX_BSP_LEAF_FACES)
			Com_Error(ERR_FATAL, "MAX_BSP_LEAF_FACES\n");

		d_bsp.leaf_faces[d_bsp.num_leaf_faces] = face_num;
		d_bsp.num_leaf_faces++;
	}
}


/*
 * EmitLeaf
 */
static void EmitLeaf(node_t *node){
	d_bsp_leaf_t *leaf_p;
	portal_t *p;
	int s;
	face_t *f;
	bsp_brush_t *b;
	int i;
	int brush_num;

	// emit a leaf
	if(d_bsp.num_leafs >= MAX_BSP_LEAFS)
		Com_Error(ERR_FATAL, "MAX_BSP_LEAFS\n");

	leaf_p = &d_bsp.leafs[d_bsp.num_leafs];
	d_bsp.num_leafs++;

	leaf_p->contents = node->contents;
	leaf_p->cluster = node->cluster;
	leaf_p->area = node->area;

	// write bounding box info
	VectorCopy(node->mins, leaf_p->mins);
	VectorCopy(node->maxs, leaf_p->maxs);

	// write the leaf_brushes
	leaf_p->first_leaf_brush = d_bsp.num_leaf_brushes;
	for(b = node->brushes; b; b = b->next){

		if(d_bsp.num_leaf_brushes >= MAX_BSP_LEAF_BRUSHES)
			Com_Error(ERR_FATAL, "MAX_BSP_LEAF_BRUSHES\n");

		brush_num = b->original - map_brushes;

		for(i = leaf_p->first_leaf_brush; i < d_bsp.num_leaf_brushes; i++){
			if(d_bsp.leaf_brushes[i] == brush_num)
				break;
		}

		if(i == d_bsp.num_leaf_brushes){
			d_bsp.leaf_brushes[d_bsp.num_leaf_brushes] = brush_num;
			d_bsp.num_leaf_brushes++;
		}
	}
	leaf_p->num_leaf_brushes = d_bsp.num_leaf_brushes - leaf_p->first_leaf_brush;

	// write the leaf_faces
	if(leaf_p->contents & CONTENTS_SOLID)
		return;  // no leaf_faces in solids

	leaf_p->first_leaf_face = d_bsp.num_leaf_faces;

	for(p = node->portals; p; p = p->next[s]){

		s = (p->nodes[1] == node);
		f = p->face[s];

		if(!f)
			continue;  // not a visible portal

		EmitLeafFace(leaf_p, f);
	}

	leaf_p->num_leaf_faces = d_bsp.num_leaf_faces - leaf_p->first_leaf_face;
}


/*
 * EmitFace
 */
static void EmitFace(face_t *f){
	d_bsp_face_t *df;
	int i;
	int e;

	f->output_number = -1;

	if(f->num_points < 3){
		return;  // degenerated
	}
	if(f->merged || f->split[0] || f->split[1]){
		return;  // not a final face
	}
	// save output number so leaffaces can use
	f->output_number = d_bsp.num_faces;

	if(d_bsp.num_faces >= MAX_BSP_FACES)
		Com_Error(ERR_FATAL, "num_faces == MAX_BSP_FACES\n");

	df = &d_bsp.faces[d_bsp.num_faces];
	d_bsp.num_faces++;

	// plane_num is used by qlight, but not quake
	df->plane_num = f->plane_num & (~1);
	df->side = f->plane_num & 1;

	df->first_edge = d_bsp.num_face_edges;
	df->num_edges = f->num_points;
	df->texinfo = f->texinfo;

	for(i = 0; i < f->num_points; i++){

		e = GetEdge2(f->vertexnums[i], f->vertexnums[(i + 1) % f->num_points], f);

		if(d_bsp.num_face_edges >= MAX_BSP_FACE_EDGES)
			Com_Error(ERR_FATAL, "num_surf_edges == MAX_BSP_FACE_EDGES\n");

		d_bsp.face_edges[d_bsp.num_face_edges] = e;
		d_bsp.num_face_edges++;
	}
	df->light_ofs = -1;
}


/*
 * EmitDrawingNode_r
 */
static int EmitDrawNode_r(node_t * node){
	d_bsp_node_t *n;
	face_t *f;
	int i;

	if(node->plane_num == PLANENUM_LEAF){
		EmitLeaf(node);
		return -d_bsp.num_leafs;
	}
	// emit a node
	if(d_bsp.num_nodes == MAX_BSP_NODES)
		Com_Error(ERR_FATAL, "MAX_BSP_NODES\n");

	n = &d_bsp.nodes[d_bsp.num_nodes];
	d_bsp.num_nodes++;

	VectorCopy(node->mins, n->mins);
	VectorCopy(node->maxs, n->maxs);

	if(node->plane_num & 1)
		Com_Error(ERR_FATAL, "EmitDrawNode_r: odd plane_num\n");

	n->plane_num = node->plane_num;
	n->first_face = d_bsp.num_faces;

	if(!node->faces)
		c_nofaces++;
	else
		c_facenodes++;

	for(f = node->faces; f; f = f->next)
		EmitFace(f);

	n->num_faces = d_bsp.num_faces - n->first_face;

	// recursively output the other nodes
	for(i = 0; i < 2; i++){
		if(node->children[i]->plane_num == PLANENUM_LEAF){
			n->children[i] = -(d_bsp.num_leafs + 1);
			EmitLeaf(node->children[i]);
		} else {
			n->children[i] = d_bsp.num_nodes;
			EmitDrawNode_r(node->children[i]);
		}
	}

	return n - d_bsp.nodes;
}


/*
 * WriteBSP
 */
void WriteBSP(node_t *head_node){
	int old_faces;

	c_nofaces = 0;
	c_facenodes = 0;

	Com_Verbose("--- WriteBSP ---\n");

	old_faces = d_bsp.num_faces;

	d_bsp.models[d_bsp.num_models].head_node = EmitDrawNode_r(head_node);
	EmitAreaPortals(head_node);

	Com_Verbose("%5i nodes with faces\n", c_facenodes);
	Com_Verbose("%5i nodes without faces\n", c_nofaces);
	Com_Verbose("%5i faces\n", d_bsp.num_faces - old_faces);
}


/*
 * SetModelNumbers
 */
void SetModelNumbers(void){
	int i;
	int models;
	char value[10];

	// 0 is the world - start at 1
	models = 1;
	for(i = 1; i < num_entities; i++){
		if(entities[i].num_brushes){
			sprintf(value, "*%i", models);
			models++;
			SetKeyValue(&entities[i], "model", value);
		}
	}
}


/*
 * EmitBrushes
 */
static void EmitBrushes(void){
	int i, j, bnum, s, x;
	d_bsp_brush_t *db;
	map_brush_t *b;
	d_bsp_brush_side_t *cp;
	vec3_t normal;
	vec_t dist;
	int plane_num;

	d_bsp.num_brush_sides = 0;
	d_bsp.num_brushes = num_map_brushes;

	for(bnum = 0; bnum < num_map_brushes; bnum++){
		b = &map_brushes[bnum];
		db = &d_bsp.brushes[bnum];

		db->contents = b->contents;
		db->first_side = d_bsp.num_brush_sides;
		db->num_sides = b->num_sides;

		for(j = 0; j < b->num_sides; j++){

			if(d_bsp.num_brush_sides == MAX_BSP_BRUSH_SIDES)
				Com_Error(ERR_FATAL, "MAX_BSP_BRUSH_SIDES\n");

			cp = &d_bsp.brush_sides[d_bsp.num_brush_sides];
			d_bsp.num_brush_sides++;

			cp->plane_num = b->original_sides[j].plane_num;
			cp->surf_num = b->original_sides[j].texinfo;
		}

		// add any axis planes not contained in the brush to bevel off corners
		for(x = 0; x < 3; x++){
			for(s = -1; s <= 1; s += 2){
				// add the plane
				VectorCopy(vec3_origin, normal);
				normal[x] = (float)s;

				if(s == -1)
					dist = -b->mins[x];
				else
					dist = b->maxs[x];

				plane_num = FindFloatPlane(normal, dist);

				for(i = 0; i < b->num_sides; i++)
					if(b->original_sides[i].plane_num == plane_num)
						break;

				if(i == b->num_sides){
					if(d_bsp.num_brush_sides >= MAX_BSP_BRUSH_SIDES)
						Com_Error(ERR_FATAL, "MAX_BSP_BRUSH_SIDES\n");

					d_bsp.brush_sides[d_bsp.num_brush_sides].plane_num = plane_num;
					d_bsp.brush_sides[d_bsp.num_brush_sides].surf_num =
					    d_bsp.brush_sides[d_bsp.num_brush_sides - 1].surf_num;
					d_bsp.num_brush_sides++;
					db->num_sides++;
				}
			}
		}
	}
}


/*
 * BeginBSPFile
 */
void BeginBSPFile(void){

	// edge 0 is not used, because 0 can't be negated
	d_bsp.num_edges = 1;

	// leave vertex 0 as an error
	d_bsp.num_vertexes = 1;
	d_bsp.num_normals = 1;

	// leave leaf 0 as an error
	d_bsp.num_leafs = 1;
	d_bsp.leafs[0].contents = CONTENTS_SOLID;
}


/*
 * EndBSPFile
 */
void EndBSPFile(void){

	EmitBrushes();
	EmitPlanes();

	UnparseEntities();

	// now that the verts have been resolved, align the normals count
	d_bsp.num_normals = d_bsp.num_vertexes;

	// write the map
	Com_Verbose("Writing %s\n", bsp_name);
	WriteBSPFile(bsp_name);
}


/*
 * BeginModel
 */
extern int first_bsp_model_edge;
void BeginModel(void){
	d_bsp_model_t *mod;
	int start, end;
	map_brush_t *b;
	int j;
	entity_t *e;
	vec3_t mins, maxs;

	if(d_bsp.num_models == MAX_BSP_MODELS)
		Com_Error(ERR_FATAL, "MAX_BSP_MODELS\n");
	mod = &d_bsp.models[d_bsp.num_models];

	mod->first_face = d_bsp.num_faces;

	first_bsp_model_edge = d_bsp.num_edges;

	// bound the brushes
	e = &entities[entity_num];

	start = e->first_brush;
	end = start + e->num_brushes;
	ClearBounds(mins, maxs);

	for(j = start; j < end; j++){
		b = &map_brushes[j];
		if(!b->num_sides)
			continue;				  // not a real brush (origin brush)
		AddPointToBounds(b->mins, mins, maxs);
		AddPointToBounds(b->maxs, mins, maxs);
	}

	VectorCopy(mins, mod->mins);
	VectorCopy(maxs, mod->maxs);
}


/*
 * EndModel
 */
void EndModel(void){
	d_bsp_model_t *mod;

	mod = &d_bsp.models[d_bsp.num_models];

	mod->num_faces = d_bsp.num_faces - mod->first_face;

	d_bsp.num_models++;
}
