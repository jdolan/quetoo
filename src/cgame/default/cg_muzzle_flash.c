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
	Vec3_Vectors(ent->angles, &forward, &right, NULL );
	org = Vec3_Add(ent->origin, Vec3_Scale(forward, 30.0));
	org = Vec3_Add(org, Vec3_Scale(right, 6.0));

	const cm_trace_t tr = cgi.Trace(ent->origin, org, Vec3_Zero(), Vec3_Zero(), 0, CONTENTS_MASK_CLIP_PROJECTILE);

	if (tr.fraction < 1.0) { // firing near a wall, back it up
		org = Vec3_Subtract(ent->origin, tr.end);
		org = Vec3_Scale(org, 0.75);

		org = Vec3_Add(ent->origin, org);
	}

	// and adjust for ducking
	org.z += Cg_IsDucking(ent) ? -2.0 : 20.0;

	cg_light_t l;
	l.origin = org;
	l.radius = 80.0;
	l.color = Color_Vec3(color);
	l.decay = 450;

	Cg_AddLight(&l);

	if (cgi.PointContents(ent->origin) & CONTENTS_MASK_LIQUID) {
		org2 = Vec3_Add(ent->origin, Vec3_Scale(forward, 40.0));
		Cg_BubbleTrail(NULL, org, org2, 10.0);
	}
}

/**
 * @brief
 */
static void Cg_SmokeFlash(const cl_entity_t *ent) {
	cg_sprite_t *p;
	vec3_t forward, right, org, org2;

	// project the puff just in front of the entity
	Vec3_Vectors(ent->angles, &forward, &right, NULL );
	org = Vec3_Add(ent->origin, Vec3_Scale(forward, 30.0));
	org = Vec3_Add(org, Vec3_Scale(right, 6.0));

	const cm_trace_t tr = cgi.Trace(ent->origin, org, Vec3_Zero(), Vec3_Zero(), 0, CONTENTS_MASK_CLIP_PROJECTILE);

	if (tr.fraction < 1.0) { // firing near a wall, back it up
		org = Vec3_Subtract(ent->origin, tr.end);
		org = Vec3_Scale(org, 0.75);

		org = Vec3_Add(ent->origin, org);
	}

	// and adjust for ducking
	org.z += Cg_IsDucking(ent) ? -2.0 : 16.0;

	Cg_AddLight(&(cg_light_t) {
		.origin = org,
		.radius = 120.0,
		.color = Vec3(0.8, 0.7, 0.5),
		.decay = 300
	});

	if (cgi.PointContents(ent->origin) & CONTENTS_MASK_LIQUID) {
		org2 = Vec3_Add(ent->origin, Vec3_Scale(forward, 40.0));
		Cg_BubbleTrail(NULL, org, org2, 10.0);
		return;
	}

	if (!(p = Cg_AllocSprite())) {
		return;
	}

	p->origin = org;

	p->lifetime = 500;

	p->color = Color3b(128, 128, 128);
	p->color.a = 200 / 255.0;
//	p->delta_color.a = -10;

	p->color_velocity.w = -p->color.a / MILLIS_TO_SECONDS(p->lifetime);

	p->size = 4.0;
	p->size_velocity = 16.0;
//	p->delta_size = 1.0;

	p->velocity = Vec3_RandomRange(-1.f, 1.f);
	p->velocity.z += 10.f;

	p->acceleration.z = 5.0;

	p->image = cg_sprite_smoke;

	p->rotation = RandomRangef(0.0f, M_PI);

	p->rotation_velocity = RandomRangef(.8f, 1.6f);
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
			Cg_EnergyFlash(ent, Color3b(191, 123, 111));
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
			Cg_EnergyFlash(ent, Color3b(75, 91, 39));
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
