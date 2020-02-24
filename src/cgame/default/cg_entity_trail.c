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
uint32_t Cg_ParticleTrailDensity(cl_entity_t *ent, const vec3_t start, const vec3_t end, const float min_length, const float max_length, const float density) {

	// first we have to reach up to min_length before we decide to start spawning
	if (Vec3_Distance(start, ent->previous_trail_origin) < min_length) {
		return 0;
	}

	// check how far we're gonna draw a trail
	const float dist = Vec3_Distance(ent->previous_trail_origin, end);
	const float frac = dist / (max_length - min_length);

	ent->update_trail_origin = true;

	return density * frac;
}

/**
 * @brief
 */
void Cg_BreathTrail(cl_entity_t *ent) {

	if (ent->animation1.animation < ANIM_TORSO_GESTURE) { // death animations
		return;
	}

	if (cgi.client->unclamped_time < ent->timestamp) {
		return;
	}

	cg_particle_t *p;

	vec3_t pos;
	pos = ent->origin;

	if (Cg_IsDucking(ent)) {
		pos.z += 18.0;
	} else {
		pos.z += 30.0;
	}

	vec3_t forward;
	Vec3_Vectors(ent->angles, &forward, NULL, NULL);

	pos = Vec3_Add(pos, Vec3_Scale(forward, 8.0));

	const int32_t contents = cgi.PointContents(pos);

	if (contents & MASK_LIQUID) {
		if ((contents & MASK_LIQUID) == CONTENTS_WATER) {

			if (!(p = Cg_AllocParticle())) {
				return;
			}

			p->origin = Vec3_Add(pos, Vec3_RandomRange(-2.f, 2.f));

			p->velocity = Vec3_Add(Vec3_Scale(forward, 2.0), Vec3_RandomRange(-5.f, 5.f));
			p->velocity.z += 6.0;
			p->acceleration.z = 10.0;

			p->lifetime = 1000 - (Randomf() * 100);

			p->color = Color3b(160, 160, 160);
//			p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

			p->size = 3.0;
//			p->delta_size = -0.1;

			ent->timestamp = cgi.client->unclamped_time + 3000;
		}
	} else if (cgi.view->weather & WEATHER_RAIN || cgi.view->weather & WEATHER_SNOW) {

		if (!(p = Cg_AllocParticle())) {
			return;
		}

		p->lifetime = 4000 - (Randomf() * 100);

		p->color = color_white;// cgi.ColorFromPalette(6 + (Randomr(0, 8)));
		p->color.a = 200;

//		p->delta_color.a = p->color.a * -p->lifetime / PARTICLE_FRAME;

		p->size = 1.5;
//		p->delta_size = 0.1;

		p->origin = pos;

		p->velocity = Vec3_Add(Vec3_Scale(forward, 5.0), Vec3_RandomRange(-2.f, 2.f));

		ent->timestamp = cgi.client->unclamped_time + 3000;
	}
}

/**
 * @brief
 */
void Cg_SmokeTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

	if (ent) { // don't emit smoke trails for static entities (grenades on the floor)
		if (Vec3_Equal(ent->current.origin, ent->prev.origin)) {
			return;
		}
	}

	if (cgi.PointContents(end) & MASK_LIQUID) {
		Cg_BubbleTrail(start, end, 24.0);
		return;
	}

	const vec3_t dir = Vec3_Normalize(Vec3_Subtract(end, start));

	float particles = Cg_ParticlesPerSecond(60.f);
	for (int32_t i = 0; i < particles; i++) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle())) {
			return;
		}

		p->origin = Vec3_Mix(start, end, i / particles + RandomRangef(-.5f, .5f));
		p->velocity = Vec3_Scale(dir, RandomRangef(20.f, 30.f));
		p->acceleration = Vec3_Scale(dir, -20.f);

		p->lifetime = RandomRangef(1000.f, 1400.f);

		p->color = Color4fv(Vec4_RandomRange(.22f, .33f));
		p->color_velocity.w = -p->color.a / 255.f / MILLIS_TO_SECONDS(p->lifetime);

		p->size = 1.5f;
		p->size_velocity = 10.f;
	}
}

/**
 * @brief
 */
