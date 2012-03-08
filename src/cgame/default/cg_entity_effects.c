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
 * Cg_BlasterTrail
 */
static void Cg_BlasterTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent) {
	r_corona_t c;
	r_light_t l;
	vec3_t color;
	int i;

	if (ent->time < cgi.client->time) {
		r_particle_t *p;
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

			if (!(p = Cg_AllocParticle())) {
				break;
			}

			p->type = PARTICLE_NORMAL;

			VectorMA(start, d, delta, p->org);
			VectorScale(delta, 400.0, p->vel);

			for (i = 0; i < 3; i++) {
				p->org[i] += crand() * 0.5;
				p->vel[i] += crand() * 5.0;
			}

			p->scale = 1.0;
			p->scale_vel = 0.0;

			p->alpha = 1.0;
			p->alpha_vel = -4.0 + crand();

			p->color = ent->current.client + (rand() & 7);
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
 * Cg_TeleporterTrail
 */
void Cg_TeleporterTrail(const vec3_t org, cl_entity_t *cent) {
	int i;
	r_particle_t *p;

	if (cent) { // honor a slightly randomized time interval

		if (cent->time > cgi.client->time)
			return;

		cgi.PlaySample(NULL, cent->current.number, cg_sample_respawn, ATTN_IDLE);

		cent->time = cgi.client->time + 1000 + (2000 * frand());
	}

	for (i = 0; i < 4; i++) {

		if (!(p = Cg_AllocParticle()))
			return;

		p->type = PARTICLE_SPLASH;
		p->image = cg_particle_teleporter;
		p->color = 216;
		p->scale = 16.0;
		p->scale_vel = 24.0;
		p->alpha = 1.0;
		p->alpha_vel = -1.8;

		VectorCopy(org, p->org);
		p->org[2] -= (6 * i);
		p->vel[2] = 140;
	}
}

/*
 * Cg_SmokeTrail
 */
void Cg_SmokeTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent) {
	r_particle_t *p;
	bool stationary;
	int j;

	if (cgi.view->render_mode == render_mode_pro)
		return;

	stationary = false;

	if (ent) { // trails should be framerate independent

		if (ent->time > cgi.client->time)
			return;

		// trails diminish for stationary entities (grenades)
		stationary = VectorCompare(ent->current.origin, ent->current.old_origin);

		if (stationary)
			ent->time = cgi.client->time + 128;
		else
			ent->time = cgi.client->time + 32;
	}

	if (cgi.PointContents(end) & MASK_WATER) {
		Cg_BubbleTrail(start, end, 24.0);
		return;
	}

	if (!(p = Cg_AllocParticle()))
		return;

	p->accel[2] = 5.0;

	p->image = cg_particle_smoke;
	p->type = PARTICLE_ROLL;

	p->scale = 2.0;
	p->scale_vel = 10 + 25.0 * frand();

	p->alpha = 1.0;
	p->alpha_vel = -1.0 / (1.0 + frand());

	p->color = 32 + (rand() & 7);

	for (j = 0; j < 3; j++) {
		p->org[j] = end[j];
		p->vel[j] = crand();
	}
	p->roll = crand() * 100.0; // rotation

	// make smoke rise from grenades that have come to rest
	if (ent && stationary) {
		p->alpha_vel *= 0.65;
		p->accel[2] = 20.0;
	}
}

/*
 * Cg_FlameTrail
 */
void Cg_FlameTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent) {
	r_particle_t *p;
	int j;

	if (ent) { // trails should be framerate independent

		if (ent->time > cgi.client->time)
			return;

		ent->time = cgi.client->time + 8;
	}

	if (cgi.PointContents(end) & MASK_WATER) {
		Cg_BubbleTrail(start, end, 10.0);
		return;
	}

	if (!(p = Cg_AllocParticle()))
		return;

	p->accel[2] = 15.0;

	p->image = cg_particle_flame;
	p->type = PARTICLE_NORMAL;
	p->scale = 10.0 + crand();

	p->alpha = 0.75;
	p->alpha_vel = -1.0 / (2 + frand() * 0.3);
	p->color = 220 + (rand() & 7);

	for (j = 0; j < 3; j++) {
		p->org[j] = end[j];
		p->vel[j] = crand() * 1.5;
	}

	// make static flames rise
	if (ent) {
		if (VectorCompare(ent->current.origin, ent->current.old_origin)) {
			p->alpha_vel *= 0.65;
			p->accel[2] = 20.0;
		}
	}
}

/*
 * Cg_SteamTrail
 */
