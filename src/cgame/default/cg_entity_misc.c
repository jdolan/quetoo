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
 * @brief The cmisc_dist data type.
 */
typedef struct {
	/**
	 * @brief The sprite template.
	 */
	cg_sprite_t sprite;

	/**
	 * @brief The sprite origins, clipped to the backing entity's brushes.
	 */
	vec3_t *origins;
	int32_t num_origins;

	/**
	 * @brief The dust density.
	 */
	float density;

	/**
	 * @brief The count of active sprites.
	 */
	int32_t num_active;
} cg_dust_t;

/**
 * @brief
 */
static void Cg_misc_dust_Init(cg_entity_t *self) {

	cg_dust_t *dust = self->data;

	const char *name = cgi.EntityValue(self->def, "sprite")->nullable_string ?: "particle";
	dust->sprite.image = cgi.LoadImage(va("sprites/%s", name), IT_EFFECT);
	if (dust->sprite.image == NULL) {
		dust->sprite.image = cgi.LoadImage("sprites/particle", IT_EFFECT);
		cgi.Warn("%s @ %s failed to load sprite %s\n",
				 self->clazz->class_name,
				 vtos(self->origin),
				 name);
	}

	dust->sprite.velocity = cgi.EntityValue(self->def, "sprite_velocity")->vec3;
	dust->sprite.size = cgi.EntityValue(self->def, "sprite_size")->value ?: 1.f;

	const cm_entity_t *color = cgi.EntityValue(self->def, "sprite_color");
	if (color->parsed & ENTITY_VEC4) {
		dust->sprite.color = color->vec4;
	} else if (color->parsed & ENTITY_VEC3) {
		dust->sprite.color = Vec3_ToVec4(color->vec3, 1.f);
	} else {
		dust->sprite.color = Vec4(0.f, 0.f, 1.f, 1.f);
	}

	const cm_entity_t *end_color = cgi.EntityValue(self->def, "sprite_end_color");
	if (end_color->parsed & ENTITY_VEC4) {
		dust->sprite.end_color = end_color->vec4;
	} else if (end_color->parsed & ENTITY_VEC3) {
		dust->sprite.end_color = Vec3_ToVec4(end_color->vec3, 1.f);
	} else {
		dust->sprite.end_color = Vec4(0.f, 0.f, 0.f, 0.f);
	}

	dust->sprite.lifetime = cgi.EntityValue(self->def, "sprite_lifetime")->integer ?: 10000;
	dust->sprite.softness = cgi.EntityValue(self->def, "sprite_softness")->value ?: 1.f;

	dust->density = cgi.EntityValue(self->def, "density")->value ?: 1.f;


	GPtrArray *brushes = cgi.EntityBrushes(self->def);
	for (guint i = 0; i < brushes->len; i++) {

		const cm_bsp_brush_t *brush = g_ptr_array_index(brushes, i);

		const vec3_t brush_size = Vec3_Subtract(brush->maxs, brush->mins);
		const int32_t brush_origins = Maxi(Vec3_Length(brush_size) * dust->density, 1);

		if (dust->origins) {
			dust->origins = cgi.Realloc(dust->origins, (dust->num_origins + brush_origins) * sizeof(vec3_t));
		} else {
			dust->origins = cgi.LinkMalloc(brush_origins * sizeof(vec3_t), dust);
		}

		int32_t j = dust->num_origins;
		while (j < dust->num_origins + brush_origins) {

			vec3_t point;
			point.x = RandomRangef(brush->mins.x, brush->maxs.x);
			point.y = RandomRangef(brush->mins.y, brush->maxs.y);
			point.z = RandomRangef(brush->mins.z, brush->maxs.z);

			if (cgi.PointInsideBrush(point, brush)) {
				dust->origins[j++] = point;
			}
		}

		dust->num_origins += brush_origins;
	}
}

/**
 * @brief
 */
static void Cg_misc_dust_SpriteThink(cg_sprite_t *sprite, float life, float delta) {

	cg_dust_t *dust = sprite->data;

	// TODO: Ease in the color if life < .1 or something?

	// TODO: This seems pretty reliable, but it's not foolproof.
	// TODO: A custom Free callback on sprites would be better.
	if (life + delta >= 1.f) {
		sprite->lifetime = 0;
		dust->num_active--;
	}
}

