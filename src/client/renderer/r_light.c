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

	if (R_CulludeSphere(view, l->origin, l->radius)) {
		return;
	}

	view->lights[view->num_lights++] = *l;
}

/**
 * @brief
 */
static void R_AddLightUniform(const r_light_uniform_t *l) {

	if (r_lights.block.num_lights == MAX_LIGHTS) {
		Com_Debug(DEBUG_RENDERER, "MAX_LIGHTS\n");
		return;
	}

	r_lights.block.lights[r_lights.block.num_lights++] = *l;
}

/**
 * @brief Transforms all active light sources to view space for rendering.
 */
void R_UpdateLights(const r_view_t *view) {

	r_lights.block.num_lights = 0;

	if (view->type == VIEW_MAIN) {

		const r_bsp_light_t *in = r_world_model->bsp->lights;
		for (int32_t i = 0; i < r_world_model->bsp->num_lights; i++, in++) {

			if (in->type == LIGHT_PATCH) {

				if (in->origin.w < 64.f) {
					continue;
				}

				if (Vec3_Distance(view->origin, Vec4_XYZ(in->origin)) > 2048.f) {
					continue;
				}

				const r_entity_t *e = view->entities;
				for (int32_t j = 0; j < view->num_entities; j++, e++) {

					if (e->model && Box3_Intersects(in->bounds, e->abs_model_bounds)) {
						R_AddLightUniform(&(const r_light_uniform_t) {
							.model = in->origin,
							.position = Vec3_ToVec4(Mat4_Transform(r_uniforms.block.view, Vec4_XYZ(in->origin)), in->type),
							.normal = Mat4_TransformPlane(r_uniforms.block.view, Vec4_XYZ(in->normal), in->normal.w),
							.color = in->color,
						});
						break;
					}
				}
			}
		}
	}


	const r_light_t *l = view->lights;
	for (int32_t i = 0; i < view->num_lights; i++, l++) {

		R_AddLightUniform(&(r_light_uniform_t) {
			.model = Vec3_ToVec4(l->origin, l->radius),
			.position = Vec3_ToVec4(Mat4_Transform(r_uniforms.block.view, l->origin), LIGHT_DYNAMIC),
			.color = Vec3_ToVec4(l->color, l->intensity),
		});
	}

	const r_entity_t *e = view->entities;
	for (int32_t i = 0; i < view->num_entities; i++, e++) {

		if (IS_MESH_MODEL(e->model)) {

			const vec3_t origin = Vec3_Fmaf(e->origin, 64.f, Vec3_Up());

			R_AddLightUniform(&(r_light_uniform_t) {
				.model = Vec3_ToVec4(origin, 96.f),
				.position = Vec3_ToVec4(Mat4_Transform(r_uniforms.block.view, origin), LIGHT_AMBIENT),
				.color = Vec4_One(),
			});
		}
	}

	glBindBuffer(GL_UNIFORM_BUFFER, r_lights.buffer);
	glBufferSubData(GL_UNIFORM_BUFFER, offsetof(r_lights_block_t, num_lights), sizeof(r_lights.block.num_lights), &r_lights.block.num_lights);
	glBufferSubData(GL_UNIFORM_BUFFER, offsetof(r_lights_block_t, lights), sizeof(r_light_uniform_t) * r_lights.block.num_lights, &r_lights.block.lights);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
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
}

/**
 * @brief
 */
void R_ShutdownLights(void) {

	glDeleteBuffers(1, &r_lights.buffer);
}
