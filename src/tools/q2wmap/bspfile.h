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

extern int nummodels;
extern d_bsp_model_t	dmodels[MAX_BSP_MODELS];

extern int visdatasize;
extern byte dvisdata[MAX_BSP_VISIBILITY];
extern d_bsp_vis_t *dvis;

extern int lightmap_data_size;
extern byte dlightmap_data[MAX_BSP_LIGHTING];

extern int entdatasize;
extern char dentdata[MAX_BSP_ENTSTRING];

extern int num_leafs;
extern d_bsp_leaf_t dleafs[MAX_BSP_LEAFS];

extern int num_planes;
extern d_bsp_plane_t dplanes[MAX_BSP_PLANES];

extern int num_vertexes;
extern d_bsp_vertex_t dvertexes[MAX_BSP_VERTS];

extern int numnormals;
extern d_bsp_normal_t dnormals[MAX_BSP_VERTS];

extern int num_nodes;
extern d_bsp_node_t dnodes[MAX_BSP_NODES];

extern int num_texinfo;
extern d_bsp_texinfo_t texinfo[MAX_BSP_TEXINFO];

extern int num_faces;
extern d_bsp_face_t dfaces[MAX_BSP_FACES];

extern int num_edges;
extern d_bsp_edge_t dedges[MAX_BSP_EDGES];

extern int num_leaf_faces;
extern unsigned short dleaffaces[MAX_BSP_LEAFFACES];

extern int num_leaf_brushes;
extern unsigned short dleafbrushes[MAX_BSP_LEAFBRUSHES];

extern int num_surfedges;
extern int dsurfedges[MAX_BSP_SURFEDGES];

extern int numareas;
extern d_bsp_area_t dareas[MAX_BSP_AREAS];

extern int num_area_portals;
extern d_bsp_area_portal_t dareaportals[MAX_BSP_AREAPORTALS];

extern int numbrushes;
extern d_bsp_brush_t dbrushes[MAX_BSP_BRUSHES];

extern int numbrushsides;
extern d_bsp_brush_side_t dbrushsides[MAX_BSP_BRUSHSIDES];

extern byte dpop[256];

void DecompressVis(byte *in, byte *decompressed);
int CompressVis(byte *vis, byte *dest);

void LoadBSPFile(char *filename);
void LoadBSPFileTexinfo(char *filename);	// just for qdata
void WriteBSPFile(char *filename);
void PrintBSPFileSizes(void);


typedef struct epair_s {
	struct epair_s *next;
	char *key;
	char *value;
} epair_t;

typedef struct {
	vec3_t origin;
	int firstbrush;
	int numbrushes;
	epair_t *epairs;

	// only valid for func_areaportals
	int areaportal_num;
	int portalareas[2];
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

extern int subdivide_size;  // shared by qbsp and qrad

#endif /* __BSPFILE_H__ */
