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
extern dbspmodel_t	dmodels[MAX_BSP_MODELS];

extern int visdatasize;
extern byte dvisdata[MAX_BSP_VISIBILITY];
extern dvis_t *dvis;

extern int lightdatasize;
extern byte dlightdata[MAX_BSP_LIGHTING];

extern int entdatasize;
extern char dentdata[MAX_BSP_ENTSTRING];

extern int numleafs;
extern dleaf_t dleafs[MAX_BSP_LEAFS];

extern int numplanes;
extern dplane_t dplanes[MAX_BSP_PLANES];

extern int numvertexes;
extern dbspvertex_t dvertexes[MAX_BSP_VERTS];

extern int numnormals;
extern dbspnormal_t dnormals[MAX_BSP_VERTS];

extern int numnodes;
extern dnode_t dnodes[MAX_BSP_NODES];

extern int numtexinfo;
extern dtexinfo_t texinfo[MAX_BSP_TEXINFO];

extern int numfaces;
extern dface_t dfaces[MAX_BSP_FACES];

extern int numedges;
extern dedge_t dedges[MAX_BSP_EDGES];

extern int numleaffaces;
extern unsigned short dleaffaces[MAX_BSP_LEAFFACES];

extern int numleafbrushes;
extern unsigned short dleafbrushes[MAX_BSP_LEAFBRUSHES];

extern int numsurfedges;
extern int dsurfedges[MAX_BSP_SURFEDGES];

extern int numareas;
extern darea_t dareas[MAX_BSP_AREAS];

extern int numareaportals;
extern dareaportal_t dareaportals[MAX_BSP_AREAPORTALS];

extern int numbrushes;
extern dbrush_t dbrushes[MAX_BSP_BRUSHES];

extern int numbrushsides;
extern dbrushside_t dbrushsides[MAX_BSP_BRUSHSIDES];

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
	int areaportalnum;
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
