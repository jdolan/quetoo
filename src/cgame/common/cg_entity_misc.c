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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "cg_local.h"

/**
 * @brief The `misc_dist` type.
 */
typedef struct {

  /**
   * @brief The sprite template.
   */
  CGameSprite sprite;

  /**
   * @brief The sprite origins, clipped to the backing entity's brushes.
   */
  Vec3 *origins;

  /**
   * @brief The count of sprite origins.
   */
  int32_t numOrigins;

  /**
   * @brief The dust density.
   */
  float density;

  /**
   * @brief Per-particle random acceleration range, applied symmetrically on each axis.
   */
  Vec3 accelerationSpread;

  /**
   * @brief Per-particle random initial rotation range normalized to [0, 1], where 1 is pi radians.
   */
  float rotationSpread;

  /**
   * @brief Per-particle random velocity range, applied symmetrically on each axis.
   */
  Vec3 velocitySpread;

  /**
   * @brief Per-particle random size range as a +/- fraction of base size.
   */
  float sizeSpread;

  /**
   * @brief The count of active sprites.
   */
  int32_t numActive;

  /**
   * @brief The last client time this emitter was visible.
   */
  uint32_t lastVisible;
} CGameDust;

/**
 * @brief Initializes a `misc_dust` entity by loading its sprite and computing spawn origins from brushes.
 */
static const char *cgDustPresetDefault =
  "\\sprite\\particle"
  "\\color\\1 1 1"
  "\\size\\1"
  "\\velocity_spread\\1 1 1"
  "\\size_spread\\.1"
  "\\lifetime\\10"
  "\\lighting\\1"
  "\\density\\1"
  "\\hz\\10";

static const char *cgDustPresetEmbers =
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

static const char *cgDustPresetBubbles =
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

static const char *cgDustPresetFizz =
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

static const char *cgDustPresetFlame =
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

static const char *cgDustPresetSteam =
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

static void Cg_misc_dust_Init(CGameEntity *self) {

  CGameDust *dust = self->data;

  const char *type = cgi.EntityValue(self->def, "type")->nullableString;

  const char *presetStr = cgDustPresetDefault;
  if (!q_strcmp(type, "embers")) {
    presetStr = cgDustPresetEmbers;
  } else if (!q_strcmp(type, "bubbles")) {
    presetStr = cgDustPresetBubbles;
  } else if (!q_strcmp(type, "fizz")) {
    presetStr = cgDustPresetFizz;
  } else if (!q_strcmp(type, "flame")) {
    presetStr = cgDustPresetFlame;
  } else if (!q_strcmp(type, "steam")) {
    presetStr = cgDustPresetSteam;
  }

  CmEntity *preset = cgi.EntityFromInfoString(presetStr);
  CmEntity *def = cgi.EntityAssign(self->def, preset);
  cgi.FreeEntity(preset);

  if (!q_strcmp(type, "fizz")) {
    dust->sprite.animation = cgSpriteFizz01;
  } else if (!q_strcmp(type, "flame")) {
    dust->sprite.atlasImage = cgSpriteFlame;
  } else if (!q_strcmp(type, "steam")) {
    dust->sprite.atlasImage = cgSpriteSteam;
  } else {
    const char *name = cgi.EntityValue(def, "sprite")->nullableString ?: "particle";
    dust->sprite.image = cgi.LoadImage(va("sprites/%s", name), IMG_SPRITE);
    if (dust->sprite.image == NULL) {
      dust->sprite.image = cgi.LoadImage("sprites/particle", IMG_SPRITE);
      Cg_Warn("%s @ %s failed to load sprite %s\n", self->clazz->classname, vtos(self->origin), name);
    }
  }

  dust->sprite.velocity = cgi.EntityValue(def, "velocity")->vec3;
  dust->sprite.acceleration = cgi.EntityValue(def, "acceleration")->vec3;
  dust->sprite.rotation = cgi.EntityValue(def, "rotation")->value;
  dust->sprite.rotationVelocity = cgi.EntityValue(def, "rotation_velocity")->value;
  dust->sprite.dir = cgi.EntityValue(def, "dir")->vec3;
  dust->sprite.color = cgi.EntityValue(def, "color")->vec3;
  dust->sprite.endColor = cgi.EntityValue(def, "end_color")->vec3;
  dust->sprite.size = cgi.EntityValue(def, "size")->value;
  dust->sprite.sizeVelocity = cgi.EntityValue(def, "size_velocity")->value;
  dust->sprite.sizeAcceleration = cgi.EntityValue(def, "size_acceleration")->value;
  dust->sprite.width = cgi.EntityValue(def, "width")->value;
  dust->sprite.height = cgi.EntityValue(def, "height")->value;
  dust->sprite.bounce = cgi.EntityValue(def, "bounce")->value;
  dust->sprite.lifetime = SECONDS_TO_MILLIS(cgi.EntityValue(def, "lifetime")->value);
  dust->sprite.lighting = cgi.EntityValue(def, "lighting")->value;

  dust->density = cgi.EntityValue(def, "density")->value;

  const CmEntity *sizeSpread = cgi.EntityValue(def, "size_spread");
  dust->sizeSpread = (sizeSpread->parsed & ENTITY_FLOAT) ? sizeSpread->value : .1f;
  dust->velocitySpread = cgi.EntityValue(def, "velocity_spread")->vec3;
  dust->accelerationSpread = cgi.EntityValue(def, "acceleration_spread")->vec3;
  dust->rotationSpread = Clampf01(cgi.EntityValue(def, "rotation_spread")->value);

  self->hz = cgi.EntityValue(def, "hz")->value;

  cgi.FreeEntity(def);

  self->bounds = Box3_Null();

  const CmBsp *bsp = cgi.WorldModel()->bsp->cm;
  const CmEntity *brushDef = self->id < bsp->numEntities ? bsp->entities[self->id] : self->def;
  Vector *brushes = cgi.EntityBrushes(brushDef);
  for (size_t i = 0; i < brushes->count; i++) {

    const CmBspBrush *brush = VectorValue(brushes, CmBspBrush *, i);
    self->bounds = Box3_Union(self->bounds, brush->bounds);

    const Vec3 brushSize = Box3_Size(brush->bounds);
    const int32_t brushOrigins = Maxi(Vec3_Length(brushSize) * dust->density, 1);

    if (dust->origins) {
      dust->origins = cgi.Realloc(dust->origins, (dust->numOrigins + brushOrigins) * sizeof(Vec3));
    } else {
      dust->origins = cgi.LinkMalloc(brushOrigins * sizeof(Vec3), dust);
    }

    int32_t j = dust->numOrigins;
    while (j < dust->numOrigins + brushOrigins) {

      const Vec3 point = Box3_RandomPoint(brush->bounds);

      if (cgi.PointInsideBrush(point, brush)) {
        dust->origins[j++] = point;
      }
    }

    dust->numOrigins += brushOrigins;
  }

  release(brushes);
}

