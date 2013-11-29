/*
 * Copyright(c) 1997-2001 Id Software, Inc.
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
#include "client.h"

#define LIGHTING_MAX_BSP_LIGHT_REFS 64

/*
 * @brief A comparator for sorting r_bsp_light_ref_t elements via quick sort.
 */
static int32_t R_UpdateBspLightReferences_Compare(const void *l1, const void *l2) {
	const r_bsp_light_ref_t *_l1 = (r_bsp_light_ref_t *) l1;
	const r_bsp_light_ref_t *_l2 = (r_bsp_light_ref_t *) l2;

	return (int32_t) (_l2->light - _l1->light);
}

/*
 * @brief Resolves the strongest static light sources, populating the light references
 * for the specified structure and returning the number of light sources found.
 * This facilitates directional shading in the fragment program.
 */
static uint16_t R_UpdateBspLightReferences(r_lighting_t *lighting) {
	r_bsp_light_ref_t light_refs[LIGHTING_MAX_BSP_LIGHT_REFS];
	uint16_t i, j;

	memset(lighting->bsp_light_refs, 0, sizeof(lighting->bsp_light_refs));

	r_bsp_light_t *l = r_model_state.world->bsp->bsp_lights;

	// resolve all of the light sources that could contribute to this lighting
	for (i = j = 0; i < r_model_state.world->bsp->num_bsp_lights; i++, l++) {
		vec3_t dir;

		// is the light source within the PVS this frame
		if (lighting->state == LIGHTING_DIRTY && l->leaf->vis_frame != r_locals.vis_frame)
			continue;

		// is it within range of the entity
		VectorSubtract(l->origin, lighting->origin, dir);
		const vec_t light = l->radius + lighting->radius - VectorNormalize(dir);

		if (light <= 0.0)
			continue;

		// is it visible to the entity; trace to origin and corners of bounding box

		const uint16_t skip = lighting->number;

		c_trace_t tr = Cl_Trace(l->origin, lighting->origin, NULL, NULL, skip, CONTENTS_SOLID);

		if (tr.fraction < 1.0) {
			tr = Cl_Trace(l->origin, lighting->mins, NULL, NULL, skip, CONTENTS_SOLID);

			if (tr.fraction < 1.0) {
				tr = Cl_Trace(l->origin, lighting->maxs, NULL, NULL, skip, CONTENTS_SOLID);

				if (tr.fraction < 1.0) {
					continue;
				}
			}
		}

		// everything checks out, so keep it
		r_bsp_light_ref_t *r = &light_refs[j++];

		r->bsp_light = l;
		VectorCopy(dir, r->dir);
		r->light = light;

		if (j == LIGHTING_MAX_BSP_LIGHT_REFS) {
			Com_Debug("LIGHTING_MAX_BSP_LIGHT_REFS\n");
			break;
		}
	}

	if (j) {
		// sort them by contribution
		qsort((void *) light_refs, j, sizeof(r_bsp_light_ref_t), R_UpdateBspLightReferences_Compare);

		// take the eight strongest sources
		j = MIN(j, MAX_ACTIVE_LIGHTS);

		// accumulate the total light value
		for (i = 0; i < j; i++) {
			lighting->light += light_refs[i].light;
		}

		// and copy them in
		memcpy(lighting->bsp_light_refs, light_refs, j * sizeof(r_bsp_light_ref_t));
	}

	return j;
}

/*
 * @brief Resolves the shadow plane and intensity for the specified point.
 */
static void R_UpdateShadow(r_lighting_t *lighting) {
	vec3_t dir, impact;

	// resolve the lighting direction
	VectorSubtract(lighting->pos, lighting->origin, dir);
	VectorNormalize(dir);

	// cast out on the lighting direction
	VectorMA(lighting->origin, -lighting->light, dir, impact);

	// skipping the entity itself, impacting solids
	c_trace_t trace = Cl_Trace(lighting->origin, impact, NULL, NULL, lighting->number, MASK_SOLID);

	// resolve the shadow plane
	if (!trace.start_solid && trace.fraction < 1.0) {
		lighting->shadow = 1.0 - trace.fraction;
		lighting->plane = trace.plane;
	} else {
		lighting->shadow = 0.0;
		lighting->plane.type = PLANE_NONE;
	}
}

#define LIGHTING_AMBIENT_ATTENUATION 96.0
#define LIGHTING_AMBIENT_DIST 64.0

/*
 * @brief Resolves light source and shadow information for the specified point.
 */
void R_UpdateLighting(r_lighting_t *lighting) {
	uint16_t i, j;

	// start by accounting for an ambient light source above the origin
	VectorCopy(r_bsp_light_state.ambient_light, lighting->color);

	lighting->light = LIGHTING_AMBIENT_ATTENUATION - LIGHTING_AMBIENT_DIST;
	lighting->shadow = Clamp(lighting->light - lighting->radius, 0.0, HUGE_VALF);

	VectorMA(lighting->origin, LIGHTING_AMBIENT_DIST, vec3_up, lighting->pos);

	// resolve the static light sources
	i = R_UpdateBspLightReferences(lighting);

	// resolve the lighting position and shading color
	for (j = 0; j < i; j++) {
		r_bsp_light_ref_t *r = &lighting->bsp_light_refs[j];

		const vec_t contrib = r->light / lighting->light;
		VectorMix(lighting->pos, r->bsp_light->origin, contrib, lighting->pos);

		const vec_t intensity = r->light / r->bsp_light->radius;
		VectorMA(lighting->color, intensity, r->bsp_light->color, lighting->color);
	}

	// resolve the shadow plane and intensity
	R_UpdateShadow(lighting);

	lighting->state = LIGHTING_READY; // mark it clean
}

/*
 * @brief Populates the remaining hardware light sources with static BSP lighting
 * information, if it is available. No state changes are persisted through
 * r_view or r_locals.
 */
void R_ApplyLighting(const r_lighting_t *lighting) {
	const vec3_t up = { 0.0, 0.0, 64.0 };
	uint16_t count;

	count = r_locals.active_light_count;

	if (count < MAX_ACTIVE_LIGHTS) {
		vec4_t position;
		vec4_t diffuse;

		position[3] = diffuse[3] = 1.0;

		VectorAdd(lighting->origin, up, position);
		glLightfv(GL_LIGHT0 + count, GL_POSITION, position);

		VectorScale(r_bsp_light_state.ambient_light, r_lighting->value, diffuse);
		glLightfv(GL_LIGHT0 + count, GL_DIFFUSE, diffuse);

		glLightf(GL_LIGHT0 + count, GL_CONSTANT_ATTENUATION, LIGHTING_AMBIENT_ATTENUATION);
		count++;

		uint16_t i = 0;
		while (count < MAX_ACTIVE_LIGHTS) {
			const r_bsp_light_ref_t *r = &lighting->bsp_light_refs[i];

			if (!r->light)
				break;

			VectorCopy(r->bsp_light->origin, position);
			glLightfv(GL_LIGHT0 + count, GL_POSITION, position);

			VectorScale(r->bsp_light->color, r_lighting->value, diffuse);
			glLightfv(GL_LIGHT0 + count, GL_DIFFUSE, diffuse);

			glLightf(GL_LIGHT0 + count, GL_CONSTANT_ATTENUATION, r->bsp_light->radius);

			count++;
			i++;
		}
	}

	if (count < MAX_ACTIVE_LIGHTS) // disable the next light as a stop
		glLightf(GL_LIGHT0 + count, GL_CONSTANT_ATTENUATION, 0.0);
}
