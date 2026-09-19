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
 * @brief Adds a decal to the view if decals are enabled.
 */
void Cg_AddDecal(const RenderDecal *decal) {

  if (!cg_addDecals->value) {
    return;
  }

  cgi.AddDecal(cgi.view, decal);
}

/**
 * @brief Spawns blaster impact sprites, a burn decal, a brief dynamic light, and plays the hit sound.
 */
static void Cg_BlasterEffect(const Vec3 org, const Vec3 dir, const Vec3 color) {

  for (int32_t i = 0; i < 2; i++) {
    const float saturation = RandomRangef(.8f, 1.f);

    // surface aligned blast ring sprite
    Cg_AddSprite(&(ClientGameSprite) {
      .animation = cgSpriteBlasterRing,
      .lifetime = Cg_AnimationLifetime(cgSpriteBlasterRing, 17.5f),
      .origin = Vec3_Fmaf(org, 3.f, dir),
      .size = 22.5f,
      .sizeVelocity = 75.f,
      .dir = (i == 1) ? dir : Vec3_Zero(),
      .color = Vec3_Scale(color, saturation),
    });
  }

  // radial particles
  for (int32_t i = 0; i < 32; i++) {

    const Vec3 velocity = Vec3_RandomizeDir(Vec3_Scale(dir, 125.f), .6666f);

    Cg_AddSprite(&(ClientGameSprite) {
      .atlasImage = cgSpriteParticle,
      .origin = Vec3_Fmaf(org, 3.f, dir),
      .velocity = velocity,
      .size = 4.f,
      .acceleration = Vec3_Scale(velocity, -2.f),
      .lifetime = 500,
      .color = color,
    });
  }

  // residual flames
  for (int32_t i = 0; i < 3; i++) {

    Cg_AddSprite(&(ClientGameSprite) {
      .animation = cgSpriteBlasterFlame,
      .lifetime = Cg_AnimationLifetime(cgSpriteBlasterFlame, 30),
      .origin = Vec3_Fmaf(org, 5.f, Vec3_RandomDir()),
      .rotation = RandomRadian(),
      .rotationVelocity = Randomf() * .1f,
      .size = 25.f,
      .color = color,
    });
  }

  // surface flame
  const float flameSat = RandomRangef(.8f, 1.f);

  Cg_AddSprite(&(ClientGameSprite) {
    .animation = cgSpriteBlasterFlame,
    .lifetime = Cg_AnimationLifetime(cgSpriteBlasterFlame, 30),
    .origin = Vec3_Fmaf(org, 5.f, Vec3_RandomDir()),
    .rotation = RandomRadian(),
    .rotationVelocity = Randomf() * .1f,
    .dir = dir,
    .size = 25.f,
    .sizeVelocity = 20.f,
    .color = Vec3_Scale(color, flameSat),
  });

  Cg_AddDecal(&(RenderDecal) {
    .image = cgDecalBurn[Randomi() % lengthof(cgDecalBurn)],
    .origin = org,
    .radius = RandomRangef(6.f, 12.f),
    .color = Color3fv(color),
    .lifetime = 3000 + Randomf() * 3000,
    .rotation = RandomRadian()
  });

  Cg_AddLight(&(const ClientGameLight) {
    .origin = Vec3_Fmaf(org, 32.f, dir),
    .radius = 260.f,
    .color = color,
    .intensity = 5.f,
    .decay = 500.f
  });

  Cg_AddSample(cgi.stage, &(const SoundPlaySample) {
    .sample = cgSampleBlasterHit,
    .origin = org,
  });
}

/**
 * @brief Spawns a fast-moving tracer beam sprite between two points.
 */
static void Cg_TracerEffect(const Vec3 start, const Vec3 end) {;
  const float tracerSpeed = 4000.f;
  const float tracerLength = 120.f;
  float len;
  Vec3 velocity = Vec3_NormalizeLength(Vec3_Subtract(end, start), &len);
  const uint32_t lifetime = SECONDS_TO_MILLIS((len - tracerLength) / tracerSpeed);

  Cg_AddSprite(&(ClientGameSprite) {
    .type = SPRITE_BEAM,
    .image = cgBeamTracer,
    .origin = start,
    .termination = Vec3_Fmaf(start, tracerLength, velocity),
    .size = 2.5f,
    .velocity = Vec3_Scale(velocity, tracerSpeed),
    .lifetime = lifetime,
    .color = MakeVec3(1.f, .9f, .6f),
  });
}

/**
 * @brief Renders an AI navigation node visualization with a colored marker and bounding box outline.
 */
static void Cg_AiNodeEffect(const Vec3 start, const uint8_t color, const uint16_t id) {
  const uint8_t colorId = color & 0x7;

  const float hue = colorId == 3
    ? color_hue_red
    : colorId == 2
      ? color_hue_rose
      : colorId == 1
        ? color_hue_yellow
        : color_hue_orange;

  if (color & 16) {
    Cg_AddSprite(&(ClientGameSprite) {
      .atlasImage = cgSpriteNodeWait,
      .origin = Vec3_Add(start, MakeVec3(0, 0, 16.f)),
      .flags = SPRITE_SERVER_TIME,
      .lifetime = 1,
      .size = 8.f,
      .color = MakeVec3(1.f, 1.f, 1.f),
    });
  }

  const Vec3 c = ColorHSV(hue, 1.f, 1.f).vec3;

  Cg_AddSprite(&(ClientGameSprite) {
    .atlasImage = cgSpriteParticle,
    .origin = start,
    .flags = SPRITE_SERVER_TIME,
    .size = 4.f,
    .color = c,
  });

  // draw bbox representation
  const Box3 bounds = Cg_PlayerBounds(false);

  CmTrace tr = cgi.Trace(start, Vec3_Subtract(start, MakeVec3(0, 0, MAX_WORLD_DIST)), bounds, NULL, CONTENTS_MASK_CLIP_PLAYER | CONTENTS_MASK_LIQUID);

  if (tr.startSolid) {
    tr = cgi.Trace(start, Vec3_Subtract(start, MakeVec3(0, 0, MAX_WORLD_DIST)), Cg_PlayerBounds(true), NULL, CONTENTS_MASK_CLIP_PLAYER | CONTENTS_MASK_LIQUID);
  }

  Box3 box = Box3_Translate(bounds, tr.end);
  box.maxs.z = box.mins.z = tr.end.z + bounds.mins.z;
  Vec3 points[8];
  Box3_ToPoints(box, points);

  Cg_AddSprite(&(ClientGameSprite) {
    .type = SPRITE_BEAM,
    .image = cgBeamLine,
    .origin = points[0],
    .termination = points[1],
    .size = 1.5f,
    .flags = SPRITE_SERVER_TIME,
    .color = c,
  });

  Cg_AddSprite(&(ClientGameSprite) {
    .type = SPRITE_BEAM,
    .image = cgBeamLine,
    .origin = points[0],
    .termination = points[2],
    .size = 1.5f,
    .flags = SPRITE_SERVER_TIME,
    .color = c,
  });

  Cg_AddSprite(&(ClientGameSprite) {
    .type = SPRITE_BEAM,
    .image = cgBeamLine,
    .origin = points[3],
    .termination = points[1],
    .size = 1.5f,
    .flags = SPRITE_SERVER_TIME,
    .color = c,
  });

  Cg_AddSprite(&(ClientGameSprite) {
    .type = SPRITE_BEAM,
    .image = cgBeamLine,
    .origin = points[3],
    .termination = points[2],
    .size = 1.5f,
    .flags = SPRITE_SERVER_TIME,
    .color = c,
  });
}

