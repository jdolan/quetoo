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

#pragma once

#include "bspfile.h"
#include "polylib.h"

#define	MAX_BRUSH_SIDES 128
#define	CLIP_EPSILON 0.1

#define	TEXINFO_NODE -1 // side is already on a node

typedef struct map_plane_s {
	vec3_t normal;
	dvec_t dist;
	int32_t type;
	struct map_plane_s *hash_chain;
} map_plane_t;

typedef struct {
	vec_t shift[2];
	vec_t rotate;
	vec_t scale[2];
	char name[32];
	int32_t flags;
	int32_t value;
} map_brush_texture_t;

typedef struct side_s {
	int32_t plane_num;
	int32_t texinfo;
	winding_t *winding;
	struct side_s *original; // bsp_brush_t sides will reference the brush_t sides
	int32_t contents; // from miptex
	int32_t surf; // from miptex
	_Bool visible; // choose visible planes first
	_Bool tested; // this plane already checked as a split
	_Bool bevel; // don't ever use for bsp splitting
} side_t;

typedef struct {
	int32_t entity_num;
	int32_t brush_num;

	int32_t contents;

	vec3_t mins, maxs;

	int32_t num_sides;
	side_t *original_sides;
} map_brush_t;

#define	PLANENUM_LEAF -1

#define	MAXEDGES 32

typedef struct face_s {
	struct face_s *next; // on node

	// the chain of faces off of a node can be merged or split,
	// but each face_t along the way will remain in the chain
	// until the entire tree is freed
	struct face_s *merged; // if set, this face isn't valid anymore
	struct face_s *split[2]; // if set, this face isn't valid anymore

	struct portal_s *portal;
	int32_t texinfo;
	int32_t plane_num;
	int32_t contents; // faces in different contents can't merge
	int32_t output_number;
	winding_t *w;
	int32_t num_points;
	int32_t vertexnums[MAXEDGES];
} face_t;

typedef struct brush_s {
	struct brush_s *next;
	vec3_t mins, maxs;
	int32_t side, test_side; // side of node during construction
	map_brush_t *original;
	int32_t num_sides;
	side_t sides[6]; // variably sized
} brush_t;

#define	MAX_NODE_BRUSHES	8
typedef struct node_s {
	// both leafs and nodes
	int32_t plane_num; // -1 = leaf node
	struct node_s *parent;
	vec3_t mins, maxs; // valid after portalization
	brush_t *volume; // one for each leaf/node

	// nodes only
	_Bool detail_seperator; // a detail brush caused the split
	side_t *side; // the side that created the node
	struct node_s *children[2];
	face_t *faces;

	// leafs only
	brush_t *brushes; // fragments of all brushes in this leaf
	int32_t contents; // OR of all brush contents
	int32_t occupied; // 1 or greater can reach entity
	entity_t *occupant; // for leak file testing
	int32_t cluster; // for portal file writing
	int32_t area; // for area portals
	struct portal_s *portals; // also on nodes during construction
} node_t;

typedef struct portal_s {
	map_plane_t plane;
	node_t *onnode; // NULL = outside box
	node_t *nodes[2]; // [0] = front side of plane
	struct portal_s *next[2];
	winding_t *winding;

	_Bool sidefound; // false if ->side hasn't been checked
	side_t *side; // NULL = non-visible
	face_t *face[2]; // output face in bsp file
} portal_t;

typedef struct {
	node_t *head_node;
	node_t outside_node;
	vec3_t mins, maxs;
} tree_t;

extern int32_t entity_num;

extern map_plane_t map_planes[MAX_BSP_PLANES];
extern int32_t num_map_planes;

extern int32_t num_map_brushes;
extern map_brush_t map_brushes[MAX_BSP_BRUSHES];

extern vec3_t map_mins, map_maxs;

extern _Bool noprune;
extern _Bool nodetail;
extern _Bool fulldetail;
extern _Bool onlyents;
extern _Bool nomerge;
extern _Bool nowater;
extern _Bool nocsg;
extern _Bool noweld;
extern _Bool noshare;
extern _Bool notjunc;
extern _Bool noopt;
extern _Bool leaktest;

extern int32_t block_xl, block_xh, block_yl, block_yh;

extern vec_t microvolume;

extern int32_t first_bsp_model_edge;

void LoadMapFile(const char *file_name);
int32_t FindPlane(vec3_t normal, dvec_t dist);
_Bool WindingIsTiny(const winding_t *w);

// textures.c
int32_t FindMiptex(char *name);

int32_t TexinfoForBrushTexture(map_plane_t *plane, map_brush_texture_t *bt, const vec3_t origin);

void FindGCD(int32_t *v);

map_brush_t *Brush_LoadEntity(entity_t *ent);
_Bool MakeBrushPlanes(map_brush_t *b);
int32_t FindIntPlane(int32_t *inormal, int32_t *iorigin);
void CreateBrush(int32_t brush_num);

// csg.c
brush_t *MakeBspBrushList(int32_t startbrush, int32_t endbrush, vec3_t clipmins,
                          vec3_t clipmaxs);
brush_t *ChopBrushes(brush_t *head);

void WriteBrushMap(char *name, brush_t *list);

// brushbsp.c
void WriteBrushList(char *name, brush_t *brush, _Bool onlyvis);

brush_t *CopyBrush(brush_t *brush);

void SplitBrush(brush_t *brush, int32_t plane_num, brush_t **front, brush_t **back);

tree_t *AllocTree(void);
node_t *AllocNode(void);
void FreeNode(node_t *node);
brush_t *AllocBrush(int32_t num_sides);
int32_t CountBrushList(brush_t *brushes);
void FreeBrush(brush_t *brushes);
void FreeBrushList(brush_t *brushes);

tree_t *BrushBSP(brush_t *brushlist, vec3_t mins, vec3_t maxs);

// portals.c
int32_t VisibleContents(int32_t contents);

void MakeHeadnodePortals(tree_t *tree);
void MakeNodePortal(node_t *node);
void SplitNodePortals(node_t *node);

_Bool Portal_VisFlood(const portal_t *p);
void RemovePortalFromNode(portal_t *portal, node_t *l);

_Bool FloodEntities(tree_t *tree);
void FillOutside(node_t *head_node);
void FloodAreas(tree_t *tree);
void MarkVisibleSides(tree_t *tree, int32_t start, int32_t end);
void FreePortal(portal_t *p);
void EmitAreaPortals(void);

void MakeTreePortals(tree_t *tree);

// leakfile.c
void LeakFile(tree_t *tree);

// prtfile.c
void WritePortalFile(tree_t *tree);

// writebsp.c
void SetModelNumbers(void);
void BeginBSPFile(void);
void WriteBSP(node_t *head_node);
void EndBSPFile(void);
void BeginModel(void);
void EndModel(void);

// faces.c
void MakeFaces(node_t *head_node);
void FixTjuncs(node_t *head_node);
int32_t GetEdge2(int32_t v1, int32_t v2, face_t *f);
void FreeFace(face_t *f);

// tjunctions.c
void FixTJunctionsQ3(node_t *head_node);

// tree.c
void FreeTree(tree_t *tree);
void FreeTree_r(node_t *node);
void FreeTreePortals_r(node_t *node);
void PruneNodes_r(node_t *node);
void PruneNodes(node_t *node);
