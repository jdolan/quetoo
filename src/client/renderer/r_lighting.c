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
#include "client.h"

#define LIGHTING_MAX_ILLUMINATIONS (MAX_ILLUMINATIONS * 16)

/**
 * @brief Provides a working area for gathering and sorting illuminations.
 * For a given point, all contributing illuminations are first resolved and
 * then sorted by contribution.
 */
typedef struct {
	r_illumination_t illuminations[LIGHTING_MAX_ILLUMINATIONS];
	uint16_t num_illuminations;
} r_illuminations_t;

static r_illuminations_t r_illuminations;

/**
 * @brief The impact points used to resolve illuminations.
 */
static vec3_t r_lighting_points[13];

/**
 * @brief Calculates the impact points for the given r_lighting_t.
 */
static void R_LightingPoints(const r_lighting_t *l) {
	vec3_t *p = r_lighting_points;

	VectorSet(p[0], l->mins[0], l->mins[1], l->mins[2]);
	VectorSet(p[1], l->mins[0], l->mins[1], l->origin[2]);
	VectorSet(p[2], l->mins[0], l->mins[1], l->maxs[2]);

	VectorSet(p[3], l->mins[0], l->maxs[1], l->mins[2]);
	VectorSet(p[4], l->mins[0], l->maxs[1], l->origin[2]);
	VectorSet(p[5], l->mins[0], l->maxs[1], l->maxs[2]);

	VectorSet(p[6], l->maxs[0], l->maxs[1], l->mins[2]);
	VectorSet(p[7], l->maxs[0], l->maxs[1], l->origin[2]);
	VectorSet(p[8], l->maxs[0], l->maxs[1], l->maxs[2]);

	VectorSet(p[9], l->maxs[0], l->mins[1], l->mins[2]);
	VectorSet(p[10], l->maxs[0], l->mins[1], l->origin[2]);
	VectorSet(p[11], l->maxs[0], l->mins[1], l->maxs[2]);

	VectorSet(p[12], l->origin[0], l->origin[1], l->origin[2]);
}

/**
 * @brief Adds an illumination with the given parameters.
 */
static void R_AddIllumination(const r_illumination_t *il) {

	if (r_illuminations.num_illuminations == LIGHTING_MAX_ILLUMINATIONS) {
		Com_Debug(DEBUG_RENDERER, "LIGHTING_MAX_ILLUMINATIONS\n");
		return;
	}

	r_illuminations.illuminations[r_illuminations.num_illuminations++] = *il;
}

#define LIGHTING_AMBIENT_RADIUS 400.0
#define LIGHTING_AMBIENT_DIST 320.0

/**
 * @brief Adds an illumination for ambient lighting. This weak illumination
 * is positioned far above the lighting point so that it's shadow projection
 * appears directional.
 */
static void R_AmbientIllumination(const r_lighting_t *l) {
	r_illumination_t il;

	vec_t max = 0.15;

	for (uint16_t i = 0; i < 3; i++) {
		if (r_bsp_light_state.ambient[i] > max) {
			max = r_bsp_light_state.ambient[i];
		}
	}

	VectorMA(l->origin, LIGHTING_AMBIENT_DIST, vec3_up, il.light.origin);
	VectorCopy(r_bsp_light_state.ambient, il.light.color);

	il.type = ILLUM_AMBIENT;
	il.light.radius = LIGHTING_AMBIENT_RADIUS;

	if (r_lighting->value) {
		il.light.radius *= r_lighting->value;
	}

	il.diffuse = il.light.radius - LIGHTING_AMBIENT_DIST;

	R_AddIllumination(&il);
}

#define LIGHTING_SUN_RADIUS 360.0
#define LIGHTING_SUN_DIST 260.0

/**
 * @brief Adds an illumination for each sun. Traces to the lighting bounds
 * determine sun exposure, which scales the applied sun color.
 */
