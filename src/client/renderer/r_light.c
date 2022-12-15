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
 * @brief
 */
static void R_AddBspLights(const r_view_t *view) {

	if (!r_world_model) {
		return;
	}

	if (view->type != VIEW_MAIN) {
		return;
	}

	r_light_uniform_t *out = r_lights.block.lights;

	const r_bsp_light_t *in = r_world_model->bsp->lights;
	for (int32_t i = 0; i < r_world_model->bsp->num_lights; i++, in++) {

		switch (in->type) {
			case LIGHT_SUN: {
//				const r_entity_t *e = view->entities;
//				for (int32_t j = 0; j < view->num_entities; j++, e++) {
//
//					if (e->model && !e->parent) {
//						const vec3_t origin = Vec3_Fmaf(e->origin, <#float multiply#>, <#const vec3_t add#>)
//						R_AddLightUniform(&(const r_light_uniform_t) {
//							.model = Vec3_ToVec4(e->origin, 2.f * Box3_Radius(e->abs_model_bounds)),
//							.position = Vec3_ToVec4(Mat4_Transform(r_uniforms.block.view, e->origin), in->type),
//							.normal = Mat4_TransformPlane(r_uniforms.block.view, Vec4_XYZ(in->normal), in->normal.w),
//							.color = in->color,
//						});
//					}
//				}
			}
				break;

			case LIGHT_PATCH: {

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
				break;
			default:
				break;
		}
	}
}

/**
 * @brief Transforms all active light sources to view space for rendering.
 */
void R_UpdateLights(const r_view_t *view) {

	if (view) {

		r_lights.block.num_lights = 0;

		R_AddBspLights(view);

		const r_light_t *in = view->lights;
		for (int32_t i = 0; i < view->num_lights; i++, in++) {
			
			R_AddLightUniform(&(r_light_uniform_t) {
				.model = Vec3_ToVec4(in->origin, in->radius),
				.position = Vec3_ToVec4(Mat4_Transform(r_uniforms.block.view, in->origin), LIGHT_DYNAMIC),
				.color = Vec3_ToVec4(in->color, in->intensity),
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

	glGenBuffers(1, &r_lights.buffer);
	
	glBindBuffer(GL_UNIFORM_BUFFER, r_lights.buffer);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(r_lights.block), NULL, GL_DYNAMIC_DRAW);

	glBindBufferBase(GL_UNIFORM_BUFFER, 1, r_lights.buffer);

	R_UpdateLights(NULL);
}

/**
 * @brief
 */
void R_ShutdownLights(void) {

	glDeleteBuffers(1, &r_lights.buffer);
}
