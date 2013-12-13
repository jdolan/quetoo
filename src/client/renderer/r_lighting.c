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

/*
 * @brief The ambient and sun source is cast far away so that it's shadow
 * projection resembles a directional one, lacking perspective shear.
 */
#define LIGHTING_AMBIENT_ATTENUATION 320.0
#define LIGHTING_AMBIENT_DIST 260.0

/*
 * @brief Populates the first illumination structure with ambient and sunlight.
 */
static void R_UpdateWorldIllumination(r_lighting_t *l) {
	vec3_t dir;

	r_illumination_t *il = l->illuminations;

	// resolve global ambient illumination
	il->radius = LIGHTING_AMBIENT_ATTENUATION * l->scale;
	il->ambient = il->radius - LIGHTING_AMBIENT_DIST;

	VectorScale(r_bsp_light_state.ambient, l->scale, il->color);

	VectorCopy(vec3_up, dir);

	// merge sunlight into the illumination
	const r_sun_t *sun = &r_bsp_light_state.sun;
	if (sun->diffuse) {
		vec3_t pos;

		vec_t exposure = 0.0;

		VectorMA(l->origin, MAX_WORLD_DIST, sun->dir, pos);

		cm_trace_t tr = Cl_Trace(l->origin, pos, NULL, NULL, l->number, CONTENTS_SOLID);
		if (tr.fraction < 1.0 && tr.surface->flags & SURF_SKY) {
			exposure += 0.33;
		}

		VectorMA(l->maxs, MAX_WORLD_DIST, sun->dir, pos);

		tr = Cl_Trace(l->maxs, pos, NULL, NULL, l->number, CONTENTS_SOLID);
		if (tr.fraction < 1.0 && tr.surface->flags & SURF_SKY) {
			exposure += 0.33;
		}

		VectorMA(l->mins, MAX_WORLD_DIST, sun->dir, pos);

		tr = Cl_Trace(l->mins, pos, NULL, NULL, l->number, CONTENTS_SOLID);
		if (tr.fraction < 1.0 && tr.surface->flags & SURF_SKY) {
			exposure += 0.33;
		}

		if (exposure > 0.0) {
			il->diffuse = sun->diffuse * exposure * l->scale;
			il->radius += il->diffuse;

			VectorMix(dir, sun->dir, il->diffuse / (il->ambient + il->diffuse), dir);
			VectorNormalize(dir);

			VectorMA(il->color, exposure, r_bsp_light_state.sun.color, il->color);
		}
	}

	// resolve the light source position
	VectorMA(l->origin, LIGHTING_AMBIENT_DIST, dir, il->pos);
}

/*
 * @brief Qsort comparator for r_illumination_t. Orders by diffuse, descending.
 */
static int32_t R_CompareIlluminationDiffuse(const void *a, const void *b) {

	const r_illumination_t *il0 = (const r_illumination_t *) a;
	const r_illumination_t *il1 = (const r_illumination_t *) b;

	return (int32_t) (il1->diffuse - il0->diffuse);
}

#define LIGHTING_MAX_BSP_LIGHT_ILLUMINATIONS 128

/*
 * @brief Resolves the strongest static light sources, populating illuminations
 * for the specified structure and returning the number of light sources found.
 * This facilitates directional shading in the fragment program.
 */