/**
 * @brief Renders an AI navigation node link showing direction and connection type as a colored beam.
 */
static void Cg_AiNodeLinkEffect(const Vec3 start, const Vec3 end, const uint8_t bits) {

  const float satval = (bits & 4) ? 0.2f : 1.0f;
  const Vec3 bothColor = ColorHSV(color_hue_green, satval, satval).vec3;
  const Vec3 aColor = ColorHSV(color_hue_blue, satval, satval).vec3;
  const Vec3 moverColor = ColorHSV(color_hue_cyan, satval, satval).vec3;

  // mover connection
  if (bits & 8) {
    Cg_AddSprite(&(ClientGameSprite) {
      .type = SPRITE_BEAM,
      .image = cgBeamHook,
      .origin = start,
      .termination = end,
      .size = 2.f,
      .flags = SPRITE_SERVER_TIME,
      .color = moverColor,
    });

    return;
  }

  Vec3 center = Vec3_Scale(Vec3_Add(start, end), 0.5f);
  const Vec3 euler = Vec3_Euler(Vec3_Normalize(Vec3_Subtract(start, end)));
  Vec3 up;
  Vec3_Vectors(euler, NULL, NULL, &up);
  Vec3 textCenter = center;
  textCenter.z += 8.f;

  //Cg_DrawFloatingStringLine(text_center, va("%.1f", Vec3_Distance(start, end)), 1.f, MakeVec3(0.f, 0.f, 1.f));

  if (bits & 16) {
    textCenter.z -= 2;
  //  Cg_DrawFloatingStringLine(text_center, "Slow-drop", 1.f, MakeVec3(0.f, 0.f, 1.f));
    Cg_AddSprite(&(ClientGameSprite) {
      .atlasImage = cgSpriteNodeSlow,
      .origin = textCenter,
      .flags = SPRITE_SERVER_TIME,
      .lifetime = 1,
      .size = 8.f,
      .color = MakeVec3(1.f, 1.f, 1.f),
    });
  }

  // both sides connected
  if ((bits & 3) == 3) {
    Cg_AddSprite(&(ClientGameSprite) {
      .type = SPRITE_BEAM,
      .image = cgBeamLine,
      .origin = start,
      .termination = end,
      .size = 2.f,
      .flags = SPRITE_SERVER_TIME,
      .color = bothColor,
    });

  } else {
    
    // only end connected
    if (bits & 2) {
      Cg_AddSprite(&(ClientGameSprite) {
        .type = SPRITE_BEAM,
        .image = cgBeamArrow,
        .origin = end,
        .termination = start,
        .size = 2.f,
        .flags = SPRITE_SERVER_TIME,
        .color = aColor,
      });
    } else {
      Cg_AddSprite(&(ClientGameSprite) {
        .type = SPRITE_BEAM,
        .image = cgBeamArrow,
        .origin = start,
        .termination = end,
        .size = 2.f,
        .flags = SPRITE_SERVER_TIME,
        .color = aColor,
      });
    }
  }
}

/**
 * @brief Spawns bullet impact sparks, smoke, a decal, and plays a ricochet sound.
 */
static void Cg_BulletEffect(const Vec3 org, const Vec3 dir) {
  static uint32_t last_ric_time;

  if (cgi.PointContents(org) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(NULL, org, Vec3_Fmaf(org, 8.f, dir), 2.f);
  } else {

    float sparkLife = 240.f;
    float sparkSize = RandomRangef(35.f, 45.f);

    // spark spikes billboard
    Cg_AddSprite(&(ClientGameSprite) {
      .animation = cgSpriteImpactSpark01,
      .origin = Vec3_Fmaf(org, 2.f, dir),
      .rotation = RandomRadian(),
      .size = sparkSize,
      .lifetime = sparkLife,
      .color = MakeVec3(1.f, 1.f, 1.f),
    });

    // spark spikes decal
    Cg_AddSprite(&(ClientGameSprite) {
      .animation = cgSpriteImpactSpark01,
      .origin = Vec3_Fmaf(org, 2.f, dir),
      .rotation = RandomRadian(),
      .size = sparkSize,
      .lifetime = sparkLife,
      .color = MakeVec3(1.f, 1.f, 1.f),
      .dir = dir
    });

    // spark dots - fall, bounce, and rest before fading
    Vec3 sparkOrigin = Vec3_Fmaf(org, 2.f, dir);
    for (int32_t i = 0; i < 8; i++) {
      float size = RandomRangef(2.f, 6.f);
      float lifetime = RandomRangef(800.f, 1200.f);
      Cg_AddSprite(&(ClientGameSprite) {
        .atlasImage = cgSpriteImpactSpark01Dot,
        .origin = sparkOrigin,
        .velocity = Vec3_Scale(Vec3_Mix(Vec3_RandomDir(), dir, 0.5f), 120.f),
        .acceleration.z = -SPRITE_GRAVITY * 2.f,
        .bounce = 0.3f,
        .friction = 60.f,
        .size = size,
        .sizeVelocity = -size * 0.6f,
        .lifetime = lifetime,
        .color = MakeVec3(1.f, .8f, .3f),
        .endColor = MakeVec3(0.f, 0.f, 0.f),
      });
    }

    // impact smoke
    Cg_AddSprite(&(ClientGameSprite) {
      .atlasImage = cgSpritePuffCloud,
      .origin = Vec3_Fmaf(org, 5.f, dir),
      .velocity.z = 10.0f,
      .size = RandomRangef(30.f, 50.f),
      .rotation = RandomRadian(),
      .sizeVelocity = 60.0f,
      .lifetime = 800.f,
      .color = MakeVec3(.125f, .125f, .125f),
      .lighting = 1.f
    });

    // impact hotness
    Cg_AddSprite(&(ClientGameSprite) {
      .atlasImage = cgSpriteSpark,
      .origin = Vec3_Fmaf(org, 0.5f, dir),
      .rotation = RandomRadian(),
      .dir = dir,
      .size = 4.f,
      .lifetime = 650,
      .color = ColorHSV(color_hue_orange, 0.8f, 1.f).vec3,
    });

    // impact light flash
    Cg_AddLight(&(const ClientGameLight) {
      .origin = Vec3_Fmaf(org, 2.f, dir),
      .radius = 80.f,
      .color = MakeVec3(1.f, .7f, .3f),
      .intensity = 3.f,
      .decay = 150,
      .flags = R_LIGHT_NO_SHADOW,
    });
  }

  Cg_AddDecal(&(RenderDecal) {
    .image = cgDecalBullet[Randomi() % lengthof(cgDecalBullet)],
    .origin = org,
    .radius = RandomRangef(1.f, 3.f),
    .color = color_black,
    .lifetime = 12000 + Randomf() * 10000,
    .rotation = RandomRadian()
  });

  if (cgi.client->unclampedTime < last_ric_time) {
    last_ric_time = 0;
  }

  if (cgi.client->unclampedTime - last_ric_time > 300) {
    last_ric_time = cgi.client->unclampedTime;

    Cg_AddSample(cgi.stage, &(const SoundPlaySample) {
      .sample = cgSampleMachinegunHit[RandomRangeu(0, 3)],
      .origin = org,
      .pitch = RandomRangei(-8, 9)
    });
  }
}

