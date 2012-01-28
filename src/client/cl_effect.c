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

#include "cl_local.h"

static s_sample_t *cl_sample_shotgun_fire;
static s_sample_t *cl_sample_supershotgun_fire;
static s_sample_t *cl_sample_machinegun_fire[4];
static s_sample_t *cl_sample_machinegun_hit[3];
static s_sample_t *cl_sample_grenadelauncher_fire;
static s_sample_t *cl_sample_rocketlauncher_fire;
static s_sample_t *cl_sample_hyperblaster_fire;
static s_sample_t *cl_sample_hyperblaster_hit;
static s_sample_t *cl_sample_lightning_fire;
static s_sample_t *cl_sample_lightning_fly;
static s_sample_t *cl_sample_lightning_discharge;
static s_sample_t *cl_sample_railgun_fire;
static s_sample_t *cl_sample_bfg_fire;
static s_sample_t *cl_sample_bfg_hit;

static s_sample_t *cl_sample_explosion;
static s_sample_t *cl_sample_teleport;
static s_sample_t *cl_sample_respawn;
static s_sample_t *cl_sample_sparks;
static s_sample_t *cl_sample_footsteps[4];
static s_sample_t *cl_sample_rain;
static s_sample_t *cl_sample_snow;

/*
 * Cl_LoadEffects
 */
void Cl_LoadEffects(void) {
	int i;
	char name[MAX_QPATH];

	Cl_LoadProgress(96);

	cl_sample_shotgun_fire = S_LoadSample("weapons/shotgun/fire");
	cl_sample_supershotgun_fire = S_LoadSample("weapons/supershotgun/fire");
	cl_sample_grenadelauncher_fire = S_LoadSample(
			"weapons/grenadelauncher/fire");
	cl_sample_rocketlauncher_fire = S_LoadSample("weapons/rocketlauncher/fire");
	cl_sample_hyperblaster_fire = S_LoadSample("weapons/hyperblaster/fire");
	cl_sample_hyperblaster_hit = S_LoadSample("weapons/hyperblaster/hit");
	cl_sample_lightning_fire = S_LoadSample("weapons/lightning/fire");
	cl_sample_lightning_fly = S_LoadSample("weapons/lightning/fly");
	cl_sample_lightning_discharge = S_LoadSample("weapons/lightning/discharge");
	cl_sample_railgun_fire = S_LoadSample("weapons/railgun/fire");
	cl_sample_bfg_fire = S_LoadSample("weapons/bfg/fire");
	cl_sample_bfg_hit = S_LoadSample("weapons/bfg/hit");

	cl_sample_explosion = S_LoadSample("weapons/common/explosion");
	cl_sample_teleport = S_LoadSample("world/teleport");
	cl_sample_respawn = S_LoadSample("world/respawn");
	cl_sample_sparks = S_LoadSample("world/sparks");
	cl_sample_rain = S_LoadSample("world/rain");
	cl_sample_snow = S_LoadSample("world/snow");

	Cl_LoadProgress(97);

	for (i = 0; i < 4; i++) {
		snprintf(name, sizeof(name), "weapons/machinegun/fire_%i", i + 1);
		cl_sample_machinegun_fire[i] = S_LoadSample(name);
	}

	for (i = 0; i < 3; i++) {
		snprintf(name, sizeof(name), "weapons/machinegun/hit_%i", i + 1);
		cl_sample_machinegun_hit[i] = S_LoadSample(name);
	}

	for (i = 0; i < 4; i++) {
		snprintf(name, sizeof(name), "#players/common/step_%i", i + 1);
		cl_sample_footsteps[i] = S_LoadSample(name);
	}

	Cl_LoadProgress(98);
}

/*
 * Cl_ParseMuzzleFlash
 */
void Cl_ParseMuzzleFlash(void) {
	int i, weapon;
	cl_entity_t *cent;

	i = Msg_ReadShort(&net_message);
	if (i < 1 || i >= MAX_EDICTS) {
		Com_Warn("Cl_ParseMuzzleFlash: Bad entity %d.\n", i);
		Msg_ReadByte(&net_message); // attempt to ignore cleanly
		return;
	}

	weapon = Msg_ReadByte(&net_message);
	cent = &cl.entities[i];

	switch (weapon) {
	case MZ_SHOTGUN:
		S_PlaySample(NULL, i, cl_sample_shotgun_fire, ATTN_NORM);
		Cl_SmokeFlash(&cent->current);
		break;
	case MZ_SSHOTGUN:
		S_PlaySample(NULL, i, cl_sample_supershotgun_fire, ATTN_NORM);
		Cl_SmokeFlash(&cent->current);
		break;
	case MZ_MACHINEGUN:
		S_PlaySample(NULL, i, cl_sample_machinegun_fire[rand() % 4], ATTN_NORM);
		if (rand() & 1)
			Cl_SmokeFlash(&cent->current);
		break;
	case MZ_ROCKET:
		S_PlaySample(NULL, i, cl_sample_rocketlauncher_fire, ATTN_NORM);
		Cl_SmokeFlash(&cent->current);
		break;
	case MZ_GRENADE:
		S_PlaySample(NULL, i, cl_sample_grenadelauncher_fire, ATTN_NORM);
		Cl_SmokeFlash(&cent->current);
		break;
	case MZ_HYPERBLASTER:
		S_PlaySample(NULL, i, cl_sample_hyperblaster_fire, ATTN_NORM);
		Cl_EnergyFlash(&cent->current, 105, 8);
		break;
	case MZ_LIGHTNING:
		S_PlaySample(NULL, i, cl_sample_lightning_fire, ATTN_NORM);
		break;
	case MZ_RAILGUN:
		S_PlaySample(NULL, i, cl_sample_railgun_fire, ATTN_NORM);
		break;
	case MZ_BFG:
		S_PlaySample(NULL, i, cl_sample_bfg_fire, ATTN_NORM);
		Cl_EnergyFlash(&cent->current, 200, 64);
		break;
	case MZ_LOGOUT:
		S_PlaySample(NULL, i, cl_sample_teleport, ATTN_NORM);
		Cl_LogoutEffect(cent->current.origin);
		break;
	default:
		break;
	}
}