static void R_SunIlluminations(const r_lighting_t *l) {
	/*r_illumination_t il;

	if (!r_bsp_light_state.sun.diffuse) {
		return;
	}

	const vec3_t *p = (const vec3_t *) r_lighting_points;

	vec_t exposure = 0.0;

	for (size_t i = 0; i < lengthof(r_lighting_points); i++) {
		vec3_t pos;

		VectorMA(p[i], MAX_WORLD_DIST, r_bsp_light_state.sun.dir, pos);

		const cm_trace_t tr = Cl_Trace(p[i], pos, NULL, NULL, l->number, CONTENTS_SOLID);

		if (tr.surface && (tr.surface->flags & SURF_SKY)) {
			exposure += (1.0 / lengthof(r_lighting_points));
		}
	}

	if (exposure == 0.0) {
		return;
	}

	VectorMA(l->origin, LIGHTING_SUN_DIST, r_bsp_light_state.sun.dir, il.light.origin);
	VectorScale(r_bsp_light_state.sun.color, exposure, il.light.color);

	il.type = ILLUM_SUN;
	il.light.radius = LIGHTING_SUN_RADIUS;

	if (r_lighting->value) {
		il.light.radius *= r_lighting->value;
	}

	il.diffuse = il.light.radius - LIGHTING_SUN_DIST;

	R_AddIllumination(&il);*/
}

/**
 * @brief Adds an illumination for the positional light source, if the given point is within range
 * and not occluded.
 */
static _Bool R_PositionalIllumination(const r_lighting_t *l, r_illumination_type_t type, const r_light_t *light) {
	r_illumination_t il;

	const vec3_t *p = (const vec3_t *) r_lighting_points;

	vec_t diffuse = 0.0;

	// trace to the origin as well as the bounds
	for (size_t i = 0; i < lengthof(r_lighting_points); i++) {
		vec3_t dir;

		// is it within range of the point in question
		VectorSubtract(light->origin, p[i], dir);

		const vec_t diff = light->radius * r_lighting->value - VectorLength(dir);

		if (diff <= 0.0) {
			continue;
		}

		// is it visible to the entity
		if (Cl_Trace(light->origin, p[i], NULL, NULL, l->number, CONTENTS_SOLID).fraction < 1.0) {
			continue;
		}

		diffuse += diff;
	}

	if (diffuse == 0.0) {
		return false;
	}

	il.type = type;
	il.light = *light;
	il.diffuse = diffuse / lengthof(r_lighting_points);

	R_AddIllumination(&il);
	return true;
}

/**
 * @brief Adds illuminations for static (BSP) light sources.
 */
static void R_StaticIlluminations(r_lighting_t *l) {
	byte pvs[MAX_BSP_LEAFS >> 3];

	const r_bsp_leaf_t *leaf = R_LeafForPoint(l->origin, NULL);
	Cm_ClusterPVS(leaf->cluster, pvs);

	const r_bsp_light_t *bl = r_model_state.world->bsp->bsp_lights;

	for (uint16_t i = 0; i < r_model_state.world->bsp->num_bsp_lights; i++, bl++) {

		const int16_t cluster = bl->leaf->cluster;
		if (cluster != -1) {
			if ((pvs[cluster >> 3] & (1 << (cluster & 7))) == 0) {
				continue;
			}
		}

		R_PositionalIllumination(l, ILLUM_STATIC, (const r_light_t *) & (bl->light));
	}
}

/**
 * @brief Resolves dynamic light source illuminations for the specified point.
 */
static void R_DynamicIlluminations(r_lighting_t *l) {

	uint64_t light_mask = 0;
	const r_light_t *dl = r_view.lights;

	for (uint16_t i = 0; i < r_view.num_lights; i++, dl++) {
		if (R_PositionalIllumination(l, ILLUM_DYNAMIC, dl)) {
			light_mask |= (uint64_t) (1 << i);
		}
	}

	if (!light_mask && !l->light_mask) {
		return;
	}

	l->light_mask = light_mask;
	l->state = LIGHTING_DIRTY;
}

/**
 * @brief Qsort comparator for r_illumination_t. Orders by light, descending.
 */
static int32_t R_CompareIllumination(const void *a, const void *b) {

	const r_illumination_t *il0 = (const r_illumination_t *) a;
	const r_illumination_t *il1 = (const r_illumination_t *) b;

	return Sign(il1->diffuse - il0->diffuse);
}

/**
 * @brief Updates illuminations for the specified point. If not dirty, and no
 * dynamic illuminations are active, the previous illuminations are retained.
 */
static void R_UpdateIlluminations(r_lighting_t *l) {

	r_illuminations.num_illuminations = 0;

	R_LightingPoints(l);

	R_DynamicIlluminations(l);

	// if not dirty, and no dynamic lighting, we're done
	if (l->state == LIGHTING_READY) {
		return;
	}

	memset(l->illuminations, 0, sizeof(l->illuminations));

	// otherwise, resolve ambient, sun and static illuminations as well
	R_AmbientIllumination(l);

	R_SunIlluminations(l);

	R_StaticIlluminations(l);

	r_illumination_t *il = r_illuminations.illuminations;

	// sort them by contribution
	qsort(il, r_illuminations.num_illuminations, sizeof(r_illumination_t), R_CompareIllumination);

	// take the strongest illuminations
	uint16_t n = Min(r_illuminations.num_illuminations, lengthof(l->illuminations));

	// and copy them in
	memcpy(l->illuminations, il, n * sizeof(r_illumination_t));
}