/**
 * @brief Edit callback that purges all live sprites referencing this entity's data.
 */
static void Cg_misc_dust_Free(CGameEntity *self) {
  Cg_FreeSpritesByData(self->data);
  ((CGameDust *)self->data)->lastVisible = 0;
}

/**
 * @brief Think callback that fades dust sprites in at birth and decrements the active count at death.
 */
static void Cg_misc_dust_SpriteThink(CGameSprite *sprite, float life, float delta) {

  CGameDust *dust = sprite->data;

  if (life <= .1f) {
    sprite->color = Vec3_Scale(dust->sprite.color, life / .1f);
  }

  if (life >= 1.f) {
    dust->numActive--;
  }
}

/**
 * @brief Spawns one dust sprite and optionally backdates it for visibility catch-up.
 */
static CGameSprite *Cg_misc_dust_SpawnSprite(CGameDust *dust, uint32_t ageMsec) {

  CGameSprite s = dust->sprite;

  s.origin = dust->origins[RandomRangei(0, dust->numOrigins)];
  s.origin = Vec3_Add(s.origin, Vec3_RandomDir());
  const float minSize = Maxf(0.f, s.size * (1.f - dust->sizeSpread));
  const float maxSize = Maxf(minSize, s.size * (1.f + dust->sizeSpread));
  s.size = RandomRangef(minSize, maxSize);
  s.velocity = Vec3_Add(s.velocity, Vec3_RandomRanges(
    -dust->velocitySpread.x, dust->velocitySpread.x,
    -dust->velocitySpread.y, dust->velocitySpread.y,
    -dust->velocitySpread.z, dust->velocitySpread.z));
  s.rotation += RandomRangef(-(float) M_PI * dust->rotationSpread, (float) M_PI * dust->rotationSpread);
  s.acceleration = Vec3_Add(s.acceleration, Vec3_RandomRanges(
    -dust->accelerationSpread.x, dust->accelerationSpread.x,
    -dust->accelerationSpread.y, dust->accelerationSpread.y,
    -dust->accelerationSpread.z, dust->accelerationSpread.z));
  s.lifetime = RandomRangeu(s.lifetime * .666f, s.lifetime * 1.333f);
  s.Think = Cg_misc_dust_SpriteThink;
  s.data = dust;
  s.flags |= SPRITE_DATA_NOFREE;

  CGameSprite *emitted = Cg_AddSprite(&s);
  if (!emitted) {
    return NULL;
  }

  if (ageMsec && emitted->lifetime > 1) {
    ageMsec = (uint32_t) Minui64(ageMsec, emitted->lifetime - 1);
    emitted->time -= ageMsec;
    emitted->timestamp -= ageMsec;
  }

  dust->numActive++;
  return emitted;
}

/**
 * @brief Emits dust sprites to maintain the active count up to the number of configured origins.
 */