static void R_UpdateBspLightIlluminations(r_lighting_t *l) {
	r_illumination_t illum[LIGHTING_MAX_BSP_LIGHT_ILLUMINATIONS];
	uint16_t i, n;

	const r_bsp_light_t *b = r_model_state.world->bsp->bsp_lights;

	// resolve all of the light sources that could contribute to this lighting
	for (i = n = 0; i < r_model_state.world->bsp->num_bsp_lights; i++, b++) {
		vec3_t dir;

		// is the light source within the PVS this frame
		if (l->state == LIGHTING_DIRTY) {
			if (b->leaf->vis_frame != r_locals.vis_frame)
				continue;
		}

		vec_t diffuse = 0.0;

		const vec_t *points[] = { l->origin, l->mins, l->maxs };

		// trace to the origin as well as the bounds
		for (uint16_t j = 0; j < lengthof(points); j++) {

			// is it within range of the entity
			VectorSubtract(b->origin, points[j], dir);

			// accounting for the scaled light radius
			const vec_t diff = b->radius * l->scale - VectorNormalize(dir);

			if (diff <= 0.0)
				continue;

			// is it visible to the entity
			const cm_trace_t tr = Cl_Trace(b->origin, points[j], NULL, NULL, l->number,
					CONTENTS_SOLID);

			if (tr.fraction == 1.0)
				diffuse += diff;
		}

		if (diffuse > 0.0) {
			r_illumination_t *il = &illum[n++];
			memset(il, 0, sizeof(*il));

			VectorCopy(b->origin, il->pos);
			VectorCopy(b->color, il->color);

			il->radius = b->radius;
			il->diffuse = diffuse / lengthof(points);

			if (n == LIGHTING_MAX_BSP_LIGHT_ILLUMINATIONS) {
				Com_Debug("LIGHTING_MAX_BSP_LIGHT_ILLUMINATIONS\n");
				break;
			}
		}
	}

	if (!n) // nothing nearby, we're done
		return;

	// sort them by diffuse contribution
	qsort(illum, n, sizeof(r_illumination_t), R_CompareIlluminationDiffuse);

	// take the N strongest illuminations
	n = MIN(n, MAX_BSP_LIGHT_ILLUMINATIONS);

	// and copy them in
	memcpy(&l->illuminations[1], illum, n * sizeof(r_illumination_t));
}

/*
 * @brief Qsort comparator for r_shadow_t. Orders by intensity, descending.
 */
static int32_t R_CompareShadowIntensity(const void *a, const void *b) {

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

	r_shadow_t *s = l->shadows;

	const r_illumination_t *il = l->illuminations;

	for (uint16_t i = 0; i < lengthof(l->illuminations); i++, il++) {

		if (il->radius == 0.0)
			break;

		const vec_t *points[] = { l->origin, l->mins, l->maxs };

		for (uint16_t j = 0; j < lengthof(points); j++) {
			vec3_t dir, pos;

			// check if the light exits the entity
			VectorSubtract(il->pos, points[j], dir);
			const vec_t dist = il->radius - VectorNormalize(dir);

			if (dist <= 0.0)
				continue;

			// project the farthest possible shadow impact position
			VectorMA(points[j], -dist, dir, pos);

			// trace, skipping the entity itself, impacting solids
			const cm_trace_t tr = Cl_Trace(points[j], pos, NULL, NULL, l->number, MASK_SOLID);

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

	qsort(l->shadows, s - l->shadows, sizeof(r_shadow_t), R_CompareShadowIntensity);
}

/*
 * @brief Resolves illumination and shadow information for the specified point.
 */
void R_UpdateLighting(r_lighting_t *l) {

	memset(l->illuminations, 0, sizeof(l->illuminations));
	memset(l->shadows, 0, sizeof(l->shadows));

	// resolve ambient and sunlight contributions
	R_UpdateWorldIllumination(l);

	// resolve the BSP light contributions
	R_UpdateBspLightIlluminations(l);

	// and resolve all shadows
	R_UpdateShadows(l);

	l->state = LIGHTING_READY;
}

/*
 * @brief Populates the remaining hardware light sources with illumination
 * information, if it is available. No state changes are persisted.
 */
void R_ApplyLighting(const r_lighting_t *l) {
	uint16_t count;

	count = r_locals.active_light_count;

	if (count < MAX_ACTIVE_LIGHTS) {
		vec4_t position = { 0.0, 0.0, 0.0, 1.0 };
		vec4_t diffuse = { 0.0, 0.0, 0.0, 1.0 };

		const r_illumination_t *il = l->illuminations;
		while (count < MAX_ACTIVE_LIGHTS) {

			if (il->radius == 0.0)
				break;

			VectorCopy(il->pos, position);
			glLightfv(GL_LIGHT0 + count, GL_POSITION, position);

			VectorCopy(il->color, diffuse);
			glLightfv(GL_LIGHT0 + count, GL_DIFFUSE, diffuse);

			glLightf(GL_LIGHT0 + count, GL_CONSTANT_ATTENUATION, il->radius);

			count++;
			il++;
		}
	}

	if (count < MAX_ACTIVE_LIGHTS) // disable the next light as a stop
		glLightf(GL_LIGHT0 + count, GL_CONSTANT_ATTENUATION, 0.0);
}
