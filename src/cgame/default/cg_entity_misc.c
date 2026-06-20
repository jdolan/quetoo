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
 * @brief The `misc_dist` type.
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

  /**
   * @brief The count of sprite origins.
   */
  int32_t num_origins;

  /**
   * @brief The dust density.
   */
  float density;

  /**
   * @brief Per-particle random acceleration range, applied symmetrically on each axis.
   */
  vec3_t acceleration_spread;

  /**
   * @brief Per-particle random initial rotation range normalized to [0, 1], where 1 is pi radians.
   */
  float rotation_spread;

  /**
   * @brief Per-particle random velocity range, applied symmetrically on each axis.
   */
  vec3_t velocity_spread;

  /**
   * @brief Per-particle random size range as a +/- fraction of base size.
   */
  float size_spread;

  /**
   * @brief The count of active sprites.
   */
  int32_t num_active;

  /**
   * @brief The last client time this emitter was visible.
   */
  uint32_t last_visible;
} cg_dust_t;

/**
 * @brief Initializes a `misc_dust` entity by loading its sprite and computing spawn origins from brushes.
 */
static const char *cg_dust_preset_default =
  "\\sprite\\particle"
  "\\color\\1 1 1"
  "\\size\\1"
  "\\velocity_spread\\1 1 1"
  "\\size_spread\\.1"
  "\\lifetime\\10"
  "\\lighting\\1"
  "\\density\\1"
  "\\hz\\10";

static const char *cg_dust_preset_embers =
  "\\sprite\\particle2"
  "\\velocity\\0 0 5"
  "\\acceleration\\8 8 8"
  "\\acceleration_spread\\8 12 8"
  "\\color\\1 .25 .0125"
  "\\end_color\\.125 0 0"
  "\\size\\2.25"
  "\\size_velocity\\-.25"
  "\\lifetime\\2"
  "\\lighting\\0"
  "\\density\\.33"
  "\\hz\\10";

static const char *cg_dust_preset_bubbles =
  "\\sprite\\bubble"
  "\\velocity\\0 0 32"
  "\\acceleration\\.33 .33 .33"
  "\\color\\1 1 1"
  "\\size\\0.1"
  "\\size_velocity\\1"
  "\\lifetime\\1"
  "\\lighting\\1"
  "\\density\\12"
  "\\hz\\10";

static const char *cg_dust_preset_fizz =
  "\\velocity\\0 0 4"
  "\\acceleration_spread\\4 4 2"
  "\\rotation_spread\\1"
  "\\color\\1 1 1"
  "\\end_color\\.6 .6 .6"
  "\\size\\2.5"
  "\\size_spread\\1"
  "\\lifetime\\.666"
  "\\lighting\\2"
  "\\density\\1"
  "\\hz\\10";

static const char *cg_dust_preset_flame =
  "\\velocity\\0 0 5"
  "\\acceleration\\0 0 120"
  "\\acceleration_spread\\4 4 0"
  "\\color\\1 .45 .05"
  "\\end_color\\.6 .05 0"
  "\\size\\1.5"
  "\\size_velocity\\14"
  "\\size_acceleration\\80"
  "\\lifetime\\.85"
  "\\lighting\\0"
  "\\density\\.5"
  "\\hz\\10";

static const char *cg_dust_preset_steam =
  "\\velocity\\0 0 32"
  "\\acceleration\\0 0 20"
  "\\acceleration_spread\\1 1 0"
  "\\color\\.5 .5 .5"
  "\\size\\20"
  "\\size_velocity\\10"
  "\\lifetime\\1.2"
  "\\lighting\\.5"
  "\\density\\.2"
  "\\hz\\8";