static void Cg_misc_dust_Think(CGameEntity *self) {

  if (!cg_addAtmospheric->value) {
    return;
  }

  CGameDust *dust = self->data;

  const uint32_t now = cgi.client->unclampedTime;
  uint32_t hiddenMsec = 0;
  if (dust->lastVisible) {
    const uint32_t elapsed = now - dust->lastVisible;
    const uint32_t gapThreshold = (uint32_t) Maxui64(cgi.client->worldMsec * 2u, 64u);
    if (elapsed > gapThreshold) {
      hiddenMsec = elapsed;
    }
  }

  while (dust->numActive < dust->numOrigins) {
    uint32_t ageMsec = 0;
    if (hiddenMsec) {
      const uint32_t maxLifetimeAge = (uint32_t) Maxf(dust->sprite.lifetime, 2.f) - 1;
      const uint32_t maxAge = (uint32_t) Minui64(hiddenMsec, maxLifetimeAge);
      const uint32_t minAge = (uint32_t) Minui64(maxAge, maxAge / 10);
      ageMsec = RandomRangeu(minAge, maxAge + 1);
    }

    if (!Cg_misc_dust_SpawnSprite(dust, ageMsec)) {
      break;
    }
  }

  dust->lastVisible = now;
}

/**
 * @brief The client-side entity class descriptor for the `misc_dust` ambient dust emitter.
 */
const CGameEntityClass cgMiscDust = {
  .classname = "misc_dust",
  .Init = Cg_misc_dust_Init,
  .Free = Cg_misc_dust_Free,
  .Think = Cg_misc_dust_Think,
  .dataSize = sizeof(CGameDust)
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
  SoundSample *sample;
} CGameFlame;

/**
 * @brief Initializes a `misc_flame` entity by reading radius, density, and sound from the entity definition.
 */
static void Cg_misc_flame_Init(CGameEntity *self) {

  self->hz = cgi.EntityValue(self->def, "hz")->value ?: 10.f;
  self->drift = cgi.EntityValue(self->def, "drift")->value ?: .1f;

  CGameFlame *flame = self->data;

  flame->density = cgi.EntityValue(self->def, "density")->value ?: .666f;
  flame->radius = cgi.EntityValue(self->def, "radius")->value ?: 16.f;

  self->bounds = Box3_FromCenterRadius(self->origin, flame->radius * 16.f);

  const char *sound = cgi.EntityValue(self->def, "sound")->nullableString;
  if (sound) {
    if (q_strcmp(sound, "none")) {
      flame->sample = cgi.LoadSample(sound, ASSET_CONTEXT_SOUNDS);
    }
  } else {
    flame->sample = cgSampleFire;
  }
}

/**
 * @brief Emits flame sprites for a `misc_flame` entity each frame.
 */
