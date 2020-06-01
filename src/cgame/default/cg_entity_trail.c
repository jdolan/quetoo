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
static int32_t Cg_TrailDensity(cl_entity_t *ent, const vec3_t start, const vec3_t end, float density, cl_trail_id_t trail, vec3_t *origin) {
	const float min_length = 16.f, max_length = 64.f;

	// first we have to reach up to min_length before we decide to start spawning
	if (ent && Vec3_Distance(start, ent->trails[trail].last_origin) < min_length) {
		return 0;
	}

	// check how far we're gonna draw a trail
	*origin = ent ? ent->trails[trail].last_origin : start;

	const float dist = Vec3_Distance(*origin, end);
	const float frac = dist / (max_length - min_length);

	if (ent) {
		ent->trails[trail].trail_updated = true;
	}

	return (int32_t) ceilf(density * frac);
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

	cg_sprite_t *s;

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

	if (contents & CONTENTS_MASK_LIQUID) {
		if ((contents & CONTENTS_MASK_LIQUID) == CONTENTS_WATER) {

			if (!(s = Cg_AllocSprite())) {
				return;
			}

			s->atlas_image = cg_sprite_particle;
			s->origin = Vec3_Add(pos, Vec3_RandomRange(-2.f, 2.f));
			s->velocity = Vec3_Add(Vec3_Scale(forward, 2.0), Vec3_RandomRange(-5.f, 5.f));
			s->velocity.z += 6.0;
			s->acceleration.z = 10.0;
			s->lifetime = 1000 - (Randomf() * 100);
			s->color = Color3b(160, 160, 160);
			s->size = 24.f;

			ent->timestamp = cgi.client->unclamped_time + 3000;
		}
	} else if (cgi.view->weather & WEATHER_RAIN || cgi.view->weather & WEATHER_SNOW) {

		if (!(s = Cg_AllocSprite())) {
			return;
		}

		s->atlas_image = cg_sprite_smoke;
		s->lifetime = 4000 - (Randomf() * 100);
		s->color = color_white;
		s->color.a = 200 / 255.f;
		s->size = 1.5;
		s->origin = pos;
		s->velocity = Vec3_Add(Vec3_Scale(forward, 5.0), Vec3_RandomRange(-2.f, 2.f));

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

	if (cgi.PointContents(end) & CONTENTS_MASK_LIQUID) {
		Cg_BubbleTrail(ent, start, end, 24.0);
		return;
	}

	vec3_t origin;
	const int32_t count = Cg_TrailDensity(ent, start, end, 8, TRAIL_SECONDARY, &origin);

	if (!count) {
		return;
	}

	const float step = 1.f / count;
	const vec3_t dir = Vec3_Normalize(Vec3_Subtract(end, origin));

	for (int32_t i = 0; i < count; i++) {
		cg_sprite_t *s;

		if (!(s = Cg_AllocSprite())) {
			break;
		}

		s->atlas_image = cg_sprite_smoke;
		s->origin = Vec3_Mix(origin, end, (step * i) + RandomRangef(-.5f, .5f));
		s->velocity = Vec3_Scale(dir, RandomRangef(20.f, 30.f));
		s->acceleration = Vec3_Scale(dir, -20.f);
		s->lifetime = RandomRangef(1000.f, 1400.f);
		const float color = RandomRangef(.7f, .9f);
		s->color = Color4f(color, color, color, RandomRangef(.6f, .9f));
		s->size = 2.5f;
		s->size_velocity = 15.f;
		s->rotation = RandomRangef(.0f, M_PI);
		s->rotation_velocity = RandomRangef(.2f, 1.f);
	}
}

/**
 * @brief
 */
void Cg_FlameTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {
	cg_sprite_t *s;

	if (cgi.PointContents(end) & CONTENTS_MASK_LIQUID) {
		Cg_BubbleTrail(ent, start, end, 10.0);
		return;
	}

	if (!(s = Cg_AllocSprite())) {
		return;
	}

	s->atlas_image = cg_sprite_flame;
	s->origin = end;
	s->velocity = Vec3_RandomRange(-1.5, 1.5);
	s->acceleration.z = 15.0;
	s->lifetime = 1500;
	const uint32_t r = RandomRangeu(0, 16);
	s->color = Color3b(224 + r, 180 + r, 40 + r);
	s->color.a = .78f;
	s->size = RandomRangef(-10.f, 10.f);

	// make static flames rise
	if (ent) {
		if (Vec3_Equal(ent->current.origin, ent->prev.origin)) {
			s->lifetime /= 0.65;
			s->acceleration.z = 20.0;
		}
	}
}

/**
 * @brief
 */
void Cg_SteamTrail(cl_entity_t *ent, const vec3_t org, const vec3_t vel) {
	cg_sprite_t *s;

	vec3_t end;
	end = Vec3_Add(org, vel);

	if (cgi.PointContents(org) & CONTENTS_MASK_LIQUID) {
		Cg_BubbleTrail(ent, org, end, 10.0);
		return;
	}

	if (!(s = Cg_AllocSprite())) {
		return;
	}

	s->media = (r_media_t *) cg_sprite_steam;
	s->origin = org;
	s->velocity = Vec3_Add(vel, Vec3_RandomRange(-2.f, 2.f));
	s->lifetime = 4500 / (5.0 + Randomf() * 0.5);
	s->color = color_white;
	s->color.a = 50 / 255.f;
	s->size = 8.0;
}

/**
 * @brief
 */
void Cg_BubbleTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end, float target) {

	vec3_t origin;
	const int32_t count = Cg_TrailDensity(ent, start, end, target, TRAIL_SECONDARY, &origin);

	if (!count) {
		return;
	}

	const float step = 1.f / count;

	for (int32_t i = 0; i < count; i++) {

		const vec3_t pos = Vec3_Mix(origin, end, step * i);

		const int32_t contents = cgi.PointContents(pos);
		if (!(contents & CONTENTS_MASK_LIQUID)) {
			continue;
		}

		cg_sprite_t *s;

		if (!(s = Cg_AllocSprite())) {
			return;
		}

		s->atlas_image = cg_sprite_bubble;
		s->origin = Vec3_Add(pos, Vec3_RandomRange(-2.f, 2.f));
		s->velocity = Vec3_RandomRange(-5.f, 5.f);
		s->velocity.z += 6.0;
		s->acceleration.z = 10.0;
		s->lifetime = 1000 - (Randomf() * 100);
		s->color = Color3bv(0x447788);

		if (contents & CONTENTS_LAVA) {
			s->color = Color3bv(0x886644);
			s->velocity = Vec3_Scale(s->velocity, .33f);
		} else if (contents & CONTENTS_SLIME) {
			s->color = Color3bv(0x556644);
			s->velocity = Vec3_Scale(s->velocity, .66f);
		}

		s->color.a = RandomRangef(.4f, .7f);
		s->size = RandomRangef(1.f, 2.f);
		s->size_velocity = -s->size / MILLIS_TO_SECONDS(s->lifetime);
	}
}

