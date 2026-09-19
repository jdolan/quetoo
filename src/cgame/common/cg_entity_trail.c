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

/**
 * @brief Returns the BSP contents flags for the bounding box spanning the given trail segment.
 */
int32_t Cg_TrailContents(const Vec3 start, const Vec3 end) {
  return cgi.BoxContents(Box3_FromPoints((const Vec3[]) { start, end }, 2));
}

/**
 * @brief Calculates the life start fraction and fractional step for trail particle spawning.
 */
static inline void Cg_ParticleTrailLifeOffset(Vec3 start, Vec3 end, float speed, float step, float *lifeStart, float *lifeFrac) {
  *lifeStart = (speed - Vec3_Distance(start, end)) / 1000;
  *lifeFrac = (1.0 - *lifeStart) * step;
}

/**
 * @brief Calculates the number of "distance" steps that have been travelled between the last recorded trail position & the end.
 * @param end The end of the trail - the current position, basically.
 * @param distance The distance that the trail should spawn particles within.
 * @param ent The entity spawning this trail. If this is `NULL`, "start" must be pointing to a proper start point.
 * @param trail The trail ID.
 * @param start The trail's output start position.
 * @param dir The trail's output direction.
 * @return The number of whole steps that was travelled by this trail.
*/
static int32_t Cg_TrailCount(const Vec3 end, float freq, ClientEntity *ent, ClientTrailId trail, Vec3 *start, Vec3 *dir) {
  const float dist = Vec3_Distance(end, ent ? ent->trailOrigins[trail] : *start);
  static Vec3 _start, _dir;

  // haven't travelled long enough yet
  if (dist < freq) {
    return 0;
  }

  // calculate steps
  const int32_t steps = (int) truncf(dist / freq);

  // allow nulls to be passed
  if (!start) {
    start = &_start;
  }

  if (!dir) {
    dir = &_dir;
  }

  // adjust new origin
  if (ent) {
    *start = ent->trailOrigins[trail];
    *dir = Vec3_Scale(Vec3_Subtract(end, ent->trailOrigins[trail]), 1.0f / dist);
    ent->trailOrigins[trail] = Vec3_Fmaf(ent->trailOrigins[trail], steps * freq, *dir);
  } else {
    *dir = Vec3_Scale(Vec3_Subtract(end, *start), 1.0f / dist);
  }

  return steps;
}

/**
 * @brief Emits a breath bubble from the entity's mouth when the entity is submerged in liquid.
 */
void Cg_BreathTrail(ClientEntity *ent) {

  if (ent->animation1.animation < ANIM_TORSO_GESTURE) { // death animations
    return;
  }

  if (cgi.client->unclampedTime < ent->timestamp) {
    return;
  }

  Vec3 pos = ent->origin;

  if (Cg_IsDucking(ent)) {
    pos.z += 18.0;
  } else {
    pos.z += 30.0;
  }

  Vec3 forward;
  Vec3_Vectors(ent->angles, &forward, NULL, NULL);

  pos = Vec3_Fmaf(pos, 8.f, forward);

  const int32_t contents = cgi.PointContents(pos);

  if (contents & CONTENTS_MASK_LIQUID) {
    if ((contents & CONTENTS_MASK_LIQUID) == CONTENTS_WATER) {

      Cg_AddSprite(&(ClientGameSprite) {
        .atlasImage = cg_sprite_bubble,
        .origin = Vec3_Add(pos, Vec3_RandomRange(-2.f, 2.f)),
        .velocity = Vec3_Add(Vec3_Add(Vec3_Scale(forward, 2.f), Vec3_RandomRange(-5.f, 5.f)), MakeVec3(0.f, 0.f, 6.f)),
        .acceleration.z = 10.f,
        .lifetime = 1000 - Randomf() * 100,
        .size = RandomRangef(2.f, 3.f),
        .color = MakeVec3(1.f, 1.f, 1.f),
        .lighting = 1.f
      });

      ent->timestamp = cgi.client->unclampedTime + 800;
    }
  }
}

/**
 * @brief Emits a flame sprite at the trail endpoint for fire trail effects.
 */
void Cg_FlameTrail(ClientEntity *ent, const Vec3 start, const Vec3 end) {

  const float hue = RandomRangef(50.f, 60.f);
  const Vec3 color = ColorHSV(hue, 1.f, 1.f).vec3;

  ClientGameSprite *s = Cg_AddSprite(&(ClientGameSprite) {
    .atlasImage = cg_sprite_flame,
    .origin = end,
    .velocity = Vec3_RandomRange(-4.f, 4.f),
    .acceleration.z = 15.f,
    .lifetime = 500 + Randomf() * 250,
    .size = RandomRangef(4.f, 12.f),
    .color = color,
  });

  // make static flames rise
  if (ent && s) {
    if (Vec3_Equal(ent->current.origin, ent->prev.origin)) {
      s->lifetime /= .65f;
      s->acceleration.z = 20.f;
    }
  }
}

/**
 * @brief Think callback that stops a bubble sprite once it exits liquid.
 */
static void Cg_BubbleTrail_Think(ClientGameSprite *s, float life, float delta) {

  if (cgi.PointContents(s->origin) & CONTENTS_MASK_LIQUID) {
    return;
  }

  s->velocity = Vec3_Zero();
  s->acceleration = Vec3_Zero();

  s->lifetime = Mini(cgi.client->unclampedTime + 100 - s->time, s->lifetime);
}

/**
 * @brief Emits bubble sprites along the portion of a trail that passes through liquid.
 */
