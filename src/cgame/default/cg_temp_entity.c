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
void Cg_AddDecal(const r_decal_t *decal) {

  if (!cg_add_decals->value) {
    return;
  }

  cgi.AddDecal(cgi.view, decal);
}

/**
 * @brief
 */
static void Cg_BlasterEffect(const vec3_t org, const vec3_t dir, const vec3_t color) {

  for (int32_t i = 0; i < 2; i++) {
    const float saturation = RandomRangef(.8f, 1.f);

    // surface aligned blast ring sprite
    Cg_AddSprite(&(cg_sprite_t) {
      .animation = cg_sprite_blaster_ring,
      .lifetime = Cg_AnimationLifetime(cg_sprite_blaster_ring, 17.5f),
      .origin = Vec3_Fmaf(org, 3.f, dir),
      .size = 22.5f,
      .size_velocity = 75.f,
      .dir = (i == 1) ? dir : Vec3_Zero(),
      .color = Vec3_Scale(color, saturation),
    });
  }

  // radial particles
  for (int32_t i = 0; i < 32; i++) {

    const vec3_t velocity = Vec3_RandomizeDir(Vec3_Scale(dir, 125.f), .6666f);

    Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_particle,
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

    Cg_AddSprite(&(cg_sprite_t) {
      .animation = cg_sprite_blaster_flame,
      .lifetime = Cg_AnimationLifetime(cg_sprite_blaster_flame, 30),
      .origin = Vec3_Fmaf(org, 5.f, Vec3_RandomDir()),
      .rotation = RandomRadian(),
      .rotation_velocity = Randomf() * .1f,
      .size = 25.f,
      .color = color,
    });
  }

  // surface flame
  const float flame_sat = RandomRangef(.8f, 1.f);

  Cg_AddSprite(&(cg_sprite_t) {
    .animation = cg_sprite_blaster_flame,
    .lifetime = Cg_AnimationLifetime(cg_sprite_blaster_flame, 30),
    .origin = Vec3_Fmaf(org, 5.f, Vec3_RandomDir()),
    .rotation = RandomRadian(),
    .rotation_velocity = Randomf() * .1f,
    .dir = dir,
    .size = 25.f,
    .size_velocity = 20.f,
    .color = Vec3_Scale(color, flame_sat),
  });

  Cg_AddDecal(&(r_decal_t) {
    .image = cg_decal_burn[Randomi() % lengthof(cg_decal_burn)],
    .origin = org,
    .radius = RandomRangef(6.f, 12.f),
    .color = Color3fv(color),
    .lifetime = 3000 + Randomf() * 3000,
    .rotation = RandomRadian()
  });

  Cg_AddLight(&(const cg_light_t) {
    .origin = Vec3_Fmaf(org, 8.f, dir),
    .radius = 180.f,
    .color = color,
    .intensity = 3.f,
    .decay = 350.f
  });

  Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
    .sample = cg_sample_blaster_hit,
    .origin = org,
    .atten = SOUND_ATTEN_LINEAR,
  });
}

/**
 * @brief
 */
static void Cg_TracerEffect(const vec3_t start, const vec3_t end) {;
  const float tracer_speed = 4000.f;
  const float tracer_length = 120.f;
  float len;
  vec3_t velocity = Vec3_NormalizeLength(Vec3_Subtract(end, start), &len);
  const uint32_t lifetime = SECONDS_TO_MILLIS((len - tracer_length) / tracer_speed);

  Cg_AddSprite(&(cg_sprite_t) {
    .type = SPRITE_BEAM,
    .image = cg_beam_tracer,
    .origin = start,
    .termination = Vec3_Fmaf(start, tracer_length, velocity),
    .size = 2.5f,
    .velocity = Vec3_Scale(velocity, tracer_speed),
    .lifetime = lifetime,
    .color = Vec3(1.f, .9f, .6f),
  });
}

/**
 * @brief
 */
