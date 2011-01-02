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
 * ONLY SAVE OUT PLANES THAT ARE ACTUALLY USED AS NODES
 */

/*
 * EmitPlanes
 *
 * There is no oportunity to discard planes, because all of the original
 * brushes will be saved in the map.
 */
static void EmitPlanes(void){
	int i;
	d_bsp_plane_t *dp;
	plane_t *mp;

	mp = mapplanes;
	for(i = 0; i < nummapplanes; i++, mp++){
		dp = &dplanes[num_planes];
		VectorCopy(mp->normal, dp->normal);
		dp->dist = mp->dist;
		dp->type = mp->type;
		num_planes++;
	}
}


/*
 * EmitMarkFace
 */
static void EmitMarkFace(d_bsp_leaf_t *leaf_p, face_t *f){
	int i;
	int facenum;

	while(f->merged)
		f = f->merged;

	if(f->split[0]){
		EmitMarkFace(leaf_p, f->split[0]);
		EmitMarkFace(leaf_p, f->split[1]);
		return;
	}

	facenum = f->outputnumber;
	if(facenum == -1)
		return;  // degenerate face

	if(facenum < 0 || facenum >= num_faces)
		Com_Error(ERR_FATAL, "Bad leafface\n");
	for(i = leaf_p->first_leaf_face; i < num_leaf_faces; i++)
		if(dleaffaces[i] == facenum)
			break;  // merged out face
	if(i == num_leaf_faces){
		if(num_leaf_faces >= MAX_BSP_LEAFFACES)
			Com_Error(ERR_FATAL, "MAX_BSP_LEAFFACES\n");

		dleaffaces[num_leaf_faces] = facenum;
		num_leaf_faces++;
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
	bspbrush_t *b;
	int i;
	int brushnum;

	// emit a leaf
	if(num_leafs >= MAX_BSP_LEAFS)
		Com_Error(ERR_FATAL, "MAX_BSP_LEAFS\n");

	leaf_p = &dleafs[num_leafs];
	num_leafs++;

	leaf_p->contents = node->contents;
	leaf_p->cluster = node->cluster;
	leaf_p->area = node->area;

	// write bounding box info
	VectorCopy(node->mins, leaf_p->mins);
	VectorCopy(node->maxs, leaf_p->maxs);

	// write the leafbrushes
	leaf_p->first_leaf_brush = num_leaf_brushes;
	for(b = node->brushlist; b; b = b->next){
		if(num_leaf_brushes >= MAX_BSP_LEAFBRUSHES)
			Com_Error(ERR_FATAL, "MAX_BSP_LEAFBRUSHES\n");

		brushnum = b->original - mapbrushes;
		for(i = leaf_p->first_leaf_brush; i < num_leaf_brushes; i++)
			if(dleafbrushes[i] == brushnum)
				break;
		if(i == num_leaf_brushes){
			dleafbrushes[num_leaf_brushes] = brushnum;
			num_leaf_brushes++;
		}
	}
	leaf_p->num_leaf_brushes = num_leaf_brushes - leaf_p->first_leaf_brush;

	// write the leaffaces
	if(leaf_p->contents & CONTENTS_SOLID)
		return;  // no leaffaces in solids

	leaf_p->first_leaf_face = num_leaf_faces;

	for(p = node->portals; p; p = p->next[s]){
		s = (p->nodes[1] == node);
		f = p->face[s];
		if(!f)
			continue;  // not a visible portal

		EmitMarkFace(leaf_p, f);
	}

	leaf_p->num_leaf_faces = num_leaf_faces - leaf_p->first_leaf_face;
}


/*
 * EmitFace
 */
static void EmitFace(face_t *f){
	d_bsp_face_t *df;
	int i;
	int e;

	f->outputnumber = -1;

	if(f->numpoints < 3){
		return;  // degenerated
	}
	if(f->merged || f->split[0] || f->split[1]){
		return;  // not a final face
	}
	// save output number so leaffaces can use
	f->outputnumber = num_faces;

	if(num_faces >= MAX_BSP_FACES)
		Com_Error(ERR_FATAL, "num_faces == MAX_BSP_FACES\n");
	df = &dfaces[num_faces];
	num_faces++;

	// plane_num is used by qlight, but not quake
	df->plane_num = f->plane_num & (~1);
	df->side = f->plane_num & 1;

	df->first_edge = num_surfedges;
	df->num_edges = f->numpoints;
	df->texinfo = f->texinfo;
	for(i = 0; i < f->numpoints; i++){
		e = GetEdge2(f->vertexnums[i], f->vertexnums[(i + 1) % f->numpoints], f);
		if(num_surfedges >= MAX_BSP_SURFEDGES)
			Com_Error(ERR_FATAL, "num_surfedges == MAX_BSP_SURFEDGES\n");
		dsurfedges[num_surfedges] = e;
		num_surfedges++;
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
		return -num_leafs;
	}
	// emit a node
	if(num_nodes == MAX_BSP_NODES)
		Com_Error(ERR_FATAL, "MAX_BSP_NODES\n");
	n = &dnodes[num_nodes];
	num_nodes++;

	VectorCopy(node->mins, n->mins);
	VectorCopy(node->maxs, n->maxs);

	if(node->plane_num & 1)
		Com_Error(ERR_FATAL, "EmitDrawNode_r: odd plane_num\n");
	n->plane_num = node->plane_num;
	n->first_face = num_faces;

	if(!node->faces)
		c_nofaces++;
	else
		c_facenodes++;

	for(f = node->faces; f; f = f->next)
		EmitFace(f);

	n->num_faces = num_faces - n->first_face;


	// recursively output the other nodes
	for(i = 0; i < 2; i++){
		if(node->children[i]->plane_num == PLANENUM_LEAF){
			n->children[i] = -(num_leafs + 1);
			EmitLeaf(node->children[i]);
		} else {
			n->children[i] = num_nodes;
			EmitDrawNode_r(node->children[i]);
		}
	}

	return n - dnodes;
}


/*
 * WriteBSP
 */
void WriteBSP(node_t * head_node){
	int oldfaces;

	c_nofaces = 0;
	c_facenodes = 0;

	Com_Verbose("--- WriteBSP ---\n");

	oldfaces = num_faces;
	dmodels[nummodels].head_node = EmitDrawNode_r(head_node);
	EmitAreaPortals(head_node);

	Com_Verbose("%5i nodes with faces\n", c_facenodes);
	Com_Verbose("%5i nodes without faces\n", c_nofaces);
	Com_Verbose("%5i faces\n", num_faces - oldfaces);
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
		if(entities[i].numbrushes){
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
	mapbrush_t *b;
	d_bsp_brush_side_t *cp;
	vec3_t normal;
	vec_t dist;
	int plane_num;

	numbrushsides = 0;
	numbrushes = nummapbrushes;

	for(bnum = 0; bnum < nummapbrushes; bnum++){
		b = &mapbrushes[bnum];
		db = &dbrushes[bnum];

		db->contents = b->contents;
		db->firstside = numbrushsides;
		db->numsides = b->numsides;
		for(j = 0; j < b->numsides; j++){
			if(numbrushsides == MAX_BSP_BRUSHSIDES)
				Com_Error(ERR_FATAL, "MAX_BSP_BRUSHSIDES\n");
			cp = &dbrushsides[numbrushsides];
			numbrushsides++;
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
				for(i = 0; i < b->numsides; i++)
					if(b->original_sides[i].plane_num == plane_num)
						break;
				if(i == b->numsides){
					if(numbrushsides >= MAX_BSP_BRUSHSIDES)
						Com_Error(ERR_FATAL, "MAX_BSP_BRUSHSIDES\n");

					dbrushsides[numbrushsides].plane_num = plane_num;
					dbrushsides[numbrushsides].surf_num =
					    dbrushsides[numbrushsides - 1].surf_num;
					numbrushsides++;
					db->numsides++;
				}
			}
		}
	}
}


/*
 * BeginBSPFile
 */
void BeginBSPFile(void){
	// these values may actually be initialized
	// if the file existed when loaded, so clear them explicitly
	nummodels = 0;
	num_faces = 0;
	num_nodes = 0;
	numbrushsides = 0;
	num_vertexes = 0;
	numnormals = 0;
	num_leaf_faces = 0;
	num_leaf_brushes = 0;
	num_surfedges = 0;

	// edge 0 is not used, because 0 can't be negated
	num_edges = 1;

	// leave vertex 0 as an error
	num_vertexes = 1;
	numnormals = 1;

	// leave leaf 0 as an error
	num_leafs = 1;
	dleafs[0].contents = CONTENTS_SOLID;
}


/*
 * EndBSPFile
 */
void EndBSPFile(void){

	EmitBrushes();
	EmitPlanes();
	UnparseEntities();

	// now that the verts have been resolved, align the normals count
	memset(dnormals, 0, sizeof(dnormals));
	numnormals = num_vertexes;

	// write the map
	Com_Verbose("Writing %s\n", bspname);
	WriteBSPFile(bspname);
}


/*
 * BeginModel
 */
extern int firstmodeledge;
void BeginModel(void){
	d_bsp_model_t *mod;
	int start, end;
	mapbrush_t *b;
	int j;
	entity_t *e;
	vec3_t mins, maxs;

	if(nummodels == MAX_BSP_MODELS)
		Com_Error(ERR_FATAL, "MAX_BSP_MODELS\n");
	mod = &dmodels[nummodels];

	mod->first_face = num_faces;

	firstmodeledge = num_edges;

	// bound the brushes
	e = &entities[entity_num];

	start = e->firstbrush;
	end = start + e->numbrushes;
	ClearBounds(mins, maxs);

	for(j = start; j < end; j++){
		b = &mapbrushes[j];
		if(!b->numsides)
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

	mod = &dmodels[nummodels];

	mod->num_faces = num_faces - mod->first_face;

	nummodels++;
}
