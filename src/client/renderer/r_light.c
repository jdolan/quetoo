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

	R_AddLight(view, &(r_light_t) {
		.origin = light->origin,
		.radius = light->radius,
		.color = light->color,
		.intensity = -light->intensity
	});
}

/**
 * @brief
 */
static void R_AddBspLight_Spot(r_view_t *view, const r_bsp_light_t *light) {

	R_AddLight(view, &(r_light_t) {
		.origin = light->origin,
		.radius = light->radius,
		.color = light->color,
		.intensity = -light->intensity
	});
}

/**
 * @brief
 */
static void R_AddBspLight_Patch(r_view_t *view, const r_bsp_light_t *light) {

	R_AddLight(view, &(r_light_t) {
		.origin = light->origin,
		.radius = light->radius,
		.color = light->color,
		.intensity = -light->intensity
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

		R_AddBspLights(view);

		const r_light_t *in = view->lights;
		r_light_uniform_t *out = r_lights.block.lights;

		if (r_world_model) {
			r_bsp_vertex_t *vertex = r_world_model->bsp->vertexes;
			for (int32_t i = 0; i < r_world_model->bsp->num_vertexes; i++, vertex++) {
				vertex->lights = Vec4i(0, 0, 0, 0);
			}
		}

		for (int32_t i = 0; i < view->num_lights; i++, in++, out++) {

			if (r_world_model) {
				const r_bsp_model_t *bsp = r_world_model->bsp;

				const box3_t bounds = Box3_FromCenterRadius(in->origin, in->radius);

				const r_bsp_node_t *node = R_NodeForBounds(bounds);
				if (node == NULL) {
					continue;
				}

				const r_bsp_face_t *face = node->faces;
				for (int32_t j = 0; j < node->num_faces; j++, face++) {

					r_bsp_vertex_t *vertex = face->vertexes;
					for (int32_t k = 0; k < face->num_vertexes; k++, vertex++) {

						for (size_t l = 0; l < lengthof(vertex->lights.xyzw); l++) {
							if (vertex->lights.xyzw[l] == 0) {
								vertex->lights.xyzw[l] = i;
								break;
							}
						}
					}
				}

				glBindBuffer(GL_ARRAY_BUFFER, bsp->vertex_buffer);
				glBufferData(GL_ARRAY_BUFFER, bsp->num_vertexes * sizeof(r_bsp_vertex_t), bsp->vertexes, GL_STATIC_DRAW);
			}

			out->origin = in->origin;
			out->radius = in->radius;
			out->color = in->color;
			out->intensity = in->intensity;
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
