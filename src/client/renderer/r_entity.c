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

	r_entity_t *e = &r_view.entities[r_view.num_entities++];
	*e = *ent;

	if (!(e->effects & EF_LINKED)) { // child models use explicit matrix, do not recompute
		Matrix4x4_CreateFromEntity(&e->matrix, e->origin, e->angles, e->scale);
	}

	if (IS_MESH_MODEL(e->model)) {
		R_ApplyMeshModelConfig(e);
	}

	Matrix4x4_Invert_Simple(&e->inverse_matrix, &e->matrix);

	return e;
}

#if 0
/**
 * @brief Draws a place-holder "white diamond" prism for the specified entity.
 */
static void R_DrawNullModel(const r_entity_t *e) {

	R_BindDiffuseTexture(r_image_state.null->texnum);

	R_RotateForEntity(e);

	R_DrawArrays(GL_TRIANGLES, 0, (GLsizei) r_model_state.null_elements_count);

	R_RotateForEntity(NULL);
}

/**
 * @brief Draws all entities added to the view but missing a model.
 */
static void R_DrawNullModels(const r_entities_t *ents) {

	if (!ents->count) {
		return;
	}

	R_BindAttributeBuffer(R_ATTRIB_POSITION, &r_model_state.null_vertices);
	R_BindAttributeBuffer(R_ATTRIB_ELEMENTS, &r_model_state.null_elements);

	for (size_t i = 0; i < ents->count; i++) {
		const r_entity_t *e = ents->entities[i];

		if (e->effects & EF_NO_DRAW) {
			continue;
		}

		r_view.current_entity = e;

		R_DrawNullModel(e);
	}

	R_UnbindAttributeBuffer(R_ATTRIB_POSITION);
	R_UnbindAttributeBuffer(R_ATTRIB_ELEMENTS);

	r_view.current_entity = NULL;
}

/**
 * @brief Draws bounding boxes for all non-linked entities in `ents`.
 */
static void R_DrawEntityBounds(const r_entities_t *ents, const vec4_t color) {

	if (!r_draw_entity_bounds->value) {
		return;
	}

	if (ents->count == 0) {
		return;
	}

	R_BindDiffuseTexture(r_image_state.null->texnum); // TODO: Disable texture instead?

	R_EnableColorArray(true);

	R_BindAttributeInterleaveBuffer(&r_model_state.bound_vertice_buffer, R_ATTRIB_MASK_ALL);
	R_BindAttributeBuffer(R_ATTRIB_ELEMENTS, &r_model_state.bound_element_buffer);

	for (int32_t i = 0; i < 8; i++) {
		ColorDecompose(color, r_model_state.bound_vertices[i].color);
	}

	static matrix4x4_t mat, modelview;

	R_GetMatrix(R_MATRIX_MODELVIEW, &modelview);

	for (size_t i = 0; i < ents->count; i++) {
		const r_entity_t *e = ents->entities[i];

		if ((e->effects & EF_WEAPON) || !IS_MESH_MODEL(e->model)) {
			continue;
		}

		VectorSet(r_model_state.bound_vertices[0].position, e->mins[0], e->mins[1], e->mins[2]);
		VectorSet(r_model_state.bound_vertices[1].position, e->maxs[0], e->mins[1], e->mins[2]);
		VectorSet(r_model_state.bound_vertices[2].position, e->maxs[0], e->maxs[1], e->mins[2]);
		VectorSet(r_model_state.bound_vertices[3].position, e->mins[0], e->maxs[1], e->mins[2]);

		VectorSet(r_model_state.bound_vertices[4].position, e->mins[0], e->mins[1], e->maxs[2]);
		VectorSet(r_model_state.bound_vertices[5].position, e->maxs[0], e->mins[1], e->maxs[2]);
		VectorSet(r_model_state.bound_vertices[6].position, e->maxs[0], e->maxs[1], e->maxs[2]);
		VectorSet(r_model_state.bound_vertices[7].position, e->mins[0], e->maxs[1], e->maxs[2]);

		R_UploadToBuffer(&r_model_state.bound_vertice_buffer, sizeof(r_bound_interleave_vertex_t) * 8,
		                 r_model_state.bound_vertices);

		// draw box
		const vec_t *origin;

		if (e->effects & EF_BOB) {
			origin = e->termination;
		} else {
			origin = e->origin;
		}

		Matrix4x4_CreateFromEntity(&mat, origin, vec3_origin, e->scale);

		Matrix4x4_Concat(&mat, &modelview, &mat);

		R_SetMatrix(R_MATRIX_MODELVIEW, &mat);

		R_DrawArrays(GL_LINES, 0, (GLint) r_model_state.bound_element_count - 6);

		// draw origin
		Matrix4x4_CreateFromEntity(&mat, origin, e->angles, e->scale);

		Matrix4x4_Concat(&mat, &modelview, &mat);

		R_SetMatrix(R_MATRIX_MODELVIEW, &mat);

		R_DrawArrays(GL_LINES, (GLint) r_model_state.bound_element_count - 6, 6);
	}

	R_SetMatrix(R_MATRIX_MODELVIEW, &modelview);

	R_UnbindAttributeBuffer(R_ATTRIB_ELEMENTS);

	R_EnableColorArray(false);
}
#endif

/**
 * @brief Primary entry point for drawing all entities.
 */
void R_DrawEntities(void) {

//	R_DrawNullModels(&r_sorted_entities.null_entities);

	R_DrawMeshEntities();

//	vec4_t yellow = { 0.9, 0.9, 0.0, 1.0 };
//	R_DrawEntityBounds(&r_sorted_entities.mesh_entities, yellow);
}
