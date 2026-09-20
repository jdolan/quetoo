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
#include "game/common/bg_pmove.h"
#include "collision/collision.h"

typedef struct {
  float start;
  float peak;
  float end;
  float peakLife;
} CGameItemRespawnIntensity;

typedef struct {
  float radius;
  float turns;
  float z;
  float drop;
  Vec3 color;
  CGameItemRespawnIntensity intensity;
} CGameItemRespawnHelix;

typedef struct {
  float size;
  float z;
  float drop;
  Vec3 color;
  CGameItemRespawnIntensity intensity;
} CGameItemRespawnRing;

/**
 * @brief Returns envelope intensity for item respawn effects.
 */
static float Cg_ItemRespawnIntensity(const float life, const CGameItemRespawnIntensity *intensity) {

  const float clampedLife = Clampf01(life);

  if (clampedLife <= intensity->peakLife) {
    const float t = Smoothf(clampedLife, 0.f, intensity->peakLife);
    return Mixf(intensity->start, intensity->peak, t);
  }

  const float t = Smoothf(clampedLife, intensity->peakLife, 1.f);
  return Mixf(intensity->peak, intensity->end, t);
}

/**
 * @brief Think callback for item respawn helix sprites.
 */
static void Cg_ItemRespawn_Think(CGameSprite *sprite, float life, float delta) {

  const CGameItemRespawnHelix *helix = sprite->data;
  if (!helix) {
    return;
  }

  const float intensity = Cg_ItemRespawnIntensity(life, &helix->intensity);
  const Vec3 center = sprite->termination;
  const float radius = helix->radius * intensity;
  const float angle = sprite->rotation + life * helix->turns * 2.f * M_PI;

  sprite->origin.x = center.x + cosf(angle) * radius;
  sprite->origin.y = center.y + sinf(angle) * radius;
  sprite->origin.z = center.z + helix->z - life * helix->drop;

  sprite->size = (4.5f + sinf(angle * 1.7f) * 0.8f) * (1.f - life * 0.5f);

  const Vec3 color = Vec3_Scale(helix->color, intensity);
  sprite->color = color;
  sprite->endColor = color;
}

/**
 * @brief Think callback for a face-up ring that descends with item respawn helix.
 */
static void Cg_ItemRespawnRing_Think(CGameSprite *sprite, float life, float delta) {
  (void) delta;

  const CGameItemRespawnRing *ring = sprite->data;
  if (!ring) {
    return;
  }

  const float intensity = Cg_ItemRespawnIntensity(life, &ring->intensity);
  const Vec3 center = sprite->termination;

  sprite->origin = center;
  sprite->origin.z = center.z + ring->z - life * ring->drop;
  sprite->size = ring->size * (0.8f + 0.4f * intensity);

  const Vec3 color = Vec3_Scale(ring->color, intensity);
  sprite->color = color;
  sprite->endColor = color;
}

/**
 * @brief Spawns a descending helix "curtain" and glow for an item respawn event.
 */
