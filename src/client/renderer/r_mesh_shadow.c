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

	vec_t alpha = r_shadows->value * MESH_SHADOW_ALPHA;

	if (e->effects & EF_BLEND)
		alpha *= e->color[3];

	alpha *= 1.0 - (e->origin[2] - e->lighting->plane.dist) / LIGHTING_SHADOW_DISTANCE;

	vec4_t color = { 0.0, 0.0, 0.0, alpha };

	R_Color(color);
}

/*
 * @brief Applies translation, rotation and scale for the shadow of the specified
 * entity. In order to reuse the vertex arrays from the primary rendering
 * pass, the light position and ground normal are transformed into model-view space.
 */
static void R_RotateForMeshShadow_default(const r_entity_t *e) {
	vec4_t pos, normal;
	matrix4x4_t proj;

	if (!e) {
		glPopMatrix();
		return;
	}

	R_TransformForEntity(e, e->lighting->pos, pos);
	pos[3] = 0.0;

	Matrix4x4_TransformPositivePlane(&e->inverse_matrix, e->lighting->plane.normal[0],
			e->lighting->plane.normal[1], e->lighting->plane.normal[2], e->lighting->plane.dist,
			normal);

	glPushMatrix();

	glTranslatef(0.0, 0.0, normal[3] + 1.0);

	glRotatef(-e->angles[ROLL], 1.0, 0.0, 0.0);
	glRotatef(-e->angles[PITCH], 0.0, 1.0, 0.0);

	glScalef(1.0, 1.0, 0.0);

	normal[3] = -normal[3];
	const vec_t dot = DotProduct(pos, normal) + pos[3] * normal[3];

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

	R_EnableTexture(&texunit_diffuse, false);

	R_SetMeshShadowColor_default(e);

	if (!(e->effects & EF_BLEND))
		R_EnableBlend(true);

	R_EnableStencilTest(true);

	R_RotateForMeshShadow_default(e);
}

/*
 * @brief
 */
static void R_ResetMeshShadowState_default(const r_entity_t *e) {

	R_RotateForMeshShadow_default(NULL);

	R_EnableStencilTest(false);

	if (!(e->effects & EF_BLEND))
		R_EnableBlend(false);

	R_Color(NULL);

	R_EnableTexture(&texunit_diffuse, true);
}

/*
 * @brief Re-draws the mesh using the stencil test. Meshes with stale lighting
 * information, or with a lighting point above our view, are not drawn.
 */
void R_DrawMeshShadow_default(const r_entity_t *e) {

	if (!r_shadows->value)
		return;

	if (r_draw_wireframe->value)
		return;

	if (e->effects & EF_NO_SHADOW)
		return;

	if (e->lighting->plane.type == PLANE_NONE)
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