void Cg_FlameTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {
	cg_particle_t *p;

	if (cgi.PointContents(end) & MASK_LIQUID) {
		Cg_BubbleTrail(start, end, 10.0);
		return;
	}

	if (!(p = Cg_AllocParticle())) {
		return;
	}

	p->origin = end;
	p->velocity = Vec3_RandomRange(-1.5, 1.5);

	p->acceleration.z = 15.0;

	p->lifetime = 1500;

	const int32_t r = Random() & 15;
	p->color = Color3b(224 + r, 180 * r, 40 + r);
	p->color.a = 200;
//	p->delta_color.a = p->color.a * -p->lifetime / PARTICLE_FRAME;

	p->size = 10.0 + Randomc();

	// make static flames rise
	if (ent) {
		if (Vec3_Equal(ent->current.origin, ent->prev.origin)) {
			p->lifetime /= 0.65;
			p->acceleration.z = 20.0;
		}
	}
}

/**
 * @brief
 */
void Cg_SteamTrail(cl_entity_t *ent, const vec3_t org, const vec3_t vel) {
	cg_particle_t *p;

	vec3_t end;
	end = Vec3_Add(org, vel);

	if (cgi.PointContents(org) & MASK_LIQUID) {
		Cg_BubbleTrail(org, end, 10.0);
		return;
	}

	if (!(p = Cg_AllocParticle())) {
		return;
	}

	p->origin = org;
	p->velocity = Vec3_Add(vel, Vec3_RandomRange(-2.f, 2.f));

	p->lifetime = 4500 / (5.0 + Randomf() * 0.5);

	p->color = color_white;//cgi.ColorFromPalette(6 + (Randomr(0, 8)));
	p->color.a = 50;

//	p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

	p->size = 8.0;
//	p->delta_size = 20.0 / p->lifetime / PARTICLE_FRAME;
}

/**
 * @brief
 */
void Cg_BubbleTrail(const vec3_t start, const vec3_t end, float target) {

	const float particles = Cg_ParticlesPerSecond(target);

	for (int32_t i = 0; i < particles; i++) {

		const vec3_t pos = Vec3_Mix(start, end, i / particles);
		const int32_t contents = cgi.PointContents(pos);
		if (!(contents & MASK_LIQUID)) {
			continue;
		}

		cg_particle_t *p;
		if (!(p = Cg_AllocParticle())) {
			return;
		}

		p->origin = Vec3_Add(pos, Vec3_RandomRange(-2.f, 2.f));
		p->velocity = Vec3_RandomRange(-5.f, 5.f);
		p->velocity.z += 6.0;
		p->acceleration.z = 10.0;

		p->lifetime = 1000 - (Randomf() * 100);

		p->color = Color3bv(0x447788);

		if (contents & CONTENTS_LAVA) {
			p->color = Color3bv(0x886644);
			p->velocity = Vec3_Scale(p->velocity, .33f);
		} else if (contents & CONTENTS_SLIME) {
			p->color = Color3bv(0x556644);
			p->velocity = Vec3_Scale(p->velocity, .66f);
		}

		p->color.a = RandomRangef(.4f, .7f) * 255;
		p->color_velocity.w = -p->color.a / 255.f / MILLIS_TO_SECONDS(p->lifetime);

		p->size = RandomRangef(1.f, 2.f);
		p->size_velocity = -p->size / MILLIS_TO_SECONDS(p->lifetime);
	}
}

/**
 * @brief
 */