static void Cg_ItemRespawnEffect(const Vec3 org, const Color color) {

  const int32_t strands = 2;
  const int32_t segments = 24;
  const float radius = 18.f;
  const float height = 56.f;
  const float turns = .666f;

  for (int32_t i = 0; i < segments; i++) {
    const float t = (float) i / (segments - 1);
    const float z = height * t;
    const float phase = turns * 2.f * M_PI * t;

    for (int32_t strand = 0; strand < strands; strand++) {

      CGameItemRespawnHelix *helix = cgi.Malloc(sizeof(*helix), MEM_TAG_CGAME_LEVEL);

      helix->radius = radius * RandomRangef(0.9f, 1.1f);
      helix->turns = turns * RandomRangef(0.5f, 1.15f) * (Randomf() < 0.5f ? -1.f : 1.f);
      helix->z = z;
      helix->drop = RandomRangef(48.f, 56.f);
      helix->color = color.vec3;
      helix->intensity.start = RandomRangef(0.0f, 0.15f);
      helix->intensity.peak = RandomRangef(1.5f, 2.5f);
      helix->intensity.end = RandomRangef(0.0f, 0.15f);
      helix->intensity.peakLife = RandomRangef(0.2f, 0.4f);

      const float angle = phase + strand * M_PI;

      Cg_AddSprite(&(CGameSprite) {
        .atlasImage = cgSpriteParticle3,
        .origin = org,
        .termination = org,
        .lifetime = RandomRangeu(1700, 2301),
        .rotation = angle,
        .data = helix,
        .Think = Cg_ItemRespawn_Think,
        .lighting = 1.f,
      });
    }
  }

  CGameItemRespawnRing *ring = cgi.Malloc(sizeof(*ring), MEM_TAG_CGAME_LEVEL);
  ring->size = 48.f;
  ring->z = height;
  ring->drop = height * 1.35f;
  ring->color = color.vec3;
  ring->intensity.start = RandomRangef(0.0f, 0.15f);
  ring->intensity.peak = RandomRangef(1.4f, 2.0f);
  ring->intensity.end = RandomRangef(0.0f, 0.15f);
  ring->intensity.peakLife = RandomRangef(0.2f, 0.4f);

  Cg_AddSprite(&(CGameSprite) {
    .atlasImage = cgSpriteRing,
    .origin = Vec3_Fmaf(org, height, Vec3_Up()),
    .termination = org,
    .lifetime = 1200,
    .dir = Vec3_Up(),
    .data = ring,
    .Think = Cg_ItemRespawnRing_Think,
    .lighting = 1.f,
  });

  // glow
  Cg_AddSprite(&(CGameSprite) {
    .origin = Vec3_Fmaf(org, 20.f, Vec3_Up()),
    .lifetime = 1000,
    .size = 150.f,
    .atlasImage = cgSpriteParticle,
    .color = color.vec3,
  });

  Cg_AddLight(&(CGameLight) {
    .origin = org,
    .radius = 160.f,
    .color = color.vec3,
    .intensity = 1.f,
    .decay = 1000
  });
}

/**
 * @brief Spawns a ring and glow sprite for an item pickup event.
 */
static void Cg_ItemPickupEffect(const Vec3 org, const Color color) {

  CGameSprite *s;

  // ring
  if ((s = Cg_AddSprite(&(CGameSprite) {
      .origin = org,
      .lifetime = 400,
      .size = 10.f,
      .atlasImage = cgSpriteRing,
      .color = color.vec3,
      .dir = Vec3_Up()
    }))) {
    s->sizeVelocity = 50.f / MILLIS_TO_SECONDS(s->lifetime);
  }

  // glow
  Cg_AddSprite(&(CGameSprite) {
    .origin = org,
    .lifetime = 1000,
    .size = 150,
    .atlasImage = cgSpriteParticle,
    .color = color.vec3,
  });

  Cg_AddLight(&(CGameLight) {
    .origin = org,
    .radius = 160.f,
    .color = color.vec3,
    .intensity = 1.f,
    .decay = 1000
  });
}

/**
 * @brief Spawns particles and a brief light for a teleporter activation event.
 */
void Cg_TeleporterEffect(const Vec3 org) {

  for (int32_t i = 0; i < 64; i++) {

    Cg_AddSprite(&(CGameSprite) {
      .atlasImage = cgSpriteParticle,
      .size = 8.f,
      .origin = Vec3_Add(Vec3_Add(org, Vec3_RandomRange(-16.f, 16.f)), MakeVec3(0.f, 0.f, RandomRangef(8.f, 32.f))),
      .velocity = Vec3_Add(Vec3_RandomRange(-24.f, 24.f), MakeVec3(0.f, 0.f, RandomRangef(16.f, 48.f))),
      .acceleration.z = -SPRITE_GRAVITY * .1f,
      .lifetime = 500,
      .color = MakeVec3(1.f, 1.f, 1.f),
    });
  }

  Cg_AddLight(&(CGameLight) {
    .origin = org,
    .radius = 120.f,
    .color = MakeVec3(.9f, .9f, .9f),
    .intensity = 1.f,
    .decay = 1000
  });
}

/**
 * @brief A player is gasping for air under water.
 */
static void Cg_GurpEffect(ClientEntity *ent) {

  Vec3 start = ent->origin;
  start.z += 16.0;

  Vec3 end = start;
  end.z += 16.0;

  Cg_BubbleTrail(NULL, start, end, 2.f);
}

/**
 * @brief A player has drowned.
 */
static void Cg_DrownEffect(ClientEntity *ent) {

  Vec3 start = ent->origin;
  start.z += 16.0;

  Vec3 end = start;
  end.z += 16.0;

  Cg_BubbleTrail(NULL, start, end, 2.f);
}

