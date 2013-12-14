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

#include "r_local.h"

// entities are chained together by type to reduce state changes
typedef struct r_entities_s {
	r_entity_t *bsp;
	r_entity_t *mesh;
	r_entity_t *mesh_alpha_test;
	r_entity_t *mesh_blend;
	r_entity_t *null;
} r_entities_t;

r_entities_t r_entities;

/*
 * @brief Returns the appropriate entity list for the specified entity.
 */
static r_entity_t **R_EntityList(const r_entity_t *e) {

	if (!e->model)
		return &r_entities.null;

	if (IS_BSP_INLINE_MODEL(e->model))
		return &r_entities.bsp;

	// mesh models

	if (e->effects & EF_ALPHATEST)
		return &r_entities.mesh_alpha_test;

	if (e->effects & EF_BLEND)
		return &r_entities.mesh_blend;

	return &r_entities.mesh;
}

/*
 * @brief Copies the specified entity into the view structure and inserts it
 * into the appropriate sorted chain. The sorted chains allow for object
 * instancing.
 */
const r_entity_t *R_AddEntity(const r_entity_t *ent) {
	r_entity_t *e, *in, **ents;

	if (r_view.num_entities == MAX_ENTITIES) {
		Com_Warn("MAX_ENTITIES reached\n");
		return NULL;
	}

	// copy in to renderer array
	e = &r_view.entities[r_view.num_entities++];
	*e = *ent;

	// and insert into the sorted draw list
	ents = R_EntityList(e);
	in = *ents;

	while (in) {

		if (in->model == e->model) {
			e->next = in->next;
			in->next = e;
			break;
		}

		in = in->next;
	}

	// push to the head if necessary
	if (!in) {
		e->next = *ents;
		*ents = e;
	}

	return e;
}

/*
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

/*
 * @brief Applies translation, rotation, and scale for the specified entity.
 */
void R_RotateForEntity(const r_entity_t *e) {
	GLfloat mat[16];

	if (!e) {
		glPopMatrix();
		return;
	}

	Matrix4x4_ToArrayFloatGL(&e->matrix, mat);

	glPushMatrix();

	glMultMatrixf(mat);
}

/*
 * @brief Transforms a point by the inverse of the world-model matrix for the
 * specified entity, translating and rotating it into the entity's model-view.
 */
void R_TransformForEntity(const r_entity_t *e, const vec3_t in, vec3_t out) {
	Matrix4x4_Transform(&e->inverse_matrix, in, out);
}

/*
 * @brief Applies any configuration and tag alignment, populating the model-view
 * matrix for the entity in the process.
 */
static void R_SetMatrixForEntity(r_entity_t *e) {

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

		R_ApplyMeshModelConfig(e);

		Matrix4x4_CreateFromEntity(&e->matrix, e->origin, e->angles, e->scale);

		R_ApplyMeshModelTag(e); // interpolate and concatenate the tag matrix

		Matrix4x4_Invert_Simple(&e->inverse_matrix, &e->matrix);

		Matrix4x4_Transform(&e->matrix, vec3_origin, e->origin);
		Matrix4x4_Transform(&e->matrix, vec3_forward, forward);

		VectorAngles(forward, e->angles);
		return;
	}

	if (IS_MESH_MODEL(e->model)) {
		R_ApplyMeshModelConfig(e);
	}

	Matrix4x4_CreateFromEntity(&e->matrix, e->origin, e->angles, e->scale);
	Matrix4x4_Invert_Simple(&e->inverse_matrix, &e->matrix);
}

/*
 * @brief Dispatches the appropriate sub-routine for frustum-culling the entity.
 *
 * @return True if the entity is culled (fails frustum test), false otherwise.
 */
static _Bool R_CullEntity(r_entity_t *e) {

	if (!e->model) {
		e->culled = false;
	} else if (IS_BSP_INLINE_MODEL(e->model)) {
		e->culled = R_CullBspModel(e);
	} else { // mesh model
		e->culled = R_CullMeshModel(e);
	}

	return e->culled;
}