static void Cg_AiNodeEffect(const vec3_t start, const uint8_t color, const uint16_t id) {
  const uint8_t color_id = color & 0x7;

  const float hue = color_id == 3
    ? color_hue_red
    : color_id == 2
      ? color_hue_rose
      : color_id == 1
        ? color_hue_yellow
        : color_hue_orange;

  if (color & 16) {
    Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_node_wait,
      .origin = Vec3_Add(start, Vec3(0, 0, 16.f)),
      .flags = SPRITE_SERVER_TIME,
      .lifetime = 1,
      .size = 8.f,
      .color = Vec3(1.f, 1.f, 1.f),
    });
  }

  const vec3_t c = ColorHSV(hue, 1.f, 1.f).vec3;

  Cg_AddSprite(&(cg_sprite_t) {
    .atlas_image = cg_sprite_particle,
    .origin = start,
    .flags = SPRITE_SERVER_TIME,
    .size = 4.f,
    .color = c,
  });

  // draw bbox representation
  cm_trace_t tr = cgi.Trace(start, Vec3_Subtract(start, Vec3(0, 0, MAX_WORLD_DIST)), PM_BOUNDS, NULL, CONTENTS_MASK_CLIP_PLAYER | CONTENTS_MASK_LIQUID);

  if (tr.start_solid) {
    tr = cgi.Trace(start, Vec3_Subtract(start, Vec3(0, 0, MAX_WORLD_DIST)), PM_CROUCHED_BOUNDS, NULL, CONTENTS_MASK_CLIP_PLAYER | CONTENTS_MASK_LIQUID);
  }

  box3_t box = Box3_Translate(PM_BOUNDS, tr.end);
  box.maxs.z = box.mins.z = tr.end.z + PM_BOUNDS.mins.z;
  vec3_t points[8];
  Box3_ToPoints(box, points);

  Cg_AddSprite(&(cg_sprite_t) {
    .type = SPRITE_BEAM,
    .image = cg_beam_line,
    .origin = points[0],
    .termination = points[1],
    .size = 1.5f,
    .flags = SPRITE_SERVER_TIME,
    .color = c,
  });

  Cg_AddSprite(&(cg_sprite_t) {
    .type = SPRITE_BEAM,
    .image = cg_beam_line,
    .origin = points[0],
    .termination = points[2],
    .size = 1.5f,
    .flags = SPRITE_SERVER_TIME,
    .color = c,
  });

  Cg_AddSprite(&(cg_sprite_t) {
    .type = SPRITE_BEAM,
    .image = cg_beam_line,
    .origin = points[3],
    .termination = points[1],
    .size = 1.5f,
    .flags = SPRITE_SERVER_TIME,
    .color = c,
  });

  Cg_AddSprite(&(cg_sprite_t) {
    .type = SPRITE_BEAM,
    .image = cg_beam_line,
    .origin = points[3],
    .termination = points[2],
    .size = 1.5f,
    .flags = SPRITE_SERVER_TIME,
    .color = c,
  });
}

/**
 * @brief
 */
static void Cg_AiNodeLinkEffect(const vec3_t start, const vec3_t end, const uint8_t bits) {

  const float satval = (bits & 4) ? 0.2f : 1.0f;
  const vec3_t both_color = ColorHSV(color_hue_green, satval, satval).vec3;
  const vec3_t a_color = ColorHSV(color_hue_blue, satval, satval).vec3;
  const vec3_t mover_color = ColorHSV(color_hue_cyan, satval, satval).vec3;

  // mover connection
  if (bits & 8) {
    Cg_AddSprite(&(cg_sprite_t) {
      .type = SPRITE_BEAM,
      .image = cg_beam_hook,
      .origin = start,
      .termination = end,
      .size = 2.f,
      .flags = SPRITE_SERVER_TIME,
      .color = mover_color,
    });

    return;
  }

  vec3_t center = Vec3_Scale(Vec3_Add(start, end), 0.5f);
  const vec3_t euler = Vec3_Euler(Vec3_Normalize(Vec3_Subtract(start, end)));
  vec3_t up;
  Vec3_Vectors(euler, NULL, NULL, &up);
  vec3_t text_center = center;
  text_center.z += 8.f;

  //Cg_DrawFloatingStringLine(text_center, va("%.1f", Vec3_Distance(start, end)), 1.f, Vec3(0.f, 0.f, 1.f));

  if (bits & 16) {
    text_center.z -= 2;
  //  Cg_DrawFloatingStringLine(text_center, "Slow-drop", 1.f, Vec3(0.f, 0.f, 1.f));
    Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_node_slow,
      .origin = text_center,
      .flags = SPRITE_SERVER_TIME,
      .lifetime = 1,
      .size = 8.f,
      .color = Vec3(1.f, 1.f, 1.f),
    });
  }

  // both sides connected
  if ((bits & 3) == 3) {
    Cg_AddSprite(&(cg_sprite_t) {
      .type = SPRITE_BEAM,
      .image = cg_beam_line,
      .origin = start,
      .termination = end,
      .size = 2.f,
      .flags = SPRITE_SERVER_TIME,
      .color = both_color,
    });

  } else {
    
    // only end connected
    if (bits & 2) {
      Cg_AddSprite(&(cg_sprite_t) {
        .type = SPRITE_BEAM,
        .image = cg_beam_arrow,
        .origin = end,
        .termination = start,
        .size = 2.f,
        .flags = SPRITE_SERVER_TIME,
        .color = a_color,
      });
    } else {
      Cg_AddSprite(&(cg_sprite_t) {
        .type = SPRITE_BEAM,
        .image = cg_beam_arrow,
        .origin = start,
        .termination = end,
        .size = 2.f,
        .flags = SPRITE_SERVER_TIME,
        .color = a_color,
      });
    }
  }
}

/**
 * @brief
 */
