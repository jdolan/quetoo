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

	memset(r_uniforms.block.lights, 0, sizeof(r_uniforms.block.lights));
	r_light_t *out = r_uniforms.block.lights;

	const r_light_t *in = r_view.lights;
	for (int32_t i = 0; i < r_view.num_lights; i++, in++, out++) {

		*out = *in;

		Matrix4x4_Transform(&r_uniforms.block.view, in->origin.xyz, out->origin.xyz);
	}

	glBindBuffer(GL_UNIFORM_BUFFER, r_uniforms.buffer);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(r_uniforms.block), &r_uniforms.block, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}
