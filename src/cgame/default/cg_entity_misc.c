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
 * @file Client-sided entities emit lights, sounds, static models, sprites, etc.
 * These are used to add atmosphere to the level without incuring network overhead.
 */

/**
 * @return The target entity for the specified entity, if any.
 */
static const cm_entity_t *Cg_EntityTarget(const cg_entity_t *self) {

	if (cgi.EntityValue(self->def, "target")->parsed & ENTITY_STRING) {
		const char *target_name = cgi.EntityValue(self->def, "target")->string;

		const cm_bsp_t *bsp = cgi.WorldModel()->bsp->cm;
		for (size_t i = 0; i < bsp->num_entities; i++) {

			const cm_entity_t *ent = bsp->entities[i];
			if (!g_strcmp0(target_name, cgi.EntityValue(ent, "targetname")->string)) {
				return ent;
			}
		}

		const char *class_name = cgi.EntityValue(self->def, "classname")->string;
		cgi.Warn("Target not found for %s @ %s\n", class_name, vtos(self->origin));
	}

	return NULL;
}

/**
 * @brief
 */
typedef struct {
	float radius;
} cg_flame_t;

/**
 * @brief
 */
static void Cg_misc_flame_Init(cg_entity_t *self) {

	self->hz = self->hz ?: 5.f;
	self->drift = self->drift ?: .1f;

	cg_flame_t *flame = self->data;

	flame->radius = cgi.EntityValue(self->def, "radius")->value ?: 16.f;
}

/**
 * @brief
 */
static void Cg_misc_flame_Think(cg_entity_t *self) {

	const cg_flame_t *flame = self->data;

	for (int32_t i = 0; i < flame->radius / 8.f; i++) {
		const float hue = color_hue_orange + RandomRangef(-20.f, 20.f);
		const float sat = RandomRangef(.7f, 1.f);

		if (!Cg_AddSprite(&(cg_sprite_t) {
				.atlas_image = cg_sprite_flame,
				.origin = Vec3_Add(self->origin, Vec3_RandomRange(-8.f, 8.f)),
				.velocity = Vec3_RandomRange(-4.f, 4.f),
				.acceleration.z = 15.f,
				.lifetime = 500 + Randomf() * 250,
				.size = RandomRangef(4.f, 12.f),
				.bounce = .6f,
				.color = Vec4(hue, sat, RandomRangef(.7f, 1.f), RandomRangef(.56f, 1.f)),
				.end_color = Vec4(hue, sat, 0.f, 0.f)
			})) {
			break;
		}
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = self->origin,
		.radius = flame->radius,
		.color = Vec3(1.f, 0.8f, 0.4f),
		.decay = 32
	});

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_fire,
		.origin = self->origin,
		.atten = SOUND_ATTEN_CUBIC,
		.flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
const cg_entity_class_t cg_misc_flame = {
	.class_name = "misc_flame",
	.Init = Cg_misc_flame_Init,
	.Think = Cg_misc_flame_Think,
	.data_size = sizeof(cg_flame_t)
};

/**
 * @brief
 */
static void Cg_misc_light_Init(cg_entity_t *self) {

	cg_light_t *light = self->data;

	light->origin = self->origin;
	light->radius = cgi.EntityValue(self->def, "light")->value ?: DEFAULT_LIGHT;

	const cm_entity_t *color = cgi.EntityValue(self->def, "_color");
	if (color->parsed & ENTITY_VEC3) {
		light->color = color->vec3;
	} else {
		light->color = Vec3(1.f, 1.f, 1.f);
	}

	light->intensity = cgi.EntityValue(self->def, "intensity")->value;
}

/**
 * @brief
 */
static void Cg_misc_light_Think(cg_entity_t *self) {

	Cg_AddLight((cg_light_t *) self->data);
}

/**
 * @brief
 */
const cg_entity_class_t cg_misc_light = {
	.class_name = "misc_light",
	.Init = Cg_misc_light_Init,
	.Think = Cg_misc_light_Think,
	.data_size = sizeof(cg_light_t)
};