void Cg_BubbleTrail(ClientEntity *ent, const Vec3 start, const Vec3 end, float freq) {

  Vec3 origin = start;
  const int32_t count = Cg_TrailCount(end, freq, ent, TRAIL_BUBBLE, &origin, NULL);

  if (!count) {
    return;
  }

  const float step = 1.f / count;

  for (int32_t i = 0; i <= count; i++) {

    const Vec3 pos = Vec3_Mix(end, origin, step * i);

    const int32_t contents = cgi.PointContents(pos);
    if (!(contents & CONTENTS_MASK_LIQUID)) {
      continue;
    }

    const float v = RandomRangef(.6f, 1.f);

    ClientGameSprite *s = Cg_AddSprite(&(ClientGameSprite) {
      .atlasImage = cg_sprite_bubble,
      .origin = Vec3_Add(pos, Vec3_RandomRange(-2.f, 2.f)),
      .velocity = Vec3_Add(Vec3_RandomRange(-5.f, 5.f), MakeVec3(0.f, 0.f, 6.f)),
      .acceleration = Vec3_Add(Vec3_RandomRange(-4.f, 4.f), MakeVec3(0.f, 0.f, 14.f)),
      .lifetime = 2000 - (Randomf() * 500),
      .size = RandomRangef(.5f, 1.f),
      .rotation = Randomf(),
      .color = MakeVec3(v, v, v),
      .lighting = 1.f,
      .Think = Cg_BubbleTrail_Think,
    });

    if (!s) {
      break;
    }

    if (contents & CONTENTS_LAVA) {
      s->velocity = Vec3_Scale(s->velocity, .33f);
      s->lifetime *= .33f;
    } else if (contents & CONTENTS_SLIME) {
      s->velocity = Vec3_Scale(s->velocity, .66f);
      s->lifetime *= .66f;
    }

    s->sizeVelocity = -s->size / MILLIS_TO_SECONDS(s->lifetime);
  }
}

/**
 * @brief Renders the blaster projectile trail with energy particles and a dynamic light.
 */
static void Cg_BlasterTrail(ClientEntity *ent, const Vec3 start, const Vec3 end) {

  const Vec3 color = Cg_ClientEffectColor(ent->current.client, NULL, color_hue_orange);

  const int32_t liquid = Cg_TrailContents(start, end) & CONTENTS_MASK_LIQUID;
  if (liquid) {
    Cg_BubbleTrail(ent, start, end, 2.f);
  }

  Vec3 org, dir;
  const int32_t count = Cg_TrailCount(end, 8.f, ent, TRAIL_PRIMARY, &org, &dir);

  if (count) {

    for (int32_t i = 0; i <= count; i++) {

      if (liquid && (i & 1)) {
        continue;
      }

      const float scale = 7.f;
      const float power = 3.f;
      const Vec3 pdir = Vec3_Normalize(Vec3_RandomRange(-1.f, 1.f));
      const Vec3 porg = Vec3_Scale(pdir, powf(Randomf(), power) * scale);
      const float pdist = Vec3_Distance(Vec3_Zero(), porg) / scale;

      if (!Cg_AddSprite(&(ClientGameSprite) {
        .atlasImage = cg_sprite_particle,
        .lifetime = 1000,
        .velocity = Vec3_Scale(pdir, pdist * 10.f),
        .origin = Vec3_Add(porg, Vec3_Mix(end, org, 1.0f / i)),
        .size = Maxf(1.85f, powf(1.85f - pdist, power)),
        .sizeVelocity = Mixf(-3.5f, -.2f, pdist) * RandomRangef(.66f, 1.f),
        .color = color,
        .lighting = .3f,
      })) {
        break;
      }
    }
  }

  cgi.AddSprite(cgi.view, &(RenderSprite) {
    .media = (RenderMedia *) cg_sprite_particle,
    .origin = end,
    .size = 8.f,
    .color = color,
  });

  Cg_AddLight(&(ClientGameLight) {
    .origin = end,
    .radius = 200.f,
    .color = color,
    .intensity = 2.5f,
  });
}

/**
 * @brief Renders the grenade projectile trail with smoke and a pulsing dynamic light.
 */
static void Cg_GrenadeTrail(ClientEntity *ent, const Vec3 start, const Vec3 end) {

  if (Cg_TrailContents(start, end) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(ent, start, end, 6.f);
  }

  // Smoke trail
  Vec3 origin, dir;
  const int32_t count = Cg_TrailCount(end, 12.f, ent, TRAIL_PRIMARY, &origin, &dir);

  if (count) {
    const float step = 1.f / count;

    for (int32_t i = 0; i <= count; i++) {
      Cg_AddSprite(&(ClientGameSprite) {
        .atlasImage = cg_sprite_smoke,
        .origin = Vec3_Mix(end, origin, (step * i) + RandomRangef(-.5f, .5f)),
        .velocity = MakeVec3(RandomRangef(-5.f, 5.f), RandomRangef(-5.f, 5.f), RandomRangef(10.f, 20.f)),
        .lifetime = RandomRangef(600.f, 900.f),
        .color = MakeVec3(.6f, .6f, .6f),
        .size = 1.5f,
        .sizeVelocity = 10.f,
        .rotation = RandomRadian(),
        .rotationVelocity = RandomRangef(.2f, .8f),
        .lighting = 1.f,
      });
    }
  }

  // Dynamic lights — green glow with red detonator blink
  const float pulse = sinf(cgi.client->unclampedTime * .02f) * .5f + .5f;

  Cg_AddLight(&(ClientGameLight) {
    .origin = end,
    .radius = 150.f + 30.f * pulse,
    .color = MakeVec3(.2f, .8f, .2f),
    .intensity = 2.f,
    .source = ent,
  });

  Cg_AddLight(&(ClientGameLight) {
    .origin = end,
    .radius = 60.f + 20.f * (1.f - pulse),
    .color = MakeVec3(.8f, .1f, .05f),
    .intensity = 2.f * (1.f - pulse),
    .source = ent,
  });
}

/**
 * @brief Renders the Quake grenade projectile trail with smoke and a warm fuse-ember glow.
 */
