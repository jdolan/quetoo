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

/**
 * @brief
 */
void Cg_BreathTrail(cl_entity_t *ent) {

	if (ent->animation1.animation < ANIM_TORSO_GESTURE) { // death animations
		return;
	}

	if (cgi.client->unclamped_time < ent->timestamp) {
		return;
	}

	cg_particle_t *p;

	vec3_t pos;
	VectorCopy(ent->origin, pos);

	if (Cg_IsDucking(ent)) {
		pos[2] += 18.0;
	} else {
		pos[2] += 30.0;
	}

	vec3_t forward;
	AngleVectors(ent->angles, forward, NULL, NULL);

	VectorMA(pos, 8.0, forward, pos);

	const int32_t contents = cgi.PointContents(pos);

	if (contents & MASK_LIQUID) {
		if ((contents & MASK_LIQUID) == CONTENTS_WATER) {

			if (!(p = Cg_AllocParticle())) {
				return;
			}

			VectorScale(forward, 2.0, p->velocity);

			for (int32_t j = 0; j < 3; j++) {
				p->origin[j] = pos[j] + Randomc() * 2.0;
				p->velocity[j] += Randomc() * 5.0;
			}

			p->velocity[2] += 6.0;
			p->acceleration[2] = 10.0;


			p->lifetime = 1000 - (Randomf() * 100);

			cgi.ColorFromPalette(6 + (Randomr(0, 4)), &p->color);
			p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

			p->size = 3.0;
			p->delta_size = -0.1;

			ent->timestamp = cgi.client->unclamped_time + 3000;
		}
	} else if (cgi.view->weather & WEATHER_RAIN || cgi.view->weather & WEATHER_SNOW) {

		if (!(p = Cg_AllocParticle())) {
			return;
		}

		p->lifetime = 4000 - (Randomf() * 100);

		cgi.ColorFromPalette(6 + (Randomr(0, 8)), &p->color);
		p->color.a = 200;

		p->delta_color.a = p->color.a * -p->lifetime / PARTICLE_FRAME;

		p->size = 1.5;
		p->delta_size = 0.1;

		VectorCopy(pos, p->origin);

		VectorScale(forward, 5.0, p->velocity);

		for (int32_t i = 0; i < 3; i++) {
			p->velocity[i] += 2.0 * Randomc();
		}

		ent->timestamp = cgi.client->unclamped_time + 3000;
	}
}

#define SMOKE_DENSITY 1.0

/**
 * @brief
 */
void Cg_SmokeTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {
	cg_particle_t *p;

	if (ent) {
		// don't emit smoke trails for static entities (grenades on the floor)
		if (VectorCompare(ent->current.origin, ent->prev.origin)) {
			return;
		}
	}

	if (cgi.PointContents(end) & MASK_LIQUID) {
		Cg_BubbleTrail(start, end, 24.0);
		return;
	}

	vec3_t vec, move;

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	const vec_t len = VectorNormalize(vec);

	VectorScale(vec, SMOKE_DENSITY, vec);
	VectorSubtract(move, vec, move);

	for (vec_t i = 0.0; i < len; i += SMOKE_DENSITY) {
		VectorAdd(move, vec, move);

		if (!(p = Cg_AllocParticle())) {
			return;
		}

		VectorCopy(move, p->origin);
		VectorScale(vec, len, p->velocity);

		VectorScale(vec, -len, p->acceleration);
		p->acceleration[2] += 9.0 + (Randomc() * 6.0);

		p->lifetime = 1000 + Randomf() * 800;

		p->color.abgr = 0x40808080;
		p->delta_color.a = -2;

		p->size = 10.0;
		p->delta_size = 0.1;
	}
}

/**
 * @brief
 */
void Cg_FlameTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {
	cg_particle_t *p;
	int32_t j;

	if (cgi.PointContents(end) & MASK_LIQUID) {
		Cg_BubbleTrail(start, end, 10.0);
		return;
	}

	if (!(p = Cg_AllocParticle())) {
		return;
	}

	for (j = 0; j < 3; j++) {
		p->origin[j] = end[j];
		p->velocity[j] = Randomc() * 1.5;
	}

	p->acceleration[2] = 15.0;

	p->lifetime = 1500;

	cgi.ColorFromPalette(220 + (Randomr(0, 8)), &p->color);
	p->color.a = 200;
	p->delta_color.a = p->color.a * -p->lifetime / PARTICLE_FRAME;

	p->size = 10.0 + Randomc();

	// make static flames rise
	if (ent) {
		if (VectorCompare(ent->current.origin, ent->prev.origin)) {
			p->lifetime /= 0.65;
			p->acceleration[2] = 20.0;
		}
	}
}