/**
 * @brief
 */
static void Cg_misc_dust_Think(cg_entity_t *self) {

	if (!cg_add_atmospheric->value) {
		return;
	}
	
	cg_dust_t *dust = self->data;
	while (dust->num_active < dust->num_origins) {

		cg_sprite_t s = dust->sprite;

		s.origin = dust->origins[RandomRangei(0, dust->num_origins)];
		s.origin = Vec3_Add(s.origin, Vec3_RandomDir());
		s.size = RandomRangef(s.size * .9f, s.size * 1.1f);
		s.velocity = Vec3_Add(s.velocity, Vec3_RandomDir());
		s.lifetime = RandomRangeu(s.lifetime * .666f, s.lifetime * 1.333f);
		s.think = Cg_misc_dust_SpriteThink;
		s.data = dust;
		s.flags |= SPRITE_DATA_NOFREE;

		Cg_AddSprite(&s);

		dust->num_active++;
	}
}

/**
 * @brief
 */
const cg_entity_class_t cg_misc_dust = {
	.class_name = "misc_dust",
	.Init = Cg_misc_dust_Init,
	.Think = Cg_misc_dust_Think,
	.data_size = sizeof(cg_dust_t)
};

/**
 * @brief
 */
typedef struct {
	float hz, drift;
	float radius;
} cg_flame_t;

/**
 * @brief
 */
static void Cg_misc_flame_Init(cg_entity_t *self) {

	cg_flame_t *flame = self->data;

	flame->hz = cgi.EntityValue(self->def, "hz")->value ?: 5.f;
	flame->drift = cgi.EntityValue(self->def, "drift")->value ?: .1f;

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
				.end_color = Vec4(hue, sat, 0.f, 0.f),
				.softness = 1.f
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

	Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
		.sample = cg_sample_fire,
		.origin = self->origin,
		.atten = SOUND_ATTEN_CUBIC,
	});

	self->next_think += 1000.f / flame->hz + 1000.f * flame->drift * Randomf();
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

	r_entity_t *entity = self->data;

	entity->origin = self->origin;
	entity->angles = cgi.EntityValue(self->def, "angles")->vec3;
	entity->scale = cgi.EntityValue(self->def, "scale")->value ?: 1.f;
	entity->lerp = 1.f;

	if (cgi.EntityValue(self->def, "model")->parsed & ENTITY_STRING) {
		entity->model = cgi.LoadModel(cgi.EntityValue(self->def, "model")->string);
	} else {
		cgi.Warn("%s @ %s has no sound specified\n", self->clazz->class_name, vtos(self->origin));
	}
}

/**
 * @brief
 */
