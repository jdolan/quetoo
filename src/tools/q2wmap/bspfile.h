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

#ifndef __BSPFILE_H__
#define __BSPFILE_H__

#include "q2wmap.h"

typedef struct d_bsp_s {
	int num_models;
	d_bsp_model_t models[MAX_BSP_MODELS];

	int vis_data_size;
	byte vis_data[MAX_BSP_VISIBILITY];

	int lightmap_data_size;
	byte lightmap_data[MAX_BSP_LIGHTING];

	int entity_string_len;
	char entity_string[MAX_BSP_ENT_STRING];

	int num_leafs;
	d_bsp_leaf_t leafs[MAX_BSP_LEAFS];

	int num_planes;
	d_bsp_plane_t planes[MAX_BSP_PLANES];

	int num_vertexes;
	d_bsp_vertex_t vertexes[MAX_BSP_VERTS];

	int num_normals;
	d_bsp_normal_t normals[MAX_BSP_VERTS];

	int num_nodes;
	d_bsp_node_t nodes[MAX_BSP_NODES];

	int num_texinfo;
	d_bsp_texinfo_t texinfo[MAX_BSP_TEXINFO];

	int num_faces;
	d_bsp_face_t faces[MAX_BSP_FACES];

	int num_edges;
	d_bsp_edge_t edges[MAX_BSP_EDGES];

	int num_leaf_faces;
	unsigned short leaf_faces[MAX_BSP_LEAF_FACES];

	int num_leaf_brushes;
	unsigned short leaf_brushes[MAX_BSP_LEAF_BRUSHES];

	int num_face_edges;
	int face_edges[MAX_BSP_FACE_EDGES];

	int num_areas;
	d_bsp_area_t areas[MAX_BSP_AREAS];

	int num_area_portals;
	d_bsp_area_portal_t area_portals[MAX_BSP_AREA_PORTALS];

	int num_brushes;
	d_bsp_brush_t brushes[MAX_BSP_BRUSHES];

	int num_brush_sides;
	d_bsp_brush_side_t brush_sides[MAX_BSP_BRUSH_SIDES];

	byte dpop[256];
} d_bsp_t;

extern d_bsp_t d_bsp;
extern d_bsp_vis_t *d_vis;


void DecompressVis(byte *in, byte *decompressed);
int CompressVis(byte *vis, byte *dest);

void LoadBSPFile(char *file_name);
void LoadBSPFileTexinfo(char *file_name);	// just for qdata
void WriteBSPFile(char *file_name);
void PrintBSPFileSizes(void);

typedef struct epair_s {
	struct epair_s *next;
	char *key;
	char *value;
} epair_t;

typedef struct {
	vec3_t origin;
	int first_brush;
	int num_brushes;
	epair_t *epairs;

	// only valid for func_areaportals
	int area_portal_num;
	int portal_areas[2];
} entity_t;

extern int num_entities;
extern entity_t entities[MAX_BSP_ENTITIES];

void ParseEntities(void);
void UnparseEntities(void);

void SetKeyValue(entity_t *ent, const char *key, const char *value);
const char *ValueForKey(const entity_t *ent, const char *key);
// will return "" if not present

vec_t FloatForKey(const entity_t *ent, const char *key);
void GetVectorForKey(const entity_t *ent, const char *key, vec3_t vec);

epair_t *ParseEpair(void);

extern int subdivide_size;  // shared by qbsp and light

#endif /* __BSPFILE_H__ */