static void Cg_BlasterTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {
	cg_particle_t *p;
	uint32_t particles;
	const vec3_t trail_start = ent->previous_trail_origin;

	const color_t color = Cg_ResolveEffectColor(ent->current.client, EFFECT_COLOR_ORANGE);

	const vec3_t dir = Vec3_Normalize(Vec3_Subtract(end, start));

	if (cgi.PointContents(end) & MASK_LIQUID) {
		Cg_BubbleTrail(start, end, 12.0);

		particles = Cg_ParticleTrailDensity(ent, start, end, 16.f, 64.f, 8);
		for (int32_t i = 0; i < particles; i++) {

			if (!(p = Cg_AllocParticle())) {
				break;
			}

			p->lifetime = RandomRangef(250.f, 300.f);
			p->origin = Vec3_Mix(trail_start, end, i / (float) particles);
			p->velocity = Vec3_Scale(dir, RandomRangef(50.f, 100.f));
			p->color = Color_Add(color, Color3fv(Vec3_RandomRange(-.1f, .1f)));
			p->color_velocity.w = -1.f / MILLIS_TO_SECONDS(p->lifetime);
		}
	} else {

		particles = Cg_ParticleTrailDensity(ent, start, end, 16.f, 64.f, 24);

		for (int32_t i = 0; i < particles; i++) {

			if (!(p = Cg_AllocParticle())) {
				break;
			}

			p->lifetime = RandomRangef(250.f, 300.f);
			p->origin = Vec3_Mix(trail_start, end, i / (float) particles);
			p->velocity = Vec3_Scale(dir, RandomRangef(50.f, 100.f));
			p->color = Color_Add(color, Color3fv(Vec3_RandomRange(-.1f, .1f)));
			p->color_velocity.w = -1.f / MILLIS_TO_SECONDS(p->lifetime);
		}

		particles = Cg_ParticleTrailDensity(ent, start, end, 16.f, 64.f, 8);

		for (int32_t i = 0; i < particles; i++) {

			if (!(p = Cg_AllocParticle())) {
				break;
			}

			p->lifetime = RandomRangef(450.f, 650.f);
			p->origin = Vec3_Mix(trail_start, end, i / (float) particles);
			p->velocity = Vec3_Scale(dir, RandomRangef(50.f, 100.f));
			p->velocity = Vec3_Add(p->velocity, Vec3_RandomRange(-20.f, 20.f));
			p->acceleration.z -= PARTICLE_GRAVITY;
			p->color = Color_Add(color, Color3fv(Vec3_RandomRange(-.1f, .1f)));
			p->color_velocity.w = -1.f / MILLIS_TO_SECONDS(p->lifetime);
		}
	}
	
	if ((p = Cg_AllocParticle())) {
		p->lifetime = 0;
		p->origin = end;
		p->size = 2;
		p->color = Color_Add(color, Color3fv(Vec3_RandomRange(-.1f, .1f)));
	}

	Cg_AddLight(&(cg_light_t) {
		.origin = end,
		.radius = 100.f,
		.color = Color_Vec3(color)
	});
}

/**
 * @brief
 */
static void Cg_GrenadeTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

	Cg_AddLight(&(cg_light_t) {
		.origin = end,
		.radius = 12.f + 12.f * sinf(cgi.client->unclamped_time * 0.02),
		.color = Color_Vec3(color_green)
	});
}

/**
 * @brief
 */
static void Cg_RocketTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {
	cg_particle_t *p;
	float particles;

	Cg_SmokeTrail(ent, start, end);

	const vec3_t dir = Vec3_Normalize(Vec3_Subtract(end, start));

	particles = Cg_ParticlesPerSecond(80.f);
	for (int32_t i = 0; i < particles; i++) {

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		p->lifetime = RandomRangef(75.f, 150.f);
		p->origin = Vec3_Mix(start, end, i / particles);
		p->velocity = Vec3_Scale(dir, RandomRangef(150.f, 200.f));
		p->acceleration = Vec3_RandomRange(-10.f, 10.f);
		p->acceleration.z -= PARTICLE_GRAVITY;
		p->color = Color4bv(0xffaa22ff);
		p->size = 2.0;
		p->size_velocity = -2.f / MILLIS_TO_SECONDS(p->lifetime);
	}

	particles = Cg_ParticlesPerSecond(120.f);
	for (int32_t i = 0; i < particles; i++) {

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		p->lifetime = RandomRangef(450.f, 650.f);
		p->origin = Vec3_Mix(start, end, i / particles);
		p->velocity = Vec3_Scale(dir, RandomRangef(50.f, 150.f));
		p->velocity = Vec3_Add(p->velocity, Vec3_RandomRange(-20.f, 20.f));
		p->acceleration = Vec3_RandomRange(-10.f, 10.f);
		p->acceleration.z -= PARTICLE_GRAVITY;
		p->color = Color4bv(0xcc9922ff);
		p->color_velocity.w = -1.f / MILLIS_TO_SECONDS(p->lifetime);
	}

	Cg_AddLight(&(cg_light_t) {
		.origin = end,
		.radius = 150.f,
		.color = Vec3(.8f, .4f, .2f)
	});
}

/**
 * @brief
 */
