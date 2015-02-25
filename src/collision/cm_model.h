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

#ifndef __CM_MODEL_H__
#define __CM_MODEL_H__

#include "cm_types.h"

cm_bsp_model_t *Cm_LoadBspModel(const char *name, int64_t *size);
cm_bsp_model_t *Cm_Model(const char *name); // *1, *2, etc

int32_t Cm_NumClusters(void);
int32_t Cm_NumModels(void);

const char *Cm_EntityString(void);
const char *Cm_WorldspawnValue(const char *key);

int32_t Cm_LeafContents(const int32_t leaf_num);
int32_t Cm_LeafCluster(const int32_t leaf_num);
int32_t Cm_LeafArea(const int32_t leaf_num);

#ifdef __CM_LOCAL_H__

#include "files.h"

typedef struct {
	char name[MAX_QPATH];
	byte *base;

	int32_t entity_string_len;
	char entity_string[MAX_BSP_ENT_STRING];

	int32_t num_planes;
	cm_bsp_plane_t planes[MAX_BSP_PLANES + 12]; // extra for box hull

	int32_t num_nodes;
	cm_bsp_node_t nodes[MAX_BSP_NODES + 6]; // extra for box hull

	int32_t num_surfaces;
	cm_bsp_surface_t surfaces[MAX_BSP_TEXINFO];

	int32_t num_leafs;
	cm_bsp_leaf_t leafs[MAX_BSP_LEAFS + 1]; // extra for box hull
	int32_t empty_leaf, solid_leaf;

	int32_t num_leaf_brushes;
	uint16_t leaf_brushes[MAX_BSP_LEAF_BRUSHES + 1]; // extra for box hull

	int32_t num_models;
	cm_bsp_model_t models[MAX_BSP_MODELS];

	int32_t num_brushes;
	cm_bsp_brush_t brushes[MAX_BSP_BRUSHES + 1]; // extra for box hull

	int32_t num_brush_sides;
	cm_bsp_brush_side_t brush_sides[MAX_BSP_BRUSH_SIDES + 6]; // extra for box hull

	int32_t num_visibility;
	byte visibility[MAX_BSP_VISIBILITY];

	int32_t num_areas;
	cm_bsp_area_t areas[MAX_BSP_AREAS];

	int32_t num_area_portals;
	d_bsp_area_portal_t area_portals[MAX_BSP_AREA_PORTALS];

	_Bool portal_open[MAX_BSP_AREA_PORTALS];
	int32_t flood_valid;
} cm_bsp_t;

typedef d_bsp_vis_t cm_vis_t;

extern cm_bsp_t cm_bsp;
extern cm_vis_t *cm_vis;

#endif /* __CM_LOCAL_H__ */

#endif /* __CM_MODEL_H__ */