static void Cg_misc_dust_Init(cg_entity_t *self) {

  cg_dust_t *dust = self->data;

  const char *type = cgi.EntityValue(self->def, "type")->nullable_string;

  const char *preset_str = cg_dust_preset_default;
  if (!q_strcmp(type, "embers")) {
    preset_str = cg_dust_preset_embers;
  } else if (!q_strcmp(type, "bubbles")) {
    preset_str = cg_dust_preset_bubbles;
  } else if (!q_strcmp(type, "fizz")) {
    preset_str = cg_dust_preset_fizz;
  } else if (!q_strcmp(type, "flame")) {
    preset_str = cg_dust_preset_flame;
  } else if (!q_strcmp(type, "steam")) {
    preset_str = cg_dust_preset_steam;
  }

  cm_entity_t *preset = cgi.EntityFromInfoString(preset_str);
  cm_entity_t *def = cgi.EntityAssign(self->def, preset);
  cgi.FreeEntity(preset);

  if (!q_strcmp(type, "fizz")) {
    dust->sprite.animation = cg_sprite_fizz_01;
  } else if (!q_strcmp(type, "flame")) {
    dust->sprite.atlas_image = cg_sprite_flame;
  } else if (!q_strcmp(type, "steam")) {
    dust->sprite.atlas_image = cg_sprite_steam;
  } else {
    const char *name = cgi.EntityValue(def, "sprite")->nullable_string ?: "particle";
    dust->sprite.image = cgi.LoadImage(va("sprites/%s", name), IMG_SPRITE);
    if (dust->sprite.image == NULL) {
      dust->sprite.image = cgi.LoadImage("sprites/particle", IMG_SPRITE);
      Cg_Warn("%s @ %s failed to load sprite %s\n", self->clazz->classname, vtos(self->origin), name);
    }
  }

  dust->sprite.velocity = cgi.EntityValue(def, "velocity")->vec3;
  dust->sprite.acceleration = cgi.EntityValue(def, "acceleration")->vec3;
  dust->sprite.rotation = cgi.EntityValue(def, "rotation")->value;
  dust->sprite.rotation_velocity = cgi.EntityValue(def, "rotation_velocity")->value;
  dust->sprite.dir = cgi.EntityValue(def, "dir")->vec3;
  dust->sprite.color = cgi.EntityValue(def, "color")->vec3;
  dust->sprite.end_color = cgi.EntityValue(def, "end_color")->vec3;
  dust->sprite.size = cgi.EntityValue(def, "size")->value;
  dust->sprite.size_velocity = cgi.EntityValue(def, "size_velocity")->value;
  dust->sprite.size_acceleration = cgi.EntityValue(def, "size_acceleration")->value;
  dust->sprite.width = cgi.EntityValue(def, "width")->value;
  dust->sprite.height = cgi.EntityValue(def, "height")->value;
  dust->sprite.bounce = cgi.EntityValue(def, "bounce")->value;
  dust->sprite.lifetime = SECONDS_TO_MILLIS(cgi.EntityValue(def, "lifetime")->value);
  dust->sprite.lighting = cgi.EntityValue(def, "lighting")->value;

  dust->density = cgi.EntityValue(def, "density")->value;

  const cm_entity_t *size_spread = cgi.EntityValue(def, "size_spread");
  dust->size_spread = (size_spread->parsed & ENTITY_FLOAT) ? size_spread->value : .1f;
  dust->velocity_spread = cgi.EntityValue(def, "velocity_spread")->vec3;
  dust->acceleration_spread = cgi.EntityValue(def, "acceleration_spread")->vec3;
  dust->rotation_spread = Clampf01(cgi.EntityValue(def, "rotation_spread")->value);

  self->hz = cgi.EntityValue(def, "hz")->value;

  cgi.FreeEntity(def);

  self->bounds = Box3_Null();

  const cm_bsp_t *bsp = cgi.WorldModel()->bsp->cm;
  const cm_entity_t *brush_def = self->id < bsp->num_entities ? bsp->entities[self->id] : self->def;
  Vector *brushes = cgi.EntityBrushes(brush_def);
  for (size_t i = 0; i < brushes->count; i++) {

    const cm_bsp_brush_t *brush = *VectorElement(brushes, cm_bsp_brush_t *, i);
    self->bounds = Box3_Union(self->bounds, brush->bounds);

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

  release(brushes);
}

/**
 * @brief Edit callback that purges all live sprites referencing this entity's data.
 */
static void Cg_misc_dust_Free(cg_entity_t *self) {
  Cg_FreeSpritesByData(self->data);
  ((cg_dust_t *)self->data)->last_visible = 0;
}

/**
 * @brief Think callback that fades dust sprites in at birth and decrements the active count at death.
 */
static void Cg_misc_dust_SpriteThink(cg_sprite_t *sprite, float life, float delta) {

  cg_dust_t *dust = sprite->data;

  if (life <= .1f) {
    sprite->color = Vec3_Scale(dust->sprite.color, life / .1f);
  }

  if (life >= 1.f) {
    dust->num_active--;
  }
}

/**
 * @brief Spawns one dust sprite and optionally backdates it for visibility catch-up.
 */
static cg_sprite_t *Cg_misc_dust_SpawnSprite(cg_dust_t *dust, uint32_t age_msec) {

  cg_sprite_t s = dust->sprite;

  s.origin = dust->origins[RandomRangei(0, dust->num_origins)];
  s.origin = Vec3_Add(s.origin, Vec3_RandomDir());
  const float min_size = Maxf(0.f, s.size * (1.f - dust->size_spread));
  const float max_size = Maxf(min_size, s.size * (1.f + dust->size_spread));
  s.size = RandomRangef(min_size, max_size);
  s.velocity = Vec3_Add(s.velocity, Vec3_RandomRanges(
    -dust->velocity_spread.x, dust->velocity_spread.x,
    -dust->velocity_spread.y, dust->velocity_spread.y,
    -dust->velocity_spread.z, dust->velocity_spread.z));
  s.rotation += RandomRangef(-(float) M_PI * dust->rotation_spread, (float) M_PI * dust->rotation_spread);
  s.acceleration = Vec3_Add(s.acceleration, Vec3_RandomRanges(
    -dust->acceleration_spread.x, dust->acceleration_spread.x,
    -dust->acceleration_spread.y, dust->acceleration_spread.y,
    -dust->acceleration_spread.z, dust->acceleration_spread.z));
  s.lifetime = RandomRangeu(s.lifetime * .666f, s.lifetime * 1.333f);
  s.Think = Cg_misc_dust_SpriteThink;
  s.data = dust;
  s.flags |= SPRITE_DATA_NOFREE;

  cg_sprite_t *emitted = Cg_AddSprite(&s);
  if (!emitted) {
    return NULL;
  }

  if (age_msec && emitted->lifetime > 1) {
    age_msec = (uint32_t) Minui64(age_msec, emitted->lifetime - 1);
    emitted->time -= age_msec;
    emitted->timestamp -= age_msec;
  }

  dust->num_active++;
  return emitted;
}

/**
 * @brief Emits dust sprites to maintain the active count up to the number of configured origins.
 */
static void Cg_misc_dust_Think(cg_entity_t *self) {

  if (!cg_add_atmospheric->value) {
    return;
  }

  cg_dust_t *dust = self->data;

  const uint32_t now = cgi.client->unclamped_time;
  if (cgi.CulludeBox(cgi.view, self->bounds)) {
    return;
  }

  uint32_t hidden_msec = 0;
  if (dust->last_visible) {
    const uint32_t elapsed = now - dust->last_visible;
    const uint32_t gap_threshold = (uint32_t) Maxui64(cgi.client->frame_msec * 2u, 64u);
    if (elapsed > gap_threshold) {
      hidden_msec = elapsed;
    }
  }

  while (dust->num_active < dust->num_origins) {
    uint32_t age_msec = 0;
    if (hidden_msec) {
      const uint32_t max_lifetime_age = (uint32_t) Maxf(dust->sprite.lifetime, 2.f) - 1;
      const uint32_t max_age = (uint32_t) Minui64(hidden_msec, max_lifetime_age);
      const uint32_t min_age = (uint32_t) Minui64(max_age, max_age / 10);
      age_msec = RandomRangeu(min_age, max_age + 1);
    }

    if (!Cg_misc_dust_SpawnSprite(dust, age_msec)) {
      break;
    }
  }

  dust->last_visible = now;
}

/**
 * @brief The client-side entity class descriptor for the `misc_dust` ambient dust emitter.
 */
const cg_entity_class_t cg_misc_dust = {
  .classname = "misc_dust",
  .Init = Cg_misc_dust_Init,
  .Free = Cg_misc_dust_Free,
  .Think = Cg_misc_dust_Think,
  .data_size = sizeof(cg_dust_t)
};

/**
 * @brief The `misc_flame` type.
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
   * @brief The looping sample to play.
   */
  s_sample_t *sample;
} cg_flame_t;

