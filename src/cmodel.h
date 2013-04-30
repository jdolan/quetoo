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

#include "cvar.h"
#include "files.h"

c_model_t *Cm_LoadBsp(const char *name, int32_t *map_size);
c_model_t *Cm_Model(const char *name);  // *1, *2, etc

int32_t Cm_NumClusters(void);
int32_t Cm_NumModels(void);
char *Cm_EntityString(void);

// creates a clipping hull for an arbitrary box
int32_t Cm_HeadnodeForBox(const vec3_t mins, const vec3_t maxs);

// returns an ORed contents mask
int32_t Cm_PointContents(const vec3_t p, int32_t head_node);
int32_t Cm_TransformedPointContents(const vec3_t p, int32_t head_node, const vec3_t origin, const vec3_t angles);

c_trace_t Cm_BoxTrace(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
		int32_t head_node, int32_t brush_mask);
c_trace_t Cm_TransformedBoxTrace(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
		int32_t head_node, int32_t brush_mask, const vec3_t origin, const vec3_t angles);

byte *Cm_ClusterPVS(const int32_t cluster);
byte *Cm_ClusterPHS(const int32_t cluster);

int32_t Cm_PointLeafnum(const vec3_t p);

// call with top_node set to the head_node, returns with top_node
// set to the first node that splits the box
int32_t Cm_BoxLeafnums(const vec3_t mins, const vec3_t maxs, int32_t *list, size_t len, int32_t *top_node);

int32_t Cm_LeafContents(const int32_t leaf_num);
int32_t Cm_LeafCluster(const int32_t leaf_num);
int32_t Cm_LeafArea(const int32_t leaf_num);

void Cm_SetAreaPortalState(const int32_t portal_num, const bool open);
bool Cm_AreasConnected(const int32_t area1, const int32_t area2);

int32_t Cm_WriteAreaBits(byte *buffer, const int32_t area);
bool Cm_HeadnodeVisible(const int32_t head_node, const byte *vis);

#endif /* __CMODEL_H__ */