static void Cg_HyperblasterTrail(cl_entity_t *ent) {

	const float radius = 6.f;

	const float ltime = (ent->current.number + cgi.client->unclamped_time) * .01;

	for (int32_t i = 0; i < NUM_APPROXIMATE_NORMALS; i += 2) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		const float pitch = ltime * approximate_normals[i].x;
		const float sp = sinf(pitch);
		const float cp = cosf(pitch);

		const float yaw = ltime * approximate_normals[i].y;
		const float sy = sinf(yaw);
		const float cy = cosf(yaw);

		const vec3_t forward = Vec3(cp * sy, cy * sy, -sp);

		float dist = sinf(ltime + i) * radius;

		p->origin = ent->origin;
		p->origin = Vec3_Add(p->origin, Vec3_Scale(approximate_normals[i], dist));
		p->origin = Vec3_Add(p->origin, Vec3_Scale(forward, radius));

		const float d = dist / radius / 2.f;
		p->color = Color_Add(Color3bv(0x22aaff), Color3fv(Vec3(d, d, d)));
	}

	if (cgi.PointContents(ent->origin) & MASK_LIQUID) {
		Cg_BubbleTrail(ent->prev.origin, ent->origin, radius / 4.0);
	}

	Cg_AddLight(&(cg_light_t) {
		.origin = ent->origin,
		.radius = 100.f,
		.color = Vec3(.4f, .7f, 1.f)
	});
}

/**
 * @brief
 */
static void Cg_LightningTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {
	vec3_t dir, delta, pos, vel;

	cg_light_t l = {
		.origin = start,
		.radius = 90.f + RandomRangef(-10.f, 10.f),
		.color = Vec3(.6f, .6f, 1.f)
	};

	Cg_AddLight(&l);

	float dist_total;
	dir = Vec3_Subtract(start, end);
	dir = Vec3_NormalizeLength(dir, &dist_total);

	delta = Vec3_Scale(dir, -48.0);
	pos = start;

	vel = Vec3_Subtract(ent->current.origin, ent->prev.origin);

	float dist = dist_total;
	int32_t i = 0;
	while (dist > 0.0) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		p->color = Color3b(190, 190, Randomr(190, 210));//cgi.ColorFromPalette(12 + (Randomr(0, 4)));

		p->size = 1.0;

		p->origin = pos;

		if (dist <= 48.0) {
			delta = Vec3_Scale(dir, -dist);
		}

		pos = Vec3_Add(pos, delta);
		p->velocity = vel;

		dist -= 48.0;

		if (dist > 12.0) {
			l.origin = p->origin;
			l.radius = 90.0 + RandomRangef(-10.f, 10.f);
			Cg_AddLight(&l);
		}
	}

	l.origin = Vec3_Add(end, Vec3_Scale(dir, 12.0));
	l.radius = 90.0 + RandomRangef(-10.f, 10.f);
	Cg_AddLight(&l);

	if (ent->current.animation1 != LIGHTNING_SOLID_HIT) {
		return;
	}

	cg_particle_t *p;

	if (ent->timestamp < cgi.client->unclamped_time) {

		cm_trace_t tr = cgi.Trace(start, Vec3_Add(end, Vec3_Scale(dir, -128.0)), Vec3_Zero(), Vec3_Zero(), 0, CONTENTS_SOLID);

		if (tr.surface) {

			pos = Vec3_Add(tr.end, Vec3_Scale(tr.plane.normal, 1.0));

			cgi.AddStain(&(const r_stain_t) {
				.origin = pos,
				.radius = 8.0 + Randomf() * 4.0,
				.media = cg_stain_lightning,
				.color = Color4bv(0x00000040),
			});

			pos = Vec3_Add(tr.end, Vec3_Scale(tr.plane.normal, 2.0));

			if ((cgi.PointContents(pos) & MASK_LIQUID) == 0) {
				for (i = 0; i < 6; i++) {

					if (!(p = Cg_AllocParticle())) {
						break;
					}

					p->origin = pos;

					p->velocity = Vec3_Scale(Vec3_Add(tr.plane.normal, Vec3_RandomRange(-.2f, .2f)), RandomRangef(50, 200));

					p->acceleration.z = -PARTICLE_GRAVITY * 3.0;

					p->lifetime = 600 + Randomf() * 300;

					p->color = ColorHSL(180, RandomRangef(.0f, .7f), RandomRangef(.5f, 1.f));

					p->bounce = 1.15;

					p->size = 1.3 + Randomf() * 0.6;

					p->color_velocity.w = -1.f / MILLIS_TO_SECONDS(p->lifetime);
				}
			}
		}

		ent->timestamp = cgi.client->unclamped_time + 25; // 40hz
	}
}

