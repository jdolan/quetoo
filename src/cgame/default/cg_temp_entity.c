/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

/*
 * @brief
 */
static void Cg_BlasterEffect(const vec3_t org, const vec3_t dir, int32_t color) {
	cg_particle_t *p;
	r_sustained_light_t s;
	int32_t i, j;

	for (i = 0; i < 16; i++) {

		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, cg_particles_spark)))
			break;

		p->part.color = color + (Random() & 7);

		p->part.alpha = 1.0;
		p->alpha_vel = -1.0 / (0.7 + Randomc() * 0.1);

		p->part.scale = 3.5;
		p->scale_vel = -4.0;

		VectorCopy(org, p->part.org);

		VectorScale(dir, 150.0, p->vel);

		for (j = 0; j < 3; j++) {
			p->vel[j] += Randomc() * 50.0;
		}

		if (p->vel[2] < 100.0) // deflect up a bit
			p->vel[2] = 100.0;

		p->accel[2] -= 2.0 * PARTICLE_GRAVITY;
	}

	VectorAdd(org, dir, s.light.origin);
	s.light.radius = 80.0;
	VectorSet(s.light.color, 0.5, 0.3, 0.2);
	s.sustain = 250;

	cgi.AddSustainedLight(&s);

	cgi.PlaySample(org, 0, cg_sample_blaster_hit, ATTN_NORM);
}

/*
 * @brief
 */
static void Cg_TracerEffect(const vec3_t start, const vec3_t end) {
	cg_particle_t *p;

	if (!(p = Cg_AllocParticle(PARTICLE_BEAM, cg_particles_beam)))
		return;

	p->part.color = 14;

	p->part.alpha = 0.2;
	p->alpha_vel = -0.4;

	p->part.scale = 1.0;

	VectorCopy(start, p->part.org);
	VectorCopy(end, p->part.end);

	VectorSubtract(end, start, p->vel);
	VectorScale(p->vel, 2.0, p->vel);

	const vec_t v = VectorNormalize(p->vel);
	VectorScale(p->vel, v < 1000.0 ? v : 1000.0, p->vel);
}

/*
 * @brief
 */
static void Cg_BulletEffect(const vec3_t org, const vec3_t dir) {
	static uint32_t last_ric_time;
	cg_particle_t *p;
	r_sustained_light_t s;
	vec3_t v;
	int32_t j;

	cg_particles_t *ps = cg_particles_bullet[Random() % 3];

	if ((p = Cg_AllocParticle(PARTICLE_DECAL, ps))) {

		p->part.blend = GL_ONE_MINUS_SRC_ALPHA;
		p->part.color = 0 + (Random() & 1);

		p->part.alpha = 2.0;
		p->alpha_vel = -1.0 / (2.0 + Randomf() * 0.3);

		p->part.scale = 1.5;

		VectorScale(dir, -1.0, v);
		VectorAngles(v, p->part.dir);
		p->part.dir[ROLL] = Random() % 360;

		VectorAdd(org, dir, p->part.org);
	}

	if ((p = Cg_AllocParticle(PARTICLE_NORMAL, cg_particles_spark))) {

		p->part.color = 221 + (Random() & 7);

		p->part.alpha = 1.0;
		p->alpha_vel = -1.0 / (0.6 + Randomc() * 0.1);

		p->part.scale = 2.0;
		p->scale_vel = -3.0;

		VectorCopy(org, p->part.org);

		VectorScale(dir, 200.0, p->vel);

		for (j = 0; j < 3; j++) {
			p->vel[j] += Randomc() * 75.0;
		}

		if (p->vel[2] < 100.0) // deflect up a bit
			p->vel[2] = 100.0;

		p->accel[2] = -3.0 * PARTICLE_GRAVITY;
	}

	VectorAdd(org, dir, s.light.origin);
	s.light.radius = 20.0;
	VectorSet(s.light.color, 0.5, 0.3, 0.2);
	s.sustain = 250;

	cgi.AddSustainedLight(&s);

	if (cgi.client->time < last_ric_time)
		last_ric_time = 0;

	if (cgi.client->time - last_ric_time > 300) {
		cgi.PlaySample(org, -1, cg_sample_machinegun_hit[Random() % 3], ATTN_NORM);
		last_ric_time = cgi.client->time;
	}
}