/**
 * @brief Spawns nail impact effects and plays the nail hit sound.
 */
static void Cg_NailEffect(const Vec3 org, const Vec3 dir) {
  if (cgi.PointContents(org) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(NULL, org, Vec3_Fmaf(org, 8.f, dir), 2.f);
  } else {

    float sparkLife = 240.f;
    float sparkSize = RandomRangef(35.f, 45.f);

    // spark spikes billboard
    Cg_AddSprite(&(ClientGameSprite) {
      .animation = cgSpriteImpactSpark01,
      .origin = Vec3_Fmaf(org, 2.f, dir),
      .rotation = RandomRadian(),
      .size = sparkSize,
      .lifetime = sparkLife,
      .color = MakeVec3(1.f, 1.f, 1.f),
    });

    // spark spikes decal
    Cg_AddSprite(&(ClientGameSprite) {
      .animation = cgSpriteImpactSpark01,
      .origin = Vec3_Fmaf(org, 2.f, dir),
      .rotation = RandomRadian(),
      .size = sparkSize,
      .lifetime = sparkLife,
      .color = MakeVec3(1.f, 1.f, 1.f),
      .dir = dir
    });

    // spark dots - fall, bounce, and rest before fading
    Vec3 sparkOrigin = Vec3_Fmaf(org, 2.f, dir);
    for (int32_t i = 0; i < 6; i++) {
      float size = RandomRangef(2.f, 6.f);
      float lifetime = RandomRangef(800.f, 1200.f);
      Cg_AddSprite(&(ClientGameSprite) {
        .atlasImage = cgSpriteImpactSpark01Dot,
        .origin = sparkOrigin,
        .velocity = Vec3_Scale(Vec3_Mix(Vec3_RandomDir(), dir, 0.5f), 120.f),
        .acceleration.z = -SPRITE_GRAVITY * 2.f,
        .bounce = 0.3f,
        .friction = 60.f,
        .size = size,
        .sizeVelocity = -size * 0.6f,
        .lifetime = lifetime,
        .color = MakeVec3(1.f, .8f, .3f),
        .endColor = MakeVec3(0.f, 0.f, 0.f),
      });
    }

    // impact smoke
    Cg_AddSprite(&(ClientGameSprite) {
      .atlasImage = cgSpritePuffCloud,
      .origin = Vec3_Fmaf(org, 5.f, dir),
      .velocity.z = 10.0f,
      .size = RandomRangef(30.f, 50.f),
      .rotation = RandomRadian(),
      .sizeVelocity = 60.0f,
      .lifetime = 800.f,
      .color = MakeVec3(.125f, .125f, .125f),
      .lighting = 1.f
    });

    // impact hotness
    Cg_AddSprite(&(ClientGameSprite) {
      .atlasImage = cgSpriteSpark,
      .origin = Vec3_Fmaf(org, 0.5f, dir),
      .rotation = RandomRadian(),
      .dir = dir,
      .size = 4.f,
      .lifetime = 650,
      .color = ColorHSV(color_hue_orange, 0.8f, 1.f).vec3,
    });

    // impact light flash
    Cg_AddLight(&(const ClientGameLight) {
      .origin = Vec3_Fmaf(org, 2.f, dir),
      .radius = 80.f,
      .color = MakeVec3(1.f, .7f, .3f),
      .intensity = 3.f,
      .decay = 150,
      .flags = R_LIGHT_NO_SHADOW,
    });
  }

  Cg_AddDecal(&(RenderDecal) {
    .image = cgDecalBullet[Randomi() % lengthof(cgDecalBullet)],
    .origin = org,
    .radius = RandomRangef(1.f, 3.f),
    .color = color_black,
    .lifetime = 12000 + Randomf() * 10000,
    .rotation = RandomRadian()
  });

  Cg_AddSample(cgi.stage, &(const SoundPlaySample) {
    .sample = cgSampleQuakeNailHit,
    .origin = org,
    .pitch = RandomRangei(0, 5)
  });
}

/**
 * @brief Spawns blood splatter sprites and decals at the impact point.
 */
static void Cg_BloodEffect(const Vec3 org, const Vec3 dir, int32_t count) {

  for (int32_t i = 0; i < count; i += 3) {

    if (!Cg_AddSprite(&(ClientGameSprite) {
        .animation = cgSpriteBlood01,
        .lifetime = Cg_AnimationLifetime(cgSpriteBlood01, 30) + Randomf() * 500,
        .size = RandomRangef(48.f, 64.f),
        .rotation = RandomRadian(),
        .origin = Vec3_Fmaf(Vec3_Add(org, Vec3_RandomRange(-10.f, 10.f)), RandomRangef(0.f, 32.f), dir),
        .velocity = Vec3_RandomRange(-30.f, 30.f),
        .acceleration.z = -SPRITE_GRAVITY / 2.0,
        .color = MakeVec3(.9f, .9f, .9f),
        .lighting = .55f,
      })) {
      break;
    }

    if (i % 6) {
      continue;
    }

    Cg_AddDecal(&(RenderDecal) {
      .image = cgDecalBlood[Randomi() % lengthof(cgDecalBlood)],
      .origin = Vec3_Add(org, Vec3_RandomRange(-8.f, 8.f)),
      .radius = RandomRangef(32.f, 64.f),
      .color = Color3f(.6f, 0.f, 0.f),
      .lifetime = 6000 + Randomf() * 6000,
      .rotation = RandomRadian()
    });
  }
}

#define GIB_STREAM_DIST 220.0
#define GIB_STREAM_COUNT 12

/**
 * @brief Spawns gib stream sprites, a large blood decal, and plays the gib sound.
 */
