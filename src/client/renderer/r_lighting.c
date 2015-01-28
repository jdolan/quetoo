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
#include "client.h"

#define LIGHTING_MAX_ILLUMINATIONS 128

/*
 * @brief Provides a working area for gathering and sorting illuminations.
 * For a given point, all contributing illuminations are first resolved and
 * then sorted by contribution.
 */
typedef struct {
	r_illumination_t illuminations[LIGHTING_MAX_ILLUMINATIONS];
	uint16_t num_illuminations;
} r_illuminations_t;

static r_illuminations_t r_illuminations;

/*
 * @brief Adds an illumination with the given parameters.
 */
static void R_AddIllumination(const r_illumination_t *il) {

	if (r_illuminations.num_illuminations == LIGHTING_MAX_ILLUMINATIONS) {
		Com_Debug("LIGHTING_MAX_ILLUMINATIONS\n");
		return;
	}

	r_illuminations.illuminations[r_illuminations.num_illuminations++] = *il;
}

#define LIGHTING_AMBIENT_RADIUS 300.0
#define LIGHTING_AMBIENT_DIST 260.0

/*
 * @brief Adds an illumination for ambient lighting. This weak illumination
 * is positioned far above the lighting point so that it's shadow projection
 * appears directional.
 */
static void R_AmbientIllumination(const r_lighting_t *l) {
	r_illumination_t il;

	vec_t max = 0.15;

	for (uint16_t i = 0; i < 3; i++) {
		if (r_bsp_light_state.ambient[i] > max)
			max = r_bsp_light_state.ambient[i];
	}

	VectorMA(l->origin, LIGHTING_AMBIENT_DIST, vec3_up, il.light.origin);
	VectorScale(r_bsp_light_state.ambient, 1.0 / max, il.light.color);
	il.light.radius = LIGHTING_AMBIENT_RADIUS * r_lighting->value;
	il.diffuse = il.light.radius - LIGHTING_AMBIENT_DIST;

	R_AddIllumination(&il);
}

#define LIGHTING_SUN_RADIUS 320.0
#define LIGHTING_SUN_DIST 260.0

/*
 * @brief Adds an illumination for sun lighting. Traces to the lighting bounds
 * determine sun exposure, which scales the applied sun color.
 */
static void R_SunIllumination(const r_lighting_t *l) {
	r_illumination_t il;

	if (!r_bsp_light_state.sun.diffuse)
		return;

	const vec_t *p[] = { l->origin, l->mins, l->maxs };

	vec_t exposure = 0.0;

	for (uint16_t i = 0; i < lengthof(p); i++) {
		vec3_t pos;

		VectorMA(p[i], MAX_WORLD_DIST, r_bsp_light_state.sun.dir, pos);

		const cm_trace_t tr = Cl_Trace(p[i], pos, NULL, NULL, l->number, CONTENTS_SOLID);

		if (tr.surface && (tr.surface->flags & SURF_SKY)) {
			exposure += (1.0 / lengthof(p));
		}
	}

	if (exposure == 0.0)
		return;

	VectorMA(l->origin, LIGHTING_SUN_DIST, r_bsp_light_state.sun.dir, il.light.origin);
	VectorScale(r_bsp_light_state.sun.color, exposure, il.light.color);
	il.light.radius = LIGHTING_SUN_RADIUS * r_lighting->value;
	il.diffuse = il.light.radius - LIGHTING_SUN_DIST;

	R_AddIllumination(&il);
}

/*
 * @brief Adds an illumination for the positional light source, if the given
 * point is within range and not occluded.
 */
static _Bool R_PositionalIllumination(const r_lighting_t *l, const r_light_t *light) {
	r_illumination_t il;

	const vec_t *p[] = { l->origin, l->mins, l->maxs };

	vec_t diffuse = 0.0;

	// trace to the origin as well as the bounds
	for (uint16_t i = 0; i < lengthof(p); i++) {
		vec3_t dir;

		// is it within range of the point in question
		VectorSubtract(light->origin, p[i], dir);

		const vec_t diff = light->radius - VectorLength(dir);

		if (diff <= 0.0)
			continue;

		// is it visible to the entity
		if (Cl_Trace(light->origin, p[i], NULL, NULL, l->number, CONTENTS_SOLID).fraction < 1.0)
			continue;

		diffuse += diff;
	}

	if (diffuse == 0.0)
		return false;

	il.light = *light;
	il.diffuse = diffuse / lengthof(p);

	R_AddIllumination(&il);
	return true;
}

/*
 * @brief Adds illuminations for static (BSP) light sources.
 */
static void R_StaticIlluminations(r_lighting_t *l) {

	const r_bsp_light_t *bl = r_model_state.world->bsp->bsp_lights;

	for (uint16_t i = 0; i < r_model_state.world->bsp->num_bsp_lights; i++, bl++) {

		if (l->state == LIGHTING_DIRTY) {
			if (bl->leaf->vis_frame != r_locals.vis_frame)
				continue;
		}

		R_PositionalIllumination(l, (const r_light_t *) &(bl->light));
	}
}

/*
 * @brief Resolves dynamic light source illuminations for the specified point.
 */
static void R_DynamicIlluminations(r_lighting_t *l) {

	l->light_mask = 0;

	const r_light_t *dl = r_view.lights;

	for (uint16_t i = 0; i < r_view.num_lights; i++, dl++) {
		if (R_PositionalIllumination(l, dl)) {
			l->light_mask |= (uint64_t) (1 << i);
		}
	}
}

/*
 * @brief Qsort comparator for r_illumination_t. Orders by light, descending.
 */
