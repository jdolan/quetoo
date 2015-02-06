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

#define MESH_SHADOW_MIN 30.0
#define MESH_SHADOW_MAX 300.0
#define MESH_SHADOW_ALPHA 0.33

/*
 * @brief Sets the shadow color, which is dependent on the entity's alpha blend
 * and its distance from the shadow plane.
 */
static void R_SetMeshShadowColor_default(const r_entity_t *e, const r_shadow_t *s) {

	vec_t alpha = MESH_SHADOW_ALPHA;

	if (!r_programs->value) {
		const vec_t shadow = Clamp(s->shadow, MESH_SHADOW_MIN, MESH_SHADOW_MAX);
		alpha *= shadow / MESH_SHADOW_MAX;
	}

	if (e->effects & EF_BLEND)
		alpha *= e->color[3];

	vec4_t color = { 0.0, 0.0, 0.0, alpha * r_shadows->value };

	R_Color(color);
}

/*
 * @brief Projects the model view matrix for the entity onto the shadow plane.
 */
static void R_RotateForMeshShadow_default(const r_entity_t *e, r_shadow_t *s) {
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
 * @brief Draws the specified shadow for the given entity. A scissor test is
 * employed in order to clip shadows to the planes they are cast upon.
 */
static void R_DrawMeshShadow_default_(const r_entity_t *e, r_shadow_t *s) {

	R_SetMeshShadowColor_default(e, s);

	R_RotateForMeshShadow_default(e, s);

	R_EnableShadow(r_state.shadow_program, true);

	glStencilFunc(GL_EQUAL, (s->plane.num % 0xff) + 1, ~0);

	glDrawArrays(GL_TRIANGLES, 0, e->model->num_verts);

	R_EnableShadow(NULL, false);

	R_RotateForMeshShadow_default(NULL, NULL);
}

/*
 * @brief Re-draws the mesh using the stencil test. Meshes with a lighting
 * position above our view origin are not drawn.
 */
void R_DrawMeshShadow_default(const r_entity_t *e) {

	if (!r_shadows->value)
		return;

	if (r_draw_wireframe->value)
		return;

	if (e->effects & (EF_NO_LIGHTING | EF_NO_SHADOW))
		return;

	R_EnableLighting(NULL, false);

	R_EnableFog(false);

	if (!(e->effects & EF_BLEND))
		R_EnableBlend(true);

	R_EnableTexture(&texunit_diffuse, false);

	R_EnablePolygonOffset(GL_POLYGON_OFFSET_FILL, true);

	R_EnableStencilTest(GL_ZERO, true);

	r_shadow_t *s = e->lighting->shadows;

	for (uint16_t i = 0; i < lengthof(e->lighting->shadows); i++, s++) {

		if (s->shadow == 0.0)
			break;

#if 0
		if (e->effects & EF_CLIENT) {
			r_corona_t c;

			VectorCopy(s->illumination->pos, c.origin);
			VectorCopy(s->illumination->color, c.color);
			c.radius = s->illumination->radius / 8;

			R_AddCorona(&c);
		}
#endif

		r_view.current_shadow = s;

		R_DrawMeshShadow_default_(e, s);
	}

	r_view.current_shadow = NULL;

	R_EnableStencilTest(GL_KEEP, false);

	R_EnablePolygonOffset(GL_POLYGON_OFFSET_FILL, false);

	R_EnableTexture(&texunit_diffuse, true);

	if (!(e->effects & EF_BLEND))
		R_EnableBlend(false);

	R_EnableFog(true);

	R_EnableLighting(r_state.default_program, true);

	R_Color(NULL);
}