void Cg_GibEffect(const Vec3 org, int32_t count) {

  // if a player has died underwater, emit some bubbles
  if (cgi.PointContents(org) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(NULL, org, Vec3_Add(org, MakeVec3(0.f, 0.f, 64.f)), 1.f);
  }

  for (int32_t i = 0; i < count; i++) {

    // set the origin and velocity for each gib stream
    const Vec3 o = Vec3_Add(MakeVec3(RandomRangef(-8.f, 8.f), RandomRangef(-8.f, 8.f), RandomRangef(-4.f, 20.f)), org);
    const Vec3 v = MakeVec3(RandomRangef(-1.f, 1.f), RandomRangef(-1.f, 1.f), RandomRangef(.2f, 1.2f));

    float dist = GIB_STREAM_DIST;
    Vec3 tmp = Vec3_Fmaf(o, dist, v);

    const CmTrace tr = cgi.Trace(o, tmp, Box3_Zero(), NULL, CONTENTS_MASK_CLIP_PROJECTILE);
    dist = GIB_STREAM_DIST * tr.fraction;

    for (int32_t j = 1; j < GIB_STREAM_COUNT; j++) {

      if (!Cg_AddSprite(&(ClientGameSprite) {
          .animation = cgSpriteBlood01,
          .lifetime = Cg_AnimationLifetime(cgSpriteBlood01, 30) + Randomf() * 500,
          .origin = o,
          .velocity = Vec3_Add(Vec3_Add(Vec3_Scale(v, dist * ((float)j / GIB_STREAM_COUNT)), Vec3_RandomRange(-2.f, 2.f)), MakeVec3(0.f, 0.f, 100.f)),
          .acceleration.z = -SPRITE_GRAVITY * 2.0,
          .size = RandomRangef(24.f, 56.f),
          .color = MakeVec3(.85f, .85f, .85f),
          .lighting = .65f
        })) {
        break;
      }
    }
  }

  Cg_AddDecal(&(RenderDecal) {
    .image = cgDecalBlood[Randomi() % lengthof(cgDecalBlood)],
    .origin = org,
    .radius = RandomRangef(64.f, 128.f),
    .color = color_red,
    .lifetime = 6000 + Randomf() * 6000,
    .rotation = RandomRadian()
  });

  Cg_AddSample(cgi.stage, &(const SoundPlaySample) {
    .sample = cgSampleGib,
    .origin = org,
  });
}

/**
 * @brief Spawns spark flash billboards, bouncing sparks, a light, and plays the sparks sound.
 */
void Cg_SparksEffect(const Vec3 org, const Vec3 dir, int32_t count) {

  const Vec3 offsetOrg = Vec3_Fmaf(org, 2.f, dir);

  // spark flash billboard
  Cg_AddSprite(&(ClientGameSprite) {
    .animation = cgSpriteImpactSpark01,
    .origin = offsetOrg,
    .rotation = RandomRadian(),
    .size = RandomRangef(30.f, 40.f),
    .lifetime = 240.f,
    .color = MakeVec3(1.f, .9f, .7f),
  });

  // spark flash decal
  Cg_AddSprite(&(ClientGameSprite) {
    .animation = cgSpriteImpactSpark01,
    .origin = offsetOrg,
    .rotation = RandomRadian(),
    .size = RandomRangef(30.f, 40.f),
    .lifetime = 240.f,
    .color = MakeVec3(1.f, .9f, .7f),
    .dir = dir,
  });

  // spark dots
  for (int32_t i = 0; i < 8; i++) {
    Cg_AddSprite(&(ClientGameSprite) {
      .atlasImage = cgSpriteImpactSpark01Dot,
      .origin = offsetOrg,
      .velocity = Vec3_Scale(Vec3_Mix(Vec3_RandomDir(), dir, .33f), RandomRangef(40.f, 80.f)),
      .size = RandomRangef(3.f, 6.f),
      .sizeVelocity = -8.f,
      .lifetime = RandomRangef(300.f, 500.f),
      .color = MakeVec3(1.f, .9f, .7f),
    });
  }

  // hot spot glow on surface
  Cg_AddSprite(&(ClientGameSprite) {
    .atlasImage = cgSpriteSpark,
    .origin = Vec3_Fmaf(org, .5f, dir),
    .rotation = RandomRadian(),
    .dir = dir,
    .size = 4.f,
    .lifetime = 650,
    .color = ColorHSV(color_hue_orange, .8f, 1.f).vec3,
  });

  // bouncing sparks
  for (int32_t i = 0; i < count; i++) {
    const float hue = color_hue_yellow - RandomRangef(4.f, 40.f);

    if (!Cg_AddSprite(&(ClientGameSprite) {
        .atlasImage = cgSpriteSpark,
        .origin = Vec3_Add(org, Vec3_RandomRange(-4.f, 4.f)),
        .velocity = Vec3_Scale(Vec3_RandomizeDir(dir, .33f), RandomRangef(64.f, 128.f)),
        .acceleration.z = -SPRITE_GRAVITY,
        .lifetime = 1000 + Randomf() * 1000,
        .size = RandomRangef(.5f, 3.f),
        .sizeVelocity = -1.f,
        .rotation = RandomRadian(),
        .rotationVelocity = 1.f,
        .bounce = .3f,
        .color = ColorHSV(hue, .4f, 1.f).vec3,
        .lighting = 1.f,
      })) {
      break;
    }
  }

  Cg_AddLight(&(const ClientGameLight) {
    .origin = org,
    .radius = 100.0,
    .color = MakeVec3(.7f, .5f, .5f),
    .intensity = 2.f,
    .decay = RandomRangeu(120, 180),
  });

  Cg_AddSample(cgi.stage, &(const SoundPlaySample) {
    .sample = cgSampleSparks,
    .origin = org,
  });
}

/**
 * @brief Spawns explosion sprites, ember particles, a burn decal, a bright decaying light, and plays the explosion sound.
 */
static void Cg_ExplosionEffect(const Vec3 org, const Vec3 dir) {

  if (cgi.PointContents(org) & CONTENTS_MASK_LIQUID) {
    for (int32_t i = 0; i < 16; i++) {

      const Vec3 start = Vec3_Add(org, Vec3_RandomRange(-16.f, 16.f));
      const Vec3 end = Vec3_Fmaf(start, RandomRangef(64.f, 192.f), Vec3_RandomDir());

      Cg_BubbleTrail(NULL, start, end, 1.f);
    }
  } else {
    // ember sparks
    for (int32_t i = 0; i < 150; i++) {
      const uint32_t lifetime = 3000 + Randomf() * 500;
      const float size = 2.f + Randomf() * 2.f;
      const float hue = RandomRangef(10.f, 50.f);

      if (!Cg_AddSprite(&(ClientGameSprite) {
          .atlasImage = cgSpriteParticle2,
          .origin = Vec3_Add(org, Vec3_RandomRange(-16.f, 16.f)),
          .velocity = Vec3_RandomRange(-400.f, 400.f),
          .acceleration.z = -SPRITE_GRAVITY * 2.f,
          .lifetime = lifetime,
          .size = size,
          .sizeVelocity = -size / MILLIS_TO_SECONDS(lifetime),
          .bounce = .4f,
          .color = ColorHSV(hue, .9f, .8f).vec3,
          .lighting = .35f,
        })) {
        break;
      }
    }
  }

  // billboard explosion 1
  Cg_AddSprite(&(ClientGameSprite) {
    .origin = org,
    .animation = cgSpriteExplosion,
    .lifetime = Cg_AnimationLifetime(cgSpriteExplosion, 40),
    .size = 150.f,
    .sizeVelocity = 40.f,
    .rotation = RandomRadian(),
    .color = MakeVec3(.5f, .5f, .5f),
  });

  // billboard explosion 2
  Cg_AddSprite(&(ClientGameSprite) {
    .origin = org,
    .animation = cgSpriteExplosion,
    .lifetime = Cg_AnimationLifetime(cgSpriteExplosion, 30),
    .size = 250.f,
    .sizeVelocity = 40.f,
    .rotation = RandomRadian(),
    .color = MakeVec3(.5f, .5f, .5f),
  });

  // decal explosion
  Cg_AddSprite(&(ClientGameSprite) {
    .origin = org,
    .animation = cgSpriteExplosion,
    .lifetime = Cg_AnimationLifetime(cgSpriteExplosion, 30),
    .size = 250.f,
    .sizeVelocity = 40.f,
    .rotation = RandomRadian(),
    .color = MakeVec3(.5f, .5f, .5f),
    .dir = dir
  });

  // decal blast ring
  Cg_AddSprite(&(ClientGameSprite) {
    .origin = org,
    .animation = cgSpriteExplosionRing02,
    .lifetime = Cg_AnimationLifetime(cgSpriteExplosionRing02, 20),
    .size = 100.f,
    .sizeVelocity = 700.f,
    .sizeAcceleration = -700.f,
    .rotation = RandomRadian(),
    .color = MakeVec3(.5f, .5f, .5f),
    .dir = dir
  });

  // blast glow
  Cg_AddSprite(&(ClientGameSprite) {
    .origin = org,
    .lifetime = 600,
    .size = 300.f,
    .rotation = RandomRadian(),
    .atlasImage = cgSpriteExplosionGlow,
    .color = MakeVec3(.4f, .4f, .4f),
    .lighting = .55f
  });

  Cg_AddDecal(&(RenderDecal) {
    .image = cgDecalBurn[Randomi() % lengthof(cgDecalBurn)],
    .origin = org,
    .radius = RandomRangef(32.f, 64.f),
    .color = Color4f(0.f, 0.f, 0.f, .5f + Randomf() * .4f),
    .lifetime = 16000 + Randomf() * 8000,
    .rotation = RandomRadian()
  });

  // secondary blast glow (smaller + long last)
  Cg_AddSprite(&(ClientGameSprite) {
      .origin = org,
           .lifetime = 1500,
           .size = 72.f,
           .rotation = RandomRadian(),
           .atlasImage = cgSpriteExplosionGlow,
           .color = MakeVec3(.7f, .45f, .2f),
           .lighting = .45f
  });

  Cg_AddLight(&(const ClientGameLight) {
    .origin = org,
    .radius = 360.0,
    .color = MakeVec3(.9f, .6f, .3f),
    .intensity = 6.f,
    .decay = 1600
  });

  Cg_AddSample(cgi.stage, &(const SoundPlaySample) {
    .sample = cgSampleExplosion,
    .origin = org,
  });
}

