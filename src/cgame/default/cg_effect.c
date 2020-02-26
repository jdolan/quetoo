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
	int32_t num_origins; // the number of origins
	vec4_t *origins; // the origins for particle spawns
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

	cgi.view->fog = Vec4(0.75, 0.75, 0.75, 1.0);

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
			err = sscanf(c + 4, "%f %f %f %f",
						 &cgi.view->fog.x,
						 &cgi.view->fog.y,
						 &cgi.view->fog.z,
						 &cgi.view->fog.w);
		}

		if (err != 3 && err != 4) { // default to gray
			cgi.view->fog = Vec4(0.75, 0.75, 0.75, 1.0);
		}
	}
}

/**
 * @brief Creates an emitter for the given surface. The number of origins for the
 * emitter depends on the area of the surface.
 */
static void Cg_LoadWeather_(const r_bsp_model_t *bsp, const r_bsp_face_t *s) {
	vec3_t center, delta;

	cg_weather_emit_t *e = cgi.Malloc(sizeof(cg_weather_emit_t), MEM_TAG_CGAME_LEVEL);

	// resolve the leaf for the point just in front of the surface

	center = Vec3_Mix(s->mins, s->maxs, 0.5);
	center = Vec3_Add(center, Vec3_Scale(s->plane->normal, 1.0));

	e->leaf = cgi.LeafForPoint(center, bsp);

	// resolve the number of origins based on surface area
	delta = Vec3_Subtract(s->maxs, s->mins);
	e->num_origins = Vec3_Length(delta) / 32.0;
	e->num_origins = Clampf(e->num_origins, 1, 128);

	e->origins = cgi.Malloc(sizeof(vec4_t) * e->num_origins, MEM_TAG_CGAME_LEVEL);

	// resolve the origins and their end positions

	int32_t i = 0;
	while (i < e->num_origins) {
		vec4_t *origin = e->origins + i;

		// randomize the origin over the surface

		const vec3_t org = Vec3_Add(Vec3_Mix3(s->mins, s->maxs, Vec3_Random()), s->plane->normal);

		vec3_t end = org;
		end.z -= MAX_WORLD_DIST;

		const cm_trace_t tr = cgi.Trace(org, end, Vec3_Zero(), Vec3_Zero(), 0, MASK_CLIP_PROJECTILE | MASK_LIQUID);
		if (!tr.surface) {
			continue;
		}

		*origin = Vec3_ToVec4(org, tr.end.z);
		i++;
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

	if (!(cgi.view->weather & (WEATHER_RAIN | WEATHER_SNOW))) {
		return;
	}

	const r_bsp_model_t *bsp = cgi.WorldModel()->bsp;
	const r_bsp_face_t *face = bsp->faces;

	// iterate the world surfaces, testing sky surfaces
	for (i = j = 0; i < bsp->num_faces; i++, face++) {

		// for downward facing sky brushes, create an emitter
		if ((face->texinfo->flags & SURF_SKY) && face->plane->normal.z < -0.1) {
			Cg_LoadWeather_(bsp, face);
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

	for (int32_t i = 0; i < e->num_origins; i++) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		const vec4_t origin = *(e->origins + i);

		p->origin = Vec3_Add(Vec4_XYZ(origin), Vec3_RandomRange(-16.f, 16.f));

		// keep particle z origin relatively close to the view origin
		if (origin.w < cgi.view->origin.z) {
			if (p->origin.z - cgi.view->origin.z > 512.0) {
				p->origin.z = cgi.view->origin.z + 256.0 + Randomf() * 256.0;
			}
		}

		if (cgi.view->weather & WEATHER_RAIN) {
			p->color = Color4bv(0x90909040);
			p->size = .33f;

			p->velocity = Vec3_RandomRange(-2.f, 2.f);
			p->velocity.z -= 600.f;

			p->acceleration = Vec3_RandomRange(-2.f, 2.f);
		} else {
			p->color = Color4bv(0xf0f0f090);
			p->size = 0.66f;

			p->velocity = Vec3_RandomRange(-12.f, 12.f);
			p->velocity.z -= 120.0;

			p->acceleration = Vec3_RandomRange(-12.f, 12.f);
		}

		// free the particle roughly when it will reach the floor
		p->lifetime = 1000 * (p->origin.z - origin.w) / fabsf(p->velocity.z);
	}
}

/**
 * @brief Adds particles and issues ambient loop sounds for weather effects.
 */
static void Cg_AddWeather(void) {

	if (!cg_add_weather->value) {
		return;
	}

	if (!(cgi.view->weather & (WEATHER_RAIN | WEATHER_SNOW))) {
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
