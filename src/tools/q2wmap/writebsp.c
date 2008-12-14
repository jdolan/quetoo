/*
* Copyright(c) 1997-2001 Id Software, Inc.
* Copyright(c) 2002 The Quakeforge Project.
* Copyright(c) 2006 Quake2World.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or(at your option) any later version.
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
ONLY SAVE OUT PLANES THAT ARE ACTUALLY USED AS NODES
*/

/*
EmitPlanes

There is no oportunity to discard planes, because all of the original
brushes will be saved in the map.
*/
static void EmitPlanes(void){
	int i;
	dplane_t *dp;
	plane_t *mp;

	mp = mapplanes;
	for(i = 0; i < nummapplanes; i++, mp++){
		dp = &dplanes[numplanes];
		VectorCopy(mp->normal, dp->normal);
		dp->dist = mp->dist;
		dp->type = mp->type;
		numplanes++;
	}
}


/*
EmitMarkFace
*/
static void EmitMarkFace(dleaf_t *leaf_p, face_t *f){
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

	if(facenum < 0 || facenum >= numfaces)
		Error("Bad leafface\n");
	for(i = leaf_p->firstleafface; i < numleaffaces; i++)
		if(dleaffaces[i] == facenum)
			break;  // merged out face
	if(i == numleaffaces){
		if(numleaffaces >= MAX_BSP_LEAFFACES)
			Error("MAX_BSP_LEAFFACES\n");

		dleaffaces[numleaffaces] = facenum;
		numleaffaces++;
	}
}


/*
EmitLeaf
*/
static void EmitLeaf(node_t *node){
	dleaf_t *leaf_p;
	portal_t *p;
	int s;
	face_t *f;
	bspbrush_t *b;
	int i;
	int brushnum;

	// emit a leaf
	if(numleafs >= MAX_BSP_LEAFS)
		Error("MAX_BSP_LEAFS\n");

	leaf_p = &dleafs[numleafs];
	numleafs++;

	leaf_p->contents = node->contents;
	leaf_p->cluster = node->cluster;
	leaf_p->area = node->area;

	// write bounding box info
	VectorCopy((short)node->mins, leaf_p->mins);
	VectorCopy((short)node->maxs, leaf_p->maxs);

	// write the leafbrushes
	leaf_p->firstleafbrush = numleafbrushes;
	for(b = node->brushlist; b; b = b->next){
		if(numleafbrushes >= MAX_BSP_LEAFBRUSHES)
			Error("MAX_BSP_LEAFBRUSHES\n");

		brushnum = b->original - mapbrushes;
		for(i = leaf_p->firstleafbrush; i < numleafbrushes; i++)
			if(dleafbrushes[i] == brushnum)
				break;
		if(i == numleafbrushes){
			dleafbrushes[numleafbrushes] = brushnum;
			numleafbrushes++;
		}
	}
	leaf_p->numleafbrushes = numleafbrushes - leaf_p->firstleafbrush;

	// write the leaffaces
	if(leaf_p->contents & CONTENTS_SOLID)
		return;  // no leaffaces in solids

	leaf_p->firstleafface = numleaffaces;

	for(p = node->portals; p; p = p->next[s]){
		s = (p->nodes[1] == node);
		f = p->face[s];
		if(!f)
			continue;  // not a visible portal

		EmitMarkFace(leaf_p, f);
	}

	leaf_p->numleaffaces = numleaffaces - leaf_p->firstleafface;
}


/*
EmitFace
*/
static void EmitFace(face_t *f){
	dface_t *df;
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
	f->outputnumber = numfaces;

	if(numfaces >= MAX_BSP_FACES)
		Error("numfaces == MAX_BSP_FACES\n");
	df = &dfaces[numfaces];
	numfaces++;

	// planenum is used by qlight, but not quake
	df->planenum = f->planenum & (~1);
	df->side = f->planenum & 1;

	df->firstedge = numsurfedges;
	df->numedges = f->numpoints;
	df->texinfo = f->texinfo;
	for(i = 0; i < f->numpoints; i++){
		e = GetEdge2(f->vertexnums[i], f->vertexnums[(i + 1) % f->numpoints], f);
		if(numsurfedges >= MAX_BSP_SURFEDGES)
			Error("numsurfedges == MAX_BSP_SURFEDGES\n");
		dsurfedges[numsurfedges] = e;
		numsurfedges++;
	}
	df->lightofs = -1;
}


