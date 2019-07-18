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

		if (!(p = Cg_AllocParticle(cg_particles_spark))) {
			break;
		}

		p->effects = PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE | PARTICLE_EFFECT_BOUNCE;
		p->lifetime = 450 + Randomf() * 450;

		ColorToVec4(color, p->color_start);
		p->color_start[3] = 2.0;
		VectorCopy(p->color_start, p->color_end);
		p->color_end[3] = 0.0;

		p->scale_start = 3.5;
		p->scale_end = Randomf() * 1.0;

		p->bounce = 1.15;

		VectorAdd(org, dir, p->part.org);

		VectorScale(dir, 150.0, p->vel);

		for (int32_t j = 0; j < 3; j++) {
			p->vel[j] += Randomc() * 50.0;
		}

		p->accel[2] -= 2.0 * PARTICLE_GRAVITY;

		p->spark.length = 0.1 + Randomc() * 0.05;

		VectorMA(p->part.org, p->spark.length, p->vel, p->part.end);
	}

	vec3_t c;
	ColorToVec3(color, c);

	cgi.AddSustainedLight(&(const r_sustained_light_t) {
		.light.origin = { org[0] + dir[0], org[1] + dir[1], org[2] + dir[2] },
		.light.color = { c[0], c[1], c[2] },
		.light.radius = 150.0,
		.sustain = 350
	});

	cgi.AddStain(&(const r_stain_t) {
		.origin = { org[0], org[1], org[2] },
		.radius = 4.0,
		.media = cg_particles_default->media,
		.color = { c[0], c[1], c[2], 0.33 }
	});

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

	if ((p = Cg_AllocParticle(cg_particles_tracer))) {
		p->lifetime = 0.1 * VectorDistance(start, end); // 0.1ms per unit in distance
		p->effects |= PARTICLE_EFFECT_SCALE;

		Vector4Set(p->part.color, 1.0, 0.7, 0.3, 1.0);

		p->scale_start = 1.5;
		p->scale_end = 0.5;

		VectorCopy(start, p->part.org);

		VectorSubtract(end, start, p->vel);
		VectorMA(p->vel, 4.0, p->vel, p->vel); // Ghetto indeed

		p->spark.length = 0.02 + Randomf() * 0.05;

		VectorMA(p->part.org, p->spark.length, p->vel, p->part.end);

		p->accel[2] = -PARTICLE_GRAVITY * 3.0;
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
			if ((p = Cg_AllocParticle(cg_particles_beam))) {

				p->effects |= PARTICLE_EFFECT_BOUNCE;
				p->bounce = 1.5;
				p->lifetime = 100 + Randomf() * 350;

				cgi.ColorFromPalette(221 + (Randomr(0, 8)), p->part.color);
				p->part.color[3] = 0.7 + Randomf() * 0.3;

				p->part.scale = 0.6 + Randomf() * 0.4;

				VectorCopy(org, p->part.org);

				VectorScale(dir, 180.0 + Randomf() * 40.0, p->vel);

				p->accel[0] = Randomc() * 40.0;
				p->accel[1] = Randomc() * 40.0;
				p->accel[2] = -(PARTICLE_GRAVITY + (Randomf() * 60.0));
				p->spark.length = 0.04 + Randomf() * 0.02;

				VectorMA(p->part.org, p->spark.length, p->vel, p->part.end);
			}
		}

		if ((p = Cg_AllocParticle(cg_particles_smoke))) {

			p->lifetime = 150 + Randomf() * 600;
			p->effects = PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

			Vector4Set(p->color_start, 0.8, 0.8, 0.8, 0.8);
			VectorCopy(p->color_start, p->color_end);
			p->color_end[3] = 0.0;

			p->scale_start = 2.0 + Randomf() * 2.0;
			p->scale_end = 18.0 + Randomf() * 8.0;
			p->part.roll = Randomc() * 50.0;

			VectorCopy(org, p->part.org);
			VectorScale(dir, 60.0 + (Randomc() * 60.0), p->vel);
			VectorScale(dir, -80.0, p->accel);
			VectorMA(p->accel, 20.0, vec3_up, p->accel);

			p->part.blend = GL_ONE_MINUS_SRC_ALPHA;
		}
	}

	cgi.AddSustainedLight(&(const r_sustained_light_t) {
		.light.origin = { org[0] + dir[0], org[1] + dir[1], org[2] + dir[2] },
		       .light.color = { 0.5, 0.3, 0.2 },
		              .light.radius = 20.0,
		                     .sustain = 250
	});

	cgi.AddStain(&(const r_stain_t) {
		.origin = { org[0], org[1], org[2] },
		.radius = 2.0,
		.media = cg_particles_default->media,
		.color = { 0.0, 0.0, 0.0, 0.33 },
	});

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

		if (!(p = Cg_AllocParticle(cg_particles_blood))) {
			break;
		}

		p->lifetime = 100 + Randomf() * 500;
		p->effects = PARTICLE_EFFECT_COLOR;

		Vector4Set(p->color_start, Randomfr(0.5, 0.8), 0.0, 0.0, 0.5);
		VectorCopy(p->color_start, p->color_end);
		p->color_end[3] = 0.0;

		p->part.scale = Randomfr(5.0, 8.0);
		p->part.roll = Randomc() * 100.0;

		const vec_t d = Randomr(0, 32);
		for (int32_t j = 0; j < 3; j++) {
			p->part.org[j] = org[j] + Randomfr(-10.0, 10.0) + d * dir[j];
			p->vel[j] = Randomc() * 30.0;
		}

		p->accel[0] = p->accel[1] = 0.0;
		p->accel[2] = -PARTICLE_GRAVITY / 2.0;

		p->part.blend = GL_ONE_MINUS_SRC_ALPHA;
	}

	cgi.AddStain(&(const r_stain_t) {
		.origin = { org[0], org[1], org[2] },
		.radius = count * 6.0,
		.media = cg_stain_blood,
		.color = { 0.5 + (Randomf() * 0.3), 0.0, 0.0, 0.1 + Randomf() * 0.2 },
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

			if (!(p = Cg_AllocParticle(cg_particles_blood))) {
				break;
			}

			p->lifetime = 700 + Randomf() * 400;
			p->effects = PARTICLE_EFFECT_COLOR;

			Vector4Set(p->color_start, Randomfr(0.5, 0.8), 0.0, 0.0, 0.5);
			VectorCopy(p->color_start, p->color_end);
			p->color_end[3] = 0.0;

			p->part.scale = Randomfr(3.0, 7.0);
			p->part.roll = Randomc() * 100.0;

			VectorCopy(o, p->part.org);

			VectorScale(v, dist * ((vec_t)j / GIB_STREAM_COUNT), p->vel);
			p->vel[0] += Randomc() * 2.0;
			p->vel[1] += Randomc() * 2.0;
			p->vel[2] += 100.0;

			p->accel[0] = p->accel[1] = 0.0;
			p->accel[2] = -PARTICLE_GRAVITY * 2.0;

			p->part.blend = GL_ONE_MINUS_SRC_ALPHA;
		}
	}

	cgi.AddStain(&(const r_stain_t) {
		.origin = { org[0], org[1], org[2] },
		.radius = count * 6.0,
		.media = cg_particles_default->media,
		.color = { 0.5 + (Randomf() * 0.3), 0.0, 0.0, 0.1 + Randomf() * 0.2 },
	});

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

		if (!(p = Cg_AllocParticle(cg_particles_spark))) {
			break;
		}

		p->lifetime = 50 + Randomf() * 150;

		cgi.ColorFromPalette(221 + (Randomr(0, 8)), p->part.color);
		p->part.color[3] = 0.7 + Randomf() * 0.3;

		p->part.scale = 0.6 + Randomf() * 0.2;

		VectorCopy(org, p->part.org);

		VectorScale(dir, 4, p->vel);

		for (int32_t j = 0; j < 3; j++) {
			p->part.org[j] += Randomc() * 4.0;
			p->vel[j] += Randomc() * 90.0;
		}

		p->accel[0] = Randomc() * 1.0;
		p->accel[1] = Randomc() * 1.0;
		p->accel[2] = -0.5 * PARTICLE_GRAVITY;
		p->spark.length = 0.15;

		VectorMA(p->part.org, p->spark.length, p->vel, p->part.end);
	}

	cgi.AddSustainedLight(&(const r_sustained_light_t) {
		.light.origin = { org[0], org[1], org[2] },
		       .light.color = { 0.7, 0.5, 0.5 },
		              .light.radius = 80.0,
		                     .sustain = 650
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

	if ((p = Cg_AllocParticle(cg_particles_explosion))) {

		p->lifetime = 250;
		p->effects = PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

		cgi.ColorFromPalette(224, p->color_start);
		VectorCopy(p->color_start, p->color_end);
		p->color_end[3] = 0.0;

		p->scale_start = 6.0;
		p->scale_end = 128.0;

		p->part.roll = Randomc() * 100.0;

		VectorCopy(org, p->part.org);
	}

	if ((cgi.PointContents(org) & MASK_LIQUID) == 0) {

		for (int32_t i = 0; i < 10; i++) {

			if (!(p = Cg_AllocParticle(cg_particles_smoke))) {
				break;
			}

			p->lifetime = 1500 + (Randomc() * 500);
			p->effects = PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

			const vec_t smoke_color = Randomfr(0.6, 0.9);

			Vector4Set(p->color_start, smoke_color, smoke_color, smoke_color, 0.125);
			Vector4Set(p->color_end, smoke_color, smoke_color, smoke_color, 0.0);

			p->scale_start = 40.0;
			p->scale_end = 16.0;

			p->part.roll = Randomc() * 60.0;

			p->part.org[0] = org[0] + Randomc() * 16.0;
			p->part.org[1] = org[1] + Randomc() * 16.0;
			p->part.org[2] = org[2] + Randomc() * 16.0;

			VectorSet(p->vel, Randomc() * 50.0, Randomc() * 50.0, Randomc() * 50.0);
			VectorSet(p->accel, -p->vel[0], -p->vel[1], -p->vel[2]);

			p->accel[2] += PARTICLE_GRAVITY / Randomfr(6.0, 9.0);

			p->part.blend = GL_ONE_MINUS_SRC_ALPHA;
		}

		for (int32_t i = 0; i < 40; i++) {
			if (!(p = Cg_AllocParticle(cg_particles_spark))) {
				break;
			}

			p->lifetime = 200 + Randomf() * 300;

			cgi.ColorFromPalette(221 + (Randomr(0, 8)), p->part.color);
			p->part.color[3] = 0.7 + Randomf() * 0.3;

			p->part.scale = 0.9 + Randomf() * 0.4;

			VectorCopy(org, p->part.org);

			p->vel[0] = Randomc() * 170.0;
			p->vel[1] = Randomc() * 170.0;
			p->vel[2] = Randomc() * 170.0;

			p->accel[2] = -PARTICLE_GRAVITY * 2.0;

			p->spark.length = 0.04 + Randomf() * 0.06;

			VectorMA(p->part.org, p->spark.length, p->vel, p->part.end);
		}
	}

	if (cg_particle_quality->integer) {
		for (int32_t i = 0; i < 24; i++) {

			if (!(p = Cg_AllocParticle(cg_particles_debris[Randomr(0, 4)]))) {
				break;
			}

			p->lifetime = 400 + Randomf() * 300;
			p->effects = PARTICLE_EFFECT_COLOR;

			VectorSet(p->color_start, 1.0, 1.0, 1.0);
			VectorCopy(p->color_start, p->color_end);
			p->color_end[3] = 0.3;

			p->part.scale = 3.0;
			p->part.roll = Randomc() * 30.0;

			p->part.org[0] = org[0] + Randomfr(-16.0, 16.0);
			p->part.org[1] = org[1] + Randomfr(-16.0, 16.0);
			p->part.org[2] = org[2] + Randomfr(-16.0, 16.0);

			p->vel[0] = Randomc() * 800.0;
			p->vel[1] = Randomc() * 800.0;
			p->vel[2] = Randomc() * 800.0;

			p->part.blend = GL_ONE_MINUS_SRC_ALPHA;

			p->accel[2] = -PARTICLE_GRAVITY * 2;
		}
	}

	cgi.AddSustainedLight(&(const r_sustained_light_t) {
		.light.origin = { org[0], org[1], org[2] },
		 .light.color = { 0.8, 0.4, 0.2 },
		  .light.radius = 200.0,
		   .sustain = 1000
	});

	vec3_t c;
	cgi.ColorFromPalette(Randomr(0, 2), c);

	cgi.AddStain(&(const r_stain_t) {
		.origin = { org[0], org[1], org[2] },
		.radius = 88.0 + (64.0 * Randomc() * 0.15),
		.media = cg_stain_explosion,
		.color = { c[0], c[1], c[2], 0.66 },
	});

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

	vec3_t color;
	cgi.ColorFromPalette(113 + Randomr(0, 3), color);

	for (int32_t i = 0; i < 2; i++) {

		if (!(p = Cg_AllocParticle(cg_particles_explosion))) {
			break;
		}

		p->lifetime = 150;
		p->effects = PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

		VectorCopy(color, p->color_start);
		VectorCopy(color, p->color_end);
		p->color_end[3] = 0.0;

		p->scale_start = 1.5;
		p->scale_end = 25.0 * (i + 1);

		p->part.roll = 100.0 * Randomc();

		VectorAdd(org, dir, p->part.org);
	}

	if (cg_particle_quality->integer && (cgi.PointContents(org) & MASK_LIQUID) == 0) {
		for (int32_t i = 0; i < 6; i++) {

			if (!(p = Cg_AllocParticle(cg_particles_spark))) {
				break;
			}

			p->effects = PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE | PARTICLE_EFFECT_BOUNCE;
			p->lifetime = 200 + Randomf() * 500;

			Vector4Set(p->color_start, 0.4, 0.7, 1.0, 1.0);
			p->color_start[3] = 2.0;
			VectorCopy(p->color_start, p->color_end);
			p->color_end[3] = 0.0;

			p->scale_start = 3.5;
			p->scale_end = Randomf() * 1.0;

			p->bounce = 1.15;

			VectorCopy(org, p->part.org);

			p->vel[0] = dir[0] * Randomfr(20.0, 140.0) + Randomc() * 16.0;
			p->vel[1] = dir[1] * Randomfr(20.0, 140.0) + Randomc() * 16.0;
			p->vel[2] = dir[2] * Randomfr(20.0, 140.0) + Randomc() * 16.0;

			p->accel[2] = -PARTICLE_GRAVITY * 2.0;

			p->spark.length = 0.03 + Randomf() * 0.04;

			VectorMA(p->part.org, p->spark.length, p->vel, p->part.end);
		}
	}

	cgi.AddSustainedLight(&(const r_sustained_light_t) {
		.light.origin = { org[0] + dir[0], org[1] + dir[1], org[2] + dir[2] },
		       .light.color = { 0.4, 0.7, 1.0 },
		              .light.radius = 80.0,
		                     .sustain = 250
	});

	cgi.AddStain(&(const r_stain_t) {
		.origin = { org[0], org[1], org[2] },
		.radius = 16.0,
		.media = cg_particles_default->media,
		.color = { color[0], color[1], color[2], 0.33 },
	});

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

	cgi.AddSustainedLight(&(const r_sustained_light_t) {
		.light.origin = { org[0], org[1], org[2] },
		       .light.color = { 0.6, 0.6, 1.0 },
		              .light.radius = 160.0,
		                     .sustain = 750
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
	r_sustained_light_t s;

	VectorCopy(start, s.light.origin);
	s.light.radius = 100.0;
	ColorToVec3(color, s.light.color);
	s.sustain = 500;

	cgi.AddSustainedLight(&s);

	// Rail core

	if ((p = Cg_AllocParticle(cg_particles_beam))) {
		p->lifetime = 1600;
		p->effects = PARTICLE_EFFECT_COLOR;

		ColorToVec4(color, p->color_start);
		VectorCopy(p->color_start, p->color_end);
		p->color_end[3] = 0.0;

		p->part.scale = 8.0;

		VectorCopy(start, p->part.org);
		VectorCopy(end, p->part.end);
	}

	// Rail wake

	VectorCopy(start, point);

	VectorSubtract(end, start, vec);

	const vec_t len = VectorNormalize(vec);

	VectorSet(right, vec[2], -vec[0], vec[1]);
	CrossProduct(vec, right, up);

	for (int32_t i = 0; i < len && i < 2048; i += 3) {

		if (!(p = Cg_AllocParticle(cg_particles_rail_wake))) {
			break;
		}

		p->lifetime = 600 + ((i / len) * 600.0);
		p->effects = PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

		Vector4Set(p->color_start, 1.0, 1.0, 1.0, 0.2);
		VectorCopy(p->color_start, p->color_end);
		p->color_end[3] = 0.0;

		p->scale_start = 2.0;
		p->scale_end = 1.0;

		p->part.roll = -(100.0 + ((1.0 - (i / len)) * 700.0));

		VectorMA(point, 3.0, vec, point);
		VectorCopy(point, p->part.org);

		VectorScale(vec, 20.0, p->vel);

		const vec_t cosi = cosf(i * 0.1);
		const vec_t sini = sinf(i * 0.1);

		VectorMA(p->part.org, cosi * 2.0, right, p->part.org);
		VectorMA(p->part.org, sini * 2.0, up, p->part.org);

		VectorMA(p->vel, cosi * 10.0, right, p->vel);
		VectorMA(p->vel, sini * 10.0, up, p->vel);

		// Check for bubble trail

		if (i % 6 == 0 && (cgi.PointContents(point) & MASK_LIQUID)) {
			Cg_BubbleTrail(point, p->part.org, 16.0);
		}

		// Add sustained lights

		if (i > 0 && i < len - 64.0 && i % 64 == 0) {
			VectorCopy(point, s.light.origin);
			s.sustain = 200;
			cgi.AddSustainedLight(&s);
		}
	}

	// Check for explosion effect on solids

	if (flags & SURF_SKY) {
		return;
	}

	if ((p = Cg_AllocParticle(cg_particles_explosion))) {
		p->lifetime = 250;
		p->effects = PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

		ColorToVec4(color, p->color_start);
		VectorCopy(p->color_start, p->color_end);
		p->color_end[3] = 0.0;

		p->scale_start = 6.0;
		p->scale_end = 128.0;

		VectorCopy(end, p->part.org);
	}

	// Rail impact sparks

	if (cg_particle_quality->integer && (cgi.PointContents(end) & MASK_LIQUID) == 0) {

		for (int32_t i = 0; i < 24; i++) {
			if (!(p = Cg_AllocParticle(cg_particles_spark))) {
				break;
			}

			p->lifetime = 50 + Randomf() * 100;

			ColorToVec4(color, p->part.color);

			p->part.scale = 1.6 + Randomf() * 0.6;

			VectorCopy(end, p->part.org);

			VectorScale(dir, 400.0 + Randomf() * 200, p->vel);

			p->vel[0] += Randomc() * 100;
			p->vel[1] += Randomc() * 100;
			p->vel[2] += Randomc() * 100;

			p->accel[2] = -PARTICLE_GRAVITY * 2.0;

			p->spark.length = 0.04 + Randomf() * 0.07;

			VectorMA(p->part.org, p->spark.length, p->vel, p->part.end);
		}
	}

	// Impact light

	VectorMA(end, -12.0, vec, s.light.origin);
	s.light.radius = 120.0;
	s.sustain += 250;

	cgi.AddSustainedLight(&s);

	cgi.AddStain(&(const r_stain_t) {
		.origin = { end[0], end[1], end[2] },
		.radius = 8.0,
		.media = cg_particles_default->media,
		.color = { s.light.color[0], s.light.color[1], s.light.color[2], 0.66 },
	});
}

/**
 * @brief
 */
static void Cg_BfgLaserEffect(const vec3_t org, const vec3_t end) {
	cg_particle_t *p;
	r_sustained_light_t s;

	if ((p = Cg_AllocParticle(cg_particles_beam))) {
		VectorCopy(org, p->part.org);
		VectorCopy(end, p->part.end);

		p->lifetime = 50;

		cgi.ColorFromPalette(200 + Randomr(0, 3), p->part.color);

		p->part.scale = 6.0;

		p->part.scroll_s = -4.0;
	}

	VectorCopy(end, s.light.origin);
	s.light.radius = 80.0;
	VectorSet(s.light.color, 0.8, 1.0, 0.5);
	s.sustain = 50;

	cgi.AddSustainedLight(&s);
}

/**
 * @brief
 */
static void Cg_BfgEffect(const vec3_t org) {
	cg_particle_t *p;

	for (int32_t i = 0; i < 4; i++) {

		if (!(p = Cg_AllocParticle(cg_particles_explosion))) {
			break;
		}

		p->lifetime = 500;
		p->effects |= PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

		cgi.ColorFromPalette(200 + Randomr(0, 3), p->color_start);
		VectorCopy(p->color_start, p->color_end);
		p->color_end[3] = 0.0;

		p->scale_start = 6.0;
		p->scale_end = 48.0 * (i + 1);

		p->part.roll = 100.0 * Randomc();

		VectorCopy(org, p->part.org);
	}

	for (int32_t i = 0; i < 128; i++) {

		if (!(p = Cg_AllocParticle(cg_particles_default))) {
			break;
		}

		p->lifetime = 750;
		p->effects |= PARTICLE_EFFECT_COLOR;

		cgi.ColorFromPalette(206, p->color_start);
		Vector4Set(p->color_end, 1.0, 2.0, 0.0, 0.0);

		p->part.scale = 2.0;

		p->part.org[0] = org[0] + Randomfr(-24.0, 24.0);
		p->part.org[1] = org[1] + Randomfr(-24.0, 24.0);
		p->part.org[2] = org[2] + Randomfr(-24.0, 24.0);

		p->vel[0] = Randomfr(-256.0, 256.0);
		p->vel[1] = Randomfr(-256.0, 256.0);
		p->vel[2] = Randomfr(-256.0, 256.0);

		VectorSet(p->accel, 0.0, 0.0, -3.0 * PARTICLE_GRAVITY);
	}

	cgi.AddSustainedLight(&(const r_sustained_light_t) {
		.light.origin = { org[0], org[1], org[2] },
		       .light.color = { 0.8, 1.0, 0.5 },
		              .light.radius = 200.0,
		                     .sustain = 1000
	});

	vec3_t c;
	cgi.ColorFromPalette(203 + Randomr(0, 3), c);

	cgi.AddStain(&(const r_stain_t) {
		.origin = { org[0], org[1], org[2] },
		.radius = 96.0,
		.media = cg_particles_default->media,
		.color = { c[0], c[1], c[2], 0.75 },
	});

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

	if (!(p = Cg_AllocParticle(cg_particles_ripple[Randomr(0, 3)]))) {
		return;
	}

	p->lifetime = Randomr(500, 1500) * (viscosity * 0.1);

	p->effects = PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;
	p->part.flags = PARTICLE_FLAG_NO_DEPTH;

	Vector4Set(p->color_start, 0.5, 0.5, 0.5, 0.3 + (Randomf() * 0.2));
	Vector4Set(p->color_end, 0.0, 0.0, 0.0, 0.0);

	p->scale_start = size + Randomc() * 0.5;
	p->scale_end = size * (3.0 + Randomf() * 0.5);

	VectorCopy(org, p->part.org);
}

/**
 * @brief
 */
static void Cg_SplashEffect(const vec3_t org, const vec3_t dir) {
	cg_particle_t *p;

	for (int32_t i = 0; i < 10; i++) {
		if (!(p = Cg_AllocParticle(cg_particles_default))) {
			break;
		}

		p->lifetime = 250 + Randomc() * 150;

		p->effects = PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

		Vector4Set(p->color_start, 1.0, 1.0, 1.0, 2.0);
		Vector4Set(p->color_end, 1.0, 1.0, 1.0, 0.0);

		p->scale_start = 0.8;
		p->scale_end = 0.5;

		p->part.org[0] = org[0] + (Randomc() * 8.0);
		p->part.org[1] = org[1] + (Randomc() * 8.0);
		p->part.org[2] = org[2] + (Randomf() * 8.0);

		p->vel[0] = Randomc() * 64.0;
		p->vel[1] = Randomc() * 64.0;
		p->vel[2] = 36.0 + Randomf() * 64.0;

		VectorSet(p->accel, 0.0, 0.0, -PARTICLE_GRAVITY / 2.0);
	}

	if (cg_particle_quality->integer == 0) {
		return;
	}

	if ((p = Cg_AllocParticle(cg_particles_beam))) {

		p->lifetime = 120 + Randomf() * 80;
		p->effects = PARTICLE_EFFECT_COLOR;

		Vector4Set(p->color_start, 0.8, 0.8, 0.8, 0.45);
		Vector4Set(p->color_end, 0.8, 0.8, 0.8, 0.0);

		p->part.scale = 1.4 + Randomf() * 0.7;

		VectorCopy(org, p->part.org);

		VectorScale(dir, 70.0 + Randomf() * 30.0, p->vel);
		p->vel[0] += Randomc() * 8.0;
		p->vel[1] += Randomc() * 8.0;

		p->spark.length = 0.3 + Randomf() * 0.03;

		VectorMA(p->part.org, p->spark.length, p->vel, p->part.end);

		VectorSet(p->accel, 0.0, 0.0, -PARTICLE_GRAVITY / 2.0);
	}
}

/**
 * @brief
 */
static void Cg_HookImpactEffect(const vec3_t org, const vec3_t dir) {

	for (int32_t i = 0; i < 32; i++) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle(cg_particles_spark))) {
			break;
		}

		p->lifetime = 100 + (Randomf() * 150);

		cgi.ColorFromPalette(221 + (Randomr(0, 8)), p->part.color);
		p->part.color[3] = 0.7 + Randomf() * 0.3;

		p->part.scale = 0.8 + Randomf() * 0.4;

		VectorCopy(org, p->part.org);

		VectorScale(dir, 9, p->vel);

		for (int32_t j = 0; j < 3; j++) {
			p->part.org[j] += Randomc() * 4.0;
			p->vel[j] += Randomc() * 90.0;
		}

		p->accel[0] = Randomc() * 2.0;
		p->accel[1] = Randomc() * 2.0;
		p->accel[2] = -0.5 * PARTICLE_GRAVITY;
		p->spark.length = 0.15;

		VectorMA(p->part.org, p->spark.length, p->vel, p->part.end);
	}

	vec3_t v;
	VectorAdd(org, dir, v);

	cgi.AddSustainedLight(&(const r_sustained_light_t) {
		.light.origin = { v[0], v[1], v[2] },
		       .light.color = { 0.7, 0.5, 0.5 },
		              .light.radius = 80.0,
		                     .sustain = 850
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
