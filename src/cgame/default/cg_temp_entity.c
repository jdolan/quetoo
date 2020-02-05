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

		VectorAdd(org, dir, p->origin);

		VectorScale(dir, 150.0, p->velocity);

		for (int32_t j = 0; j < 3; j++) {
			p->velocity[j] += Randomc() * 50.0;
		}

		p->acceleration[2] -= 2.0 * PARTICLE_GRAVITY;

		p->lifetime = 450 + Randomf() * 450;

		p->color = color;
		p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

		p->size = 3.5;
		p->delta_size = -450 / PARTICLE_FRAME;

		p->bounce = 1.15;
	}

	vec3_t c;
	ColorToVec3(color, c);

	Cg_AddLight(&(const cg_light_t) {
		.origin = { org[0] + dir[0], org[1] + dir[1], org[2] + dir[2] },
		.radius = 150.0,
		.color = { c[0], c[1], c[2] },
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
		.origin = { org[0], org[1], org[2] },
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

		VectorCopy(start, p->origin);

		VectorSubtract(end, start, p->velocity);
		VectorMA(p->velocity, 4.0, p->velocity, p->velocity); // Ghetto indeed

		p->acceleration[2] = -PARTICLE_GRAVITY * 3.0;

		p->lifetime = 0.1 * VectorDistance(start, end); // 0.1ms per unit in distance

		p->color.r = 255;
		p->color.g = 220;
		p->color.b = 80;

		p->size = 1.5;
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
		VectorMA(org, 8.0, dir, vec);
		Cg_BubbleTrail(org, vec, 32.0);
	} else {
		while (k--) {
			if ((p = Cg_AllocParticle())) {

				VectorCopy(org, p->origin);
				VectorScale(dir, 180.0 + Randomf() * 40.0, p->velocity);

				p->acceleration[0] = Randomc() * 40.0;
				p->acceleration[1] = Randomc() * 40.0;
				p->acceleration[2] = -(PARTICLE_GRAVITY + (Randomf() * 60.0));

				p->bounce = 1.5;
				p->lifetime = 100 + Randomf() * 350;

				cgi.ColorFromPalette(221 + (Randomr(0, 8)), &p->color);
				p->color.a = 200 + Randomf() * 55;

				p->size = 0.6 + Randomf() * 0.4;
			}
		}

		if ((p = Cg_AllocParticle())) {

			VectorCopy(org, p->origin);
			VectorScale(dir, 60.0 + (Randomc() * 60.0), p->velocity);
			VectorScale(dir, -80.0, p->acceleration);
			VectorMA(p->acceleration, 20.0, vec3_up, p->acceleration);

			p->lifetime = 150 + Randomf() * 600;

			p->color.abgr = 0xf0f0f0f0;
			p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

			p->size = 2.0 + Randomf() * 2.0;
			p->delta_size = 0.1 + Randomf() * 8.0;
		}
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = { org[0] + dir[0], org[1] + dir[1], org[2] + dir[2] },
		.radius = 20.0,
		.color = { 0.5, 0.3, 0.2 },
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
			.origin = { org[0], org[1], org[2] },
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

		const vec_t d = Randomr(0, 32);
		for (int32_t j = 0; j < 3; j++) {
			p->origin[j] = org[j] + Randomfr(-10.0, 10.0) + d * dir[j];
			p->velocity[j] = Randomc() * 30.0;
		}

		p->acceleration[0] = p->acceleration[1] = 0.0;
		p->acceleration[2] = -PARTICLE_GRAVITY / 2.0;

		p->lifetime = 100 + Randomf() * 500;

		p->color.abgr = 0xaa002288;
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
	vec_t dist;

	// if a player has died underwater, emit some bubbles
	if (cgi.PointContents(org) & MASK_LIQUID) {
		VectorCopy(org, tmp);
		tmp[2] += 64.0;

		Cg_BubbleTrail(org, tmp, 16.0);
	}

	for (int32_t i = 0; i < count; i++) {

		// set the origin and velocity for each gib stream
		VectorSet(o, Randomc() * 8.0, Randomc() * 8.0, 8.0 + Randomc() * 12.0);
		VectorAdd(o, org, o);

		VectorSet(v, Randomc(), Randomc(), 0.2 + Randomf());

		dist = GIB_STREAM_DIST;
		VectorMA(o, dist, v, tmp);

		const cm_trace_t tr = cgi.Trace(o, tmp, NULL, NULL, 0, MASK_CLIP_PROJECTILE);
		dist = GIB_STREAM_DIST * tr.fraction;

		for (int32_t j = 1; j < GIB_STREAM_COUNT; j++) {

			if (!(p = Cg_AllocParticle())) {
				break;
			}

			VectorCopy(o, p->origin);

			VectorScale(v, dist * ((vec_t)j / GIB_STREAM_COUNT), p->velocity);
			p->velocity[0] += Randomc() * 2.0;
			p->velocity[1] += Randomc() * 2.0;
			p->velocity[2] += 100.0;

			p->acceleration[0] = p->acceleration[1] = 0.0;
			p->acceleration[2] = -PARTICLE_GRAVITY * 2.0;

			p->lifetime = 700 + Randomf() * 400;

			p->color.abgr = 0x800000f8;
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
		.origin = { org[0], org[1], org[2] },
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

		VectorCopy(org, p->origin);
		VectorScale(dir, 4, p->velocity);

		for (int32_t j = 0; j < 3; j++) {
			p->origin[j] += Randomc() * 4.0;
			p->velocity[j] += Randomc() * 90.0;
		}

		p->acceleration[0] = Randomc() * 1.0;
		p->acceleration[1] = Randomc() * 1.0;
		p->acceleration[2] = -0.5 * PARTICLE_GRAVITY;

		p->lifetime = 500 + Randomf() * 250;

		cgi.ColorFromPalette(221 + (Randomr(0, 8)), &p->color);
		p->color.a = 200 + Randomf() * 55;

		p->size = 0.6 + Randomf() * 0.2;

		p->bounce = 1.15;
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = { org[0], org[1], org[2] },
		.radius = 80.0,
		.color = { 0.7, 0.5, 0.5 },
		.decay = 650
	});

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_sparks,
		.origin = { org[0], org[1], org[2] },
		.attenuation = ATTEN_STATIC,
		.flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
static void Cg_ExplosionEffect(const vec3_t org) {
	cg_particle_t *p;

	if ((cgi.PointContents(org) & MASK_LIQUID) == 0) {

		for (int32_t i = 0; i < 10; i++) {

			if (!(p = Cg_AllocParticle())) {
				break;
			}

			p->origin[0] = org[0] + Randomc() * 16.0;
			p->origin[1] = org[1] + Randomc() * 16.0;
			p->origin[2] = org[2] + Randomc() * 16.0;

			VectorSet(p->velocity, Randomc() * 50.0, Randomc() * 50.0, Randomc() * 50.0);
			VectorSet(p->acceleration, -p->velocity[0], -p->velocity[1], -p->velocity[2]);

			p->acceleration[2] += PARTICLE_GRAVITY / Randomfr(6.0, 9.0);

			p->lifetime = 1500 + (Randomc() * 500);

			p->color.abgr = 0x802080f0;
			p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

			p->size = 40.0;
			p->delta_size = 0.5 * -p->lifetime / PARTICLE_FRAME;
		}

		for (int32_t i = 0; i < 40; i++) {
			if (!(p = Cg_AllocParticle())) {
				break;
			}

			VectorCopy(org, p->origin);

			p->velocity[0] = Randomc() * 170.0;
			p->velocity[1] = Randomc() * 170.0;
			p->velocity[2] = Randomc() * 170.0;

			p->acceleration[2] = -PARTICLE_GRAVITY * 2.0;

			p->lifetime = 200 + Randomf() * 300;

			cgi.ColorFromPalette(221 + (Randomr(0, 8)), &p->color);
			p->color.a = 200 + Randomf() * 55;

			p->size = 0.9 + Randomf() * 0.4;
		}
	}

	if (cg_particle_quality->integer) {
		for (int32_t i = 0; i < 24; i++) {

			if (!(p = Cg_AllocParticle())) {
				break;
			}

			p->origin[0] = org[0] + Randomfr(-16.0, 16.0);
			p->origin[1] = org[1] + Randomfr(-16.0, 16.0);
			p->origin[2] = org[2] + Randomfr(-16.0, 16.0);

			p->velocity[0] = Randomc() * 800.0;
			p->velocity[1] = Randomc() * 800.0;
			p->velocity[2] = Randomc() * 800.0;

			p->acceleration[2] = -PARTICLE_GRAVITY * 2;

			p->lifetime = 400 + Randomf() * 300;

			p->color.abgr = 0x80404040;
			p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

			p->size = 3.0;
		}
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = { org[0], org[1], org[2] },
		.radius = 200.0,
		.color = { 0.8, 0.4, 0.2 },
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
		.origin = { org[0], org[1], org[2] },
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

			VectorCopy(org, p->origin);

			p->velocity[0] = dir[0] * Randomfr(20.0, 140.0) + Randomc() * 16.0;
			p->velocity[1] = dir[1] * Randomfr(20.0, 140.0) + Randomc() * 16.0;
			p->velocity[2] = dir[2] * Randomfr(20.0, 140.0) + Randomc() * 16.0;

			p->acceleration[2] = -PARTICLE_GRAVITY * 2.0;

			p->lifetime = 200 + Randomf() * 500;

			p->color.abgr = 0xffffaa22;
			p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

			p->size = 3.5;
			p->delta_size = Randomf() / PARTICLE_FRAME;

			p->bounce = 1.15;
		}
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = { org[0] + dir[0], org[1] + dir[1], org[2] + dir[2] },
		.radius = 80.0,
		.color = { 0.4, 0.7, 1.0 },
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
		.origin = { org[0], org[1], org[2] },
		.attenuation = ATTEN_NORM,
		.flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
static void Cg_LightningDischargeEffect(const vec3_t org) {

	for (int32_t i = 0; i < 40; i++) {
		vec3_t tmp;
		VectorCopy(org, tmp);

		for (int32_t j = 0; j < 3; j++) {
			tmp[j] = tmp[j] + Randomfr(-48.0, 48.0);
		}

		Cg_BubbleTrail(org, tmp, 4.0);
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = { org[0], org[1], org[2] },
		.radius = 160.0,
		.color = { 0.6, 0.6, 1.0 },
		.decay = 750
	});

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_lightning_discharge,
		.origin = { org[0], org[1], org[2] },
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

	VectorCopy(start, l.origin);
	l.radius = 100.0;
	ColorToVec3(color, l.color);
	l.decay = 500;

	Cg_AddLight(&l);

	VectorCopy(start, point);

	VectorSubtract(end, start, vec);

	const int32_t len = VectorNormalize(vec);

	VectorSet(right, vec[2], -vec[0], vec[1]);
	CrossProduct(vec, right, up);

	for (int32_t i = 0; i < len; i++) {

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		VectorAdd(point, vec, point);
		VectorCopy(point, p->origin);

		VectorScale(vec, 20.0, p->velocity);

		const vec_t cosi = cosf(i * 0.03);
		const vec_t sini = sinf(i * 0.03);

		VectorMA(p->origin, cosi * 1.0, right, p->origin);
		VectorMA(p->origin, sini * 1.0, up, p->origin);

		VectorMA(p->velocity, cosi * 10.0, right, p->velocity);
		VectorMA(p->velocity, sini * 10.0, up, p->velocity);

		VectorAdd(right, up, p->acceleration);

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

		VectorCopy(point, p->origin);
		VectorScale(vec, 20.0, p->velocity);

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

			VectorCopy(end, p->origin);

			VectorScale(dir, 400.0 + Randomf() * 200, p->velocity);

			p->velocity[0] += Randomc() * 100;
			p->velocity[1] += Randomc() * 100;
			p->velocity[2] += Randomc() * 100;

			p->acceleration[2] = -PARTICLE_GRAVITY * 2.0;

			p->lifetime = 50 + Randomf() * 100;

			p->color = color;

			p->size = 1.6 + Randomf() * 0.6;
		}
	}

	// Impact light

	VectorMA(end, -12.0, vec, l.origin);
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
		VectorCopy(org, p->origin);

		p->lifetime = 50;

		cgi.ColorFromPalette(200 + Randomr(0, 3), &p->color);

		p->size = 6.0;
	}

	VectorCopy(end, l.origin);
	l.radius = 80.0;
	VectorSet(l.color, 0.8, 1.0, 0.5);
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

		p->origin[0] = org[0] + Randomfr(-24.0, 24.0);
		p->origin[1] = org[1] + Randomfr(-24.0, 24.0);
		p->origin[2] = org[2] + Randomfr(-24.0, 24.0);

		p->velocity[0] = Randomfr(-256.0, 256.0);
		p->velocity[1] = Randomfr(-256.0, 256.0);
		p->velocity[2] = Randomfr(-256.0, 256.0);

		VectorSet(p->acceleration, 0.0, 0.0, -3.0 * PARTICLE_GRAVITY);

		p->lifetime = 750;

		cgi.ColorFromPalette(206, &p->color);
		p->delta_color.g = 0x10;
		p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

		p->size = 2.0;
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = { org[0], org[1], org[2] },
		.radius = 200.0,
		.color = { 0.8, 1.0, 0.5 },
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
		.origin = { org[0], org[1], org[2] },
		.attenuation = ATTEN_NORM,
		.flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
void Cg_RippleEffect(const vec3_t org, const vec_t size, const uint8_t viscosity) {
	cg_particle_t *p;

	if (cg_particle_quality->integer == 0) {
		return;
	}

	if (!(p = Cg_AllocParticle())) {
		return;
	}

	VectorCopy(org, p->origin);

	p->lifetime = Randomr(500, 1500) * (viscosity * 0.1);

	p->color.abgr = 0x80a08080;
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

		p->origin[0] = org[0] + (Randomc() * 8.0);
		p->origin[1] = org[1] + (Randomc() * 8.0);
		p->origin[2] = org[2] + (Randomf() * 8.0);

		p->velocity[0] = Randomc() * 64.0;
		p->velocity[1] = Randomc() * 64.0;
		p->velocity[2] = 36.0 + Randomf() * 64.0;

		VectorSet(p->acceleration, 0.0, 0.0, -PARTICLE_GRAVITY / 2.0);

		p->lifetime = 250 + Randomc() * 150;

		p->color.abgr = 0xffffffff;
		p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

		p->size = 0.8;
		p->delta_size = 0.3 * -p->lifetime / PARTICLE_FRAME;
	}

	if ((p = Cg_AllocParticle())) {

		VectorCopy(org, p->origin);

		VectorScale(dir, 70.0 + Randomf() * 30.0, p->velocity);
		p->velocity[0] += Randomc() * 8.0;
		p->velocity[1] += Randomc() * 8.0;

		VectorSet(p->acceleration, 0.0, 0.0, -PARTICLE_GRAVITY / 2.0);

		p->lifetime = 120 + Randomf() * 80;

		p->color.abgr = 0x80e0e0e0;
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

		VectorCopy(org, p->origin);

		VectorScale(dir, 9, p->velocity);

		for (int32_t j = 0; j < 3; j++) {
			p->origin[j] += Randomc() * 4.0;
			p->velocity[j] += Randomc() * 90.0;
		}

		p->acceleration[0] = Randomc() * 2.0;
		p->acceleration[1] = Randomc() * 2.0;
		p->acceleration[2] = -0.5 * PARTICLE_GRAVITY;

		p->lifetime = 100 + (Randomf() * 150);

		cgi.ColorFromPalette(221 + (Randomr(0, 8)), &p->color);
		p->color.a = 200 * Randomf() * 55;

		p->size = 0.8 + Randomf() * 0.4;
	}

	vec3_t v;
	VectorAdd(org, dir, v);

	Cg_AddLight(&(const cg_light_t) {
		.origin = { v[0], v[1], v[2] },
		.radius = 80.0,
		.color = { 0.7, 0.5, 0.5 },
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
			cgi.ReadPosition(pos);
			cgi.ReadDir(dir);
			i = cgi.ReadByte();
			Cg_BlasterEffect(pos, dir, Cg_ResolveEffectColor(i ? i - 1 : 0, EFFECT_COLOR_ORANGE));
			break;

		case TE_TRACER:
			cgi.ReadPosition(pos);
			cgi.ReadPosition(pos2);
			Cg_TracerEffect(pos, pos2);
			break;

		case TE_BULLET: // bullet hitting wall
			cgi.ReadPosition(pos);
			cgi.ReadDir(dir);
			Cg_BulletEffect(pos, dir);
			break;

		case TE_BLOOD: // projectile hitting flesh
			cgi.ReadPosition(pos);
			cgi.ReadDir(dir);
			Cg_BloodEffect(pos, dir, 12);
			break;

		case TE_GIB: // player over-death
			cgi.ReadPosition(pos);
			Cg_GibEffect(pos, 12);
			break;

		case TE_SPARKS: // colored sparks
			cgi.ReadPosition(pos);
			cgi.ReadDir(dir);
			Cg_SparksEffect(pos, dir, 12);
			break;

		case TE_HYPERBLASTER: // hyperblaster hitting wall
			cgi.ReadPosition(pos);
			cgi.ReadDir(dir);
			Cg_HyperblasterEffect(pos, dir);
			break;

		case TE_LIGHTNING_DISCHARGE: // lightning discharge in water
			cgi.ReadPosition(pos);
			Cg_LightningDischargeEffect(pos);
			break;

		case TE_RAIL: // railgun effect
			cgi.ReadPosition(pos);
			cgi.ReadPosition(pos2);
			cgi.ReadDir(dir);
			i = cgi.ReadLong();
			j = cgi.ReadByte();
			Cg_RailEffect(pos, pos2, dir, i, Cg_ResolveEffectColor(j ? j - 1 : 0, EFFECT_COLOR_BLUE));
			break;

		case TE_EXPLOSION: // rocket and grenade explosions
			cgi.ReadPosition(pos);
			Cg_ExplosionEffect(pos);
			break;

		case TE_BFG_LASER:
			cgi.ReadPosition(pos);
			cgi.ReadPosition(pos2);
			Cg_BfgLaserEffect(pos, pos2);
			break;

		case TE_BFG: // bfg explosion
			cgi.ReadPosition(pos);
			Cg_BfgEffect(pos);
			break;

		case TE_BUBBLES: // bubbles chasing projectiles in water
			cgi.ReadPosition(pos);
			cgi.ReadPosition(pos2);
			Cg_BubbleTrail(pos, pos2, 1.0);
			break;

		case TE_RIPPLE: // liquid surface ripples
			cgi.ReadPosition(pos);
			cgi.ReadDir(dir);
			i = cgi.ReadByte();
			j = cgi.ReadByte();
			Cg_RippleEffect(pos, i, j);
			if (cgi.ReadByte()) {
				Cg_SplashEffect(pos, dir);
			}
			break;

		case TE_HOOK_IMPACT: // grapple hook impact
			cgi.ReadPosition(pos);
			cgi.ReadDir(dir);
			Cg_HookImpactEffect(pos, dir);
			break;

		default:
			cgi.Warn("Unknown type: %d\n", type);
			return;
	}
}