/**
 * @brief
 */
static void Cg_BlasterTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

	cg_sprite_t *s;

	color_t color = Cg_ResolveEffectColor(ent->current.client, EFFECT_COLOR_ORANGE);
	color.a = 0;

	Cg_BubbleTrail(ent, start, end, 12.0);

	vec3_t origin;
	const int32_t count = Cg_TrailDensity(ent, start, end, 30, TRAIL_PRIMARY, &origin);

	if (count) {
		const float step = 1.f / count;

		for (int32_t i = 0; i < count; i++) {

			if (!(s = Cg_AllocSprite())) {
				break;
			}

			float scale = 7.f;
			float power = 3.f;
			vec3_t pdir = Vec3_Normalize(Vec3_RandomRange(-1.f, 1.f));
			vec3_t porig = Vec3_Scale(pdir, powf(Randomf(), power) * scale);
			float pdist = Vec3_Distance(Vec3_Zero(), porig) / scale;

			s->atlas_image = cg_sprite_particle;
			s->lifetime = 1000.f;
			s->velocity = Vec3_Scale(pdir, pdist * 10.f);
			s->origin = Vec3_Add(porig, Vec3_Mix(origin, end, step * i));
				s->size = Maxf(1.7f, powf(1.7f - pdist, power));
			s->size_velocity = Mixf(-3.5f, -.2f, pdist);
			s->size_velocity *= RandomRangef(.66f, 1.f);
			s->color = Color_Mix(color, Color4fv(Vec4(1.f, 1.f, 1.f, 0.f)), pdist);
		}
	}

	if ((s = Cg_AllocSprite())) {
		s->atlas_image = cg_sprite_particle;
		s->lifetime = 0.f;
		s->origin = end;
		s->size = 8.f;
		s->color = color;
	}

	Cg_AddLight(&(cg_light_t) {
		.origin = end,
		.radius = 100.f,
		.color = Color_Vec3(color),
		.intensity = .025f,
	});
}