static void Cg_QuakeGrenadeTrail(ClientEntity *ent, const Vec3 start, const Vec3 end) {

  if (Cg_TrailContents(start, end) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(ent, start, end, 6.f);
  }

  // Smoke trail
  Vec3 origin, dir;
  const int32_t count = Cg_TrailCount(end, 12.f, ent, TRAIL_PRIMARY, &origin, &dir);

  if (count) {
    const float step = 1.f / count;

    for (int32_t i = 0; i <= count; i++) {
      Cg_AddSprite(&(ClientGameSprite) {
        .atlasImage = cg_sprite_smoke,
        .origin = Vec3_Mix(end, origin, (step * i) + RandomRangef(-.5f, .5f)),
        .velocity = MakeVec3(RandomRangef(-5.f, 5.f), RandomRangef(-5.f, 5.f), RandomRangef(10.f, 20.f)),
        .lifetime = RandomRangef(600.f, 900.f),
        .color = MakeVec3(.6f, .6f, .6f),
        .size = 1.5f,
        .sizeVelocity = 10.f,
        .rotation = RandomRadian(),
        .rotationVelocity = RandomRangef(.2f, .8f),
        .lighting = 1.f,
      });
    }
  }

  // Dynamic light — warm orange ember pulse from the burning fuse
  const float pulse = sinf(cgi.client->unclampedTime * .025f) * .5f + .5f;

  Cg_AddLight(&(ClientGameLight) {
    .origin = end,
    .radius = 120.f + 40.f * pulse,
    .color = MakeVec3(.9f, .5f, .1f),
    .intensity = 1.5f + .5f * pulse,
    .source = ent,
  });
}

static void Cg_FireFlyTrail_Think(ClientGameSprite *sprite, float life, float delta) {

  sprite->velocity = Vec3_Fmaf(sprite->velocity, delta * 1000.f, Vec3_RandomDir());
}

/**
 * @brief Renders the rocket projectile trail with flame, smoke, and firefly spark effects.
 */
static void Cg_RocketTrail(ClientEntity *ent, const Vec3 start, const Vec3 end) {
  int32_t count;
  Vec3 origin;

  float sine = sinf(cgi.client->unclampedTime * .001f) * M_PI;

  const Vec3 velocity = Vec3_Subtract(ent->current.origin, ent->prev.origin);
  const Vec3 direction = Vec3_Normalize(velocity);
  const float speed = Vec3_Length(velocity) / QUETOO_TICK_SECONDS;

  // exhaust glow
  cgi.AddSprite(cgi.view, &(RenderSprite) {
    .media = (RenderMedia *) cg_sprite_explosion_glow,
    .origin = Vec3_Fmaf(ent->origin, -20.f, direction),
    .size = 50.f,
    .color = ColorHSV(29.f, .57f, .34f).vec3,
  });

  // exhaust flare
  for (int32_t i = 0; i < 2; i++) {
    cgi.AddSprite(cgi.view, &(RenderSprite) {
      .media = (RenderMedia *) cg_sprite_explosion_flash,
      .origin = Vec3_Fmaf(ent->origin, -20.f, direction),
      .size = 35.f,
      .color = ColorHSV(0.f, 0.f, .50f).vec3,
      .rotation = (i == 0 ? sine : -sine),
    });
  }

  const int32_t liquid = Cg_TrailContents(start, end) & CONTENTS_MASK_LIQUID;
  if (liquid) {
    Cg_BubbleTrail(ent, start, end, 1.f);
  }

  // exhaust flames
  count = Cg_TrailCount(end, 12.f, ent, TRAIL_PRIMARY, &origin, NULL);
  if (count && !liquid) {
    const float step = 1.f / count;
    float lifeStart, lifeFrac;

    Cg_ParticleTrailLifeOffset(start, origin, speed, step, &lifeStart, &lifeFrac);

    for (int32_t i = 0; i < count; i++) {
      const float particleLifeFrac = lifeStart + (lifeFrac * (i + 1));

      // fire
      if (!Cg_AddSprite(&(ClientGameSprite) {
          .animation = cg_sprite_rocket_flame,
          .lifetime = Cg_AnimationLifetime(cg_sprite_rocket_flame, 90) * particleLifeFrac,
          .origin = Vec3_Mix(start, origin, step * i),
          .velocity = velocity,
          .rotation = RandomRadian(),
          .size = 10.f,
          .sizeVelocity = -20.f,
          .color = MakeVec3(1.f, 1.f, 1.f),
        })) {
        break;
      }
    }
  }

  // smoke trail
  count = Cg_TrailCount(end, 16.f, ent, TRAIL_SECONDARY, &origin, NULL);
  if (count && !liquid) {
    const float step = 1.f / count;
    float lifeStart, lifeFrac;

    Cg_ParticleTrailLifeOffset(start, origin, speed, step, &lifeStart, &lifeFrac);

    for (int32_t i = 0; i < count; i++) {
      const float particleLifeFrac = lifeStart + (lifeFrac * (i + 1));

      // interlace smoke 1 and 2 for some subtle variety
      // smoke 1
      if (!Cg_AddSprite(&(ClientGameSprite) {
          .animation = cg_sprite_smoke_04,
          .lifetime = Cg_AnimationLifetime(cg_sprite_smoke_04, 60) * particleLifeFrac,
          .origin = Vec3_Add(Vec3_Mix(start, origin, step * i), Vec3_RandomRange(-2.5f, 2.5f)),
          .velocity = Vec3_Scale(velocity, 0.5),
          .rotation = RandomRadian(),
          .size = Randomf() * 5.f + 10.f,
          .sizeVelocity = Randomf() * 5.f + 10.f,
          .color = MakeVec3(.5f, .5f, .5f),
          .lighting = 1.f
        })) {
        break;
      }

      // smoke 2
      if (!Cg_AddSprite(&(ClientGameSprite) {
          .animation = cg_sprite_smoke_05,
          .lifetime = Cg_AnimationLifetime(cg_sprite_smoke_05, 60) * particleLifeFrac,
          .origin = Vec3_Add(Vec3_Mix(start, origin, (step * i) + (step * .5f)), Vec3_RandomRange(-2.5f, 2.5f)),
          .velocity = Vec3_Scale(velocity, 0.5),
          .rotation = RandomRadian(),
          .size = Randomf() * 5.f + 10.f,
          .sizeVelocity = Randomf() * 5.f + 10.f,
          .color = MakeVec3(.5f, .5f, .5f),
          .lighting = 1.f
        })) {
        break;
      }
    }
  }

  // firefly trail
  count = Cg_TrailCount(end, 10.f, ent, TRAIL_TERTIARY, &origin, NULL);
  if (count) {
    const float step = 1.f / count;
    float lifeStart, lifeFrac;

    Cg_ParticleTrailLifeOffset(start, origin, speed, step, &lifeStart, &lifeFrac);

    for (int32_t i = 0; i < count; i++) {
      const float particleLifeFrac = lifeStart + (lifeFrac * (i + 1));

      // sparks
      if (!Cg_AddSprite(&(ClientGameSprite) {
          .atlasImage = cg_sprite_particle,
          .lifetime = RandomRangef(900.f, 1300.f) * particleLifeFrac,
          .origin = Vec3_Mix(start, origin, step * i),
          .velocity = Vec3_RandomRange(-10.f, 10.f),
          .acceleration = Vec3_RandomRange(-10.f, 10.f),
          .size = Randomf() * 1.6f + 1.6f,
          .Think = Cg_FireFlyTrail_Think,
          .color = MakeVec3(1.f, .75f, 0.f),
        })) {
        break;
      }
    }
  }

  Cg_AddLight(&(ClientGameLight) {
    .origin = end,
    .radius = 240.f,
    .color = MakeVec3(.8f, .5f, .2f),
    .intensity = 2.f,
    .source = ent,
  });
}