void Cg_SteamTrail(const vec3_t org, const vec3_t vel, cl_entity_t *ent) {
	r_particle_t *p;
	vec3_t end;
	int j;

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

	if (!(p = Cg_AllocParticle()))
		return;

	p->image = cg_particle_steam;
	p->type = PARTICLE_ROLL;

	p->scale = 8.0;
	p->scale_vel = 20.0;

	p->alpha = 0.75;
	p->alpha_vel = -1.0 / (1 + frand() * 0.6);

	p->color = 6 + (rand() & 7);

	VectorCopy(org, p->org);
	VectorCopy(vel, p->vel);

	for (j = 0; j < 3; j++) {
		p->vel[j] += 2.0 * crand();
	}

	p->roll = crand() * 100.0; // rotation
}

/*
 * Cg_BubbleTrail
 */
void Cg_BubbleTrail(const vec3_t start, const vec3_t end, float density) {
	vec3_t move;
	vec3_t vec;
	float len, f;
	int i, j;
	r_particle_t *p;

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	len = VectorNormalize(vec);

	f = 24.0 / density;
	VectorScale(vec, f, vec);

	for (i = 0; i < len; i += f) {

		if (!(p = Cg_AllocParticle()))
			return;

		p->image = cg_particle_bubble;
		p->type = PARTICLE_BUBBLE;

		p->alpha = 1.0;
		p->alpha_vel = -1.0 / (1 + frand() * 0.2);
		p->color = 4 + (rand() & 7);
		for (j = 0; j < 3; j++) {
			p->org[j] = move[j] + crand() * 2;
			p->vel[j] = crand() * 5;
		}
		p->vel[2] += 6;

		VectorAdd(move, vec, move);
	}
}

/*
 * Cg_EnergyTrail
 */
static void Cg_EnergyTrail(cl_entity_t *ent, float radius, int color) {
	static vec3_t angles[NUM_APPROXIMATE_NORMALS];
	int i, c;
	r_particle_t *p;
	float angle;
	float sp, sy, cp, cy;
	vec3_t forward;
	float dist;
	vec3_t v;
	float ltime;

	if (!angles[0][0]) { // initialize our angular velocities
		for (i = 0; i < NUM_APPROXIMATE_NORMALS * 3; i++)
			angles[0][i] = (rand() & 255) * 0.01;
	}

	ltime = (float) cgi.client->time / 300.0;

	for (i = 0; i < NUM_APPROXIMATE_NORMALS; i += 2) {

		angle = ltime * angles[i][0];
		sy = sin(angle);
		cy = cos(angle);

		angle = ltime * angles[i][1];
		sp = sin(angle);
		cp = cos(angle);

		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;

		if (!(p = Cg_AllocParticle()))
			return;

		p->scale = 0.75;
		p->scale_vel = 800.0;

		dist = sin(ltime + i) * radius;

		for (c = 0; c < 3; c++) {
			// project the origin outward, adding in angular velocity
			p->org[c] = ent->current.origin[c] + (approximate_normals[i][c] * dist) + forward[c]
					* 16.0;
		}

		VectorSubtract(p->org, ent->current.origin, v);
		dist = VectorLength(v) / (3.0 * radius);
		p->color = color + dist * 7.0;

		p->alpha = 1.0 - dist;
		p->alpha_vel = -100.0;

		p->vel[0] = p->vel[1] = p->vel[2] = 2.0 * crand();
	}

	// add a bubble trail if appropriate
	if (ent->time > cgi.client->time)
		return;

	ent->time = cgi.client->time + 8;

	if (cgi.PointContents(ent->current.origin) & MASK_WATER)
		Cg_BubbleTrail(ent->prev.origin, ent->current.origin, 1.0);
}

/*
 * Cg_GrenadeTrail
 */
static void Cg_GrenadeTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent) {
	Cg_SmokeTrail(start, end, ent);
}

/*
 * Cg_RocketTrail
 */