/**
 * @brief Initializes a `misc_flame` entity by reading radius, density, and sound from the entity definition.
 */
static void Cg_misc_flame_Init(cg_entity_t *self) {

  self->hz = cgi.EntityValue(self->def, "hz")->value ?: 10.f;
  self->drift = cgi.EntityValue(self->def, "drift")->value ?: .1f;

  cg_flame_t *flame = self->data;

  flame->density = cgi.EntityValue(self->def, "density")->value ?: .666f;
  flame->radius = cgi.EntityValue(self->def, "radius")->value ?: 16.f;

  self->bounds = Box3_FromCenterRadius(self->origin, flame->radius * 16.f);

  const char *sound = cgi.EntityValue(self->def, "sound")->nullable_string;
  if (sound) {
    if (strcmp(sound, "none")) {
      flame->sample = cgi.LoadSample(sound);
    }
  } else {
    flame->sample = cg_sample_fire;
  }
}

/**
 * @brief Emits flame sprites for a `misc_flame` entity each frame.
 */
static void Cg_misc_flame_Think(cg_entity_t *self) {

  cg_flame_t *flame = self->data;

  if (cgi.CulludeBox(cgi.view, self->bounds)) {
    return;
  }

  const float r = flame->radius;
  const float s = Clampf(r / 64.f, .125f, 1.f);

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
        .color = ColorHSV(hue, sat, RandomRangef(.7f, 1.f)).vec3,
      })) {
      break;
    }
  }

  // Smoke — rises above the flame column, expanding and drifting upward
  const int32_t num_smoke = (int32_t) Maxf(1.f, flame->radius * flame->density * .15f);
  for (int32_t i = 0; i < num_smoke; i++) {
    r_animation_t *anim = (i & 1) ? cg_sprite_smoke_05 : cg_sprite_smoke_04;
    const vec3_t smoke_origin = {
      .x = self->origin.x + RandomRangef(-r * .3f, r * .3f),
      .y = self->origin.y + RandomRangef(-r * .3f, r * .3f),
      .z = self->origin.z + r * RandomRangef(1.f, 2.5f),
    };
    const float sz = Maxf(4.f, r * RandomRangef(.4f, .8f));
    if (!Cg_AddSprite(&(cg_sprite_t) {
        .animation = anim,
        .origin = smoke_origin,
        .velocity = Vec3(RandomRangef(-8.f, 8.f) * s,
                         RandomRangef(-8.f, 8.f) * s,
                         RandomRangef(20.f, 40.f) * s),
        .rotation = RandomRadian(),
        .rotation_velocity = RandomRangef(-.2f, .2f),
        .lifetime = Cg_AnimationLifetime(anim, RandomRangef(10.f, 20.f)),
        .size = sz,
        .size_velocity = sz * .5f,
        .color = Vec3_Scale(Vec3_One(), RandomRangef(.15f, .65f)),
        .lighting = 1.f,
      })) {
      break;
    }
  }

  if (flame->sample) {
    Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
      .sample = flame->sample,
      .origin = self->origin,
      .flags = S_PLAY_AMBIENT | S_PLAY_LOOP | S_PLAY_FRAME,
      .pitch = RandomRangei(-1, 1),
      .data = self
    });
  }
}

