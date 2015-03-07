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

#define MESH_SHADOW_ALPHA 0.33

/*
 * @brief Sets the shadow color, which is dependent on the entity's alpha blend
 * and its distance from the shadow plane.
 */
static void R_SetMeshShadowColor_default(const r_entity_t *e, const r_shadow_t *s) {

	vec_t alpha = MESH_SHADOW_ALPHA * s->shadow / s->illumination->diffuse;

	if (e->effects & EF_BLEND)
		alpha *= e->color[3];

	vec4_t color = { 0.0, 0.0, 0.0, alpha * r_shadows->value };

	R_Color(color);
}

/*
 * @brief Projects the model view matrix for the entity onto the shadow plane,
 * and concatenates it to the current model view matrix.
 */
static void R_RotateForMeshShadow_default(const r_entity_t *e, const r_shadow_t *s) {
	matrix4x4_t proj;
	vec_t dot;

	if (!e) {
		glPopMatrix();
		return;
	}

	const cm_bsp_plane_t *p = &s->plane;

	// project the entity onto the shadow plane
	vec3_t vx, vy, vz, t;
	Matrix4x4_ToVectors(&e->matrix, vx, vy, vz, t);

	dot = DotProduct(vx, p->normal);
	VectorMA(vx, -dot, p->normal, vx);

	dot = DotProduct(vy, p->normal);
	VectorMA(vy, -dot, p->normal, vy);

	dot = DotProduct(vz, p->normal);
	VectorMA(vz, -dot, p->normal, vz);

	dot = DotProduct(t, p->normal) - p->dist;
	VectorMA(t, -dot, p->normal, t);

	Matrix4x4_FromVectors(&proj, vx, vy, vz, t);

	glPushMatrix();

	glMultMatrixf((GLfloat *) proj.m);
}

/*
 * @brief Sets renderer state for the given entity and shadow.
 */
static void R_SetMeshShadowState_default(const r_entity_t *e, const r_shadow_t *s) {

	R_SetMeshShadowColor_default(e, s);

	R_RotateForMeshShadow_default(e, s);

	// TODO: This is a hack, but we don't want to toggle the program
	// just to re-calculate the shadow matrix. Figure out a clean way
	// to make this happen.
	R_UseProgram_shadow();

	glStencilFunc(GL_EQUAL, (s->plane.num % 0xff) + 1, ~0);
}

/*
 * @brief Restores renderer state for the given entity and shadow.
 */
static void R_ResetMeshShadowState_default(const r_entity_t *e __attribute__((unused)),
		const r_shadow_t *s __attribute__((unused))) {

	R_RotateForMeshShadow_default(NULL, NULL);
}

/*
 * @brief Draws the shadow `s` for the specified entity `e`.
 */
static void R_DrawMeshShadow_default_(const r_entity_t *e, const r_shadow_t *s) {

	R_SetMeshShadowState_default(e, s);

	glDrawArrays(GL_TRIANGLES, 0, e->model->num_verts);

	R_ResetMeshShadowState_default(e, s);
}

/*
 * @brief Draws all shadows for the specified entity.
 */
void R_DrawMeshShadow_default(const r_entity_t *e) {

	if (e->model->mesh->num_frames == 1) {
		R_SetArrayState(e->model);
	} else {
		R_ResetArrayState();

		R_InterpolateMeshModel(e);
	}

	r_shadow_t *s = e->lighting->shadows;
	for (size_t i = 0; i < lengthof(e->lighting->shadows); i++, s++) {

		if (s->shadow == 0.0)
			break;

#if 0
		if (e->effects & EF_CLIENT) {
			r_corona_t c;

			VectorCopy(s->illumination->light.origin, c.origin);
			VectorCopy(s->illumination->light.color, c.color);
			c.radius = s->illumination->light.radius / 8;

			R_AddCorona(&c);
		}
#endif

		r_view.current_shadow = s;

		R_DrawMeshShadow_default_(e, s);
	}

	r_view.current_shadow = NULL;
}

/*
 * @brief Draws all mesh model shadows for the current frame.
 */
void R_DrawMeshShadows_default(const r_entities_t *ents) {

	if (!r_shadows->value)
		return;

	if (r_draw_wireframe->value)
		return;

	R_EnableTexture(&texunit_diffuse, false);

	R_EnablePolygonOffset(GL_POLYGON_OFFSET_FILL, true);

	R_EnableShadow(r_state.shadow_program, true);

	R_EnableStencilTest(GL_ZERO, true);

	for (size_t i = 0; i < ents->count; i++) {
		const r_entity_t *e = ents->entities[i];

		if (e->effects & (EF_NO_LIGHTING | EF_NO_SHADOW))
			continue;

		r_view.current_entity = e;

		R_DrawMeshShadow_default(e);
	}

	r_view.current_entity = NULL;

	R_EnableShadow(NULL, false);

	R_EnableStencilTest(GL_KEEP, false);

	R_EnablePolygonOffset(GL_POLYGON_OFFSET_FILL, false);

	R_EnableTexture(&texunit_diffuse, true);

	R_Color(NULL);
}
