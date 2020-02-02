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
void R_AddLight(const r_light_t *l) {

	if (!r_lights->value) {
		return;
	}

	if (r_view.num_lights == MAX_LIGHTS) {
		Com_Debug(DEBUG_RENDERER, "MAX_LIGHTS reached\n");
		return;
	}

	r_view.lights[r_view.num_lights] = *l;
	r_view.lights[r_view.num_lights].origin[3] *= r_lights->value;
	
	r_view.num_lights++;
}

/**
 * @brief
 */
void R_AddSustainedLight(const r_sustained_light_t *s) {
//	int32_t i;

//	for (i = 0; i < MAX_LIGHTS; i++)
//		if (!r_view.sustained_lights[i].sustain) {
//			break;
//		}
//
//	if (i == MAX_LIGHTS) {
//		Com_Debug(DEBUG_RENDERER, "MAX_LIGHTS reached\n");
//		return;
//	}
//
//	r_view.sustained_lights[i] = *s;
//
//	r_view.sustained_lights[i].time = r_view.ticks;
//	r_view.sustained_lights[i].sustain = r_view.ticks + s->sustain;
}

/**
 * @brief
 */
void R_AddSustainedLights(void) {
//	r_sustained_light_t *s;
//	int32_t i;
//
//	// sustains must be recalculated every frame
//	for (i = 0, s = r_view.sustained_lights; i < MAX_LIGHTS; i++, s++) {
//
//		if (s->sustain <= r_view.ticks) { // clear it
//			s->sustain = 0.0;
//			continue;
//		}
//
//		r_light_t l = s->light;
//
//		const vec_t intensity = (s->sustain - r_view.ticks) / (vec_t) (s->sustain - s->time);
//
//		l.radius *= intensity;
//		VectorScale(l.color, intensity, l.color);
//
//		R_AddLight(&l);
//	}
}

/**
 * @brief
 */
r_light_t *R_TransformLights(const matrix4x4_t *transform) {
	static r_light_t lights[MAX_LIGHTS];

	memset(lights, 0, sizeof(lights));

	const r_light_t *in = r_view.lights;
	r_light_t *out = lights;

	for (int32_t i = 0; i < r_view.num_lights; i++, in++, out++) {

		*out = *in;

		Matrix4x4_Transform(transform, in->origin, out->origin);
	}

	return lights;
}