static void Cg_RocketTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent) {
	r_corona_t c;
	r_light_t l;

	const unsigned int old_time = ent->time;

	Cg_SmokeTrail(start, end, ent);

	if (old_time != ent->time) { // time to add new particles
		r_particle_t *p;
		vec3_t delta;
		float d;

		VectorSubtract(end, start, delta);
		const float dist = VectorNormalize(delta);

		d = 0.0;

		while (d < dist) {

			if (!(p = Cg_AllocParticle()))
				break;

			VectorMA(start, d, delta, p->org);

			p->vel[0] = 24.0 * crand();
			p->vel[1] = 24.0 * crand();
			p->vel[2] = -PARTICLE_GRAVITY * 0.25;

			p->alpha = 0.5 + crand() * 0.25;
			p->alpha_vel = -1.0 + 0.25 * crand();

			p->color = 0xe0 + (rand() & 7);
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
 * Cg_HyperblasterTrail
 */
static void Cg_HyperblasterTrail(cl_entity_t *ent) {
	r_corona_t c;
	r_light_t l;

	Cg_EnergyTrail(ent, 8.0, 107);

	VectorCopy(ent->current.origin, c.origin);
	c.radius = 12.0;
	c.flicker = 0.15;
	VectorSet(c.color, 0.4, 0.7, 1.0);

	cgi.AddCorona(&c);

	VectorCopy(ent->current.origin, l.origin);
	l.radius = 100.0;
	VectorSet(l.color, 0.4, 0.7, 1.0);

	cgi.AddLight(&l);
}

/*
 * Cg_LightningTrail
 */
static void Cg_LightningTrail(const vec3_t start, const vec3_t end) {
	r_particle_t *p;
	r_light_t l;
	vec3_t dir, delta, pos;
	float dist;

	cgi.LoopSample(start, cg_sample_lightning_fly);
	cgi.LoopSample(end, cg_sample_lightning_fly);

	VectorSubtract(start, end, dir);
	dist = VectorNormalize(dir);

	VectorScale(dir, -48.0, delta);

	VectorSet(pos, crand() * 0.5, crand() * 0.5, crand() * 0.5);
	VectorAdd(pos, start, pos);

	while (dist > 0.0) {

		if (!(p = Cg_AllocParticle()))
			return;

		p->type = PARTICLE_BEAM;
		p->image = cg_particle_lightning;

		p->scale = 8.0;

		VectorCopy(pos, p->org);

		if (dist < 48.0)
			VectorScale(dir, dist, delta);

		VectorAdd(pos, delta, pos);

		VectorCopy(pos, p->end);

		p->alpha = 1.0;
		p->alpha_vel = -50.0;

		p->color = 12 + (rand() & 3);

		p->scroll_s = -4.0;

		dist -= 48.0;
	}

	VectorCopy(start, l.origin);
	l.radius = 90.0 + 10.0 * crand();
	VectorSet(l.color, 0.6, 0.6, 1.0);

	cgi.AddLight(&l);

	VectorMA(end, 12.0, dir, l.origin);
	cgi.AddLight(&l);
}

/*
 * Cg_BfgTrail
 */
static void Cg_BfgTrail(cl_entity_t *ent) {
	r_corona_t c;
	r_light_t l;

	Cg_EnergyTrail(ent, 16.0, 206);

	VectorCopy(ent->current.origin, c.origin);
	c.radius = 24.0;
	c.flicker = 0.05;
	VectorSet(c.color, 0.4, 1.0, 0.4);

	cgi.AddCorona(&c);

	VectorCopy(ent->current.origin, l.origin);
	l.radius = 120.0;
	VectorSet(l.color, 0.4, 1.0, 0.4);

	cgi.AddLight(&l);
}

/*
 * Cg_InactiveTrail
 */
void Cg_InactiveTrail(const vec3_t start) {
	r_particle_t *p;

	if (!(p = Cg_AllocParticle()))
		return;

	p->image = cg_particle_inactive;
	p->type = PARTICLE_NORMAL;

	p->alpha = 1.0;
	p->alpha_vel = -99999999.0;
	p->color = 11;
	VectorCopy(start, p->org);
	p->org[2] += 50;
	p->scale = 10.0;
}

/**
 * Cg_EntityEffects
 *
 * Processes the specified entity's effects mask, augmenting the given renderer
 * entity and adding additional effects such as particle trails to the view.
 */
void Cg_EntityEffects(cl_entity_t *e, r_entity_t *ent) {
	vec3_t start, end;

	const unsigned int time = cgi.client->time;
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
			VectorMA(start, 30.0, cgi.view->forward, start);
			VectorMA(start, 6.0, cgi.view->right, start);
			start[2] -= 10.0;
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
		Cg_GrenadeTrail(start, end, e);
	}

	if (s->effects & EF_ROCKET) {
		Cg_RocketTrail(start, end, e);
	}

	if (s->effects & EF_HYPERBLASTER) {
		Cg_HyperblasterTrail(e);
	}

	if (s->effects & EF_LIGHTNING) {
		Cg_LightningTrail(start, end);
	}

	if (s->effects & EF_BFG) {
		Cg_BfgTrail(e);
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

	if (s->effects & EF_TELEPORTER) {
		Cg_TeleporterTrail(ent->origin, e);
	}

	if (s->effects & EF_INACTIVE) {
			Cg_InactiveTrail(ent->origin);
	}
}
