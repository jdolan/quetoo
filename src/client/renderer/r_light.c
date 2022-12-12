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

	view->lights[view->num_lights] = *l;
	view->lights[view->num_lights].type = l->type ?: LIGHT_DYNAMIC;
	view->num_lights++;
}

/**
 * @brief
 */
static void R_AddBspLight_Ambient(r_view_t *view, const r_bsp_light_t *light) {

}

/**
 * @brief
 */
static void R_AddBspLight_Sun(r_view_t *view, const r_bsp_light_t *light) {

}

/**
 * @brief
 */
static void R_AddBspLight_Point(r_view_t *view, const r_bsp_light_t *light) {

}

/**
 * @brief
 */
static void R_AddBspLight_Spot(r_view_t *view, const r_bsp_light_t *light) {

}

/**
 * @brief
 */
static void R_AddBspLight_Patch(r_view_t *view, const r_bsp_light_t *light) {

	if (Box3_Distance(light->bounds) < 64.f) {
		return;
	}

	//R_Draw3DBox(light->bounds, Color3fv(Vec4_XYZ(light->color)), false);

	R_AddLight(view, &(r_light_t) {
		.type = light->type,
		.origin = Vec4_XYZ(light->origin),
		.radius = light->origin.w,
		.normal = Vec4_XYZ(light->plane),
		.dist = light->plane.w,
		.color = Vec4_XYZ(light->color),
		.intensity = light->color.w,
	});

}

/**
 * @brief
 */
static void R_AddBspLights(r_view_t *view) {

	if (!r_world_model) {
		return;
	}

	if (view->type != VIEW_MAIN) {
		return;
	}

	const r_bsp_light_t *light = r_world_model->bsp->lights;
	for (int32_t i = 0; i < r_world_model->bsp->num_lights; i++, light++) {

		if (light->atten != LIGHT_ATTEN_NONE) {

			if (Vec3_Distance(view->origin, Vec4_XYZ(light->origin)) > 2048.f) {
				continue;
			}

			_Bool has_entities = false;

			const r_entity_t *e = view->entities;
			for (int32_t j = 0; j < view->num_entities; j++, e++) {

				if (!e->model) {
					continue;
				}

				if (Box3_Intersects(light->bounds, e->abs_model_bounds)) {
					has_entities = true;
					break;
				}
			}

			if (!has_entities) {
				continue;
			}
		}

		switch (light->type) {
			case LIGHT_AMBIENT:
				R_AddBspLight_Ambient(view, light);
				break;
			case LIGHT_SUN:
				R_AddBspLight_Sun(view, light);
				break;
			case LIGHT_POINT:
				R_AddBspLight_Point(view, light);
				break;
			case LIGHT_SPOT:
				R_AddBspLight_Spot(view, light);
				break;
			case LIGHT_PATCH:
				R_AddBspLight_Patch(view, light);
				break;
			default:
				break;
		}
	}
}

/**
 * @brief Transforms all active light sources to view space for rendering.
 */
void R_UpdateLights(r_view_t *view) {

	if (view) {

		//R_AddBspLights(view);

		const r_light_t *in = view->lights;
		r_light_uniform_t *out = r_lights.block.lights;

		for (int32_t i = 0; i < view->num_lights; i++, in++, out++) {
			out->model = Vec3_ToVec4(in->origin, in->radius);
			out->position = Vec3_ToVec4(Mat4_Transform(r_uniforms.block.view, in->origin), in->type);
			out->normal = Mat4_TransformPlane(r_uniforms.block.view, in->normal, in->dist);
			out->color = Vec3_ToVec4(in->color, in->intensity);
		}

		r_lights.block.num_lights = view->num_lights;
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
