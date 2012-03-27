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

#ifndef __CMODEL_H__
#define __CMODEL_H__

#include "files.h"
#include "filesystem.h"

c_model_t *Cm_LoadBsp(const char *name, int *map_size);
c_model_t *Cm_Model(const char *name);  // *1, *2, etc

int Cm_NumClusters(void);
int Cm_NumModels(void);
char *Cm_EntityString(void);

// creates a clipping hull for an arbitrary box
int Cm_HeadnodeForBox(const vec3_t mins, const vec3_t maxs);

// returns an ORed contents mask
int Cm_PointContents(const vec3_t p, int head_node);
int Cm_TransformedPointContents(const vec3_t p, int head_node, const vec3_t origin, const vec3_t angles);

c_trace_t Cm_BoxTrace(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
		int head_node, int brush_mask);
c_trace_t Cm_TransformedBoxTrace(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
		int head_node, int brush_mask, const vec3_t origin, const vec3_t angles);

byte *Cm_ClusterPVS(const int cluster);
byte *Cm_ClusterPHS(const int cluster);

int Cm_PointLeafnum(const vec3_t p);

// call with top_node set to the head_node, returns with top_node
// set to the first node that splits the box
int Cm_BoxLeafnums(const vec3_t mins, const vec3_t maxs, int *list, size_t len, int *top_node);

int Cm_LeafContents(const int leaf_num);
int Cm_LeafCluster(const int leaf_num);
int Cm_LeafArea(const int leaf_num);

void Cm_SetAreaPortalState(const int portal_num, const bool open);
bool Cm_AreasConnected(const int area1, const int area2);

int Cm_WriteAreaBits(byte *buffer, const int area);
bool Cm_HeadnodeVisible(const int head_node, const byte *vis);

#endif /* __CMODEL_H__ */
