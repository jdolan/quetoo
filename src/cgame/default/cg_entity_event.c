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
#include "game/default/bg_pmove.h"
#include "collision/collision.h"

static vec3_t Cg_FibonacciLatticeDir(int32_t count, int32_t index) {
	// source: http://extremelearning.com.au/evenly-distributing-points-on-a-sphere/
	float phi = acosf(1.f - 2.f * index / count);
	float golden_ratio = (1.f + pow(5.f, .5f)) * .5f;
	float theta = 2.f * M_PI * index / golden_ratio;
	return Vec3(cosf(theta) * sinf(phi), sinf(theta) * sinf(phi), cosf(phi));
}

/**
 * @brief
 */
static void Cg_ItemRespawnEffect(const vec3_t org, const color_t color) {

	cg_sprite_t *s;

	int32_t particle_count = 64;
	vec3_t z_offset = org;
	z_offset.z += 20.f;

	// sphere particles
	for (int32_t i = 0; i < particle_count; i++) {

		if (!(s = Cg_AddSprite(&(cg_sprite_t) {
				.atlas_image = cg_sprite_particle2,
				.lifetime = 1000,
				.origin = z_offset,
				.velocity = Vec3_Scale(Cg_FibonacciLatticeDir(particle_count, i + 1), 55.f),
				.color = Vec4(144.f, 0.f, 1.f, 0.f),
				.end_color = Vec4(144.f, .83f, .6f, 0.f),
				.size = 10.f,
				.softness = 1.f
			}))) {
			break;
		}

		s->acceleration = Vec3_Scale(s->velocity, -1.f);
		s->size_velocity = -s->size / MILLIS_TO_SECONDS(s->lifetime);
	}

	// glow
	Cg_AddSprite(&(cg_sprite_t) {
		.origin = z_offset,
		.lifetime = 1000,
		.size = 150.f,
		.atlas_image = cg_sprite_particle,
		.color = Vec4(150.f, .5f, .4f, 0.f),
		.end_color = Vec4(150.f, .5f, 0.f, 0.f),
		.softness = 4.f
	});

	Cg_AddLight(&(cg_light_t) {
		.origin = org,
		.radius = 80.f,
		.color = Vec3(.1f, .6f, .3f),
		.decay = 1000
	});
}

/**
 * @brief
 */
static void Cg_ItemPickupEffect(const vec3_t org, const color_t color) {

	cg_sprite_t *s;

	// ring
	if ((s = Cg_AddSprite(&(cg_sprite_t) {
			.origin = org,
			.lifetime = 400,
			.size = 10.f,
			.atlas_image = cg_sprite_ring,
			.color = Vec4(150.f, .5f, .4f, 0.f),
			.end_color = Vec4(150.f, .5f, 0.f, 0.f),
			.dir = Vec3_Up()
		}))) {
		s->size_velocity = 50.f / MILLIS_TO_SECONDS(s->lifetime);
	}

	// glow
	Cg_AddSprite(&(cg_sprite_t) {
		.origin = org,
		.lifetime = 1000,
		.size = 150,
		.atlas_image = cg_sprite_particle,
		.color = Vec4(150.f, .5f, .4f, 0.f),
		.end_color = Vec4(150.f, .5f, 0.f, 0.f),
		.softness = 4.f
	});

	Cg_AddLight(&(cg_light_t) {
		.origin = org,
		.radius = 80.f,
		.color = Vec3(.2f, .4f, .3f),
		.decay = 1000
	});
}

/**
 * @brief
 */
static void Cg_TeleporterEffect(const vec3_t org) {

	for (int32_t i = 0; i < 64; i++) {

		Cg_AddSprite(&(cg_sprite_t) {
			.atlas_image = cg_sprite_particle,
			.size = 8.f,
			.origin = Vec3_Add(Vec3_Add(org, Vec3_RandomRange(-16.f, 16.f)), Vec3(0.f, 0.f, RandomRangef(8.f, 32.f))),
			.velocity = Vec3_Add(Vec3_RandomRange(-24.f, 24.f), Vec3(0.f, 0.f, RandomRangef(16.f, 48.f))),
			.acceleration.z = -SPRITE_GRAVITY * .1f,
			.lifetime = 500,
			.color = Vec4(0.f, 0.f, .87f, 0.f),
			.softness = 1.f
		});
	}

	Cg_AddLight(&(cg_light_t) {
		.origin = org,
		.radius = 120.f,
		.color = Vec3(.9f, .9f, .9f),
		.decay = 1000
	});
}

