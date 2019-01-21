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

	vec3_t color;
	cgi.ColorFromPalette(110, color);

	for (int32_t i = 0; i < 64; i++) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, NULL, false))) {
			break;
		}

		p->lifetime = 500 + Randomf() * 100;
		p->effects |= PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

		VectorCopy(color, p->color_start);
		VectorCopy(color, p->color_end);
		p->color_end[3] = 0.0;

		p->scale_start = 1.0;
		p->scale_end = 0.8;

		p->part.org[0] = org[0] + Randomc() * 8.0;
		p->part.org[1] = org[1] + Randomc() * 8.0;
		p->part.org[2] = org[2] + 8.0 + Randomf() * 8.0;

		p->vel[0] = Randomc() * 48.0;
		p->vel[1] = Randomc() * 48.0;
		p->vel[2] = Randomf() * 48.0;

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY * 0.1;
	}

	r_sustained_light_t s;
	VectorCopy(org, s.light.origin);
	s.light.radius = 80.0;
	VectorSet(s.light.color, 0.9, 0.9, 0.9);
	s.sustain = 1000;

	cgi.AddSustainedLight(&s);
}

/**
 * @brief
 */
static void Cg_ItemPickupEffect(const vec3_t org) {

	vec3_t color;
	cgi.ColorFromPalette(110, color);

	for (int32_t i = 0; i < 32; i++) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, NULL, false))) {
			break;
		}

		p->lifetime = 500 + Randomf() * 100;
		p->effects |= PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

		VectorCopy(color, p->color_start);
		VectorCopy(color, p->color_end);
		p->color_end[3] = 0.0;

		p->scale_start = 1.0;
		p->scale_end = 0.8;

		p->part.org[0] = org[0] + Randomc() * 8.0;
		p->part.org[1] = org[1] + Randomc() * 8.0;
		p->part.org[2] = org[2] + 8 + Randomc() * 16.0;

		p->vel[0] = Randomc() * 16.0;
		p->vel[1] = Randomc() * 16.0;
		p->vel[2] = Randomf() * 128.0;

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = PARTICLE_GRAVITY * 0.2;
	}

	r_sustained_light_t s;
	VectorCopy(org, s.light.origin);
	s.light.radius = 80.0;
	VectorSet(s.light.color, 0.9, 1.0, 1.0);
	s.sustain = 1000;

	cgi.AddSustainedLight(&s);
}

/**
 * @brief
 */
static void Cg_TeleporterEffect(const vec3_t org) {

	vec3_t color;
	cgi.ColorFromPalette(110, color);

	for (int32_t i = 0; i < 64; i++) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, NULL, false))) {
			break;
		}

		p->lifetime = 500;
		p->effects |= PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

		VectorCopy(color, p->color_start);
		VectorCopy(color, p->color_end);
		p->color_end[3] = 0.0;

		p->scale_start = 1.0;
		p->scale_end = 0.8;

		p->part.org[0] = org[0] + Randomc() * 16.0;
		p->part.org[1] = org[1] + Randomc() * 16.0;
		p->part.org[2] = org[2] + 8.0 + Randomf() * 24.0;

		p->vel[0] = Randomc() * 24.0;
		p->vel[1] = Randomc() * 24.0;
		p->vel[2] = Randomf() * 64.0;

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY * 0.1;
	}

	r_sustained_light_t s;
	VectorCopy(org, s.light.origin);
	s.light.radius = 120.0;
	VectorSet(s.light.color, 0.9, 0.9, 0.9);
	s.sustain = 1000;

	cgi.AddSustainedLight(&s);

	const s_play_sample_t play = {
		.sample = cg_sample_respawn,
		.origin = { org[0], org[1], org[2] },
		.attenuation = ATTEN_IDLE,
		.flags = S_PLAY_POSITIONED
	};

	cgi.AddSample(&play);
}

/**
 * @brief A player is gasping for air under water.
 */
static void Cg_GurpEffect(cl_entity_t *ent) {
	vec3_t start, end;

	const s_play_sample_t play = {
		.sample = cgi.LoadSample("*gurp_1"),
		.entity = ent->current.number,
		.attenuation = ATTEN_NORM,
		.flags = S_PLAY_ENTITY
	};

	cgi.AddSample(&play);

	VectorCopy(ent->current.origin, start);
	start[2] += 16.0;

	VectorCopy(start, end);
	end[2] += 16.0;

	Cg_BubbleTrail(start, end, 32.0);
}

/**
 * @brief A player has drowned.
 */
static void Cg_DrownEffect(cl_entity_t *ent) {
	vec3_t start, end;

	const s_play_sample_t play = {
		.sample = cgi.LoadSample("*drown_1"),
		.entity = ent->current.number,
		.attenuation = ATTEN_NORM,
		.flags = S_PLAY_ENTITY
	};

	cgi.AddSample(&play);

	VectorCopy(ent->current.origin, start);
	start[2] += 16.0;

	VectorCopy(start, end);
	end[2] += 16.0;

	Cg_BubbleTrail(start, end, 32.0);
}

/**
 * @brief
 */
static s_sample_t *Cg_Footstep(cl_entity_t *ent) {

	const char *footsteps = "default";

	vec3_t mins, maxs;
	UnpackBounds(ent->current.bounds, mins, maxs);

	cm_trace_t tr = cgi.Trace((const vec3_t) {
		ent->current.origin[0],
		ent->current.origin[1],
		ent->current.origin[2] + mins[2]
	}, (const vec3_t) {
		ent->current.origin[0],
		ent->current.origin[1],
		ent->current.origin[2] + mins[2] - PM_STEP_HEIGHT
	}, vec3_origin, vec3_origin, ent->current.number, MASK_SOLID);

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
			play.pitch = (int16_t) (Randomc() * 12.0);
			break;
		case EV_CLIENT_GURP:
			Cg_GurpEffect(ent);
			break;
		case EV_CLIENT_JUMP:
			play.sample = cgi.LoadSample(va("*jump_%d", Randomr(1, 5)));
			break;
		case EV_CLIENT_LAND:
			play.sample = cgi.LoadSample("*land_1");
			break;
		case EV_CLIENT_STEP: {
			const vec_t height = ent->current.origin[2] - ent->prev.origin[2];
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