/**
 * @brief
 */
void Cg_SteamTrail(cl_entity_t *ent, const vec3_t org, const vec3_t vel) {
	cg_particle_t *p;

	vec3_t end;
	VectorAdd(org, vel, end);

	if (cgi.PointContents(org) & MASK_LIQUID) {
		Cg_BubbleTrail(org, end, 10.0);
		return;
	}

	if (!(p = Cg_AllocParticle())) {
		return;
	}

	VectorCopy(org, p->origin);
	VectorCopy(vel, p->velocity);

	for (int32_t i = 0; i < 3; i++) {
		p->velocity[i] += 2.0 * Randomc();
	}

	p->lifetime = 4500 / (5.0 + Randomf() * 0.5);

	cgi.ColorFromPalette(6 + (Randomr(0, 8)), &p->color);
	p->color.a = 50;

	p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

	p->size = 8.0;
	p->delta_size = 20.0 / p->lifetime / PARTICLE_FRAME;
}

/**
 * @brief
 */
void Cg_BubbleTrail(const vec3_t start, const vec3_t end, vec_t density) {
	vec3_t vec, move;

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	const vec_t len = VectorNormalize(vec);

	const vec_t delta = 16.0 / density;
	VectorScale(vec, delta, vec);
	VectorSubtract(move, vec, move);

	for (vec_t i = 0.0; i < len; i += delta) {
		VectorAdd(move, vec, move);

		if (!(cgi.PointContents(move) & MASK_LIQUID)) {
			continue;
		}

		cg_particle_t *p;

		if (!(p = Cg_AllocParticle())) {
			return;
		}

		for (int32_t j = 0; j < 3; j++) {
			p->origin[j] = move[j] + Randomc() * 2.0;
			p->velocity[j] = Randomc() * 5.0;
		}

		p->velocity[2] += 6.0;
		p->acceleration[2] = 10.0;

		p->lifetime = 1000 - (Randomf() * 100);

		cgi.ColorFromPalette(6 + (Randomr(0, 4)), &p->color);

		p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

		p->size = 1.5;
		p->delta_size = p->size - (0.6 + Randomf() * 0.2) * -p->lifetime / PARTICLE_FRAME;
	}
}

/**
 * @brief
 */
static void Cg_BlasterTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

	const color_t color = Cg_ResolveEffectColor(ent->current.client, EFFECT_COLOR_ORANGE);
	cg_particle_t *p;

	vec3_t delta;

	vec_t step = 4.0;

	if (cgi.PointContents(end) & MASK_LIQUID) {
		Cg_BubbleTrail(start, end, 12.0);
		step = 6.0;
	}

	vec_t d = 0.0;

	VectorSubtract(end, start, delta);
	const vec_t dist = VectorNormalize(delta);

	while (d < dist) {
		if (!(p = Cg_AllocParticle())) {
			break;
		}

		p->lifetime = 250 + Randomf() * 10;

		VectorMA(start, d, delta, p->origin);

		p->color = color;
		p->color.a = 200;
		p->delta_color.a = -15;
		p->size = 4.0;

		d += step;
	}

	cg_light_t l;
	VectorCopy(end, l.origin);
	l.origin[2] += 4.0;
	l.radius = 100.0;
	ColorToVec4(color, l.color);

	Cg_AddLight(&l);
}

/**
 * @brief
 */
static void Cg_GrenadeTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

	Cg_SmokeTrail(ent, start, end);
}

/**
 * @brief
 */