/**
 * @brief The client-side entity class descriptor for the `misc_flame` ambient fire effect.
 */
const cg_entity_class_t cg_misc_flame = {
  .classname = "misc_flame",
  .Init = Cg_misc_flame_Init,
  .Think = Cg_misc_flame_Think,
  .data_size = sizeof(cg_flame_t)
};

/**
 * @brief Initializes a `misc_model` entity by loading its model from the entity definition.
 */
static void Cg_misc_model_Init(cg_entity_t *self) {

  r_entity_t *entity = self->data;

  entity->origin = self->origin;
  entity->angles = cgi.EntityValue(self->def, "angles")->vec3;
  entity->scale = cgi.EntityValue(self->def, "scale")->value ?: 1.f;
  entity->lerp = 1.f;
  entity->color = Vec4_One();

  const cm_entity_t *model = cgi.EntityValue(self->def, "model");
  if (model->parsed & ENTITY_STRING) {
    entity->model = cgi.LoadModel(model->string);
    if (entity->model) {
      entity->bounds = Box3_Scale(entity->model->bounds, entity->scale);
      entity->abs_bounds = Box3_Translate(entity->bounds, entity->origin);
    } else {
      Cg_Warn("%s @ %s: Failed to load %s\n", self->clazz->classname, vtos(self->origin), model->string);
    }
  } else {
    Cg_Warn("%s @ %s has no model specified\n", self->clazz->classname, vtos(self->origin));
  }
}

/**
 * @brief Adds the `misc_model` entity's render entity to the view each frame.
 */
static void Cg_misc_model_Think(cg_entity_t *self) {

  const r_entity_t *entity = self->data;

  if (entity->model) {
    r_entity_t *out = cgi.AddEntity(cgi.view, entity);
    if (out) {
      self->bounds = out->abs_model_bounds;
    }
  }
}

/**
 * @brief The client-side entity class descriptor for the `misc_model` static model renderer.
 */
const cg_entity_class_t cg_misc_model = {
  .classname = "misc_model",
  .Init = Cg_misc_model_Init,
  .Think = Cg_misc_model_Think,
  .data_size = sizeof(r_entity_t)
};

/**
 * @brief The `misc_sound` type.
 */
typedef struct {

  /**
   * @brief The play sample template.
   */
  s_play_sample_t play;
} cg_misc_sound_t;

/**
 * @brief Initializes a `misc_sound` entity by loading its sample and configuring play parameters.
 */
static void Cg_misc_sound_Init(cg_entity_t *self) {

  self->hz = cgi.EntityValue(self->def, "hz")->value ?: 0.f;
  self->drift = cgi.EntityValue(self->def, "drift")->value ?: .3f;

  cg_misc_sound_t *sound = self->data;

  if (cgi.EntityValue(self->def, "sound")->parsed & ENTITY_STRING) {
    sound->play.sample = cgi.LoadSample(cgi.EntityValue(self->def, "sound")->string);
  } else {
    Cg_Warn("%s @ %s has no sound specified\n", self->clazz->classname, vtos(self->origin));
  }

  sound->play.origin = self->origin;
  sound->play.flags = S_PLAY_AMBIENT;

  if (self->hz == 0.f) {
    sound->play.flags |= S_PLAY_LOOP | S_PLAY_FRAME;
  }

  sound->play.data = sound;
}

/**
 * @brief Plays the configured ambient sound sample for a `misc_sound` entity each frame.
 */
static void Cg_misc_sound_Think(cg_entity_t *self) {

  const cg_misc_sound_t *sound = self->data;

  if (sound->play.sample) {
    Cg_AddSample(cgi.stage, &sound->play);
  }
}