/*
 *
 * PARTICLE MANAGEMENT
 *
 */

static r_particle_t *active_particles, *free_particles;
r_particle_t particles[MAX_PARTICLES];

#define WEATHER_PARTICLES 2048
static int weather_particles;

/*
 * Cl_ClearParticle
 */
static void Cl_ClearParticle(r_particle_t *p) {

	if (p->type == PARTICLE_WEATHER)
		weather_particles--;

	memset(p, 0, sizeof(r_particle_t));

	p->type = PARTICLE_NORMAL;
	p->image = r_particle_image;
	p->scale = 1.0;
	p->blend = GL_ONE;

	p->next = free_particles;
	free_particles = p;
}

/*
 * Cl_ClearParticles
 */
static void Cl_ClearParticles(void) {
	int i;

	for (i = 0; i < MAX_PARTICLES; i++) {
		Cl_ClearParticle(&particles[i]);
		particles[i].next = i < MAX_PARTICLES - 1 ? &particles[i + 1] : NULL;
	}

	free_particles = &particles[0];
	active_particles = NULL;
}

/*
 * Cl_TracerEffect
 */
void Cl_TracerEffect(const vec3_t start, const vec3_t end) {
	r_particle_t *p;
	float v;

	if (!(p = Cl_AllocParticle()))
		return;

	p->type = PARTICLE_BEAM;
	p->image = r_beam_image;

	p->scale = 1.0;

	VectorCopy(start, p->org);
	VectorCopy(end, p->end);

	VectorSubtract(end, start, p->vel);
	VectorScale(p->vel, 2.0, p->vel);

	v = VectorNormalize(p->vel);
	VectorScale(p->vel, v < 1000.0 ? v : 1000.0, p->vel);

	p->alpha = 0.2;
	p->alpha_vel = -0.4;

	p->color = 14;
}

/*
 * Cl_BulletEffect
 */
void Cl_BulletEffect(const vec3_t org, const vec3_t dir) {
	static unsigned int last_ric_time;
	r_particle_t *p;
	r_sustained_light_t s;
	vec3_t v;

	if (!(p = Cl_AllocParticle()))
		return;

	p->type = PARTICLE_DECAL;
	VectorAdd(org, dir, p->org);

	VectorScale(dir, -1.0, v);
	VectorAngles(v, p->dir);
	p->dir[ROLL] = rand() % 360;

	p->image = r_bullet_images[rand() & (NUM_BULLET_IMAGES - 1)];

	p->color = 0 + (rand() & 1);

	p->scale = 1.5;

	p->alpha = 2.0;
	p->alpha_vel = -1.0 / (2.0 + frand() * 0.3);

	p->blend = GL_ONE_MINUS_SRC_ALPHA;

	if (!(p = Cl_AllocParticle()))
		return;

	VectorCopy(org, p->org);

	VectorScale(dir, 200.0, p->vel);

	if (p->vel[2] < 100.0) // deflect up a bit
		p->vel[2] = 100.0;

	p->accel[2] -= 4.0 * PARTICLE_GRAVITY;

	p->image = r_spark_image;

	p->color = 221 + (rand() & 7);

	p->scale = 1.5;
	p->scale_vel = -4.0;

	p->alpha = 1.0;
	p->alpha_vel = -1.0 / (0.7 + crand() * 0.1);

	VectorAdd(org, dir, s.light.origin);
	s.light.radius = 20.0;
	VectorSet(s.light.color, 0.5, 0.3, 0.2);
	s.sustain = 0.25;

	R_AddSustainedLight(&s);

	if (cl.time < last_ric_time)
		last_ric_time = 0;

	if (cl.time - last_ric_time > 300) {
		S_PlaySample(org, -1, cl_sample_machinegun_hit[rand() % 3], ATTN_NORM);
		last_ric_time = cl.time;
	}
}

/*
 * Cl_BurnEffect
 */
