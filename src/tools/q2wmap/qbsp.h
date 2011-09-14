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

#ifndef __QBSP_H__
#define __QBSP_H__

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
} map_plane_t;

typedef struct {
	vec_t shift[2];
	vec_t rotate;
	vec_t scale[2];
	char name[32];
	int flags;
	int value;
} map_brush_texture_t;

typedef struct side_s {
	int plane_num;
	int texinfo;
	winding_t *winding;
	struct side_s *original;	// bsp_brush_t sides will reference the brush_t sides
	int contents;				// from miptex
	int surf;					// from miptex
	boolean_t visible;			// choose visble planes first
	boolean_t tested;			// this plane already checked as a split
	boolean_t bevel;				// don't ever use for bsp splitting
} side_t;

typedef struct brush_s {
	int entity_num;
	int brush_num;

	int contents;

	vec3_t mins, maxs;

	int num_sides;
	side_t *original_sides;
} map_brush_t;

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
	int plane_num;
	int contents;				// faces in different contents can't merge
	int output_number;
	winding_t *w;
	int num_points;
	int vertexnums[MAXEDGES];
} face_t;


typedef struct bsp_brush_s {
	struct bsp_brush_s *next;
	vec3_t mins, maxs;
	int side, test_side;		// side of node during construction
	map_brush_t *original;
	int num_sides;
	side_t sides[6];			// variably sized
} bsp_brush_t;


#define	MAX_NODE_BRUSHES	8
typedef struct node_s {
	// both leafs and nodes
	int plane_num;				// -1 = leaf node
	struct node_s *parent;
	vec3_t mins, maxs;			// valid after portalization
	bsp_brush_t *volume;		// one for each leaf/node

	// nodes only
	boolean_t detail_seperator;	// a detail brush caused the split
	side_t *side;				 // the side that created the node
	struct node_s *children[2];
	face_t *faces;

	// leafs only
	bsp_brush_t *brushes;		// fragments of all brushes in this leaf
	int contents;				// OR of all brush contents
	int occupied;				// 1 or greater can reach entity
	entity_t *occupant;			// for leak file testing
	int cluster;				// for portalfile writing
	int area;					// for areaportals
	struct portal_s *portals;	// also on nodes during construction
} node_t;

typedef struct portal_s {
	map_plane_t plane;
	node_t *onnode;				  // NULL = outside box
	node_t *nodes[2];			  // [0] = front side of plane
	struct portal_s *next[2];
	winding_t *winding;

	boolean_t sidefound;			  // false if ->side hasn't been checked
	side_t *side;				  // NULL = non-visible
	face_t *face[2];			  // output face in bsp file
} portal_t;

typedef struct {
	node_t *head_node;
	node_t outside_node;
	vec3_t mins, maxs;
} tree_t;

extern int entity_num;

extern map_plane_t map_planes[MAX_BSP_PLANES];
extern int num_map_planes;

extern int num_map_brushes;
extern map_brush_t map_brushes[MAX_BSP_BRUSHES];

extern vec3_t map_mins, map_maxs;

extern boolean_t noprune;
extern boolean_t nodetail;
extern boolean_t fulldetail;
extern boolean_t nomerge;
extern boolean_t nosubdivide;
extern boolean_t nowater;
extern boolean_t noweld;
extern boolean_t noshare;
extern boolean_t notjunc;

extern vec_t microvolume;

void LoadMapFile(const char *file_name);
int FindFloatPlane(vec3_t normal, vec_t dist);
boolean_t WindingIsTiny(const winding_t * w);

// textures.c
int FindMiptex(char *name);

int TexinfoForBrushTexture(map_plane_t *plane, map_brush_texture_t *bt, vec3_t origin);

void FindGCD(int *v);

map_brush_t *Brush_LoadEntity(entity_t * ent);
boolean_t MakeBrushPlanes(map_brush_t * b);
int FindIntPlane(int *inormal, int *iorigin);
void CreateBrush(int brush_num);

// csg.c
bsp_brush_t *MakeBspBrushList(int startbrush, int endbrush,
                             vec3_t clipmins, vec3_t clipmaxs);
bsp_brush_t *ChopBrushes(bsp_brush_t * head);

void WriteBrushMap(char *name, bsp_brush_t * list);

// brushbsp.c
void WriteBrushList(char *name, bsp_brush_t * brush, boolean_t onlyvis);

bsp_brush_t *CopyBrush(bsp_brush_t * brush);

void SplitBrush(bsp_brush_t * brush, int plane_num,
                bsp_brush_t ** front, bsp_brush_t ** back);

tree_t *AllocTree(void);
node_t *AllocNode(void);
bsp_brush_t *AllocBrush(int num_sides);
int CountBrushList(bsp_brush_t * brushes);
void FreeBrush(bsp_brush_t * brushes);
void FreeBrushList(bsp_brush_t * brushes);

tree_t *BrushBSP(bsp_brush_t * brushlist, vec3_t mins, vec3_t maxs);

// portals.c
int VisibleContents(int contents);

void MakeHeadnodePortals(tree_t * tree);
void MakeNodePortal(node_t * node);
void SplitNodePortals(node_t * node);

boolean_t Portal_VisFlood(const portal_t * p);
void RemovePortalFromNode(portal_t * portal, node_t * l);

boolean_t FloodEntities(tree_t * tree);
void FillOutside(node_t * head_node);
void FloodAreas(tree_t * tree);
void MarkVisibleSides(tree_t * tree, int start, int end);
void FreePortal(portal_t * p);
void EmitAreaPortals(node_t * head_node);

void MakeTreePortals(tree_t * tree);

// leakfile.c
void LeakFile(tree_t * tree);

// prtfile.c
void WritePortalFile(tree_t * tree);

// writebsp.c
void SetModelNumbers(void);
void BeginBSPFile(void);
void WriteBSP(node_t * head_node);
void EndBSPFile(void);
void BeginModel(void);
void EndModel(void);

// faces.c
void MakeFaces(node_t * head_node);
void FixTjuncs(node_t * head_node);
int GetEdge2(int v1, int v2, face_t * f);

void FreeFace(face_t * f);

// tree.c
void FreeTree(tree_t * tree);
void FreeTree_r(node_t * node);
void FreeTreePortals_r(node_t * node);
void PruneNodes_r(node_t * node);
void PruneNodes(node_t * node);

#endif /* __QBSP_H__ */