/*
 * @brief Performs a frustum-cull of all entities. This is performed in a separate
 * thread while the renderer draws the world. Entities which pass a frustum
 * cull will also have their static lighting information updated.
 */
void R_CullEntities(void *data __attribute__((unused))) {
	r_entity_t *e = r_view.entities;
	uint16_t i;

	for (i = 0, e = r_view.entities; i < r_view.num_entities; i++, e++) {

		if (!R_CullEntity(e)) { // cull it

			R_SetMatrixForEntity(e); // set the transform matrix

			if (IS_MESH_MODEL(e->model)) {
				R_UpdateMeshModelLighting(e);
			}
		}
	}
}

/*
 * @brief
 */
void R_DrawBspEntities() {

	const r_entity_t *e = r_entities.bsp;

	while (e) {
		if (!e->culled) {
			r_view.current_entity = e;
			R_DrawBspInlineModel(e);
		}
		e = e->next;
	}

	r_view.current_entity = NULL;
}

/*
 * @brief
 */
static void R_DrawMeshEntities(r_entity_t *ents) {

	const r_entity_t *e = ents;

	while (e) {
		if (!e->culled) {
			r_view.current_entity = e;
			R_DrawMeshModel(e);
		}
		e = e->next;
	}

	r_view.current_entity = NULL;
}

/*
 * @brief
 */
void R_DrawOpaqueMeshEntities(void) {

	if (!r_entities.mesh)
		return;

	R_DrawMeshEntities(r_entities.mesh);
}

/*
 * @brief
 */
void R_DrawAlphaTestMeshEntities(void) {

	if (!r_entities.mesh_alpha_test)
		return;

	R_EnableAlphaTest(true);

	R_DrawMeshEntities(r_entities.mesh_alpha_test);

	R_EnableAlphaTest(false);
}

/*
 * @brief
 */
static void R_DrawBlendMeshEntities(void) {

	if (!r_entities.mesh_blend)
		return;

	R_EnableBlend(true);

	R_DrawMeshEntities(r_entities.mesh_blend);

	R_EnableBlend(false);
}

/*
 * @brief Draws a place-holder "white diamond" prism for the specified entity.
 */
static void R_DrawNullModel(const r_entity_t *e) {
	int32_t i;

	R_EnableTexture(&texunit_diffuse, false);

	R_RotateForEntity(e);

	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0.0, 0.0, -16.0);
	for (i = 0; i <= 4; i++)
		glVertex3f(16.0 * cos(i * M_PI_2), 16.0 * sin(i * M_PI_2), 0.0);
	glEnd();

	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0.0, 0.0, 16.0);
	for (i = 4; i >= 0; i--)
		glVertex3f(16.0 * cos(i * M_PI_2), 16.0 * sin(i * M_PI_2), 0.0);
	glEnd();

	R_RotateForEntity(NULL);

	R_EnableTexture(&texunit_diffuse, true);
}

/*
 * @brief
 */
static void R_DrawNullEntities(void) {
	const r_entity_t *e;

	if (!r_entities.null)
		return;

	e = r_entities.null;

	while (e) {
		R_DrawNullModel(e);
		e = e->next;
	}
}

/*
 * @brief Primary entry point for drawing all entities.
 */
void R_DrawEntities(void) {

	if (r_draw_wireframe->value) {
		R_EnableTexture(&texunit_diffuse, false);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	R_DrawBspEntities();

	R_EnableLighting(r_state.default_program, true);

	R_DrawOpaqueMeshEntities();

	R_DrawAlphaTestMeshEntities();

	R_DrawBlendMeshEntities();

	if (r_draw_wireframe->value) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		R_EnableTexture(&texunit_diffuse, true);
	}

	R_EnableLighting(NULL, false);

	R_Color(NULL);

	R_DrawNullEntities();

	// clear draw lists for next frame
	memset(&r_entities, 0, sizeof(r_entities));
}