void Cl_BurnEffect(const vec3_t org, const vec3_t dir, int scale) {
	r_particle_t *p;
	vec3_t v;

	if (!(p = Cl_AllocParticle()))
		return;

	p->image = r_burn_image;
	p->color = 0 + (rand() & 1);
	p->type = PARTICLE_DECAL;
	p->scale = scale;

	VectorScale(dir, -1, v);
	VectorAngles(v, p->dir);
	p->dir[ROLL] = rand() % 360;
	VectorAdd(org, dir, p->org);

	p->alpha = 2.0;
	p->alpha_vel = -1.0 / (2.0 + frand() * 0.3);

	p->blend = GL_ONE_MINUS_SRC_ALPHA;
}

/*
 * Cl_BloodEffect
 */
void Cl_BloodEffect(const vec3_t org, const vec3_t dir, int count) {
	int i, j;
	r_particle_t *p;
	float d;

	for (i = 0; i < count; i++) {

		if (!(p = Cl_AllocParticle()))
			return;

		p->color = 232 + (rand() & 7);
		p->image = r_blood_image;
		p->scale = 6.0;

		d = rand() & 31;
		for (j = 0; j < 3; j++) {
			p->org[j] = org[j] + ((rand() & 7) - 4.0) + d * dir[j];
			p->vel[j] = crand() * 20.0;
		}
		p->org[2] += 16.0 * PM_SCALE;

		p->accel[0] = p->accel[1] = 0.0;
		p->accel[2] = PARTICLE_GRAVITY / 4.0;

		p->alpha = 1.0;
		p->alpha_vel = -1.0 / (0.5 + frand() * 0.3);
	}
}

#define GIB_STREAM_DIST 180.0
#define GIB_STREAM_COUNT 25

/*
 * Cl_GibEffect
 */
void Cl_GibEffect(const vec3_t org, int count) {
	r_particle_t *p;
	vec3_t o, v, tmp;
	c_trace_t tr;
	float dist;
	int i, j;

	// if a player has died underwater, emit some bubbles
	if (Cm_PointContents(org, r_world_model->first_node) & MASK_WATER) {
		VectorCopy(org, tmp);
		tmp[2] += 64.0;

		Cl_BubbleTrail(org, tmp, 16.0);
	}

	for (i = 0; i < count; i++) {

		// set the origin and velocity for each gib stream
		VectorSet(o, crand() * 8.0, crand() * 8.0, 8.0 + crand() * 12.0);
		VectorAdd(o, org, o);

		VectorSet(v, crand(), crand(), 0.2 + frand());

		dist = GIB_STREAM_DIST;
		VectorMA(o, dist, v, tmp);

		tr = Cm_BoxTrace(o, tmp, vec3_origin, vec3_origin, 0, MASK_SHOT);
		dist = GIB_STREAM_DIST * tr.fraction;

		for (j = 1; j < GIB_STREAM_COUNT; j++) {

			if (!(p = Cl_AllocParticle()))
				return;

			p->color = 232 + (rand() & 7);
			p->image = r_blood_image;

			VectorCopy(o, p->org);

			VectorScale(v, dist * ((float)j / GIB_STREAM_COUNT), p->vel);
			p->vel[0] += crand() * 2.0;
			p->vel[1] += crand() * 2.0;
			p->vel[2] += 100.0;

			p->accel[0] = p->accel[1] = 0.0;
			p->accel[2] = -PARTICLE_GRAVITY * 2.0;

			p->scale = 6.0 + frand();
			p->scale_vel = -6.0 + crand();

			p->alpha = 1.0;
			p->alpha_vel = -1.0 / (2.0 + frand() * 0.3);
		}
	}
}

/*
 * Cl_SparksEffect
 */