/*
 * @brief
 */
static void Cg_BurnEffect(const vec3_t org, const vec3_t dir, int32_t scale) {
	cg_particle_t *p;
	vec3_t v;

	if (!(p = Cg_AllocParticle(PARTICLE_DECAL, cg_particles_burn)))
		return;

	p->part.blend = GL_ONE_MINUS_SRC_ALPHA;
	p->part.color = 0 + (Random() & 1);

	p->part.alpha = 2.0;
	p->alpha_vel = -1.0 / (2.0 + Randomf() * 0.3);

	p->part.scale = scale;

	VectorScale(dir, -1, v);
	VectorAngles(v, p->part.dir);
	p->part.dir[ROLL] = Random() % 360;
	VectorAdd(org, dir, p->part.org);
}

/*
 * @brief
 */
static void Cg_BloodEffect(const vec3_t org, const vec3_t dir, int32_t count) {
	int32_t i, j;
	cg_particle_t *p;
	vec_t d;

	for (i = 0; i < count; i++) {

		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, cg_particles_blood)))
			break;

		p->part.color = 232 + (Random() & 7);

		p->part.alpha = 1.0;
		p->alpha_vel = -1.0 / (0.5 + Randomf() * 0.3);

		p->part.scale = 6.0;

		d = Random() & 31;
		for (j = 0; j < 3; j++) {
			p->part.org[j] = org[j] + ((Random() & 7) - 4.0) + d * dir[j];
			p->vel[j] = Randomc() * 20.0;
		}
		p->part.org[2] += 16.0 * PM_SCALE;

		p->accel[0] = p->accel[1] = 0.0;
		p->accel[2] = PARTICLE_GRAVITY / 4.0;
	}
}

#define GIB_STREAM_DIST 180.0
#define GIB_STREAM_COUNT 24

/*
 * @brief
 */
void Cg_GibEffect(const vec3_t org, int32_t count) {
	cg_particle_t *p;
	vec3_t o, v, tmp;
	c_trace_t tr;
	vec_t dist;
	int32_t i, j;

	// if a player has died underwater, emit some bubbles
	if (cgi.PointContents(org) & MASK_WATER) {
		VectorCopy(org, tmp);
		tmp[2] += 64.0;

		Cg_BubbleTrail(org, tmp, 16.0);
	}

	for (i = 0; i < count; i++) {

		// set the origin and velocity for each gib stream
		VectorSet(o, Randomc() * 8.0, Randomc() * 8.0, 8.0 + Randomc() * 12.0);
		VectorAdd(o, org, o);

		VectorSet(v, Randomc(), Randomc(), 0.2 + Randomf());

		dist = GIB_STREAM_DIST;
		VectorMA(o, dist, v, tmp);

		tr = cgi.Trace(o, tmp, vec3_origin, vec3_origin, MASK_SHOT);
		dist = GIB_STREAM_DIST * tr.fraction;

		for (j = 1; j < GIB_STREAM_COUNT; j++) {

			if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, cg_particles_blood)))
				return;

			p->part.color = 232 + (Random() & 7);

			p->part.alpha = 1.0;
			p->alpha_vel = -1.0 / (2.0 + Randomf() * 0.3);

			p->part.scale = 6.0 + Randomf();
			p->scale_vel = -6.0 + Randomc();

			VectorCopy(o, p->part.org);

			VectorScale(v, dist * ((vec_t)j / GIB_STREAM_COUNT), p->vel);
			p->vel[0] += Randomc() * 2.0;
			p->vel[1] += Randomc() * 2.0;
			p->vel[2] += 100.0;

			p->accel[0] = p->accel[1] = 0.0;
			p->accel[2] = -PARTICLE_GRAVITY * 2.0;
		}
	}
}

/*
 * @brief
 */