/**
 * @brief Spawns hyperblaster impact sprites, a flash, a burn decal, a light, and plays the hit sound.
 */
static void Cg_HyperblasterEffect(const Vec3 org, const Vec3 dir) {
  
  Vec3 color = ColorHSV(204.f, .6f, 1.f).vec3;

  // impact "splash"
  for (uint32_t i = 0; i < 6; i++) {
    Cg_AddSprite(&(ClientGameSprite) {
      .origin = org,
      .animation = cgSpriteElectro01,
      .lifetime = Cg_AnimationLifetime(cgSpriteElectro01, 20),
      .size = 50.f,
      .sizeVelocity = 400.f,
      .rotation = RandomRadian(),
      .dir = Vec3_RandomRange(-1.f, 1.f),
      .color = color,
      .lighting = .3f
    });
  }

  Cg_AddSprite(&(ClientGameSprite) {
    .origin = org,
    .animation = cgSpriteElectro01,
    .lifetime = Cg_AnimationLifetime(cgSpriteElectro01, 8),
    .size = 100.f,
    .sizeVelocity = 25.f,
    .rotation = RandomRadian(),
    .dir = dir,
    .color = color,
    .lighting = 1.f
  });

  // impact flash
  for (uint32_t i = 0; i < 2; i++) {
    Cg_AddSprite(&(ClientGameSprite) {
      .origin = org,
      .atlasImage = cgSpriteFlash,
      .lifetime = 150,
      .size = RandomRangef(75.f, 100.f),
      .rotation = RandomRadian(),
      .rotationVelocity = i == 0 ? .66f : -.66f,
      .color = color,
    });
  }

  Cg_AddDecal(&(RenderDecal) {
    .image = cgDecalBurn[Randomi() % lengthof(cgDecalBurn)],
    .origin = org,
    .radius = RandomRangef(12.f, 18.f),
    .color = Color3fv(ColorHSV(225.f, .7f, .8f).vec3),
    .lifetime = 3000 + Randomf() * 3000,
    .rotation = RandomRadian()
  });

  Cg_AddLight(&(ClientGameLight) {
    .origin = Vec3_Add(org, dir),
    .radius = 200.f,
    .color = MakeVec3(.4f, .7f, 1.f),
    .intensity = 3.f,
    .decay = 250,
  });

  Cg_AddSample(cgi.stage, &(const SoundPlaySample) {
    .sample = cgSampleHyperblasterHit,
    .origin = Vec3_Add(org, dir),
  });
}

/**
 * @brief Spawns bubble trails for a lightning discharge in liquid and plays the discharge sound.
 */
static void Cg_LightningDischargeEffect(const Vec3 org) {
  for (int32_t i = 0; i < 40; i++) {
    Cg_BubbleTrail(NULL, org, Vec3_Add(org, Vec3_RandomRange(-48.f, 48.f)), 1.f);
  }

  Cg_AddLight(&(const ClientGameLight) {
    .origin = org,
    .radius = 160.f,
    .color = MakeVec3(.6f, .6f, 1.f),
    .intensity = 1.f,
    .decay = 750
  });

  Cg_AddSample(cgi.stage, &(const SoundPlaySample) {
    .sample = cgSampleLightningDischarge,
    .origin = org,
  });

  
}

/**
 * @brief Renders a railgun trail with a core beam, helix particles, lights, and impact effects.
 */
