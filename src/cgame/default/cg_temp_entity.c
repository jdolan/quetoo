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
	cg_particle_t *p;

	for (int32_t i = 0; i < 24; i++) {

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		p->origin = Vec3_Add(org, dir);

		p->velocity = Vec3_Scale(dir, 150.0);
		p->velocity = Vec3_Add(p->velocity, Vec3_RandomRange(-50.0, 50.0));
		p->acceleration.z = -2.5 * PARTICLE_GRAVITY;

		p->lifetime = RandomRangef(500, 1000);

		p->color = color;
		p->color_velocity.w = -1.f / MILLIS_TO_SECONDS(p->lifetime);

		p->size = 3.5;
		p->size_velocity = -p->size / MILLIS_TO_SECONDS(p->lifetime);

		p->bounce = 0.6f;
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = Vec3_Add(org, dir),
		.radius = 150.0,
		.color = Color_Vec3(color),
		.decay = 350
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
	cg_particle_t *p;

	if ((p = Cg_AllocParticle())) {

		p->origin = start;

		const float dist = Vec3_DistanceDir(end, start, &p->velocity);

		p->velocity = Vec3_Scale(p->velocity, 8000.f);
		p->acceleration.z = -PARTICLE_GRAVITY * 3.f;

		p->lifetime = SECONDS_TO_MILLIS(dist / 8000.f);

		p->color = Color3bv(0xffa050);
	}
}

/**
 * @brief
 */
static void Cg_BulletEffect(const vec3_t org, const vec3_t dir) {
	static uint32_t last_ric_time;
	cg_particle_t *p;
	vec3_t vec;

	if (cgi.PointContents(org) & CONTENTS_MASK_LIQUID) {
		vec = Vec3_Add(org, Vec3_Scale(dir, 8.f));
		Cg_BubbleTrail(NULL, org, vec, 32.0);
	} else {
		if ((p = Cg_AllocParticle())) {

			p->origin = org;
			p->velocity = Vec3_Scale(dir, RandomRangef(180.f, 400.f));

			p->acceleration = Vec3_RandomRange(-40.f, -40.f);
			p->acceleration.z -= PARTICLE_GRAVITY;

			p->bounce = 0.6f;
			p->lifetime = RandomRangef(150.f, 300.f);

			p->color = Color_Add(Color3bv(0xffa050), Color3fv(Vec3_RandomRange(-1.f, .1f)));
			p->color_velocity.w = -1.f / MILLIS_TO_SECONDS(p->lifetime);
		}

		if ((p = Cg_AllocParticle())) {

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
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = Vec3_Add(org, dir),
		.radius = 20.0,
		.color = Vec3(0.5f, 0.3f, 0.2f),
		.decay = 250
	});

	cgi.AddStain(&(const r_stain_t) {
		.origin = org,
		.radius = 2.0,
		.color = Color4bv(0x00000050),
	});

	if (cgi.client->unclamped_time < last_ric_time) {
		last_ric_time = 0;
	}

	if (cgi.client->unclamped_time - last_ric_time > 300) {
		last_ric_time = cgi.client->unclamped_time;

		cgi.AddSample(&(const s_play_sample_t) {
			.sample = cg_sample_machinegun_hit[Randomr(0, 3)],
			.origin = org,
			.attenuation = ATTEN_NORM,
			.flags = S_PLAY_POSITIONED,
			.pitch = (int16_t) (Randomc() * 8)
		});
	}
}

/**
 * @brief
 */
