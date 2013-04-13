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
#include "common-anorms.h"

/*
 * @brief
 */
static void Cg_BlasterTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent) {
	r_corona_t c;
	r_light_t l;
	vec3_t color;
	int32_t i;

	if (ent->time < cgi.client->time) {
		cg_particle_t *p;
		vec3_t delta;
		float d, dist, step;

		step = 0.5;

		if (cgi.PointContents(end) & MASK_WATER) {
			Cg_BubbleTrail(start, end, 12.0);
			step = 1.5;
		}

		d = 0.0;

		VectorSubtract(end, start, delta);
		dist = VectorNormalize(delta);

		while (d < dist) {

			if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, NULL ))) {
				break;
			}

			p->part.color = ent->current.client + (Random() & 7);

			p->part.alpha = 1.0;
			p->alpha_vel = -4.0 + Randomc();

			p->part.scale = 1.0;
			p->scale_vel = 0.0;

			VectorMA(start, d, delta, p->part.org);
			VectorScale(delta, 400.0, p->vel);

			for (i = 0; i < 3; i++) {
				p->part.org[i] += Randomc() * 0.5;
				p->vel[i] += Randomc() * 5.0;
			}

			d += step;
		}

		ent->time = cgi.client->time + 32;
	}

	cgi.ColorFromPalette(ent->current.client, color);

	VectorScale(color, 3.0, color);

	for (i = 0; i < 3; i++) {
		if (color[i] > 1.0)
			color[i] = 1.0;
	}

	VectorCopy(end, c.origin);
	c.radius = 3.0;
	c.flicker = 0.5;
	VectorCopy(color, c.color);

	cgi.AddCorona(&c);

	VectorCopy(end, l.origin);
	l.radius = 60.0;
	VectorCopy(color, l.color);

	cgi.AddLight(&l);
}

/*
 * @brief
 */
void Cg_TeleporterTrail(const vec3_t org, cl_entity_t *cent) {
	int32_t i;
	cg_particle_t *p;

	if (cent) { // honor a slightly randomized time interval

		if (cent->time > cgi.client->time)
			return;

		cgi.PlaySample(NULL, cent->current.number, cg_sample_respawn, ATTN_IDLE);

		cent->time = cgi.client->time + 1000 + (2000 * Randomf());
	}

	for (i = 0; i < 4; i++) {

		if (!(p = Cg_AllocParticle(PARTICLE_SPLASH, cg_particle_teleporter)))
			return;

		p->part.color = 216;

		p->part.alpha = 1.0;
		p->alpha_vel = -1.8;

		p->part.scale = 16.0;
		p->scale_vel = 24.0;

		VectorCopy(org, p->part.org);
		p->part.org[2] -= (6 * i);
		p->vel[2] = 140;
	}
}

/*
 * @brief
 */
void Cg_SmokeTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent) {
	cg_particle_t *p;
	int32_t j;

	if (ent) { // trails should be framerate independent

		if (ent->time > cgi.client->time)
			return;

		ent->time = cgi.client->time + 32;
	}

	if (cgi.PointContents(end) & MASK_WATER) {
		Cg_BubbleTrail(start, end, 24.0);
		return;
	}

	if (!(p = Cg_AllocParticle(PARTICLE_ROLL, cg_particle_smoke)))
		return;

	p->part.blend = GL_ONE;
	p->part.color = 32 + (Random() & 7);

	p->part.alpha = 1.5;
	p->alpha_vel = -1.0 / (1.0 + Randomf());

	p->part.scale = 2.0;
	p->scale_vel = 10 + 25.0 * Randomf();

	p->part.roll = Randomc() * 100.0;

	for (j = 0; j < 3; j++) {
		p->part.org[j] = end[j];
		p->vel[j] = Randomc();
	}

	p->accel[2] = 5.0;
}

/*
 * @brief
 */
void Cg_FlameTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent) {
	cg_particle_t *p;
	int32_t j;

	if (ent) { // trails should be framerate independent

		if (ent->time > cgi.client->time)
			return;

		ent->time = cgi.client->time + 8;
	}

	if (cgi.PointContents(end) & MASK_WATER) {
		Cg_BubbleTrail(start, end, 10.0);
		return;
	}

	if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, cg_particle_flame)))
		return;

	p->part.color = 220 + (Random() & 7);

	p->part.alpha = 0.75;
	p->alpha_vel = -1.0 / (2 + Randomf() * 0.3);

	p->part.scale = 10.0 + Randomc();

	for (j = 0; j < 3; j++) {
		p->part.org[j] = end[j];
		p->vel[j] = Randomc() * 1.5;
	}

	p->accel[2] = 15.0;

	// make static flames rise
	if (ent) {
		if (VectorCompare(ent->current.origin, ent->current.old_origin)) {
			p->alpha_vel *= 0.65;
			p->accel[2] = 20.0;
		}
	}
}