static void Cg_RocketTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

	cg_particle_t *p;

	Cg_SmokeTrail(ent, start, end);

	vec3_t delta;

	VectorSubtract(end, start, delta);
	const vec_t dist = VectorNormalize(delta);

	vec_t d = 0.0;
	while (d < dist) {

		// make larger outer orange flame
		if (!(p = Cg_AllocParticle())) {
			break;
		}

		VectorMA(start, d, delta, p->origin);

		VectorScale(delta, -150.0 + Randomf() * 50.0, p->velocity);

		p->lifetime = 75 + Randomf() * 75;

		p->color.abgr = 0xff44aaff;
		p->delta_color.a = -10;

		p->size = 3.0;
		p->delta_size = p->size * -p->lifetime / PARTICLE_FRAME;

		d += 1.0;
	}

	cg_light_t l;
	VectorCopy(end, l.origin);
	l.radius = 150.0;
	VectorSet(l.color, 0.8, 0.4, 0.2);

	Cg_AddLight(&l);
}

/**
 * @brief
 */
static void Cg_EnergyTrail(cl_entity_t *ent, vec_t radius, int32_t color) {

	const vec_t ltime = (vec_t) (cgi.client->unclamped_time + ent->current.number) / 300.0;

	const int32_t step = (cg_particle_quality->integer ? 1 : 3);

	for (int32_t i = 0; i < NUM_APPROXIMATE_NORMALS; i += step) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		vec_t angle = ltime * approximate_normals[i][0];
		const vec_t sp = sinf(angle);
		const vec_t cp = cosf(angle);

		angle = ltime * approximate_normals[i][1];
		const vec_t sy = sinf(angle);
		const vec_t cy = cosf(angle);

		vec3_t forward;
		VectorSet(forward, cp * sy, cy * sy, -sp);

		vec_t dist = sinf(ltime + i) * radius;

		for (int32_t j = 0; j < 3; j++) { // project the origin outward and forward
			p->origin[j] = ent->origin[j] + (approximate_normals[i][j] * dist) + forward[j] * radius;
		}

		p->size = 0.5 + (0.05 * radius);

		vec3_t delta;
		VectorSubtract(p->origin, ent->origin, delta);
		dist = VectorLength(delta) / (3.0 * radius);

		cgi.ColorFromPalette(color + dist * 7.0, &p->color);
	}

	if (cgi.PointContents(ent->origin) & MASK_LIQUID) {
		Cg_BubbleTrail(ent->prev.origin, ent->origin, radius / 4.0);
	}
}

/**
 * @brief
 */
static void Cg_HyperblasterTrail(cl_entity_t *ent) {

	Cg_EnergyTrail(ent, 6.0, 107);

	cg_light_t l;
	VectorCopy(ent->origin, l.origin);
	l.radius = 100.0;
	VectorSet(l.color, 0.4, 0.7, 1.0);

	Cg_AddLight(&l);
}

/**
 * @brief
 */
static void Cg_LightningTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {
	vec3_t dir, delta, pos, vel;
	vec_t dist;
	int32_t i;

	cg_light_t l;
	VectorCopy(start, l.origin);
	l.radius = 90.0 + 10.0 * Randomc();
	VectorSet(l.color, 0.6, 0.6, 1.0);
	Cg_AddLight(&l);

	VectorSubtract(start, end, dir);
	const vec_t dist_total = dist = VectorNormalize(dir);

	VectorScale(dir, -48.0, delta);
	VectorCopy(start, pos);

	VectorSubtract(ent->current.origin, ent->prev.origin, vel);

	i = 0;
	while (dist > 0.0) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		cgi.ColorFromPalette(12 + (Randomr(0, 4)), &p->color);

		p->size = 8.0;

		VectorCopy(pos, p->origin);

		if (dist <= 48.0) {
			VectorScale(dir, -dist, delta);
		}

		VectorAdd(pos, delta, pos);
		VectorCopy(vel, p->velocity);

		dist -= 48.0;

		if (dist > 12.0) {
			VectorCopy(p->origin, l.origin);
			l.radius = 90.0 + 10.0 * Randomc();
			Cg_AddLight(&l);
		}
	}

	VectorMA(end, 12.0, dir, l.origin);
	l.radius = 90.0 + 10.0 * Randomc();
	Cg_AddLight(&l);

	if (ent->current.animation1 != LIGHTNING_SOLID_HIT) {
		return;
	}

	cg_particle_t *p;

	if (ent->timestamp < cgi.client->unclamped_time) {

		vec3_t real_end;
		VectorMA(start, -(dist_total + 32.0), dir, real_end);
		cm_trace_t tr = cgi.Trace(start, real_end, NULL, NULL, 0, CONTENTS_SOLID);

		if (tr.surface) {

			VectorMA(tr.end, 1.0, tr.plane.normal, pos);

			cgi.AddStain(&(const r_stain_t) {
				.origin = { pos[0], pos[1], pos[2] },
				.radius = 8.0 + Randomf() * 4.0,
				.media = cg_stain_lightning,
				.color = { 0.0, 0.0, 0.0, 0.33 },
			});

			VectorMA(tr.end, 2.0, tr.plane.normal, pos);

			if ((cgi.PointContents(pos) & MASK_LIQUID) == 0) {
				for (i = 0; i < 6; i++) {

					if (!(p = Cg_AllocParticle())) {
						break;
					}

					VectorCopy(pos, p->origin);

					p->velocity[0] = dir[0] * -200.0 + Randomc() * 100.0;
					p->velocity[1] = dir[1] * -200.0 + Randomc() * 100.0;
					p->velocity[2] = dir[2] * -200.0 + Randomf() * 100.0;

					p->acceleration[2] = -PARTICLE_GRAVITY * 3.0;

					p->lifetime = 600 + Randomf() * 300;

					if (i % 3 == 0) {
						p->color.abgr = 0xffffffff;
					} else {
						p->color.abgr = 0xffffaaaa;
					}

					p->bounce = 1.15;

					p->size = 1.3 + Randomf() * 0.6;
				}
			}
		}

		ent->timestamp = cgi.client->unclamped_time + 25; // 40hz
	}
}

