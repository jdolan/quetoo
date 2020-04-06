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
		Com_Debug(DEBUG_RENDERER, "MAX_LIGHTS reached\n");
		return;
	}

	if (R_CullSphere(in->origin, in->radius)) {
		return;
	}

	r_light_t *out = &r_view.lights[r_view.num_lights++];
	*out = *in;

	out->intensity *= r_light_intensity->value;
}

/**
 * @brief
 */
void R_UpdateLights(void) {

	memset(r_locals.view_lights, 0, sizeof(r_locals.view_lights));

	const r_light_t *in = r_view.lights;
	r_light_t *out = r_locals.view_lights;

	for (int32_t i = 0; i < r_view.num_lights; i++, in++) {

		if (R_CullSphere(in->origin, in->radius)) {
			continue;
		}

		*out = *in;

		Matrix4x4_Transform(&r_locals.view, in->origin.xyz, out->origin.xyz);

		out++;
	}
}
