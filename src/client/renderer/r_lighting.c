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

/*
 * @brief The illumination light source is cast far away so that it's shadow
 * projection almost resembles a directional one.
 */
#define LIGHTING_AMBIENT_ATTENUATION 640.0
#define LIGHTING_AMBIENT_DIST 580.0

/*
 * @brief Resolves ambient contribution for the specified point.
 */
static void R_UpdateIllumination_Ambient(r_lighting_t *lighting) {

	r_illumination_t *il = &lighting->illumination;

	il->ambient = LIGHTING_AMBIENT_ATTENUATION * lighting->scale - LIGHTING_AMBIENT_DIST;
}

/*
 * @brief Resolves diffuse (sunlight) contribution for the specified point.
 */
static void R_UpdateIllumination_Diffuse(r_lighting_t *lighting) {
	vec3_t pos;

	const r_sun_t *sun = &r_bsp_light_state.sun;

	if (!sun->diffuse)
		return;

	r_illumination_t *il = &lighting->illumination;

	const uint16_t skip = lighting->number;

	il-> exposure = 0.0;

	VectorMA(lighting->origin, MAX_WORLD_DIST, sun->dir, pos);

	c_trace_t tr = Cl_Trace(lighting->origin, pos, NULL, NULL, skip, CONTENTS_SOLID);
	if (tr.fraction < 1.0 && tr.surface->flags & SURF_SKY) {
		il->exposure += 0.33;
	}

	VectorMA(lighting->maxs, MAX_WORLD_DIST, sun->dir, pos);

	tr = Cl_Trace(lighting->maxs, pos, NULL, NULL, skip, CONTENTS_SOLID);
	if (tr.fraction < 1.0 && tr.surface->flags & SURF_SKY) {
		il->exposure += 0.33;
	}

	VectorMA(lighting->mins, MAX_WORLD_DIST, sun->dir, pos);

	tr = Cl_Trace(lighting->mins, pos, NULL, NULL, skip, CONTENTS_SOLID);
	if (tr.fraction < 1.0 && tr.surface->flags & SURF_SKY) {
		il->exposure += 0.33;
	}

	il->diffuse = il->exposure * lighting->scale * sun->diffuse;
}

/*
 * @brief Trace from the specified lighting point to the world, accounting for
 * attenuation within the radius of the object. Any impacted planes are saved
 * to the shadow.
 */
static void R_UpdateShadow(r_lighting_t *lighting, const vec3_t dir, vec_t light, r_shadow_t *s) {
	vec3_t pos;

	const vec_t dist = light - lighting->radius;

	if (dist <= 0.0)
		return;

	// project outward along the lighting direction
	VectorMA(lighting->origin, -dist, dir, pos);

	// skipping the entity itself, impacting solids
	const c_trace_t tr = Cl_Trace(lighting->origin, pos, NULL, NULL, lighting->number, MASK_SOLID);

	// resolve the shadow plane
	if (!tr.start_solid && tr.fraction < 1.0) {
		s->intensity = 1.0 - tr.fraction;
		s->plane = tr.plane;
	} else {
		s->intensity = 0.0;
		s->plane.type = PLANE_NONE;
	}
}

/*
 * @brief Resolves ambient and sunlight contributions, plus the primary shadow,
 * for the specified point.
 */
static void R_UpdateIllumination(r_lighting_t *lighting) {

	r_illumination_t *il = &lighting->illumination;

	R_UpdateIllumination_Ambient(lighting);

	R_UpdateIllumination_Diffuse(lighting);

	const r_sun_t *sun = &r_bsp_light_state.sun;

	VectorMix(vec3_up, sun->dir, il->diffuse / (il->ambient + il->diffuse), il->dir);

	VectorNormalize(il->dir);

	VectorScale(r_bsp_light_state.ambient, lighting->scale, il->color);

	VectorMA(il->color, il->exposure, r_bsp_light_state.sun.color, il->color);

	VectorMA(lighting->origin, LIGHTING_AMBIENT_DIST, il->dir, il->shadow.pos);

	R_UpdateShadow(lighting, il->dir, il->ambient + il->diffuse, &il->shadow);
}

#define LIGHTING_MAX_BSP_LIGHT_REFS 64

/*
 * @brief Qsort comparator for r_bsp_light_ref_t.
 */
