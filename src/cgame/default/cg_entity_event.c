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

/**
 * @brief
 */
static void Cg_ItemRespawnEffect(const vec3_t org) {

	for (int32_t i = 0; i < 64; i++) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		p->origin = Vec3_Add(org, Vec3_RandomRange(-8.f, 8.f));
		p->origin.z += 8.f;

		p->velocity = Vec3_RandomRange(-24.f, 24.f);
		p->velocity.z = fabsf(p->velocity.z);

		p->acceleration = Vec3_RandomRange(-24.f, 24.f);
		p->acceleration.z = RandomRangef(60.f, 90.f);

		p->lifetime = RandomRangef(800.f, 1200.f);

		p->color = Color3f(.3f, .6f, .3f);
		p->color_velocity = Vec4_Scale(Vec4(.3f, .4f, .3f, -1.f), 1.f / p->lifetime);
	}

	Cg_AddLight(&(cg_light_t) {
		.origin = org,
		.radius = 80.f,
		.color = Vec3(.9f, 1.f, .9f),
		.decay = 1000
	});
}

/**
 * @brief
 */
static void Cg_ItemPickupEffect(const vec3_t org) {

	for (int32_t i = 0; i < 32; i++) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		p->origin = Vec3_Add(org, Vec3_RandomRange(-8.0, 8.0));
		p->origin.z += 8.f;

		p->velocity = Vec3_RandomRange(-16.f, 16.f);
		p->velocity.z += 100.f;

		p->acceleration.z = PARTICLE_GRAVITY * 0.2;

		p->lifetime = 500 + Randomf() * 100;

		p->color = Color3b(224, 224, 224);
//		p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

		p->size = 1.0;
//		p->delta_size = 0.2 * -p->lifetime / PARTICLE_FRAME;
	}

	Cg_AddLight(&(cg_light_t) {
		.origin = org,
		.radius = 80.0,
		.color = Vec3(0.9, 1.0, 1.0),
		.decay = 1000,
	});
}

/**
 * @brief
 */
static void Cg_TeleporterEffect(const vec3_t org) {

	for (int32_t i = 0; i < 64; i++) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		p->origin = Vec3_Add(org, Vec3_RandomRange(-16.f, 16.f));
		p->origin.z += RandomRangef(8.f, 32.f);

		p->velocity = Vec3_RandomRange(-24.f, 24.f);
		p->velocity.z = RandomRangef(16.f, 48.f);

		p->acceleration.z = -PARTICLE_GRAVITY * 0.1;

		p->lifetime = 500;

		p->color = Color3b(224, 224, 224);
//		p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

		p->size = 1.0;
//		p->delta_size = 0.2 * -p->lifetime / PARTICLE_FRAME;
	}

	Cg_AddLight(&(cg_light_t) {
		.origin = org,
		.radius = 120.f,
		.color = Vec3(.9f, .9f, .9f),
		.decay = 1000
	});

	const s_play_sample_t play = {
		.sample = cg_sample_respawn,
		.origin = org,
		.attenuation = ATTEN_IDLE,
		.flags = S_PLAY_POSITIONED
	};

	cgi.AddSample(&play);
}

/**
 * @brief A player is gasping for air under water.
 */
static void Cg_GurpEffect(cl_entity_t *ent) {

	const s_play_sample_t play = {
		.sample = cgi.LoadSample("*gurp_1"),
		.entity = ent->current.number,
		.attenuation = ATTEN_NORM,
		.flags = S_PLAY_ENTITY
	};

	cgi.AddSample(&play);

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

	const s_play_sample_t play = {
		.sample = cgi.LoadSample("*drown_1"),
		.entity = ent->current.number,
		.attenuation = ATTEN_NORM,
		.flags = S_PLAY_ENTITY
	};

	cgi.AddSample(&play);

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

	if (tr.fraction < 1.0 && tr.surface && tr.surface->material && *tr.surface->material->footsteps) {
		footsteps = tr.surface->material->footsteps;
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
			Cg_ItemRespawnEffect(s->origin);
			break;
		case EV_ITEM_PICKUP:
			Cg_ItemPickupEffect(s->origin);
			break;

		default:
			break;
	}

	if (play.sample) {
		cgi.AddSample(&play);
	}

	s->event = 0;
}
