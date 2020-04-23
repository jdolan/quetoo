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
static void Cg_BlasterEffect(const vec3_t org, const vec3_t dir, const color_t color) {
	cg_sprite_t *s;

	// surface aligned blast ring sprite
	if ((s = Cg_AllocSprite())) {
		s->animation = cg_sprite_blaster_ring;
		s->lifetime = cg_sprite_blaster_ring->num_images * FRAMES_TO_SECONDS(17.5);
		s->origin = Vec3_Add(org, Vec3_Scale(dir, 3.0));
		s->size = 22.5f;
		s->size_velocity = 75.f;
		s->dir = dir;
		s->color = Color_Mix(color, color_white, .2f);
		s->color.a = 0.f;
	}

	// radial particles
	for (int32_t i = 0; i < 32; i++) {
		if ((s = Cg_AllocSprite())) {
			s->atlas_image = cg_sprite_particle;
			s->origin = Vec3_Add(org, Vec3_Scale(dir, 3.0));
			s->velocity = Vec3_RandomizeDir(Vec3_Scale(dir, 125.f), .6666f);
			s->size = .5f;
			s->acceleration = Vec3_Scale(s->velocity, -2.f);
			s->lifetime = 500;
			s->color = color;
			s->color_velocity.w = -1.33f / MILLIS_TO_SECONDS(s->lifetime);
		}
	}

	// residual flames
	for (int32_t i = 0; i < 3; i++) {
		if ((s = Cg_AllocSprite())) {
			s->animation = cg_sprite_blaster_flame;
			s->lifetime = cg_sprite_blaster_flame->num_images * FRAMES_TO_SECONDS(30);
			s->origin = Vec3_Add(org, Vec3_Scale(dir, 5.f));
			s->origin = Vec3_Add(org, Vec3_Scale(Vec3_RandomDir(), 5.f));
			s->rotation = Randomf() * M_PI * 2.f;
			s->rotation_velocity = Randomf() * .1f;
			s->size = 25.f;
			s->color = color;
			s->color.a = 0.f;
		}
	}

	// surface flame
	if ((s = Cg_AllocSprite())) {
		s->animation = cg_sprite_blaster_flame;
		s->lifetime = cg_sprite_blaster_flame->num_images * FRAMES_TO_SECONDS(30);
		s->origin = Vec3_Add(org, Vec3_Scale(dir, 7.5f));
		s->origin = Vec3_Add(org, Vec3_Scale(Vec3_RandomDir(), 5.f));
		s->rotation = Randomf() * M_PI * 2.f;
		s->rotation_velocity = Randomf() * .1f;
		s->dir = dir;
		s->size = 25.f;
		s->size_velocity = 20.f;
		s->color = Color_Mix(color, color_white, .2f);
		s->color.a = 0.f;
	}

	// surface flame
	if ((s = Cg_AllocSprite())) {
		s->animation = cg_sprite_blaster_flame;
		s->lifetime = cg_sprite_blaster_flame->num_images * FRAMES_TO_SECONDS(30);
		s->origin = Vec3_Add(org, Vec3_Scale(dir, 7.5f));
		s->origin = Vec3_Add(org, Vec3_Scale(Vec3_RandomDir(), 5.f));
		s->rotation = Randomf() * M_PI * 2.f;
		s->rotation_velocity = Randomf() * .1f;
		s->dir = dir;
		s->size = 25.f;
		s->size_velocity = 20.f;
		s->color = Color_Mix(color, color_white, .2f);
		s->color.a = 0.f;
	}

	// smouldering hole
	if ((s = Cg_AllocSprite())) {
		s->image = cg_sprite_blaster_flash;
		s->lifetime = 3000;
		s->origin = Vec3_Add(org, Vec3_Scale(dir, 3.f));
		s->rotation = Randomf() * M_PI * 2.f;
		s->dir = dir;
		s->size = 25.f;
		s->color = Color_Mix(color, color_white, .3f);
		s->color.a = 0.f;
		s->color_velocity.w = -s->color.a / MILLIS_TO_SECONDS(s->lifetime);
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = Vec3_Add(org, Vec3_Scale(dir, 8.f)),
		.radius = 100.f,
		.color = Color_Vec3(color),
		.intensity = .125f,
		.decay = 350.f
	});

	cgi.AddStain(&(const r_stain_t) {
		.origin = org,
		.radius = 4.0,
		.color = Color_Add(color, Color4f(0.f, 0.f, 0.f, -.66f))
	});

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_blaster_hit,
		.origin = org,
		.attenuation = ATTEN_NORM,
		.flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