static int32_t R_CompareIllumination(const void *a, const void *b) {

	const r_illumination_t *il0 = (const r_illumination_t *) a;
	const r_illumination_t *il1 = (const r_illumination_t *) b;

	return (int32_t) (il1->diffuse - il0->diffuse);
}

/*
 * @brief Updates illuminations for the specified point. If not dirty, and no
 * dynamic illuminations are active, the previous illuminations are retained.
 */
static void R_UpdateIlluminations(r_lighting_t *l) {

	r_illuminations.num_illuminations = 0;

	const uint64_t prev_light_mask = l->light_mask;

	R_DynamicIlluminations(l);

	// if not dirty, and no dynamic lighting, we're done
	if (l->state == LIGHTING_READY) {
		if (prev_light_mask == 0 && l->light_mask == 0)
			return;
	}

	l->state = MIN(l->state, LIGHTING_DIRTY);

	memset(l->illuminations, 0, sizeof(l->illuminations));

	// otherwise, resolve ambient, sun and static illuminations as well
	R_AmbientIllumination(l);

	R_SunIllumination(l);

	R_StaticIlluminations(l);

	r_illumination_t *il = r_illuminations.illuminations;

	// sort them by contribution
	qsort(il, r_illuminations.num_illuminations, sizeof(r_illumination_t), R_CompareIllumination);

	// take the strongest illuminations
	uint16_t n = MIN(r_illuminations.num_illuminations, MAX_ILLUMINATIONS);

	// and copy them in
	memcpy(l->illuminations, il, n * sizeof(r_illumination_t));
}

/*
 * @brief Qsort comparator for r_shadow_t. Orders by intensity, descending.
 */
static int32_t R_CompareShadow(const void *a, const void *b) {

	const r_shadow_t *s0 = (const r_shadow_t *) a;
	const r_shadow_t *s1 = (const r_shadow_t *) b;

	return (int32_t) (1000.0 * (s1->intensity - s0->intensity));
}

/*
 * @brief For each active illumination, trace from the specified lighting point
 * to the world, from the origin as well as the corners of the bounding box,
 * accounting for light attenuation within the radius of the object. Cast
 * shadows on impacted planes.
 */
static void R_UpdateShadows(r_lighting_t *l) {

	if (!r_shadows->value)
		return;

	// if not dirty and nothing has changed, retain our cached shadows
	if (l->state == LIGHTING_READY)
		return;

	// otherwise, refresh all shadow information based on the new illuminations
	memset(l->shadows, 0, sizeof(l->shadows));

	r_shadow_t *s = l->shadows;

	const r_illumination_t *il = l->illuminations;

	for (uint16_t i = 0; i < lengthof(l->illuminations); i++, il++) {

		if (il->diffuse == 0.0)
			break;

		const vec_t *p[] = { l->origin, l->mins, l->maxs };

		for (uint16_t j = 0; j < lengthof(p); j++) {
			vec3_t dir, pos;

			// check if the light exits the entity
			VectorSubtract(il->light.origin, p[j], dir);
			const vec_t dist = il->light.radius - VectorNormalize(dir);

			if (dist <= 0.0)
				continue;

			// project the farthest possible shadow impact position
			VectorMA(p[j], -dist, dir, pos);

			// trace, skipping the entity itself, impacting solids
			const cm_trace_t tr = Cl_Trace(p[j], pos, NULL, NULL, l->number, MASK_SOLID);

			// check if the trace impacted a valid plane
			if (tr.start_solid || tr.fraction == 1.0)
				continue;

			const vec_t dot = DotProduct(r_view.origin, tr.plane.normal) - tr.plane.dist;

			// check if the plane faces the view origin
			if (dot <= 0.0)
				continue;

			// resolve the intensity of the shadow based on how much light we occlude
			const vec_t intensity = (1.0 - tr.fraction) * DotProduct(dir, tr.plane.normal);

			// prepare to cast a shadow, search for the plane in previous shadows
			s = l->shadows;
			while (s->illumination) {

				// check if the shadow references the same illumination and plane
				if (s->illumination == il && s->plane.num == tr.plane.num) {
					s->intensity = MAX(s->intensity, intensity);
					break;
				}

				s++;
			}

			// plane already counted for this illumination
			if (s->illumination)
				continue;

			// finally, cast the shadow
			s->illumination = il;
			s->plane = tr.plane;
			s->intensity = intensity;

			s++;
		}
	}

	qsort(l->shadows, s - l->shadows, sizeof(r_shadow_t), R_CompareShadow);
}

/*
 * @brief Resolves illumination and shadow information for the specified point.
 */
void R_UpdateLighting(r_lighting_t *l) {

	R_UpdateIlluminations(l);

	R_UpdateShadows(l);

	l->state = LIGHTING_READY;
}

/*
 * @brief Populates hardware light sources with illumination information.
 */
void R_ApplyLighting(const r_lighting_t *l) {

	vec4_t position = { 0.0, 0.0, 0.0, 1.0 };
	vec4_t diffuse = { 0.0, 0.0, 0.0, 1.0 };

	uint16_t i = 0;
	while (i < MAX_ACTIVE_LIGHTS) {

		const r_illumination_t *il = &l->illuminations[i];

		if (il->diffuse == 0.0)
			break;

		VectorCopy(il->light.origin, position);
		glLightfv(GL_LIGHT0 + i, GL_POSITION, position);

		VectorCopy(il->light.color, diffuse);
		glLightfv(GL_LIGHT0 + i, GL_DIFFUSE, diffuse);

		glLightf(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, il->light.radius);
		i++;
	}

	if (i < MAX_ACTIVE_LIGHTS) // disable the next light as a stop
		glLightf(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, 0.0);
}