/**
 * @brief Qsort comparator for r_shadow_t. Orders by intensity, descending.
 */
static int32_t R_CompareShadow(const void *a, const void *b) {

	const r_shadow_t *s0 = (const r_shadow_t *) a;
	const r_shadow_t *s1 = (const r_shadow_t *) b;

	return Sign(s1->shadow - s0->shadow);
}

/**
 * @brief Cast shadows for an individual illumination.
 */
static void R_CastShadows(r_lighting_t *l, const r_illumination_t *il) {

	const vec3_t *p = (const vec3_t *) r_lighting_points;

	for (size_t j = 0; j < lengthof(r_lighting_points); j++) {
		vec3_t dir, pos;

		// check if the light exits the entity
		VectorSubtract(il->light.origin, p[j], dir);

		if (il->light.radius <= VectorNormalize(dir)) {
			continue;
		}

		// project the farthest possible shadow impact position
		VectorMA(il->light.origin, -il->light.radius, dir, pos);

		// trace, skipping the entity itself, impacting solids
		cm_trace_t tr = Cl_Trace(p[j], pos, NULL, NULL, l->number, MASK_SOLID);

		// check if the trace impacted a valid plane
		if (tr.start_solid || tr.fraction == 1.0) {
			continue;
		}

		// calculate the distance from the light source to the plane
		const vec_t dist = DotProduct(il->light.origin, tr.plane.normal) - tr.plane.dist;

		// and resolve the shadow intensity
		const vec_t shadow = Max(il->light.radius - dist, il->diffuse);

		// search for the plane in previous shadows
		r_shadow_t *s = l->shadows;
		while (s->illumination) {

			if (s - l->shadows == lengthof(l->shadows) - 1) {
				Com_Debug(DEBUG_RENDERER, "Entity %u @ %s: MAX_SHADOWS\n", l->number, vtos(l->origin));
				return;
			}

			if (s->illumination == il && s->plane.num == tr.plane.num) {
				s->shadow = Max(s->shadow, shadow);
				break;
			}

			s++;
		}

		if (s->illumination) { // already got it
			continue;
		}

		s->illumination = il;
		s->plane = tr.plane;
		s->shadow = shadow;

		// increment the plane's shadow count
		r_model_state.world->bsp->plane_shadows[s->plane.num]++;
	}
}

/**
 * @brief For each active illumination, trace from the specified lighting point
 * to the world, from the origin as well as the corners of the bounding box,
 * accounting for light attenuation within the radius of the object. Cast
 * shadows on impacted planes.
 */
static void R_UpdateShadows(r_lighting_t *l) {

	if (!r_shadows->value) {
		return;
	}

	// if not dirty and nothing has changed, retain our cached shadows
	if (l->state == LIGHTING_READY) {
		return;
	}

	// otherwise, refresh all shadow information based on the new illuminations
	r_shadow_t *s = l->shadows;
	for (size_t i = 0; i < lengthof(l->shadows); i++, s++) {
		if (s->plane.num) {
			r_model_state.world->bsp->plane_shadows[s->plane.num]--;
		}
	}

	memset(l->shadows, 0, sizeof(l->shadows));

	const r_illumination_t *il = l->illuminations;
	for (size_t i = 0; i < lengthof(l->illuminations); i++, il++) {

		if (il->diffuse == 0.0) {
			break;
		}

		switch (il->type) {
			case ILLUM_AMBIENT:
				break;
			case ILLUM_SUN:
				if (r_shadows->integer < 2) {
					continue;
				}
				break;
			case ILLUM_STATIC:
			case ILLUM_DYNAMIC:
				if (r_shadows->integer < 3) {
					continue;
				}
				break;
		}

		R_CastShadows(l, il);
	}

	qsort(l->shadows, lengthof(l->shadows), sizeof(r_shadow_t), R_CompareShadow);
}

/**
 * @brief Resolves illumination and shadow information for the specified point.
 */
void R_UpdateLighting(r_lighting_t *l) {

	R_UpdateIlluminations(l);

	R_UpdateShadows(l);

	l->state = LIGHTING_READY;
}
