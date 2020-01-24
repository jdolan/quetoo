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

#include "r_types.h"

void R_DrawMeshModel(const r_entity_t *e);
void R_DrawMeshModelMaterials(const r_entity_t *e);
const r_model_tag_t *R_MeshModelTag(const r_model_t *mod, const char *name, const int32_t frame);

#ifdef __R_LOCAL_H__
typedef struct {
	r_material_t *material;
	matrix4x4_t world_view; // the modelview matrix pre-entity rotation
	vec4_t color; // the last color we bound
} r_mesh_state_t;

extern r_mesh_state_t r_mesh_state;

void R_ApplyMeshModelConfig(r_entity_t *e);
_Bool R_CullMeshModel(const r_entity_t *e);
void R_DrawMeshModels(const r_entities_t *ents);
#endif /* __R_LOCAL_H__ */