static void Cg_misc_model_Think(cg_entity_t *self) {

	const r_entity_t *entity = self->data;

	if (entity->model) {
		cgi.AddEntity(cgi.view, entity);
	}
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
typedef struct {
	s_play_sample_t play;
	float hz, drift;
} cg_misc_sound_t;

/**
 * @brief
 */
static void Cg_misc_sound_Init(cg_entity_t *self) {

	cg_misc_sound_t *sound = self->data;

	sound->hz = cgi.EntityValue(self->def, "hz")->value ?: 0.f;
	sound->drift = cgi.EntityValue(self->def, "drift")->value ?: .3f;

	if (cgi.EntityValue(self->def, "sound")->parsed & ENTITY_STRING) {
		sound->play.sample = cgi.LoadSample(cgi.EntityValue(self->def, "sound")->string);
	} else {
		cgi.Warn("%s @ %s has no sound specified\n", self->clazz->class_name, vtos(self->origin));
	}

	sound->play.origin = self->origin;

	if (cgi.EntityValue(self->def, "atten")->parsed & ENTITY_INTEGER) {
		sound->play.atten = cgi.EntityValue(self->def, "atten")->integer;
	} else {
		sound->play.atten = SOUND_ATTEN_SQUARE;
	}

	sound->play.flags = S_PLAY_AMBIENT;

	if (sound->hz == 0.f) {
		sound->play.flags |= S_PLAY_LOOP | S_PLAY_FRAME;
	}
}

/**
 * @brief
 */
static void Cg_misc_sound_Think(cg_entity_t *self) {

	const cg_misc_sound_t *sound = self->data;

	if (sound->play.sample) {
		Cg_AddSample(cgi.stage, &sound->play);
	}

	self->next_think += 1000.f / sound->hz + 1000.f * sound->drift * Randomf();
}

/**
 * @brief
 */
const cg_entity_class_t cg_misc_sound = {
	.class_name = "misc_sound",
	.Init = Cg_misc_sound_Init,
	.Think = Cg_misc_sound_Think,
	.data_size = sizeof(cg_misc_sound_t)
};

/**
 * @brief
 */
typedef struct {
	float hz, drift;
	vec3_t dir;
	int32_t count;
} cg_misc_sparks_t;

/**
 * @brief
 */
static void Cg_misc_sparks_Init(cg_entity_t *self) {

	cg_misc_sparks_t *sparks = self->data;

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

	const cg_misc_sparks_t *sparks = self->data;

	Cg_SparksEffect(self->origin, sparks->dir, sparks->count);

	self->next_think += 1000.f / sparks->hz + 1000.f * sparks->drift * Randomf();
}

/**
 * @brief
 */
const cg_entity_class_t cg_misc_sparks = {
	.class_name = "misc_sparks",
	.Init = Cg_misc_sparks_Init,
	.Think = Cg_misc_sparks_Think,
	.data_size = sizeof(cg_misc_sparks_t)
};

/**
 * @brief
 */
typedef struct {
	cg_sprite_t sprite;
	float hz, drift;
} cg_misc_sprite_t;

/**
 * @brief
 */
static void Cg_misc_sprite_Init(cg_entity_t *self) {

	cg_misc_sprite_t *sprite = self->data;

	sprite->sprite.origin = self->origin;

	const cm_entity_t *target = Cg_EntityTarget(self);
	if (target) {
		const vec3_t target_origin = cgi.EntityValue(target, "origin")->vec3;
		sprite->sprite.velocity = Vec3_Subtract(target_origin, self->origin);
	} else {
		sprite->sprite.velocity = cgi.EntityValue(self->def, "velocity")->vec3;
	}

	sprite->sprite.acceleration = cgi.EntityValue(self->def, "acceleration")->vec3;
	sprite->sprite.rotation = cgi.EntityValue(self->def, "rotation")->value;
	sprite->sprite.rotation_velocity = cgi.EntityValue(self->def, "rotation_velocity")->value;
	sprite->sprite.dir = Vec3_Normalize(cgi.EntityValue(self->def, "dir")->vec3);

	const cm_entity_t *color = cgi.EntityValue(self->def, "_color");
	if (color->parsed & ENTITY_VEC4) {
		sprite->sprite.color = color->vec4;
	} else if (color->parsed & ENTITY_VEC3) {
		sprite->sprite.color = Vec3_ToVec4(color->vec3, 1.f);
	}

	const cm_entity_t *end_color = cgi.EntityValue(self->def, "_end_color");
	if (end_color->parsed & ENTITY_VEC4) {
		sprite->sprite.end_color = end_color->vec4;
	} else if (end_color->parsed & ENTITY_VEC3) {
		sprite->sprite.end_color = Vec3_ToVec4(end_color->vec3, 1.f);
	}

	sprite->sprite.size = cgi.EntityValue(self->def, "size")->value ?: 1.f;
	sprite->sprite.size_velocity = cgi.EntityValue(self->def, "size_velocity")->value;
	sprite->sprite.size_acceleration = cgi.EntityValue(self->def, "size_acceleration")->value;
	sprite->sprite.bounce = cgi.EntityValue(self->def, "bounce")->value;
	sprite->sprite.softness = cgi.EntityValue(self->def, "softness")->value;

	sprite->hz = cgi.EntityValue(self->def, "hz")->value ?: .5f;
	sprite->drift = cgi.EntityValue(self->def, "drift")->value ?: 3.f;
}

/**
 * @brief
 */
static void Cg_misc_sprite_Think(cg_entity_t *self) {

	const cg_misc_sprite_t *sprite = self->data;

	Cg_AddSprite(&sprite->sprite);

	self->next_think += 1000.f / sprite->hz + 1000.f * sprite->drift * Randomf();
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
} cg_misc_steam_t;

/**
 * @brief
 */
static void Cg_misc_steam_Init(cg_entity_t *self) {

	cg_misc_steam_t *steam = self->data;

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

	Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
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
