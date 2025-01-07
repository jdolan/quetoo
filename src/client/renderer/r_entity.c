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
 * @brief
 */
static void R_SetEntityBounds(r_entity_t *e) {
	e->abs_model_bounds = Mat4_TransformBounds(e->matrix, e->model->bounds);
}

/**
 * @brief
 */
bool R_CullEntity(const r_view_t *view, const r_entity_t *e) {
	
	if (e->parent) {
		return false;
	}

	if (e->effects & (EF_SELF | EF_WEAPON)) {
		return false;
	}

	if (R_CulludeBox(view, e->abs_model_bounds)) {
		return true;
	}

	return false;
}

/**
 * @brief Adds an entity to the view if it passes frustum culling and occlusion tests.
 * @return The renderer copy of the entity, if any. This is to enable linked entities.
 */
r_entity_t *R_AddEntity(r_view_t *view, const r_entity_t *ent) {

	assert(view);
	assert(ent);

	if (view->num_entities == MAX_ENTITIES) {
		Com_Warn("MAX_ENTITIES\n");
		return NULL;
	}

	r_entity_t *e = &view->entities[view->num_entities];
	*e = *ent;

	e->matrix = Mat4_FromRotationTranslationScale(e->angles, e->origin, e->scale);

	if (IS_MESH_MODEL(e->model)) {

		if (e->parent && e->tag) {
			R_ApplyMeshTag(e);
		}

		R_ApplyMeshConfig(e);
	}

	e->inverse_matrix = Mat4_Inverse(e->matrix);

	R_SetEntityBounds(e);

	view->num_entities++;
	return e;
}

/**
 * @brief
 */
void R_UpdateEntities(r_view_t *view) {

	R_UpdateMeshEntities(view);

	R_UpdateBspInlineEntities(view);
}

/**
 * @brief
 */
static void R_DrawEntityBounds(const r_entity_t *e) {

	if (r_draw_entity_bounds->integer == 2) {
		R_Draw3DBox(e->abs_model_bounds, color_yellow, false);
	} else {
		R_Draw3DBox(e->abs_bounds, color_yellow, false);
	}
}

/**
 * @brief
 */
static void R_DrawEntitiesBounds(const r_view_t *view, int32_t blend_depth) {

	if (!r_draw_entity_bounds->value) {
		return;
	}

	const r_entity_t *e = view->entities;
	for (int32_t i = 0; i < view->num_entities; i++, e++) {

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

		if (R_CulludeBox(view, e->abs_model_bounds)) {
			continue;
		}

		R_DrawEntityBounds(e);
	}
}

/**
 * @brief Draw all entities at the specified depth value.
 */
void R_DrawEntities(const r_view_t *view, int32_t blend_depth) {

	R_DrawBspInlineEntities(view, blend_depth);

	R_DrawMeshEntities(view, blend_depth);

	R_DrawEntitiesBounds(view, blend_depth);
}
