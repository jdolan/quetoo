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

	int32_t particle_count = 256;
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
				.size = 5.f
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
		.size = 200.f,
		.atlas_image = cg_sprite_particle,
		.color = Vec4(0.f, 0.f, 1.f, 0.f),
		.end_color = Vec4(0.f, 0.f, 0.f, 0.f)
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
			.dir = Vec3(0.f, 0.f, 1.f)
		}))) {
		s->size_velocity = 50.f / MILLIS_TO_SECONDS(s->lifetime);
	}

	// glow
	Cg_AddSprite(&(cg_sprite_t) {
		.origin = org,
		.lifetime = 1000,
		.size = 200.f,
		.atlas_image = cg_sprite_particle,
		.color = Vec4(150.f, .5f, .4f, 0.f),
		.end_color = Vec4(150.f, .5f, 0.f, 0.f)
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
			.color = Vec4(0.f, 0.f, .87f, 0.f)
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

	vec3_t start = ent->current.origin;
	start.z += 16.0;

	vec3_t end = start;
	end.z += 16.0;

	Cg_BubbleTrail(NULL, start, end, 32.0);
}

/**
 * @brief A player has drowned.
 */
static void Cg_DrownEffect(cl_entity_t *ent) {

	vec3_t start = ent->current.origin;
	start.z += 16.0;

	vec3_t end = start;
	end.z += 16.0;

	Cg_BubbleTrail(NULL, start, end, 32.0);
}

/**
 * @brief
 */
static s_sample_t *Cg_Footstep(cl_entity_t *ent) {

	const char *footsteps = "default";

	vec3_t start = ent->current.origin;
	start.z += ent->current.mins.z;

	vec3_t end = start;
	end.z -= PM_STEP_HEIGHT;

	cm_trace_t tr = cgi.Trace(start, end, Vec3_Zero(), Vec3_Zero(), ent->current.number, CONTENTS_MASK_SOLID);

	if (tr.fraction < 1.0 && tr.texinfo && tr.texinfo->material && *tr.texinfo->material->footsteps) {
		footsteps = tr.texinfo->material->footsteps;
	}

	return Cg_GetFootstepSample(footsteps);
}

/**
 * @brief Process any event set on the given entity. These are only valid for a single
 * frame, so we reset the event flag after processing it.
 */
void Cg_EntityEvent(cl_entity_t *ent) {

	entity_state_t *s = &ent->current;

	s_play_sample_t play = {
		.entity = s->number,
		.attenuation = ATTEN_NORM,
		.flags = S_PLAY_ENTITY,
	};

	switch (s->event) {
		case EV_CLIENT_DROWN:
			play.sample = cgi.LoadSample("*drown_1");
			Cg_DrownEffect(ent);
			break;
		case EV_CLIENT_FALL:
			play.sample = cgi.LoadSample("*fall_2");
			break;
		case EV_CLIENT_FALL_FAR:
			play.sample = cgi.LoadSample("*fall_1");
			break;
		case EV_CLIENT_FOOTSTEP:
			play.sample = Cg_Footstep(ent);
			play.pitch = RandomRangei(-12, 13);
			break;
		case EV_CLIENT_GURP:
			play.sample = cgi.LoadSample("*gurp_1");
			Cg_GurpEffect(ent);
			break;
		case EV_CLIENT_JUMP:
			play.sample = cgi.LoadSample(va("*jump_%d", RandomRangeu(1, 5)));
			break;
		case EV_CLIENT_LAND:
			play.sample = cgi.LoadSample("*land_1");
			break;
		case EV_CLIENT_STEP: {
			const float height = ent->current.origin.z - ent->prev.origin.z;
			Cg_TraverseStep(&ent->step, cgi.client->unclamped_time, height);
		}
			break;
		case EV_CLIENT_SIZZLE:
			play.sample = cgi.LoadSample("*sizzle_1");
			break;
		case EV_CLIENT_TELEPORT:
			play.sample = cg_sample_teleport;
			play.attenuation = ATTEN_IDLE;
			Cg_TeleporterEffect(s->origin);
			break;

		case EV_ITEM_RESPAWN:
			play.sample = cg_sample_respawn;
			play.attenuation = ATTEN_IDLE;
			Cg_ItemRespawnEffect(s->origin, color_white); //TODO: wire up colors, white is placeholder
			break;
		case EV_ITEM_PICKUP:
			Cg_ItemPickupEffect(s->origin, color_white); // TODO: wire up colors, white is placeholder
			break;

		default:
			break;
	}

	if (play.sample) {
		cgi.AddSample(&play);
	}

	s->event = 0;
}