/**
 * @brief
 */
static void Cg_misc_model_Init(cg_entity_t *self) {

	r_entity_t *model = self->data;

	model->origin = self->origin;
	model->angles = cgi.EntityValue(self->def, "angles")->vec3;
	model->scale = cgi.EntityValue(self->def, "scale")->value ?: 1.f;
	model->lerp = 1.f;
}

/**
 * @brief
 */
static void Cg_misc_model_Think(cg_entity_t *self) {

	cgi.AddEntity((r_entity_t *) self->data);
}

/**
 * @brief
 */
const cg_entity_class_t cg_misc_model = {
	.class_name = "misc_model",
	.Init = Cg_misc_model_Init,
	.Think = Cg_misc_model_Think,
	.data_size = sizeof(r_entity_t)
};

/**
 * @brief
 */
static void Cg_misc_sound_Init(cg_entity_t *self) {

	s_play_sample_t *sound = self->data;

	sound->sample = cgi.LoadSample(cgi.EntityValue(self->def, "sound")->string);
	sound->origin = self->origin;

	if (cgi.EntityValue(self->def, "atten")->parsed & ENTITY_INTEGER) {
		sound->atten = cgi.EntityValue(self->def, "atten")->integer;
	} else {
		sound->atten = SOUND_ATTEN_SQUARE;
	}

	sound->flags = S_PLAY_AMBIENT;

	if (sound->atten != SOUND_ATTEN_NONE) {
		sound->flags |= S_PLAY_POSITIONED;
	}

	if (self->hz == 0.f) {
		sound->flags |= S_PLAY_LOOP | S_PLAY_FRAME;
	}
}

/**
 * @brief
 */
static void Cg_misc_sound_Think(cg_entity_t *self) {

	const s_play_sample_t *sample = self->data;

	cgi.AddSample(sample);
}

/**
 * @brief
 */
const cg_entity_class_t cg_misc_sound = {
	.class_name = "misc_sound",
	.Init = Cg_misc_sound_Init,
	.Think = Cg_misc_sound_Think,
	.data_size = sizeof(s_play_sample_t)
};

/**
 * @brief
 */
typedef struct {
	float hz, drift;
	vec3_t dir;
	int32_t count;
} cg_sparks_t;

/**
 * @brief
 */
static void Cg_misc_sparks_Init(cg_entity_t *self) {

	cg_sparks_t *sparks = self->data;

	sparks->hz = cgi.EntityValue(self->def, "hz")->value ?: .5f;
	sparks->drift = cgi.EntityValue(self->def, "drift")->value ?: 3.f;

	const cm_entity_t *target = Cg_EntityTarget(self);
	if (target) {
		const vec3_t target_origin = cgi.EntityValue(target, "origin")->vec3;
		sparks->dir = Vec3_Normalize(Vec3_Subtract(target_origin, self->origin));
	} else {
		if (cgi.EntityValue(self->def, "_angle")->parsed & ENTITY_INTEGER) {
			const int32_t angle = cgi.EntityValue(self->def, "_angle")->integer;
			if (angle == -1.f) {
				sparks->dir = Vec3_Up();
			} else if (angle == -2.f) {
				sparks->dir = Vec3_Down();
			} else {
				Vec3_Vectors(Vec3(0.f, angle, 0.f), &sparks->dir, NULL, NULL);
			}
		} else {
			sparks->dir = Vec3_Up();
		}
	}

	sparks->count = cgi.EntityValue(self->def, "count")->integer ?: 12;
}

/**
 * @brief
 */
static void Cg_misc_sparks_Think(cg_entity_t *self) {

	const cg_sparks_t *sparks = self->data;

	Cg_SparksEffect(self->origin, sparks->dir, sparks->count);
}

/**
 * @brief
 */
const cg_entity_class_t cg_misc_sparks = {
	.class_name = "misc_sparks",
	.Init = Cg_misc_sparks_Init,
	.Think = Cg_misc_sparks_Think,
	.data_size = sizeof(cg_sparks_t)
};