static void Cg_BulletEffect(const vec3_t org, const vec3_t dir) {
  static uint32_t last_ric_time;

  if (cgi.PointContents(org) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(NULL, org, Vec3_Fmaf(org, 8.f, dir), 2.f);
  } else {

    float spark_life = 200.f;
    float spark_size = RandomRangef(35.f, 45.f);

    // spark spikes billboard
    Cg_AddSprite(&(cg_sprite_t) {
      .animation = cg_sprite_impact_spark_01,
      .origin = Vec3_Fmaf(org, 2.f, dir),
      .rotation = RandomRadian(),
      .size = spark_size,
      .lifetime = spark_life,
      .color = Vec3(1.f, 1.f, 1.f),
    });

    // spark spikes decal
    Cg_AddSprite(&(cg_sprite_t) {
      .animation = cg_sprite_impact_spark_01,
      .origin = Vec3_Fmaf(org, 2.f, dir),
      .rotation = RandomRadian(),
      .size = spark_size,
      .lifetime = spark_life,
      .color = Vec3(1.f, 1.f, 1.f),
      .dir = dir
    });

    // spark dots
    vec3_t spark_origin = Vec3_Fmaf(org, 2.f, dir);
    for (int32_t i = 0; i < 6; i++) {
      float size = RandomRangef(3.f, 6.f);
      float lifetime = RandomRangef(150.f, 300.f);
      Cg_AddSprite(&(cg_sprite_t) {
        .atlas_image = cg_sprite_impact_spark_01_dot,
        .origin = spark_origin,
        .velocity = Vec3_Scale(Vec3_Mix(Vec3_RandomDir(), dir, 0.33f), 55.f),
        .size = size,
        .size_velocity = -size * 5.f,
        .lifetime = lifetime,
        .color = Vec3(1.f, 1.f, 1.f),
      });
    }

    // impact smoke
    Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_puff_cloud,
      .origin = Vec3_Fmaf(org, 5.f, dir),
      .velocity.z = 10.0f,
      .size = RandomRangef(30.f, 50.f),
      .rotation = RandomRadian(),
      .size_velocity = 60.0f,
      .lifetime = 800.f,
      .color = Vec3(1.f, 1.f, 1.f),
      .lighting = 0.65f
    });

    // impact hotness
    Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_spark,
      .origin = Vec3_Fmaf(org, 0.5f, dir),
      .rotation = RandomRadian(),
      .dir = dir,
      .size = 4.f,
      .lifetime = 650,
      .color = ColorHSV(color_hue_orange, 0.8f, 1.f).vec3,
    });

    Cg_AddDecal(&(r_decal_t) {
      .image = cg_decal_bullet[Randomi() % lengthof(cg_decal_bullet)],
      .origin = org,
      .radius = RandomRangef(1.f, 3.f),
      .color = color_black,
      .lifetime = 12000 + Randomf() * 10000,
      .rotation = RandomRadian()
    });
  }

  if (cgi.client->unclamped_time < last_ric_time) {
    last_ric_time = 0;
  }

  if (cgi.client->unclamped_time - last_ric_time > 300) {
    last_ric_time = cgi.client->unclamped_time;

    Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
      .sample = cg_sample_machinegun_hit[RandomRangeu(0, 3)],
      .origin = org,
      .atten = SOUND_ATTEN_LINEAR,
      .pitch = RandomRangei(-8, 9)
    });
  }
}

/**
 * @brief
 */
static void Cg_BloodEffect(const vec3_t org, const vec3_t dir, int32_t count) {

  for (int32_t i = 0; i < count; i += 3) {

    if (!Cg_AddSprite(&(cg_sprite_t) {
        .animation = cg_sprite_blood_01,
        .lifetime = Cg_AnimationLifetime(cg_sprite_blood_01, 30) + Randomf() * 500,
        .size = RandomRangef(48.f, 64.f),
        .rotation = RandomRadian(),
        .origin = Vec3_Fmaf(Vec3_Add(org, Vec3_RandomRange(-10.f, 10.f)), RandomRangef(0.f, 32.f), dir),
        .velocity = Vec3_RandomRange(-30.f, 30.f),
        .acceleration.z = -SPRITE_GRAVITY / 2.0,
        .color = Vec3(1.f, 1.f, 1.f),
        .lighting = .8f,
      })) {
      break;
    }

    Cg_AddDecal(&(r_decal_t) {
      .image = cg_decal_blood[Randomi() % lengthof(cg_decal_blood)],
      .origin = org,
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
 * @brief
 */
void Cg_GibEffect(const vec3_t org, int32_t count) {

  // if a player has died underwater, emit some bubbles
  if (cgi.PointContents(org) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(NULL, org, Vec3_Add(org, Vec3(0.f, 0.f, 64.f)), 1.f);
  }

  for (int32_t i = 0; i < count; i++) {

    // set the origin and velocity for each gib stream
    const vec3_t o = Vec3_Add(Vec3(RandomRangef(-8.f, 8.f), RandomRangef(-8.f, 8.f), RandomRangef(-4.f, 20.f)), org);
    const vec3_t v = Vec3(RandomRangef(-1.f, 1.f), RandomRangef(-1.f, 1.f), RandomRangef(.2f, 1.2f));

    float dist = GIB_STREAM_DIST;
    vec3_t tmp = Vec3_Fmaf(o, dist, v);

    const cm_trace_t tr = cgi.Trace(o, tmp, Box3_Zero(), NULL, CONTENTS_MASK_CLIP_PROJECTILE);
    dist = GIB_STREAM_DIST * tr.fraction;

    for (int32_t j = 1; j < GIB_STREAM_COUNT; j++) {

      if (!Cg_AddSprite(&(cg_sprite_t) {
          .animation = cg_sprite_blood_01,
          .lifetime = Cg_AnimationLifetime(cg_sprite_blood_01, 30) + Randomf() * 500,
          .origin = o,
          .velocity = Vec3_Add(Vec3_Add(Vec3_Scale(v, dist * ((float)j / GIB_STREAM_COUNT)), Vec3_RandomRange(-2.f, 2.f)), Vec3(0.f, 0.f, 100.f)),
          .acceleration.z = -SPRITE_GRAVITY * 2.0,
          .size = RandomRangef(24.f, 56.f),
          .color = Vec3(1.f, 1.f, 1.f),
          .lighting = 1.f
        })) {
        break;
      }
    }
  }

  Cg_AddDecal(&(r_decal_t) {
    .image = cg_decal_blood[Randomi() % lengthof(cg_decal_blood)],
    .origin = org,
    .radius = RandomRangef(64.f, 128.f),
    .color = color_red,
    .lifetime = 6000 + Randomf() * 6000,
    .rotation = RandomRadian()
  });

  Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
    .sample = cg_sample_gib,
    .origin = org,
    .atten = SOUND_ATTEN_LINEAR,
  });
}

/**
 * @brief
 */
void Cg_SparksEffect(const vec3_t org, const vec3_t dir, int32_t count) {

  for (int32_t i = 0; i < count; i++) {
    const float hue = color_hue_yellow - RandomRangef(4.f, 40.f);

    if (!Cg_AddSprite(&(cg_sprite_t) {
        .atlas_image = cg_sprite_spark,
        .origin = Vec3_Add(org, Vec3_RandomRange(-4.f, 4.f)),
        .velocity = Vec3_Scale(Vec3_RandomizeDir(dir, .33f), RandomRangef(64.f, 128.f)),
        .acceleration.z = -SPRITE_GRAVITY,
        .lifetime = 1000 + Randomf() * 1000,
        .size = RandomRangef(.5f, 3.f),
        .size_velocity = -1.f,
        .rotation = RandomRadian(),
        .rotation_velocity = 1.f,
        .bounce = .3f,
        .color = ColorHSV(hue, .4f, 1.f).vec3,
        .lighting = 1.f,
      })) {
      break;
    }
  }

  Cg_AddLight(&(const cg_light_t) {
    .origin = org,
    .radius = 100.0,
    .color = Vec3(.7f, .5f, .5f),
    .intensity = 1.f,
    .decay = RandomRangeu(120, 180),
  });

  Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
    .sample = cg_sample_sparks,
    .origin = org,
    .atten = SOUND_ATTEN_SQUARE,
  });
}

