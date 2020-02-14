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
		p->acceleration.z -= 2.0 * PARTICLE_GRAVITY;

		p->lifetime = 450 + g_random_double_range(0., 450.);

		p->color = color;
		p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

		p->size = 3.5;
		p->delta_size = -450 / PARTICLE_FRAME;

		p->bounce = 1.15;
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = Vec3_Add(org, dir),
		.radius = 150.0,
		.color = ColorToVector3(color),
		.decay = 350
	});

//	cgi.AddStain(&(const r_stain_t) {
//		.origin = { org[0], org[1], org[2] },
//		.radius = 4.0,
//		.media = cg_particles_default->media,
//		.color = { c[0], c[1], c[2], 0.33 }
//	});

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

		p->velocity = Vec3_Subtract(end, start);
		p->velocity = Vec3_Add(p->velocity, Vec3_Scale(p->velocity, 4.f)); // Ghetto indeed
		p->acceleration.z = -PARTICLE_GRAVITY * 3.f;

		p->lifetime = 0.1f * Vec3_Distance(start, end); // 0.1ms per unit in distance

		p->color.r = 255;
		p->color.g = 220;
		p->color.b = 80;

		p->size = 1.5f;
		p->delta_size = -p->lifetime / PARTICLE_FRAME;
	}
}

/**
 * @brief
 */
static void Cg_BulletEffect(const vec3_t org, const vec3_t dir) {
	static uint32_t last_ric_time;
	cg_particle_t *p;
	int32_t k = Randomr(1, 5);
	vec3_t vec;

	if (cgi.PointContents(org) & MASK_LIQUID) {
		vec = Vec3_Add(org, Vec3_Scale(dir, 8.f));
		Cg_BubbleTrail(org, vec, 32.0);
	} else {
		while (k--) {
			if ((p = Cg_AllocParticle())) {

				p->origin = org;
				p->velocity = Vec3_Scale(dir, 180.f + Randomf() * 40.f);

				p->acceleration = Vec3_RandomRange(-40.f, -40.f);
				p->acceleration.z -= PARTICLE_GRAVITY;

				p->bounce = 1.5f;
				p->lifetime = 100 + Randomf() * 350;

				cgi.ColorFromPalette(221 + (Randomr(0, 8)), &p->color);
				p->color.a = 200 + Randomf() * 55;

				p->size = 0.6f + Randomf() * 0.4f;
			}
		}

		if ((p = Cg_AllocParticle())) {

			p->origin = org;
			p->velocity = Vec3_Scale(dir, 60.f + (Randomc() * 60.f));
			p->acceleration = Vec3_Scale(dir, -80.f);
			p->acceleration = Vec3_Add(p->acceleration, Vec3_Scale(Vec3_Up(), 20.f));

			p->lifetime = 150 + Randomf() * 600;

			p->color = Color4bv(0xf0f0f0f0);
			p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

			p->size = 2.f + Randomf() * 2.f;
			p->delta_size = 0.1f + Randomf() * 8.f;
		}
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = Vec3_Add(org, dir),
		.radius = 20.0,
		.color = Vec3(0.5f, 0.3f, 0.2f),
		.decay = 250
	});

//	cgi.AddStain(&(const r_stain_t) {
//		.origin = { org[0], org[1], org[2] },
//		.radius = 2.0,
//		.media = cg_particles_default->media,
//		.color = { 0.0, 0.0, 0.0, 0.33 },
//	});

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
		p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

		p->size = Randomfr(5.0, 8.0);
	}