/**
 * @brief
 */
static void Cg_GrenadeTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

	float pulse1 = sinf(cgi.client->unclamped_time * .02f)  * .5f + .5f;
	float pulse2 = sinf(cgi.client->unclamped_time * .04f)  * .5f + .5f;
	// vec3_t dir = Vec3_Normalize(Vec3_Subtract(end, start));

	cg_sprite_t *s;

	// ring
	if ((s = Cg_AllocSprite())) {
		s->atlas_image = cg_sprite_ring;
		s->lifetime = 1;
		s->origin = ent->origin;
		s->size = pulse1 * 10.f + 20.f;
		// s->color = Color4bv(0x00660000);
		s->color = Color4f(0.f, .2f, 0.f, 0.f);
	}

	// streak
	if ((s = Cg_AllocSprite())) {
		s->atlas_image = cg_sprite_aniso_flare_01;
		s->lifetime = 1;
		s->origin = ent->origin;
		s->size = pulse2 * 10.f + 10.f;
		// s->color = Color4f(0.f, .5f + pulse2 * .5f, 0.f, 0.f);
		s->color = Color4f(0.f, .33f + pulse2 * .33f, 0.f, 0.f);
	}

	// glow
	for (int32_t i = 0; i < 2; i++) {
		if ((s = Cg_AllocSprite())) {
			s->atlas_image = cg_sprite_flash;
			s->lifetime = 1;
			s->origin = ent->origin;
			s->size = 40.f;
			// s->color = Color4f(0.f, 0.33f, 0.f, 0.f);
			s->color = Color4f(0.f, 0.22f, 0.f, 0.f);
			s->rotation = sinf(cgi.client->unclamped_time * (i == 0 ? .002f : -.001f));
		}
	}

	Cg_AddLight(&(cg_light_t) {
		.origin = end, // TODO: find a way to nudge this away from the surface a bit
		.radius = 40.f + 20.f * pulse1,
		.color = Color_Vec3(color_green)
	});
}

static inline void Cg_ParticleTrailLifeOffset(vec3_t start, vec3_t end, float speed, float step, float *life_start, float *life_frac) {

	const float trail_distance = Vec3_Distance(start, end);
	*life_start = (speed - trail_distance) / 1000;
	*life_frac = (1.0 - *life_start) * step;
}

/**
 * @brief
 */