/**
 * @brief Loads a wildcard sample name for the specified client entity.
 */
static SoundSample *Cg_ClientModelSample(const ClientEntity *ent, const char *name) {

  const int32_t client = ent->current.client;
  const CGameClientInfo *info = &cgState.clients[client];

  if (!*info->model) {
    return NULL;
  }

  SoundSample *result = cgi.LoadClientModelSample(info->model, info->torso->mesh->sounds, name);
  return result;
}

/**
 * @brief Resolves the appropriate footstep sound sample for the entity's current ground material.
 */
static SoundSample *Cg_Footstep(ClientEntity *ent) {

  Vec3 start = ent->current.origin;
  start.z += ent->current.bounds.mins.z;

  Vec3 end = start;
  end.z -= PM_STEP_HEIGHT;

  CmTrace tr = cgi.Trace(start, end, Box3_Zero(), ent, CONTENTS_MASK_SOLID);

  if (tr.material) {
    const CmFootsteps *footsteps = &cgi.LoadMaterial(tr.material->name, ASSET_CONTEXT_TEXTURES)->cm->footsteps;

    if (footsteps->numSamples) {
      static uint32_t lastIndex = -1;
      uint32_t index = RandomRangeu(0, footsteps->numSamples);

      if (lastIndex == index) {
        index = (index ^ 1) % footsteps->numSamples;
      }

      lastIndex = index;

      return cgi.LoadSample(footsteps->samples[index].name, ASSET_CONTEXT_NONE);
    }
  }

  Cg_Debug("No ground found for footstep at %s\n", vtos(end));
  return cgi.LoadSample(va("common/step_default_%d", RandomRangei(1, 5)), ASSET_CONTEXT_PLAYERS);
}

/**
 * @brief Process any event set on the given entity. These are only valid for a single
 * frame, so we reset the event flag after processing it.
 */
void Cg_EntityEvent(ClientEntity *ent) {

  EntityState *s = &ent->current;

  SoundPlaySample play = {
    .origin = ent->current.origin,
    .entity = ent,
  };

  switch (s->event) {
    case EV_CLIENT_DROWN:
      play.sample = Cg_ClientModelSample(ent, "*drown_1");
      Cg_DrownEffect(ent);
      break;
    case EV_CLIENT_FALL:
      play.sample = Cg_ClientModelSample(ent, "*fall_2");
      break;
    case EV_CLIENT_FALL_FAR:
      play.sample = Cg_ClientModelSample(ent, "*fall_1");
      break;
    case EV_CLIENT_FOOTSTEP:
      play.sample = Cg_Footstep(ent);
      play.pitch = RandomRangei(-12, 13);
      break;
    case EV_CLIENT_GURP:
      play.sample = Cg_ClientModelSample(ent, "*gurp_1");
      Cg_GurpEffect(ent);
      break;
    case EV_CLIENT_JUMP:
      play.sample = Cg_ClientModelSample(ent, va("*jump_%d", RandomRangeu(1, 5)));
      break;
    case EV_CLIENT_LAND:
      play.sample = Cg_ClientModelSample(ent, "*land_1");
      break;
    case EV_CLIENT_SIZZLE:
      play.sample = Cg_ClientModelSample(ent, "*sizzle_1");
      play.pitch = RandomRangei(-12, +12);
      break;
    case EV_CLIENT_TELEPORT:
      Cg_TeleporterEffect(s->origin);
      break;

    case EV_ITEM_RESPAWN: {
      const GameItemTag tag = (GameItemTag) s->eventData;
      const Color effectColor = bgItemDefs[tag].effectColor;
      play.sample = cgSampleRespawn;
      Cg_ItemRespawnEffect(s->origin, effectColor);
      break;
    }
    case EV_ITEM_PICKUP: {
      const GameItemTag tag = (GameItemTag) s->eventData;
      const Color effectColor = bgItemDefs[tag].effectColor;
      Cg_ItemPickupEffect(s->origin, effectColor);
    }
      break;

    case EV_CLIENT_RAGDOLL:
      Cg_ClientRagdoll(ent);
      break;

    default:
      if (s->event) {
        Cg_Debug("NULL sample for ent #%d event=%d\n",
                 (int32_t) (ent - cgi.client->entities), s->event);
      }
      break;
  }

  if (play.sample) {
    Cg_AddSample(cgi.stage, &play);
  }

  s->event = 0;
}
