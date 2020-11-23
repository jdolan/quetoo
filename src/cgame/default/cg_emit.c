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
 * @file Emits are client-sided entities for emitting lights, sounds, models,
 * sprites, etc. They are run once per frame, and culled by both PHS and PVS,
 * depending on their flags.
 */

typedef struct cg_emit_s cg_emit_t;

typedef void (*EmitSpawn)(cg_emit_t *self);
typedef void (*EmitThink)(cg_emit_t *self);

/**
 * @brief The emitter class.
 */
typedef struct {
	/**
	 * @brief The entity class name.
	 */
	const char *class_name;

	/**
	 * @brief The spawn function, called once per level.
	 */
	EmitSpawn Spawn;

	/**
	 * @brief The think function, called once per frame.
	 */
	EmitThink Think;

} cg_emit_class_t;

/**
 * @brief The emitter type.
 */
struct cg_emit_s {

	/**
	 * @brief The emitter class.
	 */
	const cg_emit_class_t *clazz;

	/**
	 * @brief The backing entity definition.
	 */
	const cm_entity_t *ent;

	/**
	 * @brief The emitter origin.
	 */
	vec3_t origin;

	/**
	 * @brief For PVS and PHS culling.
	 */
	const r_bsp_leaf_t *leaf;

	/**
	 * @brief The emitter frequency and randomness.
	 */
	float hz, drift;

	/**
	 * @brief Timestamp for next emission.
	 */
	uint32_t next_think;

	/**
	 * @brief Specialized emitter types.
	 */
	union {
		/**
		 * @brief Flame emission.
		 */
		struct {
			/**
			 * @brief The flame radius.
			 */
			float radius;
		} flame;

		/**
		 * @brief Sparks emission.
		 */
		struct {
			/**
			 * @brief The sparks direction.
			 */
			vec3_t dir;

			/**
			 * @brief The sparks count per emission.
			 */
			int32_t count;
		} sparks;

		/**
		 * @brief Steam emission.
		 */
		struct {
			/**
			 * @brief The steam velocity.
			 */
			vec3_t velocity;

			/**
			 * @brief The steam density.
			 */
			int32_t count;
		} steam;

		/**
		 * @brief Light emission.
		 */
		cg_light_t light;

		/**
		 * @brief Model emission.
		 */
		r_entity_t model;

		/**
		 * @brief Sound emission.
		 */
		s_play_sample_t sound;

		/**
		 * @brief Sprite emission.
		 */
		cg_sprite_t sprite;
	};
};

static GArray *cg_emits;

/**
 * @return The target entity for the specified emitter, if any.
 */
static const cm_entity_t *Cg_EmitTarget(const cg_emit_t *self) {

	if (cgi.EntityValue(self->ent, "target")->parsed & ENTITY_STRING) {
		const char *target_name = cgi.EntityValue(self->ent, "target")->string;

		const cm_bsp_t *bsp = cgi.WorldModel()->bsp->cm;
		for (size_t i = 0; i < bsp->num_entities; i++) {

			const cm_entity_t *ent = bsp->entities[i];
			if (!g_strcmp0(target_name, cgi.EntityValue(ent, "targetname")->string)) {
				return ent;
			}
		}

		const char *class_name = cgi.EntityValue(self->ent, "classname")->string;
		cgi.Warn("Target not found for %s @ %s\n", class_name, vtos(self->origin));
	}

	return NULL;
}

/**
 * @brief
 */
static void Cg_misc_flame(cg_emit_t *self) {

	self->hz = self->hz ?: 5.f;
	self->drift = self->drift ?: .1f;
}

/**
 * @brief
 */
static void Cg_misc_flame_Think(cg_emit_t *self) {

	for (int32_t i = 0; i < self->flame.radius / 8.f; i++) {
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
		.radius = self->flame.radius,
		.color = Vec3(1.f, 0.8f, 0.4f),
		.decay = 32
	});

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_fire,
		.origin = self->origin,
		.attenuation = ATTEN_STATIC,
		.flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
