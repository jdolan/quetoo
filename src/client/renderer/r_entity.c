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

#include "r_local.h"

/**
 * @brief Adds an entity to the view.
 */
r_entity_t *R_AddEntity(const r_entity_t *ent) {

	if (r_view.num_entities == lengthof(r_view.entities)) {
		Com_Warn("MAX_ENTITIES\n");
		return NULL;
	}

	r_entity_t *e = &r_view.entities[r_view.num_entities];
	*e = *ent;

	Matrix4x4_CreateFromEntity(&e->matrix, e->origin, e->angles, e->scale);

	if (IS_MESH_MODEL(e->model)) {

		if (e->parent && e->tag) {
			R_ApplyMeshTag(e);
		}

		R_ApplyMeshConfig(e);
	}

	Matrix4x4_Invert_Simple(&e->inverse_matrix, &e->matrix);

	Matrix4x4_Transform(&e->matrix, e->model->mins.xyz, e->abs_model_mins.xyz);
	Matrix4x4_Transform(&e->matrix, e->model->maxs.xyz, e->abs_model_maxs.xyz);

	if (!(e->effects & (EF_SELF | EF_WEAPON))) {

		if (R_CullBox(e->abs_model_mins, e->abs_model_maxs)) {
			return NULL;
		}
	}

	r_view.num_entities++;
	return e;
}

/**
 * @brief
 */
void R_UpdateEntities(void) {
	
	R_UpdateMeshEntities();
	
	R_UpdateMeshEntitiesShadows();
}

/**
 * @brief
 */
static void R_DrawEntityBounds(const r_entity_t *e) {

	vec3_t mins, maxs;
	if (r_draw_entity_bounds->integer == 2) {
		mins = e->abs_model_mins;
		maxs = e->abs_model_maxs;
	} else {
		mins = e->abs_mins;
		maxs = e->abs_maxs;
	}

	R_Draw3DLines((const vec3_t []) {
		Vec3(mins.x, mins.y, mins.z),
		Vec3(maxs.x, mins.y, mins.z),
		Vec3(maxs.x, maxs.y, mins.z),
		Vec3(mins.x, maxs.y, mins.z),
		Vec3(mins.x, mins.y, mins.z),
	}, 5, color_yellow);

	R_Draw3DLines((const vec3_t []) {
		Vec3(mins.x, mins.y, maxs.z),
		Vec3(maxs.x, mins.y, maxs.z),
		Vec3(maxs.x, maxs.y, maxs.z),
		Vec3(mins.x, maxs.y, maxs.z),
		Vec3(mins.x, mins.y, maxs.z),
	}, 5, color_yellow);

	R_Draw3DLines((const vec3_t []) {
		Vec3(mins.x, mins.y, mins.z),
		Vec3(mins.x, mins.y, maxs.z),
	}, 2, color_yellow);

	R_Draw3DLines((const vec3_t []) {
		Vec3(mins.x, maxs.y, mins.z),
		Vec3(mins.x, maxs.y, maxs.z),
	}, 2, color_yellow);

	R_Draw3DLines((const vec3_t []) {
		Vec3(maxs.x, maxs.y, mins.z),
		Vec3(maxs.x, maxs.y, maxs.z),
	}, 2, color_yellow);

	R_Draw3DLines((const vec3_t []) {
		Vec3(maxs.x, mins.y, mins.z),
		Vec3(maxs.x, mins.y, maxs.z),
	}, 2, color_yellow);
}

/**
 * @brief Draw all entities at the specified depth value.
 */
void R_DrawEntities(int32_t blend_depth) {
	
	R_DrawMeshEntities(blend_depth);

	R_DrawMeshEntitiesShadows(blend_depth);

	if (!r_draw_entity_bounds->value) {
		return;
	}

	const r_entity_t *e = r_view.entities;
	for (int32_t i = 0; i < r_view.num_entities; i++, e++) {

		if (e->model == NULL) {
			continue;
		}

		if (e->parent) {
			continue;
		}

		if (e->effects & (EF_SELF | EF_WEAPON)) {
			continue;
		}

		if (e->blend_depth != blend_depth) {
			continue;
		}

		R_DrawEntityBounds(e);
	}
}
