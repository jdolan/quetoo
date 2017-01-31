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

			if (!(p = Cg_AllocParticle(PARTICLE_BUBBLE, cg_particles_bubble))) {
				return;
			}

			p->lifetime = 1000 - (Randomf() * 100);
			p->effects |= PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

			cgi.ColorFromPalette(6 + (Random() & 3), p->color_start);
			p->color_start[3] = 1.0;

			Vector4Copy(p->color_start, p->color_end);
			p->color_end[3] = 0;

			p->scale_start = 3.0;
			p->scale_end = p->scale_start - (0.4 + Randomf() * 0.2);

			VectorScale(forward, 2.0, p->vel);

			for (int32_t j = 0; j < 3; j++) {
				p->part.org[j] = pos[j] + Randomc() * 2.0;
				p->vel[j] += Randomc() * 5.0;
			}

			p->vel[2] += 6.0;
			p->accel[2] = 10.0;

			ent->timestamp = cgi.client->unclamped_time + 3000;
		}
	} else if (cgi.view->weather & WEATHER_RAIN || cgi.view->weather & WEATHER_SNOW) {

		if (!(p = Cg_AllocParticle(PARTICLE_ROLL, cg_particles_steam))) {
			return;
		}

		p->lifetime = 4000 - (Randomf() * 100);
		p->effects |= PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

		cgi.ColorFromPalette(6 + (Random() & 7), p->color_start);
		p->color_start[3] = 0.7;

		Vector4Copy(p->color_start, p->color_end);
		p->color_end[3] = 0;

		p->scale_start = 1.5;
		p->scale_end = 8.0;

		p->part.roll = Randomc() * 20.0;

		VectorCopy(pos, p->part.org);

		VectorScale(forward, 5.0, p->vel);

		for (int32_t i = 0; i < 3; i++) {
			p->vel[i] += 2.0 * Randomc();
		}

		ent->timestamp = cgi.client->unclamped_time + 3000;
	}
}

#define SMOKE_DENSITY 4.0

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

		if (!(p = Cg_AllocParticle(PARTICLE_ROLL, cg_particles_smoke))) {
			return;
		}

		const vec_t c = 0.3 - Randomf() * 0.2;

		p->lifetime = 900 + Randomf() * 200;
		p->effects |= PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

		Vector4Set(p->color_start, c, c, c, 0.2);
		Vector4Set(p->color_end, c, c, c, 0.0);

		p->scale_start = 2.0;
		p->scale_end = 5.0 + (Randomf() * 5.0);

		p->part.roll = Randomc() * 240.0;

		VectorScale(vec, len, p->vel);

		for (int32_t j = 0; j < 3; j++) {
			p->part.org[j] = move[j];
			p->vel[j] += Randomc();
		}

		VectorScale(vec, -len, p->accel);
		p->accel[2] += 10.0 + (Randomc() * 2.0);
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

	if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, cg_particles_flame))) {
		return;
	}

	p->lifetime = 1500;
	p->effects |= PARTICLE_EFFECT_COLOR;

	cgi.ColorFromPalette(220 + (Random() & 7), p->color_start);
	p->color_start[3] = 0.75;

	VectorCopy(p->color_start, p->color_end);
	p->color_end[3] = 0.0;

	p->part.scale = 10.0 + Randomc();

	for (j = 0; j < 3; j++) {
		p->part.org[j] = end[j];
		p->vel[j] = Randomc() * 1.5;
	}

	p->accel[2] = 15.0;

	// make static flames rise
	if (ent) {
		if (VectorCompare(ent->current.origin, ent->prev.origin)) {
			p->lifetime /= 0.65;
			p->accel[2] = 20.0;
		}
	}
}

/**
 * @brief
 */