void Cl_SparksEffect(const vec3_t org, const vec3_t dir, int count) {
	r_particle_t *p;
	r_sustained_light_t s;
	int i, j;

	for (i = 0; i < count; i++) {

		if (!(p = Cl_AllocParticle()))
			return;

		p->image = r_spark_image;

		p->color = 0xe0 + (rand() & 7);

		VectorCopy(org, p->org);
		VectorCopy(dir, p->vel);

		for (j = 0; j < 3; j++) {
			p->org[j] += crand() * 4.0;
			p->vel[j] += crand() * 4.0;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;

		p->scale = 2.5;
		p->scale_vel = -4.0;

		p->alpha = 1.5;
		p->alpha_vel = -1.0 / (0.5 + frand() * 0.3);
	}

	VectorCopy(org, s.light.origin);
	s.light.radius = 80.0;
	VectorSet(s.light.color, 0.7, 0.5, 0.5);
	s.sustain = 0.65;

	R_AddSustainedLight(&s);

	S_PlaySample(org, -1, cl_sample_sparks, ATTN_STATIC);
}

/*
 * Cl_TeleporterTrail
 */
void Cl_TeleporterTrail(const vec3_t org, cl_entity_t *cent) {
	int i;
	r_particle_t *p;

	if (cent) { // honor a slightly randomized time interval

		if (cent->time > cl.time)
			return;

		S_PlaySample(NULL, cent->current.number, cl_sample_respawn, ATTN_IDLE);

		cent->time = cl.time + 1000 + (2000 * frand());
	}

	for (i = 0; i < 4; i++) {

		if (!(p = Cl_AllocParticle()))
			return;

		p->type = PARTICLE_SPLASH;
		p->image = r_teleport_image;
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
 * Cl_LogoutEffect
 */
void Cl_LogoutEffect(const vec3_t org) {
	Cl_GibEffect(org, 12);
}

/*
 * Cl_ItemRespawnEffect
 */
void Cl_ItemRespawnEffect(const vec3_t org) {
	r_particle_t *p;
	r_sustained_light_t s;
	int i, j;

	for (i = 0; i < 64; i++) {

		if (!(p = Cl_AllocParticle()))
			return;

		p->image = r_spark_image;
		p->scale_vel = 3.0;

		p->color = 110; // white

		p->org[0] = org[0] + crand() * 8.0;
		p->org[1] = org[1] + crand() * 8.0;
		p->org[2] = org[2] + 8 + frand() * 8.0;

		for (j = 0; j < 2; j++)
			p->vel[j] = crand() * 48.0;
		p->vel[2] = frand() * 48.0;

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY * 0.1;

		p->alpha = 1.0;
		p->alpha_vel = -1.5 + frand() * 0.5;
	}

	VectorCopy(org, s.light.origin);
	s.light.radius = 80.0;
	VectorSet(s.light.color, 0.9, 0.9, 0.9);
	s.sustain = 1.0;

	R_AddSustainedLight(&s);
}

/*
 * Cl_ItemPickupEffect
 */
void Cl_ItemPickupEffect(const vec3_t org) {
	r_particle_t *p;
	r_sustained_light_t s;
	int i, j;

	for (i = 0; i < 32; i++) {

		if (!(p = Cl_AllocParticle()))
			return;

		p->image = r_spark_image;
		p->scale_vel = 3.0;

		p->color = 110; // white

		p->org[0] = org[0] + crand() * 8.0;
		p->org[1] = org[1] + crand() * 8.0;
		p->org[2] = org[2] + 8 + crand() * 16.0;

		for (j = 0; j < 2; j++)
			p->vel[j] = crand() * 16.0;
		p->vel[2] = frand() * 128.0;

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = PARTICLE_GRAVITY * 0.2;

		p->alpha = 1.0;
		p->alpha_vel = -1.5 + frand() * 0.5;
	}

	VectorCopy(org, s.light.origin);
	s.light.radius = 80.0;
	VectorSet(s.light.color, 0.9, 1.0, 1.0);
	s.sustain = 1.0;

	R_AddSustainedLight(&s);
}

/*
 * Cl_ExplosionEffect
 */
void Cl_ExplosionEffect(const vec3_t org) {
	int i, j;
	r_particle_t *p;
	r_sustained_light_t s;

	if (!(p = Cl_AllocParticle()))
		return;

	p->image = r_explosion_image;

	p->scale = 1.0;
	p->scale_vel = 600.0;

	p->alpha = 1.0;
	p->alpha_vel = -4.0;

	p->color = 224;

	VectorCopy(org, p->org);

	if (!(p = Cl_AllocParticle()))
		return;

	i = CONTENTS_SLIME | CONTENTS_WATER;

	if (Cm_PointContents(org, r_world_model->first_node) & i)
		return;

	p->accel[2] = 20;

	p->image = r_smoke_image;

	p->type = PARTICLE_ROLL;
	p->roll = crand() * 100.0;

	p->scale = 12.0;
	p->scale_vel = 40.0;

	p->alpha = 1.0;
	p->alpha_vel = -1.0 / (1 + frand() * 0.6);

	p->color = rand() & 7;
	p->blend = GL_ONE;

	VectorCopy(org, p->org);
	p->org[2] += 10;

	for (j = 0; j < 3; j++) {
		p->vel[j] = crand();
	}

	VectorCopy(org, s.light.origin);
	s.light.radius = 100.0;
	VectorSet(s.light.color, 1.0, 0.5, 0.3);
	s.sustain = 1.0;

	R_AddSustainedLight(&s);

	S_PlaySample(org, -1, cl_sample_explosion, ATTN_NORM);
}

/*
 * Cl_SmokeTrail
 */
void Cl_SmokeTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent) {
	r_particle_t *p;
	boolean_t stationary;
	int j, c;

	if (r_view.render_mode == render_mode_pro)
		return;

	stationary = false;

	if (ent) { // trails should be framerate independent

		if (ent->time > cl.time)
			return;

		// trails diminish for stationary entities (grenades)
		stationary
				= VectorCompare(ent->current.origin, ent->current.old_origin);

		if (stationary)
			ent->time = cl.time + 128;
		else
			ent->time = cl.time + 32;
	}

	c = CONTENTS_SLIME | CONTENTS_WATER;

	if (Cm_PointContents(end, r_world_model->first_node) & c) {
		Cl_BubbleTrail(start, end, 16.0);
		return;
	}

	if (!(p = Cl_AllocParticle()))
		return;

	p->accel[2] = 5.0;

	p->image = r_smoke_image;
	p->type = PARTICLE_ROLL;

	p->scale = 2.0;
	p->scale_vel = 20.0;

	p->alpha = 1.0;
	p->alpha_vel = -1.0 / (1 + frand() * 0.6);

	p->color = rand() & 7;
	p->blend = GL_ONE;

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
 * Cl_SmokeFlash
 */
void Cl_SmokeFlash(entity_state_t *ent) {
	r_particle_t *p;
	r_sustained_light_t s;
	vec3_t forward, right, org, org2;
	c_trace_t tr;
	float dist;
	int j, c;

	// project the puff just in front of the entity
	AngleVectors(ent->angles, forward, right, NULL);
	VectorMA(ent->origin, 30.0, forward, org);
	VectorMA(org, 6.0, right, org);

	tr = Cm_BoxTrace(ent->origin, org, vec3_origin, vec3_origin, 0, MASK_SHOT);

	if (tr.fraction < 1.0) { // firing near a wall, back it up
		VectorSubtract(ent->origin, tr.end, org);
		VectorScale(org, 0.75, org);

		VectorAdd(ent->origin, org, org);
	}

	// and adjust for ducking (this is a hack)
	dist = ent->solid == 8290 ? -2.0 : 20.0;
	org[2] += dist;

	VectorCopy(org, s.light.origin);
	s.light.radius = 80.0;
	VectorSet(s.light.color, 0.8, 0.7, 0.5);
	s.sustain = 0.3;

	R_AddSustainedLight(&s);

	c = CONTENTS_SLIME | CONTENTS_WATER;

	if (Cm_PointContents(ent->origin, r_world_model->first_node) & c) {
		VectorMA(ent->origin, 40.0, forward, org2);
		Cl_BubbleTrail(org, org2, 10.0);
		return;
	}

	if (!(p = Cl_AllocParticle()))
		return;

	p->accel[2] = 5.0;

	p->image = r_smoke_image;
	p->type = PARTICLE_ROLL;

	p->scale = 4.0;
	p->scale_vel = 24.0;

	p->alpha = 0.8;
	p->alpha_vel = -1.0;

	p->color = rand() & 7;
	p->blend = GL_ONE;

	VectorCopy(org, p->org);

	for (j = 0; j < 2; j++) {
		p->vel[j] = crand();
	}
	p->vel[2] = 10.0;

	p->roll = crand() * 100.0; // rotation
}

/*
 * Cl_FlameTrail
 */
void Cl_FlameTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent) {
	r_particle_t *p;
	int j, c;

	if (ent) { // trails should be framerate independent

		if (ent->time > cl.time)
			return;

		ent->time = cl.time + 8;
	}

	c = CONTENTS_SLIME | CONTENTS_WATER;

	if (Cm_PointContents(end, r_world_model->first_node) & c) {
		Cl_BubbleTrail(start, end, 10.0);
		return;
	}

	if (!(p = Cl_AllocParticle()))
		return;

	p->accel[2] = 15.0;

	p->image = r_flame_image;
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
 * Cl_SteamTrail
 */
void Cl_SteamTrail(const vec3_t org, const vec3_t vel, cl_entity_t *ent) {
	r_particle_t *p;
	vec3_t end;
	int j, c;

	VectorAdd(org, vel, end);

	if (ent) { // trails should be framerate independent

		if (ent->time > cl.time)
			return;

		ent->time = cl.time + 8;
	}

	c = CONTENTS_SLIME | CONTENTS_WATER;

	if (Cm_PointContents(org, r_world_model->first_node) & c) {
		Cl_BubbleTrail(org, end, 10.0);
		return;
	}

	if (!(p = Cl_AllocParticle()))
		return;

	p->image = r_steam_image;
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
 * Cl_BubbleTrail
 */
void Cl_BubbleTrail(const vec3_t start, const vec3_t end, float density) {
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

		if (!(p = Cl_AllocParticle()))
			return;

		p->image = r_bubble_image;
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
 * Cl_EnergyTrail
 */
void Cl_EnergyTrail(cl_entity_t *ent, float radius, int color) {
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

	ltime = (float) cl.time / 300.0;

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

		if (!(p = Cl_AllocParticle()))
			return;

		p->scale = 0.75;
		p->scale_vel = 800.0;

		dist = sin(ltime + i) * radius;

		for (c = 0; c < 3; c++) {
			// project the origin outward, adding in angular velocity
			p->org[c] = ent->current.origin[c] + (approximate_normals[i][c]
					* dist) + forward[c] * 16.0;
		}

		VectorSubtract(p->org, ent->current.origin, v);
		dist = VectorLength(v) / (3.0 * radius);
		p->color = color + dist * 7.0;

		p->alpha = 1.0 - dist;
		p->alpha_vel = -100.0;

		p->vel[0] = p->vel[1] = p->vel[2] = 2.0 * crand();
	}

	// add a bubble trail if appropriate
	if (ent->time > cl.time)
		return;

	ent->time = cl.time + 8;

	c = CONTENTS_SLIME | CONTENTS_WATER;

	if (Cm_PointContents(ent->current.origin, r_world_model->first_node) & c)
		Cl_BubbleTrail(ent->prev.origin, ent->current.origin, 1.0);
}

/*
 * Cl_EnergyFlash
 */
void Cl_EnergyFlash(entity_state_t *ent, int color, int count) {
	r_particle_t *p;
	r_sustained_light_t s;
	vec3_t forward, right, org, org2;
	c_trace_t tr;
	float dist;
	int i, j, c;

	// project the particles just in front of the entity
	AngleVectors(ent->angles, forward, right, NULL);
	VectorMA(ent->origin, 30.0, forward, org);
	VectorMA(org, 6.0, right, org);

	tr = Cm_BoxTrace(ent->origin, org, vec3_origin, vec3_origin, 0, MASK_SHOT);

	if (tr.fraction < 1.0) { // firing near a wall, back it up
		VectorSubtract(ent->origin, tr.end, org);
		VectorScale(org, 0.75, org);

		VectorAdd(ent->origin, org, org);
	}

	// and adjust for ducking (this is a hack)
	dist = ent->solid == 8290 ? -2.0 : 20.0;
	org[2] += dist;

	VectorCopy(org, s.light.origin);
	s.light.radius = 80.0;
	VectorSet(s.light.color, 0.7, 0.9, 0.9);
	s.sustain = 0.3;

	R_AddSustainedLight(&s);

	c = CONTENTS_SLIME | CONTENTS_WATER;

	if (Cm_PointContents(ent->origin, r_world_model->first_node) & c) {
		VectorMA(ent->origin, 40.0, forward, org2);
		Cl_BubbleTrail(org, org2, 10.0);
		return;
	}

	for (i = 0; i < count; i++) {

		if (!(p = Cl_AllocParticle()))
			return;

		p->accel[2] = -PARTICLE_GRAVITY;

		p->alpha = 1.0;
		p->alpha_vel = -2.0;

		p->scale_vel = 4.0;

		p->color = color + (rand() & 15);

		for (j = 0; j < 3; j++) {
			p->org[j] = org[j] + 8.0 * crand();
			p->vel[j] = 128.0 * crand();
		}
	}
}

/*
 * Cl_GrenadeTrail
 */
void Cl_GrenadeTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent) {
	Cl_SmokeTrail(start, end, ent);
}

/*
 * Cl_RocketTrail
 */
void Cl_RocketTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent) {
	r_corona_t c;
	r_light_t l;

	Cl_SmokeTrail(start, end, ent);

	VectorCopy(end, c.org);
	c.radius = 3.0;
	c.flicker = 0.25;
	VectorSet(c.color, 1.0, 0.5, 0.3);

	R_AddCorona(&c);

	VectorCopy(end, l.origin);
	l.radius = 120.0;
	VectorSet(l.color, 1.0, 0.5, 0.3);

	R_AddLight(&l);
}

/*
 * Cl_HyperblasterTrail
 */
void Cl_HyperblasterTrail(cl_entity_t *ent) {
	r_corona_t c;
	r_light_t l;

	Cl_EnergyTrail(ent, 8.0, 107);

	VectorCopy(ent->current.origin, c.org);
	c.radius = 12.0;
	c.flicker = 0.15;
	VectorSet(c.color, 0.4, 0.7, 1.0);

	R_AddCorona(&c);

	VectorCopy(ent->current.origin, l.origin);
	l.radius = 100.0;
	VectorSet(l.color, 0.4, 0.7, 1.0);

	R_AddLight(&l);
}

/*
 * Cl_HyperblasterEffect
 */
void Cl_HyperblasterEffect(const vec3_t org) {
	r_sustained_light_t s;

	VectorCopy(org, s.light.origin);
	s.light.radius = 80.0;
	VectorSet(s.light.color, 0.4, 0.7, 1.0);
	s.sustain = 0.25;

	R_AddSustainedLight(&s);

	S_PlaySample(org, -1, cl_sample_hyperblaster_hit, ATTN_NORM);
}

/*
 * Cl_LightningEffect
 */
void Cl_LightningEffect(const vec3_t org) {
	r_sustained_light_t s;
	vec3_t tmp;
	int i, j;

	for (i = 0; i < 40; i++) {

		VectorCopy(org, tmp);

		for (j = 0; j < 3; j++)
			tmp[j] = tmp[j] + (rand() % 96) - 48;

		Cl_BubbleTrail(org, tmp, 4.0);
	}

	VectorCopy(org, s.light.origin);
	s.light.radius = 160.0;
	VectorSet(s.light.color, 0.6, 0.6, 1.0);
	s.sustain = 0.75;

	R_AddSustainedLight(&s);

	S_PlaySample(org, -1, cl_sample_lightning_discharge, ATTN_NORM);
}

/*
 * Cl_LightningTrail
 */
void Cl_LightningTrail(const vec3_t start, const vec3_t end) {
	r_particle_t *p;
	r_light_t l;
	vec3_t dir, delta, pos;
	float dist;

	S_LoopSample(start, cl_sample_lightning_fly);
	S_LoopSample(end, cl_sample_lightning_fly);

	VectorSubtract(start, end, dir);
	dist = VectorNormalize(dir);

	VectorScale(dir, -48.0, delta);

	VectorSet(pos, crand() * 0.5, crand() * 0.5, crand() * 0.5);
	VectorAdd(pos, start, pos);

	while (dist > 0.0) {

		if (!(p = Cl_AllocParticle()))
			return;

		p->type = PARTICLE_BEAM;
		p->image = r_lightning_image;

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

	R_AddLight(&l);

	VectorMA(end, 12.0, dir, l.origin);
	R_AddLight(&l);
}

/*
 * Cl_RailEffect
 */
void Cl_RailEffect(const vec3_t start, const vec3_t end, int flags, int color) {
	vec3_t vec, move;
	float len;
	r_particle_t *p;
	r_sustained_light_t s;
	int i;

	VectorCopy(start, s.light.origin);
	s.light.radius = 100.0;
	Img_ColorFromPalette(color, s.light.color);
	s.sustain = 0.75;

	R_AddSustainedLight(&s);

	if (!(p = Cl_AllocParticle()))
		return;

	// draw the core with a beam
	p->type = PARTICLE_BEAM;
	p->image = r_beam_image;
	p->scale = 3.0;

	VectorCopy(start, p->org);
	VectorCopy(end, p->end);

	p->alpha = 0.75;
	p->alpha_vel = -0.75;

	// white cores for some colors, shifted for others
	p->color = (color > 239 || color == 187 ? 216 : color + 6);

	// and the spiral with normal parts
	VectorCopy(start, move);

	VectorSubtract(end, start, vec);
	len = VectorNormalize(vec);

	for (i = 0; i < len; i += 24) {

		if (!(p = Cl_AllocParticle()))
			return;

		p->type = PARTICLE_ROLL;
		p->roll = crand() * 300.0;

		VectorMA(start, i, vec, p->org);

		VectorSet(p->vel, 0.0, 0.0, 2.0);

		p->image = r_rail_trail_image;

		p->alpha = 1.0;
		p->alpha_vel = -1.0;

		p->scale = 3.0 + crand() * 0.3;
		p->scale_vel = 8.0 + crand() * 0.3;

		p->color = color;

		// check for bubble trail
		if (i && (Cm_PointContents(move, r_world_model->first_node)
				& (CONTENTS_SLIME | CONTENTS_WATER)) ) {
			Cl_BubbleTrail(move, p->org, 16.0);
		}

		VectorCopy(p->org, move);
	}

	// check for explosion effect on solids
	if (flags & SURF_SKY)
		return;

	if (!(p = Cl_AllocParticle()))
		return;

	p->image = r_explosion_image;

	p->scale = 1.0;
	p->scale_vel = 400.0;

	p->alpha = 2.0;
	p->alpha_vel = -10.0;

	p->color = color;

	VectorCopy(end, p->org);

	VectorMA(end, -12.0, vec, s.light.origin);
	s.light.radius = 120.0;
	s.sustain = 1.25;

	R_AddSustainedLight(&s);
}

/*
 * Cl_BfgTrail
 */
void Cl_BfgTrail(cl_entity_t *ent) {
	r_corona_t c;
	r_light_t l;

	Cl_EnergyTrail(ent, 16.0, 206);

	VectorCopy(ent->current.origin, c.org);
	c.radius = 24.0;
	c.flicker = 0.05;
	VectorSet(c.color, 0.4, 1.0, 0.4);

	R_AddCorona(&c);

	VectorCopy(ent->current.origin, l.origin);
	l.radius = 120.0;
	VectorSet(l.color, 0.4, 1.0, 0.4);

	R_AddLight(&l);
}

/*
 * Cl_BfgEffect
 */
void Cl_BfgEffect(const vec3_t org) {
	r_particle_t *p;
	r_sustained_light_t s;
	int i;

	for (i = 0; i < 4; i++) {

		if (!(p = Cl_AllocParticle()))
			return;

		p->image = r_explosion_image;

		p->scale = 1.0;
		p->scale_vel = 200.0 * (i + 1);

		p->alpha = 1.0;
		p->alpha_vel = -3.0;

		p->color = 201;

		VectorCopy(org, p->org);
	}

	VectorCopy(org, s.light.origin);
	s.light.radius = 200.0;
	VectorSet(s.light.color, 0.8, 1.0, 0.5);
	s.sustain = 1.0;

	R_AddSustainedLight(&s);

	S_PlaySample(org, -1, cl_sample_bfg_hit, ATTN_NORM);
}

/*
 * Cl_TeleporterEffect
 */
static void Cl_TeleporterEffect(const vec3_t org) {
	Cl_TeleporterTrail(org, NULL);
}

/*
 * Cl_EntityEvent
 */
void Cl_EntityEvent(entity_state_t *ent) {
	switch (ent->event) {
	case EV_ITEM_RESPAWN:
		S_PlaySample(NULL, ent->number, cl_sample_respawn, ATTN_IDLE);
		Cl_ItemRespawnEffect(ent->origin);
		break;
	case EV_ITEM_PICKUP:
		Cl_ItemPickupEffect(ent->origin);
		break;
	case EV_TELEPORT:
		S_PlaySample(NULL, ent->number, cl_sample_teleport, ATTN_IDLE);
		Cl_TeleporterEffect(ent->origin);
		break;
	case EV_CLIENT_JUMP:
		S_PlaySample(NULL, ent->number,
				S_LoadSample(va("*jump_%d", rand() % 5 + 1)), ATTN_NORM);
		break;
	case EV_CLIENT_FOOTSTEP:
		S_PlaySample(NULL, ent->number, cl_sample_footsteps[rand() & 3],
				ATTN_NORM);
		break;
	case EV_CLIENT_LAND:
		S_PlaySample(NULL, ent->number, S_LoadSample("*land_1"), ATTN_NORM);
		break;
	case EV_CLIENT_FALL:
		S_PlaySample(NULL, ent->number, S_LoadSample("*fall_2"), ATTN_NORM);
		break;
	case EV_CLIENT_FALL_FAR:
		S_PlaySample(NULL, ent->number, S_LoadSample("*fall_1"), ATTN_NORM);
		break;
	default:
		break;
	}
}

/*
 * Cl_WeatherEffects
 */
static void Cl_WeatherEffects(void) {
	int j, k, max;
	vec3_t start, end;
	c_trace_t tr;
	float ceiling;
	r_particle_t *p;
	s_sample_t *s;

	if (!cl_weather->value)
		return;

	if (!(r_view.weather & (WEATHER_RAIN | WEATHER_SNOW)))
		return;

	p = NULL; // so that we add the right amount

	// never attempt to add more than a set amount per frame
	max = 50 * cl_weather->value;

	k = 0;
	while (weather_particles < WEATHER_PARTICLES && k++ < max) {

		VectorCopy(r_view.origin, start);
		start[0] = start[0] + (rand() % 2048) - 1024;
		start[1] = start[1] + (rand() % 2048) - 1024;

		VectorCopy(start, end);
		end[2] += 8192;

		// trace up looking for sky
		tr = Cm_BoxTrace(start, end, vec3_origin, vec3_origin, 0, MASK_SHOT);

		if (!(tr.surface->flags & SURF_SKY))
			continue;

		if (!(p = Cl_AllocParticle()))
			break;

		p->type = PARTICLE_WEATHER;

		// drop down somewhere between sky and player
		ceiling = tr.end[2] > start[2] + 1024 ? start[2] + 1024 : tr.end[2];
		tr.end[2] = tr.end[2] - ((ceiling - start[2]) * frand());

		VectorCopy(tr.end, p->org);
		p->org[2] -= 1;

		// trace down look for ground
		VectorCopy(start, end);
		end[2] -= 8192;

		tr = Cm_BoxTrace(p->org, end, vec3_origin, vec3_origin, 0, MASK_ALL);

		if (!tr.surface) // this shouldn't happen
			VectorCopy(start, p->end);
		else
			VectorCopy(tr.end, p->end);

		// setup the particles
		if (r_view.weather & WEATHER_RAIN) {
			p->image = r_rain_image;
			p->vel[2] = -800;
			p->alpha = 0.4;
			p->color = 8;
			p->scale = 6;
		} else if (r_view.weather & WEATHER_SNOW) {
			p->image = r_snow_image;
			p->vel[2] = -120;
			p->alpha = 0.6;
			p->alpha_vel = frand() * -1;
			p->color = 8;
			p->scale = 1.5;
		} else
			// undefined
			continue;

		for (j = 0; j < 2; j++) {
			p->vel[j] = crand() * 2;
			p->accel[j] = crand() * 2;
		}
		weather_particles++;
	}

	// add an appropriate looping sound
	if (r_view.weather & WEATHER_RAIN)
		s = cl_sample_rain;
	else if (r_view.weather & WEATHER_SNOW)
		s = cl_sample_snow;
	else
		s = NULL;

	if (!s)
		return;

	S_LoopSample(r_view.origin, s);
}

/*
 * Cl_AddParticles
 */
void Cl_AddParticles(void) {
	r_particle_t *p, *next;
	r_particle_t *active, *tail;
	float time;
	int i;

	if (!cl_add_particles->value)
		return;

	// add weather effects after all other effects for the frame
	Cl_WeatherEffects();

	active = NULL;
	tail = NULL;

	for (p = active_particles; p; p = next) {
		next = p->next;

		time = (cl.time - p->time) * 0.001;

		p->current_alpha = p->alpha + time * p->alpha_vel;
		p->current_scale = p->scale + time * p->scale_vel;

		// free up particles that have faded or shrunk
		if (p->current_alpha <= 0 || p->current_scale <= 0) {
			Cl_ClearParticle(p);
			continue;
		}

		if (p->current_alpha > 1.0) // clamp alpha
			p->current_alpha = 1.0;

		for (i = 0; i < 3; i++) { // update origin and end
			p->current_org[i] = p->org[i] + p->vel[i] * time + p->accel[i]
					* time * time;
			p->current_end[i] = p->end[i] + p->vel[i] * time + p->accel[i]
					* time * time;
		}

		// free up weather particles that have hit the ground
		if (p->type == PARTICLE_WEATHER && (p->current_org[2] <= p->end[2])) {
			Cl_ClearParticle(p);
			continue;
		}

		p->next = NULL;
		if (!tail)
			active = tail = p;
		else {
			tail->next = p;
			tail = p;
		}

		R_AddParticle(p);
	}

	active_particles = active;
}

/*
 * Cl_AllocParticle
 */
r_particle_t *Cl_AllocParticle(void) {
	r_particle_t *p;

	if (!cl_add_particles->integer || !free_particles)
		return NULL;

	p = free_particles;
	free_particles = p->next;
	p->next = active_particles;
	active_particles = p;

	p->time = cl.time;
	return p;
}

/*
 * Cl_ClearEffects
 */
void Cl_ClearEffects(void) {
	Cl_ClearParticles();
}