/*
 * @brief
 */
void Cg_SteamTrail(const vec3_t org, const vec3_t vel, cl_entity_t *ent) {
	cg_particle_t *p;
	vec3_t end;
	int32_t j;

	VectorAdd(org, vel, end);

	if (ent) { // trails should be framerate independent

		if (ent->time > cgi.client->time)
			return;

		ent->time = cgi.client->time + 8;
	}

	if (cgi.PointContents(org) & MASK_WATER) {
		Cg_BubbleTrail(org, end, 10.0);
		return;
	}

	if (!(p = Cg_AllocParticle(PARTICLE_ROLL, cg_particle_steam)))
		return;

	p->part.color = 6 + (Random() & 7);

	p->part.alpha = 0.3;
	p->alpha_vel = -1.0 / (5.0 + Randomf() * 0.5);

	p->part.scale = 8.0;
	p->scale_vel = 20.0;

	p->part.roll = Randomc() * 100.0;

	VectorCopy(org, p->part.org);
	VectorCopy(vel, p->vel);

	for (j = 0; j < 3; j++) {
		p->vel[j] += 2.0 * Randomc();
	}
}

/*
 * @brief
 */
void Cg_BubbleTrail(const vec3_t start, const vec3_t end, float density) {
	cg_particle_t *p;
	vec3_t vec, move;
	float i, len, delta;
	int32_t j;

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	len = VectorNormalize(vec);

	delta = 16.0 / density;
	VectorScale(vec, delta, vec);
	VectorSubtract(move, vec, move);

	for (i = 0.0; i < len; i += delta) {
		VectorAdd(move, vec, move);

		if (!(cgi.PointContents(move) & MASK_WATER))
			continue;

		if (!(p = Cg_AllocParticle(PARTICLE_BUBBLE, cg_particle_bubble)))
			return;

		p->part.color = 6 + (Random() & 3);

		p->part.alpha = 1.0;
		p->alpha_vel = -0.2 - Randomf() * 0.2;

		p->part.scale = 1.5;
		p->scale_vel = -0.4 - Randomf() * 0.2;

		for (j = 0; j < 3; j++) {
			p->part.org[j] = move[j] + Randomc() * 2.0;
			p->vel[j] = Randomc() * 5.0;
		}

		p->vel[2] += 6.0;
		p->accel[2] = 10.0;
	}
}

/*
 * @brief
 */
static void Cg_EnergyTrail(cl_entity_t *ent, const vec3_t org, float radius, int32_t color) {
	static vec3_t angles[NUM_APPROXIMATE_NORMALS];
	int32_t i, c;
	cg_particle_t *p;
	float angle;
	float sp, sy, cp, cy;
	vec3_t forward;
	float dist;
	vec3_t v;
	float ltime;

	if (!angles[0][0]) { // initialize our angular velocities
		for (i = 0; i < NUM_APPROXIMATE_NORMALS * 3; i++)
			angles[0][i] = (Random() & 255) * 0.01;
	}

	ltime = (float) cgi.client->time / 300.0;

	for (i = 0; i < NUM_APPROXIMATE_NORMALS; i += 2) {
		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, NULL )))
			return;

		angle = ltime * angles[i][0];
		sy = sin(angle);
		cy = cos(angle);

		angle = ltime * angles[i][1];
		sp = sin(angle);
		cp = cos(angle);

		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;

		p->part.alpha = 1.0;
		p->alpha_vel = -100.0;

		p->part.scale = 0.5 + (0.05 * radius);

		dist = sin(ltime + i) * radius;

		for (c = 0; c < 3; c++) {
			// project the origin outward, adding in angular velocity
			p->part.org[c] = org[c] + (approximate_normals[i][c] * dist) + forward[c] * radius;
		}

		VectorSubtract(p->part.org, org, v);
		dist = VectorLength(v) / (3.0 * radius);
		p->part.color = color + dist * 7.0;

		p->vel[0] = p->vel[1] = p->vel[2] = 2.0 * Randomc();
	}

	// add a bubble trail if appropriate
	if (ent->time > cgi.client->time)
		return;

	ent->time = cgi.client->time + 8;

	if (cgi.PointContents(org) & MASK_WATER)
		Cg_BubbleTrail(ent->prev.origin, ent->current.origin, radius / 4.0);
}