void Cg_SteamTrail(cl_entity_t *ent, const vec3_t org, const vec3_t vel) {
	cg_particle_t *p;
	int32_t i;

	vec3_t end;
	VectorAdd(org, vel, end);

	if (cgi.PointContents(org) & MASK_LIQUID) {
		Cg_BubbleTrail(org, end, 10.0);
		return;
	}

	if (!(p = Cg_AllocParticle(PARTICLE_ROLL, cg_particles_steam))) {
		return;
	}

	p->lifetime = 4500 / (5.0 + Randomf() * 0.5);
	p->effects |= PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

	cgi.ColorFromPalette(6 + (Random() & 7), p->color_start);
	p->color_start[3] = 0.3;

	VectorCopy(p->color_start, p->color_end);
	p->color_end[3] = 0.0;

	p->scale_start = 8.0;
	p->scale_end = 20.0;

	p->part.roll = Randomc() * 100.0;

	VectorCopy(org, p->part.org);
	VectorCopy(vel, p->vel);

	for (i = 0; i < 3; i++) {
		p->vel[i] += 2.0 * Randomc();
	}
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

		if (!(p = Cg_AllocParticle(PARTICLE_BUBBLE, cg_particles_bubble))) {
			return;
		}

		p->lifetime = 1000 - (Randomf() * 100);
		p->effects |= PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

		cgi.ColorFromPalette(6 + (Random() & 3), p->color_start);

		Vector4Copy(p->color_start, p->color_end);
		p->color_end[3] = 0;

		p->scale_start = 1.5;
		p->scale_end = p->scale_start - (0.6 + Randomf() * 0.2);

		for (int32_t j = 0; j < 3; j++) {
			p->part.org[j] = move[j] + Randomc() * 2.0;
			p->vel[j] = Randomc() * 5.0;
		}

		p->vel[2] += 6.0;
		p->accel[2] = 10.0;
	}
}

/**
 * @brief
 */
static void Cg_BlasterTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

	const color_t col = Cg_ResolveEffectColor(ent->current.client, EFFECT_COLOR_ORANGE);
	cg_particle_t *p;

	vec3_t delta;

	vec_t step = 1.0;

	if (cgi.PointContents(end) & MASK_LIQUID) {
		Cg_BubbleTrail(start, end, 12.0);
		step = 2.0;
	}

	vec_t d = 0.0;

	VectorSubtract(end, start, delta);
	const vec_t dist = VectorNormalize(delta);

	while (d < dist) {
		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, NULL))) {
			break;
		}

		p->effects |= PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;
		p->lifetime = 200 + Randomf() * 100;

		// TODO: color modulation
		//cgi.ColorFromPalette(col + (Random() & 5), p->color_start);
		ColorToVec4(col, p->color_start);
		VectorCopy(p->color_start, p->color_end);
		p->color_start[3] = 0.66;
		p->color_end[3] = 0.0;

		p->scale_start = 3.0;
		p->scale_end = 1.5;

		VectorMA(start, d, delta, p->part.org);
		VectorScale(delta, -24.0, p->vel);
		VectorScale(delta, 24.0, p->accel);

		d += step;
	}

	vec3_t color;
	ColorToVec3(col, color);

	VectorScale(color, 3.0, color);

	for (int32_t i = 0; i < 3; i++) {
		if (color[i] > 1.0) {
			color[i] = 1.0;
		}
	}

	if ((p = Cg_AllocParticle(PARTICLE_CORONA, NULL))) {
		VectorCopy(color, p->part.color);
		VectorCopy(end, p->part.org);

		p->lifetime = PARTICLE_IMMEDIATE;
		p->part.scale = CORONA_SCALE(3.0, 0.125);
	}

	r_light_t l;
	VectorCopy(end, l.origin);
	l.origin[2] += 4.0;
	l.radius = 100.0;
	VectorCopy(color, l.color);

	cgi.AddLight(&l);
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

		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, cg_particles_flame))) {
			break;
		}

		p->lifetime = 250 + Randomf() * 200;
		p->effects |= PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

		// TODO: color modulation
		//cgi.ColorFromPalette(EFFECT_COLOR_ORANGE + (Random() & 5), p->color_start);
		ColorToVec4(EFFECT_COLOR_ORANGE, p->color_start);
		VectorCopy(p->color_start, p->color_end);
		p->color_end[3] = 0.0;

		p->scale_start = 4.0;
		p->scale_end = 1.0;

		const vec_t vel_scale = 50 - Randomf() * 200;

		VectorMA(start, d, delta, p->part.org);
		VectorScale(delta, vel_scale, p->vel);

		d += 1.0;
	}

	if ((p = Cg_AllocParticle(PARTICLE_CORONA, NULL))) {
		VectorSet(p->part.color, 0.8, 0.4, 0.2);
		VectorCopy(end, p->part.org);

		p->lifetime = PARTICLE_IMMEDIATE;
		p->part.scale = CORONA_SCALE(3.0, 0.125);
	}

	r_light_t l;
	VectorCopy(end, l.origin);
	l.radius = 150.0;
	VectorSet(l.color, 0.8, 0.4, 0.2);

	cgi.AddLight(&l);
}

