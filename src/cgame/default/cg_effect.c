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
 * @brief Parses CS_WEATHER for weather and fog parameters, e.g. "rain fog 0.8 0.75 0.65 [128.0 2048.0 1.0]".
 */
void Cg_ResolveWeather(const char *weather) {

	Cg_Debug("%s\n", weather);

	cgi.view->weather = WEATHER_NONE;

	if (!weather || *weather == '\0') {
		return;
	}

	if (strstr(weather, "rain")) {
		cgi.view->weather |= WEATHER_RAIN;
	}

	if (strstr(weather, "snow")) {
		cgi.view->weather |= WEATHER_SNOW;
	}
}

/**
 * @brief Creates an emitter for the given surface. The number of origins for the
 * emitter depends on the area of the surface.
 */
static void Cg_LoadWeather_(const r_bsp_face_t *face) {

	cg_weather_emit_t *e = cgi.Malloc(sizeof(cg_weather_emit_t), MEM_TAG_CGAME_LEVEL);

	// resolve the leaf for the point just in front of the surface

	vec3_t mins = Vec3_Mins();
	vec3_t maxs = Vec3_Maxs();

	const r_bsp_vertex_t *v = face->vertexes;
	for (int32_t i = 0; i < face->num_vertexes; i++, v++) {
		mins = Vec3_Minf(mins, v->position);
		maxs = Vec3_Maxf(maxs, v->position);
	}

	vec3_t center = Vec3_Mix(mins, maxs, 0.5);
	center = Vec3_Add(center, Vec3_Scale(face->plane->cm->normal, 1.0));

	e->leaf = cgi.LeafForPoint(center);

	// resolve the number of origins based on surface area
	vec3_t delta = Vec3_Subtract(maxs, mins);
	e->num_origins = Vec3_Length(delta) / 32.0;
	e->num_origins = Clampf(e->num_origins, 1, 128);

	e->origins = cgi.Malloc(sizeof(vec4_t) * e->num_origins, MEM_TAG_CGAME_LEVEL);

	// resolve the origins and their end positions

	int32_t i = 0;
	while (i < e->num_origins) {
		vec4_t *origin = e->origins + i;

		// randomize the origin over the surface

		const vec3_t org = Vec3_Add(Vec3_Mix3(mins, maxs, Vec3_Random()), face->plane->cm->normal);

		vec3_t end = org;
		end.z -= MAX_WORLD_DIST;

		const cm_trace_t tr = cgi.Trace(org, end, Vec3_Zero(), Vec3_Zero(), 0, CONTENTS_MASK_CLIP_PROJECTILE | CONTENTS_MASK_LIQUID);
		if (!tr.texinfo) {
			continue;
		}

		*origin = Vec3_ToVec4(org, tr.end.z);
		i++;
	}

	// push on to the linked list
	e->next = cg_weather_state.emits;
	cg_weather_state.emits = e;

	Cg_Debug("%s: %d origins\n", vtos(center), e->num_origins);
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
		if ((face->texinfo->flags & SURF_SKY) && face->plane->cm->normal.z < -0.1) {
			Cg_LoadWeather_(face);
			j++;
		}
	}

	Cg_Debug("%d emits\n", j);
}

/**
 * @brief Loads all resources required by client-side effects such as weather.
 */
void Cg_LoadEffects(void) {
	Cg_LoadWeather();
}

/**
 * @brief Adds weather sprites for the specified emitter. The number of sprites
 * added is dependent on the size of the surface associated with the emitter.
 */
static void Cg_AddWeather_(const cg_weather_emit_t *e) {

	for (int32_t i = 0; i < e->num_origins; i++) {
		const vec4_t origin = *(e->origins + i);

		vec3_t sprite_origin = Vec3_Add(Vec4_XYZ(origin), Vec3_RandomRange(-16.f, 16.f));

		// keep sprite z origin relatively close to the view origin
		if (origin.w < cgi.view->origin.z) {
			if (sprite_origin.z - cgi.view->origin.z > 512.0) {
				sprite_origin.z = cgi.view->origin.z + 256.0 + Randomf() * 256.0;
			}
		}

		// free the sprite roughly when it will reach the floor
		cg_sprite_t *s;

		if (cgi.view->weather & WEATHER_RAIN) {
			s = Cg_AddSprite(&(cg_sprite_t) {
				.origin = sprite_origin,
				.atlas_image = cg_sprite_rain,
				.color = Vec4(0.f, 0.f, .87f, .8f),
				.end_color = Vec4(0.f, 0.f, .87f, .0f),
				.size = 8.f,
				.velocity = Vec3_Subtract(Vec3_RandomRange(-2.f, 2.f), Vec3(0.f, 0.f, 600.f)),
				.acceleration = Vec3_RandomRange(-2.f, 2.f),
				.flags = SPRITE_NO_BLEND_DEPTH,
				.axis = SPRITE_AXIS_X | SPRITE_AXIS_Y
			});
		} else {
			s = Cg_AddSprite(&(cg_sprite_t) {
				.origin = sprite_origin,
				.atlas_image = cg_sprite_snow,
				.color = Vec4(0.f, 0.f, .87f, .4f),
				.end_color = Vec4(0.f, 0.f, .87f, .0f),
				.size = 8.f,
				.velocity = Vec3_Subtract(Vec3_RandomRange(-12.f, 12.f), Vec3(0.f, 0.f, 120.f)),
				.acceleration = Vec3_RandomRange(-12.f, 12.f),
				.flags = SPRITE_NO_BLEND_DEPTH
			});
		}

		if (!s) {
			return;
		}

		s->lifetime = 1000 * (sprite_origin.z - origin.w) / fabsf(s->velocity.z);
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
		if (!cgi.CullBox(e->leaf->mins, e->leaf->maxs)) {
			Cg_AddWeather_(e);
		}
		e = e->next;
	}
}

/**
 * @brief Adds an ambient loop sound when the player's view is underwater.
 */
static void Cg_AddUnderwater(void) {

	if (cgi.view->contents & CONTENTS_MASK_LIQUID) {
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