/**
 * @brief
 */
static void Cg_ExplosionEffect(const vec3_t org, const vec3_t dir) {

  if (cgi.PointContents(org) & CONTENTS_MASK_LIQUID) {
    for (int32_t i = 0; i < 16; i++) {

      const vec3_t start = Vec3_Add(org, Vec3_RandomRange(-16.f, 16.f));
      const vec3_t end = Vec3_Fmaf(start, RandomRangef(64.f, 192.f), Vec3_RandomDir());

      Cg_BubbleTrail(NULL, start, end, 1.f);
    }
  } else {
    // ember sparks
    for (int32_t i = 0; i < 150; i++) {
      const uint32_t lifetime = 3000 + Randomf() * 500;
      const float size = 2.f + Randomf() * 2.f;
      const float hue = RandomRangef(10.f, 50.f);

      if (!Cg_AddSprite(&(cg_sprite_t) {
          .atlas_image = cg_sprite_particle2,
          .origin = Vec3_Add(org, Vec3_RandomRange(-16.f, 16.f)),
          .velocity = Vec3_RandomRange(-400.f, 400.f),
          .acceleration.z = -SPRITE_GRAVITY * 2.f,
          .lifetime = lifetime,
          .size = size,
          .size_velocity = -size / MILLIS_TO_SECONDS(lifetime),
          .bounce = .4f,
          .color = ColorHSV(hue, 1.f, 1.f).vec3,
          .lighting = .5f,
        })) {
        break;
      }
    }
  }

  // billboard explosion 1
  Cg_AddSprite(&(cg_sprite_t) {
    .origin = org,
    .animation = cg_sprite_explosion,
    .lifetime = Cg_AnimationLifetime(cg_sprite_explosion, 40),
    .size = 150.f,
    .size_velocity = 40.f,
    .rotation = RandomRadian(),
    .color = Vec3(1.f, 1.f, 1.f),
  });

  // billboard explosion 2
  Cg_AddSprite(&(cg_sprite_t) {
    .origin = org,
    .animation = cg_sprite_explosion,
    .lifetime = Cg_AnimationLifetime(cg_sprite_explosion, 30),
    .size = 250.f,
    .size_velocity = 40.f,
    .rotation = RandomRadian(),
    .color = Vec3(1.f, 1.f, 1.f),
  });

  // decal explosion
  Cg_AddSprite(&(cg_sprite_t) {
    .origin = org,
    .animation = cg_sprite_explosion,
    .lifetime = Cg_AnimationLifetime(cg_sprite_explosion, 30),
    .size = 250.f,
    .size_velocity = 40.f,
    .rotation = RandomRadian(),
    .color = Vec3(1.f, 1.f, 1.f),
    .dir = dir
  });

  // decal blast ring
  Cg_AddSprite(&(cg_sprite_t) {
    .origin = org,
    .animation = cg_sprite_explosion_ring_02,
    .lifetime = Cg_AnimationLifetime(cg_sprite_explosion_ring_02, 20),
    .size = 100.f,
    .size_velocity = 700.f,
    .size_acceleration = -700.f,
    .rotation = RandomRadian(),
    .color = Vec3(1.f, 1.f, 1.f),
    .dir = dir
  });

  // blast glow
  Cg_AddSprite(&(cg_sprite_t) {
    .origin = org,
    .lifetime = 600,
    .size = 300.f,
    .rotation = RandomRadian(),
    .atlas_image = cg_sprite_explosion_glow,
    .color = Vec3(1.f, 1.f, 1.f),
    .lighting = 1.f
  });

  Cg_AddDecal(&(r_decal_t) {
    .image = cg_decal_burn[Randomi() % lengthof(cg_decal_burn)],
    .origin = org,
    .radius = RandomRangef(32.f, 64.f),
    .color = Color4f(0.f, 0.f, 0.f, .5f + Randomf() * .4f),
    .lifetime = 16000 + Randomf() * 8000,
    .rotation = RandomRadian()
  });

  // secondary blast glow (smaller + long last)
  Cg_AddSprite(&(cg_sprite_t) {
      .origin = org,
          .lifetime = 1500,
          .size = 72.f,
          .rotation = RandomRadian(),
          .atlas_image = cg_sprite_explosion_glow,
          .color = Vec3(.9f, .6f, .3f),
          .lighting = 1.f
  });

  Cg_AddLight(&(const cg_light_t) {
    .origin = org,
    .radius = 360.0,
    .color = Vec3(.9f, .6f, .3f),
    .intensity = 8.f,
    .decay = 1600
  });

  Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
    .sample = cg_sample_explosion,
    .origin = org,
    .atten = SOUND_ATTEN_LINEAR,
  });
}