/**
 * @brief A player is gasping for air under water.
 */
static void Cg_GurpEffect(cl_entity_t *ent) {

	vec3_t start = ent->origin;
	start.z += 16.0;

	vec3_t end = start;
	end.z += 16.0;

	Cg_BubbleTrail(NULL, start, end, 2.f);
}

/**
 * @brief A player has drowned.
 */
static void Cg_DrownEffect(cl_entity_t *ent) {

	vec3_t start = ent->origin;
	start.z += 16.0;

	vec3_t end = start;
	end.z += 16.0;

	Cg_BubbleTrail(NULL, start, end, 2.f);
}

/**
 * @brief Loads a wildcard sample name for the specified client entity.
 */
static s_sample_t *Cg_ClientModelSample(const cl_entity_t *ent, const char *name) {

	const cg_client_info_t *info = &cg_state.clients[ent->current.client];

	assert(info->model);

	return cgi.LoadClientModelSample(info->model, name);
}

/**
 * @brief
 */
static s_sample_t *Cg_Footstep(cl_entity_t *ent) {

	vec3_t start = ent->current.origin;
	start.z += ent->current.bounds.mins.z;

	vec3_t end = start;
	end.z -= PM_STEP_HEIGHT;

	cm_trace_t tr = cgi.Trace(start, end, Box3_Zero(), ent->current.number, CONTENTS_MASK_SOLID);

	if (tr.material) {
		const cm_footsteps_t *footsteps = &cgi.LoadMaterial(tr.material->name, ASSET_CONTEXT_TEXTURES)->cm->footsteps;

		static uint32_t last_index = -1;
		uint32_t index = RandomRangeu(0, footsteps->num_samples);

		if (last_index == index) {
			index = (index ^ 1) % footsteps->num_samples;
		}

		last_index = index;

		return cgi.LoadSample(footsteps->samples[index].name);
	}

	Cg_Debug("No ground found for footstep at %s\n", vtos(end));
	return cgi.LoadSample(va("#players/common/step_default_%d", RandomRangei(0, 4)));
}

/**
 * @brief Process any event set on the given entity. These are only valid for a single
 * frame, so we reset the event flag after processing it.
 */
void Cg_EntityEvent(cl_entity_t *ent) {

	entity_state_t *s = &ent->current;

	s_play_sample_t play = {
		.origin = ent->current.origin,
		.atten = SOUND_ATTEN_SQUARE,
		.entity = s->number,
	};

	switch (s->event) {
		case EV_CLIENT_DROWN:
			play.sample = Cg_ClientModelSample(ent, "*drown_1");
			Cg_DrownEffect(ent);
			break;
		case EV_CLIENT_FALL:
			play.sample = Cg_ClientModelSample(ent, "*fall_2");
			break;
		case EV_CLIENT_FALL_FAR:
			play.sample = Cg_ClientModelSample(ent, "*fall_1");
			break;
		case EV_CLIENT_FOOTSTEP:
			play.sample = Cg_Footstep(ent);
			play.pitch = RandomRangei(-12, 13);
			break;
		case EV_CLIENT_GURP:
			play.sample = Cg_ClientModelSample(ent, "*gurp_1");
			Cg_GurpEffect(ent);
			break;
		case EV_CLIENT_JUMP:
			play.sample = Cg_ClientModelSample(ent, va("*jump_%d", RandomRangeu(1, 5)));
			break;
		case EV_CLIENT_LAND:
			play.sample = Cg_ClientModelSample(ent, "*land_1");
			break;
		case EV_CLIENT_SIZZLE:
			play.sample = Cg_ClientModelSample(ent, "*sizzle_1");
			break;
		case EV_CLIENT_TELEPORT:
			play.sample = cg_sample_teleport;
			Cg_TeleporterEffect(s->origin);
			break;

		case EV_ITEM_RESPAWN:
			play.sample = cg_sample_respawn;
			Cg_ItemRespawnEffect(s->origin, color_white); //TODO: wire up colors, white is placeholder
			break;
		case EV_ITEM_PICKUP:
			Cg_ItemPickupEffect(s->origin, color_white); // TODO: wire up colors, white is placeholder
			break;

		default:
			break;
	}

	if (play.sample) {
		Cg_AddSample(cgi.stage, &play);
	}

	s->event = 0;
}