/**
 * @brief Renders the hyperblaster projectile trail with plasma glow and a beam sprite.
 */
static void Cg_HyperblasterTrail(ClientEntity *ent, Vec3 start, Vec3 end) {

  if (Cg_TrailContents(start, end) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(ent, start, end, 1.f);
  }

  const Vec3 color = ColorHSV(204.f, .8f, 1.f).vec3;
  const Vec3 coreColor = MakeVec3(.5f, .85f, 1.f);

  Vec3 dir = Vec3_Direction(start, end);

  RenderAtlasImage *variation[] = {
    cg_sprite_plasma_var01,
    cg_sprite_plasma_var02,
    cg_sprite_plasma_var03
  };

  // outer rim
  if (ent->timestamp < cgi.client->unclampedTime) {
    for (int32_t i = 0; i < 3; i++) {
      Cg_AddSprite(&(ClientGameSprite) {
        .atlasImage = variation[i],
        .size = RandomRangef(12.f, 18.f),
        .rotation = RandomRadian(),
        .lifetime = 100.f,
        .color = color,
        .lighting = .15f,
        .flags = SPRITE_FOLLOW_ENTITY | SPRITE_ENTITY_UNLINK_ON_DEATH,
        .entity = Cg_GetSpriteEntity(ent)
      });
    }
    ent->timestamp = cgi.client->unclampedTime + 32;
  }

  // center blob
  cgi.AddSprite(cgi.view, &(RenderSprite) {
    .media = (RenderMedia *) cg_sprite_blob_01,
    .origin = ent->origin,
    .size = RandomRangef(14.f, 18.f),
    .rotation = RandomRadian(),
    .color = coreColor,
    .lighting = .1f,
  });

  // center core (bright hot spot)
  cgi.AddSprite(cgi.view, &(RenderSprite) {
    .media = (RenderMedia *) cg_sprite_particle,
    .origin = ent->origin,
    .size = RandomRangef(6.f, 9.f),
    .rotation = RandomRadian(),
    .color = MakeVec3(.8f, .95f, 1.f),
    .lighting = .0f,
  });

  cgi.AddBeam(cgi.view, &(RenderBeam) {
    .start = Vec3_Fmaf(end, 70.f, dir),
    .end = start,
    .color = coreColor,
    .image = cg_beam_tail,
    .size = 6.0f,
    .translate = cgi.client->unclampedTime * RandomRangef(.003f, .009f),
    .lighting = .5f,
  });

  Cg_AddLight(&(ClientGameLight) {
    .origin = ent->origin,
    .radius = 250.f,
    .color = MakeVec3(.4f, .7f, 1.f),
    .intensity = 3.f
  });
}

/**
 * @brief Renders the lightning beam with oscillating segments, sparks, and impact effects.
 */