static void Cg_misc_light(cg_emit_t *self) {

	self->light.origin = self->origin;
	self->light.radius = cgi.EntityValue(self->ent, "light")->value ?: DEFAULT_LIGHT;

	const cm_entity_t *color = cgi.EntityValue(self->ent, "_color");
	if (color->parsed & ENTITY_VEC3) {
		self->light.color = color->vec3;
	} else {
		self->light.color = Vec3(1.f, 1.f, 1.f);
	}

	self->light.intensity = cgi.EntityValue(self->ent, "intensity")->value;
}

/**
 * @brief
 */
static void Cg_misc_light_Think(cg_emit_t *self) {

	if (cgi.LeafVisible(self->leaf)) {
		Cg_AddLight(&self->light);
	}
}

/**
 * @brief
 */
static void Cg_misc_model(cg_emit_t *self) {

	self->model.origin = self->origin;
	self->model.angles = cgi.EntityValue(self->ent, "angles")->vec3;
	self->model.scale = cgi.EntityValue(self->ent, "scale")->value ?: 1.f;
	self->model.lerp = 1.f;
}

/**
 * @brief
 */
static void Cg_misc_model_Think(cg_emit_t *self) {

	if (cgi.LeafVisible(self->leaf)) {
		cgi.AddEntity(&self->model);
	}
}

/**
 * @brief
 */
static void Cg_misc_sound(cg_emit_t *self) {

	self->sound.sample = cgi.LoadSample(cgi.EntityValue(self->ent, "sound")->string);

	if (cgi.EntityValue(self->ent, "atten")->parsed & ENTITY_INTEGER) {
		self->sound.attenuation = cgi.EntityValue(self->ent, "atten")->integer;
	} else {
		self->sound.attenuation = ATTEN_IDLE;
	}

	self->sound.flags = S_PLAY_AMBIENT;

	if (self->sound.attenuation != ATTEN_NONE) {
		self->sound.flags |= S_PLAY_POSITIONED;
	}

	if (self->hz == 0.f) {
		self->sound.flags |= S_PLAY_LOOP | S_PLAY_FRAME;
	}
}

/**
 * @brief
 */
static void Cg_misc_sound_Think(cg_emit_t *self) {

	if (cgi.LeafHearable(self->leaf)) {
		cgi.AddSample(&self->sound);
	}
}

/**
 * @brief
 */
static void Cg_misc_sparks(cg_emit_t *self) {

	self->hz = cgi.EntityValue(self->ent, "hz")->value ?: .5f;
	self->drift = cgi.EntityValue(self->ent, "drift")->value ?: 3.f;

	const cm_entity_t *target = Cg_EmitTarget(self);
	if (target) {
		const vec3_t target_origin = cgi.EntityValue(target, "origin")->vec3;
		self->sparks.dir = Vec3_Normalize(Vec3_Subtract(target_origin, self->origin));
	} else {
		if (cgi.EntityValue(self->ent, "_angle")->parsed & ENTITY_INTEGER) {
			const int32_t angle = cgi.EntityValue(self->ent, "_angle")->integer;
			if (angle == -1.f) {
				self->sparks.dir = Vec3_Up();
			} else if (angle == -2.f) {
				self->sparks.dir = Vec3_Down();
			} else {
				Vec3_Vectors(Vec3(0.f, angle, 0.f), &self->sparks.dir, NULL, NULL);
			}
		} else {
			self->sparks.dir = Vec3_Up();
		}
	}

	self->sparks.count = cgi.EntityValue(self->ent, "count")->integer;
}

/**
 * @brief
 */
static void Cg_misc_sparks_Think(cg_emit_t *self) {

	Cg_SparksEffect(self->origin, self->sparks.dir, self->sparks.count);
}

/**
 * @brief
 */
