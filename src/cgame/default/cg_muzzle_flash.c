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

// FIXME: up/down positioning is messed up, roughly right though.

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

	#if 0 // looks mediocre
	cg_sprite_t *s;
	for (uint32_t i = 0; i < 2; i++) {
		if ((s = Cg_AllocSprite())) {
			s->origin = org;
			s->lifetime = 66;
			s->color = Color4f(.1f, .45f, .45f, .0f);
			s->size = RandomRangef(20, 35);
			s->rotation = RandomRadian();
			s->rotation_velocity = i == 0 ? .66f : -.66f;
			s->atlas_image = cg_sprite_flash;
		}
	}

	if ((s = Cg_AllocSprite())) {
		s->atlas_image = cg_sprite_particle;
		s->origin = org;
		s->lifetime = 100;
		s->color = Color4f(.4f, .7f, .9f, .0f);
		s->size = 10;
	}
	#endif

	Cg_AddLight(&(cg_light_t) {
		.origin = org,
		.radius = 80.0,
		.color = Color_Vec3(color),
		.decay = 450,
	});

	if (cgi.PointContents(ent->origin) & CONTENTS_MASK_LIQUID) {
		org2 = Vec3_Add(ent->origin, Vec3_Scale(forward, 40.0));
		Cg_BubbleTrail(NULL, org, org2, 10.0);
	}
}

/**
 * @brief
 */
static void Cg_SmokeFlash(const cl_entity_t *ent) {
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

	Cg_AddSprite(&(cg_sprite_t) {
		.origin = org,
		.lifetime = 500,
		.size = 4.f,
		.size_velocity = 16.f,
		.velocity = Vec3_Add(Vec3_RandomRange(-1.f, 1.f), Vec3(0.f, 0.f, 10.f)),
		.acceleration.z = 5.f,
		.atlas_image = cg_sprite_smoke,
		.rotation = RandomRangef(0.0f, M_PI),
		.rotation_velocity = RandomRangef(.8f, 1.6f),
		.color = Vec4(0.f, 0.f, .7f, .78f),
		.end_color = Vec4(0.f, 0.f, .7f, 0.f)
	});
}

/**
 * @brief
 */
static void Cg_BlasterFlash(const cl_entity_t *ent, const float hue) {
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
		.color = Color_Vec3(ColorHSV(hue, .5f, 1.f)),
		.decay = 300
	});

	if (cgi.PointContents(ent->origin) & CONTENTS_MASK_LIQUID) {
		org2 = Vec3_Add(ent->origin, Vec3_Scale(forward, 40.0));
		Cg_BubbleTrail(NULL, org, org2, 10.0);
		return;
	}

	// TODO: make less ugly and align better with gun

	const int32_t np = 5;
	const float flashlen = 2.f;

	for (int32_t i = 0; i < np; i++){

		Cg_AddSprite(&(cg_sprite_t) {
			.animation = cg_sprite_blaster_flame,
			.lifetime = Cg_AnimationLifetime(cg_sprite_blaster_flame, 70) + (i / flashlen * 20.f),
			.origin = Vec3_Add(org, Vec3_Scale(forward, 3.f * (i / flashlen))),
			.rotation = Randomf() * M_PI * 2.f,
			.size = 1.f + 2.f * (np - i / flashlen),
			.color = Vec4(hue, 1.f, 1.f, 0.f),
			.end_color = Vec4(hue, 1.f, 0.f, 0.f)
		});
	}

	Cg_AddSprite(&(cg_sprite_t) {
		.image = cg_sprite_blaster_flash,
		.lifetime = 200,
		.origin = Vec3_Add(org, Vec3_Scale(forward, 3.f)),
		.size = 30.f,
		.size_velocity = 30.f,
		.color = Vec4(hue, 1.f, 1.f, 0.f),
		.end_color = Vec4(hue, 1.f, 0.f, 0.f)
	});
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
 			Cg_BlasterFlash(ent, Cg_ResolveEffectHue(c ? c - 1 : 0, color_hue_orange));
			pitch = 5;
			break;
		case MZ_SHOTGUN:
			sample = cg_sample_shotgun_fire;
			Cg_SmokeFlash(ent);
			pitch = 3;
			break;
		case MZ_SUPER_SHOTGUN:
			sample = cg_sample_supershotgun_fire;
			Cg_SmokeFlash(ent);
			pitch = 3;
			break;
		case MZ_MACHINEGUN:
			sample = cg_sample_machinegun_fire[RandomRangeu(0, 4)];
			if (Randomb()) {
				Cg_SmokeFlash(ent);
			}
			pitch = 5;
			break;
		case MZ_ROCKET_LAUNCHER:
			sample = cg_sample_rocketlauncher_fire;
			Cg_SmokeFlash(ent);
			pitch = 3;
			break;
		case MZ_GRENADE_LAUNCHER:
			sample = cg_sample_grenadelauncher_fire;
			Cg_SmokeFlash(ent);
			pitch = 3;
			break;
		case MZ_HYPERBLASTER:
			sample = cg_sample_hyperblaster_fire;
			Cg_EnergyFlash(ent, Color3b(191, 123, 111));
			pitch = 5;
			break;
		case MZ_LIGHTNING:
			sample = cg_sample_lightning_fire;
			pitch = 3;
			break;
		case MZ_RAILGUN:
			sample = cg_sample_railgun_fire;
			pitch = 2;
			break;
		case MZ_BFG10K:
			sample = cg_sample_bfg_fire;
			Cg_EnergyFlash(ent, Color3b(75, 91, 39));
			pitch = 2;
			break;
		case MZ_LOGOUT:
			sample = cg_sample_teleport;
			Cg_LogoutFlash(ent);
			pitch = 4;
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
		.pitch = RandomRangei(-pitch, pitch + 1)
	});
}