static void Cg_RailEffect(const Vec3 start, const Vec3 end, const Vec3 dir, int32_t flags, float hue) {

  const Vec3 color = ColorHSV(hue, 1.f, 1.f).vec3;

  Cg_AddLight(&(ClientGameLight) {
    .origin = start,
    .radius = 140.f,
    .color = color,
    .intensity = 5.f,
    .decay = 240,
  });

  if (cgi.BoxContents(Box3_FromPoints((const Vec3[]) { start, end }, 2)) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(NULL, start, end, 1.f);
  }

  Vec3 forward;
  const float dist = Vec3_DistanceDir(end, start, &forward);
  
  // Add lights along the trail at regular intervals
  for (float f = 128.f; f < dist; f += 128.f) {
    Cg_AddLight(&(const ClientGameLight) {
      .origin = Vec3_Fmaf(start, f, forward),
      .radius = 120.f,
      .color = color,
      .intensity = 2.5f,
      .decay = 150.f,
    });
  }

  const Vec3 right = MakeVec3(forward.z, -forward.x, forward.y);
  const Vec3 up = Vec3_Cross(forward, right);

  const uint32_t coreLifetime = 500;
  const uint32_t vaporLifetime = 900;

  for (int32_t i = 0; i < dist; i++) {
    const float cosi = cosf(i * 0.1f);
    const float sini = sinf(i * 0.1f);

    const Vec3 org = Vec3_Add(Vec3_Add(Vec3_Fmaf(start, i, forward), Vec3_Scale(right, cosi)), Vec3_Scale(up, sini));
    const Vec3 accel = Vec3_Add(Vec3_Add(Vec3_Scale(right, cosi * 32.f), Vec3_Scale(up, sini * 32.f)), Vec3_Up());

    if (cgi.PointContents(org) & CONTENTS_MASK_LIQUID) {
      if (Randomb()) {
        continue;
      }
    }

    Cg_AddSprite(&(ClientGameSprite) {
        .atlasImage = cgSpriteParticle3,
            .origin = org,
            .velocity = Vec3_Scale(forward, 256.f),
            .acceleration = Vec3_Add(accel, Vec3_Scale(Vec3_RandomDir(), 192.f)),
            .friction = 2048.f,
            .lifetime = coreLifetime + Randomf() * 120,
            .size = 1.f,
            .sizeVelocity = 1.0 / MILLIS_TO_SECONDS(coreLifetime),
            .color = Color3f(color.x / 2, color.y / 2, color.z / 2).vec3,
            .lighting = 1.f,
    });

    if (i % 3 == 0) {
      const int32_t h = (int32_t) hue + RandomRangei(10, 20) % 360;
      const Vec3 altColor = ColorHSV(h, 0.f, 1.f).vec3;
      Cg_AddSprite(&(ClientGameSprite) {
        .atlasImage = cgSpriteParticle3,
        .origin = org,
        .velocity = Vec3_Scale(Vec3_RandomDir(), 64.f),
        .acceleration = MakeVec3(RandomRangef(-64.f, 64.f), RandomRangef(-64.f, 64.f), -SPRITE_GRAVITY),
        .lifetime = vaporLifetime + Randomf() * 160,
        .size = 2.5f,
        .sizeVelocity = -.5f / MILLIS_TO_SECONDS(vaporLifetime),
        .color = altColor,
        .lighting = 1.f,
      });
    }
  }

  // Core beam
  Cg_AddSprite(&(ClientGameSprite) {
    .type = SPRITE_BEAM,
    .origin = start,
    .termination = end,
    .image = cgBeamRail,
    .flags = SPRITE_BEAM_REPEAT,
    .size = 3.f,
    .lifetime = 400,
    .color = color,
  });

  // Check for explosion effect on solids

  if (flags & SURF_SKY) {
    return;
  }

  // Impact light
  Cg_AddLight(&(ClientGameLight) {
    .origin = Vec3_Fmaf(end, 1.f, dir),
    .radius = 12.f,
    .color = color,
    .intensity = 10.f,
    .decay = 800.f,
  });

  // hit billboards
  for (int32_t i = 0; i < 2; i++) {
    Cg_AddSprite(&(ClientGameSprite) {
      .origin = Vec3_Add(end, dir),
      .atlasImage = cgSpriteFlash,
      .lifetime = 250,
      .size = 120.f,
      .sizeVelocity = RandomRangef(100.f, 200.f),
      .rotation = RandomRadian(),
      .rotationVelocity = i == 0 ? .66f : -.66f,
      .color = color,
    });
  }

  // slug debris
  if ((cgi.PointContents(end) & CONTENTS_MASK_LIQUID) == 0) {

    for (int32_t i = 0; i < 32; i++) {
      const uint32_t lifetime = 2000 + Randomf() * 300;
      const float size = 2.f + Randomf();

      if (!Cg_AddSprite(&(ClientGameSprite) {
          .atlasImage = cgSpriteParticle2,
          .origin = Vec3_Add(end, Vec3_RandomRange(-4.f, 4.f)),
          .velocity = Vec3_RandomRange(-200.f, 200.f),
          .acceleration.z = -SPRITE_GRAVITY * 2.f,
          .lifetime = lifetime,
          .size = size,
          .sizeVelocity = -size / MILLIS_TO_SECONDS(lifetime),
          .bounce = .4f,
          .color = color,
          .lighting = 1.f,
        })) {
        break;
      }
    }
  }

  Cg_AddDecal(&(RenderDecal) {
    .image = cgDecalSlug[Randomi() % lengthof(cgDecalBurn)],
    .origin = end,
    .radius = RandomRangef(8.f, 12.f),
    .color = Color3fv(color),
    .lifetime = 12000 + Randomf() * 4000,
    .rotation = RandomRadian()
  });
}

/**
 * @brief Packs two entity indices into a void pointer for use as BFG laser sprite data.
 */
typedef union {
  struct {
    uint16_t org, dest;
  };

  void *data;
} ClientGameBfgLaserData;

/**
 * @brief Think callback that updates a BFG laser beam's endpoints from the linked entity origins each frame.
 */
static void Cg_BfgLaserThink(ClientGameSprite *sprite, float life, float delta) {

  const ClientGameBfgLaserData data = { .data = sprite->data };

  const Vec3 org = cgi.client->entities[data.org].origin;
  const Vec3 end = cgi.client->entities[data.dest].origin;

  sprite->origin = org;
  sprite->termination = end;

  if (cgi.BoxContents(Box3_FromPoints((const Vec3[]) { org, end }, 2)) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(NULL, org, end, 4.f);
  }
}

/**
 * @brief Spawns a faint BFG laser beam sprite targeting a dead entity (corpse or giblet).
 */
static void Cg_BfgLaserDeadEffect(const int16_t orgEntity, const int16_t destEntity) {

  const Vec3 org = cgi.client->entities[orgEntity].origin;
  const Vec3 end = cgi.client->entities[destEntity].origin;

  Cg_AddSprite(&(ClientGameSprite) {
    .type = SPRITE_BEAM,
    .image = cgBeamRail,
    .origin = org,
    .termination = end,
    .size = 1.5f,
    .flags = SPRITE_SERVER_TIME | SPRITE_DATA_NOFREE,
    .color = ColorHSV(color_hue_green, .5f, .5f).vec3,
    .data = ((ClientGameBfgLaserData) { .org = orgEntity, .dest = destEntity }).data,
    .Think = Cg_BfgLaserThink,
    .lighting = .25f,
  });
}

/**
 * @brief Spawns a persistent BFG laser beam sprite between two entities with a light and burn decal.
 */
static void Cg_BfgLaserEffect(const int16_t orgEntity, const int16_t destEntity) {

  const Vec3 org = cgi.client->entities[orgEntity].origin;
  const Vec3 end = cgi.client->entities[destEntity].origin;

  Cg_AddSprite(&(ClientGameSprite) {
    .type = SPRITE_BEAM,
    .image = cgBeamRail,
    .origin = org,
    .termination = end,
    .size = 5.f,
    .flags = SPRITE_SERVER_TIME | SPRITE_DATA_NOFREE,
    .color = ColorHSV(color_hue_green, 1.f, 1.f).vec3,
    .data = ((ClientGameBfgLaserData) { .org = orgEntity, .dest = destEntity }).data,
    .Think = Cg_BfgLaserThink,
    .lighting = .5f,
  });

  Cg_AddLight(&(ClientGameLight) {
    .origin = end,
    .radius = 150.0,
    .color = MakeVec3(.8f, 1.f, .5f),
    .intensity = 3.f,
    .decay = 50,
  });

  Cg_AddDecal(&(RenderDecal) {
    .image = cgDecalBurn[Randomi() % lengthof(cgDecalBurn)],
    .origin = end,
    .radius = RandomRangef(12.f, 24.f),
    .color = ColorHSV(color_hue_green, 1.f, 1.f),
    .lifetime = 10000 + Randomf() * 4000,
    .rotation = RandomRadian()
  });
}

