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
 * R_EntityList
 *
 * Returns the appropriate entity list for the specified entity.
 */
static r_entity_t **R_EntityList(const r_entity_t *e) {

	if (!e->model)
		return &r_entities.null;

	if (e->model->type == mod_bsp_submodel)
		return &r_entities.bsp;

	// mesh models

	if (e->effects & EF_ALPHATEST)
		return &r_entities.mesh_alpha_test;

	if (e->effects & EF_BLEND)
		return &r_entities.mesh_blend;

	return &r_entities.mesh;
}

/*
 * R_AddEntity
 *
 * Copies the specified entity into the view structure, calculating its
 * model-view matrix and performing a frustum cull in the process. Entities
 * which pass the frustum cull are added to draw lists, sorted by model to
 * allow instancing.
 */
const r_entity_t *R_AddEntity(const r_entity_t *ent) {
	r_entity_t *e, *in, **ents;

	if (r_view.num_entities == MAX_ENTITIES) {
		Com_Warn("R_AddEntity: MAX_ENTITIES reached.\n");
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
 * R_RotateForEntity
 *
 * Applies translation, rotation, and scale for the specified entity.
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
 *
 * R_TransformForEntity
 *
 * Transforms a point by the inverse of the world-model matrix for the
 * specified entity, translating and rotating it into the entity's model-view.
 */
void R_TransformForEntity(const r_entity_t *e, const vec3_t in, vec3_t out) {
	matrix4x4_t mat;

	Matrix4x4_Invert_Simple(&mat, &e->matrix);

	Matrix4x4_Transform(&mat, in, out);
}

/*
 * R_SetMatrixForEntity
 *
 * Applies any configuration and tag alignment, populating the model-view
 * matrix for the entity in the process.
 */
static void R_SetMatrixForEntity(r_entity_t *e) {

	if (e->parent) {
		vec3_t tmp;

		if (!IS_MESH_MODEL(e->model)) {
			Com_Warn("R_SetMatrixForEntity: Invalid model for linked entity\n");
			return;
		}

		VectorClear(e->origin);
		VectorClear(e->angles);

		R_ApplyMeshModelConfig(e);

		Matrix4x4_CreateFromQuakeEntity(&e->matrix, e->origin[0], e->origin[1],
				e->origin[2], e->angles[0], e->angles[1], e->angles[2],
				e->scale);

		R_ApplyMeshModelTag(e);

		Matrix4x4_ToVectors(&e->matrix, tmp, tmp, tmp, e->origin);
		VectorCopy(e->parent->angles, e->angles);
		return;
	}

	if (IS_MESH_MODEL(e->model)) {
		R_ApplyMeshModelConfig(e);
	}

	Matrix4x4_CreateFromQuakeEntity(&e->matrix, e->origin[0], e->origin[1],
			e->origin[2], e->angles[0], e->angles[1], e->angles[2], e->scale);
}

/*
 * R_CullEntity
 *
 * Dispatches the appropriate sub-routine for frustum-culling the entity.
 */
static boolean_t R_CullEntity(r_entity_t *e) {

	if (!e->model) {
		e->culled = false;
	} else if (e->model->type == mod_bsp_submodel) {
		e->culled = R_CullBspModel(e);
	} else { // mesh model
		e->culled = R_CullMeshModel(e);
	}

	return e->culled;
}

/*
 * R_CullEntities
 *
 * Performs a frustum-cull of all entities. This is performed in a separate
 * thread while the renderer draws the world. Entities which pass a frustum
 * cull will also have their static lighting information updated.
 */
void R_CullEntities(void *data __attribute__((unused))) {
	r_entity_t *e = r_view.entities;
	unsigned short i;

	for (i = 0, e = r_view.entities; i < r_view.num_entities; i++, e++) {

		R_SetMatrixForEntity(e); // set the transform matrix

		if (!R_CullEntity(e)) { // cull it

			if (IS_MESH_MODEL(e->model)) {
				R_UpdateMeshModelLighting(e);
			}
		}
	}
}

/*
 * R_DrawBspEntities
 */
static void R_DrawBspEntities() {
	const r_entity_t *e;

	e = r_entities.bsp;

	while (e) {
		if (!e->culled)
			R_DrawBspModel(e);
		e = e->next;
	}
}

/*
 * R_DrawMeshEntities
 */
static void R_DrawMeshEntities(r_entity_t *ents) {
	r_entity_t *e;

	e = ents;

	while (e) {
		if (!e->culled)
			R_DrawMeshModel(e);
		e = e->next;
	}
}

/*
 * R_DrawOpaqueMeshEntities
 */
static void R_DrawOpaqueMeshEntities() {

	if (!r_entities.mesh)
		return;

	R_EnableLighting(r_state.mesh_program, true);

	R_DrawMeshEntities(r_entities.mesh);

	R_EnableLighting(NULL, false);
}

/*
 * R_DrawAlphaTestMeshEntities
 */
static void R_DrawAlphaTestMeshEntities() {

	if (!r_entities.mesh_alpha_test)
		return;

	R_EnableAlphaTest(true);

	R_EnableLighting(r_state.mesh_program, true);

	R_DrawMeshEntities(r_entities.mesh_alpha_test);

	R_EnableLighting(NULL, false);

	R_EnableAlphaTest(false);
}

/*
 * R_DrawBlendMeshEntities
 */
static void R_DrawBlendMeshEntities() {

	if (!r_entities.mesh_blend)
		return;

	R_EnableBlend(true);

	R_DrawMeshEntities(r_entities.mesh_blend);

	R_EnableBlend(false);
}

/*
 * R_DrawNullModel
 *
 * Draws a place-holder "white diamond" prism for the specified entity.
 */
static void R_DrawNullModel(const r_entity_t *e) {
	int i;

	R_EnableTexture(&texunit_diffuse, false);

	R_RotateForEntity(e);

	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0.0, 0.0, -16.0);
	for (i = 0; i <= 4; i++)
		glVertex3f(16.0 * cos(i * M_PI / 2.0), 16.0 * sin(i * M_PI / 2.0), 0.0);
	glEnd();

	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0.0, 0.0, 16.0);
	for (i = 4; i >= 0; i--)
		glVertex3f(16.0 * cos(i * M_PI / 2.0), 16.0 * sin(i * M_PI / 2.0), 0.0);
	glEnd();

	R_RotateForEntity(NULL);

	R_EnableTexture(&texunit_diffuse, true);
}

/*
 * R_DrawNullEntities
 */
static void R_DrawNullEntities() {
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
 * R_DrawEntities
 *
 * Primary entry point for drawing all entities.
 */
void R_DrawEntities(void) {

	if (r_draw_wireframe->value) {
		R_BindTexture(r_null_image->texnum);
		R_EnableTexture(&texunit_diffuse, false);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	R_DrawOpaqueMeshEntities();

	R_DrawAlphaTestMeshEntities();

	R_DrawBlendMeshEntities();

	if (r_draw_wireframe->value) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		R_EnableTexture(&texunit_diffuse, true);
	}

	glColor4ubv(color_white);

	R_DrawBspEntities();

	R_DrawNullEntities();

	// clear draw lists for next frame
	memset(&r_entities, 0, sizeof(r_entities));
}