static void Cg_LightningTrail(ClientEntity *ent, const Vec3 start, const Vec3 end) {

  Vec3 dir = Vec3_Direction(end, start);
  const float dist = Vec3_Distance(start, end);

  // World-space oscillation axes — reads clearly from any viewing angle
  const Vec3 worldUp = MakeVec3(0.f, 0.f, 1.f);
  const Vec3 worldRight = Vec3_Normalize(Vec3_Cross(dir, worldUp));

  // Break beam into oscillating segments
  const float segmentLength = 4.f;
  const int32_t numSegments = Maxi(1, (int32_t)(dist / segmentLength));
  const float translate = cgi.client->unclampedTime * .006f;

  // Smooth continuous time for fluid wave animation
  const float time = cgi.client->unclampedTime * .01f;

  // Precompute joint positions along the oscillating path
  Vec3 joints[numSegments + 1];
  joints[0] = start;
  joints[numSegments] = end;

  for (int32_t i = 1; i < numSegments; i++) {
    const float frac = (float)i / (float)numSegments;
    Vec3 point = Vec3_Mix(start, end, frac);

    // Taper amplitude — sharper falloff near endpoints
    const float envelope = sinf(M_PI * frac) * sinf(M_PI * frac);
    const float along = frac * dist;

    // Dominant up/down wave in world Z
    const float offsetZ = sinf(along * .056f - time * 3.f) * 12.f * envelope;

    // Small random sideways jitter in world horizontal
    const float hash = sinf(along * .37f + time * 7.f) * sinf(along * .71f - time * 3.3f);
    const float offsetH = hash * 5.f * envelope;
    point = Vec3_Fmaf(point, offsetH, worldRight);
    point.z += offsetZ;

    joints[i] = point;
  }

  // Draw segments with slight overlap to hide joints
  for (int32_t i = 0; i < numSegments; i++) {
    const Vec3 segStart = joints[i];
    const Vec3 segEnd = joints[i + 1];
    const Vec3 segDir = Vec3_Normalize(Vec3_Subtract(segEnd, segStart));

    // Extend each segment slightly to hide joints
    const Vec3 drawStart = i > 0 ? Vec3_Fmaf(segStart, -2.f, segDir) : segStart;
    const Vec3 drawEnd = i < numSegments - 1 ? Vec3_Fmaf(segEnd, 2.f, segDir) : segEnd;

    cgi.AddBeam(cgi.view, &(const RenderBeam) {
      .start = drawStart,
      .end = drawEnd,
      .color = MakeVec3(.85f, .85f, 1.f),
      .image = cg_beam_lightning,
      .size = 5.5f,
      .flags = SPRITE_BEAM_REPEAT,
      .translate = translate,
    });
  }

  // beam endpoint cap
  Cg_AddSprite(&(ClientGameSprite) {
    .atlasImage = cg_sprite_electro_02,
    .origin = Vec3_Fmaf(end, -10.f, dir),
    .lifetime = 30.f,
    .size = 50.f,
    .rotation = RandomRadian(),
    .color = MakeVec3(.75f, .75f, 1.f),
  });

  // lights and flying sparks
  const int32_t seed = (int32_t) cgi.client->unclampedTime % 96;
  for (float f = seed; f < Vec3_Distance(start, end); f += 128.f) {
    Cg_AddLight(&(const ClientGameLight) {
      .origin = Vec3_Fmaf(start, f + seed, dir),
      .radius = 192.f + RandomRangef(-32.f, 32.f),
      .color = MakeVec3(.7f, .35f, .75f),
      .intensity = 2.2f,
    });

    Cg_AddSprite(&(ClientGameSprite) {
      .atlasImage = cg_sprite_particle3,
      .origin = Vec3_Fmaf(start, f + seed, dir),
      .velocity = Vec3_Scale(Vec3_Add(dir, Vec3_RandomRange(-.2f, .2f)), RandomRangef(50, 200)),
      .acceleration.z = -SPRITE_GRAVITY * 3.0,
      .lifetime = 200 + Randomf() * 800,
      .bounce = 0.2f,
      .size = 1.f + RandomRangef(1.f, 2.f),
      .color = MakeVec3(.7f, .35f, .8f),
    });
  }

  if (ent->current.animation1 != LIGHTNING_SOLID_HIT) {
    return;
  }

  //hit face decal
  Cg_AddDecal(&(RenderDecal) {
      .image = cg_decal_bullet[Randomi() % lengthof(cg_decal_bullet)],
          .origin = end,
          .radius = RandomRangef(2.f, 4.f),
          .color = Color3f(0.02f, 0.01f, 0.02f),
          .lifetime = 7500 + Randomf() * 7500,
          .rotation = RandomRadian()
  });

  if (ent->timestamp < cgi.client->unclampedTime) {

    Vec3 dir;
    Vec3_Vectors(ent->angles, &dir, NULL, NULL);

    if ((cgi.PointContents(ent->termination) & CONTENTS_MASK_LIQUID) == 0) {

      // hit billboards
      for (int32_t i = 0; i < 2; i++) {
        Cg_AddSprite(&(ClientGameSprite) {
          .atlasImage = cg_sprite_electro_02,
          .origin = end,
          .lifetime = 60 * (i + 1),
          .size = RandomRangef(100.f, 200.f),
          .sizeVelocity = 400.f,
          .rotation = RandomRadian(),
          .dir = Vec3_RandomRange(-1.f, 1.f),
          .color = MakeVec3(.75f, .75f, 1.f),
        });
      }

      // hit decal
      Cg_AddSprite(&(ClientGameSprite) {
        .atlasImage = cg_sprite_electro_02,
        .origin = Vec3_Add(end, dir),
        .lifetime = 120,
        .size = RandomRangef(100.f, 200.f),
        .sizeVelocity = 400.f,
        .rotation = RandomRadian(),
        .dir = dir,
        .color = MakeVec3(.75f, .75f, 1.f),
      });

      // hit sparks
      for (int32_t i = 0; i < 2; i++) {
        Cg_AddSprite(&(ClientGameSprite) {
          .atlasImage = cg_sprite_particle3,
          .origin = end,
          .velocity = Vec3_Scale(Vec3_Add(dir, Vec3_RandomRange(-.2f, .2f)), RandomRangef(50, 200)),
          .acceleration.z = -SPRITE_GRAVITY * 3.0,
          .lifetime = 200 + Randomf() * 800,
          .bounce = 0.2f,
          .size = 2.0f + RandomRangef(1.0f, 2.0f),
          .color = MakeVec3(.7f, .35f, .8f),
        });
      }
    }

    ent->timestamp = cgi.client->unclampedTime + 25; // 40hz
  }
}

#if defined(G_HOOK)
/**
 * @brief Renders the grappling hook cable as a beam between the entity and its attachment point.
 */
static void Cg_HookTrail(ClientEntity *ent, const Vec3 start, const Vec3 end) {

  Vec3 forward;
  Vec3_Vectors(ent->angles, &forward, NULL, NULL);

  const Vec3 color = Cg_ClientEffectColor(ent->current.client, NULL, color_hue_green);

  cgi.AddBeam(cgi.view, &(const RenderBeam) {
    .start = start,
    .end = Vec3_Fmaf(end, -3.f, forward),
    .color = color,
    .image = cg_beam_hook,
    .size = 1.f,
    .flags = SPRITE_BEAM_REPEAT,
  });
}
#endif

#define BFG_BALLS_SPEED  400.f

/**
 * @brief Think callback that applies centripetal acceleration to orbiting BFG ball sprites.
 */
static void Cg_BfgTrail_Think(ClientGameSprite *sprite, float life, float delta) {

  if (!(sprite->flags & SPRITE_FOLLOW_ENTITY)) {
    sprite->Think = NULL;
    sprite->acceleration = MakeVec3(0.f, 0.f, -3.f * SPRITE_GRAVITY);
    const float lifetime = RandomRangef(2000, 3500);
    sprite->lifetime = lifetime;
    sprite->sizeVelocity = -sprite->size / MILLIS_TO_SECONDS(lifetime);
    sprite->time = sprite->timestamp = cgi.client->unclampedTime;
    sprite->bounce = .2f;
    return;
  }

  float length;
  
  sprite->acceleration = Vec3_NormalizeLength(sprite->origin, &length);
  sprite->acceleration = Vec3_Scale(sprite->acceleration, -length * (BFG_BALLS_SPEED * QUETOO_TICK_SECONDS));

  sprite->size = Clampf((1.0 - (length / 100.f)) * 24.f, 6.f, 24.f);

  length = Vec3_Length(sprite->velocity);

  if (length > BFG_BALLS_SPEED) {
    sprite->velocity = Vec3_Scale(Vec3_Normalize(sprite->velocity), BFG_BALLS_SPEED);
  }
}

