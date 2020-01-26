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
	r_view.lights[r_view.num_lights].radius *= r_lights->value;
	
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
 * @brief Recursively populates light source bit mask for the specified node.
 */
static void R_MarkLight(const r_light_t *l, r_bsp_node_t *node) {

	if (node->vis_frame != r_locals.vis_frame) {
		return;
	}

	if (node->contents != CONTENTS_NODE) {
		return;
	}

	const vec_t dist = Cm_DistanceToPlane(l->origin, node->plane);

	if (dist > l->radius) { // front only
		R_MarkLight(l, node->children[0]);
		return;
	}

	if (dist < -l->radius) { // back only
		R_MarkLight(l, node->children[1]);
		return;
	}

	// the light resides in this node, so turn it on
	node->lights |= 1 << (l - r_view.lights);

	// now go down both sides
	R_MarkLight(l, node->children[0]);
	R_MarkLight(l, node->children[1]);
}

/**
 * @brief Recurses the world, populating the light source bit masks of surfaces
 * that receive light.
 */
void R_MarkLights(void) {

	const r_light_t *l = r_view.lights;
	for (int32_t i = 0; i < r_view.num_lights; i++, l++) {
		R_MarkLight(l, r_model_state.world->bsp->nodes);
	}
}

