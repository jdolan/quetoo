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

#define MESH_SHADOW_ALPHA 0.15

/*
 * @brief Sets the shadow color, which is dependent on the entity's alpha blend
 * and its distance from the shadow origin.
 */
static void R_SetMeshShadowColor_default(const r_entity_t *e) {

	vec_t alpha = /*e->lighting->shadow */r_shadows->value * MESH_SHADOW_ALPHA;

	if (e->effects & EF_BLEND)
		alpha *= e->color[3];

	while (e->parent) {
		e = e->parent;
	}

	vec4_t color = { 0.0, 0.0, 0.0, alpha };

	R_Color(color);
}

/*
 * @brief Applies translation, rotation, and projection (scale) for the entity.
 * We borrow the standard projection matrix from SGI's cook book, and adjust it
 * for Quake's negative-distance planes:
 *
 * ftp://ftp.sgi.com/opengl/contrib/blythe/advanced99/notes/node192.html
 *
 * Light sources are treated as directional rather than point lights so that
 * they produce smaller shadow projections and thus fewer artifacts.
 */
static void R_RotateForMeshShadow_default(const r_entity_t *e) {
	vec4_t pos, normal, origin;
	matrix4x4_t proj;
	vec_t dot;

	if (!e) {
		glPopMatrix();
		return;
	}

	// transform the lighting position into model space
	R_TransformForEntity(e, e->lighting->pos, pos);
	pos[3] = 0.0;

	// along with the impacted plane that we will project onto
	Matrix4x4_TransformPositivePlane(&e->inverse_matrix, e->lighting->plane.normal[0],
			e->lighting->plane.normal[1], e->lighting->plane.normal[2], e->lighting->plane.dist,
			normal);

	// project the shadow origin onto the impacted plane in model space
	dot = DotProduct(vec3_origin, normal) - normal[3];
	VectorScale(normal, -dot, origin);

	glPushMatrix();

	glTranslatef(origin[0], origin[1], origin[2]);

	glRotatef(-e->angles[ROLL], 1.0, 0.0, 0.0);
	glRotatef(-e->angles[PITCH], 0.0, 1.0, 0.0);

	glScalef(1.0, 1.0, 0.0);

	// calculate the projection, accounting for Quake's positive plane equation
	normal[3] = -normal[3];
	dot = DotProduct(pos, normal) + pos[3] * normal[3];

	proj.m[0][0] = dot - pos[0] * normal[0];
	proj.m[1][0] = 0.0 - pos[0] * normal[1];
	proj.m[2][0] = 0.0 - pos[0] * normal[2];
	proj.m[3][0] = 0.0 - pos[0] * normal[3];
	proj.m[0][1] = 0.0 - pos[1] * normal[0];
	proj.m[1][1] = dot - pos[1] * normal[1];
	proj.m[2][1] = 0.0 - pos[1] * normal[2];
	proj.m[3][1] = 0.0 - pos[1] * normal[3];
	proj.m[0][2] = 0.0 - pos[2] * normal[0];
	proj.m[1][2] = 0.0 - pos[2] * normal[1];
	proj.m[2][2] = dot - pos[2] * normal[2];
	proj.m[3][2] = 0.0 - pos[2] * normal[3];
	proj.m[0][3] = 0.0 - pos[3] * normal[0];
	proj.m[1][3] = 0.0 - pos[3] * normal[1];
	proj.m[2][3] = 0.0 - pos[3] * normal[2];
	proj.m[3][3] = dot - pos[3] * normal[3];

	glMultMatrixf((GLfloat *) proj.m);
}

/*
 * @brief Sets GL state to draw the specified entity shadow.
 */
static void R_SetMeshShadowState_default(const r_entity_t *e) {

	R_EnableLighting(NULL, false);

	R_EnableTexture(&texunit_diffuse, false);

	R_SetMeshShadowColor_default(e);

	if (!(e->effects & EF_BLEND))
		R_EnableBlend(true);

	glDisable(GL_DEPTH_TEST);

	R_EnableStencilTest(true, GL_ZERO);

	glStencilFunc(GL_EQUAL, R_STENCIL_REF(&e->lighting->plane), ~0);

	R_RotateForMeshShadow_default(e);
}

/*
 * @brief Restores GL state.
 */
static void R_ResetMeshShadowState_default(const r_entity_t *e) {

	R_RotateForMeshShadow_default(NULL);

	R_EnableStencilTest(false, GL_ZERO);

	glEnable(GL_DEPTH_TEST);

	if (!(e->effects & EF_BLEND))
		R_EnableBlend(false);

	R_Color(NULL);

	R_EnableTexture(&texunit_diffuse, true);

	R_EnableLighting(r_state.default_program, true);
}

/*
 * @brief Re-draws the mesh using the stencil test. Meshes with a lighting
 * position above our view origin, are not drawn.
 */
void R_DrawMeshShadow_default(const r_entity_t *e) {

	if (!r_shadows->value)
		return;

	if (r_draw_wireframe->value)
		return;

	if (e->effects & EF_NO_SHADOW)
		return;

	if (e->lighting->shadow == 0.0)
		return;

#if 0
	r_corona_t c;

	VectorCopy(e->lighting->pos, c.origin);
	VectorCopy(e->lighting->color, c.color);
	c.radius = e->lighting->light;
	c.flicker = 0.0;

	R_AddCorona(&c);
#endif

	R_SetMeshShadowState_default(e);

	glDrawArrays(GL_TRIANGLES, 0, e->model->num_verts);

	R_ResetMeshShadowState_default(e);
}
