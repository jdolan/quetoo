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
 * @brief The misc_dist type.
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
	dust->sprite.image = cgi.LoadImage(va("sprites/%s", name), IT_SPRITE);
	if (dust->sprite.image == NULL) {
		dust->sprite.image = cgi.LoadImage("sprites/particle", IT_SPRITE);
		cgi.Warn("%s @ %s failed to load sprite %s\n",
				 self->clazz->class_name,
				 vtos(self->origin),
				 name);
	}

	dust->sprite.velocity = cgi.EntityValue(self->def, "velocity")->vec3;
	dust->sprite.acceleration = cgi.EntityValue(self->def, "acceleration")->vec3;
	dust->sprite.rotation = cgi.EntityValue(self->def, "rotation")->value;
	dust->sprite.rotation_velocity = cgi.EntityValue(self->def, "rotation_velocity")->value;
	dust->sprite.dir = cgi.EntityValue(self->def, "dir")->vec3;

	const cm_entity_t *color = cgi.EntityValue(self->def, "_color");
	if (color->parsed & ENTITY_VEC4) {
		dust->sprite.color = color->vec4;
	} else if (color->parsed & ENTITY_VEC3) {
		dust->sprite.color = Vec3_ToVec4(color->vec3, 1.f);
	} else {
		dust->sprite.color = Vec4(0.f, 0.f, 1.f, 1.f);
	}

	const cm_entity_t *end_color = cgi.EntityValue(self->def, "_end_color");
	if (end_color->parsed & ENTITY_VEC4) {
		dust->sprite.end_color = end_color->vec4;
	} else if (end_color->parsed & ENTITY_VEC3) {
		dust->sprite.end_color = Vec3_ToVec4(end_color->vec3, 0.f);
	} else {
		dust->sprite.end_color = Vec4(0.f, 0.f, 0.f, 0.f);
	}

	dust->sprite.size = cgi.EntityValue(self->def, "_size")->value ?: 1.f;
	dust->sprite.size_velocity = cgi.EntityValue(self->def, "size_velocity")->value;
	dust->sprite.size_acceleration = cgi.EntityValue(self->def, "size_acceleration")->value;
	dust->sprite.width = cgi.EntityValue(self->def, "width")->value;
	dust->sprite.height = cgi.EntityValue(self->def, "height")->value;
	dust->sprite.bounce = cgi.EntityValue(self->def, "bounce")->value;
	dust->sprite.lifetime = SECONDS_TO_MILLIS(cgi.EntityValue(self->def, "lifetime")->value ?: 10.f);
	dust->sprite.softness = cgi.EntityValue(self->def, "softness")->value ?: 1.f;
	dust->sprite.lighting = cgi.EntityValue(self->def, "lighting")->value ?: 1.f;

	dust->density = cgi.EntityValue(self->def, "density")->value ?: 1.f;

	GPtrArray *brushes = cgi.EntityBrushes(self->def);
	for (guint i = 0; i < brushes->len; i++) {

		const cm_bsp_brush_t *brush = g_ptr_array_index(brushes, i);

		const vec3_t brush_size = Box3_Size(brush->bounds);
		const int32_t brush_origins = Maxi(Vec3_Length(brush_size) * dust->density, 1);

		if (dust->origins) {
			dust->origins = cgi.Realloc(dust->origins, (dust->num_origins + brush_origins) * sizeof(vec3_t));
		} else {
			dust->origins = cgi.LinkMalloc(brush_origins * sizeof(vec3_t), dust);
		}

		int32_t j = dust->num_origins;
		while (j < dust->num_origins + brush_origins) {

			const vec3_t point = Box3_RandomPoint(brush->bounds);

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

	if (life <= .1f) {
		sprite->color = Vec4_Scale(dust->sprite.color, life / .1f);
	}

	if (life >= 1.f) {
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
		s.Think = Cg_misc_dust_SpriteThink;
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
 * @brief The misc_flame type.
 */
typedef struct {
	/**
	 * @brief Flame radius.
	 */
	float radius;

	/**
	 * @brief Flame density.
	 */
	float density;

	/**
	 * @brief The last time a light was emitted.
	 */
	uint32_t light_time;

	/**
	 * @brief The light decay interval.
	 */
	uint32_t light_decay;
} cg_flame_t;

/**
 * @brief
 */
static void Cg_misc_flame_Init(cg_entity_t *self) {

	self->hz = cgi.EntityValue(self->def, "hz")->value ?: 10.f;
	self->drift = cgi.EntityValue(self->def, "drift")->value ?: .1f;

	cg_flame_t *flame = self->data;

	flame->density = cgi.EntityValue(self->def, "density")->value ?: 1.f;
	flame->radius = cgi.EntityValue(self->def, "radius")->value ?: 16.f;
}

/**
 * @brief
 */
static void Cg_misc_flame_Think(cg_entity_t *self) {

	cg_flame_t *flame = self->data;

	const float r = flame->radius;
	const float s = Clampf(flame->radius / 64.f, .125f, 1.f);

	for (int32_t i = 0; i < flame->radius * flame->density; i++) {
		const float hue = color_hue_orange + RandomRangef(-20.f, 20.f);
		const float sat = RandomRangef(.7f, 1.f);

		if (!Cg_AddSprite(&(cg_sprite_t) {
				.atlas_image = cg_sprite_flame,
				.origin = Vec3_Fmaf(self->origin, r, Vec3_RandomRanges(-s, s, -s, s, -.1f, .5f)),
				.velocity = Vec3_Scale(Vec3_RandomRanges(-r, r, -r, r, 0.f, 24.f), s * s),
				.acceleration.z = 150.f * s,
				.lifetime = 750 + Randomf() * 250,
				.size = 1.f,
				.size_velocity = 16.f,
				.size_acceleration = 150.f * s,
				.color = Vec4(hue, sat, RandomRangef(.7f, 1.f), RandomRangef(.56f, 1.f)),
				.end_color = Vec4(hue, sat, 0.f, 0.f),
				.softness = 1.f
			})) {
			break;
		}
	}

	if (cgi.client->unclamped_time - flame->light_time > flame->light_decay * .9f) {

		flame->light_time = cgi.client->unclamped_time;
		flame->light_decay = RandomRangeu(100, 500);

		Cg_AddLight(&(const cg_light_t) {
			.origin = self->origin,
			.radius = r * RandomRangef(3.f, 4.f),
			.color = Vec3(1.f, 0.5f, 0.3f),
			.intensity = .05f,
			.decay = flame->light_decay,
		});
	}

	Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
		.sample = cg_sample_fire,
		.origin = self->origin,
		.atten = SOUND_ATTEN_CUBIC,
		.flags = S_PLAY_AMBIENT | S_PLAY_LOOP | S_PLAY_FRAME,
		.pitch = RandomRangei(-1, 1),
		.entity = self->id
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
 * @brief Animated light styles.
 */
typedef struct {
	/**
	 * @brief The style string, a-z (26 levels), animated at 10Hz.
	 * @details With 256 bytes at 10hz, you can animate up to 25 seconds.
	 */
	char string[MAX_BSP_ENTITY_VALUE];

	/**
	 * @brief The current index into the string, using modulo.
	 */
	uint32_t index;

	/**
	 * @brief The last time the animation was advanced.
	 */
	uint32_t time;
} cg_light_style_t;

/**
 * @brief The misc_light data type.
 */
typedef struct {
	/**
	 * @brief The light instance.
	 */
	cg_light_t light;

	cg_light_style_t style;
} cg_misc_light_data_t;

/**
 * @brief
 */
static void Cg_misc_light_Init(cg_entity_t *self) {

	self->hz = cgi.EntityValue(self->def, "hz")->value;
	self->drift = cgi.EntityValue(self->def, "drift")->value;

	cg_misc_light_data_t *data = self->data;

	data->light.origin = self->origin;
	data->light.radius = cgi.EntityValue(self->def, "light")->value ?: DEFAULT_LIGHT;

	const cm_entity_t *color = cgi.EntityValue(self->def, "_color");
	if (color->parsed & ENTITY_VEC3) {
		data->light.color = color->vec3;
	} else {
		data->light.color = Vec3(1.f, 1.f, 1.f);
	}

	data->light.intensity = cgi.EntityValue(self->def, "intensity")->value;

	const char *style = cgi.EntityValue(self->def, "style")->nullable_string ?: "zzz";
	g_strlcpy(data->style.string, style, sizeof(data->style.string));
}

/**
 * @brief
 */
static void Cg_misc_light_Think(cg_entity_t *self) {

	cg_misc_light_data_t *data = self->data;

	cg_light_t light = data->light;

	cg_light_style_t *style = &data->style;
	if (*style->string) {
		const size_t len = strlen(style->string);

		if (cgi.client->unclamped_time - style->time >= 100) {
			style->index++;
			style->time = cgi.client->unclamped_time;
		}

		const float lerp = (cgi.client->unclamped_time - style->time) / 100.f;

		const float s = (style->string[(style->index + 0) % len] - 'a') / (float) ('z' - 'a');
		const float t = (style->string[(style->index + 1) % len] - 'a') / (float) ('z' - 'a');

		light.intensity = Clampf(Mixf(s, t, lerp), FLT_EPSILON, 1.f);

		if (data->light.intensity) {
			light.intensity *= data->light.intensity;
		}
	}

	Cg_AddLight(&light);
}

/**
 * @brief
 */
const cg_entity_class_t cg_misc_light = {
	.class_name = "misc_light",
	.Init = Cg_misc_light_Init,
	.Think = Cg_misc_light_Think,
	.data_size = sizeof(cg_misc_light_data_t)
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
		cgi.Warn("%s @ %s has no model specified\n", self->clazz->class_name, vtos(self->origin));
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
 * @brief The misc_sound type.
 */
typedef struct {
	/**
	 * @brief The play sample template.
	 */
	s_play_sample_t play;
} cg_misc_sound_t;

/**
 * @brief
 */
static void Cg_misc_sound_Init(cg_entity_t *self) {

	self->hz = cgi.EntityValue(self->def, "hz")->value ?: 0.f;
	self->drift = cgi.EntityValue(self->def, "drift")->value ?: .3f;

	cg_misc_sound_t *sound = self->data;

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

	if (self->hz == 0.f) {
		sound->play.flags |= S_PLAY_LOOP | S_PLAY_FRAME;
	}

	sound->play.entity = self->id;
}

/**
 * @brief
 */
static void Cg_misc_sound_Think(cg_entity_t *self) {

	const cg_misc_sound_t *sound = self->data;

	if (sound->play.sample) {
		Cg_AddSample(cgi.stage, &sound->play);
	}
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
 * @brief The misc_sparks type.
 */
typedef struct {
	/**
	 * @brief The sparks direction, configured by either key, or by target entity.
	 */
	vec3_t dir;

	/**
	 * @brief The count of sparks per emission.
	 */
	int32_t count;
} cg_misc_sparks_t;

/**
 * @brief
 */
static void Cg_misc_sparks_Init(cg_entity_t *self) {

	self->hz = cgi.EntityValue(self->def, "hz")->value ?: .5f;
	self->drift = cgi.EntityValue(self->def, "drift")->value ?: 3.f;

	cg_misc_sparks_t *sparks = self->data;

	if (self->target) {
		const vec3_t target_origin = cgi.EntityValue(self->target, "origin")->vec3;
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
 * @brief The misc_sprite type.
 */
typedef struct {
	/**
	 * @brief The sprite template instance.
	 */
	cg_sprite_t sprite;

	/**
	 * @brief The count of sprites to spawn per Think.
	 */
	int32_t count;
} cg_misc_sprite_t;

/**
 * @brief
 */
static void Cg_misc_sprite_Init(cg_entity_t *self) {

	self->hz = cgi.EntityValue(self->def, "hz")->value ?: .5f;
	self->drift = cgi.EntityValue(self->def, "drift")->value ?: 3.f;

	cg_misc_sprite_t *sprite = self->data;

	sprite->count = cgi.EntityValue(self->def, "count")->integer ?: 1;

	const char *name = cgi.EntityValue(self->def, "sprite")->nullable_string ?: "particle";
	sprite->sprite.image = cgi.LoadImage(va("sprites/%s", name), IT_SPRITE);
	if (sprite->sprite.image == NULL) {
		sprite->sprite.image = cgi.LoadImage("sprites/particle", IT_SPRITE);
		cgi.Warn("%s @ %s failed to load sprite %s\n",
				 self->clazz->class_name,
				 vtos(self->origin),
				 name);
	}

	sprite->sprite.origin = self->origin;

	if (self->target) {
		const vec3_t target_origin = cgi.EntityValue(self->target, "origin")->vec3;
		sprite->sprite.velocity = Vec3_Subtract(target_origin, self->origin);
	} else {
		sprite->sprite.velocity = cgi.EntityValue(self->def, "velocity")->vec3;
	}

	sprite->sprite.acceleration = cgi.EntityValue(self->def, "acceleration")->vec3;
	sprite->sprite.rotation = cgi.EntityValue(self->def, "rotation")->value;
	sprite->sprite.rotation_velocity = cgi.EntityValue(self->def, "rotation_velocity")->value;
	sprite->sprite.dir = cgi.EntityValue(self->def, "dir")->vec3;

	const cm_entity_t *color = cgi.EntityValue(self->def, "_color");
	if (color->parsed & ENTITY_VEC4) {
		sprite->sprite.color = color->vec4;
	} else if (color->parsed & ENTITY_VEC3) {
		sprite->sprite.color = Vec3_ToVec4(color->vec3, 1.f);
	} else {
		sprite->sprite.color = Vec4(0.f, 0.f, 1.f, 1.f);
	}

	const cm_entity_t *end_color = cgi.EntityValue(self->def, "_end_color");
	if (end_color->parsed & ENTITY_VEC4) {
		sprite->sprite.end_color = end_color->vec4;
	} else if (end_color->parsed & ENTITY_VEC3) {
		sprite->sprite.end_color = Vec3_ToVec4(end_color->vec3, 0.f);
	} else {
		sprite->sprite.end_color = Vec4(0.f, 0.f, 0.f, 0.f);
	}

	sprite->sprite.size = cgi.EntityValue(self->def, "_size")->value ?: 1.f;
	sprite->sprite.size_velocity = cgi.EntityValue(self->def, "size_velocity")->value;
	sprite->sprite.size_acceleration = cgi.EntityValue(self->def, "size_acceleration")->value;
	sprite->sprite.width = cgi.EntityValue(self->def, "width")->value;
	sprite->sprite.height = cgi.EntityValue(self->def, "height")->value;
	sprite->sprite.bounce = cgi.EntityValue(self->def, "bounce")->value;
	sprite->sprite.lifetime = SECONDS_TO_MILLIS(cgi.EntityValue(self->def, "lifetime")->value ?: 10.f);
	sprite->sprite.softness = cgi.EntityValue(self->def, "softness")->value ?: 1.f;
	sprite->sprite.lighting = cgi.EntityValue(self->def, "lighting")->value ?: 1.f;
}

/**
 * @brief
 */
static void Cg_misc_sprite_Think(cg_entity_t *self) {

	const cg_misc_sprite_t *this = self->data, *that = self->data;

	const cg_entity_t *teammate = Cg_EntityForDefinition(self->team);
	if (teammate) {
		if (!g_strcmp0(self->clazz->class_name, teammate->clazz->class_name)) {
			that = teammate->data;
		} else {
			cgi.Warn("Teammate is not %s\n", self->clazz->class_name);
		}
	}

	if (this->sprite.media) {
		for (int32_t i = 0; i < this->count; i++) {

			cg_sprite_t s = this->sprite;

			if (Randomi() & 1) {
				s.media = that->sprite.media;
			}

			s.origin = Vec3_Mix(this->sprite.origin, that->sprite.origin, Randomf());
			s.velocity = Vec3_Mix(this->sprite.origin, that->sprite.origin, Randomf());
			s.acceleration = Vec3_Mix(this->sprite.acceleration, that->sprite.acceleration, Randomf());
			s.rotation = Mixf(this->sprite.rotation, that->sprite.rotation, Randomf());
			s.rotation_velocity = Mixf(this->sprite.rotation_velocity, that->sprite.rotation_velocity, Randomf());
			s.dir = Vec3_Mix(this->sprite.dir, that->sprite.dir, Randomf());
			s.color = Vec4_Mix(this->sprite.color, that->sprite.color, Randomf());
			s.end_color = Vec4_Mix(this->sprite.end_color, that->sprite.end_color, Randomf());
			s.size = Mixf(this->sprite.size, that->sprite.size, Randomf());
			s.width = Mixf(this->sprite.width, that->sprite.width, Randomf());
			s.height = Mixf(this->sprite.height, that->sprite.height, Randomf());
			s.size_velocity = Mixf(this->sprite.size_velocity, that->sprite.size_velocity, Randomf());
			s.size_acceleration = Mixf(this->sprite.size_acceleration, that->sprite.size_acceleration, Randomf());
			s.bounce = Mixf(this->sprite.bounce, that->sprite.bounce, Randomf());
			s.softness = Mixf(this->sprite.softness, that->sprite.softness, Randomf());
			s.lighting = Mixf(this->sprite.lighting, that->sprite.lighting, Randomf());

			Cg_AddSprite(&s);
		}
	}
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
	vec3_t velocity;
	float size;
	int32_t count;
} cg_misc_steam_t;

/**
 * @brief
 */
static void Cg_misc_steam_Init(cg_entity_t *self) {

	cg_misc_steam_t *steam = self->data;

	steam->hz = cgi.EntityValue(self->def, "hz")->value ?: .3f;
	steam->drift = cgi.EntityValue(self->def, "drift")->value ?: .01f;

	if (self->target) {
		const vec3_t target_origin = cgi.EntityValue(self->target, "origin")->vec3;
		steam->velocity = Vec3_Subtract(target_origin, self->origin);
	} else {
		const cm_entity_t *velocity = cgi.EntityValue(self->def, "velocity");
		if (velocity->parsed & ENTITY_VEC3) {
			steam->velocity = velocity->vec3;
		} else {
			steam->velocity = Vec3(0.f, 0.f, 32.f);
		}
	}

	steam->size = cgi.EntityValue(self->def, "size")->value ?: 1.f;
	steam->count = cgi.EntityValue(self->def, "count")->integer ?: 1;
}

/**
 * @brief
 */
static void Cg_misc_steam_Think(cg_entity_t *self) {

	const cg_misc_steam_t *steam = self->data;

	const vec3_t end = Vec3_Add(self->origin, steam->velocity);

	if (cgi.PointContents(self->origin) & CONTENTS_MASK_LIQUID) {
		Cg_BubbleTrail(NULL, self->origin, end);
		return;
	}

	for (int32_t i = 0; i < steam->count; i++) {
		if (!Cg_AddSprite(&(cg_sprite_t) {
			.atlas_image = cg_sprite_steam,
			.origin = self->origin,
			.velocity = Vec3_Add(steam->velocity, Vec3_RandomRange(-2.f, 2.f)),
			.acceleration = Vec3_Add(Vec3_Scale(Vec3_Up(), 20.f), Vec3_RandomDir()),
			.lifetime = 4500 / (5.f + Randomf() * .5f),
			.size = steam->size,
			.size_velocity = 10.f,
			.color = Vec4(0.f, 0.f, 1.f, .5f),
			.lighting = 1.f,
			.softness = 1.f,
		})) {
			break;
		};
	}

	Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
		.sample = cg_sample_steam,
		.origin = self->origin,
		.atten = SOUND_ATTEN_CUBIC,
		.flags = S_PLAY_AMBIENT | S_PLAY_LOOP | S_PLAY_FRAME,
		.entity = self->id,
	});
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
