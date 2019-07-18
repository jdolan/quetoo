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

#include "cg_local.h"

// weather emitters are bound to downward-facing sky surfaces
typedef struct cg_weather_emit_s {
	const r_bsp_leaf_t *leaf;
	uint16_t num_origins; // the number of origins
	vec_t *origins; // the origins for particle spawns
	vec_t *end_z; // the "floors" where particles are freed
	struct cg_weather_emit_s *next;
} cg_weather_emit_t;

typedef struct {
	cg_weather_emit_t *emits;
	uint32_t time;
} cg_weather_state_t;

static cg_weather_state_t cg_weather_state;

/**
 * @brief Parses CS_WEATHER for weather and fog parameters, e.g. "rain fog 0.8 0.75 0.65 [1.0]".
 */
void Cg_ResolveWeather(const char *weather) {
	char *c;
	int32_t err;

	cgi.Debug("%s\n", weather);

	cgi.view->weather = WEATHER_NONE;

	Vector4Set(cgi.view->fog_color, 0.75, 0.75, 0.75, 1.0);

	if (!weather || *weather == '\0') {
		return;
	}

	if (strstr(weather, "rain")) {
		cgi.view->weather |= WEATHER_RAIN;
	}

	if (strstr(weather, "snow")) {
		cgi.view->weather |= WEATHER_SNOW;
	}

	if ((c = strstr(weather, "fog"))) {

		cgi.view->weather |= WEATHER_FOG;
		err = -1;

		if (strlen(c) > 3) { // try to parse fog color
			vec_t *f = cgi.view->fog_color;

			err = sscanf(c + 4, "%f %f %f %f", f, f + 1, f + 2, f + 3);
		}

		if (err != 3 && err != 4) { // default to gray
			Vector4Set(cgi.view->fog_color, 0.75, 0.75, 0.75, 1.0);
		}
	}
}

/**
 * @brief Creates an emitter for the given surface. The number of origins for the
 * emitter depends on the area of the surface.
 */
static void Cg_LoadWeather_(const r_bsp_model_t *bsp, const r_bsp_surface_t *s) {
	vec3_t center, delta;
	uint16_t i;

	cg_weather_emit_t *e = cgi.Malloc(sizeof(cg_weather_emit_t), MEM_TAG_CGAME_LEVEL);

	// resolve the leaf for the point just in front of the surface

	VectorMix(s->mins, s->maxs, 0.5, center);
	VectorMA(center, 1.0, s->plane->normal, center);

	e->leaf = cgi.LeafForPoint(center, bsp);

	// resolve the number of origins based on surface area
	VectorSubtract(s->maxs, s->mins, delta);
	e->num_origins = VectorLength(delta) / 32.0;
	e->num_origins = Clamp(e->num_origins, 1, 128);

	e->origins = cgi.Malloc(sizeof(vec3_t) * e->num_origins, MEM_TAG_CGAME_LEVEL);
	e->end_z = cgi.Malloc(sizeof(vec_t) * e->num_origins, MEM_TAG_CGAME_LEVEL);

	// resolve the origins and end_z
	for (i = 0; i < e->num_origins; i++) {
		vec_t *org = &e->origins[i * 3];
		uint16_t j;

		// randomize the origin over the surface
		for (j = 0; j < 3; j++) {
			org[j] = s->mins[j] + Randomf() * delta[j];
		}

		VectorAdd(org, s->plane->normal, org);

		vec3_t end;
		VectorSet(end, org[0], org[1], org[2] - MAX_WORLD_DIST);

		cm_trace_t trace = cgi.Trace(org, end, NULL, NULL, 0, MASK_CLIP_PROJECTILE | MASK_LIQUID);
		e->end_z[i] = trace.end[2];
	}

	// push on to the linked list
	e->next = cg_weather_state.emits;
	cg_weather_state.emits = e;

	cgi.Debug("%s: %d origins\n", vtos(center), e->num_origins);
}

/**
 * @brief Iterates the world surfaces, generating weather emitters from sky brushes.
 * Valid weather origins and z-depths are resolved and cached.
 */