/**
 * @brief
 */
static void Cg_HyperblasterEffect(const vec3_t org, const vec3_t dir) {
  
  vec3_t color = ColorHSV(204.f, .6f, 1.f).vec3;

  // impact "splash"
  for (uint32_t i = 0; i < 6; i++) {
    Cg_AddSprite(&(cg_sprite_t) {
      .origin = org,
      .animation = cg_sprite_electro_01,
      .lifetime = Cg_AnimationLifetime(cg_sprite_electro_01, 20),
      .size = 50.f,
      .size_velocity = 400.f,
      .rotation = RandomRadian(),
      .dir = Vec3_RandomRange(-1.f, 1.f),
      .color = color,
      .lighting = .3f
    });
  }

  Cg_AddSprite(&(cg_sprite_t) {
    .origin = org,
    .animation = cg_sprite_electro_01,
    .lifetime = Cg_AnimationLifetime(cg_sprite_electro_01, 8),
    .size = 100.f,
    .size_velocity = 25.f,
    .rotation = RandomRadian(),
    .dir = dir,
    .color = color,
    .lighting = 1.f
  });

  // impact flash
  for (uint32_t i = 0; i < 2; i++) {
    Cg_AddSprite(&(cg_sprite_t) {
      .origin = org,
      .atlas_image = cg_sprite_flash,
      .lifetime = 150,
      .size = RandomRangef(75.f, 100.f),
      .rotation = RandomRadian(),
      .rotation_velocity = i == 0 ? .66f : -.66f,
      .color = color,
    });
  }

  Cg_AddDecal(&(r_decal_t) {
    .image = cg_decal_burn[Randomi() % lengthof(cg_decal_burn)],
    .origin = org,
    .radius = RandomRangef(12.f, 18.f),
    .color = Color3fv(ColorHSV(225.f, .7f, .8f).vec3),
    .lifetime = 3000 + Randomf() * 3000,
    .rotation = RandomRadian()
  });

  Cg_AddLight(&(cg_light_t) {
    .origin = Vec3_Add(org, dir),
    .radius = 200.f,
    .color = Vec3(.4f, .7f, 1.f),
    .intensity = 3.f,
    .decay = 250,
  });

  Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
    .sample = cg_sample_hyperblaster_hit,
    .origin = Vec3_Add(org, dir),
    .atten = SOUND_ATTEN_LINEAR,
  });
}

/**
 * @brief
 */
static void Cg_LightningDischargeEffect(const vec3_t org) {
  for (int32_t i = 0; i < 40; i++) {
    Cg_BubbleTrail(NULL, org, Vec3_Add(org, Vec3_RandomRange(-48.f, 48.f)), 1.f);
  }

  Cg_AddLight(&(const cg_light_t) {
    .origin = org,
    .radius = 160.f,
    .color = Vec3(.6f, .6f, 1.f),
    .intensity = 1.f,
    .decay = 750
  });

  Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
    .sample = cg_sample_lightning_discharge,
    .origin = org,
    .atten = SOUND_ATTEN_LINEAR,
  });

  
}

/**
 * @brief
 */