/**
 * @brief Spawns BFG explosion sprites, flash billboards, a large burn decal, and a decaying light.
 */
static void Cg_BfgEffect(const Vec3 org) {
  // explosion 1
  for (int32_t i = 0; i < 4; i++) {

    Cg_AddSprite(&(ClientGameSprite) {
      .animation = cgSpriteBfgExplosion2,
      .lifetime = Cg_AnimationLifetime(cgSpriteBfgExplosion2, 15),
      .size = RandomRangef(200.f, 300.f),
      .sizeVelocity = 100.f,
      .sizeAcceleration = -10.f,
      .rotation = RandomRangef(0.f, 2.f * M_PI),
      .origin = Vec3_Fmaf(org, 50.f, Vec3_RandomDir()),
      .color = MakeVec3(1.f, 1.f, 1.f),
    });
  }

  // explosion 2
  for (int32_t i = 0; i < 4; i++) {

    Cg_AddSprite(&(ClientGameSprite) {
      .animation = cgSpriteBfgExplosion3,
      .lifetime = Cg_AnimationLifetime(cgSpriteBfgExplosion3, 15),
      .size = RandomRangef(200.f, 300.f),
      .sizeVelocity = 100.f,
      .sizeAcceleration = -10.f,
      .rotation = RandomRangef(0.f, 2.f * M_PI),
      .origin = Vec3_Fmaf(org, 50.f, Vec3_RandomDir()),
      .color = MakeVec3(1.f, 1.f, 1.f),
      .lighting = .5f,
    });
  }

  // impact flash 1
  for (uint32_t i = 0; i < 4; i++) {

    Cg_AddSprite(&(ClientGameSprite) {
      .atlasImage = cgSpriteFlash,
      .origin = org,
      .lifetime = 600,
      .size = RandomRangef(300, 400),
      .rotation = RandomRadian(),
      .dir = Vec3_Random(),
      .color = MakeVec3(.6f, 1.f, .6f),
      .lighting = .3f,
    });
  }

  // impact flash 2
  Cg_AddSprite(&(ClientGameSprite) {
    .atlasImage = cgSpriteFlash,
    .origin = org,
    .lifetime = 600,
    .size = 400.f,
    .rotation = RandomRadian(),
    .color = MakeVec3(.6f, 1.f, .6f),
    .lighting = .3f,
  });

  // glow
  Cg_AddSprite(&(ClientGameSprite) {
    .atlasImage = cgSpriteParticle,
    .origin = org,
    .lifetime = 1000,
    .size = 600.f,
    .rotation = RandomRadian(),
    .color = MakeVec3(.6f, 1.f, .6f),
    .lighting = .2f,
  });

  Cg_AddDecal(&(RenderDecal) {
    .image = cgDecalBurn[Randomi() % lengthof(cgDecalBurn)],
    .origin = org,
    .radius = RandomRangef(96.f, 128.f),
    .color = Color3f(.6f, 1.f, .6f),
    .lifetime = 18000 + Randomf() * 10000,
    .rotation = RandomRadian()
  });

  Cg_AddLight(&(const ClientGameLight) {
    .origin = org,
    .radius = 300.f,
    .color = MakeVec3(.4f, 1.f, .4f),
    .intensity = 3.f,
    .decay = 1500
  });

  Cg_AddSample(cgi.stage, &(const SoundPlaySample) {
    .sample = cgSampleBfgHit,
    .origin = org,
  });
}

/**
 * @brief Think callback that translates the splash sprite upward proportional to its growth rate.
 */
static void Cg_SplashEffect_Think(ClientGameSprite *s, float life, float delta) {
  s->origin.z += .5f * delta * s->sizeVelocity;
}

/**
 * @brief Spawns a vertical splash column sprite and droplet particles at a liquid surface impact.
 */
static void Cg_SplashEffect(const RenderBspBrushSide *side, const Vec3 org, const Vec3 dir, float size) {

  // vertical spray

  const Vec3 color = side->material->color.vec3;

  const float scale = Clampf01(size / 64.f);

  const uint32_t lifetime = 1800 * scale;

  Cg_AddSprite(&(ClientGameSprite) {
    .atlasImage = cgSpriteSplash0203,
    .lifetime = lifetime,
    .origin = Vec3_Fmaf(org, .5f, MakeVec3(0.f, 0.f, size)),
    .size = size,
    .sizeVelocity = size * 2.f / MILLIS_TO_SECONDS(lifetime),
    .color = color,
    .lighting = 1.f,
    .Think = Cg_SplashEffect_Think,
  });

  // splash droplets

  for (int32_t i = 0; i < 64 * scale; i++) {
    ClientGameSprite *p = Cg_AddSprite(&(ClientGameSprite) {
      .atlasImage = cgSpriteParticle,
      .lifetime = RandomRangeu(0, lifetime),
      .origin = org,
      .size = RandomRangef(1.f, 3.f),
      .velocity = Vec3_Scale(Vec3_RandomizeDir(dir, 0.33f), RandomRangef(100.f, 200.f)),
      .acceleration.z = -SPRITE_GRAVITY,
      .color = color,
      .lighting = 1.f
    });

    if (!p) {
      break;
    }
  }
}

/**
 * @brief Spawns a ripple poof animation and an expanding ring decal on a liquid surface.
 */
static void Cg_RippleEffect(const RenderBspBrushSide *side, const Vec3 org, float size) {

  size *= RandomRangef(0.9f, 1.1f);

  const Vec3 color = side->material->color.vec3;

  float viscosity;
  if (side->contents & CONTENTS_LAVA) {
    viscosity = 10.f;
  } else if (side->contents & CONTENTS_SLIME) {
    viscosity = 20.f;
  } else {
    viscosity = 30.f;
  }

  // center decal
  Cg_AddSprite(&(ClientGameSprite) {
    .animation = cgSpritePoof01,
    .lifetime = Cg_AnimationLifetime(cgSpritePoof01, 30.0f) * (viscosity * .1f),
    .origin = org,
    .size = size * 8.f,
    .sizeVelocity = size,
    .dir = Vec3_Up(),
    .rotation = RandomRadian(),
    .color = color,
    .lighting = 1.f
  });

  // ring decal
  Cg_AddSprite(&(ClientGameSprite) {
    .atlasImage = cgSpriteWaterRing,
    .lifetime = 1000.f,
    .origin = org,
    .size = size * 4.f,
    .sizeVelocity = size * 6.f,
    .rotation = RandomRadian(),
    .dir = Vec3_Up(),
    .color = color,
    .lighting = 1.f
  });
}

/**
 * @brief Spawns a ripple and optional splash effect at a liquid BSP brush side impact.
 */