/**
 * @brief
 */
static void Cg_HookTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

	cg_particle_t *p;

	if ((p = Cg_AllocParticle())) {

		p->origin = start;

		p->color = Cg_ResolveEffectColor(ent->current.client, EFFECT_COLOR_GREEN);

		p->size = 0.35;
	}
}

/**
 * @brief
 */
static void Cg_BfgTrail(cl_entity_t *ent) {

	const float radius = 48.f;

	const float ltime = cgi.client->unclamped_time * .001f;

	for (int32_t i = 0; i < NUM_APPROXIMATE_NORMALS; i++) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		const float pitch = ltime * approximate_normals[i].x;
		const float sp = sinf(pitch);
		const float cp = cosf(pitch);

		const float yaw = ltime * approximate_normals[i].y;
		const float sy = sinf(yaw);
		const float cy = cosf(yaw);

		const vec3_t forward = Vec3(cp * sy, cy * sy, -sp);

		p->origin = ent->origin;
		p->origin = Vec3_Add(p->origin, Vec3_Scale(approximate_normals[i], radius));
		p->origin = Vec3_Add(p->origin, Vec3_Scale(forward, radius));

		p->color = Color3bv(0x22ff44);

		p->size = 2.5f;
	}

	if (cgi.PointContents(ent->origin) & MASK_LIQUID) {
		Cg_BubbleTrail(ent->prev.origin, ent->origin, radius / 4.0);
	}

	const float mod = sinf(cgi.client->unclamped_time >> 5);

	Cg_AddLight(&(cg_light_t) {
		.origin = ent->origin,
		.radius = 160.f + 48.f * mod,
		.color = Vec3(.4f, 1.f, .4f)
	});
}

/**
 * @brief
 */
static void Cg_TeleporterTrail(cl_entity_t *ent, const color_t color) {

//	cgi.AddSample(&(const s_play_sample_t) {
//		.sample = cg_sample_respawn,
//			.entity = ent->current.number,
//			.attenuation = ATTEN_IDLE,
//			.flags = S_PLAY_ENTITY
//	});

	if (ent->timestamp > cgi.client->unclamped_time) {
		return;
	}

	ent->timestamp = cgi.client->unclamped_time + 16;

	for (int32_t i = 0; i < 8; i++) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		p->origin = Vec3_Add(ent->origin, Vec3_RandomRange(-32.0f, 32.0f));

		p->velocity.z = RandomRangef(80.f, 120.f);

		p->acceleration = Vec3_RandomRange(-80.f, 80.f);
		p->acceleration.z = 20.f;

		p->lifetime = 450;

		p->color = color;
//		p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

		p->size = 2.0;
	}
}

/**
 * @brief
 */
static void Cg_SpawnPointTrail(cl_entity_t *ent, const color_t color) {

	if (ent->timestamp > cgi.client->unclamped_time) {
		return;
	}

	ent->timestamp = cgi.client->unclamped_time + 1000;

	cg_particle_t *p;

	if ((p = Cg_AllocParticle())) {

		p->origin = ent->origin;

		p->origin.z -= 20.0;
		p->velocity.z = 2.0;

		p->lifetime = 450;

		p->color = color;
//		p->delta_color.a = -p->lifetime / PARTICLE_FRAME;

		p->size = 16.0;
//		p->delta_size = 12.0 * -p->lifetime / PARTICLE_FRAME;
	}
}

/**
 * @brief
 */
static void Cg_GibTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

	if (cgi.PointContents(end) & MASK_LIQUID) {
		Cg_BubbleTrail(start, end, 8.0);
		return;
	}

	const vec3_t dir = Vec3_Normalize(Vec3_Subtract(end, start));

	const float particles = Cg_ParticlesPerSecond(90.f);
	for (int32_t i = 0; i < particles; i++) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle())) {
			break;
		}

		p->lifetime = RandomRangef(1000.f, 1500.f);

		p->origin = Vec3_Mix(start, end, i / particles);
		p->velocity = Vec3_Scale(dir, 20.0);
		p->acceleration.z = -PARTICLE_GRAVITY / 2.0;

		p->color = Color4bv(0x80000080);
		p->color_velocity.x = -.1f / MILLIS_TO_SECONDS(p->lifetime);
		p->color_velocity.w = -p->color.a / MILLIS_TO_SECONDS(p->lifetime);

		p->size = RandomRangef(3.0, 7.0);

		static uint32_t added = 0;
		if ((added++ % 3) == 0) {
			cgi.AddStain(&(const r_stain_t) {
				.origin = p->origin,
				.radius = 12.0 * Randomf() * 3.0,
				.media = cg_stain_blood,
				.color = Color4bv(0x80101080),
			});
		}
	}
}