static void Cg_RailEffect(const vec3_t start, const vec3_t end, const vec3_t dir, int32_t flags, float hue) {

  const vec3_t color = ColorHSV(hue, 1.f, 1.f).vec3;

  Cg_AddLight(&(cg_light_t) {
    .origin = start,
    .radius = 140.f,
    .color = color,
    .intensity = 5.f,
    .decay = 240,
  });

  if (cgi.BoxContents(Box3_FromPoints((const vec3_t[]) { start, end }, 2)) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(NULL, start, end, 1.f);
  }

  vec3_t forward;
  const float dist = Vec3_DistanceDir(end, start, &forward);
  const vec3_t right = Vec3(forward.z, -forward.x, forward.y);
  const vec3_t up = Vec3_Cross(forward, right);

  const uint32_t core_lifetime = 500;
  const uint32_t vapor_lifetime = 900;

  for (int32_t i = 0; i < dist; i++) {
    const float cosi = cosf(i * 0.1f);
    const float sini = sinf(i * 0.1f);

    const vec3_t org = Vec3_Add(Vec3_Add(Vec3_Fmaf(start, i, forward), Vec3_Scale(right, cosi)), Vec3_Scale(up, sini));
    const vec3_t accel = Vec3_Add(Vec3_Add(Vec3_Scale(right, cosi * 32.f), Vec3_Scale(up, sini * 32.f)), Vec3_Up());

    if (cgi.PointContents(org) & CONTENTS_MASK_LIQUID) {
      if (Randomb()) {
        continue;
      }
    }

    Cg_AddSprite(&(cg_sprite_t) {
        .atlas_image = cg_sprite_particle3,
            .origin = org,
            .velocity = Vec3_Scale(forward, 256.f),
            .acceleration = Vec3_Add(accel, Vec3_Scale(Vec3_RandomDir(), 192.f)),
            .friction = 2048.f,
            .lifetime = core_lifetime + Randomf() * 120,
            .size = 1.f,
            .size_velocity = 1.0 / MILLIS_TO_SECONDS(core_lifetime),
            .color = Color3f(color.x / 2, color.y / 2, color.z / 2).vec3,
            .lighting = .2f,
    });

    if (i % 3 == 0) {
      const int32_t h = (int32_t) hue + RandomRangei(10, 20) % 360;
      const vec3_t alt_color = ColorHSV(h, 0.f, 1.f).vec3;
      Cg_AddSprite(&(cg_sprite_t) {
        .atlas_image = cg_sprite_particle3,
        .origin = org,
        .velocity = Vec3_Scale(Vec3_RandomDir(), 64.f),
        .acceleration = Vec3(RandomRangef(-64.f, 64.f), RandomRangef(-64.f, 64.f), -SPRITE_GRAVITY),
        .lifetime = vapor_lifetime + Randomf() * 160,
        .size = 2.5f,
        .size_velocity = -.5f / MILLIS_TO_SECONDS(vapor_lifetime),
        .color = alt_color,
        .lighting = 1.f,
      });
    }
  }

  // Core beam
  Cg_AddSprite(&(cg_sprite_t) {
    .type = SPRITE_BEAM,
    .origin = start,
    .termination = end,
    .image = cg_beam_rail,
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
  Cg_AddLight(&(cg_light_t) {
    .origin = Vec3_Fmaf(end, 1.f, dir),
    .radius = 12.f,
    .color = color,
    .intensity = 10.f,
    .decay = 800.f,
  });

  // hit billboards
  for (int32_t i = 0; i < 2; i++) {
    Cg_AddSprite(&(cg_sprite_t) {
      .origin = Vec3_Add(end, dir),
      .atlas_image = cg_sprite_flash,
      .lifetime = 250,
      .size = 120.f,
      .size_velocity = RandomRangef(100.f, 200.f),
      .rotation = RandomRadian(),
      .rotation_velocity = i == 0 ? .66f : -.66f,
      .color = color,
    });
  }

  // slug debris
  if ((cgi.PointContents(end) & CONTENTS_MASK_LIQUID) == 0) {

    for (int32_t i = 0; i < 32; i++) {
      const uint32_t lifetime = 2000 + Randomf() * 300;
      const float size = 2.f + Randomf();

      if (!Cg_AddSprite(&(cg_sprite_t) {
          .atlas_image = cg_sprite_particle2,
          .origin = Vec3_Add(end, Vec3_RandomRange(-4.f, 4.f)),
          .velocity = Vec3_RandomRange(-200.f, 200.f),
          .acceleration.z = -SPRITE_GRAVITY * 2.f,
          .lifetime = lifetime,
          .size = size,
          .size_velocity = -size / MILLIS_TO_SECONDS(lifetime),
          .bounce = .4f,
          .color = color,
          .lighting = .5f,
        })) {
        break;
      }
    }
  }

  Cg_AddDecal(&(r_decal_t) {
    .image = cg_decal_slug[Randomi() % lengthof(cg_decal_burn)],
    .origin = end,
    .radius = RandomRangef(8.f, 12.f),
    .color = Color3fv(color),
    .lifetime = 12000 + Randomf() * 4000,
    .rotation = RandomRadian()
  });
}

/**
 * @brief 
 */
typedef union {
  struct {
    uint16_t org, dest;
  };

  void *data;
} cg_bfg_laser_data_t;

/**
 * @brief
 */
static void Cg_BfgLaserThink(cg_sprite_t *sprite, float life, float delta) {

  const cg_bfg_laser_data_t data = { .data = sprite->data };

  const vec3_t org = cgi.client->entities[data.org].origin;
  const vec3_t end = cgi.client->entities[data.dest].origin;

  sprite->origin = org;
  sprite->termination = end;

  if (cgi.BoxContents(Box3_FromPoints((const vec3_t[]) { org, end }, 2)) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(NULL, org, end, 4.f);
  }
}

/**
 * @brief
 */
static void Cg_BfgLaserEffect(const int16_t org_entity, const int16_t dest_entity) {

  const vec3_t org = cgi.client->entities[org_entity].origin;
  const vec3_t end = cgi.client->entities[dest_entity].origin;

  Cg_AddSprite(&(cg_sprite_t) {
    .type = SPRITE_BEAM,
    .image = cg_beam_rail,
    .origin = org,
    .termination = end,
    .size = 5.f,
    .flags = SPRITE_SERVER_TIME | SPRITE_DATA_NOFREE,
    .color = ColorHSV(color_hue_green, 1.f, 1.f).vec3,
    .data = ((cg_bfg_laser_data_t) { .org = org_entity, .dest = dest_entity }).data,
    .Think = Cg_BfgLaserThink,
    .lighting = .5f,
  });

  Cg_AddLight(&(cg_light_t) {
    .origin = end,
    .radius = 150.0,
    .color = Vec3(.8f, 1.f, .5f),
    .intensity = 3.f,
    .decay = 50,
  });

  Cg_AddDecal(&(r_decal_t) {
    .image = cg_decal_burn[Randomi() % lengthof(cg_decal_burn)],
    .origin = end,
    .radius = RandomRangef(12.f, 24.f),
    .color = ColorHSV(color_hue_green, 1.f, 1.f),
    .lifetime = 10000 + Randomf() * 4000,
    .rotation = RandomRadian()
  });
}

/**
 * @brief
 */
static void Cg_BfgEffect(const vec3_t org) {
  // explosion 1
  for (int32_t i = 0; i < 4; i++) {

    Cg_AddSprite(&(cg_sprite_t) {
      .animation = cg_sprite_bfg_explosion_2,
      .lifetime = Cg_AnimationLifetime(cg_sprite_bfg_explosion_2, 15),
      .size = RandomRangef(200.f, 300.f),
      .size_velocity = 100.f,
      .size_acceleration = -10.f,
      .rotation = RandomRangef(0.f, 2.f * M_PI),
      .origin = Vec3_Fmaf(org, 50.f, Vec3_RandomDir()),
      .color = Vec3(1.f, 1.f, 1.f),
    });
  }

  // explosion 2
  for (int32_t i = 0; i < 4; i++) {

    Cg_AddSprite(&(cg_sprite_t) {
      .animation = cg_sprite_bfg_explosion_3,
      .lifetime = Cg_AnimationLifetime(cg_sprite_bfg_explosion_3, 15),
      .size = RandomRangef(200.f, 300.f),
      .size_velocity = 100.f,
      .size_acceleration = -10.f,
      .rotation = RandomRangef(0.f, 2.f * M_PI),
      .origin = Vec3_Fmaf(org, 50.f, Vec3_RandomDir()),
      .color = Vec3(1.f, 1.f, 1.f),
      .lighting = .5f,
    });
  }

  // impact flash 1
  for (uint32_t i = 0; i < 4; i++) {

    Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_flash,
      .origin = org,
      .lifetime = 600,
      .size = RandomRangef(300, 400),
      .rotation = RandomRadian(),
      .dir = Vec3_Random(),
      .color = Vec3(.6f, 1.f, .6f),
      .lighting = .3f,
    });
  }

  // impact flash 2
  Cg_AddSprite(&(cg_sprite_t) {
    .atlas_image = cg_sprite_flash,
    .origin = org,
    .lifetime = 600,
    .size = 400.f,
    .rotation = RandomRadian(),
    .color = Vec3(.6f, 1.f, .6f),
    .lighting = .3f,
  });

  // glow
  Cg_AddSprite(&(cg_sprite_t) {
    .atlas_image = cg_sprite_particle,
    .origin = org,
    .lifetime = 1000,
    .size = 600.f,
    .rotation = RandomRadian(),
    .color = Vec3(.6f, 1.f, .6f),
    .lighting = .2f,
  });

  Cg_AddDecal(&(r_decal_t) {
    .image = cg_decal_burn[Randomi() % lengthof(cg_decal_burn)],
    .origin = org,
    .radius = RandomRangef(96.f, 128.f),
    .color = Color3f(.6f, 1.f, .6f),
    .lifetime = 18000 + Randomf() * 10000,
    .rotation = RandomRadian()
  });

  Cg_AddLight(&(const cg_light_t) {
    .origin = org,
    .radius = 300.f,
    .color = Vec3(.4f, 1.f, .4f),
    .intensity = 3.f,
    .decay = 1500
  });

  Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
    .sample = cg_sample_bfg_hit,
    .origin = org,
    .atten = SOUND_ATTEN_LINEAR,
  });
}