static void Cg_misc_flame_Think(CGameEntity *self) {

  CGameFlame *flame = self->data;

  const float r = flame->radius;
  const float s = Clampf(r / 64.f, .125f, 1.f);

  for (int32_t i = 0; i < flame->radius * flame->density; i++) {
    const float hue = color_hue_orange + RandomRangef(-20.f, 20.f);
    const float sat = RandomRangef(.7f, 1.f);

    if (!Cg_AddSprite(&(CGameSprite) {
        .atlasImage = cgSpriteFlame,
        .origin = Vec3_Fmaf(self->origin, r, Vec3_RandomRanges(-s, s, -s, s, -.1f, .5f)),
        .velocity = Vec3_Scale(Vec3_RandomRanges(-r, r, -r, r, 0.f, 24.f), s * s),
        .acceleration.z = 150.f * s,
        .lifetime = 750 + Randomf() * 250,
        .size = 1.f,
        .sizeVelocity = 16.f,
        .sizeAcceleration = 150.f * s,
        .color = ColorHSV(hue, sat, RandomRangef(.7f, 1.f)).vec3,
      })) {
      break;
    }
  }

  // Smoke — rises above the flame column, expanding and drifting upward
  const int32_t numSmoke = (int32_t) Maxf(1.f, flame->radius * flame->density * .15f);
  for (int32_t i = 0; i < numSmoke; i++) {
    RenderAnimation *anim = (i & 1) ? cgSpriteSmoke05 : cgSpriteSmoke04;
    const Vec3 smokeOrigin = {
      .x = self->origin.x + RandomRangef(-r * .3f, r * .3f),
      .y = self->origin.y + RandomRangef(-r * .3f, r * .3f),
      .z = self->origin.z + r * RandomRangef(1.f, 2.5f),
    };
    const float sz = Maxf(4.f, r * RandomRangef(.4f, .8f));
    if (!Cg_AddSprite(&(CGameSprite) {
        .animation = anim,
        .origin = smokeOrigin,
        .velocity = MakeVec3(RandomRangef(-8.f, 8.f) * s,
                         RandomRangef(-8.f, 8.f) * s,
                         RandomRangef(20.f, 40.f) * s),
        .rotation = RandomRadian(),
        .rotationVelocity = RandomRangef(-.2f, .2f),
        .lifetime = Cg_AnimationLifetime(anim, RandomRangef(10.f, 20.f)),
        .size = sz,
        .sizeVelocity = sz * .5f,
        .color = Vec3_Scale(Vec3_One(), RandomRangef(.15f, .65f)),
        .lighting = 1.f,
      })) {
      break;
    }
  }

  if (flame->sample) {
    Cg_AddSample(cgi.stage, &(const SoundPlaySample) {
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
const CGameEntityClass cgMiscFlame = {
  .classname = "misc_flame",
  .Init = Cg_misc_flame_Init,
  .Think = Cg_misc_flame_Think,
  .dataSize = sizeof(CGameFlame)
};

/**
 * @brief Initializes a `misc_model` entity by loading its model from the entity definition.
 */
static void Cg_misc_model_Init(CGameEntity *self) {

  RenderEntity *entity = self->data;

  entity->origin = self->origin;
  entity->angles = cgi.EntityValue(self->def, "angles")->vec3;
  entity->scale = cgi.EntityValue(self->def, "scale")->value ?: 1.f;
  entity->lerp = 1.f;
  entity->color = Vec4_One();

  const CmEntity *model = cgi.EntityValue(self->def, "model");
  if (model->parsed & ENTITY_STRING) {
    entity->model = cgi.LoadModel(model->string);
    if (entity->model) {
      entity->bounds = Box3_Scale(entity->model->bounds, entity->scale);
      entity->absBounds = Box3_Translate(entity->bounds, entity->origin);
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
static void Cg_misc_model_Think(CGameEntity *self) {

  const RenderEntity *entity = self->data;

  if (entity->model) {
    RenderEntity *out = cgi.AddEntity(cgi.view, entity);
    if (out) {
      self->bounds = out->absModelBounds;
    }
  }
}

/**
 * @brief The client-side entity class descriptor for the `misc_model` static model renderer.
 */
const CGameEntityClass cgMiscModel = {
  .classname = "misc_model",
  .Init = Cg_misc_model_Init,
  .Think = Cg_misc_model_Think,
  .dataSize = sizeof(RenderEntity)
};

/**
 * @brief The `misc_sound` type.
 */
typedef struct {

  /**
   * @brief The play sample template.
   */
  SoundPlaySample play;
} CGameMiscSound;

/**
 * @brief Initializes a `misc_sound` entity by loading its sample and configuring play parameters.
 */
static void Cg_misc_sound_Init(CGameEntity *self) {

  self->hz = cgi.EntityValue(self->def, "hz")->value ?: 0.f;
  self->drift = cgi.EntityValue(self->def, "drift")->value ?: .3f;

  CGameMiscSound *sound = self->data;

  if (cgi.EntityValue(self->def, "sound")->parsed & ENTITY_STRING) {
    sound->play.sample = cgi.LoadSample(cgi.EntityValue(self->def, "sound")->string, ASSET_CONTEXT_SOUNDS);
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
static void Cg_misc_sound_Think(CGameEntity *self) {

  const CGameMiscSound *sound = self->data;

  if (sound->play.sample) {
    Cg_AddSample(cgi.stage, &sound->play);
  }
}

/**
 * @brief The client-side entity class descriptor for the `misc_sound` ambient sound emitter.
 */
const CGameEntityClass cgMiscSound = {
  .classname = "misc_sound",
  .Init = Cg_misc_sound_Init,
  .Think = Cg_misc_sound_Think,
  .dataSize = sizeof(CGameMiscSound)
};

/**
 * @brief The `misc_sparks` type.
 */
typedef struct {

  /**
   * @brief The sparks direction, configured by either key, or by target entity.
   */
  Vec3 dir;

  /**
   * @brief The count of sparks per emission.
   */
  int32_t count;
} CGameMiscSparks;

/**
 * @brief Initializes a `misc_sparks` entity by resolving its emission direction and spark count.
 */
static void Cg_misc_sparks_Init(CGameEntity *self) {

  self->hz = cgi.EntityValue(self->def, "hz")->value ?: .5f;
  self->drift = cgi.EntityValue(self->def, "drift")->value ?: 3.f;

  CGameMiscSparks *sparks = self->data;

  if (self->target) {
    const Vec3 targetOrigin = cgi.EntityValue(self->target, "origin")->vec3;
    sparks->dir = Vec3_Normalize(Vec3_Subtract(targetOrigin, self->origin));
  } else {
    if (cgi.EntityValue(self->def, "angle")->parsed & ENTITY_INTEGER) {
      const int32_t angle = cgi.EntityValue(self->def, "angle")->integer;
      if (angle == -1.f) {
        sparks->dir = Vec3_Up();
      } else if (angle == -2.f) {
        sparks->dir = Vec3_Down();
      } else {
        Vec3_Vectors(MakeVec3(0.f, angle, 0.f), &sparks->dir, NULL, NULL);
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
static void Cg_misc_sparks_Think(CGameEntity *self) {

  const CGameMiscSparks *sparks = self->data;

  Cg_SparksEffect(self->origin, sparks->dir, sparks->count);
}

/**
 * @brief The client-side entity class descriptor for the `misc_sparks` spark emitter.
 */
const CGameEntityClass cgMiscSparks = {
  .classname = "misc_sparks",
  .Init = Cg_misc_sparks_Init,
  .Think = Cg_misc_sparks_Think,
  .dataSize = sizeof(CGameMiscSparks)
};

/**
 * @brief The `misc_sprite` type.
 */
typedef struct {

  /**
   * @brief The sprite template instance.
   */
  CGameSprite sprite;

  /**
   * @brief The count of sprites to spawn per Think.
   */
  int32_t count;
} CGameMiscSprite;

/**
 * @brief Initializes a `misc_sprite` entity by loading the sprite and reading emission parameters.
 */
static void Cg_misc_sprite_Init(CGameEntity *self) {

  self->hz = cgi.EntityValue(self->def, "hz")->value ?: .5f;
  self->drift = cgi.EntityValue(self->def, "drift")->value ?: 3.f;

  CGameMiscSprite *sprite = self->data;

  sprite->count = cgi.EntityValue(self->def, "count")->integer ?: 1;

  const char *name = cgi.EntityValue(self->def, "sprite")->nullableString ?: "particle";
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
    const Vec3 targetOrigin = cgi.EntityValue(self->target, "origin")->vec3;
    sprite->sprite.velocity = Vec3_Subtract(targetOrigin, self->origin);
  } else {
    sprite->sprite.velocity = cgi.EntityValue(self->def, "velocity")->vec3;
  }

  sprite->sprite.acceleration = cgi.EntityValue(self->def, "acceleration")->vec3;
  sprite->sprite.rotation = cgi.EntityValue(self->def, "rotation")->value;
  sprite->sprite.rotationVelocity = cgi.EntityValue(self->def, "rotation_velocity")->value;
  sprite->sprite.dir = cgi.EntityValue(self->def, "dir")->vec3;

  const CmEntity *color = cgi.EntityValue(self->def, "color");
  if (color->parsed & ENTITY_VEC3) {
    sprite->sprite.color = color->vec3;
  } else {
    sprite->sprite.color = MakeVec3(1.f, 1.f, 1.f);
  }

  const CmEntity *endColor = cgi.EntityValue(self->def, "end_color");
  if (endColor->parsed & ENTITY_VEC3) {
    sprite->sprite.endColor = endColor->vec3;
  } else {
    sprite->sprite.endColor = MakeVec3(0.f, 0.f, 0.f);
  }

  sprite->sprite.size = cgi.EntityValue(self->def, "size")->value ?: 1.f;
  sprite->sprite.sizeVelocity = cgi.EntityValue(self->def, "size_velocity")->value;
  sprite->sprite.sizeAcceleration = cgi.EntityValue(self->def, "size_acceleration")->value;
  sprite->sprite.width = cgi.EntityValue(self->def, "width")->value;
  sprite->sprite.height = cgi.EntityValue(self->def, "height")->value;
  sprite->sprite.bounce = cgi.EntityValue(self->def, "bounce")->value;
  sprite->sprite.lifetime = SECONDS_TO_MILLIS(cgi.EntityValue(self->def, "lifetime")->value ?: 10.f);
  sprite->sprite.lighting = cgi.EntityValue(self->def, "lighting")->value ?: 1.f;
}

/**
 * @brief Emits interpolated sprites between the entity and its team counterpart each frame.
 */
static void Cg_misc_sprite_Think(CGameEntity *self) {

  const CGameMiscSprite *this = self->data, *that = self->data;

  const CGameEntity *teammate = Cg_EntityForDefinition(self->team);
  if (teammate) {
    if (!q_strcmp(self->clazz->classname, teammate->clazz->classname)) {
      that = teammate->data;
    } else {
      Cg_Warn("Teammate is not %s\n", self->clazz->classname);
    }
  }

  if (this->sprite.media) {
    for (int32_t i = 0; i < this->count; i++) {

      CGameSprite s = this->sprite;

      if (Randomi() & 1) {
        s.media = that->sprite.media;
      }

      s.origin = Vec3_Mix(this->sprite.origin, that->sprite.origin, Randomf());
      s.velocity = Vec3_Mix(this->sprite.velocity, that->sprite.velocity, Randomf());
      s.acceleration = Vec3_Mix(this->sprite.acceleration, that->sprite.acceleration, Randomf());
      s.rotation = Mixf(this->sprite.rotation, that->sprite.rotation, Randomf());
      s.rotationVelocity = Mixf(this->sprite.rotationVelocity, that->sprite.rotationVelocity, Randomf());
      s.dir = Vec3_Mix(this->sprite.dir, that->sprite.dir, Randomf());
      s.color = Vec3_Mix(this->sprite.color, that->sprite.color, Randomf());
      s.endColor = Vec3_Mix(this->sprite.endColor, that->sprite.endColor, Randomf());
      s.size = Mixf(this->sprite.size, that->sprite.size, Randomf());
      s.width = Mixf(this->sprite.width, that->sprite.width, Randomf());
      s.height = Mixf(this->sprite.height, that->sprite.height, Randomf());
      s.sizeVelocity = Mixf(this->sprite.sizeVelocity, that->sprite.sizeVelocity, Randomf());
      s.sizeAcceleration = Mixf(this->sprite.sizeAcceleration, that->sprite.sizeAcceleration, Randomf());
      s.bounce = Mixf(this->sprite.bounce, that->sprite.bounce, Randomf());
      s.lighting = Mixf(this->sprite.lighting, that->sprite.lighting, Randomf());

      Cg_AddSprite(&s);
    }
  }
}

/**
 * @brief The client-side entity class descriptor for the `misc_sprite` configurable sprite emitter.
 */
const CGameEntityClass cgMiscSprite = {
  .classname = "misc_sprite",
  .Init = Cg_misc_sprite_Init,
  .Think = Cg_misc_sprite_Think,
  .dataSize = sizeof(CGameMiscSprite)
};

/**
 * @brief Data struct holding velocity, size, count, and sound sample for a `misc_steam` entity.
 */
typedef struct {
  Vec3 velocity;
  float size;
  int32_t count;
  SoundSample *sample;
} CGameMiscSteam;

/**
 * @brief Initializes a `misc_steam` entity by resolving velocity, size, count, and optional sound.
 */
static void Cg_misc_steam_Init(CGameEntity *self) {

  CGameMiscSteam *steam = self->data;

  self->hz = cgi.EntityValue(self->def, "hz")->value ?: 30.f;
  self->drift = cgi.EntityValue(self->def, "drift")->value ?: .01f;

  if (self->target) {
    const Vec3 targetOrigin = cgi.EntityValue(self->target, "origin")->vec3;
    steam->velocity = Vec3_Subtract(targetOrigin, self->origin);
  } else {
    const CmEntity *velocity = cgi.EntityValue(self->def, "velocity");
    if (velocity->parsed & ENTITY_VEC3) {
      steam->velocity = velocity->vec3;
    } else {
      steam->velocity = MakeVec3(0.f, 0.f, 32.f);
    }
  }

  steam->size = cgi.EntityValue(self->def, "size")->value ?: 32.f;
  steam->count = cgi.EntityValue(self->def, "count")->integer ?: 1;

  const char *sound = cgi.EntityValue(self->def, "sound")->nullableString;
  if (sound) {
    if (q_strcmp(sound, "none")) {
      steam->sample = cgi.LoadSample(sound, ASSET_CONTEXT_SOUNDS);
    }
  } else {
    steam->sample = cgSampleSteam;
  }
}

/**
 * @brief Emits steam puff sprites or a bubble trail and plays the steam loop sound each frame.
 */
static void Cg_misc_steam_Think(CGameEntity *self) {

  const CGameMiscSteam *steam = self->data;

  const Vec3 end = Vec3_Add(self->origin, steam->velocity);

  if (cgi.PointContents(end) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(NULL, self->origin, end, 2.f);
    return;
  }

  for (int32_t i = 0; i < steam->count; i++) {
    if (!Cg_AddSprite(&(CGameSprite) {
      .atlasImage = cgSpriteSteam,
      .origin = self->origin,
      .velocity = Vec3_Add(steam->velocity, Vec3_RandomRange(-2.f, 2.f)),
      .acceleration = Vec3_Add(Vec3_Scale(Vec3_Up(), 20.f), Vec3_RandomDir()),
      .lifetime = 6500 / (5.f + Randomf() * .5f),
      .rotation = Randomf(),
      .rotationVelocity = RandomRangef(-1.f, 1.f),
      .size = RandomRangef(.9f * steam->size, 1.1f * steam->size),
      .sizeVelocity = 10.f,
      .color = MakeVec3(.25f, .25f, .25f),
      .lighting = 0.5f,
    })) {
      break;
    };
  }

  if (steam->sample) {
    Cg_AddSample(cgi.stage, &(const SoundPlaySample) {
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
const CGameEntityClass cgMiscSteam = {
  .classname = "misc_steam",
  .Init = Cg_misc_steam_Init,
  .Think = Cg_misc_steam_Think,
  .dataSize = sizeof(CGameMiscSteam)
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
  Vec4 *origins;

  /**
   * @brief The count of sprite origins.
   */
  int32_t numOrigins;

  /**
   * @brief The density multiplier.
   */
  float density;

  /**
   * @brief The count of active sprites.
   */
  int32_t numActive;

  /**
   * @brief The ambient sound sample.
   */
  SoundSample *sample;

  /**
   * @brief The last client time this emitter was visible.
   */
  uint32_t lastVisible;
} CGameWeather;

/**
 * @brief Initializes a `misc_weather` entity by reading weather type, sound, and brush spawn origins.
 */
static void Cg_misc_weather_Init(CGameEntity *self) {

  CGameWeather *weather = self->data;

  const char *type = cgi.EntityValue(self->def, "weather")->nullableString;
  if (type) {
    if (q_strstr(type, "rain")) {
      weather->weather |= WEATHER_RAIN;
    }
    if (q_strstr(type, "snow")) {
      weather->weather |= WEATHER_SNOW;
    }
    if (q_strstr(type, "ash")) {
      weather->weather |= WEATHER_ASH;
    }
  }

  if (!weather->weather) {
    weather->weather = WEATHER_RAIN;
  }

  const char *sound = cgi.EntityValue(self->def, "sound")->nullableString;
  if (sound) {
    if (q_strcmp(sound, "none")) {
      weather->sample = cgi.LoadSample(sound, ASSET_CONTEXT_SOUNDS);
    }
  } else {
    if (weather->weather & WEATHER_RAIN) {
      weather->sample = cgSampleRain;
    } else if (weather->weather & WEATHER_SNOW) {
      weather->sample = cgSampleSnow;
    } else if (weather->weather & WEATHER_ASH) {
      weather->sample = cgSampleAsh;
    }
  }

  weather->density = cgi.EntityValue(self->def, "density")->value ?: 1.f;

  if (weather->weather & WEATHER_RAIN) {
    self->hz = cgi.EntityValue(self->def, "hz")->value ?: 30.f;
  } else {
    self->hz = cgi.EntityValue(self->def, "hz")->value ?: 10.f;
  }

  self->bounds = Box3_Null();

  const CmBsp *bsp = cgi.WorldModel()->bsp->cm;
  const CmEntity *brushDef = self->id < bsp->numEntities ? bsp->entities[self->id] : self->def;
  Vector *brushes = cgi.EntityBrushes(brushDef);
  for (size_t i = 0; i < brushes->count; i++) {

    const CmBspBrush *brush = VectorValue(brushes, CmBspBrush *, i);
    self->bounds = Box3_Union(self->bounds, brush->bounds);

    const Vec3 brushSize = Box3_Size(brush->bounds);

    // Grid spacing derived from XY footprint and density, independent of brush height
    const float spacing = powf(brushSize.x * brushSize.y, 0.25f) / sqrtf(weather->density);
    const int32_t numCols = Maxi((int32_t) (brushSize.x / spacing), 1);
    const int32_t numRows = Maxi((int32_t) (brushSize.y / spacing), 1);
    const float dx = brushSize.x / numCols;
    const float dy = brushSize.y / numRows;

    if (weather->origins) {
      weather->origins = cgi.Realloc(weather->origins, (weather->numOrigins + numCols * numRows) * sizeof(Vec4));
    } else {
      weather->origins = cgi.LinkMalloc(numCols * numRows * sizeof(Vec4), weather);
    }

    for (int32_t row = 0; row < numRows; row++) {
      for (int32_t col = 0; col < numCols; col++) {

        const float x = brush->bounds.mins.x + (col + 0.5f) * dx;
        const float y = brush->bounds.mins.y + (row + 0.5f) * dy;

        for (float z = brush->bounds.maxs.z; z >= brush->bounds.mins.z; z -= 1.f) {
          if (cgi.PointInsideBrush(MakeVec3(x, y, z), brush)) {
            weather->origins[weather->numOrigins++] = MakeVec4(x, y, z, 0.f);
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
static void Cg_misc_weather_SpriteThink(CGameSprite *sprite, float life, float delta) {

  CGameWeather *weather = sprite->data;

  if (life >= 1.f) {
    weather->numActive--;
  }
}

/**
 * @brief Spawns one weather sprite and optionally backdates it for visibility catch-up.
 */
static CGameSprite *Cg_misc_weather_SpawnSprite(CGameEntity *self, CGameWeather *weather, uint32_t ageMsec) {

  const int32_t index = RandomRangei(0, weather->numOrigins);
  Vec4 *origin = &weather->origins[index];
  Vec3 pos = MakeVec3(origin->x, origin->y, origin->z);

  if (origin->w == 0.f) {
    const Vec3 end = MakeVec3(pos.x, pos.y, pos.z - MAX_WORLD_AXIAL);
    const CmTrace trace = cgi.Trace(pos, end, Box3_Zero(), NULL, CONTENTS_SOLID);
    origin->w = pos.z - trace.end.z;
    self->bounds = Box3_Append(self->bounds, trace.end);
  }

  float height = origin->w;

  // Distribute spawn origins randomly along the vertical axis to avoid "sheet" effect.
  const float verticalOffset = Randomf() * origin->w;
  pos.z -= verticalOffset;
  height = origin->w - verticalOffset;

  CGameSprite s = {
    .origin = pos,
    .Think = Cg_misc_weather_SpriteThink,
    .data = weather,
    .flags = SPRITE_DATA_NOFREE,
  };

  if (weather->weather & WEATHER_RAIN) {
    s.atlasImage = cgSpriteRain;
    s.color = MakeVec3(1.f, 1.f, 1.f);
    s.size = 32.f;
    s.velocity = Vec3_Subtract(Vec3_RandomRange(-2.f, 2.f), MakeVec3(0.f, 0.f, 800.f));
    s.axis = SPRITE_AXIS_X | SPRITE_AXIS_Y;
    s.lifetime = 1000.f * Maxf(height - s.size, 0.f) / 800.f * RandomRangef(.8f, 1.2f);
    s.lighting = 1.f;

    // Suppress splash bursts for catch-up sprites; they should appear already in-flight.
    if (!ageMsec && Randomf() > .8f) {
      Cg_AddSprite(&(CGameSprite) {
        .atlasImage = cgSpriteWaterRing,
        .lifetime = 300,
        .origin = MakeVec3(pos.x, pos.y, pos.z - height + 2.f),
        .size = 4.f,
        .sizeVelocity = 4.f * 6.f,
        .rotation = RandomRadian(),
        .dir = Vec3_Up(),
        .color = MakeVec3(1.f, 1.f, 1.f),
        .lighting = 1.f
      });
    }
  } else if (weather->weather & WEATHER_SNOW) {
    s.atlasImage = cgSpriteSnow;
    s.color = MakeVec3(1.f, 1.f, 1.f);
    s.size = 4.f;
    s.velocity = Vec3_Subtract(Vec3_RandomRange(-12.f, 12.f), MakeVec3(0.f, 0.f, 120.f));
    s.acceleration = Vec3_RandomRange(-12.f, 12.f);
    s.lifetime = 1000.f * height / 120.f * RandomRangef(.8f, 1.2f);
  } else if (weather->weather & WEATHER_ASH) {
    const float color = RandomRangef(0.25f, 0.75f);
    s.atlasImage = cgSpriteAsh;
    s.color = MakeVec3(color, color, color);
    s.size = RandomRangef(1.f, 3.f);
    s.velocity = Vec3_Subtract(Vec3_RandomRange(-12.f, 12.f), MakeVec3(0.f, 0.f, 25.f));
    s.acceleration = Vec3_RandomRange(-12.f, 12.f);
    s.rotation = RandomRadian();
    s.lifetime = 1000.f * height / 25.f * RandomRangef(.8f, 1.2f);
  }

  CGameSprite *emitted = Cg_AddSprite(&s);
  if (!emitted) {
    return NULL;
  }

  if (ageMsec && emitted->lifetime > 1) {
    ageMsec = (uint32_t) Minui64(ageMsec, emitted->lifetime - 1);
    emitted->time -= ageMsec;
    emitted->timestamp -= ageMsec;
  }

  weather->numActive++;
  return emitted;
}

/**
 * @brief Edit callback that purges all live sprites referencing this entity's data.
 */
static void Cg_misc_weather_Free(CGameEntity *self) {
  Cg_FreeSpritesByData(self->data);
  ((CGameWeather *)self->data)->lastVisible = 0;
}

static void Cg_misc_weather_Think(CGameEntity *self) {

  if (!cg_addWeather->value) {
    return;
  }

  CGameWeather *weather = self->data;

  const uint32_t now = cgi.client->unclampedTime;
  if (weather->sample) {
    Cg_AddSample(cgi.stage, &(const SoundPlaySample) {
      .sample = weather->sample,
      .flags = S_PLAY_AMBIENT | S_PLAY_LOOP | S_PLAY_FRAME,
      .entity = Cg_Self()
    });
  }

  uint32_t hiddenMsec = 0;
  if (weather->lastVisible) {
    const uint32_t elapsed = now - weather->lastVisible;
    const uint32_t gapThreshold = (uint32_t) Maxui64(cgi.client->worldMsec * 2u, 64u);
    if (elapsed > gapThreshold) {
      hiddenMsec = elapsed;
    }
  }

  while (weather->numActive < weather->numOrigins * cg_addWeather->value) {
    uint32_t ageMsec = 0;
    if (hiddenMsec) {
      const uint32_t maxAge = (uint32_t) Minui64(hiddenMsec, 1500u);
      const uint32_t minAge = (uint32_t) Minui64(maxAge, maxAge / 10);
      ageMsec = RandomRangeu(minAge, maxAge + 1);
    }

    if (!Cg_misc_weather_SpawnSprite(self, weather, ageMsec)) {
      break;
    }
  }

  weather->lastVisible = now;
}

/**
 * @brief The client-side entity class descriptor for the `misc_weather` ambient weather system.
 */
const CGameEntityClass cgMiscWeather = {
  .classname = "misc_weather",
  .Init = Cg_misc_weather_Init,
  .Free = Cg_misc_weather_Free,
  .Think = Cg_misc_weather_Think,
  .dataSize = sizeof(CGameWeather)
};
