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
		Com_Debug(DEBUG_RENDERER, "MAX_LIGHTS\n");
		return;
	}

	if (R_CullSphere(in->origin, in->radius)) {
		return;
	}

	if (R_OccludeSphere(in->origin, in->radius)) {
		return;
	}

	r_light_t *out = &r_view.lights[r_view.num_lights++];
	*out = *in;
}

/**
 * @brief Transforms all active light sources to view space for rendering.
 */
void R_UpdateLights(void) {

	memset(r_lights.block.lights, 0, sizeof(r_lights.block.lights));
	r_light_t *out = r_lights.block.lights;

	const r_light_t *in = r_view.lights;
	for (int32_t i = 0; i < r_view.num_lights; i++, in++, out++) {

		*out = *in;

		Matrix4x4_Transform(&r_uniforms.block.view, in->origin.xyz, out->origin.xyz);
	}

	r_lights.block.num_lights = r_view.num_lights;

	glBindBuffer(GL_UNIFORM_BUFFER, r_lights.buffer);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(r_lights.block), &r_lights.block, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

/**
 * @brief
 */
void R_InitLights(void) {

	glGenBuffers(1, &r_lights.buffer);

	R_UpdateLights();
}

/**
 * @brief
 */
void R_ShutdownLights(void) {

	glDeleteBuffers(1, &r_lights.buffer);
}
