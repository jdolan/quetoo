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

#include "cg_local.h"

// weather emitters are bound to downward-facing sky surfaces
typedef struct cg_weather_emit_s {
	const r_bsp_leaf_t *leaf;
	uint16_t num_origins; // the number of origins
	float *origins; // the origins for particle spawns
	float *end_z; // the "floors" where particles are freed
	struct cg_weather_emit_s *next;
} cg_weather_emit_t;

static cg_weather_emit_t *cg_weather_emits;
static uint32_t cg_weather_time;

/*
 * @brief Creates an emitter for the given surface. The number of origins for the
 * emitter depends on the area of the surface.
 */
static void Cg_LoadWeather_(const r_model_t *world, const r_bsp_surface_t *s) {
	vec3_t delta;
	uint16_t i;

	cg_weather_emit_t *e = cgi.Malloc(sizeof(cg_weather_emit_t), TAG_CGAME_MEDIA);

	// resolve the leaf for the point just in front of the surface
	VectorMA(s->center, 1.0, s->normal, delta);
	e->leaf = cgi.LeafForPoint(delta, world);

	// resolve the number of origins based on surface area
	VectorSubtract(s->maxs, s->mins, delta);
	e->num_origins = VectorLength(delta) / 64.0;

	if (e->num_origins < 1) { // clamp it
		e->num_origins = 1;
	}

	e->origins = cgi.Malloc(sizeof(vec3_t) * e->num_origins, TAG_CGAME_MEDIA);
	e->end_z = cgi.Malloc(sizeof(vec_t) * e->num_origins, TAG_CGAME_MEDIA);

	// resolve the origins and end_z
	for (i = 0; i < e->num_origins; i++) {
		float *org = &e->origins[i * 3];
		uint16_t j;

		// randomize the origin over the surface
		for (j = 0; j < 3; j++) {
			org[j] = s->mins[j] + Randomf() * delta[j];
		}

		VectorAdd(org, s->normal, org);

		vec3_t end;
		VectorSet(end, org[0], org[1], org[2] - 8192.0);

		c_trace_t trace = cgi.Trace(org, end, vec3_origin, vec3_origin, MASK_SHOT);
		e->end_z[i] = trace.end[2];
	}

	// push on to the linked list
	e->next = cg_weather_emits;
	cg_weather_emits = e;

	cgi.Debug("Cg_LoadWeather: %s: %d origins\n", vtos(s->center), e->num_origins);
}

/*
 * @brief Iterates the world surfaces, generating weather emitters from sky brushes.
 * Valid weather origins and z-depths are resolved and cached.
 */
void Cg_LoadWeather(void) {
	uint16_t i, j;

	cg_weather_emits = NULL;
	cg_weather_time = 0;

	const r_model_t *world = cgi.LoadModel(cgi.ConfigString(CS_MODELS + 1));
	const r_bsp_surface_t *s = world->surfaces;

	// iterate the world surfaces, testing sky surfaces
	for (i = j = 0; i < world->num_surfaces; i++, s++) {

		// for downward facing sky brushes, create an emitter
		if ((s->texinfo->flags & SURF_SKY) && s->normal[2] < -0.1) {
			Cg_LoadWeather_(world, s);
			j++;
		}
	}

	cgi.Debug("Cg_LoadWeather: %d emits\n", j);
}

/*
 * @brief Loads all resources required by client-side effects such as weather.
 */
void Cg_LoadEffects(void) {
	Cg_LoadWeather();
}

/*
 * @brief Adds weather particles for the specified emitter. The number of particles
 * added is dependent on the size of the surface associated with the emitter.
 */
static void Cg_AddWeather_(const cg_weather_emit_t *e) {
	int32_t i;

	for (i = 0; i < e->num_origins; i++) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle(PARTICLE_WEATHER)))
			return;

		const float *org = &e->origins[i * 3];
		int32_t j;

		// setup the origin and end_z
		for (j = 0; j < 3; j++) {
			p->org[j] = org[j] + Randomc() * 16.0;
		}

		p->end_z = e->end_z[i];

		// keep particle z origin relatively close to the view origin
		if (p->end_z < cgi.view->origin[2]) {
			if (p->org[2] - cgi.view->origin[2] > 512.0) {
				p->org[2] = cgi.view->origin[2] + 256.0 + Randomf() * 256.0;
			}
		}

		if (cgi.view->weather & WEATHER_RAIN) {
			// randomize the velocity and acceleration
			for (j = 0; j < 2; j++) {
				p->vel[j] = Randomc() * 2.0;
				p->accel[j] = Randomc() * 2.0;
			}
			p->image = cg_particle_rain;
			p->vel[2] = -800.0;
			p->alpha = 0.4;
			p->color = 8;
			p->scale = 6.0;
		} else {
			// randomize the velocity and acceleration
			for (j = 0; j < 2; j++) {
				p->vel[j] = Randomc() * 12.0;
				p->accel[j] = Randomc() * 12.0;
			}
			p->image = cg_particle_snow;
			p->vel[2] = -120.0;
			p->alpha = 0.6;
			p->alpha_vel = Randomf() * -1.0;
			p->color = 8;
			p->scale = 1.5;
		}
	}
}

/*
 * @brief Adds particles and issues ambient loop sounds for weather effects.
 */
static void Cg_AddWeather(void) {

	if (!cg_add_weather->value)
		return;

	if (!(cgi.view->weather & (WEATHER_RAIN | WEATHER_SNOW)))
		return;

	if (cgi.client->time - cg_weather_time < 100)
		return;

	cg_weather_time = cgi.client->time;

	const cg_weather_emit_t *e = cg_weather_emits;

	while (e) {
		if (cgi.LeafInPhs(e->leaf)) {
			Cg_AddWeather_(e);
		}
		e = e->next;
	}

	s_sample_t *s; // add an appropriate looping sound

	if (cgi.view->weather & WEATHER_RAIN) {
		s = cg_sample_rain;
	} else {
		s = cg_sample_snow;
	}

	cgi.LoopSample(cgi.view->origin, s);
}

/**
 * Cg_AddEffects
 */
void Cg_AddEffects() {
	Cg_AddWeather();
}
