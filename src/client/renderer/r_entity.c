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

static r_sorted_entities_t r_sorted_entities;

/**
 * @brief Adds an entity to the view.
 */
const r_entity_t *R_AddEntity(const r_entity_t *ent) {

	if (r_view.num_entities == lengthof(r_view.entities)) {
		Com_Warn("MAX_ENTITIES exceeded\n");
		return NULL;
	}

	// copy to view array
	r_view.entities[r_view.num_entities] = *ent;

	return &r_view.entities[r_view.num_entities++];
}

/**
 * @brief Binds a linked model to its parent, and copies it into the view structure.
 */
const r_entity_t *R_AddLinkedEntity(const r_entity_t *parent, const r_model_t *model,
		const char *tag_name) {

	if (!parent) {
		Com_Warn("NULL parent\n");
		return NULL;
	}

	r_entity_t ent = *parent;

	ent.parent = parent;
	ent.tag_name = tag_name;

	ent.model = model;

	memset(ent.skins, 0, sizeof(ent.skins));
	ent.num_skins = 0;

	ent.frame = ent.old_frame = 0;

	ent.lerp = 1.0;
	ent.back_lerp = 0.0;

	return R_AddEntity(&ent);
}

/**
 * @brief Applies translation, rotation, and scale for the specified entity.
 */
void R_RotateForEntity(const r_entity_t *e) {

	if (!e) {
		glPopMatrix();
		return;
	}

	glPushMatrix();

	glMultMatrixf((GLfloat *) e->matrix.m);
}

/**
 * @brief Applies any configuration and tag alignment, populating the model-view
 * matrix for the entity in the process.
 */
void R_SetMatrixForEntity(r_entity_t *e) {

	if (e->parent) {
		vec3_t forward;

		if (!IS_MESH_MODEL(e->model)) {
			Com_Warn("Invalid model for linked entity\n");
			return;
		}

		const r_entity_t *p = e->parent;
		while (p->parent) {
			p = p->parent;
		}

		AngleVectors(p->angles, forward, NULL, NULL);

		VectorClear(e->origin);
		VectorClear(e->angles);

		Matrix4x4_CreateFromEntity(&e->matrix, e->origin, e->angles, e->scale);

		R_ApplyMeshModelTag(e);

		R_ApplyMeshModelConfig(e);

		Matrix4x4_Invert_Simple(&e->inverse_matrix, &e->matrix);

		Matrix4x4_Transform(&e->matrix, vec3_origin, e->origin);
		Matrix4x4_Transform(&e->matrix, vec3_forward, forward);

		VectorAngles(forward, e->angles);
		return;
	}

	Matrix4x4_CreateFromEntity(&e->matrix, e->origin, e->angles, e->scale);

	if (IS_MESH_MODEL(e->model)) {
		R_ApplyMeshModelConfig(e);
	}

	Matrix4x4_Invert_Simple(&e->inverse_matrix, &e->matrix);
}

/**
 * @brief Qsort comparator for R_CullEntities.
 */
static int32_t R_CullEntities_compare(const void *a, const void *b) {

	const r_entity_t *e1 = *((const r_entity_t **) a);
	const r_entity_t *e2 = *((const r_entity_t **) b);

	return strcmp(e1->model->media.name, e2->model->media.name);
}

/**
 * @brief Performs a frustum-cull of all entities. This is performed in a separate
 * thread while the renderer draws the world. Mesh entities which pass a frustum
 * cull will also have their lighting information updated.
 */
void R_CullEntities(void *data __attribute__((unused))) {

	r_entity_t *e = r_view.entities;
	for (uint16_t i = 0; i < r_view.num_entities; i++, e++) {

		r_entities_t *ents = &r_sorted_entities.null_entities;

		if (IS_BSP_INLINE_MODEL(e->model)) {

			if (R_CullBspInlineModel(e))
				continue;

			ents = &r_sorted_entities.bsp_inline_entities;
		}
		else if (IS_MESH_MODEL(e->model)) {

			if (R_CullMeshModel(e))
				continue;

			R_UpdateMeshModelLighting(e);

			ents = &r_sorted_entities.mesh_entities;
		}

		R_ENTITY_TO_ENTITIES(ents, e); // append to the appropriate draw list

		R_SetMatrixForEntity(e); // set the transform matrix
	}

	// sort the mesh entities list by model to allow object instancing

	r_entities_t *mesh = &r_sorted_entities.mesh_entities;
	qsort(mesh, mesh->count, sizeof(r_entity_t *), R_CullEntities_compare);
}