/**
 * @brief
 */
static void Cg_HookTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

	cg_particle_t *p;

	if ((p = Cg_AllocParticle())) {

		VectorCopy(start, p->origin);

		p->color = Cg_ResolveEffectColor(ent->current.client, EFFECT_COLOR_GREEN);

		p->size = 0.35;
	}
}

/**
 * @brief
 */
static void Cg_BfgTrail(cl_entity_t *ent) {

	Cg_EnergyTrail(ent, 48.0, 206);

	const vec_t mod = sinf(cgi.client->unclamped_time >> 5);

	cg_light_t l;
	VectorCopy(ent->origin, l.origin);
	l.radius = 160.0 + 48.0 * mod;
	VectorSet(l.color, 0.4, 1.0, 0.4);

	Cg_AddLight(&l);
}

/**
 * @brief
 */
static void Cg_TeleporterTrail(cl_entity_t *ent, const color_t color) {

//	cgi.AddSample(&(const s_play_sample_t) {
//		.sample = cg_sample_respawn,
//			.entity = ent->current.number,
//			.attenuation = ATTEN_IDLE,
//			.flags = S_PLAY_ENTITY
//	});

	for (int32_t i = 0; i < 8; i++) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		VectorCopy(ent->origin, p->origin);

		p->velocity[2] = 120.0;

		p->acceleration[0] = 80.0 * Randomf();
		p->acceleration[1] = 80.0 * Randomf();

		p->lifetime = 450;

		p->color = color;
		p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

		p->size = 2.0;
	}
}

/**
 * @brief
 */
static void Cg_SpawnPointTrail(cl_entity_t *ent, const color_t color) {

	if (ent->timestamp > cgi.client->unclamped_time) {
		return;
	}

	ent->timestamp = cgi.client->unclamped_time + 1000;

	cg_particle_t *p;

	if ((p = Cg_AllocParticle())) {

		VectorCopy(ent->origin, p->origin);

		p->origin[2] -= 20.0;
		p->velocity[2] = 2.0;

		p->lifetime = 450;

		p->color = color;
		p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

		p->size = 16.0;
		p->delta_size = 12.0 * -p->lifetime / PARTICLE_FRAME;
	}
}

/**
 * @brief
 */
static void Cg_GibTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

	if (cgi.PointContents(end) & MASK_LIQUID) {
		Cg_BubbleTrail(start, end, 8.0);
		return;
	}

	vec3_t move;
	VectorSubtract(end, start, move);

	vec_t dist = VectorNormalize(move);
	static uint32_t added = 0;

	while (dist > 0.0) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		VectorMA(end, dist, move, p->origin);

		p->lifetime = 1000 + Randomf() * 500;

		if ((added++ % 3) == 0) {
			cgi.AddStain(&(const r_stain_t) {
				.origin = { p->origin[0], p->origin[1], p->origin[2] },
				.radius = 12.0 * Randomf() * 3.0,
				.media = cg_stain_blood,
				.color = { 0.5 + (Randomf() * 0.3), 0.0, 0.0, 0.1 + Randomf() * 0.2 },
			});
		}

		p->color.abgr = 0x80000080;
		p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

		p->size = Randomfr(3.0, 7.0);

		VectorScale(move, 20.0, p->velocity);

		p->acceleration[0] = p->acceleration[1] = 0.0;
		p->acceleration[2] = -PARTICLE_GRAVITY / 2.0;
		
		dist -= 1.5;
	}
}