static int32_t R_BspLightReference_Compare(const void *l1, const void *l2) {
	const r_bsp_light_ref_t *_l1 = (r_bsp_light_ref_t *) l1;
	const r_bsp_light_ref_t *_l2 = (r_bsp_light_ref_t *) l2;

	return (int32_t) (_l2->diffuse - _l1->diffuse);
}

/*
 * @brief Resolves the strongest static light sources, populating the light references
 * for the specified structure and returning the number of light sources found.
 * This facilitates directional shading in the fragment program.
 */
static void R_UpdateBspLightReferences(r_lighting_t *lighting) {
	r_bsp_light_ref_t light_refs[LIGHTING_MAX_BSP_LIGHT_REFS];
	uint16_t i, n;

	r_bsp_light_ref_t *r = lighting->bsp_light_refs;

	memset(r, 0, sizeof(r));

	r_bsp_light_t *l = r_model_state.world->bsp->bsp_lights;

	// resolve all of the light sources that could contribute to this lighting
	for (i = n = 0; i < r_model_state.world->bsp->num_bsp_lights; i++, l++) {
		vec3_t dir;

		// is the light source within the PVS this frame
		if (lighting->state == LIGHTING_DIRTY) {
			if (l->leaf->vis_frame != r_locals.vis_frame)
				continue;
		}

		// is it within range of the entity
		VectorSubtract(l->origin, lighting->origin, dir);
		const vec_t dist = VectorNormalize(dir);

		const vec_t diffuse = l->radius * lighting->scale + lighting->radius - dist;

		if (diffuse <= 0.0)
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

		// everything checks out, so keep it, if it is strong, we may use it
		light_refs[n].bsp_light = l;
		VectorCopy(dir, light_refs[n].dir);
		light_refs[n].diffuse = diffuse;

		if (++n == LIGHTING_MAX_BSP_LIGHT_REFS) {
			Com_Debug("LIGHTING_MAX_BSP_LIGHT_REFS\n");
			break;
		}
	}

	if (!n) // nothing nearby, we're done
		return;

	// sort them by contribution
	qsort((void *) light_refs, n, sizeof(r_bsp_light_ref_t), R_BspLightReference_Compare);

	// take the eight strongest sources
	n = MIN(n, MAX_ACTIVE_LIGHTS);

	// and copy them in
	memcpy(r, light_refs, n * sizeof(r_bsp_light_ref_t));

	// lastly, update their shadows
	for (i = 0; i < n; i++, r++) {
		VectorCopy(r->bsp_light->origin, r->shadow.pos);
		R_UpdateShadow(lighting, r->dir, r->diffuse, &r->shadow);
	}
}

/*
 * @brief Resolves light source and shadow information for the specified point.
 */
void R_UpdateLighting(r_lighting_t *lighting) {

	// resolve illumination from ambient and sunlight
	R_UpdateIllumination(lighting);

	// resolve the static light sources
	R_UpdateBspLightReferences(lighting);

	lighting->state = LIGHTING_READY;
}

/*
 * @brief Populates the remaining hardware light sources with static BSP lighting
 * information, if it is available. No state changes are persisted through
 * r_view or r_locals.
 */
void R_ApplyLighting(const r_lighting_t *lighting) {
	uint16_t count;

	count = r_locals.active_light_count;

	if (count < MAX_ACTIVE_LIGHTS) {
		vec4_t position = { 0.0, 0.0, 0.0, 1.0 };
		vec4_t diffuse = { 0.0, 0.0, 0.0, 1.0 };

		VectorMA(lighting->origin, LIGHTING_AMBIENT_DIST, lighting->illumination.dir, position);
		glLightfv(GL_LIGHT0 + count, GL_POSITION, position);

		VectorCopy(lighting->illumination.color, diffuse);
		glLightfv(GL_LIGHT0 + count, GL_DIFFUSE, diffuse);

		glLightf(GL_LIGHT0 + count, GL_CONSTANT_ATTENUATION, LIGHTING_AMBIENT_ATTENUATION);
		count++;

		uint16_t i = 0;
		while (count < MAX_ACTIVE_LIGHTS) {
			const r_bsp_light_ref_t *r = &lighting->bsp_light_refs[i];

			if (!r->diffuse)
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