void Cg_SparksEffect(const vec3_t org, const vec3_t dir, int32_t count) {
	cg_particle_t *p;
	r_sustained_light_t s;
	int32_t i, j;

	for (i = 0; i < count; i++) {

		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, cg_particles_spark)))
			break;

		p->part.color = 0xd7 + (i % 14);

		p->part.alpha = 1.5;
		p->alpha_vel = -1.0 / (0.5 + Randomf() * 0.3);

		p->part.scale = 2.5;

		VectorCopy(org, p->part.org);
		VectorCopy(dir, p->vel);

		for (j = 0; j < 3; j++) {
			p->part.org[j] += Randomc() * 4.0;
			p->vel[j] += Randomc() * 20.0;
		}

		p->accel[0] = Randomc() * 16.0;
		p->accel[1] = Randomc() * 16.0;
		p->accel[2] = -PARTICLE_GRAVITY;
	}

	VectorCopy(org, s.light.origin);
	s.light.radius = 80.0;
	VectorSet(s.light.color, 0.7, 0.5, 0.5);
	s.sustain = 650;

	cgi.AddSustainedLight(&s);

	cgi.PlaySample(org, -1, cg_sample_sparks, ATTN_STATIC);
}

/*
 * @brief
 */
static void Cg_ExplosionEffect(const vec3_t org) {
	int32_t j;
	cg_particle_t *p;
	r_sustained_light_t s;

	if ((p = Cg_AllocParticle(PARTICLE_ROLL, cg_particles_explosion))) {

		p->part.color = 224;

		p->part.scale = 6.0;
		p->scale_vel = 600.0;

		p->part.alpha = 1.0;
		p->alpha_vel = -4.0;

		p->part.roll = Randomc() * 100.0;

		VectorCopy(org, p->part.org);
	}

	if (!(cgi.PointContents(org) & MASK_WATER)) {

		if ((p = Cg_AllocParticle(PARTICLE_ROLL, cg_particles_smoke))) {

			p->part.blend = GL_ONE;
			p->part.color = Random() & 7;

			p->part.scale = 12.0;
			p->scale_vel = 40.0;

			p->part.alpha = 1.0;
			p->alpha_vel = -1.0 / (1 + Randomf() * 0.6);

			p->part.roll = Randomc() * 100.0;

			VectorCopy(org, p->part.org);
			p->part.org[2] += 10;

			for (j = 0; j < 3; j++) {
				p->vel[j] = Randomc();
			}

			p->accel[2] = 20;
		}
	}

	for (j = 0; j < 128; j++) {

		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, NULL )))
			break;

		p->part.color = 0xe0 + (Random() & 7);

		p->part.alpha = 0.5 + Randomc() * 0.25;
		p->alpha_vel = -1.0 + 0.25 * Randomc();

		p->part.scale = 2.0;

		p->part.org[0] = org[0] + (Random() % 32) - 16.0;
		p->part.org[1] = org[1] + (Random() % 32) - 16.0;
		p->part.org[2] = org[2] + (Random() % 32) - 16.0;

		p->vel[0] = (Random() % 512) - 256;
		p->vel[1] = (Random() % 512) - 256;
		p->vel[2] = (Random() % 512) - 256;

		VectorSet(p->accel, 0.0, 0.0, -PARTICLE_GRAVITY);
	}

	VectorCopy(org, s.light.origin);
	s.light.radius = 200.0;
	VectorSet(s.light.color, 0.8, 0.4, 0.2);
	s.sustain = 1000;

	cgi.AddSustainedLight(&s);

	cgi.PlaySample(org, -1, cg_sample_explosion, ATTN_NORM);
}

/*
 * @brief
 */
static void Cg_HyperblasterEffect(const vec3_t org) {
	cg_particle_t *p;
	r_sustained_light_t s;
	int32_t i;

	for (i = 0; i < 2; i++) {

		if (!(p = Cg_AllocParticle(PARTICLE_ROLL, cg_particles_explosion)))
			break;

		p->part.color = 113 + Random() % 3;

		p->part.alpha = 1.0;
		p->alpha_vel = -8.0;

		p->part.scale = 1.5;
		p->scale_vel = 225.0 * (i + 1);

		p->part.roll = 100.0 * Randomc();

		VectorCopy(org, p->part.org);
	}

	VectorCopy(org, s.light.origin);
	s.light.radius = 80.0;
	VectorSet(s.light.color, 0.4, 0.7, 1.0);
	s.sustain = 250;

	cgi.AddSustainedLight(&s);

	cgi.PlaySample(org, -1, cg_sample_hyperblaster_hit, ATTN_NORM);
}