static void Cg_TracerEffect(const vec3_t start, const vec3_t end) {
	cg_sprite_t *s;

	if ((s = Cg_AllocSprite())) {
		s->atlas_image = cg_sprite_particle;
		s->origin = start;
		const float dist = Vec3_DistanceDir(end, start, &s->velocity);
		s->velocity = Vec3_Scale(s->velocity, 8000.f);
		s->acceleration.z = -SPRITE_GRAVITY * 3.f;
		s->lifetime = SECONDS_TO_MILLIS(dist / 8000.f);
		s->color = Color3bv(0xffa050);
	}
}

/**
 * @brief
 */
static void Cg_BulletEffect(const vec3_t org, const vec3_t dir) {
	static uint32_t last_ric_time;
	cg_sprite_t *s;
	vec3_t vec;

	if (cgi.PointContents(org) & CONTENTS_MASK_LIQUID) {

		vec = Vec3_Add(org, Vec3_Scale(dir, 8.f));
		Cg_BubbleTrail(NULL, org, vec, 32.0);

	} else {

		if ((s = Cg_AllocSprite())) {
			s->atlas_image = cg_sprite_particle;
			s->origin = Vec3_Add(org, dir);
			s->velocity = Vec3_Scale(dir, RandomRangef(100.f, 200.f));
			s->size = .5f;
			s->acceleration = Vec3_RandomRange(-40.f, 40.f);
			s->acceleration.z -= SPRITE_GRAVITY;
			s->lifetime = RandomRangef(150.f, 300.f);
			s->color = Color_Add(Color3bv(0xffa050), Color3fv(Vec3_RandomRange(-1.f, .1f)));
			s->color_velocity.w = -1.f / MILLIS_TO_SECONDS(s->lifetime);
		}

		if ((s = Cg_AllocSprite())) {
			s->atlas_image = cg_sprite_particle;
			s->origin = Vec3_Add(org, dir);
			// s->velocity = Vec3_Scale(dir, RandomRangef(100.f, 200.f));
			// s->acceleration = Vec3_RandomRange(-40.f, 40.f);
			// s->acceleration.z -= SPRITE_GRAVITY;
			s->lifetime = 1000;
			s->color = Color3bv(0xffa050);
			s->color_velocity.w = -1.f / MILLIS_TO_SECONDS(s->lifetime);
		}

		if ((s = Cg_AllocSprite())) {
			if (Randomf() < .75f) {
				s->animation = cg_sprite_poof_02;
				s->lifetime = cg_sprite_poof_02->num_images * FRAMES_TO_SECONDS(20);
				s->origin = Vec3_Add(org, dir);
				// s->dir = dir;
				s->size = 35.f;
				s->size_velocity = 100.f;
				s->rotation = Randomf() * M_PI * 2.f;
				s->color = Color4f(1.f, 1.f, 1.f, 0.f);
			} else {
				s->animation = cg_sprite_smoke_05;
				s->lifetime = cg_sprite_smoke_05->num_images * FRAMES_TO_SECONDS(80);
				s->origin = Vec3_Add(org, Vec3_Scale(dir, 3.f));
				s->dir = dir;
				s->size = 35.f;
				s->rotation = Randomf() * M_PI * 2.f;
				s->color = Color4f(1.f, 1.f, 1.f, 0.f);

			}
		}
	}

	/*
	if ((p = Cg_AllocSprite())) {

		p->origin = org;
		p->velocity = Vec3_Scale(dir, RandomRangef(20.f, 30.f));
		p->acceleration = Vec3_Scale(dir, -20.f);
		p->acceleration.z = RandomRangef(20.f, 30.f);

		p->lifetime = RandomRangef(600.f, 1000.f);

		p->color = Color_Add(Color3bv(0xa0a0a0), Color3fv(Vec3_RandomRange(-1.f, .1f)));
		p->color_velocity.w = -p->color.a / MILLIS_TO_SECONDS(p->lifetime);

		p->size = RandomRangef(1.f, 2.f);
		p->size_velocity = 1.f;
	}
	*/

	/*
	Cg_AddLight(&(const cg_light_t) {
		.origin = Vec3_Add(org, dir),
		.radius = 20.0,
		.color = Vec3(0.5f, 0.3f, 0.2f),
		.decay = 250
	});
	*/

	cgi.AddStain(&(const r_stain_t) {
		.origin = org,
		.radius = 2.0,
		.color = Color4bv(0x00000090),
	});

	if (cgi.client->unclamped_time < last_ric_time) {
		last_ric_time = 0;
	}

	if (cgi.client->unclamped_time - last_ric_time > 300) {
		last_ric_time = cgi.client->unclamped_time;

		cgi.AddSample(&(const s_play_sample_t) {
			.sample = cg_sample_machinegun_hit[RandomRangeu(0, 3)],
			.origin = org,
			.attenuation = ATTEN_NORM,
			.flags = S_PLAY_POSITIONED,
			.pitch = RandomRangei(-8, 9)
		});
	}
}

