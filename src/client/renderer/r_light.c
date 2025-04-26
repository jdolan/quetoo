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

r_lights_t r_lights;

/**
 * @brief
 */
void R_AddLight(r_view_t *view, const r_light_t *l) {

	if (view->num_lights == MAX_LIGHTS) {
		Com_Debug(DEBUG_RENDERER, "MAX_LIGHTS\n");
		return;
	}

	switch (l->type) {
		case LIGHT_AMBIENT:
		case LIGHT_SUN:
		case LIGHT_POINT:
		case LIGHT_SPOT:
		case LIGHT_BRUSH_SIDE:
			if (r_shadowmap->integer < 2) {
				return;
			}
			break;
		case LIGHT_DYNAMIC:
			break;
		default:
			return;
	}

	if (R_CulludeBox(view, l->bounds)) {
		return;
	}

	r_light_t *out = &view->lights[view->num_lights++];

	*out = *l;

	out->index = -1;
}

/**
 * @brief
 */
static void R_AddLightUniform(r_light_t *in) {

	if (r_lights.block.num_lights == MAX_LIGHT_UNIFORMS) {
		Com_Warn("MAX_LIGHT_UNIFORMS\n");
		return;
	}

	in->index = r_lights.block.num_lights++;

	r_light_uniform_t *out = &r_lights.block.lights[in->index];

	out->model = Vec3_ToVec4(in->origin, in->radius);
	out->mins = Vec3_ToVec4(in->bounds.mins, in->size);
	out->maxs = Vec3_ToVec4(in->bounds.maxs, in->atten);
	out->position = Vec3_ToVec4(Mat4_Transform(r_uniforms.block.view, in->origin), in->type);
	out->normal = Mat4_TransformPlane(r_uniforms.block.view, Vec4_XYZ(in->normal), in->normal.w);
	out->color = Vec3_ToVec4(in->color, in->intensity);
}

/**
 * @brief Cull lights by occlusion queries, and transform them into view space.
 */
void R_UpdateLights(r_view_t *view) {

	r_light_uniform_block_t *out = &r_lights.block;

	memset(out, 0, sizeof(*out));

	const float f = BSP_BLOCK_SIZE * 0.5f;

	out->light_projection_ortho = Mat4_FromOrtho(-f, f, -f, f, NEAR_DIST, MAX_WORLD_DIST);
	out->light_projection = Mat4_FromFrustum(-1.f, 1.f, -1.f, 1.f, NEAR_DIST, MAX_WORLD_DIST);
	out->light_view[0] = Mat4_LookAt(Vec3_Zero(), Vec3( 1.f,  0.f,  0.f), Vec3(0.f, -1.f,  0.f));
	out->light_view[1] = Mat4_LookAt(Vec3_Zero(), Vec3(-1.f,  0.f,  0.f), Vec3(0.f, -1.f,  0.f));
	out->light_view[2] = Mat4_LookAt(Vec3_Zero(), Vec3( 0.f,  1.f,  0.f), Vec3(0.f,  0.f,  1.f));
	out->light_view[3] = Mat4_LookAt(Vec3_Zero(), Vec3( 0.f, -1.f,  0.f), Vec3(0.f,  0.f, -1.f));
	out->light_view[4] = Mat4_LookAt(Vec3_Zero(), Vec3( 0.f,  0.f,  1.f), Vec3(0.f, -1.f,  0.f));
	out->light_view[5] = Mat4_LookAt(Vec3_Zero(), Vec3( 0.f,  0.f, -1.f), Vec3(0.f, -1.f,  0.f));

	r_light_t *l = view->lights;
	for (int32_t i = 0; i < view->num_lights; i++, l++) {

		if (l->bsp_light && l->bsp_light->occluded) {
			continue;
		}

		if (r_draw_light_bounds->value) {
			R_Draw3DBox(l->bounds, Color3fv(l->color), false);
		}

		R_AddLightUniform(l);
	}

	r_stats.lights = out->num_lights;

	glBindBuffer(GL_UNIFORM_BUFFER, r_lights.buffer);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(*out), out);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

/**
 * @brief
 */
void R_ActiveLights(const r_view_t *view, const box3_t bounds, GLint name) {

	memset(r_lights.active_lights, 0, sizeof(r_lights.active_lights));

	const r_light_uniform_t *light = r_lights.block.lights;
	for (int32_t i = 0; i < r_lights.block.num_lights; i++, light++) {

		const box3_t light_bounds = Box3(Vec4_XYZ(light->mins), Vec4_XYZ(light->maxs));
		if (Box3_Intersects(light_bounds, bounds)) {
			r_lights.active_lights[i / 32] |= (1 << (i % 32));
		}
	}

	glUniform1uiv(name, lengthof(r_lights.active_lights), r_lights.active_lights);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_InitLights(void) {

	memset(&r_lights, 0, sizeof(r_lights));

	glGenBuffers(1, &r_lights.buffer);
	glBindBuffer(GL_UNIFORM_BUFFER, r_lights.buffer);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(r_lights.block), &r_lights.block, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, 1, r_lights.buffer);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_ShutdownLights(void) {

	glDeleteBuffers(1, &r_lights.buffer);
}
