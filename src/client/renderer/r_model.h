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

#ifndef __R_MODEL_H__
#define __R_MODEL_H__

#include "r_types.h"

r_model_t *R_LoadModel(const char *name);
r_model_t *R_WorldModel(void);

#ifdef __R_LOCAL_H__

#define MAX_MOD_KNOWN 512

typedef struct {
	r_model_t models[MAX_MOD_KNOWN];
	uint16_t num_models;

	r_model_t *load;

	r_model_t *world;
	r_model_t bsp_models[MAX_BSP_MODELS];
} r_models_t;

extern r_models_t r_models;

#define IS_MESH_MODEL(m) (m && (m->type == mod_md3 || m->type == mod_obj))

void *R_HunkBegin(void);
size_t R_HunkEnd(void *buf);
void *R_HunkAlloc(size_t size);
void R_AllocVertexArrays(r_model_t *mod);
void R_InitModels(void);
void R_ShutdownModels(void);
void R_BeginLoading(const char *map, int32_t mapsize);
void R_ListModels_f(void);
void R_HunkStats_f(void);

#endif /* __R_LOCAL_H__ */

#endif /* __R_MODEL_H__ */