/**
 * @brief
 */
static void Cg_SplashEffect_Think(cg_sprite_t *s, float life, float delta) {
  s->origin.z += .5f * delta * s->size_velocity;
}

/**
 * @brief
 */
static void Cg_SplashEffect(const r_bsp_brush_side_t *side, const vec3_t org, const vec3_t dir, float size) {

  // vertical spray

  const vec3_t color = side->material->color.vec3;

  const float scale = Clampf01(size / 64.f);

  const uint32_t lifetime = 1800 * scale;

  Cg_AddSprite(&(cg_sprite_t) {
    .atlas_image = cg_sprite_splash_02_03,
    .lifetime = lifetime,
    .origin = Vec3_Fmaf(org, .5f, Vec3(0.f, 0.f, size)),
    .size = size,
    .size_velocity = size * 2.f / MILLIS_TO_SECONDS(lifetime),
    .color = color,
    .lighting = 1.f,
    .Think = Cg_SplashEffect_Think,
  });

  // splash droplets

  for (int32_t i = 0; i < 64 * scale; i++) {
    cg_sprite_t *p = Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_particle,
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
 * @brief
 */
static void Cg_RippleEffect(const r_bsp_brush_side_t *side, const vec3_t org, float size) {

  size *= RandomRangef(0.9f, 1.1f);

  const vec3_t color = side->material->color.vec3;

  float viscosity;
  if (side->contents & CONTENTS_LAVA) {
    viscosity = 10.f;
  } else if (side->contents & CONTENTS_SLIME) {
    viscosity = 20.f;
  } else {
    viscosity = 30.f;
  }

  // center decal
  Cg_AddSprite(&(cg_sprite_t) {
    .animation = cg_sprite_poof_01,
    .lifetime = Cg_AnimationLifetime(cg_sprite_poof_01, 30.0f) * (viscosity * .1f),
    .origin = org,
    .size = size * 8.f,
    .size_velocity = size,
    .dir = Vec3_Up(),
    .rotation = RandomRadian(),
    .color = color,
    .lighting = 1.f
  });

  // ring decal
  Cg_AddSprite(&(cg_sprite_t) {
    .atlas_image = cg_sprite_water_ring,
    .lifetime = 1000.f,
    .origin = org,
    .size = size * 4.f,
    .size_velocity = size * 6.f,
    .rotation = RandomRadian(),
    .dir = Vec3_Up(),
    .color = color,
    .lighting = 1.f
  });
}

/**
 * @brief
 */
static void Cg_RippleSplashEffect(const vec3_t org, const vec3_t dir, int32_t brush_side, float size, bool splash) {

  if (brush_side < 0 || brush_side > cgi.WorldModel()->bsp->num_brush_sides) {
    Cg_Warn("Invalid brush side %d\n", brush_side);
    return;
  }

  const r_bsp_brush_side_t *side = cgi.WorldModel()->bsp->brush_sides + brush_side;

  Cg_RippleEffect(side, org, size);

  if (splash) {
    Cg_SplashEffect(side, org, dir, size);
  }
}

/**
 * @brief
 */
static void Cg_HookImpactEffect(const vec3_t org, const vec3_t dir) {

  for (int32_t i = 0; i < 32; i++) {

    if (!Cg_AddSprite(&(cg_sprite_t) {
        .atlas_image = cg_sprite_particle,
        .origin = Vec3_Add(org, Vec3_RandomRange(-4.f, 4.f)),
        .velocity = Vec3_Add(Vec3_Scale(dir, 9.f), Vec3_RandomRange(-90.f, 90.f)),
        .acceleration = Vec3_Add(Vec3_RandomRange(-2.f, 2.f), Vec3(0.f, 0.f, -0.5f * SPRITE_GRAVITY)),
        .lifetime = 100 + (Randomf() * 150),
        .color = Vec3(1.f, 1.f, .4f),
        .size = 6.4f + Randomf() * 3.2f,
        .lighting = 1.f
      })) {
      break;
    }
  }

  Cg_AddLight(&(const cg_light_t) {
    .origin = Vec3_Add(org, dir),
    .radius = 80.0,
    .color = Vec3(.7f, .5f, .5f),
    .intensity = 1.f,
    .decay = 850
  });

  Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
    .sample = cg_sample_hook_hit,
    .origin = org,
    .atten = SOUND_ATTEN_LINEAR,
    .pitch = RandomRangei(-4, 5)
  });
}

/**
 * @brief
 */
void Cg_ParseTempEntity(void) {
  vec3_t pos, pos2, dir;
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

    case TE_RAIL: // railgun effect
      pos = cgi.ReadPosition();
      pos2 = cgi.ReadPosition();
      dir = cgi.ReadDir();
      const int32_t flags = cgi.ReadLong();
      const int32_t client = cgi.ReadByte();
      float hue;
      Cg_ClientEffectColor(client, &hue, color_hue_cyan);
      Cg_RailEffect(pos, pos2, dir, flags, hue);
      break;

    case TE_EXPLOSION: // rocket and grenade explosions
      pos = cgi.ReadPosition();
      dir = cgi.ReadDir();
      Cg_ExplosionEffect(pos, dir);
      break;

    case TE_BFG_LASER:
      i = cgi.ReadShort();
      j = cgi.ReadShort();
      Cg_BfgLaserEffect(j, i);
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

    case TE_HOOK_IMPACT: // grapple hook impact
      pos = cgi.ReadPosition();
      dir = cgi.ReadDir();
      Cg_HookImpactEffect(pos, dir);
      break;

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