/**
 * @brief Renders the BFG projectile trail with a core sphere and orbiting ball sprites.
 */
static void Cg_BfgTrail(ClientEntity *ent, const Vec3 start, const Vec3 end) {
  const float mod = fmodf((float)cgi.client->unclampedTime, 100.f) / 100.f;

  if (Cg_TrailContents(start, end) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(ent, ent->prev.origin, ent->origin, .5f);
  }

  // projectile core
  cgi.AddSprite(cgi.view, &(RenderSprite) {
    .origin = ent->origin,
    .size = 100.f,
    .media = (RenderMedia *) cg_sprite_hyperball_01,
    .rotation = mod * 200.f * M_PI,
    .color = MakeVec3(1.f, 1.f, 1.f),
    .life = fmod(cgi.client->unclampedTime * 0.001f, 1.0f),
  });

  if (ent->timestamp < cgi.client->unclampedTime) {
    ent->timestamp = cgi.client->unclampedTime + 4;
  
    Cg_AddSprite(&(ClientGameSprite) {
      .atlasImage = cg_sprite_particle,
      .origin = Vec3_Zero(),
      .size = 6.f,
      .color = MakeVec3(.5f, 1.f, 0.5f),
      .lifetime = 1000,
      .flags = SPRITE_FOLLOW_ENTITY | SPRITE_DATA_NOFREE | SPRITE_ENTITY_UNLINK_ON_DEATH,
      .bounce = 1.0f,
      .Think = Cg_BfgTrail_Think,
      .entity = Cg_GetSpriteEntity(ent),
      .data = ent,
      .velocity = Vec3_Scale(Vec3_RandomDir(), BFG_BALLS_SPEED)
    });
  }

  Cg_AddLight(&(ClientGameLight) {
    .origin = ent->origin,
    .radius = 240.0,
    .color = MakeVec3(.4f, 1.f, .4f),
    .intensity = 2.f,
  });
}

/**
 * @brief Think function for teleporter helix sprites.
 */
static void Cg_TeleporterTrail_Think(ClientGameSprite *sprite, float life, float delta) {

  const float t = MILLIS_TO_SECONDS(cgi.client->unclampedTime);
  const float phase = sprite->rotation;
  const float orbitSpeed = 5.f;
  const float maxRadius = 20.f;
  const float radius = maxRadius * (1.f - life * .6f);

  const float angle = phase + t * orbitSpeed;
  sprite->origin.x = cosf(angle) * radius;
  sprite->origin.y = sinf(angle) * radius;
  sprite->origin.z = -16.f + life * 56.f;

  sprite->size = (2.5f + sinf(t * 6.f + phase) * .5f) * (1.f - life * .4f);
}

static void Cg_TeleporterTrail(ClientEntity *ent) {

  const Vec3 gold = ColorHSV(color_hue_yellow, .7f, 1.f).vec3;
  const float t = MILLIS_TO_SECONDS(cgi.client->unclampedTime);

  Cg_AddSprite(&(ClientGameSprite) {
    .atlasImage = cg_sprite_teleport_core,
    .origin = Vec3_Fmaf(ent->origin, 8.f, Vec3_Up()),
    .size = 64.f,
    .color = Vec3_Scale(gold, .7f + sinf(t * 3.f) * .15f),
  });

  // Helix sprites
  if (ent->timestamp <= cgi.client->unclampedTime) {
    ent->timestamp = cgi.client->unclampedTime + 32;

    Cg_AddSprite(&(ClientGameSprite) {
      .atlasImage = cg_sprite_spark,
      .origin = Vec3_Zero(),
      .size = 2.5f,
      .color = gold,
      .endColor = MakeVec3(1.f, 1.f, .8f),
      .lifetime = 1200,
      .rotation = RandomRangef(0.f, 2.f * M_PI),
      .flags = SPRITE_FOLLOW_ENTITY | SPRITE_DATA_NOFREE,
      .Think = Cg_TeleporterTrail_Think,
      .entity = Cg_GetSpriteEntity(ent),
      .data = ent,
      .lighting = .25f,
    });
  }

  // Rising rings
  if ((cgi.client->unclampedTime % 200) < cgi.client->frameMsec) {
    Cg_AddSprite(&(ClientGameSprite) {
      .atlasImage = cg_sprite_ring,
      .dir = Vec3_Up(),
      .origin = Vec3_Fmaf(ent->origin, 16.f, Vec3_Down()),
      .velocity.z = RandomRangef(60.f, 100.f),
      .lifetime = 600,
      .size = 48.f,
      .sizeVelocity = 32.f,
      .color = gold,
      .endColor = Vec3_Scale(gold, .2f),
    });
  }

  // Pulsating light
  const float pulse = 1.5f + sinf(t * 4.f) * .5f;
  Cg_AddLight(&(const ClientGameLight) {
    .origin = ent->origin,
    .radius = 150.0,
    .color = gold,
    .intensity = pulse,
    .source = ent,
  });
}

/**
 * @brief Returns a sinusoidally oscillating value at the given frequency, amplitude, base, and phase.
 */
static inline float Cg_Oscillate(const float freq, const float amplitude, const float base, const float phase) {
  const float seconds = MILLIS_TO_SECONDS(cgi.client->unclampedTime);
  return base + sinf((phase + seconds * 2 * freq * 2)) * (amplitude * 0.5);
}

/**
 * @brief Renders a pulsing ring sprite beneath a player spawn point to mark its location.
 */
static void Cg_PlayerSpawnTrail(const ClientEntity *ent) {

  const Color color = Color32_Color(ent->current.color);
  const Vec3 rgb = color.r > 0.f || color.g > 0.f || color.b > 0.f
    ? color.vec3
    : ColorHSV(color_hue_yellow, 1.f, 1.f).vec3;

  cgi.AddSprite(cgi.view, &(RenderSprite) {
    .media = (RenderMedia *) cg_sprite_ring,
    .origin = Vec3_Fmaf(ent->origin, 16.f, Vec3_Down()),
    .size = 48.f + Cg_Oscillate(1, 12.f, 1.f, 0.f),
    .color = rgb,
    .dir = Vec3_Up()
  });
}

