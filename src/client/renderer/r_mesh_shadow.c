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

/**
 * @brief Sets the shadow color, which is dependent on the entity's alpha blend
 * and its distance from the shadow plane.
 */
static void R_SetMeshShadowColor_default(const r_entity_t *e, const r_shadow_t *s) {

	vec_t alpha = MESH_SHADOW_ALPHA * s->shadow / s->illumination->diffuse;

	if (e->effects & EF_BLEND) {
		alpha *= e->color[3];
	}

	R_Color((const vec4_t) {
		0.0, 0.0, 0.0, alpha
	});
}

/**
 * @brief Projects the model view matrix for the entity onto the shadow plane,
 * and concatenates it to the current model view matrix.
 */
static void R_RotateForMeshShadow_default(const r_entity_t *e, const r_shadow_t *s) {
	matrix4x4_t proj, modelview;
	vec_t dot;

	if (!e) {
		R_PopMatrix(R_MATRIX_MODELVIEW);
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

	R_PushMatrix(R_MATRIX_MODELVIEW);

	R_GetMatrix(R_MATRIX_MODELVIEW, &modelview);

	Matrix4x4_Concat(&proj, &modelview, &proj);

	R_SetMatrix(R_MATRIX_MODELVIEW, &proj);
}

/**
 * @brief Calculates a perspective shearing matrix for the current shadow
 * ftp://ftp.sgi.com/opengl/contrib/blythe/advanced99/notes/node192.html
 */
static void R_CalculateShadowMatrix_default(const r_entity_t *e, const r_shadow_t *s) {

	// transform the light position and shadow plane into model space
	vec4_t light, plane;

	Matrix4x4_Transform(&e->inverse_matrix, s->illumination->light.origin, light);
	light[3] = 1.0;

	const cm_bsp_plane_t *p = &s->plane;

	Matrix4x4_TransformQuakePlane(&e->inverse_matrix, p->normal, p->dist, plane);
	plane[3] = -plane[3];

	// calculate the perspective-shearing matrix
	const vec_t dot = DotProduct(light, plane) + light[3] * plane[3];

	matrix4x4_t matrix;

	matrix.m[0][0] = dot - light[0] * plane[0];
	matrix.m[1][0] = 0.0 - light[0] * plane[1];
	matrix.m[2][0] = 0.0 - light[0] * plane[2];
	matrix.m[3][0] = 0.0 - light[0] * plane[3];
	matrix.m[0][1] = 0.0 - light[1] * plane[0];
	matrix.m[1][1] = dot - light[1] * plane[1];
	matrix.m[2][1] = 0.0 - light[1] * plane[2];
	matrix.m[3][1] = 0.0 - light[1] * plane[3];
	matrix.m[0][2] = 0.0 - light[2] * plane[0];
	matrix.m[1][2] = 0.0 - light[2] * plane[1];
	matrix.m[2][2] = dot - light[2] * plane[2];
	matrix.m[3][2] = 0.0 - light[2] * plane[3];
	matrix.m[0][3] = 0.0 - light[3] * plane[0];
	matrix.m[1][3] = 0.0 - light[3] * plane[1];
	matrix.m[2][3] = 0.0 - light[3] * plane[2];
	matrix.m[3][3] = dot - light[3] * plane[3];

	Matrix4x4_Transform(&r_view.matrix, s->illumination->light.origin, light);
	light[3] = s->illumination->light.radius;

	Matrix4x4_TransformQuakePlane(&r_view.matrix, p->normal, p->dist, plane);
	plane[3] = -plane[3];

	R_SetMatrix(R_MATRIX_SHADOW, &matrix);

	R_UpdateShadowLightPlane_shadow(light, plane);
}

/**
 * @brief Sets renderer state for the given entity and shadow.
 */
static void R_SetMeshShadowState_default(const r_entity_t *e, const r_shadow_t *s) {

	// setup VBO states
	R_SetArrayState(e->model);

	R_SetMeshShadowColor_default(e, s);

	R_RotateForMeshShadow_default(e, s);

	R_CalculateShadowMatrix_default(e, s);

	// setup lerp for animating models
	if (e->old_frame != e->frame) {
		R_UseInterpolation(e->lerp);
	}

	R_StencilFunc(GL_EQUAL, R_STENCIL_REF(s->plane.num), ~0);
}

/**
 * @brief Restores renderer state for the given entity and shadow.
 */
static void R_ResetMeshShadowState_default(const r_entity_t *e,
        const r_shadow_t *s) {

	R_RotateForMeshShadow_default(NULL, NULL);

	R_ResetArrayState();

	R_UseInterpolation(0.0);
}

/**
 * @brief Draws the shadow `s` for the specified entity `e`.
 */
static void R_DrawMeshShadow_default_(const r_entity_t *e, const r_shadow_t *s) {

	R_SetMeshShadowState_default(e, s);

	R_DrawArrays(GL_TRIANGLES, 0, e->model->num_elements);

	R_ResetMeshShadowState_default(e, s);
}

/**
* @brief Stores list for global shadow intensities
*/
typedef struct {
	const r_entity_t *entity;
	const r_shadow_t *shadow;
} r_sorted_entity_shadow_t;

static r_sorted_entity_shadow_t r_sorted_shadows[MAX_SHADOWS * MAX_ENTITIES];

/**
 * @brief
 */
static int R_SortEntityShadows(const void *a, const void *b) {
	const r_sorted_entity_shadow_t *sa = (const r_sorted_entity_shadow_t *) a;
	const r_sorted_entity_shadow_t *sb = (const r_sorted_entity_shadow_t *) b;

	return Sign(sb->shadow->shadow - sa->shadow->shadow);
}

/**
 * @brief Draws all mesh model shadows for the current frame.
 */
void R_DrawMeshShadows_default(const r_entities_t *ents) {

	if (!r_shadows->value) {
		return;
	}

	if (r_draw_wireframe->value) {
		return;
	}

	// calculate shadows
	r_sorted_entity_shadow_t *sorted = r_sorted_shadows;
	for (size_t i = 0; i < ents->count; i++) {
		const r_entity_t *e = ents->entities[i];

		if (e->effects & (EF_NO_LIGHTING | EF_NO_SHADOW)) {
			continue;
		}

		r_shadow_t *s = e->lighting->shadows;
		for (size_t i = 0; i < lengthof(e->lighting->shadows); i++, s++) {

			if (s->shadow == 0.0) {
				break;
			}

			sorted->entity = e;
			sorted->shadow = s;

			sorted++;
		}
	}

	if (sorted == r_sorted_shadows) {
		return;
	}

	const size_t num_sorted = (uint32_t) (ptrdiff_t) (sorted - r_sorted_shadows);

	qsort(r_sorted_shadows, num_sorted, sizeof(r_sorted_entity_shadow_t), R_SortEntityShadows);

	R_EnablePolygonOffset(true);

	R_PolygonOffset(R_OFFSET_FACTOR, R_OFFSET_UNITS);

	R_EnableShadow(true);

	R_EnableStencilTest(GL_ZERO, true);

	for (size_t i = 0; i < num_sorted; ++i) {
		const r_sorted_entity_shadow_t *shadow = &r_sorted_shadows[i];
		const r_entity_t *e = shadow->entity;
		const r_shadow_t *s = shadow->shadow;

		r_view.current_entity = e;
		r_view.current_shadow = s;

		R_DrawMeshShadow_default_(e, s);
	}

	r_view.current_entity = NULL;

	R_EnableShadow(false);

	R_EnableStencilTest(GL_KEEP, false);

	R_EnablePolygonOffset(false);

	R_Color(NULL);
}