/**
 * @brief
 */
static void Cg_EnergyTrail(cl_entity_t *ent, vec_t radius, int32_t color) {

	const vec_t ltime = (vec_t) (cgi.client->unclamped_time + ent->current.number) / 300.0;

	for (int32_t i = 0; i < NUM_APPROXIMATE_NORMALS; i++) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, NULL))) {
			return;
		}

		vec_t angle = ltime * approximate_normals[i][0];
		const vec_t sp = sin(angle);
		const vec_t cp = cos(angle);

		angle = ltime * approximate_normals[i][1];
		const vec_t sy = sin(angle);
		const vec_t cy = cos(angle);

		vec3_t forward;
		VectorSet(forward, cp * sy, cy * sy, -sp);

		vec_t dist = sin(ltime + i) * radius;

		p->part.scale = 0.5 + (0.05 * radius);
		p->lifetime = PARTICLE_IMMEDIATE;

		for (int32_t j = 0; j < 3; j++) { // project the origin outward and forward
			p->part.org[j] = ent->origin[j] + (approximate_normals[i][j] * dist) + forward[j] * radius;
		}

		vec3_t delta;
		VectorSubtract(p->part.org, ent->origin, delta);
		dist = VectorLength(delta) / (3.0 * radius);

		cgi.ColorFromPalette(color + dist * 7.0, p->part.color);

		VectorScale(delta, 100.0, p->accel);
	}

	if (cgi.PointContents(ent->origin) & MASK_LIQUID) {
		Cg_BubbleTrail(ent->prev.origin, ent->origin, radius / 4.0);
	}
}

/**
 * @brief
 */
static void Cg_HyperblasterTrail(cl_entity_t *ent) {
	r_light_t l;
	cg_particle_t *p;

	Cg_EnergyTrail(ent, 6.0, 107);

	if ((p = Cg_AllocParticle(PARTICLE_CORONA, NULL))) {
		VectorSet(p->part.color, 0.4, 0.7, 1.0);
		VectorCopy(ent->origin, p->part.org);

		p->lifetime = PARTICLE_IMMEDIATE;
		p->part.scale = CORONA_SCALE(10.0, 0.15);
	}

	VectorCopy(ent->origin, l.origin);
	l.radius = 100.0;
	VectorSet(l.color, 0.4, 0.7, 1.0);

	cgi.AddLight(&l);
}

/**
 * @brief
 */