/*
 * @brief
 */
static void Cg_LightningEffect(const vec3_t org) {
	r_sustained_light_t s;
	vec3_t tmp;
	int32_t i, j;

	for (i = 0; i < 40; i++) {

		VectorCopy(org, tmp);

		for (j = 0; j < 3; j++)
			tmp[j] = tmp[j] + (Random() % 96) - 48.0;

		Cg_BubbleTrail(org, tmp, 4.0);
	}

	VectorCopy(org, s.light.origin);
	s.light.radius = 160.0;
	VectorSet(s.light.color, 0.6, 0.6, 1.0);
	s.sustain = 750;

	cgi.AddSustainedLight(&s);

	cgi.PlaySample(org, -1, cg_sample_lightning_discharge, ATTN_NORM);
}

/*
 * @brief
 */
static void Cg_RailEffect(const vec3_t start, const vec3_t end, int32_t flags, int32_t color) {
	vec3_t vec, right, up, point;
	vec_t len;
	cg_particle_t *p;
	r_sustained_light_t s;
	int32_t i;

	color = color ? color : EFFECT_COLOR_BLUE;

	VectorCopy(start, s.light.origin);
	s.light.radius = 100.0;
	cgi.ColorFromPalette(color, s.light.color);
	s.sustain = 500;

	cgi.AddSustainedLight(&s);

	// draw the core with a beam

	if (!(p = Cg_AllocParticle(PARTICLE_BEAM, cg_particles_beam)))
		return;

	// white cores for some colors, shifted for others
	switch (color) {
		case EFFECT_COLOR_RED:
			p->part.color = 229;
			break;
		case EFFECT_COLOR_BLUE:
		case EFFECT_COLOR_GREEN:
		case EFFECT_COLOR_PURPLE:
			p->part.color = 216;
			break;
		default:
			p->part.color = color + 6;
			break;
	}

	p->part.alpha = 0.75;
	p->alpha_vel = -1.0;

	p->part.scale = 3.0;

	VectorCopy(start, p->part.org);
	VectorCopy(end, p->part.end);

	// and the spiral with normal parts
	VectorCopy(start, point);

	VectorSubtract(end, start, vec);
	len = VectorNormalize(vec);

	VectorSet(right, vec[2], -vec[0], vec[1]);
	CrossProduct(vec, right, up);

	for (i = 0; i < len; i++) {

		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, NULL )))
			return;

		p->part.color = color;

		p->part.alpha = 1.0;
		p->alpha_vel = -2.0 + i / len;

		p->part.scale = 1.5 + Randomc() * 0.2;
		p->scale_vel = 2.0 + Randomc() * 0.2;

		VectorAdd(point, vec, point);
		VectorCopy(point, p->part.org);

		VectorScale(vec, 2.0, p->vel);

		VectorMA(p->part.org, cos(i * 0.1) * 6.0, right, p->part.org);
		VectorMA(p->part.org, sin(i * 0.1) * 6.0, up, p->part.org);

		// check for bubble trail
		if (i % 24 == 0 && (cgi.PointContents(point) & MASK_WATER)) {
			Cg_BubbleTrail(point, p->part.org, 16.0);
		}

		// add sustained lights
		if (i > 0 && i < len - 64.0 && i % 64 == 0) {
			VectorCopy(point, s.light.origin);
			s.sustain += 25;
			cgi.AddSustainedLight(&s);
		}
	}

	// check for explosion effect on solids
	if (flags & SURF_SKY)
		return;

	if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, cg_particles_explosion)))
		return;

	p->part.color = color;

	p->part.scale = 1.0;
	p->scale_vel = 800.0;

	p->part.alpha = 1.25;
	p->alpha_vel = -10.0;

	VectorCopy(end, p->part.org);

	VectorMA(end, -12.0, vec, s.light.origin);
	s.light.radius = 120.0;
	s.sustain += 250;

	cgi.AddSustainedLight(&s);
}

/*
 * @brief
 */