/**
 * @brief
 */
static void Cg_FireballTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

	if (cgi.PointContents(end) & MASK_LIQUID) {
		return;
	}

	cg_light_t l = {
		.origin = end,
		.radius = 85.f,
		.color = Vec3(0.9, 0.3, 0.1)
	};

	if (ent->current.effects & EF_DESPAWN) {
		const float decay = Clampf((cgi.client->unclamped_time - ent->timestamp) / 1000.0, 0.0, 1.0);
		l.radius *= (1.0 - decay);
	} else {
		Cg_SmokeTrail(ent, start, end);
		ent->timestamp = cgi.client->unclamped_time;
		Cg_FlameTrail(ent, start, end);
	}

	Cg_AddLight(&l);
}

/**
 * @brief Apply unique trails to entities between their previous packet origin
 * and their current interpolated origin. Beam trails are a special case: the
 * old origin field is overridden to specify the endpoint of the beam.
 */
void Cg_EntityTrail(cl_entity_t *ent) {
	const entity_state_t *s = &ent->current;

	vec3_t start, end;
	start = ent->previous_origin;

	// beams have two origins, most entities have just one
	if (s->effects & EF_BEAM) {

		end = ent->termination;

		// client is overridden to specify owner of the beam
		if (ent->current.client == cgi.client->client_num && !cgi.client->third_person) {

			// we own this beam (lightning, grapple, etc..)
			// project start & end points based on our current view origin
			float dist = Vec3_Distance(start, end);

			start = Vec3_Add(cgi.view->origin, Vec3_Scale(cgi.view->forward, 8.0));

			const float hand_scale = (ent->current.trail == TRAIL_HOOK ? -1.0 : 1.0);

			switch (cg_hand->integer) {
				case HAND_LEFT:
					start = Vec3_Add(start, Vec3_Scale(cgi.view->right, -5.5 * hand_scale));
					break;
				case HAND_RIGHT:
					start = Vec3_Add(start, Vec3_Scale(cgi.view->right, 5.5 * hand_scale));
					break;
				default:
					break;
			}

			start = Vec3_Add(start, Vec3_Scale(cgi.view->up, -8.0));

			// lightning always uses predicted end points
			if (s->trail == TRAIL_LIGHTNING) {
				end = Vec3_Add(start, Vec3_Scale(cgi.view->forward, dist));
			}
		}
	} else {
		end = ent->origin;
	}

	// add the trail

	switch (s->trail) {
		case TRAIL_BLASTER:
			Cg_BlasterTrail(ent, start, end);
			break;
		case TRAIL_GRENADE:
			Cg_GrenadeTrail(ent, start, end);
			break;
		case TRAIL_ROCKET:
			Cg_RocketTrail(ent, start, end);
			break;
		case TRAIL_HYPERBLASTER:
			Cg_HyperblasterTrail(ent);
			break;
		case TRAIL_LIGHTNING:
			Cg_LightningTrail(ent, start, end);
			break;
		case TRAIL_HOOK:
			Cg_HookTrail(ent, start, end);
			break;
		case TRAIL_BFG:
			Cg_BfgTrail(ent);
			break;
		case TRAIL_TELEPORTER:
			Cg_TeleporterTrail(ent, Color3b(255, 255, 211));
			break;
		case TRAIL_PLAYER_SPAWN:
			Cg_SpawnPointTrail(ent, ent->current.client >= MAX_TEAMS ? EFFECT_COLOR_WHITE : cg_team_info[ent->current.client].color);
			break;
		case TRAIL_GIB:
			Cg_GibTrail(ent, start, end);
			break;
		case TRAIL_FIREBALL:
			Cg_FireballTrail(ent, start, end);
			break;
		default:
			break;
	}

	if (ent->update_trail_origin) {
		ent->previous_trail_origin = end;
		ent->update_trail_origin = false;
	}
}