static void Cg_RippleSplashEffect(const Vec3 org, const Vec3 dir, int32_t brushSide, float size, bool splash) {

  if (brushSide < 0 || brushSide > cgi.WorldModel()->bsp->numBrushSides) {
    Cg_Warn("Invalid brush side %d\n", brushSide);
    return;
  }

  const RenderBspBrushSide *side = cgi.WorldModel()->bsp->brushSides + brushSide;

  Cg_RippleEffect(side, org, size);

  if (splash) {
    Cg_SplashEffect(side, org, dir, size);
  }
}

#if defined(G_HOOK)
/**
 * @brief Spawns hook impact spark particles, a brief light, and plays the hook impact sound.
 */
static void Cg_HookImpactEffect(const Vec3 org, const Vec3 dir) {

  for (int32_t i = 0; i < 32; i++) {

    if (!Cg_AddSprite(&(ClientGameSprite) {
        .atlasImage = cgSpriteParticle,
        .origin = Vec3_Add(org, Vec3_RandomRange(-4.f, 4.f)),
        .velocity = Vec3_Add(Vec3_Scale(dir, 9.f), Vec3_RandomRange(-90.f, 90.f)),
        .acceleration = Vec3_Add(Vec3_RandomRange(-2.f, 2.f), MakeVec3(0.f, 0.f, -0.5f * SPRITE_GRAVITY)),
        .lifetime = 100 + (Randomf() * 150),
        .color = MakeVec3(1.f, 1.f, .4f),
        .size = 6.4f + Randomf() * 3.2f,
        .lighting = 1.f
      })) {
      break;
    }
  }

  Cg_AddLight(&(const ClientGameLight) {
    .origin = Vec3_Add(org, dir),
    .radius = 80.0,
    .color = MakeVec3(.7f, .5f, .5f),
    .intensity = 1.f,
    .decay = 850
  });

  Cg_AddSample(cgi.stage, &(const SoundPlaySample) {
    .sample = cgSampleHookHit,
    .origin = org,
    .pitch = RandomRangei(-4, 5)
  });
}
#endif

/**
 * @brief Parses a temporary entity message from the server and dispatches the appropriate visual effect.
 */
void Cg_ParseTempEntity(void) {
  Vec3 pos, pos2, dir;
  int32_t i, j, k;

  const uint8_t type = cgi.ReadByte();

  switch (type) {

    case TE_TELEPORT:
      pos = cgi.ReadPosition();
      Cg_TeleporterEffect(pos);
      break;

    case TE_BLASTER:
      pos = cgi.ReadPosition();
      dir = cgi.ReadDir();
      i = cgi.ReadByte();
      Cg_BlasterEffect(pos, dir, Cg_ClientEffectColor(i, NULL, color_hue_orange));
      break;

    case TE_TRACER:
      pos = cgi.ReadPosition();
      pos2 = cgi.ReadPosition();
      Cg_TracerEffect(pos, pos2);
      break;

    case TE_BULLET: // bullet hitting wall
      pos = cgi.ReadPosition();
      dir = cgi.ReadDir();
      Cg_BulletEffect(pos, dir);
      break;

    case TE_NAIL: // nail hitting wall
      pos = cgi.ReadPosition();
      dir = cgi.ReadDir();
      Cg_NailEffect(pos, dir);
      break;

    case TE_BLOOD: // projectile hitting flesh
      pos = cgi.ReadPosition();
      dir = cgi.ReadDir();
      i = cgi.ReadByte();
      Cg_BloodEffect(pos, dir, i);
      break;

    case TE_GIB: // player over-death
      pos = cgi.ReadPosition();
      Cg_GibEffect(pos, 12);
      break;

    case TE_SPARKS: // player damage sparks
      pos = cgi.ReadPosition();
      dir = cgi.ReadDir();
      i = cgi.ReadByte();
      Cg_SparksEffect(pos, dir, 12 * i);
      break;

    case TE_HYPERBLASTER: // hyperblaster hitting wall
      pos = cgi.ReadPosition();
      dir = cgi.ReadDir();
      Cg_HyperblasterEffect(pos, dir);
      break;

    case TE_LIGHTNING_DISCHARGE: // lightning discharge in water
      pos = cgi.ReadPosition();
      Cg_LightningDischargeEffect(pos);
      break;

    case TE_RAIL: { // railgun effect
      pos = cgi.ReadPosition();
      pos2 = cgi.ReadPosition();
      dir = cgi.ReadDir();
      const int32_t flags = cgi.ReadLong();
      const int32_t client = cgi.ReadByte();
      float hue;
      Cg_ClientEffectColor(client, &hue, color_hue_cyan);
      if (client == cgi.client->frame.ps.client && !cgi.client->thirdPerson) {
        pos = cgState.clients[client].weaponMuzzle;
      }
      Cg_RailEffect(pos, pos2, dir, flags, hue);
      break;
    }

    case TE_EXPLOSION: // rocket and grenade explosions
      pos = cgi.ReadPosition();
      dir = cgi.ReadDir();
      Cg_ExplosionEffect(pos, dir);
      break;

    case TE_BFG_LASER:
      i = cgi.ReadShort();
      j = cgi.ReadShort();
      if (i < 0 || i >= MAX_ENTITIES || j < 0 || j >= MAX_ENTITIES) {
        Cg_Warn("TE_BFG_LASER: invalid entity index %d, %d\n", i, j);
        break;
      }
      Cg_BfgLaserEffect(j, i);
      break;

    case TE_BFG_LASER_DEAD:
      i = cgi.ReadShort();
      j = cgi.ReadShort();
      if (i < 0 || i >= MAX_ENTITIES || j < 0 || j >= MAX_ENTITIES) {
        Cg_Warn("TE_BFG_LASER_DEAD: invalid entity index %d, %d\n", i, j);
        break;
      }
      Cg_BfgLaserDeadEffect(j, i);
      break;

    case TE_BFG: // bfg explosion
      pos = cgi.ReadPosition();
      Cg_BfgEffect(pos);
      break;

    case TE_BUBBLES: // bubbles chasing projectiles in water
      pos = cgi.ReadPosition();
      pos2 = cgi.ReadPosition();
      i = cgi.ReadByte();
      Cg_BubbleTrail(NULL, pos, pos2, (float) i);
      break;

    case TE_RIPPLE: // liquid surface ripples
      pos = cgi.ReadPosition();
      dir = cgi.ReadDir();
      i = cgi.ReadLong();
      j = cgi.ReadByte();
      k = cgi.ReadByte();
      Cg_RippleSplashEffect(pos, dir, i, j, k);
      break;

#if defined(G_HOOK)
    case TE_HOOK_IMPACT: // grapple hook impact
      pos = cgi.ReadPosition();
      dir = cgi.ReadDir();
      Cg_HookImpactEffect(pos, dir);
      break;
#endif


    case TE_AI_NODE: // AI node debug
      pos = cgi.ReadPosition();
      j = cgi.ReadShort();
      i = cgi.ReadByte();
      Cg_AiNodeEffect(pos, i, j);
      break;

    case TE_AI_NODE_LINK: // AI node debug
      pos = cgi.ReadPosition();
      pos2 = cgi.ReadPosition();
      i = cgi.ReadByte();
      Cg_AiNodeLinkEffect(pos, pos2, i);
      break;

    default:
      Cg_Warn("Unknown type: %d\n", type);
      return;
  }
}