static void Cg_BfgEffect(const vec3_t org) {
	cg_particle_t *p;
	r_sustained_light_t s;
	int32_t i;

	for (i = 0; i < 4; i++) {

		if (!(p = Cg_AllocParticle(PARTICLE_ROLL, cg_particles_explosion)))
			break;

		p->part.color = 200 + Random() % 3;

		p->part.alpha = 1.0;
		p->alpha_vel = -3.0;

		p->part.scale = 4.0;
		p->scale_vel = 200.0 * (i + 1);

		p->part.roll = 100.0 * Randomc();

		VectorCopy(org, p->part.org);
	}

	for (i = 0; i < 96; i++) {

		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, NULL )))
			break;

		p->part.color = 200 + Random() % 3;

		p->part.alpha = 0.5 + Randomc() * 0.25;
		p->alpha_vel = -1.0 + 0.25 * Randomc();

		p->part.scale = 4.0;

		p->part.org[0] = org[0] + (Random() % 48) - 24;
		p->part.org[1] = org[1] + (Random() % 48) - 24;
		p->part.org[2] = org[2] + (Random() % 48) - 24;

		p->vel[0] = (Random() % 768) - 384;
		p->vel[1] = (Random() % 768) - 384;
		p->vel[2] = (Random() % 768) - 384;

		VectorSet(p->accel, 0.0, 0.0, -PARTICLE_GRAVITY);
	}

	VectorCopy(org, s.light.origin);
	s.light.radius = 200.0;
	VectorSet(s.light.color, 0.8, 1.0, 0.5);
	s.sustain = 1000;

	cgi.AddSustainedLight(&s);

	cgi.PlaySample(org, -1, cg_sample_bfg_hit, ATTN_NORM);
}

/*
 * @brief
 */
void Cg_ParseTempEntity(void) {
	vec3_t pos, pos2, dir;
	int32_t i, j;

	const byte type = cgi.ReadByte();

	switch (type) {

	case TE_BLASTER:
		cgi.ReadPosition(pos);
		cgi.ReadPosition(dir);
		i = cgi.ReadByte();
		Cg_BlasterEffect(pos, dir, i);
		break;

	case TE_TRACER:
		cgi.ReadPosition(pos);
		cgi.ReadPosition(pos2);
		Cg_TracerEffect(pos, pos2);
		break;

	case TE_BULLET: // bullet hitting wall
		cgi.ReadPosition(pos);
		cgi.ReadDirection(dir);
		Cg_BulletEffect(pos, dir);
		break;

	case TE_BURN: // burn mark on wall
		cgi.ReadPosition(pos);
		cgi.ReadDirection(dir);
		i = cgi.ReadByte();
		Cg_BurnEffect(pos, dir, i);
		break;

	case TE_BLOOD: // projectile hitting flesh
		cgi.ReadPosition(pos);
		cgi.ReadDirection(dir);
		Cg_BloodEffect(pos, dir, 12);
		break;

	case TE_GIB: // player over-death
		cgi.ReadPosition(pos);
		Cg_GibEffect(pos, 12);
		break;

	case TE_SPARKS: // colored sparks
		cgi.ReadPosition(pos);
		cgi.ReadDirection(dir);
		Cg_SparksEffect(pos, dir, 12);
		break;

	case TE_HYPERBLASTER: // hyperblaster hitting wall
		cgi.ReadPosition(pos);
		Cg_HyperblasterEffect(pos);
		break;

	case TE_LIGHTNING: // lightning discharge in water
		cgi.ReadPosition(pos);
		Cg_LightningEffect(pos);
		break;

	case TE_RAIL: // railgun effect
		cgi.ReadPosition(pos);
		cgi.ReadPosition(pos2);
		i = cgi.ReadLong();
		j = cgi.ReadByte();
		Cg_RailEffect(pos, pos2, i, j);
		break;

	case TE_EXPLOSION: // rocket and grenade explosions
		cgi.ReadPosition(pos);
		Cg_ExplosionEffect(pos);
		break;

	case TE_BFG: // bfg explosion
		cgi.ReadPosition(pos);
		Cg_BfgEffect(pos);
		break;

	case TE_BUBBLES: // bubbles chasing projectiles in water
		cgi.ReadPosition(pos);
		cgi.ReadPosition(pos2);
		Cg_BubbleTrail(pos, pos2, 1.0);
		break;

	default:
		cgi.Warn("Unknown type: %d\n", type);
		return;
	}
}