static void Cg_misc_sprite(cg_emit_t *self) {

	const cm_entity_t *target = Cg_EmitTarget(self);
	if (target) {
		const vec3_t target_origin = cgi.EntityValue(target, "origin")->vec3;
		self->sprite.velocity = Vec3_Subtract(target_origin, self->origin);
	} else {
		self->sprite.velocity = cgi.EntityValue(self->ent, "velocity")->vec3;
	}

	self->sprite.acceleration = cgi.EntityValue(self->ent, "acceleration")->vec3;
	self->sprite.rotation = cgi.EntityValue(self->ent, "rotation")->value;
	self->sprite.rotation_velocity = cgi.EntityValue(self->ent, "rotation_velocity")->value;
	self->sprite.dir = Vec3_Normalize(cgi.EntityValue(self->ent, "dir")->vec3);

	const cm_entity_t *color = cgi.EntityValue(self->ent, "_color");
	if (color->parsed & ENTITY_VEC4) {
		self->sprite.color = color->vec4;
	} else if (color->parsed & ENTITY_VEC3) {
		self->sprite.color = Vec3_ToVec4(color->vec3, 1.f);
	}

	const cm_entity_t *end_color = cgi.EntityValue(self->ent, "_end_color");
	if (end_color->parsed & ENTITY_VEC4) {
		self->sprite.end_color = end_color->vec4;
	} else if (end_color->parsed & ENTITY_VEC3) {
		self->sprite.end_color = Vec3_ToVec4(end_color->vec3, 1.f);
	}

	self->sprite.size = cgi.EntityValue(self->ent, "size")->value ?: 1.f;
	self->sprite.size_velocity = cgi.EntityValue(self->ent, "size")->value;
	self->sprite.size_acceleration = cgi.EntityValue(self->ent, "size_acceleration")->value;
	self->sprite.bounce = cgi.EntityValue(self->ent, "bounce")->value;
}

/**
 * @brief
 */
static void Cg_misc_sprite_Think(cg_emit_t *self) {

	if (cgi.LeafVisible(self->leaf)) {
		Cg_AddSprite(&self->sprite);
	}
}

/**
 * @brief
 */
static void Cg_misc_steam(cg_emit_t *self) {

	self->hz = cgi.EntityValue(self->ent, "hz")->value ?: .3f;
	self->drift = cgi.EntityValue(self->ent, "drift")->value ?: .01f;
}

/**
 * @brief
 */
static void Cg_misc_steam_Think(cg_emit_t *self) {
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
		.attenuation = ATTEN_STATIC,
		.flags = S_PLAY_POSITIONED
	});*/
}

/**
 * @brief Loads emitters from the current level.
 * @remarks This should be called once per map load, after the precache routine.
 */
void Cg_LoadEmits(void) {
	const cg_emit_class_t classes[] = {
		{ "misc_flame", Cg_misc_flame, Cg_misc_flame_Think },
		{ "misc_light", Cg_misc_light, Cg_misc_light_Think },
		{ "misc_model", Cg_misc_model, Cg_misc_model_Think },
		{ "misc_sound", Cg_misc_sound, Cg_misc_sound_Think },
		{ "misc_sparks", Cg_misc_sparks, Cg_misc_sparks_Think },
		{ "misc_sprite", Cg_misc_sprite, Cg_misc_sprite_Think },
		{ "misc_steam", Cg_misc_steam, Cg_misc_steam_Think }
	};

	Cg_FreeEmits();

	cg_emits = g_array_new(false, false, sizeof(cg_emit_t));

	const cm_bsp_t *bsp = cgi.WorldModel()->bsp->cm;
	for (size_t i = 0; i < bsp->num_entities; i++) {

		const cm_entity_t *entity = bsp->entities[i];
		const char *class_name = cgi.EntityValue(entity, "classname")->string;

		const cg_emit_class_t *clazz = classes;
		for (size_t j = 0; j < lengthof(classes); j++, clazz++) {
			if (!g_strcmp0(class_name, clazz->class_name)) {

				cg_emit_t e = {};

				e.clazz = clazz;
				e.ent = entity;
				e.origin = cgi.EntityValue(entity, "origin")->vec3;
				e.leaf = cgi.LeafForPoint(e.origin);

				e.clazz->Spawn(&e);

				g_array_append_val(cg_emits, e);
			}
		}
	}
}

/**
 * @brief
 */
void Cg_FreeEmits(void) {

	if (cg_emits) {
		g_array_free(cg_emits, true);
		cg_emits = NULL;
	}
}

/**
 * @brief
 */
void Cg_AddEmits(void) {

	if (!cg_add_emits->value) {
		return;
	}

	cg_emit_t *e = (cg_emit_t *) cg_emits->data;
	for (guint i = 0; i < cg_emits->len; i++, e++) {

		if (e->next_think > cgi.client->unclamped_time) {
			continue;
		}

		e->clazz->Think(e);

		e->next_think += 1000.f / e->hz + 1000.f * e->drift * Randomf();
	}
}