//	cgi.AddStain(&(const r_stain_t) {
//		.origin = { org[0], org[1], org[2] },
//		.radius = count * 6.0,
//		.media = cg_stain_blood,
//		.color = { 0.5 + (Randomf() * 0.3), 0.0, 0.0, 0.1 + Randomf() * 0.2 },
//	});
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
	if (cgi.PointContents(org) & MASK_LIQUID) {
		tmp = org;
		tmp.z += 64.0;

		Cg_BubbleTrail(org, tmp, 16.0);
	}

	for (int32_t i = 0; i < count; i++) {

		// set the origin and velocity for each gib stream
		o = Vec3(Randomc() * 8.0, Randomc() * 8.0, 8.0 + Randomc() * 12.0);
		o = Vec3_Add(o, org);

		v = Vec3(Randomc(), Randomc(), 0.2 + Randomf());

		dist = GIB_STREAM_DIST;
		tmp = Vec3_Add(o, Vec3_Scale(v, dist));

		const cm_trace_t tr = cgi.Trace(o, tmp, Vec3_Zero(), Vec3_Zero(), 0, MASK_CLIP_PROJECTILE);
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
			p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

			p->size = Randomfr(3.0, 7.0);
		}
	}

//	cgi.AddStain(&(const r_stain_t) {
//		.origin = { org[0], org[1], org[2] },
//		.radius = count * 6.0,
//		.media = cg_particles_default->media,
//		.color = { 0.5 + (Randomf() * 0.3), 0.0, 0.0, 0.1 + Randomf() * 0.2 },
//	});

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

		cgi.ColorFromPalette(221 + (Randomr(0, 8)), &p->color);
		p->color.a = 200 + Randomf() * 55;

		p->size = 0.6 + Randomf() * 0.2;

		p->bounce = 1.15;
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

	if ((cgi.PointContents(org) & MASK_LIQUID) == 0) {

		for (int32_t i = 0; i < 40; i++) {
			if (!(p = Cg_AllocParticle())) {
				break;
			}

			p->origin = Vec3_Add(org, Vec3_RandomRange(-10.f, 10.f));
			p->velocity = Vec3_RandomRange(-180.f, 180.f);
			p->acceleration.z = -PARTICLE_GRAVITY * 2.0;
			p->lifetime = 200 + Randomf() * 300;

			cgi.ColorFromPalette(221 + (Randomr(0, 8)), &p->color);
			p->color.a = 200 + Randomf() * 55;

			p->size = 1.0 + Randomf() * 0.4;
		}
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = org,
		.radius = 200.0,
		.color = Vec3(0.8, 0.4, 0.2),
		.decay = 1000
	});

//	vec3_t c;
//	cgi.ColorFromPalette(Randomr(0, 2), c);

//	cgi.AddStain(&(const r_stain_t) {
//		.origin = { org[0], org[1], org[2] },
//		.radius = 88.0 + (64.0 * Randomc() * 0.15),
//		.media = cg_stain_explosion,
//		.color = { c[0], c[1], c[2], 0.66 },
//	});

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

	if ((cgi.PointContents(org) & MASK_LIQUID) == 0) {
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
			p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

			p->size = 3.5;
			p->delta_size = Randomf() / PARTICLE_FRAME;

			p->bounce = 1.15;
		}
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = Vec3_Add(org, dir),
		.radius = 80.0,
		.color = Vec3(0.4, 0.7, 1.0),
		.decay = 250
	});