/*
 * @brief
 */
static void Cg_GrenadeTrail(const vec3_t start, const vec3_t end) {

	if (cgi.PointContents(end) & MASK_WATER) {
		Cg_BubbleTrail(start, end, 24.0);
	}
}

/*
 * @brief
 */
static void Cg_RocketTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent) {
	r_corona_t c;
	r_light_t l;

	const uint32_t old_time = ent->time;

	Cg_SmokeTrail(start, end, ent);

	if (old_time != ent->time) { // time to add new particles
		cg_particle_t *p;
		vec3_t delta;
		float d;

		VectorSubtract(end, start, delta);
		const float dist = VectorNormalize(delta);

		d = 0.0;

		while (d < dist) {

			if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, NULL )))
				break;

			p->part.color = 0xe0 + (Random() & 7);

			p->part.alpha = 0.5 + Randomc() * 0.25;
			p->alpha_vel = -1.0 + 0.25 * Randomc();

			VectorMA(start, d, delta, p->part.org);

			p->vel[0] = 32.0 * Randomc();
			p->vel[1] = 32.0 * Randomc();
			p->vel[2] = -PARTICLE_GRAVITY * 0.25 * Randomf();

			d += 2.0;
		}
	}

	VectorCopy(end, c.origin);
	c.radius = 3.0;
	c.flicker = 0.25;
	VectorSet(c.color, 0.8, 0.4, 0.2);

	cgi.AddCorona(&c);

	VectorCopy(end, l.origin);
	l.radius = 150.0;
	VectorSet(l.color, 0.8, 0.4, 0.2);

	cgi.AddLight(&l);
}

/*
 * @brief
 */
static void Cg_HyperblasterTrail(cl_entity_t *ent, const vec3_t org) {
	r_corona_t c;
	r_light_t l;

	Cg_EnergyTrail(ent, org, 6.0, 107);

	VectorCopy(org, c.origin);
	c.radius = 10.0;
	c.flicker = 0.15;
	VectorSet(c.color, 0.4, 0.7, 1.0);

	cgi.AddCorona(&c);

	VectorCopy(org, l.origin);
	l.radius = 100.0;
	VectorSet(l.color, 0.4, 0.7, 1.0);

	cgi.AddLight(&l);
}

/*
 * @brief
 */
static void Cg_LightningTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent) {
	cg_particle_t *p;
	r_light_t l;
	vec3_t dir, delta, pos, vel;
	float dist, offset;
	int32_t i;

	VectorCopy(start, l.origin);
	l.radius = 90.0 + 10.0 * Randomc();
	VectorSet(l.color, 0.6, 0.6, 1.0);
	cgi.AddLight(&l);

	cgi.LoopSample(start, cg_sample_lightning_fly);

	VectorSubtract(start, end, dir);
	dist = VectorNormalize(dir);

	if (dist > 256.0) {
		cgi.LoopSample(end, cg_sample_lightning_fly);
	}

	VectorScale(dir, -48.0, delta);
	VectorCopy(start, pos);

	VectorSubtract(ent->current.origin, ent->prev.origin, vel);

	i = 0;
	while (dist > 0.0) {

		if (!(p = Cg_AllocParticle(PARTICLE_BEAM, cg_particle_lightning)))
			return;

		p->part.color = 12 + (Random() & 3);

		p->part.alpha = 1.0;
		p->alpha_vel = -9999.0;

		p->part.scale = 8.0;

		p->part.scroll_s = -4.0;

		VectorCopy(pos, p->part.org);

		if (dist <= 48.0) {
			VectorScale(dir, dist, delta);
			offset = 0.0;
		} else {
			offset = fabs(8.0 * sin(dist));
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
}

/*
 * @brief
 */
static void Cg_BfgTrail(cl_entity_t *ent, const vec3_t org) {
	r_corona_t c;
	r_light_t l;

	Cg_EnergyTrail(ent, org, 24.0, 206);

	VectorCopy(org, c.origin);
	c.radius = 24.0;
	c.flicker = 0.05;
	VectorSet(c.color, 0.4, 1.0, 0.4);

	cgi.AddCorona(&c);

	VectorCopy(org, l.origin);
	l.radius = 120.0;
	VectorSet(l.color, 0.4, 1.0, 0.4);

	cgi.AddLight(&l);
}

/*
 * @brief
 */
void Cg_InactiveTrail(const vec3_t start) {
	cg_particle_t *p;

	if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, NULL )))
		return;

	p->part.color = 11;

	p->part.alpha = 1.0;
	p->alpha_vel = -99999999.0;

	p->part.scale = 10.0;

	VectorCopy(start, p->part.org);
	p->part.org[2] += 50;
}