static void Cg_LoadWeather(void) {
	int32_t i, j;

	cg_weather_state.emits = NULL;
	cg_weather_state.time = 0;

	Cg_ResolveWeather(cgi.ConfigString(CS_WEATHER));

	if (!(cgi.view->weather & WEATHER_PRECIP_MASK)) {
		return;
	}

	const r_bsp_model_t *bsp = cgi.WorldModel()->bsp;
	const r_bsp_surface_t *s = bsp->surfaces;

	// iterate the world surfaces, testing sky surfaces
	for (i = j = 0; i < bsp->num_surfaces; i++, s++) {

		// for downward facing sky brushes, create an emitter
		if ((s->texinfo->flags & SURF_SKY) && s->plane->normal[2] < -0.1) {
			Cg_LoadWeather_(bsp, s);
			j++;
		}
	}

	cgi.Debug("%d emits\n", j);
}

/**
 * @brief Loads all resources required by client-side effects such as weather.
 */
void Cg_LoadEffects(void) {
	Cg_LoadWeather();
}

/**
 * @brief Adds weather particles for the specified emitter. The number of particles
 * added is dependent on the size of the surface associated with the emitter.
 */
static void Cg_AddWeather_(const cg_weather_emit_t *e) {

	vec3_t color;
	cgi.ColorFromPalette(8, color);

	for (int32_t i = 0; i < e->num_origins; i++) {
		cg_particles_t *ps;
		cg_particle_t *p;
		int32_t j;

		ps = cgi.view->weather & WEATHER_RAIN ? cg_particles_rain : cg_particles_snow;

		if (!(p = Cg_AllocParticle(ps))) {
			break;
		}

		const vec_t *org = &e->origins[i * 3];

		// setup the origin and end_z
		for (j = 0; j < 3; j++) {
			p->part.org[j] = org[j] + Randomc() * 16.0;
		}

		p->weather.end_z = e->end_z[i];

		// keep particle z origin relatively close to the view origin
		if (p->weather.end_z < cgi.view->origin[2]) {
			if (p->part.org[2] - cgi.view->origin[2] > 512.0) {
				p->part.org[2] = cgi.view->origin[2] + 256.0 + Randomf() * 256.0;
			}
		}

		if (cgi.view->weather & WEATHER_RAIN) {
			VectorCopy(color, p->part.color);
			p->part.color[3] = 0.4;
			p->part.scale = 6.0;
			// randomize the velocity and acceleration
			for (j = 0; j < 2; j++) {
				p->vel[j] = Randomc() * 2.0;
				p->accel[j] = Randomc() * 2.0;
			}
			p->vel[2] = -600.0;
		} else {
			p->effects |= PARTICLE_EFFECT_COLOR;

			VectorCopy(color, p->color_start);
			p->color_start[3] = 0.6;

			VectorCopy(color, p->color_end);
			p->color_end[3] = 0.0;

			// TODO: this may not work for extremely long snow tubes
			p->lifetime = 500 + Randomf() * 2000;

			p->part.scale = 1.5;

			// randomize the velocity and acceleration
			for (j = 0; j < 2; j++) {
				p->vel[j] = Randomc() * 12.0;
				p->accel[j] = Randomc() * 12.0;
			}
			p->vel[2] = -120.0;
		}
	}
}

/**
 * @brief Adds particles and issues ambient loop sounds for weather effects.
 */
static void Cg_AddWeather(void) {

	if (!cg_add_weather->value) {
		return;
	}

	if (!(cgi.view->weather & WEATHER_PRECIP_MASK)) {
		return;
	}

	const s_sample_t *sample; // add an appropriate looping sound

	if (cgi.view->weather & WEATHER_RAIN) {
		sample = cg_sample_rain;
	} else {
		sample = cg_sample_snow;
	}

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = sample,
		 .flags = S_PLAY_AMBIENT | S_PLAY_LOOP | S_PLAY_FRAME
	});

	if (cgi.client->unclamped_time - cg_weather_state.time < 100) {
		return;
	}

	cg_weather_state.time = cgi.client->unclamped_time;

	const cg_weather_emit_t *e = cg_weather_state.emits;

	while (e) {
		if (cgi.LeafHearable(e->leaf)) {
			Cg_AddWeather_(e);
		}
		e = e->next;
	}
}

/**
 * @brief Adds an ambient loop sound when the player's view is underwater.
 */
static void Cg_AddUnderwater(void) {

	if (cgi.view->contents & MASK_LIQUID) {
		cgi.AddSample(&(const s_play_sample_t) {
			.sample = cg_sample_underwater,
			 .flags = S_PLAY_AMBIENT | S_PLAY_LOOP | S_PLAY_FRAME
		});
	}
}

/**
 * @brief
 */
void Cg_AddEffects() {

	Cg_AddWeather();

	Cg_AddUnderwater();
}