static void Cg_BloodEffect(const vec3_t org, const vec3_t dir, int32_t count) {

	for (int32_t i = 0; i < count; i++) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		p->origin = Vec3_Add(org, Vec3_RandomRange(-10.f, 10.f));
		p->origin = Vec3_Add(p->origin, Vec3_Scale(dir, RandomRangef(0.f, 32.f)));

		p->velocity = Vec3_RandomRange(-30.f, 30.f);
		p->acceleration.z = -PARTICLE_GRAVITY / 2.0;

		p->lifetime = 100 + Randomf() * 500;

		p->color = Color4bv(0x882200aa);
//		p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

		p->size = RandomRangef(5.0, 8.0);
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
	cg_particle_t *p;
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
		o = Vec3(Randomc() * 8.0, Randomc() * 8.0, 8.0 + Randomc() * 12.0);
		o = Vec3_Add(o, org);

		v = Vec3(Randomc(), Randomc(), 0.2 + Randomf());

		dist = GIB_STREAM_DIST;
		tmp = Vec3_Add(o, Vec3_Scale(v, dist));

		const cm_trace_t tr = cgi.Trace(o, tmp, Vec3_Zero(), Vec3_Zero(), 0, CONTENTS_MASK_CLIP_PROJECTILE);
		dist = GIB_STREAM_DIST * tr.fraction;

		for (int32_t j = 1; j < GIB_STREAM_COUNT; j++) {

			if (!(p = Cg_AllocParticle())) {
				break;
			}

			p->origin = o;

			p->velocity = Vec3_Scale(v, dist * ((float)j / GIB_STREAM_COUNT));
			p->velocity = Vec3_Add(p->velocity, Vec3_RandomRange(-2.f, 2.f));
			p->velocity.z += 100.f;

			p->acceleration.z = -PARTICLE_GRAVITY * 2.0;

			p->lifetime = 700 + Randomf() * 400;

			p->color = Color4bv(0x800000f8);
//			p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

			p->size = RandomRangef(3.0, 7.0);
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
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		p->origin = Vec3_Add(org, Vec3_RandomRange(-4.f, 4.f));

		p->velocity = Vec3_Add(Vec3_Scale(dir, 4.f), Vec3_RandomRange(-90.f, 90.f));

		p->acceleration = Vec3_RandomRange(-1.f, 1.f);
		p->acceleration.z += -0.5 * PARTICLE_GRAVITY;

		p->lifetime = 500 + Randomf() * 250;

		p->color = Color3b(Randomr(190, 255), Randomr(90, 140), Randomr(0, 20)); //cgi.ColorFromPalette(221 + (Randomr(0, 8)));
		p->color.a = (200 + Randomf() * 55) / 255.f;

		p->size = 0.6 + Randomf() * 0.2;

		p->bounce = 0.6f;
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
static void Cg_ExplosionEffect(const vec3_t org) {
	cg_particle_t *p;

	// TODO: Bubbles in water?
	
	if ((cgi.PointContents(org) & CONTENTS_MASK_LIQUID) == 0) {

		for (int32_t i = 0; i < 100; i++) {
			if (!(p = Cg_AllocParticle())) {
				break;
			}

			p->origin = Vec3_Add(org, Vec3_RandomRange(-10.f, 10.f));
			p->velocity = Vec3_RandomRange(-300.f, 300.f);
			p->acceleration.z = -PARTICLE_GRAVITY * 2.0;
			p->lifetime = 3000 + Randomf() * 300;

			// p->color = Color3b(Randomr(190, 255), Randomr(90, 140), Randomr(0, 20));
			p->color = Color3b(255, 255, 255);
			p->color.a = 255.f;
			
			p->color_velocity.x = -0.5f / MILLIS_TO_SECONDS(p->lifetime);
			p->color_velocity.y = -1.5f / MILLIS_TO_SECONDS(p->lifetime);
			p->color_velocity.z = -3.0f / MILLIS_TO_SECONDS(p->lifetime);
			p->bounce = .4f;

			p->size = .2f + Randomf() * .4f;
		}
	}

	cg_sprite_t *s;

	/*for (int32_t i = 0; i < 4; i++) {
		if ((s = Cg_AllocSprite())) {
			s->origin = Vec3_Add(org, Vec3_RandomRange(-8.f, 8.f));

			s->lifetime = RandomRangef(500, 950);

			s->color = Color3b(255, 255, 255);
			s->color_velocity.w = -1.f / MILLIS_TO_SECONDS(s->lifetime);

			s->size = RandomRangef(22.f, 48.f);
			s->size_velocity = RandomRangef(10.f, 25.f);

			s->velocity = Vec3_RandomRange(-5.f, 5.f);
			s->velocity.z += 52.f;

			s->acceleration.z = -8.0;

			s->image = cg_sprite_smoke;

			s->rotation = RandomRangef(0.0f, M_PI);

			s->rotation_velocity = RandomRangef(.1f, .5f);

			s->src = GL_SRC_ALPHA;
			s->dst = GL_ONE_MINUS_SRC_ALPHA;
		}
	}*/

	if ((s = Cg_AllocSprite())) {
		s->origin = org;
		s->lifetime = cg_fire_1->num_images * FRAMES_TO_SECONDS(80);
		s->color = color_white;
		s->size = 100.0;
		s->size_velocity = 25.0;
		s->src = GL_ONE;
		s->dst = GL_ONE_MINUS_SRC_ALPHA;
		s->animation = cg_fire_1;
		s->rotation = Randomf() * 2.f * M_PI;
	}

	if ((s = Cg_AllocSprite())) {
		s->origin = org;
		s->lifetime = cg_fire_1->num_images * FRAMES_TO_SECONDS(60);
		s->color = color_white;
		s->size = 175.0;
		s->size_velocity = 25.0;
		s->rotation = Randomf() * 2.f * M_PI;
		s->src = GL_ONE;
		s->dst = GL_ONE;
		s->animation = cg_fire_1;
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
	cg_particle_t *p;

	if ((cgi.PointContents(org) & CONTENTS_MASK_LIQUID) == 0) {
		for (int32_t i = 0; i < 6; i++) {

			if (!(p = Cg_AllocParticle())) {
				break;
			}

			p->origin = org;

			p->velocity = Vec3_Multiply(dir, Vec3_RandomRange(20.f, 140.f));
			p->velocity = Vec3_Add(p->velocity, Vec3_RandomRange(-16.f, 16.f));

			p->acceleration.z = -PARTICLE_GRAVITY * 2.0;

			p->lifetime = 200 + Randomf() * 500;

			p->color = Color4bv(0x22aaffff);
//			p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

			p->size = 3.5;
//			p->delta_size = Randomf() / PARTICLE_FRAME;

			p->bounce = 0.9f;

			p->color_velocity.w = -1.f / MILLIS_TO_SECONDS(p->lifetime);
		}
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = Vec3_Add(org, dir),
		.radius = 80.0,
		.color = Vec3(0.4, 0.7, 1.0),
		.decay = 250
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
	cg_particle_t *p;

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

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		p->origin = Vec3_Add(start, Vec3_Scale(forward, i));
		p->velocity = Vec3_Scale(forward, 20.f);

		const float cosi = cosf(i * 0.1f);
		const float sini = sinf(i * 0.1f);

		const float frac = (1.0 - (i / dist));

		p->origin = Vec3_Add(p->origin, Vec3_Scale(right, cosi));
		p->origin = Vec3_Add(p->origin, Vec3_Scale(up, sini));

		p->velocity = Vec3_Add(p->velocity, Vec3_Scale(right, cosi * frac));
		p->velocity = Vec3_Add(p->velocity, Vec3_Scale(up, sini * frac));

		p->acceleration = Vec3_Add(p->acceleration, Vec3_Scale(right, cosi * 8.f));
		p->acceleration = Vec3_Add(p->acceleration, Vec3_Scale(up, sini * 8.f));
		p->acceleration.z += 1.f;

		p->lifetime = RandomRangef(1500.f, 1550.f);

		p->color = Color_Add(color, Color4fv(Vec4_RandomRange(-.25f, .25f)));
		p->color.a = .66f;

		p->color_velocity = Vec4(-.125f, -.125f, -.125f, -p->color.a);
		p->color_velocity = Vec4_Scale(p->color_velocity, 1.f / MILLIS_TO_SECONDS(p->lifetime));

		p->color_acceleration = Vec4(-.125f, -.125f, -.125f, -.125f);
		p->color_acceleration = Vec4_Scale(p->color_acceleration, 1.f / MILLIS_TO_SECONDS(p->lifetime));

		p->size = frac;
		p->size_velocity = 1.f / MILLIS_TO_SECONDS(p->lifetime);
	}

	cg_sprite_t *s;

	if ((s = Cg_AllocSprite())) {
		s->type = SPRITE_BEAM;
		s->origin = start;
		s->beam.end = end;
		s->size = 20.f;
		s->velocity = Vec3_Scale(forward, 20.f);
		s->lifetime = RandomRangef(1500.f, 1550.f);
		s->color = Color_Add(color, color_white);
		s->color.a = .7f;
		s->color_velocity.w = -s->color.a / MILLIS_TO_SECONDS(s->lifetime);
		s->image = cg_beam_rail;
	}

	// Check for explosion effect on solids

	if (flags & SURF_SKY) {
		return;
	}

	// Rail impact sparks

	if (cg_particle_quality->integer && (cgi.PointContents(end) & CONTENTS_MASK_LIQUID) == 0) {

		for (int32_t i = 0; i < 24; i++) {
			if (!(p = Cg_AllocParticle())) {
				break;
			}

			p->origin = end;

			p->velocity = Vec3_Scale(dir, 400.0 + Randomf() * 200);
			p->velocity = Vec3_Add(p->velocity, Vec3_RandomRange(-100.f, 100.f));

			p->acceleration.z = -PARTICLE_GRAVITY * 2.0;

			p->lifetime = 50 + Randomf() * 100;

			p->color = color;

			p->size = 1.6 + Randomf() * 0.6;
		}
	}

	// Impact light

	Cg_AddLight(&(cg_light_t) {
		.origin = Vec3_Add(end, Vec3_Scale(forward, -12.f)),
		.radius = 120.f,
		.color = Color_Vec3(color),
		.decay = 250
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
	cg_particle_t *p;
	cg_light_t l;

	if ((p = Cg_AllocParticle())) {
		p->origin = org;

		p->lifetime = 50;

		p->color = color_green;//cgi.ColorFromPalette(200 + Randomr(0, 3));

		p->size = 6.0;
	}

	l.origin = end;
	l.radius = 80.0;
	l.color = Vec3(0.8, 1.0, 0.5);
	l.decay = 50;

	Cg_AddLight(&l);
}

/**
 * @brief
 */
static void Cg_BfgEffect(const vec3_t org) {
	cg_particle_t *p;

	for (int32_t i = 0; i < 128; i++) {

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		p->origin = Vec3_Add(org, Vec3_RandomRange(-24.f, 24.f));
		p->velocity = Vec3_RandomRange(-256.f, 256.f);
		p->acceleration = Vec3(0.0, 0.0, -3.0 * PARTICLE_GRAVITY);

		p->lifetime = 750;

		p->color = Color3b(60, 224, 72);
//		p->delta_color.g = 0x10;
//		p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

		p->size = 2.0;
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = org,
		.radius = 200.0,
		.color = Vec3(0.8, 1.0, 0.5),
		.decay = 1000
	});

	cgi.AddStain(&(const r_stain_t) {
		.origin = org,
		.radius = 96.0,
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
	cg_particle_t *p;

	if (cg_particle_quality->integer == 0) {
		return;
	}

	if (!(p = Cg_AllocParticle())) {
		return;
	}

	p->origin = org;

	p->lifetime = Randomr(500, 1500) * (viscosity * 0.1);

	p->color = Color4bv(0x8080a0ff);
//	p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

	p->size = size + Randomc() * 0.5;
//	p->delta_size = 3.0 + Randomf() * 0.5 / PARTICLE_FRAME;
}

/**
 * @brief
 */
static void Cg_SplashEffect(const vec3_t org, const vec3_t dir) {
	cg_particle_t *p;

	for (int32_t i = 0; i < 10; i++) {
		if (!(p = Cg_AllocParticle())) {
			break;
		}

		p->origin = Vec3_Add(org, Vec3_RandomRange(-8.f, 8.f));
		p->velocity = Vec3_RandomRange(-64.f, 64.f);
		p->velocity.z += 36.f;

		p->acceleration = Vec3(0.0, 0.0, -PARTICLE_GRAVITY / 2.0);

		p->lifetime = 250 + Randomc() * 150;

		p->color = color_white;
//		p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

		p->size = 0.8;
//		p->delta_size = 0.3 * -p->lifetime / PARTICLE_FRAME;
	}

	if ((p = Cg_AllocParticle())) {

		p->origin = org;

		p->velocity = Vec3_Scale(dir, 70.0 + Randomf() * 30.0);
		p->velocity = Vec3_Add(p->velocity, Vec3_RandomRange(-8.f, 8.f));

		p->acceleration = Vec3(0.0, 0.0, -PARTICLE_GRAVITY / 2.0);

		p->lifetime = 120 + Randomf() * 80;

		p->color = Color4bv(0x80e0e0e0);
//		p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

		p->size = 1.4 + Randomf() * 0.7;
	}
}

/**
 * @brief
 */
static void Cg_HookImpactEffect(const vec3_t org, const vec3_t dir) {

	for (int32_t i = 0; i < 32; i++) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		p->origin = Vec3_Add(org, Vec3_RandomRange(-4.f, 4.f));
		p->velocity = Vec3_Add(Vec3_Scale(dir, 9.f), Vec3_RandomRange(-90.f, 90.f));
		p->acceleration = Vec3_RandomRange(-2.f, 2.f);
		p->acceleration.z += -0.5 * PARTICLE_GRAVITY;

		p->lifetime = 100 + (Randomf() * 150);

		p->color = Color3b(248, 224, 40);
		p->color.a = (200 * Randomf() * 55) / 255.f;

		p->size = 0.8 + Randomf() * 0.4;
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
			Cg_ExplosionEffect(pos);
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
