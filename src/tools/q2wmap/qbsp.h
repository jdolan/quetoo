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

#include "bspfile.h"
#include "polylib.h"

#define	MAX_BRUSH_SIDES 128
#define	CLIP_EPSILON 0.1

#define	TEXINFO_NODE -1  // side is already on a node

typedef struct plane_s {
	vec3_t normal;
	vec_t dist;
	int type;
	struct plane_s *hash_chain;
} plane_t;

typedef struct {
	vec_t shift[2];
	vec_t rotate;
	vec_t scale[2];
	char name[32];
	int flags;
	int value;
} brush_texture_t;

typedef struct side_s {
	int planenum;
	int texinfo;
	winding_t *winding;
	struct side_s *original;	// bspbrush_t sides will reference the mapbrush_t sides
	int contents;				// from miptex
	int surf;					// from miptex
	qboolean visible;			// choose visble planes first
	qboolean tested;			// this plane already checked as a split
	qboolean bevel;				// don't ever use for bsp splitting
} side_t;

typedef struct brush_s {
	int entitynum;
	int brushnum;

	int contents;

	vec3_t mins, maxs;

	int numsides;
	side_t *original_sides;
} mapbrush_t;

#define	PLANENUM_LEAF -1

#define	MAXEDGES 20

typedef struct face_s {
	struct face_s *next;		// on node

	// the chain of faces off of a node can be merged or split,
	// but each face_t along the way will remain in the chain
	// until the entire tree is freed
	struct face_s *merged;		// if set, this face isn't valid anymore
	struct face_s *split[2];	// if set, this face isn't valid anymore

	struct portal_s *portal;
	int texinfo;
	int planenum;
	int contents;				// faces in different contents can't merge
	int outputnumber;
	winding_t *w;
	int numpoints;
	int vertexnums[MAXEDGES];
} face_t;


typedef struct bspbrush_s {
	struct bspbrush_s *next;
	vec3_t mins, maxs;
	int side, testside;			// side of node during construction
	mapbrush_t *original;
	int numsides;
	side_t sides[6];			// variably sized
} bspbrush_t;


#define	MAX_NODE_BRUSHES	8
typedef struct node_s {
	// both leafs and nodes
	int planenum;				// -1 = leaf node
	struct node_s *parent;
	vec3_t mins, maxs;			// valid after portalization
	bspbrush_t *volume;			// one for each leaf/node

	// nodes only
	qboolean detail_seperator;	// a detail brush caused the split
	side_t *side;				 // the side that created the node
	struct node_s *children[2];
	face_t *faces;

	// leafs only
	bspbrush_t *brushlist;		// fragments of all brushes in this leaf
	int contents;				// OR of all brush contents
	int occupied;				// 1 or greater can reach entity
	entity_t *occupant;			// for leak file testing
	int cluster;				// for portalfile writing
	int area;					// for areaportals
	struct portal_s *portals;	// also on nodes during construction
} node_t;

typedef struct portal_s {
	plane_t plane;
	node_t *onnode;				  // NULL = outside box
	node_t *nodes[2];			  // [0] = front side of plane
	struct portal_s *next[2];
	winding_t *winding;

	qboolean sidefound;			  // false if ->side hasn't been checked
	side_t *side;				  // NULL = non-visible
	face_t *face[2];			  // output face in bsp file
} portal_t;

typedef struct {
	node_t *headnode;
	node_t outside_node;
	vec3_t mins, maxs;
} tree_t;

extern int entity_num;

extern plane_t mapplanes[MAX_BSP_PLANES];
extern int nummapplanes;

extern int nummapbrushes;
extern mapbrush_t mapbrushes[MAX_BSP_BRUSHES];

extern vec3_t map_mins, map_maxs;

extern qboolean noprune;
extern qboolean nodetail;
extern qboolean fulldetail;
extern qboolean nomerge;
extern qboolean nosubdivide;
extern qboolean nowater;
extern qboolean noweld;
extern qboolean noshare;
extern qboolean notjunc;

extern vec_t microvolume;

void LoadMapFile(const char *filename);
int FindFloatPlane(vec3_t normal, vec_t dist);
qboolean WindingIsTiny(const winding_t * w);

// textures.c
typedef struct {
	char name[64];
	/*int flags;
	int value;
	int contents;
	char animname[64];*/
} textureref_t;

#define	MAX_BSP_TEXTURES 1024

extern textureref_t textureref[MAX_BSP_TEXTURES];

int FindMiptex(char *name);

int TexinfoForBrushTexture(plane_t *plane, brush_texture_t *bt, vec3_t origin);

void FindGCD(int *v);

mapbrush_t *Brush_LoadEntity(entity_t * ent);
qboolean MakeBrushPlanes(mapbrush_t * b);
int FindIntPlane(int *inormal, int *iorigin);
void CreateBrush(int brushnum);

// csg.c
bspbrush_t *MakeBspBrushList(int startbrush, int endbrush,
                             vec3_t clipmins, vec3_t clipmaxs);
bspbrush_t *ChopBrushes(bspbrush_t * head);

void WriteBrushMap(char *name, bspbrush_t * list);

// brushbsp.c
void WriteBrushList(char *name, bspbrush_t * brush, qboolean onlyvis);

bspbrush_t *CopyBrush(bspbrush_t * brush);

void SplitBrush(bspbrush_t * brush, int planenum,
                bspbrush_t ** front, bspbrush_t ** back);

tree_t *AllocTree(void);
node_t *AllocNode(void);
bspbrush_t *AllocBrush(int numsides);
int CountBrushList(bspbrush_t * brushes);
void FreeBrush(bspbrush_t * brushes);
void FreeBrushList(bspbrush_t * brushes);

tree_t *BrushBSP(bspbrush_t * brushlist, vec3_t mins, vec3_t maxs);

// portals.c
int VisibleContents(int contents);

void MakeHeadnodePortals(tree_t * tree);
void MakeNodePortal(node_t * node);
void SplitNodePortals(node_t * node);

qboolean Portal_VisFlood(const portal_t * p);
void RemovePortalFromNode(portal_t * portal, node_t * l);

qboolean FloodEntities(tree_t * tree);
void FillOutside(node_t * headnode);
void FloodAreas(tree_t * tree);
void MarkVisibleSides(tree_t * tree, int start, int end);
void FreePortal(portal_t * p);
void EmitAreaPortals(node_t * headnode);

void MakeTreePortals(tree_t * tree);

// leakfile.c
void LeakFile(tree_t * tree);

// prtfile.c
void WritePortalFile(tree_t * tree);

// writebsp.c
void SetModelNumbers(void);
void BeginBSPFile(void);
void WriteBSP(node_t * headnode);
void EndBSPFile(void);
void BeginModel(void);
void EndModel(void);

// faces.c
void MakeFaces(node_t * headnode);
void FixTjuncs(node_t * headnode);
int GetEdge2(int v1, int v2, face_t * f);

void FreeFace(face_t * f);

// tree.c
void FreeTree(tree_t * tree);
void FreeTree_r(node_t * node);
void FreeTreePortals_r(node_t * node);
void PruneNodes_r(node_t * node);
void PruneNodes(node_t * node);