static void Cg_LightningTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {
	r_light_t l;
	vec3_t dir, delta, pos, vel;
	vec_t dist, offset;
	int32_t i;

	VectorCopy(start, l.origin);
	l.radius = 90.0 + 10.0 * Randomc();
	VectorSet(l.color, 0.6, 0.6, 1.0);
	cgi.AddLight(&l);

	VectorSubtract(start, end, dir);
	dist = VectorNormalize(dir);

	VectorScale(dir, -48.0, delta);
	VectorCopy(start, pos);

	VectorSubtract(ent->current.origin, ent->prev.origin, vel);

	i = 0;
	while (dist > 0.0) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle(PARTICLE_BEAM, cg_particles_lightning))) {
			return;
		}

		p->lifetime = PARTICLE_IMMEDIATE;

		cgi.ColorFromPalette(12 + (Random() & 3), p->part.color);

		p->part.scale = 8.0;
		p->part.scroll_s = -8.0;

		VectorCopy(pos, p->part.org);

		if (dist <= 48.0) {
			VectorScale(dir, -dist, delta);
			offset = 0.0;
		} else {
			offset = fabs(2.0 * sin(dist));
		}

		VectorAdd(pos, delta, pos);
		pos[2] += (++i & 1) ? offset : -offset;

		VectorCopy(pos, p->part.end);
		VectorCopy(vel, p->vel);

		dist -= 48.0;

		if (dist > 12.0) {
			VectorCopy(p->part.end, l.origin);
			l.radius = 90.0 + 10.0 * Randomc();
			cgi.AddLight(&l);
		}
	}

	VectorMA(end, 12.0, dir, l.origin);
	l.radius = 90.0 + 10.0 * Randomc();
	cgi.AddLight(&l);

	if (ent->timestamp < cgi.client->unclamped_time) {

		cgi.AddStain(&(const r_stain_t) {
			.origin = { end[0], end[1], end[2] },
			 .color = { 0.0, 0.0, 0.0, 0.33 },
			  .radius = 2.0
		});

		ent->timestamp = cgi.client->unclamped_time + 64;
	}

	if (ent->current.animation1 != LIGHTNING_SOLID_HIT) {
		return;
	}

	cg_particle_t *p;

	if ((p = Cg_AllocParticle(PARTICLE_CORONA, NULL))) {
		// TODO: color modulation
		//cgi.ColorFromPalette(EFFECT_COLOR_BLUE + (Random() & 3), p->part.color);
		ColorToVec4(EFFECT_COLOR_BLUE, p->part.color);
		VectorCopy(end, p->part.org);

		p->lifetime = PARTICLE_IMMEDIATE;
		p->part.scale = CORONA_SCALE(24.0, 0.25);
	}

	// lightning zaps!
	for (i = 2 + Randomf() * 3; i >= 0; i--) {

		vec3_t forward, right, up;
		vec3_t zap_start, zap_end;
		const int32_t num_zaps = 3 + Randomf() * 3;

		AngleVectors(ent->angles, forward, right, up);

		VectorCopy(end, zap_start);
		VectorMA(zap_start, Randomc() * 8.0, forward, zap_start);
		VectorMA(zap_start, Randomc() * 8.0, right, zap_start);

		for (int32_t k = 0; k < num_zaps; k++) {

			VectorMA(zap_start, 1.0 + Randomf() * 1.0, forward, zap_end);

			vec_t angle_change;

			if (k == 0) {
				angle_change = 10.0;
			} else {
				angle_change = 6.0;
			}

			VectorMA(zap_end, Randomc() * angle_change, right, zap_end);
			VectorMA(zap_end, Randomc() * angle_change, up, zap_end);

			// zap!
			if (!(p = Cg_AllocParticle(PARTICLE_BEAM, cg_particles_lightning))) {
				return;
			}

			p->lifetime = PARTICLE_IMMEDIATE;

			cgi.ColorFromPalette(12 + (Random() & 3), p->part.color);

			p->part.scale = 2.0;
			p->part.scroll_s = -2.0;

			VectorCopy(zap_start, p->part.org);
			VectorCopy(zap_end, p->part.end);

			vec3_t zap_dir;
			VectorSubtract(zap_end, zap_start, zap_dir);
			VectorNormalize(zap_dir);
			VectorAngles(zap_dir, zap_dir);
			AngleVectors(zap_dir, forward, right, up);

			VectorCopy(zap_end, zap_start);
		}
	}
}

/**
 * @brief
 */
static void Cg_HookTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

	cg_particle_t *p;

	if (!(p = Cg_AllocParticle(PARTICLE_BEAM, cg_particles_rope))) {
		return;
	}

	p->lifetime = PARTICLE_IMMEDIATE;

	ColorToVec4(Cg_ResolveEffectColor(ent->current.client, EFFECT_COLOR_GREEN), p->part.color);

	p->part.blend = GL_ONE_MINUS_SRC_ALPHA;
	p->part.scale = 2.0;
	//p->part.scroll_s = -1.0;
	p->part.flags |= PARTICLE_FLAG_REPEAT;
	p->part.repeat_scale = 0.20;

	VectorCopy(start, p->part.org);

	// push the hook tip back a little bit so it connects to the model
	vec3_t forward;
	AngleVectors(ent->angles, forward, NULL, NULL);

	VectorMA(end, -3.0, forward, p->part.end);
}