/**
 * @brief The client-side entity class descriptor for the `misc_sound` ambient sound emitter.
 */
const cg_entity_class_t cg_misc_sound = {
  .classname = "misc_sound",
  .Init = Cg_misc_sound_Init,
  .Think = Cg_misc_sound_Think,
  .data_size = sizeof(cg_misc_sound_t)
};

/**
 * @brief The `misc_sparks` type.
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
 * @brief Initializes a `misc_sparks` entity by resolving its emission direction and spark count.
 */
static void Cg_misc_sparks_Init(cg_entity_t *self) {

  self->hz = cgi.EntityValue(self->def, "hz")->value ?: .5f;
  self->drift = cgi.EntityValue(self->def, "drift")->value ?: 3.f;

  cg_misc_sparks_t *sparks = self->data;

  if (self->target) {
    const vec3_t target_origin = cgi.EntityValue(self->target, "origin")->vec3;
    sparks->dir = Vec3_Normalize(Vec3_Subtract(target_origin, self->origin));
  } else {
    if (cgi.EntityValue(self->def, "angle")->parsed & ENTITY_INTEGER) {
      const int32_t angle = cgi.EntityValue(self->def, "angle")->integer;
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
 * @brief Emits sparks from the entity's origin using the configured direction and count.
 */
static void Cg_misc_sparks_Think(cg_entity_t *self) {

  const cg_misc_sparks_t *sparks = self->data;

  Cg_SparksEffect(self->origin, sparks->dir, sparks->count);
}

/**
 * @brief The client-side entity class descriptor for the `misc_sparks` spark emitter.
 */
const cg_entity_class_t cg_misc_sparks = {
  .classname = "misc_sparks",
  .Init = Cg_misc_sparks_Init,
  .Think = Cg_misc_sparks_Think,
  .data_size = sizeof(cg_misc_sparks_t)
};

/**
 * @brief The `misc_sprite` type.
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
 * @brief Initializes a `misc_sprite` entity by loading the sprite and reading emission parameters.
 */
static void Cg_misc_sprite_Init(cg_entity_t *self) {

  self->hz = cgi.EntityValue(self->def, "hz")->value ?: .5f;
  self->drift = cgi.EntityValue(self->def, "drift")->value ?: 3.f;

  cg_misc_sprite_t *sprite = self->data;

  sprite->count = cgi.EntityValue(self->def, "count")->integer ?: 1;

  const char *name = cgi.EntityValue(self->def, "sprite")->nullable_string ?: "particle";
  sprite->sprite.image = cgi.LoadImage(va("sprites/%s", name), IMG_SPRITE);
  if (sprite->sprite.image == NULL) {
    sprite->sprite.image = cgi.LoadImage("sprites/particle", IMG_SPRITE);
    Cg_Warn("%s @ %s failed to load sprite %s\n",
         self->clazz->classname,
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

  const cm_entity_t *color = cgi.EntityValue(self->def, "color");
  if (color->parsed & ENTITY_VEC3) {
    sprite->sprite.color = color->vec3;
  } else {
    sprite->sprite.color = Vec3(1.f, 1.f, 1.f);
  }

  const cm_entity_t *end_color = cgi.EntityValue(self->def, "end_color");
  if (end_color->parsed & ENTITY_VEC3) {
    sprite->sprite.end_color = end_color->vec3;
  } else {
    sprite->sprite.end_color = Vec3(0.f, 0.f, 0.f);
  }

  sprite->sprite.size = cgi.EntityValue(self->def, "size")->value ?: 1.f;
  sprite->sprite.size_velocity = cgi.EntityValue(self->def, "size_velocity")->value;
  sprite->sprite.size_acceleration = cgi.EntityValue(self->def, "size_acceleration")->value;
  sprite->sprite.width = cgi.EntityValue(self->def, "width")->value;
  sprite->sprite.height = cgi.EntityValue(self->def, "height")->value;
  sprite->sprite.bounce = cgi.EntityValue(self->def, "bounce")->value;
  sprite->sprite.lifetime = SECONDS_TO_MILLIS(cgi.EntityValue(self->def, "lifetime")->value ?: 10.f);
  sprite->sprite.lighting = cgi.EntityValue(self->def, "lighting")->value ?: 1.f;
}

/**
 * @brief Emits interpolated sprites between the entity and its team counterpart each frame.
 */
static void Cg_misc_sprite_Think(cg_entity_t *self) {

  const cg_misc_sprite_t *this = self->data, *that = self->data;

  const cg_entity_t *teammate = Cg_EntityForDefinition(self->team);
  if (teammate) {
    if (!strcmp(self->clazz->classname, teammate->clazz->classname)) {
      that = teammate->data;
    } else {
      Cg_Warn("Teammate is not %s\n", self->clazz->classname);
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
      s.color = Vec3_Mix(this->sprite.color, that->sprite.color, Randomf());
      s.end_color = Vec3_Mix(this->sprite.end_color, that->sprite.end_color, Randomf());
      s.size = Mixf(this->sprite.size, that->sprite.size, Randomf());
      s.width = Mixf(this->sprite.width, that->sprite.width, Randomf());
      s.height = Mixf(this->sprite.height, that->sprite.height, Randomf());
      s.size_velocity = Mixf(this->sprite.size_velocity, that->sprite.size_velocity, Randomf());
      s.size_acceleration = Mixf(this->sprite.size_acceleration, that->sprite.size_acceleration, Randomf());
      s.bounce = Mixf(this->sprite.bounce, that->sprite.bounce, Randomf());
      s.lighting = Mixf(this->sprite.lighting, that->sprite.lighting, Randomf());

      Cg_AddSprite(&s);
    }
  }
}

/**
 * @brief The client-side entity class descriptor for the `misc_sprite` configurable sprite emitter.
 */
const cg_entity_class_t cg_misc_sprite = {
  .classname = "misc_sprite",
  .Init = Cg_misc_sprite_Init,
  .Think = Cg_misc_sprite_Think,
  .data_size = sizeof(cg_sprite_t)
};

/**
 * @brief Data struct holding velocity, size, count, and sound sample for a `misc_steam` entity.
 */
typedef struct {
  vec3_t velocity;
  float size;
  int32_t count;
  s_sample_t *sample;
} cg_misc_steam_t;

/**
 * @brief Initializes a `misc_steam` entity by resolving velocity, size, count, and optional sound.
 */
static void Cg_misc_steam_Init(cg_entity_t *self) {

  cg_misc_steam_t *steam = self->data;

  self->hz = cgi.EntityValue(self->def, "hz")->value ?: 30.f;
  self->drift = cgi.EntityValue(self->def, "drift")->value ?: .01f;

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

  steam->size = cgi.EntityValue(self->def, "size")->value ?: 32.f;
  steam->count = cgi.EntityValue(self->def, "count")->integer ?: 1;

  const char *sound = cgi.EntityValue(self->def, "sound")->nullable_string;
  if (sound) {
    if (strcmp(sound, "none")) {
      steam->sample = cgi.LoadSample(sound);
    }
  } else {
    steam->sample = cg_sample_steam;
  }
}

/**
 * @brief Emits steam puff sprites or a bubble trail and plays the steam loop sound each frame.
 */
static void Cg_misc_steam_Think(cg_entity_t *self) {

  const cg_misc_steam_t *steam = self->data;

  const vec3_t end = Vec3_Add(self->origin, steam->velocity);

  if (cgi.PointContents(end) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(NULL, self->origin, end, 2.f);
    return;
  }

  for (int32_t i = 0; i < steam->count; i++) {
    if (!Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_steam,
      .origin = self->origin,
      .velocity = Vec3_Add(steam->velocity, Vec3_RandomRange(-2.f, 2.f)),
      .acceleration = Vec3_Add(Vec3_Scale(Vec3_Up(), 20.f), Vec3_RandomDir()),
      .lifetime = 6500 / (5.f + Randomf() * .5f),
      .rotation = Randomf(),
      .rotation_velocity = RandomRangef(-1.f, 1.f),
      .size = RandomRangef(.9f * steam->size, 1.1f * steam->size),
      .size_velocity = 10.f,
      .color = Vec3(.25f, .25f, .25f),
      .lighting = 0.5f,
    })) {
      break;
    };
  }

  if (steam->sample) {
    Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
      .sample = steam->sample,
      .origin = self->origin,
      .flags = S_PLAY_AMBIENT | S_PLAY_LOOP | S_PLAY_FRAME,
      .data = self,
    });
  }
}

/**
 * @brief The client-side entity class descriptor for the `misc_steam` steam vent emitter.
 */
const cg_entity_class_t cg_misc_steam = {
  .classname = "misc_steam",
  .Init = Cg_misc_steam_Init,
  .Think = Cg_misc_steam_Think,
  .data_size = sizeof(s_play_sample_t)
};

/**
 * @brief The `misc_weather` type.
 */
typedef struct {

  /**
   * @brief The weather type bitmask.
   */
  int32_t weather;

  /**
   * @brief The sprite origins (xyz) and fall heights (w).
   */
  vec4_t *origins;

  /**
   * @brief The count of sprite origins.
   */
  int32_t num_origins;

  /**
   * @brief The density multiplier.
   */
  float density;

  /**
   * @brief The count of active sprites.
   */
  int32_t num_active;

  /**
   * @brief The ambient sound sample.
   */
  s_sample_t *sample;

  /**
   * @brief The last client time this emitter was visible.
   */
  uint32_t last_visible;
} cg_weather_t;

/**
 * @brief Initializes a `misc_weather` entity by reading weather type, sound, and brush spawn origins.
 */
static void Cg_misc_weather_Init(cg_entity_t *self) {

  cg_weather_t *weather = self->data;

  const char *type = cgi.EntityValue(self->def, "weather")->nullable_string;
  if (type) {
    if (strstr(type, "rain")) {
      weather->weather |= WEATHER_RAIN;
    }
    if (strstr(type, "snow")) {
      weather->weather |= WEATHER_SNOW;
    }
    if (strstr(type, "ash")) {
      weather->weather |= WEATHER_ASH;
    }
  }

  if (!weather->weather) {
    weather->weather = WEATHER_RAIN;
  }

  const char *sound = cgi.EntityValue(self->def, "sound")->nullable_string;
  if (sound) {
    if (strcmp(sound, "none")) {
      weather->sample = cgi.LoadSample(sound);
    }
  } else {
    if (weather->weather & WEATHER_RAIN) {
      weather->sample = cg_sample_rain;
    } else if (weather->weather & WEATHER_SNOW) {
      weather->sample = cg_sample_snow;
    } else if (weather->weather & WEATHER_ASH) {
      weather->sample = cg_sample_ash;
    }
  }

  weather->density = cgi.EntityValue(self->def, "density")->value ?: 1.f;

  if (weather->weather & WEATHER_RAIN) {
    self->hz = cgi.EntityValue(self->def, "hz")->value ?: 30.f;
  } else {
    self->hz = cgi.EntityValue(self->def, "hz")->value ?: 10.f;
  }

  self->bounds = Box3_Null();

  const cm_bsp_t *bsp = cgi.WorldModel()->bsp->cm;
  const cm_entity_t *brush_def = self->id < bsp->num_entities ? bsp->entities[self->id] : self->def;
  Vector *brushes = cgi.EntityBrushes(brush_def);
  for (size_t i = 0; i < brushes->count; i++) {

    const cm_bsp_brush_t *brush = *VectorElement(brushes, cm_bsp_brush_t *, i);
    self->bounds = Box3_Union(self->bounds, brush->bounds);

    const vec3_t brush_size = Box3_Size(brush->bounds);

    // Grid spacing derived from XY footprint and density, independent of brush height
    const float spacing = powf(brush_size.x * brush_size.y, 0.25f) / sqrtf(weather->density);
    const int32_t num_cols = Maxi((int32_t) (brush_size.x / spacing), 1);
    const int32_t num_rows = Maxi((int32_t) (brush_size.y / spacing), 1);
    const float dx = brush_size.x / num_cols;
    const float dy = brush_size.y / num_rows;

    if (weather->origins) {
      weather->origins = cgi.Realloc(weather->origins, (weather->num_origins + num_cols * num_rows) * sizeof(vec4_t));
    } else {
      weather->origins = cgi.LinkMalloc(num_cols * num_rows * sizeof(vec4_t), weather);
    }

    for (int32_t row = 0; row < num_rows; row++) {
      for (int32_t col = 0; col < num_cols; col++) {

        const float x = brush->bounds.mins.x + (col + 0.5f) * dx;
        const float y = brush->bounds.mins.y + (row + 0.5f) * dy;

        for (float z = brush->bounds.maxs.z; z >= brush->bounds.mins.z; z -= 1.f) {
          if (cgi.PointInsideBrush(Vec3(x, y, z), brush)) {
            weather->origins[weather->num_origins++] = Vec4(x, y, z, 0.f);
            break;
          }
        }
      }
    }
  }

  release(brushes);
}

/**
 * @brief Think callback that decrements the active weather sprite count when a sprite expires.
 */
static void Cg_misc_weather_SpriteThink(cg_sprite_t *sprite, float life, float delta) {

  cg_weather_t *weather = sprite->data;

  if (life >= 1.f) {
    weather->num_active--;
  }
}

/**
 * @brief Spawns one weather sprite and optionally backdates it for visibility catch-up.
 */
static cg_sprite_t *Cg_misc_weather_SpawnSprite(cg_entity_t *self, cg_weather_t *weather, uint32_t age_msec) {

  const int32_t index = RandomRangei(0, weather->num_origins);
  vec4_t *origin = &weather->origins[index];
  vec3_t pos = Vec3(origin->x, origin->y, origin->z);

  if (origin->w == 0.f) {
    const vec3_t end = Vec3(pos.x, pos.y, pos.z - MAX_WORLD_AXIAL);
    const cm_trace_t trace = cgi.Trace(pos, end, Box3_Zero(), NULL, CONTENTS_SOLID);
    origin->w = pos.z - trace.end.z;
    self->bounds = Box3_Append(self->bounds, trace.end);
  }

  float height = origin->w;

  // Distribute spawn origins randomly along the vertical axis to avoid "sheet" effect.
  const float vertical_offset = Randomf() * origin->w;
  pos.z -= vertical_offset;
  height = origin->w - vertical_offset;

  cg_sprite_t s = {
    .origin = pos,
    .Think = Cg_misc_weather_SpriteThink,
    .data = weather,
    .flags = SPRITE_DATA_NOFREE,
  };

  if (weather->weather & WEATHER_RAIN) {
    s.atlas_image = cg_sprite_rain;
    s.color = Vec3(1.f, 1.f, 1.f);
    s.size = 32.f;
    s.velocity = Vec3_Subtract(Vec3_RandomRange(-2.f, 2.f), Vec3(0.f, 0.f, 800.f));
    s.axis = SPRITE_AXIS_X | SPRITE_AXIS_Y;
    s.lifetime = 1000.f * Maxf(height - s.size, 0.f) / 800.f * RandomRangef(.8f, 1.2f);
    s.lighting = 1.f;

    // Suppress splash bursts for catch-up sprites; they should appear already in-flight.
    if (!age_msec && Randomf() > .8f) {
      Cg_AddSprite(&(cg_sprite_t) {
        .atlas_image = cg_sprite_water_ring,
        .lifetime = 300,
        .origin = Vec3(pos.x, pos.y, pos.z - height + 2.f),
        .size = 4.f,
        .size_velocity = 4.f * 6.f,
        .rotation = RandomRadian(),
        .dir = Vec3_Up(),
        .color = Vec3(1.f, 1.f, 1.f),
        .lighting = 1.f
      });
    }
  } else if (weather->weather & WEATHER_SNOW) {
    s.atlas_image = cg_sprite_snow;
    s.color = Vec3(1.f, 1.f, 1.f);
    s.size = 4.f;
    s.velocity = Vec3_Subtract(Vec3_RandomRange(-12.f, 12.f), Vec3(0.f, 0.f, 120.f));
    s.acceleration = Vec3_RandomRange(-12.f, 12.f);
    s.lifetime = 1000.f * height / 120.f * RandomRangef(.8f, 1.2f);
  } else if (weather->weather & WEATHER_ASH) {
    const float color = RandomRangef(0.25f, 0.75f);
    s.atlas_image = cg_sprite_ash;
    s.color = Vec3(color, color, color);
    s.size = RandomRangef(1.f, 3.f);
    s.velocity = Vec3_Subtract(Vec3_RandomRange(-12.f, 12.f), Vec3(0.f, 0.f, 25.f));
    s.acceleration = Vec3_RandomRange(-12.f, 12.f);
    s.rotation = RandomRadian();
    s.lifetime = 1000.f * height / 25.f * RandomRangef(.8f, 1.2f);
  }

  cg_sprite_t *emitted = Cg_AddSprite(&s);
  if (!emitted) {
    return NULL;
  }

  if (age_msec && emitted->lifetime > 1) {
    age_msec = (uint32_t) Minui64(age_msec, emitted->lifetime - 1);
    emitted->time -= age_msec;
    emitted->timestamp -= age_msec;
  }

  weather->num_active++;
  return emitted;
}

/**
 * @brief Edit callback that purges all live sprites referencing this entity's data.
 */
static void Cg_misc_weather_Free(cg_entity_t *self) {
  Cg_FreeSpritesByData(self->data);
  ((cg_weather_t *)self->data)->last_visible = 0;
}

static void Cg_misc_weather_Think(cg_entity_t *self) {

  if (!cg_add_weather->value) {
    return;
  }

  cg_weather_t *weather = self->data;

  const uint32_t now = cgi.client->unclamped_time;
  if (cgi.CulludeBox(cgi.view, self->bounds)) {
    return;
  }

  if (weather->sample) {
    Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
      .sample = weather->sample,
      .flags = S_PLAY_AMBIENT | S_PLAY_LOOP | S_PLAY_FRAME,
      .entity = Cg_Self()
    });
  }

  uint32_t hidden_msec = 0;
  if (weather->last_visible) {
    const uint32_t elapsed = now - weather->last_visible;
    const uint32_t gap_threshold = (uint32_t) Maxui64(cgi.client->frame_msec * 2u, 64u);
    if (elapsed > gap_threshold) {
      hidden_msec = elapsed;
    }
  }

  while (weather->num_active < weather->num_origins * cg_add_weather->value) {
    uint32_t age_msec = 0;
    if (hidden_msec) {
      const uint32_t max_age = (uint32_t) Minui64(hidden_msec, 1500u);
      const uint32_t min_age = (uint32_t) Minui64(max_age, max_age / 10);
      age_msec = RandomRangeu(min_age, max_age + 1);
    }

    if (!Cg_misc_weather_SpawnSprite(self, weather, age_msec)) {
      break;
    }
  }

  weather->last_visible = now;
}

/**
 * @brief The client-side entity class descriptor for the `misc_weather` ambient weather system.
 */
const cg_entity_class_t cg_misc_weather = {
  .classname = "misc_weather",
  .Init = Cg_misc_weather_Init,
  .Free = Cg_misc_weather_Free,
  .Think = Cg_misc_weather_Think,
  .data_size = sizeof(cg_weather_t)
};