/**
 * @brief Renders a gib trail with blood sprites and decals along the trajectory.
 */
static void Cg_GibTrail(ClientEntity *ent, const Vec3 start, const Vec3 end) {

  if (cgi.PointContents(end) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(ent, start, end, 4.f);
    return;
  }

  Vec3 origin, dir;
  const int32_t count = Cg_TrailCount(end, 8.f, ent, TRAIL_PRIMARY, &origin, &dir);

  if (!count) {
    return;
  }

  float step = 1.f / count;

  for (int32_t i = 0; i <= count; i++) {

    if (!Cg_AddSprite(&(ClientGameSprite) {
        .animation = cg_sprite_blood_01,
        .lifetime = Cg_AnimationLifetime(cg_sprite_blood_01, 30) + Randomf() * 500,
        .size = RandomRangef(40.f, 64.f),
        .rotation = RandomRadian(),
        .origin = Vec3_Mix(end, origin, step * i),
        .velocity = Vec3_Scale(dir, 20.0),
        .acceleration.z = -SPRITE_GRAVITY / 2.0,
        .color = MakeVec3(1.f, 1.f, .1f),
        .lighting = 1.f
      })) {
      break;
    }

    Cg_AddDecal(&(RenderDecal) {
      .image = cg_decal_blood[Randomi() % lengthof(cg_decal_blood)],
      .origin = Vec3_Mix(end, origin, step * i),
      .radius = RandomRangef(8.f, 32.f),
      .color = color_red,
      .lifetime = 8000 + Randomf() * 4000,
      .rotation = RandomRadian()
    });
  }
}

/**
 * @brief Renders a laser beam between its emitter and whatever it lands on, in the color the
 * entity carries, with a light and a scorch mark at the far end.
 */
static void Cg_LaserTrail(ClientEntity *ent, const Vec3 start, const Vec3 end) {

  const Vec3 color = Color32_Vec4(ent->current.color).xyz;

  // a wide, dim sheath around a bright core, which is what gives the BFG's lasers their heft
  cgi.AddBeam(cgi.view, &(const RenderBeam) {
    .start = start,
    .end = end,
    .color = Vec3_Scale(color, .4f),
    .image = cg_beam_rail,
    .size = 14.f,
    .lighting = .5f,
  });

  cgi.AddBeam(cgi.view, &(const RenderBeam) {
    .start = start,
    .end = end,
    .color = color,
    .image = cg_beam_rail,
    .size = 5.f,
    .lighting = .5f,
  });

  Cg_AddLight(&(ClientGameLight) {
    .origin = end,
    .radius = 150.f,
    .color = color,
    .intensity = 3.f,
    .decay = 50,
  });

  if (Cg_TrailCount(end, 24.f, ent, TRAIL_PRIMARY, NULL, NULL)) {

    Cg_AddSprite(&(ClientGameSprite) {
      .atlasImage = cg_sprite_particle,
      .lifetime = 300,
      .size = RandomRangef(4.f, 8.f),
      .rotation = RandomRadian(),
      .origin = end,
      .velocity = Vec3_RandomRange(-20.f, 20.f),
      .color = color,
    });
  }
}

/**
 * @brief Renders the nail projectile trail as a thin metallic streak.
 */
static void Cg_NailTrail(ClientEntity *ent, const Vec3 start, const Vec3 end) {

  float len;
  const Vec3 dir = Vec3_NormalizeLength(Vec3_Subtract(end, start), &len);

  if (len < 1.f) {
    return;
  }

  if (Cg_TrailContents(start, end) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(ent, start, end, 2.f);
  }

  cgi.AddBeam(cgi.view, &(RenderBeam) {
    .start = Vec3_Fmaf(end, -Minf(len, 40.f), dir),
    .end = end,
    .color = MakeVec3(1.f, .9f, .6f),
    .image = cg_beam_tracer,
    .size = 1.5f,
  });
}

/**
 * @brief Renders the fireball projectile trail with flame sprites and a decaying dynamic light.
 */
static void Cg_FireballTrail(ClientEntity *ent, const Vec3 start, const Vec3 end) {

  if (cgi.PointContents(end) & CONTENTS_MASK_LIQUID) {
    return;
  }

  ClientGameLight l = {
    .origin = end,
    .radius = 185.f,
    .color = MakeVec3(.9f, .3f, .1f),
    .intensity = 3.f,
    .source = ent
  };

  if (ent->current.effects & EF_DESPAWN) {
    const float decay = Clampf01((cgi.client->unclampedTime - ent->timestamp) / 1000.f);
    l.radius *= (1.f - decay);
  } else {

    Vec3 origin, dir;
    const int32_t count = Cg_TrailCount(end, 8.f, ent, TRAIL_SECONDARY, &origin, &dir);
    if (count) {
      const float step = 1.f / count;
      for (int32_t i = 0; i <= count; i++) {
        Cg_AddSprite(&(ClientGameSprite) {
          .atlasImage = cg_sprite_smoke,
          .origin = Vec3_Mix(end, origin, (step * i) + RandomRangef(-.5f, .5f)),
          .velocity = Vec3_Scale(dir, RandomRangef(20.f, 30.f)),
          .acceleration = Vec3_Scale(dir, -20.f),
          .lifetime = RandomRangef(600.f, 900.f),
          .color = MakeVec3(.4f, .25f, .15f),
          .size = 2.5f,
          .sizeVelocity = 15.f,
          .rotation = RandomRangef(.0f, M_PI),
          .rotationVelocity = RandomRangef(.2f, 1.f),
          .lighting = .5f,
        });
      }
    }

    Cg_FlameTrail(ent, start, end);
    ent->timestamp = cgi.client->unclampedTime;
  }

  Cg_AddLight(&l);
}

#if defined(G_CTF)
/**
 * @brief Think function for orbiting powerup trail sprites.
 */