//	cgi.AddStain(&(const r_stain_t) {
//		.origin = { org[0], org[1], org[2] },
//		.radius = 16.0,
//		.media = cg_particles_default->media,
//		.color = { color[0], color[1], color[2], 0.33 },
//	});

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
		Cg_BubbleTrail(org, Vec3_Add(org, Vec3_RandomRange(-48.f, 48.f)), 4.0);
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
	vec3_t vec, right, up, point;
	cg_particle_t *p;
	cg_light_t l;

	l.origin = start;
	l.radius = 100.0;
	l.color = ColorToVector3(color);
	l.decay = 500;

	Cg_AddLight(&l);

	point = start;

	vec = Vec3_Subtract(end, start);

	float len;
	vec = Vec3_NormalizeLength(vec, &len);

	right = Vec3(vec.z, -vec.x, vec.y);
	up = Vec3_Cross(vec, right);

	for (int32_t i = 0; i < len; i++) {

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		point = Vec3_Add(point, vec);
		p->origin = point;

		p->velocity = Vec3_Scale(vec, 20.0);

		const float cosi = cosf(i * 0.03);
		const float sini = sinf(i * 0.03);

		p->origin = Vec3_Add(p->origin, Vec3_Scale(right, cosi * 1.0));
		p->origin = Vec3_Add(p->origin, Vec3_Scale(up, sini * 1.0));

		p->velocity = Vec3_Add(p->velocity, Vec3_Scale(right, cosi * 10.0));
		p->velocity = Vec3_Add(p->velocity, Vec3_Scale(up, sini * 10.0));

		p->acceleration = Vec3_Add(right, up);

		p->lifetime = 1500 + Randomf() * 50;

		p->color = color;
		p->delta_color.r =  1;
		p->delta_color.g =  1;
		p->delta_color.b =  1;
		p->delta_color.a = -4;

		p->size = 5.0;

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		p->origin = point;
		p->velocity = Vec3_Scale(vec, 20.0);

		p->lifetime = 1500 + Randomf() * 50;

		p->color = color;
		p->color.a = 200;
		p->delta_color.r =  4;
		p->delta_color.g =  4;
		p->delta_color.b =  4;
		p->delta_color.a = -4;

		p->size = 1.0;

		// Check for bubble trail

		if (i % 6 == 0 && (cgi.PointContents(point) & MASK_LIQUID)) {
			Cg_BubbleTrail(point, p->origin, 16.0);
		}
	}

	// Check for explosion effect on solids

	if (flags & SURF_SKY) {
		return;
	}

	// Rail impact sparks

	if (cg_particle_quality->integer && (cgi.PointContents(end) & MASK_LIQUID) == 0) {

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

	l.origin = Vec3_Add(end, Vec3_Scale(vec, -12.0));
	l.radius = 120.0;
	l.decay += 250;

	Cg_AddLight(&l);

//	cgi.AddStain(&(const r_stain_t) {
//		.origin = { end[0], end[1], end[2] },
//		.radius = 8.0,
//		.media = cg_particles_default->media,
//		.color = { l.color[0], l.color[1], l.color[2], 0.66 },
//	});
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

		cgi.ColorFromPalette(200 + Randomr(0, 3), &p->color);

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

		cgi.ColorFromPalette(206, &p->color);
		p->delta_color.g = 0x10;
		p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

		p->size = 2.0;
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = org,
		.radius = 200.0,
		.color = Vec3(0.8, 1.0, 0.5),
		.decay = 1000
	});

//	vec3_t c;
//	cgi.ColorFromPalette(203 + Randomr(0, 3), c);
//
//	cgi.AddStain(&(const r_stain_t) {
//		.origin = { org[0], org[1], org[2] },
//		.radius = 96.0,
//		.media = cg_particles_default->media,
//		.color = { c[0], c[1], c[2], 0.75 },
//	});

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

	p->color = Color4bv(0x8080a0FF);
	p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

	p->size = size + Randomc() * 0.5;
	p->delta_size = 3.0 + Randomf() * 0.5 / PARTICLE_FRAME;
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
		p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

		p->size = 0.8;
		p->delta_size = 0.3 * -p->lifetime / PARTICLE_FRAME;
	}

	if ((p = Cg_AllocParticle())) {

		p->origin = org;

		p->velocity = Vec3_Scale(dir, 70.0 + Randomf() * 30.0);
		p->velocity = Vec3_Add(p->velocity, Vec3_RandomRange(-8.f, 8.f));

		p->acceleration = Vec3(0.0, 0.0, -PARTICLE_GRAVITY / 2.0);

		p->lifetime = 120 + Randomf() * 80;

		p->color = Color4bv(0x80e0e0e0);
		p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

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

		cgi.ColorFromPalette(221 + (Randomr(0, 8)), &p->color);
		p->color.a = 200 * Randomf() * 55;

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
			Cg_BubbleTrail(pos, pos2, 1.0);
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