/**
 * @brief
 */
static void Cg_BfgTrail(cl_entity_t *ent) {

	Cg_EnergyTrail(ent, 48.0, 206);

	const vec_t mod = sin(cgi.client->unclamped_time >> 5);

	cg_particle_t *p;
	if ((p = Cg_AllocParticle(PARTICLE_ROLL, cg_particles_explosion))) {

		cgi.ColorFromPalette(206, p->color_start);

		p->effects |= PARTICLE_EFFECT_COLOR;
		p->lifetime = 100;

		VectorCopy(p->color_start, p->color_end);
		p->color_end[3] = 0.0;

		p->part.scale = 48.0 + 12.0 * mod;

		p->part.roll = Randomc() * 100.0;

		VectorCopy(ent->origin, p->part.org);
	}

	r_light_t l;
	VectorCopy(ent->origin, l.origin);
	l.radius = 160.0 + 48.0 * mod;
	VectorSet(l.color, 0.4, 1.0, 0.4);

	cgi.AddLight(&l);
}

/**
 * @brief
 */
static void Cg_TeleporterTrail(cl_entity_t *ent, const color_t color, const _Bool sound) {

	if (ent->timestamp > cgi.client->unclamped_time) {
		return;
	}

	if (sound) {
		cgi.AddSample(&(const s_play_sample_t) {
			.sample = cg_sample_respawn,
			 .entity = ent->current.number,
			  .attenuation = ATTEN_IDLE,
			   .flags = S_PLAY_ENTITY
		});
	}

	ent->timestamp = cgi.client->unclamped_time + 1000 + (500 * Randomf());

	for (int32_t i = 0; i < 4; i++) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle(PARTICLE_SPLASH, cg_particles_teleporter))) {
			return;
		}

		p->effects = PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;
		p->lifetime = 450;

		ColorToVec4(color, p->color_start);
		VectorCopy(p->color_start, p->color_end);
		p->color_end[3] = 0.0;

		p->scale_start = 16.0;
		p->scale_end = p->scale_start * 2.0;

		VectorCopy(ent->origin, p->part.org);
		p->part.org[2] -= (6.0 * i);
		p->vel[2] = 120.0;
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
	uint32_t added = 0;

	while (dist > 0.0) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, cg_particles_blood))) {
			break;
		}

		VectorMA(end, dist, move, p->part.org);

		p->lifetime = 1000 + Randomf() * 500;
		p->effects |= PARTICLE_EFFECT_COLOR;

		if ((added++ % 6) == 0) {
			p->special = PARTICLE_SPECIAL_BLOOD;
		}

		cgi.ColorFromPalette(232 + (Random() & 7), p->color_start);
		VectorCopy(p->color_start, p->color_end);
		p->color_end[3] = 0.0;

		p->part.scale = 3.0;

		VectorScale(move, 20.0, p->vel);

		p->accel[0] = p->accel[1] = 0.0;
		p->accel[2] = -PARTICLE_GRAVITY / 2.0;

		p->part.roll = Randomc() * 240.0;

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

	r_light_t l;
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

	cgi.AddLight(&l);
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

		// client is overridden to specify owner of the beam
		if (ent->current.client == cgi.client->client_num && !cgi.client->third_person) {

			// we own this beam (lightning, grapple, etc..)
			// project start position below the view origin

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
		}

		VectorCopy(ent->termination, end);
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
			Cg_TeleporterTrail(ent, ColorFromRGB(255, 255, 211), true);
			break;
		case TRAIL_GOOD_SPAWN:
			Cg_TeleporterTrail(ent, EFFECT_COLOR_BLUE, false);
			break;
		case TRAIL_EVIL_SPAWN:
			Cg_TeleporterTrail(ent, EFFECT_COLOR_RED, false);
			break;
		case TRAIL_NEUTRAL_SPAWN:
			Cg_TeleporterTrail(ent, EFFECT_COLOR_WHITE, false);
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