static void Cg_RocketTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {
	cg_sprite_t *s;
	int32_t count;
	vec3_t origin;

	float sine = sinf(cgi.client->unclamped_time * .001f) * M_PI;

	const vec3_t rocket_velocity = Vec3_Subtract(ent->current.origin, ent->prev.origin);
	const vec3_t rocket_direction = Vec3_Normalize(rocket_velocity);
	const float rocket_speed = Vec3_Length(rocket_velocity) / QUETOO_TICK_SECONDS;

	// exhaust flare
	for (int32_t i = 0; i < 2; i++) {
		if ((s = Cg_AllocSprite())) {
			s->atlas_image = cg_sprite_explosion_flash;
			s->lifetime = 1;
			s->origin = Vec3_Add(ent->origin, Vec3_Scale(rocket_direction, -20.f));
			s->size = 30.f;
			s->color = Color4f(.5f, .5f, .5f, 0.f);
			s->rotation = (i == 0 ? sine : -sine);
		}
	}

	// exhaust flames
	count = Cg_TrailDensity(ent, start, end, 8, TRAIL_PRIMARY, &origin);
	if (count) {
		float step = 1.f / count;
		float life_start, life_frac;

		Cg_ParticleTrailLifeOffset(origin, end, rocket_speed, step, &life_start, &life_frac);

		for (int32_t i = 0; i < count; i++) {
			const float particle_life_frac = life_start + (life_frac * (i + 1));

			// fire
			if (!(s = Cg_AllocSprite())) {
				break;
			}
			s->animation = cg_sprite_rocket_flame;
			s->color = Color4bv(0xFFFFFF00);
			s->lifetime = cg_sprite_rocket_flame->num_frames * FRAMES_TO_SECONDS(120) * particle_life_frac;
			s->origin = Vec3_Mix(origin, end, step * i);
			s->velocity = rocket_velocity;
			s->rotation = Randomf() * 2.f * M_PI;
			s->size = 8.f;
			s->size_velocity = -20.f;
		}
	}

	// smoke trail
	count = Cg_TrailDensity(ent, start, end, 2, TRAIL_SECONDARY, &origin);
	if (count) {
		float step = 1.f / count;
		float life_start, life_frac;

		Cg_ParticleTrailLifeOffset(origin, end, rocket_speed, step, &life_start, &life_frac);

		for (int32_t i = 0; i < count; i++) {
			const float particle_life_frac = life_start + (life_frac * (i + 1));

			// interlace smoke 1 and 2 for some subtle variety
			// smoke 1
			if (!(s = Cg_AllocSprite())) {
				break;
			}
			s->animation = cg_sprite_smoke_04;
			s->lifetime = cg_sprite_smoke_04->num_frames * FRAMES_TO_SECONDS(60) * particle_life_frac;
			s->origin = Vec3_Add(Vec3_Mix(origin, end, step * i), Vec3_RandomRange(-2.5f, 2.5f));
			s->velocity = Vec3_Scale(rocket_velocity, 0.5);
			s->rotation = Randomf() * 2.f * M_PI;
			s->size = Randomf() * 5.f + 10.f;
			s->size_velocity = Randomf() * 5.f + 10.f;
			s->color = Color4f(1.f, 1.f, 1.f, .25f);

			// smoke 2
			if (!(s = Cg_AllocSprite())) {
				break;
			}
			s->animation = cg_sprite_smoke_05;
			s->lifetime = cg_sprite_smoke_05->num_frames * FRAMES_TO_SECONDS(60) * particle_life_frac;
			s->origin = Vec3_Add(Vec3_Mix(origin, end, (step * i) + (step * .5f)), Vec3_RandomRange(-2.5f, 2.5f));
			s->velocity = Vec3_Scale(rocket_velocity, 0.5);
			s->rotation = Randomf() * 2.f * M_PI;
			s->size = Randomf() * 5.f + 10.f;
			s->size_velocity = Randomf() * 5.f + 10.f;
			s->color = Color4f(1.f, 1.f, 1.f, .25f);
		}
	}

	// firefly trail
	count = Cg_TrailDensity(ent, start, end, 10, TRAIL_BUBBLE, &origin);
	if (count) {
		float step = 1.f / count;
		float life_start, life_frac;

		Cg_ParticleTrailLifeOffset(origin, end, rocket_speed, step, &life_start, &life_frac);

		for (int32_t i = 0; i < count; i++) {
			const float particle_life_frac = life_start + (life_frac * (i + 1));

			// sparks
			if (!(s = Cg_AllocSprite())) {
				break;
			}
			s->atlas_image = cg_sprite_particle;
			s->lifetime = RandomRangef(900.f, 1300.f) * particle_life_frac;
			s->origin = Vec3_Mix(origin, end, step * i);
			s->velocity = Vec3_Add(s->velocity, Vec3_RandomRange(-10.f, 10.f));
			s->acceleration = Vec3_RandomRange(-10.f, 10.f);
			s->color = Color4f(1.f, 1.f, 1.f, 0.f);
			s->end_color = Color4f(0, -1.5f, -3.f, 0);
			s->size = Randomf() * 1.6f + 1.6f;
		}
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

	cg_sprite_t *s;
	r_atlas_image_t *variation[] = {
		cg_sprite_plasma_var01,
		cg_sprite_plasma_var02,
		cg_sprite_plasma_var03
	};

	// outer rim
	for (int32_t i = 0; i < 3; i++)
	{
		if ((s = Cg_AllocSprite())) {
			s->origin = ent->origin;
			s->size = RandomRangef(15.f, 20.f);
			s->color = Color4f(.2f, .35f, .45f, .0f);
			// s->color = Color4f(.4f, .7f, .9f, .0f);
			s->atlas_image = variation[i];
			s->rotation = RandomRadian();
			s->lifetime = 50.f;
		}
	}

	// center blob
	if ((s = Cg_AllocSprite())) {
		s->origin = ent->origin;
		s->size = RandomRangef(15.f, 20.f);
		s->color = Color4f(.2f, .35f, .45f, .0f);
		s->atlas_image = cg_sprite_blob_01;
		s->rotation = RandomRadian();
		s->lifetime = 20.f;
	}

	// center core
	if ((s = Cg_AllocSprite())) {
		s->origin = ent->origin;
		s->size = RandomRangef(6.f, 9.f);
		s->color = Color4f(.2f, .35f, .45f, .0f);
		s->atlas_image = cg_sprite_particle;
		s->rotation = RandomRadian();
		s->lifetime = 20.f;
	}

	if (cgi.PointContents(ent->origin) & CONTENTS_MASK_LIQUID) {
		const float radius = 6.f;

		Cg_BubbleTrail(ent, ent->prev.origin, ent->origin, radius / 4.0);
	}

	Cg_AddLight(&(cg_light_t) {
		.origin = ent->origin,
		.radius = 100.f,
		.color = Vec3(.4f, .7f, 1.f),
		.intensity = 0.1
	});
}

/**
 * @brief
 */
static void Cg_LightningTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {
	const vec3_t dir = Vec3_Normalize(Vec3_Subtract(start, end));

	cg_light_t l = {
		.origin = start,
		.radius = 90.f + RandomRangef(-10.f, 10.f),
		.color = Vec3(.6f, .6f, 1.f)
	};

	Cg_AddLight(&l);

	cgi.AddBeam(&(const r_beam_t) {
		.start = start,
		.end = end,
		.color = Color4f(1.f, 1.f, 1.f, 0.f),
		.image = cg_beam_lightning,
		.size = 8.5f,
		.translate = cgi.client->unclamped_time * RandomRangef(.003f, .009f),
		.stretch = RandomRangef(.2f, .4f),
	});

	l.origin = Vec3_Add(end, Vec3_Scale(dir, 12.0));
	l.radius = 90.0 + RandomRangef(-10.f, 10.f);
	Cg_AddLight(&l);

	if (ent->current.animation1 != LIGHTNING_SOLID_HIT) {
		return;
	}

	cg_sprite_t *s;

	if (ent->timestamp < cgi.client->unclamped_time) {

		cm_trace_t tr = cgi.Trace(start, Vec3_Add(end, Vec3_Scale(dir, -128.0)), Vec3_Zero(), Vec3_Zero(), 0, CONTENTS_SOLID);

		if (tr.surface) {
			vec3_t pos = Vec3_Add(tr.end, Vec3_Scale(tr.plane.normal, 1.0));

			cgi.AddStain(&(const r_stain_t) {
				.origin = pos,
				.radius = 8.0 + Randomf() * 4.0,
				.color = Color4bv(0x00000040),
			});

			pos = Vec3_Add(tr.end, Vec3_Scale(tr.plane.normal, 2.0));

			if ((cgi.PointContents(pos) & CONTENTS_MASK_LIQUID) == 0) {

				for (int32_t i = 0; i < 2; i++) {
					if ((s = Cg_AllocSprite())) {
						vec3_t org;
						org = Vec3_Add(end, Vec3_Scale(dir, 10.f));
						org = Vec3_Add(org, Vec3_RandomRange(-10.f, 10.f));

						s->origin = org;
						s->atlas_image = cg_sprite_electro_02;
						s->lifetime = 60 * (i + 1);
						s->color = Color4bv(0xFFFFFF00);
						s->size = RandomRangef(100.f, 200.f);
						s->size_velocity = 400.f;
						s->rotation = Randomf() * 2.f * M_PI;
						s->dir = Vec3_RandomRange(-1.f, 1.f);
					}
				}

				for (int32_t i = 0; i < 2; i++) {
					if (!(s = Cg_AllocSprite())) {
						break;
					}
					s->atlas_image = cg_sprite_particle2;
					s->origin = pos;
					s->velocity = Vec3_Scale(Vec3_Add(tr.plane.normal, Vec3_RandomRange(-.2f, .2f)), RandomRangef(50, 200));
					s->acceleration.z = -SPRITE_GRAVITY * 3.0;
					s->lifetime = 600 + Randomf() * 300;
					s->color = Color4f(1.f, 1.f, 1.f, 0.f);
					s->end_color = Color4f(.5f, .75f, 1.f, 0.f);
					s->bounce = 0.2f;
					s->size = 2.f + RandomRangef(.5f, 2.f);
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

	vec3_t forward;
	Vec3_Vectors(ent->angles, &forward, NULL, NULL);

	cgi.AddBeam(&(const r_beam_t) {
		.start = start,
		.end = Vec3_Add(end, Vec3_Scale(forward, -3.f)),
		.color = Cg_ResolveEffectColor(ent->current.client, EFFECT_COLOR_GREEN),
		.image = cg_beam_hook,
		.size = 1.f
	});
}

/**
 * @brief
 */
static void Cg_BfgTrail(cl_entity_t *ent) {
	cg_sprite_t *s;

	vec3_t delta = Vec3_Subtract(ent->origin, ent->previous_origin);
	float mod = sinf(cgi.client->unclamped_time >> 5) * 0.5 + 0.5;

	// projectile glow
	if ((s = Cg_AllocSprite())) {
		s->origin = ent->origin;
		s->size = 20.f * mod + 30.f;
		s->color = Color4f(.4f, 1.f, .4f, .0f);
		s->atlas_image = cg_sprite_particle;
		s->lifetime = 20.f;
	}

	// big trail
	{
		float len = Vec3_Length(delta);
		vec3_t dir = Vec3_Scale(delta, 1.f / len);

		int32_t count = Cg_TrailDensity(ent, ent->previous_origin, ent->origin, 5, TRAIL_PRIMARY, &ent->origin);
		float step = 1.f / count;
		float life_start, life_frac;


		if (count) {
			Cg_ParticleTrailLifeOffset(ent->origin, ent->previous_origin, len / QUETOO_TICK_SECONDS, step, &life_start, &life_frac);
			for (int32_t i = 0; i < count; i++) {

				const float particle_life_frac = life_start + (life_frac * (i + 1));

				if (!(s = Cg_AllocSprite())) {
					break;
				}

				s->color = Color4f(1.f, 1.f, 1.f, .33f);
				s->animation = cg_sprite_bfg_explosion_2;
				s->lifetime = cg_sprite_bfg_explosion_2->num_frames * FRAMES_TO_SECONDS(30) * particle_life_frac;

				s->origin = Vec3_Add(Vec3_Mix(ent->origin, ent->previous_origin, step * i), Vec3_Scale(dir, 50.f));
				s->velocity = delta;
				s->rotation = Randomf() * 2.f * M_PI;
				s->size = 40.f;
				s->size_velocity = -40.f;
			}
		}
	}

	// projectile core
	if ((s = Cg_AllocSprite())) {
		s->origin = ent->origin;
		s->size = RandomRangef(10.f, 12.5f);
		s->color     = Color4f(1.f, 1.f, 1.f, .0f);
		s->end_color = Color4f(1.f, 1.f, 1.f, .0f);
		s->atlas_image = cg_sprite_blob_01;
		s->rotation = RandomRadian();
		s->lifetime = 1; // FIXME: flickering
		s->velocity = delta;
	}

	if (cgi.PointContents(ent->origin) & CONTENTS_MASK_LIQUID) {
		Cg_BubbleTrail(ent, ent->prev.origin, ent->origin, 4.0);
	}

	Cg_AddLight(&(cg_light_t) {
		.origin = ent->origin,
		.radius = 160.f,
		.color = Vec3_Scale(Vec3(.4f, 1.f, .4f), mod * .6f + .4f)
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
		cg_sprite_t *s;

		if (!(s = Cg_AllocSprite())) {
			break;
		}

		s->atlas_image = cg_sprite_particle;
		s->origin = Vec3_Add(ent->origin, Vec3_RandomRange(-32.0f, 32.0f));
		s->velocity.z = RandomRangef(80.f, 120.f);
		s->acceleration = Vec3_RandomRange(-80.f, 80.f);
		s->acceleration.z = 20.f;
		s->lifetime = 450;
		s->color = color;
		s->color.a = 0.f;
		s->size = 10.f;
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

	cg_sprite_t *s;

	if ((s = Cg_AllocSprite())) {
		s->atlas_image = cg_sprite_particle;
		s->origin = ent->origin;
		s->origin.z -= 20.0;
		s->velocity.z = 2.0;
		s->lifetime = 450;
		s->color = color;
		s->size = 16.f;
	}
}

/**
 * @brief
 */
static void Cg_GibTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

	if (cgi.PointContents(end) & CONTENTS_MASK_LIQUID) {
		Cg_BubbleTrail(ent, start, end, 8.0);
		return;
	}

	const vec3_t dir = Vec3_Normalize(Vec3_Subtract(end, start));

	vec3_t origin;
	const int32_t count = Cg_TrailDensity(ent, start, end, 3, TRAIL_PRIMARY, &origin);

	if (!count) {
		return;
	}

	float step = 1.f / count;

	for (int32_t i = 0; i < count; i++) {
		cg_sprite_t *s;

		if (!(s = Cg_AllocSprite())) {
			break;
		}

		s->animation = cg_sprite_blood_01;
		s->lifetime = cg_sprite_blood_01->num_frames * FRAMES_TO_SECONDS(30) + Randomf() * 500;
		s->size = RandomRangef(40.f, 64.f);
		s->color = Color4bv(0x882200aa);
		s->rotation = RandomRadian();

		s->origin = Vec3_Mix(origin, end, step * i);
		s->velocity = Vec3_Scale(dir, 20.0);
		s->acceleration.z = -SPRITE_GRAVITY / 2.0;
		s->flags |= SPRITE_LERP;

		/*

		s->animation = cg_sprite_blood_01;
		s->lifetime = RandomRangef(1000.f, 1500.f);
		s->origin = Vec3_Mix(origin, end, step * i);
		s->velocity = Vec3_Scale(dir, 20.0);
		s->acceleration.z = -SPRITE_GRAVITY / 2.0;
		s->color = Color4bv(0x80000080);
		s->size = RandomRangef(24.f, 56.f);

		*/

		static uint32_t added = 0;
		if ((added++ % 3) == 0) {
			cgi.AddStain(&(const r_stain_t) {
				.origin = s->origin,
				.radius = 12.0 * Randomf() * 3.0,
				.color = Color4bv(0x80101080),
			});
		}
	}
}

/**
 * @brief
 */
static void Cg_FireballTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

	if (cgi.PointContents(end) & CONTENTS_MASK_LIQUID) {
		return;
	}

	cg_light_t l = {
		.origin = end,
		.radius = 85.f,
		.color = Vec3(0.9, 0.3, 0.1),
		.intensity = 0.125,
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

	for (cl_entity_trail_t *trail = ent->trails; trail < ent->trails + lengthof(ent->trails); trail++) {
		if (trail->trail_updated) {
			trail->last_origin = end;
			trail->trail_updated = false;
		}
	}
}