/**
 * @brief
 */
static void Cg_misc_sprite_Init(cg_entity_t *self) {

	cg_sprite_t *sprite = self->data;

	sprite->origin = self->origin;

	const cm_entity_t *target = Cg_EntityTarget(self);
	if (target) {
		const vec3_t target_origin = cgi.EntityValue(target, "origin")->vec3;
		sprite->velocity = Vec3_Subtract(target_origin, self->origin);
	} else {
		sprite->velocity = cgi.EntityValue(self->def, "velocity")->vec3;
	}

	sprite->acceleration = cgi.EntityValue(self->def, "acceleration")->vec3;
	sprite->rotation = cgi.EntityValue(self->def, "rotation")->value;
	sprite->rotation_velocity = cgi.EntityValue(self->def, "rotation_velocity")->value;
	sprite->dir = Vec3_Normalize(cgi.EntityValue(self->def, "dir")->vec3);

	const cm_entity_t *color = cgi.EntityValue(self->def, "_color");
	if (color->parsed & ENTITY_VEC4) {
		sprite->color = color->vec4;
	} else if (color->parsed & ENTITY_VEC3) {
		sprite->color = Vec3_ToVec4(color->vec3, 1.f);
	}

	const cm_entity_t *end_color = cgi.EntityValue(self->def, "_end_color");
	if (end_color->parsed & ENTITY_VEC4) {
		sprite->end_color = end_color->vec4;
	} else if (end_color->parsed & ENTITY_VEC3) {
		sprite->end_color = Vec3_ToVec4(end_color->vec3, 1.f);
	}

	sprite->size = cgi.EntityValue(self->def, "size")->value ?: 1.f;
	sprite->size_velocity = cgi.EntityValue(self->def, "size")->value;
	sprite->size_acceleration = cgi.EntityValue(self->def, "size_acceleration")->value;
	sprite->bounce = cgi.EntityValue(self->def, "bounce")->value;
}

/**
 * @brief
 */
static void Cg_misc_sprite_Think(cg_entity_t *self) {

	Cg_AddSprite((cg_sprite_t *) self->data);
}

/**
 * @brief
 */
const cg_entity_class_t cg_misc_sprite = {
	.class_name = "misc_sprite",
	.Init = Cg_misc_sprite_Init,
	.Think = Cg_misc_sprite_Think,
	.data_size = sizeof(cg_sprite_t)
};

/**
 * @brief
 */
typedef struct {
	float hz, drift;
} cg_steam_t;

/**
 * @brief
 */
static void Cg_misc_steam_Init(cg_entity_t *self) {

	cg_steam_t *steam = self->data;

	steam->hz = cgi.EntityValue(self->def, "hz")->value ?: .3f;
	steam->drift = cgi.EntityValue(self->def, "drift")->value ?: .01f;
}

/**
 * @brief
 */
static void Cg_misc_steam_Think(cg_entity_t *self) {
	/*const vec3_t end = Vec3_Add(self->origin, self->steam.velocity);

	if (cgi.PointContents(self->origin) & CONTENTS_MASK_LIQUID) {
		Cg_BubbleTrail(NULL, self->origin, end, 10.f);
		return;
	}

	Cg_AddSprite(&(cg_sprite_t) {
		.atlas_image = cg_sprite_steam,
		.origin = org,
		.velocity = Vec3_Add(vel, Vec3_RandomRange(-2.f, 2.f)),
		.lifetime = 4500 / (5.f + Randomf() * .5f),
		.size = 8.f,
		.color = Vec4(0.f, 0.f, 1.f, .19f)
	});

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_steam,
		.origin = org,
		.attenuation = SOUND_ATTEN_CUBIC,
		.flags = S_PLAY_POSITIONED
	});*/
}

/**
 * @brief
 */
const cg_entity_class_t cg_misc_steam = {
	.class_name = "misc_steam",
	.Init = Cg_misc_steam_Init,
	.Think = Cg_misc_steam_Think,
	.data_size = sizeof(s_play_sample_t)
};