/**
 * @brief
 */
static void Cg_BloodEffect(const vec3_t org, const vec3_t dir, int32_t count) {

	for (int32_t i = 0; i < count; i++) {
		cg_sprite_t *s;

		if (!(s = Cg_AllocSprite())) {
			break;
		}

		s->atlas_image = cg_sprite_particle;
		s->origin = Vec3_Add(org, Vec3_RandomRange(-10.f, 10.f));
		s->origin = Vec3_Add(s->origin, Vec3_Scale(dir, RandomRangef(0.f, 32.f)));
		s->velocity = Vec3_RandomRange(-30.f, 30.f);
		s->acceleration.z = -SPRITE_GRAVITY / 2.0;
		s->lifetime = 100 + Randomf() * 500;
		s->color = Color4bv(0x882200aa);
		s->size = RandomRangef(5.0, 8.0);
	}

	cgi.AddStain(&(const r_stain_t) {
		.origin = org,
		.radius = count * 6.0,
		.color = Color4bv(0xAA2222AA),
	});
}

#define GIB_STREAM_DIST 180.0
#define GIB_STREAM_COUNT 24

/**
 * @brief
 */
void Cg_GibEffect(const vec3_t org, int32_t count) {
	cg_sprite_t *s;
	vec3_t o, v, tmp;
	float dist;

	// if a player has died underwater, emit some bubbles
	if (cgi.PointContents(org) & CONTENTS_MASK_LIQUID) {
		tmp = org;
		tmp.z += 64.0;

		Cg_BubbleTrail(NULL, org, tmp, 16.0);
	}

	for (int32_t i = 0; i < count; i++) {

		// set the origin and velocity for each gib stream
		o = Vec3(RandomRangef(-8.f, 8.f), RandomRangef(-8.f, 8.f), RandomRangef(-4.f, 20.f));
		o = Vec3_Add(o, org);

		v = Vec3(RandomRangef(-1.f, 1.f), RandomRangef(-1.f, 1.f), RandomRangef(.2f, 1.2f));

		dist = GIB_STREAM_DIST;
		tmp = Vec3_Add(o, Vec3_Scale(v, dist));

		const cm_trace_t tr = cgi.Trace(o, tmp, Vec3_Zero(), Vec3_Zero(), 0, CONTENTS_MASK_CLIP_PROJECTILE);
		dist = GIB_STREAM_DIST * tr.fraction;

		for (int32_t j = 1; j < GIB_STREAM_COUNT; j++) {

			if (!(s = Cg_AllocSprite())) {
				break;
			}

			s->atlas_image = cg_sprite_particle;
			s->origin = o;
			s->velocity = Vec3_Scale(v, dist * ((float)j / GIB_STREAM_COUNT));
			s->velocity = Vec3_Add(s->velocity, Vec3_RandomRange(-2.f, 2.f));
			s->velocity.z += 100.f;
			s->acceleration.z = -SPRITE_GRAVITY * 2.0;
			s->lifetime = 700 + Randomf() * 400;
			s->color = Color4bv(0x800000f8);
			s->size = RandomRangef(3.0, 7.0);
		}
	}

	cgi.AddStain(&(const r_stain_t) {
		.origin = org,
		.radius = count * 6.0,
		.color = Color4bv(0x88111188),
	});

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_gib,
		.origin = org,
		.attenuation = ATTEN_NORM,
		.flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
void Cg_SparksEffect(const vec3_t org, const vec3_t dir, int32_t count) {

	for (int32_t i = 0; i < count; i++) {
		cg_sprite_t *s;

		if (!(s = Cg_AllocSprite())) {
			break;
		}

		s->atlas_image = cg_sprite_spark;
		s->origin = Vec3_Add(org, Vec3_RandomRange(-4.f, 4.f));
		s->velocity = Vec3_Add(Vec3_Scale(dir, 4.f), Vec3_RandomRange(-90.f, 90.f));
		s->acceleration = Vec3_RandomRange(-1.f, 1.f);
		s->acceleration.z += -0.5 * SPRITE_GRAVITY;
		s->lifetime = 500 + Randomf() * 250;
		s->color = Color3b(RandomRangeu(190, 256), RandomRangeu(90, 140), RandomRangeu(0, 20));
		s->color.a = RandomRangef(.56f, 1.f);
		s->size = RandomRangef(.4f, .8f);
		s->bounce = 0.6f;
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = org,
		.radius = 80.0,
		.color = Vec3(0.7, 0.5, 0.5),
		.decay = 650
	});

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_sparks,
		.origin = org,
		.attenuation = ATTEN_STATIC,
		.flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
static void Cg_ExplosionEffect(const vec3_t org, const vec3_t dir) {
	cg_sprite_t *s;

	// TODO: Bubbles in water?
	
	if ((cgi.PointContents(org) & CONTENTS_MASK_LIQUID) == 0) {

		for (int32_t i = 0; i < 100; i++) {
			if (!(s = Cg_AllocSprite())) {
				break;
			}

			s->atlas_image = cg_sprite_particle;
			s->origin = Vec3_Add(org, Vec3_RandomRange(-10.f, 10.f));
			s->velocity = Vec3_RandomRange(-300.f, 300.f);
			s->acceleration.z = -SPRITE_GRAVITY * 2.0;
			s->lifetime = 3000 + Randomf() * 300;
			s->color = Color3b(255, 255, 255);
			s->color.a = 255.f;
			s->color_velocity = Vec4_Scale(Vec4(-.5f, -1.5f, -3.f, 1.f), 1.f / MILLIS_TO_SECONDS(s->lifetime));
			s->bounce = .4f;
			s->size = .2f + Randomf() * .4f;
		}
	}

	if ((s = Cg_AllocSprite())) {
		s->origin = org;
		s->lifetime = cg_sprite_exlosion->num_images * FRAMES_TO_SECONDS(40);
		s->color = color_white;
		s->size = 100.0;
		s->size_velocity = 25.0;
		s->animation = cg_sprite_exlosion;
		s->rotation = Randomf() * 2.f * M_PI;
		s->lerp = true;
		s->color = Color4f(1.f, 1.f, 1.f, .5f);
	}

	if ((s = Cg_AllocSprite())) {
		s->origin = org;
		s->lifetime = cg_sprite_exlosion->num_images * FRAMES_TO_SECONDS(30);
		s->color = color_white;
		s->size = 175.0;
		s->size_velocity = 25.0;
		s->rotation = Randomf() * 2.f * M_PI;
		s->animation = cg_sprite_exlosion;
		s->lerp = true;
		s->color = Color4f(1.f, 1.f, 1.f, .5f);
	}
	
	Cg_AddLight(&(const cg_light_t) {
		.origin = org,
		.radius = 150.0,
		.color = Vec3(0.8, 0.4, 0.2),
		.decay = 825
	});

	cgi.AddStain(&(const r_stain_t) {
		.origin = org,
		.radius = RandomRangef(38.f, 48.f),
		.color = Color4bv(0x202020ff),
	});

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_explosion,
		.origin = org,
		.attenuation = ATTEN_NORM,
		.flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
static void Cg_HyperblasterEffect(const vec3_t org, const vec3_t dir) {
	cg_sprite_t *s;

	if ((cgi.PointContents(org) & CONTENTS_MASK_LIQUID) == 0) {
		for (int32_t i = 0; i < 6; i++) {

			if (!(s = Cg_AllocSprite())) {
				break;
			}

			s->atlas_image = cg_sprite_spark;
			s->origin = org;
			s->velocity = Vec3_Multiply(dir, Vec3_RandomRange(20.f, 140.f));
			s->velocity = Vec3_Add(s->velocity, Vec3_RandomRange(-16.f, 16.f));
			s->acceleration.z = -SPRITE_GRAVITY * 2.0;
			s->lifetime = 200 + Randomf() * 500;
			s->color = Color4bv(0x22aaffff);
			s->size = 1.5;
			s->bounce = 0.9f;
			s->color_velocity.w = -1.f / MILLIS_TO_SECONDS(s->lifetime);
		}
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = Vec3_Add(org, dir),
		.radius = 80.0,
		.color = Vec3(0.4, 0.7, 1.0),
		.decay = 250,
		.intensity = 0.05
	});

	cgi.AddStain(&(const r_stain_t) {
		.origin = org,
		.radius = 16.0,
		.color = Color4f(.4f, .7f, 1.f, .33f),
	});

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_hyperblaster_hit,
		.origin = Vec3_Add(org, dir),
		.attenuation = ATTEN_NORM,
		.flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
static void Cg_LightningDischargeEffect(const vec3_t org) {

	for (int32_t i = 0; i < 40; i++) {
		Cg_BubbleTrail(NULL, org, Vec3_Add(org, Vec3_RandomRange(-48.f, 48.f)), 4.0);
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = org,
		.radius = 160.0,
		.color = Vec3(0.6, 0.6, 1.0),
		.decay = 750
	});

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_lightning_discharge,
		.origin = org,
		.attenuation = ATTEN_NORM,
		.flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
static void Cg_RailEffect(const vec3_t start, const vec3_t end, const vec3_t dir, int32_t flags, const color_t color) {
	vec3_t forward;
	cg_sprite_t *s;

	Cg_AddLight(&(cg_light_t) {
		.origin = start,
		.radius = 100.f,
		.color = Color_Vec3(color),
		.decay = 500,
	});

	// Check for bubble trail
	Cg_BubbleTrail(NULL, start, end, 16.f);

	const float dist = Vec3_DistanceDir(end, start, &forward);
	const vec3_t right = Vec3(forward.z, -forward.x, forward.y);
	const vec3_t up = Vec3_Cross(forward, right);

	for (int32_t i = 0; i < dist; i++) {

		if (!(s = Cg_AllocSprite())) {
			break;
		}

		s->origin = Vec3_Add(start, Vec3_Scale(forward, i));
		s->velocity = Vec3_Scale(forward, 20.f);

		const float cosi = cosf(i * 0.1f);
		const float sini = sinf(i * 0.1f);

		const float frac = (1.0 - (i / dist));

		s->atlas_image = cg_sprite_particle;
		s->origin = Vec3_Add(s->origin, Vec3_Scale(right, cosi));
		s->origin = Vec3_Add(s->origin, Vec3_Scale(up, sini));
		s->velocity = Vec3_Add(s->velocity, Vec3_Scale(right, cosi * frac));
		s->velocity = Vec3_Add(s->velocity, Vec3_Scale(up, sini * frac));
		s->acceleration = Vec3_Add(s->acceleration, Vec3_Scale(right, cosi * 8.f));
		s->acceleration = Vec3_Add(s->acceleration, Vec3_Scale(up, sini * 8.f));
		s->acceleration.z += 1.f;
		s->lifetime = RandomRangef(1500.f, 1550.f);
		s->color = Color_Add(color, Color4fv(Vec4_RandomRange(-.25f, .25f)));
		s->color.a = 0;
		s->color_velocity = Vec4_Negate(Color_Vec4(s->color));
		s->color_velocity = Vec4_Scale(s->color_velocity, 1.f / MILLIS_TO_SECONDS(s->lifetime));
		s->color_acceleration = Vec4(-.125f, -.125f, -.125f, 0);
		s->color_acceleration = Vec4_Scale(s->color_acceleration, 1.f / MILLIS_TO_SECONDS(s->lifetime));
		s->size = frac;
		s->size_velocity = 1.f / MILLIS_TO_SECONDS(s->lifetime);
	}

	if ((s = Cg_AllocSprite())) {
		s->type = SPRITE_BEAM;
		s->origin = start;
		s->termination = end;
		s->size = 20.f;
		s->velocity = Vec3_Scale(forward, 20.f);
		s->lifetime = RandomRangef(1500.f, 1550.f);
		s->color = Color_Add(color, color_white);
		s->color.a = 0;
		s->color_velocity = Vec4_Scale(Color_Vec4(s->color), -1.f / MILLIS_TO_SECONDS(s->lifetime));
		s->image = cg_beam_rail;
	}

	// Check for explosion effect on solids

	if (flags & SURF_SKY) {
		return;
	}

	// Rail impact cloud
	// TODO: use a different sprite
	for (int32_t i = 0; i < 2; i++) {
		if (!(s = Cg_AllocSprite())) {
			break;
		}
		s->type = SPRITE_NORMAL;
		s->origin = Vec3_Add(end, Vec3_Scale(dir, 8.f));
		s->animation = cg_sprite_poof_01;
		s->lifetime = cg_sprite_poof_01->num_images * FRAMES_TO_SECONDS(30);
		s->size = 128.f;
		s->size_velocity = 20.f;
		s->color = Color_Mix(color, color_white, .25f);
		s->color.a = 0.5f;
		if (i == 1) {
			s->dir = dir;
		}
	}

	if (cg_particle_quality->integer && (cgi.PointContents(end) & CONTENTS_MASK_LIQUID) == 0) {

		for (int32_t i = 0; i < 16; i++) {

			if (!(s = Cg_AllocSprite())) {
				break;
			}

			s->atlas_image = cg_sprite_particle;
			s->origin = end;
			const vec3_t rdir = Vec3_RandomDir();
			s->velocity = Vec3_Scale(Vec3_Add(Vec3_Scale(dir, .5f), rdir), 100.f);
			s->acceleration = Vec3_Scale(s->velocity, -1.f);
			s->lifetime = 1000;
			s->color = color;
			s->size = 1.f;
			s->size_velocity = -s->size / (s->lifetime / 1000);
		}
	}

	// Impact light

	Cg_AddLight(&(cg_light_t) {
		.origin = Vec3_Add(end, Vec3_Scale(dir, 20.f)),
		.radius = 120.f,
		.color = Color_Vec3(color),
		.decay = 350.f,
		.intensity = .1f
	});

	cgi.AddStain(&(const r_stain_t) {
		.origin = end,
		.radius = 8.0,
		.color = color,
	});
}

/**
 * @brief
 */
static void Cg_BfgLaserEffect(const vec3_t org, const vec3_t end) {
	cg_sprite_t *s;

	if ((s = Cg_AllocSprite())) {
		// FIXME: beam
		s->atlas_image = cg_sprite_particle;
		s->origin = org;
		s->lifetime = 50;
		s->color = color_green;
		s->size = 6.0;
	}

	Cg_AddLight(&(cg_light_t) {
		.origin = end,
		.radius = 80.0,
		.color = Vec3(0.8, 1.0, 0.5),
		.decay = 50,
	});
}

/**
 * @brief
 */
static void Cg_BfgEffect(const vec3_t org) {
	cg_sprite_t *s;

	for (int32_t i = 0; i < 4; i++) {
		if (!(s = Cg_AllocSprite())) {
			break;
		}
		s->animation = cg_sprite_bfg_explosion_2;
		s->lifetime = cg_sprite_bfg_explosion_2->num_images * FRAMES_TO_SECONDS(30);
		s->size = RandomRangef(200.f, 300.f);
		s->size_velocity = 100.f;
		s->size_acceleration = -10.f;
		s->rotation = RandomRangef(0.f, 2.f * M_PI);
		s->origin = Vec3_Add(org, Vec3_Scale(Vec3_RandomDir(), 50.f));
	}

	for (int32_t i = 0; i < 4; i++) {
		if (!(s = Cg_AllocSprite())) {
			break;
		}
		s->animation = cg_sprite_bfg_explosion_3;
		s->lifetime = cg_sprite_bfg_explosion_3->num_images * FRAMES_TO_SECONDS(30);
		s->size = RandomRangef(200.f, 300.f);
		s->size_velocity = 100.f;
		s->size_acceleration = -10.f;
		s->rotation = RandomRangef(0.f, 2.f * M_PI);
		s->origin = Vec3_Add(org, Vec3_Scale(Vec3_RandomDir(), 50.f));
	}
	
	for (int32_t i = 0; i < 50; i++) {
		if (!(s = Cg_AllocSprite())) {
			break;
		}
		vec3_t rdir = Vec3_RandomDir();
		s->atlas_image = cg_sprite_particle;
		s->lifetime = 3000;
		s->size = 1.f;
		s->size_velocity = -s->size / MILLIS_TO_SECONDS(s->lifetime);
		s->bounce = .5f;
		s->velocity = Vec3_Scale(rdir, 400.f);
		s->acceleration = Vec3_Zero();
		s->acceleration.z -= 3.f * SPRITE_GRAVITY;
		s->origin = org;
		s->color = color_white;
		s->color_velocity = Vec4(-.5f, 0.f, -.4f, -.1f);
	}

	for (int32_t i = 0; i < 20; i++) {
		if (!(s = Cg_AllocSprite())) {
			break;
		}
		vec3_t rdir = Vec3_RandomDir();
		s->atlas_image = cg_sprite_particle;
		s->lifetime = 10000;
		s->size = 0.5f;
		s->size_velocity = -s->size / MILLIS_TO_SECONDS(s->lifetime);
		s->bounce = .5f;
		s->velocity = Vec3_Scale(rdir, 300.f);
		s->origin = org;
		s->color = Color_Mix(color_white, color_green, Randomf());
		s->color_velocity = Vec4_Scale(Vec4(-.5f, 0.f, -.4f, -.1f), RandomRangef(.75f, 1.25f));
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = org,
		.radius = 200.f,
		.color = Vec3(.8f, 1.f, .5f),
		.intensity = .01f,
		.decay = 1000
	});
	
	cgi.AddStain(&(const r_stain_t) {
		.origin = org,
		.radius = 96.f,
		.color = Color3b(40, 200, 60),
	});

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_bfg_hit,
		.origin = org,
		.attenuation = ATTEN_NORM,
		.flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
void Cg_RippleEffect(const vec3_t org, const float size, const uint8_t viscosity) {

	cg_sprite_t *s = Cg_AllocSprite();
	if (!s) {
		return;
	}
	s->animation = cg_sprite_poof_01;
	s->lifetime = cg_sprite_poof_01->num_images * FRAMES_TO_SECONDS(17.5) * (viscosity * .1f);
	s->origin = org;
	s->size = size * 8;
	s->dir = Vec3_Up();
}

/**
 * @brief
 */
static void Cg_SplashEffect(const vec3_t org, const vec3_t dir) {
	cg_sprite_t *s;

	for (int32_t i = 0; i < 10; i++) {
		if (!(s = Cg_AllocSprite())) {
			break;
		}

		s->atlas_image = cg_sprite_particle;
		s->origin = Vec3_Add(org, Vec3_RandomRange(-8.f, 8.f));
		s->velocity = Vec3_RandomRange(-64.f, 64.f);
		s->velocity.z += 36.f;
		s->acceleration = Vec3(0.0, 0.0, -SPRITE_GRAVITY / 2.0);
		s->lifetime = RandomRangeu(100, 400);
		s->color = color_white;
		s->size = 0.8;
	}

	if ((s = Cg_AllocSprite())) {
		s->atlas_image = cg_sprite_particle;
		s->origin = org;
		s->velocity = Vec3_Scale(dir, 70.0 + Randomf() * 30.0);
		s->velocity = Vec3_Add(s->velocity, Vec3_RandomRange(-8.f, 8.f));
		s->acceleration = Vec3(0.0, 0.0, -SPRITE_GRAVITY / 2.0);
		s->lifetime = 120 + Randomf() * 80;
		s->color = Color4bv(0x80e0e0e0);
		s->size = 1.4 + Randomf() * 0.7;
	}
}

/**
 * @brief
 */
static void Cg_HookImpactEffect(const vec3_t org, const vec3_t dir) {

	for (int32_t i = 0; i < 32; i++) {
		cg_sprite_t *s;

		if (!(s = Cg_AllocSprite())) {
			break;
		}

		s->atlas_image = cg_sprite_particle;
		s->origin = Vec3_Add(org, Vec3_RandomRange(-4.f, 4.f));
		s->velocity = Vec3_Add(Vec3_Scale(dir, 9.f), Vec3_RandomRange(-90.f, 90.f));
		s->acceleration = Vec3_RandomRange(-2.f, 2.f);
		s->acceleration.z += -0.5 * SPRITE_GRAVITY;
		s->lifetime = 100 + (Randomf() * 150);
		s->color = Color3b(248, 224, 40);
		s->color.a = (200 * Randomf() * 55) / 255.f;
		s->size = 0.8 + Randomf() * 0.4;
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = Vec3_Add(org, dir),
		.radius = 80.0,
		.color = Vec3(0.7, 0.5, 0.5),
		.decay = 850
	});
}

/**
 * @brief
 */
void Cg_ParseTempEntity(void) {
	vec3_t pos, pos2, dir;
	int32_t i, j;

	const uint8_t type = cgi.ReadByte();

	switch (type) {

		case TE_BLASTER:
			pos = cgi.ReadPosition();
			dir = cgi.ReadDir();
			i = cgi.ReadByte();
			Cg_BlasterEffect(pos, dir, Cg_ResolveEffectColor(i ? i - 1 : 0, EFFECT_COLOR_ORANGE));
			break;

		case TE_TRACER:
			pos = cgi.ReadPosition();
			pos2 = cgi.ReadPosition();
			Cg_TracerEffect(pos, pos2);
			break;

		case TE_BULLET: // bullet hitting wall
			pos = cgi.ReadPosition();
			dir = cgi.ReadDir();
			Cg_BulletEffect(pos, dir);
			break;

		case TE_BLOOD: // projectile hitting flesh
			pos = cgi.ReadPosition();
			dir = cgi.ReadDir();
			Cg_BloodEffect(pos, dir, 12);
			break;

		case TE_GIB: // player over-death
			pos = cgi.ReadPosition();
			Cg_GibEffect(pos, 12);
			break;

		case TE_SPARKS: // colored sparks
			pos = cgi.ReadPosition();
			dir = cgi.ReadDir();
			Cg_SparksEffect(pos, dir, 12);
			break;

		case TE_HYPERBLASTER: // hyperblaster hitting wall
			pos = cgi.ReadPosition();
			dir = cgi.ReadDir();
			Cg_HyperblasterEffect(pos, dir);
			break;

		case TE_LIGHTNING_DISCHARGE: // lightning discharge in water
			pos = cgi.ReadPosition();
			Cg_LightningDischargeEffect(pos);
			break;

		case TE_RAIL: // railgun effect
			pos = cgi.ReadPosition();
			pos2 = cgi.ReadPosition();
			dir = cgi.ReadDir();
			i = cgi.ReadLong();
			j = cgi.ReadByte();
			Cg_RailEffect(pos, pos2, dir, i, Cg_ResolveEffectColor(j ? j - 1 : 0, EFFECT_COLOR_BLUE));
			break;

		case TE_EXPLOSION: // rocket and grenade explosions
			pos = cgi.ReadPosition();
			dir = cgi.ReadDir();
			Cg_ExplosionEffect(pos, dir);
			break;

		case TE_BFG_LASER:
			pos = cgi.ReadPosition();
			pos2 = cgi.ReadPosition();
			Cg_BfgLaserEffect(pos, pos2);
			break;

		case TE_BFG: // bfg explosion
			pos = cgi.ReadPosition();
			Cg_BfgEffect(pos);
			break;

		case TE_BUBBLES: // bubbles chasing projectiles in water
			pos = cgi.ReadPosition();
			pos2 = cgi.ReadPosition();
			Cg_BubbleTrail(NULL, pos, pos2, 1.0);
			break;

		case TE_RIPPLE: // liquid surface ripples
			pos = cgi.ReadPosition();
			dir = cgi.ReadDir();
			i = cgi.ReadByte();
			j = cgi.ReadByte();
			Cg_RippleEffect(pos, i, j);
			if (cgi.ReadByte()) {
				Cg_SplashEffect(pos, dir);
			}
			break;

		case TE_HOOK_IMPACT: // grapple hook impact
			pos = cgi.ReadPosition();
			dir = cgi.ReadDir();
			Cg_HookImpactEffect(pos, dir);
			break;

		default:
			cgi.Warn("Unknown type: %d\n", type);
			return;
	}
}