/**
 * @brief
 */
static void Cg_FireballTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {
	const vec3_t color = { 0.9, 0.3, 0.1 };

	if (cgi.PointContents(end) & MASK_LIQUID) {
		return;
	}

	cg_light_t l;
	VectorCopy(end, l.origin);
	VectorCopy(color, l.color);
	l.radius = 85.0;

	if (ent->current.effects & EF_DESPAWN) {
		const vec_t decay = Clamp((cgi.client->unclamped_time - ent->timestamp) / 1000.0, 0.0, 1.0);
		l.radius *= (1.0 - decay);
	} else {
		Cg_SmokeTrail(ent, start, end);
		ent->timestamp = cgi.client->unclamped_time;
		Cg_FlameTrail(ent, start, end);
	}

	Cg_AddLight(&l);
}

/**
 * @brief Apply unique trails to entities between their previous packet origin
 * and their current interpolated origin. Beam trails are a special case: the
 * old origin field is overridden to specify the endpoint of the beam.
 */
void Cg_EntityTrail(cl_entity_t *ent) {
	const entity_state_t *s = &ent->current;

	vec3_t start, end;
	VectorCopy(ent->previous_origin, start);

	// beams have two origins, most entities have just one
	if (s->effects & EF_BEAM) {

		VectorCopy(ent->termination, end);

		// client is overridden to specify owner of the beam
		if (ent->current.client == cgi.client->client_num && !cgi.client->third_person) {

			// we own this beam (lightning, grapple, etc..)
			// project start & end points based on our current view origin
			vec_t dist = VectorDistance(start, end);

			VectorMA(cgi.view->origin, 8.0, cgi.view->forward, start);

			const float hand_scale = (ent->current.trail == TRAIL_HOOK ? -1.0 : 1.0);

			switch (cg_hand->integer) {
				case HAND_LEFT:
					VectorMA(start, -5.5 * hand_scale, cgi.view->right, start);
					break;
				case HAND_RIGHT:
					VectorMA(start, 5.5 * hand_scale, cgi.view->right, start);
					break;
				default:
					break;
			}

			VectorMA(start, -8.0, cgi.view->up, start);

			// lightning always uses predicted end points
			if (s->trail == TRAIL_LIGHTNING) {
				VectorMA(start, dist, cgi.view->forward, end);
			}
		}
	} else {
		VectorCopy(ent->origin, end);
	}

	// add the trail

	switch (s->trail) {
		case TRAIL_BLASTER:
			Cg_BlasterTrail(ent, start, end);
			break;
		case TRAIL_GRENADE:
			Cg_GrenadeTrail(ent, start, end);
			break;
		case TRAIL_ROCKET:
			Cg_RocketTrail(ent, start, end);
			break;
		case TRAIL_HYPERBLASTER:
			Cg_HyperblasterTrail(ent);
			break;
		case TRAIL_LIGHTNING:
			Cg_LightningTrail(ent, start, end);
			break;
		case TRAIL_HOOK:
			Cg_HookTrail(ent, start, end);
			break;
		case TRAIL_BFG:
			Cg_BfgTrail(ent);
			break;
		case TRAIL_TELEPORTER:
			Cg_TeleporterTrail(ent, ColorFromRGB(255, 255, 211));
			break;
		case TRAIL_PLAYER_SPAWN:
			Cg_SpawnPointTrail(ent, ent->current.client >= MAX_TEAMS ? EFFECT_COLOR_WHITE : cg_team_info[ent->current.client].color);
			break;
		case TRAIL_GIB:
			Cg_GibTrail(ent, start, end);
			break;
		case TRAIL_FIREBALL:
			Cg_FireballTrail(ent, start, end);
			break;
		default:
			break;
	}
}
