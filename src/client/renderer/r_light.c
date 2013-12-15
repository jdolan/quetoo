/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

/*
 * @brief
 */
void R_AddLight(const r_light_t *l) {

	if (!r_lighting->value)
		return;

	if (r_view.num_lights == MAX_LIGHTS) {
		Com_Debug("MAX_LIGHTS reached\n");
		return;
	}

	r_view.lights[r_view.num_lights++] = *l;
}

/*
 * @brief
 */
void R_AddSustainedLight(const r_sustained_light_t *s) {
	int32_t i;

	if (!r_lighting->value)
		return;

	for (i = 0; i < MAX_LIGHTS; i++)
		if (!r_view.sustained_lights[i].sustain)
			break;

	if (i == MAX_LIGHTS) {
		Com_Debug("MAX_LIGHTS reached\n");
		return;
	}

	r_view.sustained_lights[i] = *s;

	r_view.sustained_lights[i].time = r_view.time;
	r_view.sustained_lights[i].sustain = r_view.time + s->sustain;
}

/*
 * @brief
 */
static void R_AddSustainedLights(void) {
	r_sustained_light_t *s;
	int32_t i;

	// sustains must be recalculated every frame
	for (i = 0, s = r_view.sustained_lights; i < MAX_LIGHTS; i++, s++) {

		if (s->sustain <= r_view.time) { // clear it
			s->sustain = 0.0;
			continue;
		}

		r_light_t l = s->light;

		const vec_t intensity = (s->sustain - r_view.time) / (vec_t) (s->sustain - s->time);
		VectorScale(s->light.color, intensity, l.color);

		R_AddLight(&l);
	}
}

/*
 * @brief Resets hardware light source state. Note that this is accomplished purely
 * client-side. Our internal accounting lets us avoid GL state changes.
 */
void R_ResetLights(void) {

	r_locals.active_light_mask = 0xffffffffffffffff;
	r_locals.active_light_count = 0;
}

/*
 * @brief Recursively populates light source bit masks for world surfaces.
 */
void R_MarkLight(const r_light_t *l, const r_bsp_node_t *node) {
	uint16_t i;

	if (node->contents != CONTENTS_NODE) // leaf
		return;

	if (node->vis_frame != r_locals.vis_frame) { // not visible
		if (!node->model) // and not a bsp submodel
			return;
	}

	const vec_t dist = DotProduct(l->origin, node->plane->normal) - node->plane->dist;

	if (dist > l->radius) { // front only
		R_MarkLight(l, node->children[0]);
		return;
	}

	if (dist < -l->radius) { // back only
		R_MarkLight(l, node->children[1]);
		return;
	}

	const uint64_t bit = (uint64_t) (1 << (l - r_view.lights));

	// mark all surfaces in this node
	r_bsp_surface_t *surf = r_model_state.world->bsp->surfaces + node->first_surface;

	for (i = 0; i < node->num_surfaces; i++, surf++) {

		if (surf->light_frame != r_locals.light_frame) { // reset it
			surf->light_frame = r_locals.light_frame;
			surf->lights = 0;
		}

		surf->lights |= bit; // add this light
	}

	// now go down both sides
	R_MarkLight(l, node->children[0]);
	R_MarkLight(l, node->children[1]);
}

/*
 * @brief Recurses the world, populating the light source bit masks of surfaces
 * that receive light.
 */
void R_MarkLights(void) {
	const r_bsp_model_t *bsp = r_model_state.world->bsp;
	const r_light_t *l = r_view.lights;

	R_AddSustainedLights();

	r_locals.light_frame++;

	if (r_locals.light_frame == INT16_MAX) // avoid overflows
		r_locals.light_frame = 0;

	// mark each light against the world

	for (uint16_t i = 0; i < r_view.num_lights; i++, l++) {
		R_MarkLight(l, bsp->nodes);
	}
}

/*
 * @brief Enables the light sources indicated by the specified bit mask. Care
 * is taken to avoid GL state changes whenever possible.
 */
void R_EnableLights(uint64_t mask) {
	uint16_t count;

	if (mask == r_locals.active_light_mask) // no change
		return;

	r_locals.active_light_mask = mask;
	count = 0;

	if (mask) { // enable up to MAX_ACTIVE_LIGHT sources
		const r_light_t *l;
		vec4_t position;
		vec4_t diffuse;
		uint16_t i;

		position[3] = diffuse[3] = 1.0;

		for (i = 0, l = r_view.lights; i < r_view.num_lights; i++, l++) {

			if (count == MAX_ACTIVE_LIGHTS)
				break;

			const uint64_t bit = ((uint64_t) (1 << i));
			if (mask & bit) {

				VectorCopy(l->origin, position);
				glLightfv(GL_LIGHT0 + count, GL_POSITION, position);

				VectorScale(l->color, r_lighting->value, diffuse);
				glLightfv(GL_LIGHT0 + count, GL_DIFFUSE, diffuse);

				glLightf(GL_LIGHT0 + count, GL_CONSTANT_ATTENUATION, l->radius);
				count++;
			}
		}
	}

	if (count < MAX_ACTIVE_LIGHTS) // disable the next light as a stop
		glLightf(GL_LIGHT0 + count, GL_CONSTANT_ATTENUATION, 0.0);

	r_locals.active_light_count = count;
}

/*
 * @brief Enables light sources within range of the specified point. This is
 * used by mesh entities, as they are not addressed with the recursive BSP-related
 * functions above.
 */
void R_EnableLightsForEntity(const r_entity_t *e) {
	const r_light_t *l;
	uint16_t i;

	uint64_t mask = 0;

	for (i = 0, l = r_view.lights; i < r_view.num_lights; i++, l++) {
		vec3_t delta;

		VectorSubtract(l->origin, e->origin, delta);

		if (VectorLength(delta) < l->radius)
			mask |= ((uint64_t) (1 << i));

	}

	R_EnableLights(mask);
}
