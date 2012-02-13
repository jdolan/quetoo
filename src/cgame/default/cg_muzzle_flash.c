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
 * Cg_EnergyFlash
 */
static void Cg_EnergyFlash(const entity_state_t *ent, int color, int count) {
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

	tr = cgi.Trace(ent->origin, org, 0.0, MASK_SHOT);

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

	cgi.AddSustainedLight(&s);

	c = CONTENTS_SLIME | CONTENTS_WATER;

	if (cgi.PointContents(ent->origin) & c) {
		VectorMA(ent->origin, 40.0, forward, org2);
		Cg_BubbleTrail(org, org2, 10.0);
		return;
	}

	for (i = 0; i < count; i++) {

		if (!(p = Cg_AllocParticle()))
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
 * Cg_SmokeFlash
 */
static void Cg_SmokeFlash(const entity_state_t *ent) {
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

	tr = cgi.Trace(ent->origin, org, 0.0, MASK_SHOT);

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

	cgi.AddSustainedLight(&s);

	c = CONTENTS_SLIME | CONTENTS_WATER;

	if (cgi.PointContents(ent->origin) & c) {
		VectorMA(ent->origin, 40.0, forward, org2);
		Cg_BubbleTrail(org, org2, 10.0);
		return;
	}

	if (!(p = Cg_AllocParticle()))
		return;

	p->accel[2] = 5.0;

	p->image = cg_particle_smoke;
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
 * Cg_LogoutFlash
 *
 * FIXME: This should be a tentity instead; would make more sense.
 */
static void Cg_LogoutFlash(const vec3_t org) {
	Cg_GibEffect(org, 12);
}

/*
 * Cg_ParseMuzzleFlash
 */
void Cg_ParseMuzzleFlash(void) {
	int c;

	const unsigned short ent_num = cgi.ReadShort();

	if (ent_num < 1 || ent_num >= MAX_EDICTS) {
		cgi.Warn("Cg_ParseMuzzleFlash: Bad entity %u.\n", ent_num);
		cgi.ReadByte(); // attempt to ignore cleanly
		return;
	}

	const cl_entity_t *cent = &cgi.client->entities[ent_num];
	const byte flash = cgi.ReadByte();

	switch (flash) {
	case MZ_BLASTER:
		c = cgi.ReadByte();
		cgi.PlaySample(NULL, ent_num, cg_sample_blaster_fire, ATTN_NORM);
		Cg_EnergyFlash(&cent->current, c, 4);
		break;
	case MZ_SHOTGUN:
		cgi.PlaySample(NULL, ent_num, cg_sample_shotgun_fire, ATTN_NORM);
		Cg_SmokeFlash(&cent->current);
		break;
	case MZ_SSHOTGUN:
		cgi.PlaySample(NULL, ent_num, cg_sample_supershotgun_fire, ATTN_NORM);
		Cg_SmokeFlash(&cent->current);
		break;
	case MZ_MACHINEGUN:
		cgi.PlaySample(NULL, ent_num, cg_sample_machinegun_fire[rand() % 4],
				ATTN_NORM);
		if (rand() & 1)
			Cg_SmokeFlash(&cent->current);
		break;
	case MZ_ROCKET:
		cgi.PlaySample(NULL, ent_num, cg_sample_rocketlauncher_fire, ATTN_NORM);
		Cg_SmokeFlash(&cent->current);
		break;
	case MZ_GRENADE:
		cgi.PlaySample(NULL, ent_num, cg_sample_grenadelauncher_fire, ATTN_NORM);
		Cg_SmokeFlash(&cent->current);
		break;
	case MZ_HYPERBLASTER:
		cgi.PlaySample(NULL, ent_num, cg_sample_hyperblaster_fire, ATTN_NORM);
		Cg_EnergyFlash(&cent->current, 105, 8);
		break;
	case MZ_LIGHTNING:
		cgi.PlaySample(NULL, ent_num, cg_sample_lightning_fire, ATTN_NORM);
		break;
	case MZ_RAILGUN:
		cgi.PlaySample(NULL, ent_num, cg_sample_railgun_fire, ATTN_NORM);
		break;
	case MZ_BFG:
		cgi.PlaySample(NULL, ent_num, cg_sample_bfg_fire, ATTN_NORM);
		Cg_EnergyFlash(&cent->current, 200, 64);
		break;
	case MZ_LOGOUT:
		cgi.PlaySample(NULL, ent_num, cg_sample_teleport, ATTN_NORM);
		Cg_LogoutFlash(cent->current.origin);
		break;
	default:
		break;
	}
}