static void Cg_OrbitTrail_Think(ClientGameSprite *sprite, float life, float delta) {

  if (!(sprite->flags & SPRITE_FOLLOW_ENTITY)) {
    sprite->Think = NULL;
    sprite->velocity = MakeVec3(0.f, 0.f, RandomRangef(20.f, 60.f));
    sprite->sizeVelocity = -sprite->size / MILLIS_TO_SECONDS(sprite->lifetime);
    sprite->time = sprite->timestamp = cgi.client->unclampedTime;
    sprite->lifetime = RandomRangeu(500, 1000);
    return;
  }

  const float t = MILLIS_TO_SECONDS(cgi.client->unclampedTime);
  const float phase = sprite->rotation;
  const float orbitSpeed = 3.f;
  const float radius = 24.f;
  const float bobSpeed = 2.f;

  const float angle = phase + t * orbitSpeed;
  sprite->origin.x = cosf(angle) * radius;
  sprite->origin.y = sinf(angle) * radius;
  sprite->origin.z = 16.f + sinf(phase * 3.f + t * bobSpeed) * 12.f;

  sprite->size = 2.f + sinf(t * 5.f + phase) * .5f;
}
#endif

#if defined(G_CTF)
/**
 * @brief Spawns orbiting sprites and legacy particle trail for powerup effects.
 */
static void Cg_OrbitalTrail(ClientEntity *ent, const Vec3 start, const Vec3 end, const Vec3 color) {

  // Orbiting sprites: spawn a few at staggered intervals
  if (ent->timestamp < cgi.client->unclampedTime) {
    ent->timestamp = cgi.client->unclampedTime + 80;

    const float phase = RandomRangef(0.f, 2.f * M_PI);

    Cg_AddSprite(&(ClientGameSprite) {
      .atlasImage = cg_sprite_spark,
      .origin = Vec3_Zero(),
      .size = 2.5f,
      .color = color,
      .endColor = MakeVec3(1.f, 1.f, 1.f),
      .lifetime = 2000,
      .rotation = phase,
      .flags = SPRITE_FOLLOW_ENTITY | SPRITE_DATA_NOFREE | SPRITE_ENTITY_UNLINK_ON_DEATH,
      .Think = Cg_OrbitTrail_Think,
      .entity = Cg_GetSpriteEntity(ent),
      .data = ent,
      .lighting = .25f
    });
  }

  // Trailing particles for motion streak
  const int32_t count = Cg_TrailCount(end, 4.f, ent, TRAIL_PRIMARY, NULL, NULL);
  for (int32_t i = 0; i < count; i++) {

    if (!Cg_AddSprite(&(ClientGameSprite) {
      .atlasImage = cg_sprite_particle,
      .lifetime = RandomRangeu(300, 800),
      .size = RandomRangef(1.f, 2.f),
      .sizeVelocity = RandomRangef(-3.f, -1.f),
      .origin = Vec3_Add(start, Vec3_RandomRanges(-12.f, 12.f, -12.f, 12.f, 8.f, 28.f)),
      .velocity = Vec3_RandomRanges(-8.f, 8.f, -8.f, 8.f, 10.f, 30.f),
      .friction = 30.f,
      .color = color,
      .endColor = Vec3_Scale(color, .3f),
      .lighting = .5f
    })) {
      break;
    };
  }
}
#endif

#if defined(G_CTF)
/**
 * @brief Renders the CTF flag carrier orbital particle trail in the carrier's team color.
 */
static void Cg_CtfEffectTrail(ClientEntity *ent, const Vec3 start, const Vec3 end) {

  const ClientGameTeamInfo *team = cg_state.teams;
  for (size_t i = 0; i < lengthof(cg_state.teams); i++, team++) {
    if (ent->current.effects & (EF_CTF_RED << i)) {
      break;
    }
  }
  assert(team);

  Cg_OrbitalTrail(ent, start, end, ColorHSV(team->hue, 1.f, 1.f).vec3);
}
#endif

/**
 * @brief Apply unique trails to entities between their previous packet origin
 * and their current interpolated origin. Beam trails are a special case: the
 * old origin field is overridden to specify the endpoint of the beam.
 */
void Cg_EntityTrail(ClientEntity *ent) {
  const EntityState *s = &ent->current;

  Vec3 start, end;
  start = ent->previousOrigin;

  // beams have two origins, most entities have just one
  if (s->effects & EF_BEAM) {

    end = ent->termination;

    // client is overridden to specify owner of the beam
    if (ent->current.client == cgi.client->frame.ps.client && !cgi.client->thirdPerson) {

      // we own this beam (lightning, grapple, etc..)
      // anchor start to the client-side muzzle; keep end as the server-authoritative termination
      start = cg_state.clients[ent->current.client].weaponMuzzle;

#if defined(G_HOOK)
      if (s->trail == TRAIL_HOOK) {
        // the grapple cable reads better leaving the player than the view
        // weapon muzzle, which floats well ahead of the eye and shifts with
        // cg_fov (#867)
        start = Cg_Self()->origin;
      }
#endif
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
    case TRAIL_QUAKE_GRENADE:
      Cg_QuakeGrenadeTrail(ent, start, end);
      break;
    case TRAIL_LASER:
      Cg_LaserTrail(ent, start, end);
      break;
    case TRAIL_ROCKET:
      Cg_RocketTrail(ent, start, end);
      break;
    case TRAIL_HYPERBLASTER:
      Cg_HyperblasterTrail(ent, start, end);
      break;
    case TRAIL_LIGHTNING:
      Cg_LightningTrail(ent, start, end);
      break;
#if defined(G_HOOK)
    case TRAIL_HOOK:
      Cg_HookTrail(ent, start, end);
      break;
#endif

    case TRAIL_BFG:
      Cg_BfgTrail(ent, start, end);
      break;
    case TRAIL_TELEPORTER:
      Cg_TeleporterTrail(ent);
      break;
    case TRAIL_PLAYER_SPAWN:
      Cg_PlayerSpawnTrail(ent);
      break;
    case TRAIL_GIB:
      Cg_GibTrail(ent, start, end);
      break;
    case TRAIL_FIREBALL:
      Cg_FireballTrail(ent, start, end);
      break;
    case TRAIL_QUAKE_NAIL:
      Cg_NailTrail(ent, start, end);
      break;
    default:
      break;
  }

#if defined(G_CTF)
  if (s->effects & EF_CTF_MASK) {
    Cg_CtfEffectTrail(ent, start, end);
  }
#endif

}