/*
 * @brief Processes the specified entity's effects mask, augmenting the given renderer
 * entity and adding additional effects such as particle trails to the view.
 */
void Cg_EntityEffects(cl_entity_t *e, r_entity_t *ent) {
	vec3_t start, end;

	const uint32_t time = cgi.client->time;
	const float lerp = cgi.client->lerp;

	const entity_state_t *s = &e->current;
	ent->effects = s->effects;

	VectorCopy(e->prev.origin, start);
	VectorCopy(ent->origin, end);

	// beams have two origins, most entities have just one
	if (s->effects & EF_BEAM) {

		// client is overridden to specify owner of the beam
		if ((e->current.client == cgi.client->player_num + 1) && !cg_third_person->value) {
			// we own this beam (lightning, grapple, etc..)
			// project start position in front of view origin
			VectorCopy(cgi.view->origin, start);
			VectorMA(start, 22.0, cgi.view->forward, start);
			VectorMA(start, 6.0, cgi.view->right, start);
			start[2] -= 8.0;
		}

		VectorLerp(e->prev.old_origin, e->current.old_origin, lerp, end);
	}

	// bob items, shift them to randomize the effect in crowded scenes
	if (s->effects & EF_BOB) {
		const float bob = sin((time * 0.005) + ent->origin[0] + ent->origin[1]);
		ent->origin[2] += 4.0 * bob;
	}

	// calculate angles
	if (s->effects & EF_ROTATE) { // some bonus items rotate
		ent->angles[0] = 0.0;
		ent->angles[1] = time / 3.4;
		ent->angles[2] = 0.0;
	} else { // interpolate angles
		AngleLerp(e->prev.angles, e->current.angles, lerp, ent->angles);
	}

	// parse the effects bit mask

	if (s->effects & EF_BLASTER) {
		Cg_BlasterTrail(start, end, e);
	}

	if (s->effects & EF_GRENADE) {
		Cg_GrenadeTrail(start, end);
	}

	if (s->effects & EF_ROCKET) {
		Cg_RocketTrail(start, end, e);
	}

	if (s->effects & EF_HYPERBLASTER) {
		Cg_HyperblasterTrail(e, ent->origin);
	}

	if (s->effects & EF_LIGHTNING) {
		Cg_LightningTrail(start, end, e);
	}

	if (s->effects & EF_BFG) {
		Cg_BfgTrail(e, ent->origin);
	}

	VectorClear(ent->shell);

	if (s->effects & EF_CTF_BLUE) {
		r_light_t l = { { 0.0, 0.0, 0.0 }, 80.0, { 0.3, 0.3, 1.0 } };

		VectorCopy(ent->origin, l.origin);
		cgi.AddLight(&l);

		VectorScale(l.color, 0.5, ent->shell);
	}

	if (s->effects & EF_CTF_RED) {
		r_light_t l = { { 0.0, 0.0, 0.0 }, 80.0, { 1.0, 0.3, 0.3 } };

		VectorCopy(ent->origin, l.origin);
		cgi.AddLight(&l);

		VectorScale(l.color, 0.5, ent->shell);
	}

	if (s->effects & EF_QUAD) {
		r_light_t l = { { 0.0, 0.0, 0.0 }, 80.0, { 0.3, 0.7, 0.7 } };

		VectorCopy(ent->origin, l.origin);
		cgi.AddLight(&l);

		VectorScale(l.color, 0.5, ent->shell);
	}

	if (s->effects & EF_RESPAWN) {
		r_light_t l = { { 0.0, 0.0, 0.0 }, 80.0, { 1.0, 1.0, 0.0 } };

		//VectorCopy(ent->origin, l.origin);
		//cgi.AddLight(&l);
		VectorScale(l.color, 0.5, ent->shell);
	}

	if (s->effects & EF_TELEPORTER) {
		Cg_TeleporterTrail(ent->origin, e);
	}

	if (s->effects & EF_INACTIVE) {
		Cg_InactiveTrail(ent->origin);
	}
}
