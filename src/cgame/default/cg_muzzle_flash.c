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
 * @return True if the entity is ducking, false otherwise.
 */
static _Bool Cg_IsDucking(const entity_state_t *ent) {

	vec3_t mins, maxs;
	UnpackBounds(ent->bounds, mins, maxs);

	const vec_t standing_height = (PM_MAXS[2] - PM_MINS[2]) * PM_SCALE;
	const vec_t height = maxs[2] - mins[2];

	return standing_height - height > PM_STOP_EPSILON;
}

/**
 * @brief
 */
static void Cg_EnergyFlash(const entity_state_t *ent, uint8_t color) {
	r_sustained_light_t s;
	vec3_t forward, right, org, org2;

	// project the flash just in front of the entity
	AngleVectors(ent->angles, forward, right, NULL );
	VectorMA(ent->origin, 30.0, forward, org);
	VectorMA(org, 6.0, right, org);

	const cm_trace_t tr = cgi.Trace(ent->origin, org, NULL, NULL, 0, MASK_CLIP_PROJECTILE);

	if (tr.fraction < 1.0) { // firing near a wall, back it up
		VectorSubtract(ent->origin, tr.end, org);
		VectorScale(org, 0.75, org);

		VectorAdd(ent->origin, org, org);
	}

	// and adjust for ducking
	org[2] += Cg_IsDucking(ent) ? -2.0 : 20.0;

	VectorCopy(org, s.light.origin);
	s.light.radius = 80.0;
	cgi.ColorFromPalette(color, s.light.color);
	s.sustain = 450;

	cgi.AddSustainedLight(&s);

	if (cgi.PointContents(ent->origin) & MASK_LIQUID) {
		VectorMA(ent->origin, 40.0, forward, org2);
		Cg_BubbleTrail(org, org2, 10.0);
	}
}

/**
 * @brief
 */
static void Cg_SmokeFlash(const entity_state_t *ent) {
	cg_particle_t *p;
	r_sustained_light_t s;
	vec3_t forward, right, org, org2;

	// project the puff just in front of the entity
	AngleVectors(ent->angles, forward, right, NULL );
	VectorMA(ent->origin, 30.0, forward, org);
	VectorMA(org, 6.0, right, org);

	const cm_trace_t tr = cgi.Trace(ent->origin, org, NULL, NULL, 0, MASK_CLIP_PROJECTILE);

	if (tr.fraction < 1.0) { // firing near a wall, back it up
		VectorSubtract(ent->origin, tr.end, org);
		VectorScale(org, 0.75, org);

		VectorAdd(ent->origin, org, org);
	}

	// and adjust for ducking
	org[2] += Cg_IsDucking(ent) ? -2.0 : 20.0;

	VectorCopy(org, s.light.origin);
	s.light.radius = 80.0;
	VectorSet(s.light.color, 0.8, 0.7, 0.5);
	s.sustain = 300;

	cgi.AddSustainedLight(&s);

	if (cgi.PointContents(ent->origin) & MASK_LIQUID) {
		VectorMA(ent->origin, 40.0, forward, org2);
		Cg_BubbleTrail(org, org2, 10.0);
		return;
	}

	if (!(p = Cg_AllocParticle(PARTICLE_ROLL, cg_particles_smoke)))
		return;

	p->part.blend = GL_ONE;
	cgi.ColorFromPalette(Random() & 7, p->part.color);
	p->part.color[3] = 0.8;

	Vector4Set(p->color_vel, 0.0, 0.0, 0.0, -1.0);

	p->part.scale = 4.0;
	p->scale_vel = 24.0;

	p->part.roll = Randomc() * 100.0;

	VectorCopy(org, p->part.org);

	for (int32_t j = 0; j < 2; j++) {
		p->vel[j] = Randomc();
	}
	p->vel[2] = 10.0;

	p->accel[2] = 5.0;
}

/**
 * @brief FIXME: This should be a tentity instead; would make more sense.
 */
static void Cg_LogoutFlash(const vec3_t org) {
	Cg_GibEffect(org, 12);
}

/**
 * @brief
 */
void Cg_ParseMuzzleFlash(void) {
	int32_t c;

	const uint16_t ent_num = cgi.ReadShort();

	if (ent_num < 1 || ent_num >= MAX_ENTITIES) {
		cgi.Warn("Bad entity %u\n", ent_num);
		cgi.ReadByte(); // attempt to ignore cleanly
		return;
	}

	const cl_entity_t *cent = &cgi.client->entities[ent_num];
	const uint8_t flash = cgi.ReadByte();

	const s_sample_t *sample;

	switch (flash) {
	case MZ_BLASTER:
		c = cgi.ReadByte();
		sample = cg_sample_blaster_fire;
		Cg_EnergyFlash(&cent->current, c ? c : EFFECT_COLOR_ORANGE);
		break;
	case MZ_SHOTGUN:
		sample = cg_sample_shotgun_fire;
		Cg_SmokeFlash(&cent->current);
		break;
	case MZ_SSHOTGUN:
		sample = cg_sample_supershotgun_fire;
		Cg_SmokeFlash(&cent->current);
		break;
	case MZ_MACHINEGUN:
		sample = cg_sample_machinegun_fire[Random() % 4];
		if (Random() & 1)
			Cg_SmokeFlash(&cent->current);
		break;
	case MZ_ROCKET:
		sample = cg_sample_rocketlauncher_fire;
		Cg_SmokeFlash(&cent->current);
		break;
	case MZ_GRENADE:
		sample = cg_sample_grenadelauncher_fire;
		Cg_SmokeFlash(&cent->current);
		break;
	case MZ_HYPERBLASTER:
		sample = cg_sample_hyperblaster_fire;
		Cg_EnergyFlash(&cent->current, 105);
		break;
	case MZ_LIGHTNING:
		sample = cg_sample_lightning_fire;
		break;
	case MZ_RAILGUN:
		sample = cg_sample_railgun_fire;
		break;
	case MZ_BFG:
		sample = cg_sample_bfg_fire;
		Cg_EnergyFlash(&cent->current, 200);
		break;
	case MZ_LOGOUT:
		sample = cg_sample_teleport;
		Cg_LogoutFlash(cent->current.origin);
		break;
	default:
		sample = NULL;
		break;
	}

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = sample,
		.entity = ent_num,
		.attenuation = ATTEN_NORM,
		.flags = S_PLAY_ENTITY
	});
}

