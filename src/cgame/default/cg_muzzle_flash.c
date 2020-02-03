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

/**
 * @brief
 */
static void Cg_EnergyFlash(const cl_entity_t *ent, const color_t color) {
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

	cg_light_t l;
	VectorCopy(org, l.origin);
	l.radius = 80.0;
	ColorToVec3(color, l.color);
	l.decay = 450;

	Cg_AddLight(&l);

	if (cgi.PointContents(ent->origin) & MASK_LIQUID) {
		VectorMA(ent->origin, 40.0, forward, org2);
		Cg_BubbleTrail(org, org2, 10.0);
	}
}

/**
 * @brief
 */
static void Cg_SmokeFlash(const cl_entity_t *ent) {
	cg_particle_t *p;
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

	Cg_AddLight(&(cg_light_t) {
		.origin = { org[0], org[1], org[2] },
		.radius = 120.0,
		.color = { 0.8, 0.7, 0.5 },
		.decay = 300
	});

	if (cgi.PointContents(ent->origin) & MASK_LIQUID) {
		VectorMA(ent->origin, 40.0, forward, org2);
		Cg_BubbleTrail(org, org2, 10.0);
		return;
	}

	if (!(p = Cg_AllocParticle(cg_particles_smoke))) {
		return;
	}

	p->lifetime = 500;
	p->effects = PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

	cgi.ColorFromPalette(Randomr(0, 8), p->color_start);
	p->color_start[3] = 0.8;

	VectorCopy(p->color_start, p->color_end);
	p->color_end[3] = 0.0;

	p->scale_start = 4.0;
	p->scale_end = 24.0;

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
static void Cg_LogoutFlash(const cl_entity_t *ent) {
	Cg_GibEffect(ent->origin, 12);
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

	const cl_entity_t *ent = &cgi.client->entities[ent_num];
	const uint8_t flash = cgi.ReadByte();

	const s_sample_t *sample;
	int16_t pitch = 0;

	switch (flash) {
		case MZ_BLASTER:
			c = cgi.ReadByte();
			sample = cg_sample_blaster_fire;
			Cg_EnergyFlash(ent, Cg_ResolveEffectColor(c ? c - 1 : 0, EFFECT_COLOR_ORANGE));
			pitch = (int16_t) (Randomc() * 5.0);
			break;
		case MZ_SHOTGUN:
			sample = cg_sample_shotgun_fire;
			Cg_SmokeFlash(ent);
			pitch = (int16_t) (Randomc() * 3.0);
			break;
		case MZ_SUPER_SHOTGUN:
			sample = cg_sample_supershotgun_fire;
			Cg_SmokeFlash(ent);
			pitch = (int16_t) (Randomc() * 3.0);
			break;
		case MZ_MACHINEGUN:
			sample = cg_sample_machinegun_fire[Randomr(0, 4)];
			if (Randomr(0, 2)) {
				Cg_SmokeFlash(ent);
			}
			pitch = (int16_t) (Randomc() * 5.0);
			break;
		case MZ_ROCKET_LAUNCHER:
			sample = cg_sample_rocketlauncher_fire;
			Cg_SmokeFlash(ent);
			pitch = (int16_t) (Randomc() * 3.0);
			break;
		case MZ_GRENADE_LAUNCHER:
			sample = cg_sample_grenadelauncher_fire;
			Cg_SmokeFlash(ent);
			pitch = (int16_t) (Randomc() * 3.0);
			break;
		case MZ_HYPERBLASTER:
			sample = cg_sample_hyperblaster_fire;
			Cg_EnergyFlash(ent, ColorFromRGB(191, 123, 111));
			pitch = (int16_t) (Randomc() * 5.0);
			break;
		case MZ_LIGHTNING:
			sample = cg_sample_lightning_fire;
			pitch = (int16_t) (Randomc() * 3.0);
			break;
		case MZ_RAILGUN:
			sample = cg_sample_railgun_fire;
			pitch = (int16_t) (Randomc() * 2.0);
			break;
		case MZ_BFG10K:
			sample = cg_sample_bfg_fire;
			Cg_EnergyFlash(ent, ColorFromRGB(75, 91, 39));
			pitch = (int16_t) (Randomc() * 2.0);
			break;
		case MZ_LOGOUT:
			sample = cg_sample_teleport;
			Cg_LogoutFlash(ent);
			pitch = (int16_t) (Randomc() * 4.0);
			break;
		default:
			sample = NULL;
			break;
	}

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = sample,
		 .entity = ent_num,
		  .attenuation = ATTEN_NORM,// | S_SET_Z_ORIGIN_OFFSET(7),
		   .flags = S_PLAY_ENTITY,
			.pitch = pitch
	});
}