/*
EmitDrawingNode_r
*/
static int EmitDrawNode_r(node_t * node){
	dnode_t *n;
	face_t *f;
	int i;

	if(node->planenum == PLANENUM_LEAF){
		EmitLeaf(node);
		return -numleafs;
	}
	// emit a node
	if(numnodes == MAX_BSP_NODES)
		Error("MAX_BSP_NODES\n");
	n = &dnodes[numnodes];
	numnodes++;

	VectorCopy((short)node->mins, n->mins);
	VectorCopy((short)node->maxs, n->maxs);

	if(node->planenum & 1)
		Error("EmitDrawNode_r: odd planenum\n");
	n->planenum = node->planenum;
	n->firstface = numfaces;

	if(!node->faces)
		c_nofaces++;
	else
		c_facenodes++;

	for(f = node->faces; f; f = f->next)
		EmitFace(f);

	n->numfaces = numfaces - n->firstface;


	// recursively output the other nodes
	for(i = 0; i < 2; i++){
		if(node->children[i]->planenum == PLANENUM_LEAF){
			n->children[i] = -(numleafs + 1);
			EmitLeaf(node->children[i]);
		} else {
			n->children[i] = numnodes;
			EmitDrawNode_r(node->children[i]);
		}
	}

	return n - dnodes;
}


/*
WriteBSP
*/
void WriteBSP(node_t * headnode){
	int oldfaces;

	c_nofaces = 0;
	c_facenodes = 0;

	Verbose("--- WriteBSP ---\n");

	oldfaces = numfaces;
	dmodels[nummodels].headnode = EmitDrawNode_r(headnode);
	EmitAreaPortals(headnode);

	Verbose("%5i nodes with faces\n", c_facenodes);
	Verbose("%5i nodes without faces\n", c_nofaces);
	Verbose("%5i faces\n", numfaces - oldfaces);
}


/*
SetModelNumbers
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
EmitBrushes
*/
static void EmitBrushes(void){
	int i, j, bnum, s, x;
	dbrush_t *db;
	mapbrush_t *b;
	dbrushside_t *cp;
	vec3_t normal;
	vec_t dist;
	int planenum;

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
				Error("MAX_BSP_BRUSHSIDES\n");
			cp = &dbrushsides[numbrushsides];
			numbrushsides++;
			cp->planenum = b->original_sides[j].planenum;
			cp->surfnum = b->original_sides[j].texinfo;
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
				planenum = FindFloatPlane(normal, dist);
				for(i = 0; i < b->numsides; i++)
					if(b->original_sides[i].planenum == planenum)
						break;
				if(i == b->numsides){
					if(numbrushsides >= MAX_BSP_BRUSHSIDES)
						Error("MAX_BSP_BRUSHSIDES\n");

					dbrushsides[numbrushsides].planenum = planenum;
					dbrushsides[numbrushsides].surfnum =
					    dbrushsides[numbrushsides - 1].surfnum;
					numbrushsides++;
					db->numsides++;
				}
			}
		}
	}
}


/*
BeginBSPFile
*/
void BeginBSPFile(void){
	// these values may actually be initialized
	// if the file existed when loaded, so clear them explicitly
	nummodels = 0;
	numfaces = 0;
	numnodes = 0;
	numbrushsides = 0;
	numvertexes = 0;
	numnormals = 0;
	numleaffaces = 0;
	numleafbrushes = 0;
	numsurfedges = 0;

	// edge 0 is not used, because 0 can't be negated
	numedges = 1;

	// leave vertex 0 as an error
	numvertexes = 1;
	numnormals = 1;

	// leave leaf 0 as an error
	numleafs = 1;
	dleafs[0].contents = CONTENTS_SOLID;
}


/*
EndBSPFile
*/
void EndBSPFile(void){

	EmitBrushes();
	EmitPlanes();
	UnparseEntities();

	// now that the verts have been resolved, align the normals count
	memset(dnormals, 0, sizeof(dnormals));
	numnormals = numvertexes;

	// write the map
	Verbose("Writing %s\n", bspname);
	WriteBSPFile(bspname);
}


/*
BeginModel
*/
extern int firstmodeledge;
void BeginModel(void){
	dbspmodel_t *mod;
	int start, end;
	mapbrush_t *b;
	int j;
	entity_t *e;
	vec3_t mins, maxs;

	if(nummodels == MAX_BSP_MODELS)
		Error("MAX_BSP_MODELS\n");
	mod = &dmodels[nummodels];

	mod->firstface = numfaces;

	firstmodeledge = numedges;

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
EndModel
*/
void EndModel(void){
	dbspmodel_t *mod;

	mod = &dmodels[nummodels];

	mod->numfaces = numfaces - mod->firstface;

	nummodels++;
}
