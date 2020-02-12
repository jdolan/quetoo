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
	pos = ent->origin;

	if (Cg_IsDucking(ent)) {
		pos.z += 18.0;
	} else {
		pos.z += 30.0;
	}

	vec3_t forward;
	vec3_vectors(ent->angles, &forward, NULL, NULL);

	pos = vec3_add(pos, vec3_scale(forward, 8.0));

	const int32_t contents = cgi.PointContents(pos);

	if (contents & MASK_LIQUID) {
		if ((contents & MASK_LIQUID) == CONTENTS_WATER) {

			if (!(p = Cg_AllocParticle())) {
				return;
			}

			p->origin = vec3_add(pos, vec3_random_range(-2.f, 2.f));

			p->velocity = vec3_add(vec3_scale(forward, 2.0), vec3_random_range(-5.f, 5.f));
			p->velocity.z += 6.0;
			p->acceleration.z = 10.0;

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

		p->origin = pos;

		p->velocity = vec3_add(vec3_scale(forward, 5.0), vec3_random_range(-2.f, 2.f));

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
		if (vec3_equal(ent->current.origin, ent->prev.origin)) {
			return;
		}
	}

	if (cgi.PointContents(end) & MASK_LIQUID) {
		Cg_BubbleTrail(start, end, 24.0);
		return;
	}

	vec3_t vec, move;

	move = start;
	const float len = vec3_distance_dir(end, start, &vec);

	vec = vec3_scale(vec, SMOKE_DENSITY);
	move = vec3_subtract(move, vec);

	for (float i = 0.0; i < len; i += SMOKE_DENSITY) {
		move = vec3_add(move, vec);

		if (!(p = Cg_AllocParticle())) {
			return;
		}

		p->origin = move;
		p->velocity = vec3_scale(vec, len);

		p->acceleration = vec3_scale(vec, -len);
		p->acceleration.z += 9.0 + (Randomc() * 6.0);

		p->lifetime = 1000 + Randomf() * 800;

		p->color = color4bv(0x80808040);
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

	if (cgi.PointContents(end) & MASK_LIQUID) {
		Cg_BubbleTrail(start, end, 10.0);
		return;
	}

	if (!(p = Cg_AllocParticle())) {
		return;
	}

	p->origin = end;
	p->velocity = vec3_random_range(-1.5, 1.5);

	p->acceleration.z = 15.0;

	p->lifetime = 1500;

	cgi.ColorFromPalette(220 + (Randomr(0, 8)), &p->color);
	p->color.a = 200;
	p->delta_color.a = p->color.a * -p->lifetime / PARTICLE_FRAME;

	p->size = 10.0 + Randomc();

	// make static flames rise
	if (ent) {
		if (vec3_equal(ent->current.origin, ent->prev.origin)) {
			p->lifetime /= 0.65;
			p->acceleration.z = 20.0;
		}
	}
}

/**
 * @brief
 */
void Cg_SteamTrail(cl_entity_t *ent, const vec3_t org, const vec3_t vel) {
	cg_particle_t *p;

	vec3_t end;
	end = vec3_add(org, vel);

	if (cgi.PointContents(org) & MASK_LIQUID) {
		Cg_BubbleTrail(org, end, 10.0);
		return;
	}

	if (!(p = Cg_AllocParticle())) {
		return;
	}

	p->origin = org;
	p->velocity = vec3_add(vel, vec3_random_range(-2.f, 2.f));

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
void Cg_BubbleTrail(const vec3_t start, const vec3_t end, float density) {
	vec3_t vec, move;

	move = start;
	vec = vec3_subtract(end, start);

	float len;
	vec = vec3_normalize_length(vec, &len);

	const float delta = 16.0 / density;
	vec = vec3_scale(vec, delta);
	move = vec3_subtract(move, vec);

	for (float i = 0.0; i < len; i += delta) {
		move = vec3_add(move, vec);

		if (!(cgi.PointContents(move) & MASK_LIQUID)) {
			continue;
		}

		cg_particle_t *p;

		if (!(p = Cg_AllocParticle())) {
			return;
		}

		p->origin = vec3_add(move, vec3_random_range(-2.f, 2.f));
		p->velocity = vec3_random_range(-5.f, 5.f);
		p->velocity.z += 6.0;
		p->acceleration.z = 10.0;

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

	float step = 4.0;

	if (cgi.PointContents(end) & MASK_LIQUID) {
		Cg_BubbleTrail(start, end, 12.0);
		step = 6.0;
	}

	float d = 0.0;

	float dist;
	vec3_t delta = vec3_subtract(end, start);
	delta = vec3_normalize_length(delta, &dist);

	while (d < dist) {
		if (!(p = Cg_AllocParticle())) {
			break;
		}

		p->lifetime = 250 + Randomf() * 10;

		p->origin = vec3_add(start, vec3_scale(delta, d));

		p->color = color;
		p->color.a = 200;
		p->delta_color.a = -15;
		p->size = 4.0;

		d += step;
	}

	cg_light_t l;
	l.origin = end;
	l.radius = 100.0;
	l.color = color_to_vec3(color);

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

	float dist;
	vec3_t delta = vec3_subtract(end, start);
	delta = vec3_normalize_length(delta, &dist);

	float d = 0.0;
	while (d < dist) {

		// make larger outer orange flame
		if (!(p = Cg_AllocParticle())) {
			break;
		}

		p->origin = vec3_add(start, vec3_scale(delta, d));

		p->velocity = vec3_scale(delta, -150.0 + Randomf() * 50.0);

		p->lifetime = 75 + Randomf() * 75;

		p->color = color4bv(0xff44aaff);
		p->delta_color.a = -10;

		p->size = 3.0;
		p->delta_size = p->size * -p->lifetime / PARTICLE_FRAME;

		d += 1.0;
	}

	cg_light_t l;
	l.origin = end;
	l.radius = 150.0;
	l.color = vec3(0.8, 0.4, 0.2);

	Cg_AddLight(&l);
}

/**
 * @brief
 */
static void Cg_EnergyTrail(cl_entity_t *ent, float radius, int32_t color) {

	const float ltime = (float) (cgi.client->unclamped_time + ent->current.number) / 300.0;

	const int32_t step = (cg_particle_quality->integer ? 1 : 3);

	for (int32_t i = 0; i < NUM_APPROXIMATE_NORMALS; i += step) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		float angle = ltime * approximate_normals[i].x;
		const float sp = sinf(angle);
		const float cp = cosf(angle);

		angle = ltime * approximate_normals[i].y;
		const float sy = sinf(angle);
		const float cy = cosf(angle);

		vec3_t forward;
		forward = vec3(cp * sy, cy * sy, -sp);

		float dist = sinf(ltime + i) * radius;

		p->origin = ent->origin;
		p->origin = vec3_add(p->origin, vec3_scale(approximate_normals[i], dist));
		p->origin = vec3_add(p->origin, vec3_scale(forward, radius));

		p->size = 0.5 + (0.05 * radius);

		vec3_t delta;
		delta = vec3_subtract(p->origin, ent->origin);
		dist = vec3_length(delta) / (3.0 * radius);

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
	l.origin = ent->origin;
	l.radius = 100.0;
	l.color = vec3(0.4, 0.7, 1.0);

	Cg_AddLight(&l);
}

/**
 * @brief
 */
static void Cg_LightningTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {
	vec3_t dir, delta, pos, vel;

	cg_light_t l;
	l.origin = start;
	l.radius = 90.0 + 10.0 * Randomc();
	l.color = vec3(0.6, 0.6, 1.0);
	Cg_AddLight(&l);

	float dist_total;
	dir = vec3_subtract(start, end);
	dir = vec3_normalize_length(dir, &dist_total);

	delta = vec3_scale(dir, -48.0);
	pos = start;

	vel = vec3_subtract(ent->current.origin, ent->prev.origin);

	float dist = dist_total;
	int32_t i = 0;
	while (dist > 0.0) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		cgi.ColorFromPalette(12 + (Randomr(0, 4)), &p->color);

		p->size = 8.0;

		p->origin = pos;

		if (dist <= 48.0) {
			delta = vec3_scale(dir, -dist);
		}

		pos = vec3_add(pos, delta);
		p->velocity = vel;

		dist -= 48.0;

		if (dist > 12.0) {
			l.origin = p->origin;
			l.radius = 90.0 + 10.0 * Randomc();
			Cg_AddLight(&l);
		}
	}

	l.origin = vec3_add(end, vec3_scale(dir, 12.0));
	l.radius = 90.0 + 10.0 * Randomc();
	Cg_AddLight(&l);

	if (ent->current.animation1 != LIGHTNING_SOLID_HIT) {
		return;
	}

	cg_particle_t *p;

	if (ent->timestamp < cgi.client->unclamped_time) {

		vec3_t real_end;
		real_end = vec3_add(start, vec3_scale(dir, -(dist_total + 32.0)));
		cm_trace_t tr = cgi.Trace(start, real_end, vec3_zero(), vec3_zero(), 0, CONTENTS_SOLID);

		if (tr.surface) {

			pos = vec3_add(tr.end, vec3_scale(tr.plane.normal, 1.0));

			cgi.AddStain(&(const r_stain_t) {
				.origin = pos,
				.radius = 8.0 + Randomf() * 4.0,
				.media = cg_stain_lightning,
				.color = { 0.0, 0.0, 0.0, 0.33 },
			});

			pos = vec3_add(tr.end, vec3_scale(tr.plane.normal, 2.0));

			if ((cgi.PointContents(pos) & MASK_LIQUID) == 0) {
				for (i = 0; i < 6; i++) {

					if (!(p = Cg_AllocParticle())) {
						break;
					}

					p->origin = pos;

					p->velocity = vec3_add(dir, vec3_random_range(-300.f, 100.f));

					p->acceleration.z = -PARTICLE_GRAVITY * 3.0;

					p->lifetime = 600 + Randomf() * 300;

					if (i % 3 == 0) {
						p->color = color_white;
					} else {
						p->color = color4bv(0xaaaaffff);
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

		p->origin = start;

		p->color = Cg_ResolveEffectColor(ent->current.client, EFFECT_COLOR_GREEN);

		p->size = 0.35;
	}
}

/**
 * @brief
 */
static void Cg_BfgTrail(cl_entity_t *ent) {

	Cg_EnergyTrail(ent, 48.0, 206);

	const float mod = sinf(cgi.client->unclamped_time >> 5);

	cg_light_t l;
	l.origin = ent->origin;
	l.radius = 160.0 + 48.0 * mod;
	l.color = vec3(0.4, 1.0, 0.4);

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

		p->origin = ent->origin;

		p->velocity.z = random_range(80.f, 120.f);

		p->acceleration = vec3_random_range(-80.f, 80.f);
		p->acceleration.z = 20.f;

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

		p->origin = ent->origin;

		p->origin.z -= 20.0;
		p->velocity.z = 2.0;

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
	static uint32_t added = 0;

	if (cgi.PointContents(end) & MASK_LIQUID) {
		Cg_BubbleTrail(start, end, 8.0);
		return;
	}

	vec3_t move;
	float dist = vec3_distance_dir(end, start, &move);

	while (dist > 0.0) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		p->origin = vec3_add(end, vec3_scale(move, dist));

		p->lifetime = 1000 + Randomf() * 500;

		if ((added++ % 3) == 0) {
			cgi.AddStain(&(const r_stain_t) {
				.origin = p->origin,
				.radius = 12.0 * Randomf() * 3.0,
				.media = cg_stain_blood,
				.color = { 0.5 + (Randomf() * 0.3), 0.0, 0.0, 0.1 + Randomf() * 0.2 },
			});
		}

		p->color = color4bv(0x80000080);
		p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

		p->size = Randomfr(3.0, 7.0);

		p->velocity = vec3_scale(move, 20.0);

		p->acceleration.z = -PARTICLE_GRAVITY / 2.0;
		
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
	l.origin = end;
	l.color = color;
	l.radius = 85.0;

	if (ent->current.effects & EF_DESPAWN) {
		const float decay = clampf((cgi.client->unclamped_time - ent->timestamp) / 1000.0, 0.0, 1.0);
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
	start = ent->previous_origin;

	// beams have two origins, most entities have just one
	if (s->effects & EF_BEAM) {

		end = ent->termination;

		// client is overridden to specify owner of the beam
		if (ent->current.client == cgi.client->client_num && !cgi.client->third_person) {

			// we own this beam (lightning, grapple, etc..)
			// project start & end points based on our current view origin
			float dist = vec3_distance(start, end);

			start = vec3_add(cgi.view->origin, vec3_scale(cgi.view->forward, 8.0));

			const float hand_scale = (ent->current.trail == TRAIL_HOOK ? -1.0 : 1.0);

			switch (cg_hand->integer) {
				case HAND_LEFT:
					start = vec3_add(start, vec3_scale(cgi.view->right, -5.5 * hand_scale));
					break;
				case HAND_RIGHT:
					start = vec3_add(start, vec3_scale(cgi.view->right, 5.5 * hand_scale));
					break;
				default:
					break;
			}

			start = vec3_add(start, vec3_scale(cgi.view->up, -8.0));

			// lightning always uses predicted end points
			if (s->trail == TRAIL_LIGHTNING) {
				end = vec3_add(start, vec3_scale(cgi.view->forward, dist));
			}
		}
	} else {
		end = ent->origin;
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
			Cg_TeleporterTrail(ent, color3b(255, 255, 211));
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