/**
 * @brief Draws a place-holder "white diamond" prism for the specified entity.
 */
static void R_DrawNullModel(const r_entity_t *e) {

	R_EnableTexture(&texunit_diffuse, false);

	R_RotateForEntity(e);

	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0.0, 0.0, -16.0);
	for (int32_t i = 0; i <= 4; i++)
		glVertex3f(16.0 * cos(i * M_PI_2), 16.0 * sin(i * M_PI_2), 0.0);
	glEnd();

	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0.0, 0.0, 16.0);
	for (int32_t i = 4; i >= 0; i--)
		glVertex3f(16.0 * cos(i * M_PI_2), 16.0 * sin(i * M_PI_2), 0.0);
	glEnd();

	R_RotateForEntity(NULL);

	R_EnableTexture(&texunit_diffuse, true);
}

/**
 * @brief Draws all entities added to the view but missing a model.
 */
static void R_DrawNullModels(const r_entities_t *ents) {

	for (size_t i = 0; i < ents->count; i++) {
		const r_entity_t *e = ents->entities[i];

		if (e->effects & EF_NO_DRAW)
			continue;

		r_view.current_entity = e;

		R_DrawNullModel(e);
	}

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

	R_EnableTexture(&texunit_diffuse, false);

	R_ResetArrayState(); // default arrays

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	R_Color(color);

	for (size_t i = 0; i < ents->count; i++) {
		const r_entity_t *e = ents->entities[i];

		if (e->parent) {
			continue;
		}

		matrix4x4_t mat;
		Matrix4x4_CreateFromEntity(&mat, e->origin, vec3_origin, e->scale);

		glPushMatrix();
		glMultMatrixf((GLfloat *) mat.m);

		vec3_t verts[16];

		VectorSet(verts[0], e->mins[0], e->mins[1], e->mins[2]);
		VectorSet(verts[1], e->mins[0], e->mins[1], e->maxs[2]);
		VectorSet(verts[2], e->mins[0], e->maxs[1], e->maxs[2]);
		VectorSet(verts[3], e->mins[0], e->maxs[1], e->mins[2]);

		VectorSet(verts[4], e->maxs[0], e->maxs[1], e->mins[2]);
		VectorSet(verts[5], e->maxs[0], e->maxs[1], e->maxs[2]);
		VectorSet(verts[6], e->maxs[0], e->mins[1], e->maxs[2]);
		VectorSet(verts[7], e->maxs[0], e->mins[1], e->mins[2]);

		VectorSet(verts[8], e->maxs[0], e->mins[1], e->mins[2]);
		VectorSet(verts[9], e->maxs[0], e->mins[1], e->maxs[2]);
		VectorSet(verts[10], e->mins[0], e->mins[1], e->maxs[2]);
		VectorSet(verts[11], e->mins[0], e->mins[1], e->mins[2]);

		VectorSet(verts[12], e->mins[0], e->maxs[1], e->mins[2]);
		VectorSet(verts[13], e->mins[0], e->maxs[1], e->maxs[2]);
		VectorSet(verts[14], e->maxs[0], e->maxs[1], e->maxs[2]);
		VectorSet(verts[15], e->maxs[0], e->maxs[1], e->mins[2]);

		memcpy(r_state.vertex_array_3d, verts, sizeof(verts));

		glDrawArrays(GL_QUADS, 0, lengthof(verts));

		glPopMatrix();
	}

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	R_EnableTexture(&texunit_diffuse, true);
	
	R_Color(NULL);
}

/**
 * @brief Primary entry point for drawing all entities.
 */
void R_DrawEntities(void) {

	R_DrawBspInlineModels(&r_sorted_entities.bsp_inline_entities);

	R_DrawMeshModels(&r_sorted_entities.mesh_entities);

	R_DrawNullModels(&r_sorted_entities.null_entities);

	vec4_t yellow = { 0.9, 0.9, 0.0, 1.0 };
	R_DrawEntityBounds(&r_sorted_entities.mesh_entities, yellow);

	r_sorted_entities.bsp_inline_entities.count = 0;
	r_sorted_entities.mesh_entities.count = 0;
	r_sorted_entities.null_entities.count = 0;
}
