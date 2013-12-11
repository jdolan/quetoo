/*
 * Copyright(c) 1997-2001 id Software, Inc.
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

#ifndef __CM_MODEL_H__
#define __CM_MODEL_H__

#include "cm_types.h"

cm_bsp_model_t *Cm_LoadBsp(const char *name, int32_t *map_size);
cm_bsp_model_t *Cm_Model(const char *name); // *1, *2, etc

int32_t Cm_NumClusters(void);
int32_t Cm_NumModels(void);

const char *Cm_EntityString(void);
const char *Cm_WorldspawnValue(const char *key);

int32_t Cm_LeafContents(const int32_t leaf_num);
int32_t Cm_LeafCluster(const int32_t leaf_num);
int32_t Cm_LeafArea(const int32_t leaf_num);

#ifdef __CM_LOCAL_H__

extern cm_bsp_t cm_bsp;
extern cm_vis_t *cm_vis;

#endif /* __CM_LOCAL_H__ */

#endif /* __CM_MODEL_H__ */
