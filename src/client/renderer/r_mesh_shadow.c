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

#define MESH_SHADOW_ALPHA 0.33

/*
 * @brief Sets the shadow color, which is dependent on the entity's alpha blend
 * and its distance from the shadow plane.
 */
static void R_SetMeshShadowColor_default(const r_entity_t *e, const r_shadow_t *s) {

	vec_t alpha = s->intensity * r_shadows->value * MESH_SHADOW_ALPHA;

	if (e->effects & EF_BLEND)
		alpha *= e->color[3];

	vec4_t color = { 0.0, 0.0, 0.0, alpha };

	R_Color(color);
}

/*
 * @brief Projects the model view matrix for the given entity onto the shadow
 * plane. A perspective shear is then applied using the standard planar shadow
 * deformation from SGI's cookbook, adjusted for Quake's negative planes:
 *
 * ftp://ftp.sgi.com/opengl/contrib/blythe/advanced99/notes/node192.html
 */
static void R_RotateForMeshShadow_default(const r_entity_t *e, r_shadow_t *s) {
	vec4_t pos, normal;
	matrix4x4_t proj, shear;
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

	// transform the light position and shadow plane into model space
	Matrix4x4_Transform(&e->inverse_matrix, s->illumination->light.origin, pos);
	pos[3] = 1.0;

	const vec_t *n = p->normal;
	Matrix4x4_TransformPositivePlane(&e->inverse_matrix, n[0], n[1], n[2], p->dist, normal);

	// calculate shearing, accounting for Quake's positive plane equation
	normal[3] = -normal[3];
	dot = DotProduct(pos, normal) + pos[3] * normal[3];

	shear.m[0][0] = dot - pos[0] * normal[0];
	shear.m[1][0] = 0.0 - pos[0] * normal[1];
	shear.m[2][0] = 0.0 - pos[0] * normal[2];
	shear.m[3][0] = 0.0 - pos[0] * normal[3];
	shear.m[0][1] = 0.0 - pos[1] * normal[0];
	shear.m[1][1] = dot - pos[1] * normal[1];
	shear.m[2][1] = 0.0 - pos[1] * normal[2];
	shear.m[3][1] = 0.0 - pos[1] * normal[3];
	shear.m[0][2] = 0.0 - pos[2] * normal[0];
	shear.m[1][2] = 0.0 - pos[2] * normal[1];
	shear.m[2][2] = dot - pos[2] * normal[2];
	shear.m[3][2] = 0.0 - pos[2] * normal[3];
	shear.m[0][3] = 0.0 - pos[3] * normal[0];
	shear.m[1][3] = 0.0 - pos[3] * normal[1];
	shear.m[2][3] = 0.0 - pos[3] * normal[2];
	shear.m[3][3] = dot - pos[3] * normal[3];

	glMultMatrixf((GLfloat *) shear.m);

	Matrix4x4_Copy(&s->matrix, &proj);
}

/*
 * @brief Draws the specified shadow for the given entity. A scissor test is
 * employed in order to clip shadows to the planes they are cast upon.
 */
static void R_DrawMeshShadow_default_(const r_entity_t *e, r_shadow_t *s) {

	R_SetMeshShadowColor_default(e, s);

	R_RotateForMeshShadow_default(e, s);

	R_EnableShadow(r_state.shadow_program, true);

	glStencilFunc(GL_EQUAL, (s->plane.num % 0xfe) + 1, ~0);

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

	glEnable(GL_POLYGON_OFFSET_FILL);

	R_EnableStencilTest(true, GL_ZERO);

	r_shadow_t *s = e->lighting->shadows;

	for (uint16_t i = 0; i < lengthof(e->lighting->shadows); i++, s++) {

		if (s->intensity == 0.0)
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

	R_EnableStencilTest(false, GL_ZERO);

	glDisable(GL_POLYGON_OFFSET_FILL);

	R_EnableTexture(&texunit_diffuse, true);

	if (!(e->effects & EF_BLEND))
		R_EnableBlend(false);

	R_EnableFog(true);

	R_EnableLighting(r_state.default_program, true);

	R_Color(NULL);
}
