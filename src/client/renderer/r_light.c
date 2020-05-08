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
void R_AddLight(const r_light_t *in) {

	if (r_view.num_lights == MAX_LIGHTS) {
		Com_Debug(DEBUG_RENDERER, "MAX_LIGHTS reached\n");
		return;
	}

	if (R_CullSphere(in->origin, in->radius)) {
		return;
	}

	r_light_t *out = &r_view.lights[r_view.num_lights++];
	*out = *in;
}

/**
 * @brief Recursively populates light source bit masks for world nodes.
 */
static void R_MarkLight(const r_light_t *l, r_bsp_node_t *node) {

	if (node->contents != CONTENTS_NODE) {
		return;
	}

	if (node->vis_frame != r_locals.vis_frame) {
		return;
	}

	const float dist = Cm_DistanceToPlane(l->origin, node->plane);

	if (dist > l->radius) {
		R_MarkLight(l, node->children[0]);
		return;
	}

	if (dist < -l->radius) {
		R_MarkLight(l, node->children[1]);
		return;
	}

	node->lights_mask |= (1 << (l - r_view.lights));

	R_MarkLight(l, node->children[0]);
	R_MarkLight(l, node->children[1]);
}

/**
 * @brief Marks lights in world space, and transforms them to view space for rendering.
 */
void R_UpdateLights(void) {

	memset(r_lights.lights, 0, sizeof(r_lights.lights));
	r_light_t *out = r_lights.lights;

	r_light_t *in = r_view.lights;
	for (int32_t i = 0; i < r_view.num_lights; i++, in++, out++) {

		R_MarkLight(in, r_world_model->bsp->nodes);

		const vec3_t origin = in->origin;

		r_entity_t *e = r_view.entities;
		for (int32_t j = 0; j < r_view.num_entities; j++, e++) {

			if (e->model) {
				switch (e->model->type) {
					case MOD_BSP_INLINE:
						Matrix4x4_Transform(&e->inverse_matrix, origin.xyz, in->origin.xyz);
						R_MarkLight(in, e->model->bsp_inline->head_node);
						break;
					case MOD_MESH:
						if (Vec3_Distance(e->origin, in->origin) < e->model->radius + in->radius) {
							e->lights |= (1 << (in - r_view.lights));
						}
						break;
					default:
						break;
				}
			}

			in->origin = origin;
		}

		*out = *in;

		Matrix4x4_Transform(&r_locals.view, in->origin.xyz, out->origin.xyz);
	}

	glBindBuffer(GL_UNIFORM_BUFFER, r_lights.uniform_buffer);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(r_lights.lights), r_lights.lights, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

/**
 * @brief
 */
void R_InitLights(void) {
	glGenBuffers(1, &r_lights.uniform_buffer);
}

/**
 * @brief
 */
void R_ShutdownLights(void) {
	glDeleteBuffers(1, &r_lights.uniform_buffer);
}
