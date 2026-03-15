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
int32_t Cg_TrailContents(const vec3_t start, const vec3_t end) {
  return cgi.BoxContents(Box3_FromPoints((const vec3_t[]) { start, end }, 2));
}

/**
 * @brief
 */
static inline void Cg_ParticleTrailLifeOffset(vec3_t start, vec3_t end, float speed, float step, float *life_start, float *life_frac) {
  *life_start = (speed - Vec3_Distance(start, end)) / 1000;
  *life_frac = (1.0 - *life_start) * step;
}

/**
 * @brief Calculates the number of "distance" steps that have been travelled between the last recorded trail position & the end.
 * @param end The end of the trail - the current position, basically.
 * @param distance The distance that the trail should spawn particles within.
 * @param ent The entity spawning this trail. If this is NULL, "start" must be pointing to a proper start point.
 * @param trail The trail ID.
 * @param start The trail's output start position.
 * @param dir The trail's output direction.
 * @return The number of whole steps that was travelled by this trail.
*/
static int32_t Cg_TrailCount(const vec3_t end, float freq, cl_entity_t *ent, cl_trail_id_t trail, vec3_t *start, vec3_t *dir) {
  const float dist = Vec3_Distance(end, ent ? ent->trail_origins[trail] : *start);
  static vec3_t _start, _dir;

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
    *start = ent->trail_origins[trail];
    *dir = Vec3_Scale(Vec3_Subtract(end, ent->trail_origins[trail]), 1.0f / dist);
    ent->trail_origins[trail] = Vec3_Fmaf(ent->trail_origins[trail], steps * freq, *dir);
  } else {
    *dir = Vec3_Scale(Vec3_Subtract(end, *start), 1.0f / dist);
  }

  return steps;
}

/**
 * @brief
 */
void Cg_BreathTrail(cl_entity_t *ent) {

  if (ent->animation1.animation < ANIM_TORSO_GESTURE) { // death animations
    return;
  }

  if (cgi.client->unclamped_time < ent->timestamp) {
    return;
  }

  vec3_t pos = ent->origin;

  if (Cg_IsDucking(ent)) {
    pos.z += 18.0;
  } else {
    pos.z += 30.0;
  }

  vec3_t forward;
  Vec3_Vectors(ent->angles, &forward, NULL, NULL);

  pos = Vec3_Fmaf(pos, 8.f, forward);

  const int32_t contents = cgi.PointContents(pos);

  if (contents & CONTENTS_MASK_LIQUID) {
    if ((contents & CONTENTS_MASK_LIQUID) == CONTENTS_WATER) {

      Cg_AddSprite(&(cg_sprite_t) {
        .atlas_image = cg_sprite_bubble,
        .origin = Vec3_Add(pos, Vec3_RandomRange(-2.f, 2.f)),
        .velocity = Vec3_Add(Vec3_Add(Vec3_Scale(forward, 2.f), Vec3_RandomRange(-5.f, 5.f)), Vec3(0.f, 0.f, 6.f)),
        .acceleration.z = 10.f,
        .lifetime = 1000 - Randomf() * 100,
        .size = RandomRangef(2.f, 3.f),
        .color = Vec3(1.f, 1.f, 1.f),
        .lighting = 1.f
      });

      ent->timestamp = cgi.client->unclamped_time + 800;
    }
  } else if (cg_state.weather) {

    Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_smoke,
      .lifetime = 4000 - Randomf() * 100,
      .size = 12.5f,
      .origin = pos,
      .velocity = Vec3_Add(Vec3_Scale(forward, 5.0), Vec3_RandomRange(-2.f, 2.f)),
      .color = Vec3(1.f, 1.f, 1.f),
      .lighting = 1.f
    });

    ent->timestamp = cgi.client->unclamped_time + 3000;
  }
}

/**
 * @brief
 */
void Cg_SmokeTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

  vec3_t origin, dir;
  const int32_t count = Cg_TrailCount(end, 8.f, ent, TRAIL_SECONDARY, &origin, &dir);

  if (!count) {
    return;
  }

  const float step = 1.f / count;

  for (int32_t i = 0; i <= count; i++) {

    Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_smoke,
      .origin = Vec3_Mix(end, origin, (step * i) + RandomRangef(-.5f, .5f)),
      .velocity = Vec3_Scale(dir, RandomRangef(20.f, 30.f)),
      .acceleration = Vec3_Scale(dir, -20.f),
      .lifetime = RandomRangef(1000.f, 1400.f),
      .color = Vec3(1.f, 1.f, 1.f),
      .size = 2.5f,
      .size_velocity = 15.f,
      .rotation = RandomRangef(.0f, M_PI),
      .rotation_velocity = RandomRangef(.2f, 1.f),
      .lighting = 1.f,
    });
  }
}

/**
 * @brief
 */
void Cg_FlameTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

  const float hue = RandomRangef(50.f, 60.f);
  const vec3_t color = ColorHSV(hue, 1.f, 1.f).vec3;

  cg_sprite_t *s = Cg_AddSprite(&(cg_sprite_t) {
    .atlas_image = cg_sprite_flame,
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
 * @brief
 */
static void Cg_BubbleTrail_Think(cg_sprite_t *s, float life, float delta) {

  if (cgi.PointContents(s->origin) & CONTENTS_MASK_LIQUID) {
    return;
  }

  s->velocity = Vec3_Zero();
  s->acceleration = Vec3_Zero();

  s->lifetime = Mini(cgi.client->unclamped_time + 100 - s->time, s->lifetime);
}

/**
 * @brief
 */
void Cg_BubbleTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end, float freq) {

  vec3_t origin = start;
  const int32_t count = Cg_TrailCount(end, freq, ent, TRAIL_BUBBLE, &origin, NULL);

  if (!count) {
    return;
  }

  const float step = 1.f / count;

  for (int32_t i = 0; i <= count; i++) {

    const vec3_t pos = Vec3_Mix(end, origin, step * i);

    const int32_t contents = cgi.PointContents(pos);
    if (!(contents & CONTENTS_MASK_LIQUID)) {
      continue;
    }

    const float v = RandomRangef(.6f, 1.f);

    cg_sprite_t *s = Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_bubble,
      .origin = Vec3_Add(pos, Vec3_RandomRange(-2.f, 2.f)),
      .velocity = Vec3_Add(Vec3_RandomRange(-5.f, 5.f), Vec3(0.f, 0.f, 6.f)),
      .acceleration = Vec3_Add(Vec3_RandomRange(-4.f, 4.f), Vec3(0.f, 0.f, 14.f)),
      .lifetime = 2000 - (Randomf() * 500),
      .size = RandomRangef(.5f, 1.f),
      .rotation = Randomf(),
      .color = Vec3(v, v, v),
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

    s->size_velocity = -s->size / MILLIS_TO_SECONDS(s->lifetime);
  }
}

/**
 * @brief
 */
static void Cg_BlasterTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

  const vec3_t color = Cg_ClientEffectColor(ent->current.client, NULL, color_hue_orange);

  const int32_t liquid = Cg_TrailContents(start, end) & CONTENTS_MASK_LIQUID;
  if (liquid) {
    Cg_BubbleTrail(ent, start, end, 2.f);
  }

  vec3_t org, dir;
  const int32_t count = Cg_TrailCount(end, 8.f, ent, TRAIL_PRIMARY, &org, &dir);

  if (count) {

    for (int32_t i = 0; i <= count; i++) {

      if (liquid && (i & 1)) {
        continue;
      }

      const float scale = 7.f;
      const float power = 3.f;
      const vec3_t pdir = Vec3_Normalize(Vec3_RandomRange(-1.f, 1.f));
      const vec3_t porg = Vec3_Scale(pdir, powf(Randomf(), power) * scale);
      const float pdist = Vec3_Distance(Vec3_Zero(), porg) / scale;

      if (!Cg_AddSprite(&(cg_sprite_t) {
        .atlas_image = cg_sprite_particle,
        .lifetime = 1000,
        .velocity = Vec3_Scale(pdir, pdist * 10.f),
        .origin = Vec3_Add(porg, Vec3_Mix(end, org, 1.0f / i)),
        .size = Maxf(1.85f, powf(1.85f - pdist, power)),
        .size_velocity = Mixf(-3.5f, -.2f, pdist) * RandomRangef(.66f, 1.f),
        .color = color,
        .lighting = .3f,
      })) {
        break;
      }
    }
  }

  cgi.AddSprite(cgi.view, &(r_sprite_t) {
    .media = (r_media_t *) cg_sprite_particle,
    .origin = end,
    .size = 8.f,
    .color = color,
  });

  Cg_AddLight(&(cg_light_t) {
    .origin = end,
    .radius = 200.f,
    .color = color,
    .intensity = 2.5f,
  });
}

/**
 * @brief
 */
static void Cg_GrenadeTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

  if (Cg_TrailContents(start, end) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(ent, start, end, 6.f);
  }

  const float pulse1 = sinf(cgi.client->unclamped_time * .02f)  * .5f + .5f;
  const float pulse2 = sinf(cgi.client->unclamped_time * .04f)  * .5f + .5f;
  // vec3_t dir = Vec3_Normalize(Vec3_Subtract(end, start));

  // ring
  cgi.AddSprite(cgi.view, &(r_sprite_t) {
    .media = (r_media_t *) cg_sprite_ring,
    .origin = ent->origin,
    .size = pulse1 * 10.f + 20.f,
    .color = ColorHSV(120.f, .76f, .20f).vec3,
  });
  
  // streak
  cgi.AddSprite(cgi.view, &(r_sprite_t) {
    .media = (r_media_t *) cg_sprite_aniso_flare_01,
    .origin = ent->origin,
    .size = pulse2 * 10.f + 10.f,
    .color = ColorHSV(120.f, .76f, .20f + (pulse2 * .33f)).vec3,
  });

  // glow
  for (int32_t i = 0; i < 2; i++) {
    cgi.AddSprite(cgi.view, &(r_sprite_t) {
      .media = (r_media_t *) cg_sprite_flash,
      .origin = ent->origin,
      .size = 40.f,
      .color = ColorHSV(120.f, .76f, .20f).vec3,
      .rotation = sinf(cgi.client->unclamped_time * (i == 0 ? .002f : -.001f)),
    });
  }

  Cg_AddLight(&(cg_light_t) {
    .origin = end, // TODO: find a way to nudge this away from the surface a bit
    .radius = 40.f + 20.f * pulse1,
    .color = Vec3(.05f, .5f, .05f),
    .intensity = 1.f
  });

  /*mat4_t m = Mat4_FromTranslation(end);
  m = Mat4_ConcatRotation(m, ent->angles.x, Vec3(1.f, 0.f, 0.f));
  m = Mat4_ConcatRotation(m, ent->angles.y, Vec3(0.f, 1.f, 0.f));
  m = Mat4_ConcatRotation(m, ent->angles.z, Vec3(0.f, 0.f, 1.f));
  m = Mat4_ConcatTranslation(m, Vec3(0.f, 0.f, 3.f));

  vec3_t trail_pos = Mat4_Transform(m, Vec3_Zero());

  vec3_t org, dir;
  const int32_t count = Cg_TrailFraction(trail_pos, 8.f, ent, TRAIL_PRIMARY, &org, &dir);

  if (count) {
    Cg_AddSprite(&(cg_sprite_t) {
      .image = cg_beam_line,
      .type = SPRITE_BEAM,
      .origin = trail_pos,
      .termination = org,
      .size = 4.f,
      .color = ColorHSV(color_hue_red, 1.f, 1.f).vec3,
      .lifetime = 120
    });
  }*/
}

static void Cg_FireFlyTrail_Think(cg_sprite_t *sprite, float life, float delta) {

  sprite->velocity = Vec3_Fmaf(sprite->velocity, delta * 1000.f, Vec3_RandomDir());
}

/**
 * @brief
 */
static void Cg_RocketTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {
  int32_t count;
  vec3_t origin;

  float sine = sinf(cgi.client->unclamped_time * .001f) * M_PI;

  const vec3_t velocity = Vec3_Subtract(ent->current.origin, ent->prev.origin);
  const vec3_t direction = Vec3_Normalize(velocity);
  const float speed = Vec3_Length(velocity) / QUETOO_TICK_SECONDS;

  // exhaust glow
  cgi.AddSprite(cgi.view, &(r_sprite_t) {
    .media = (r_media_t *) cg_sprite_explosion_glow,
    .origin = Vec3_Fmaf(ent->origin, -20.f, direction),
    .size = 50.f,
    .color = ColorHSV(29.f, .57f, .34f).vec3,
  });

  // exhaust flare
  for (int32_t i = 0; i < 2; i++) {
    cgi.AddSprite(cgi.view, &(r_sprite_t) {
      .media = (r_media_t *) cg_sprite_explosion_flash,
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
    float life_start, life_frac;

    Cg_ParticleTrailLifeOffset(start, origin, speed, step, &life_start, &life_frac);

    for (int32_t i = 0; i < count; i++) {
      const float particle_life_frac = life_start + (life_frac * (i + 1));

      // fire
      if (!Cg_AddSprite(&(cg_sprite_t) {
          .animation = cg_sprite_rocket_flame,
          .lifetime = Cg_AnimationLifetime(cg_sprite_rocket_flame, 90) * particle_life_frac,
          .origin = Vec3_Mix(start, origin, step * i),
          .velocity = velocity,
          .rotation = RandomRadian(),
          .size = 10.f,
          .size_velocity = -20.f,
          .color = Vec3(1.f, 1.f, 1.f),
        })) {
        break;
      }
    }
  }

  // smoke trail
  count = Cg_TrailCount(end, 16.f, ent, TRAIL_SECONDARY, &origin, NULL);
  if (count && !liquid) {
    const float step = 1.f / count;
    float life_start, life_frac;

    Cg_ParticleTrailLifeOffset(start, origin, speed, step, &life_start, &life_frac);

    for (int32_t i = 0; i < count; i++) {
      const float particle_life_frac = life_start + (life_frac * (i + 1));

      // interlace smoke 1 and 2 for some subtle variety
      // smoke 1
      if (!Cg_AddSprite(&(cg_sprite_t) {
          .animation = cg_sprite_smoke_04,
          .lifetime = Cg_AnimationLifetime(cg_sprite_smoke_04, 60) * particle_life_frac,
          .origin = Vec3_Add(Vec3_Mix(start, origin, step * i), Vec3_RandomRange(-2.5f, 2.5f)),
          .velocity = Vec3_Scale(velocity, 0.5),
          .rotation = RandomRadian(),
          .size = Randomf() * 5.f + 10.f,
          .size_velocity = Randomf() * 5.f + 10.f,
          .color = Vec3(1.f, 1.f, 1.f),
          .lighting = 1.f
        })) {
        break;
      }

      // smoke 2
      if (!Cg_AddSprite(&(cg_sprite_t) {
          .animation = cg_sprite_smoke_05,
          .lifetime = Cg_AnimationLifetime(cg_sprite_smoke_05, 60) * particle_life_frac,
          .origin = Vec3_Add(Vec3_Mix(start, origin, (step * i) + (step * .5f)), Vec3_RandomRange(-2.5f, 2.5f)),
          .velocity = Vec3_Scale(velocity, 0.5),
          .rotation = RandomRadian(),
          .size = Randomf() * 5.f + 10.f,
          .size_velocity = Randomf() * 5.f + 10.f,
          .color = Vec3(1.f, 1.f, 1.f),
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
    float life_start, life_frac;

    Cg_ParticleTrailLifeOffset(start, origin, speed, step, &life_start, &life_frac);

    for (int32_t i = 0; i < count; i++) {
      const float particle_life_frac = life_start + (life_frac * (i + 1));

      // sparks
      if (!Cg_AddSprite(&(cg_sprite_t) {
          .atlas_image = cg_sprite_particle,
          .lifetime = RandomRangef(900.f, 1300.f) * particle_life_frac,
          .origin = Vec3_Mix(start, origin, step * i),
          .velocity = Vec3_RandomRange(-10.f, 10.f),
          .acceleration = Vec3_RandomRange(-10.f, 10.f),
          .size = Randomf() * 1.6f + 1.6f,
          .Think = Cg_FireFlyTrail_Think,
          .color = Vec3(1.f, .75f, 0.f),
        })) {
        break;
      }
    }
  }

  Cg_AddLight(&(cg_light_t) {
    .origin = end,
    .radius = 240.f,
    .color = Vec3(.8f, .5f, .2f),
    .intensity = 2.f,
    .source = ent,
  });
}

/**
 * @brief
 */
static void Cg_HyperblasterTrail(cl_entity_t *ent, vec3_t start, vec3_t end) {

  if (Cg_TrailContents(start, end) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(ent, start, end, 1.f);
  }

  const vec3_t color = ColorHSV(204.f, .75f, .44f).vec3;

  vec3_t dir = Vec3_Direction(start, end);

  r_atlas_image_t *variation[] = {
    cg_sprite_plasma_var01,
    cg_sprite_plasma_var02,
    cg_sprite_plasma_var03
  };

  // outer rim
  if (ent->timestamp < cgi.client->unclamped_time) {
    for (int32_t i = 0; i < 3; i++) {
      Cg_AddSprite(&(cg_sprite_t) {
        .atlas_image = variation[i],
        .size = RandomRangef(10.f, 15.f),
        .rotation = RandomRadian(),
        .lifetime = 100.f,
        .color = color,
        .lighting = .3f,
        .flags = SPRITE_FOLLOW_ENTITY | SPRITE_ENTITY_UNLINK_ON_DEATH,
        .entity = Cg_GetSpriteEntity(ent)
      });
    }
    ent->timestamp = cgi.client->unclamped_time + 32;
  }

  // center blob
  cgi.AddSprite(cgi.view, &(r_sprite_t) {
    .media = (r_media_t *) cg_sprite_blob_01,
    .origin = ent->origin,
    .size = RandomRangef(10.f, 15.f),
    .rotation = RandomRadian(),
    .color = Vec3(.06f, .25f, .37f),
    .lighting = .3f,
  });

  // center core
  cgi.AddSprite(cgi.view, &(r_sprite_t) {
    .media = (r_media_t *) cg_sprite_particle,
    .origin = ent->origin,
    .size = RandomRangef(4.5f, 7.f),
    .rotation = RandomRadian(),
    .color = Vec3(.06f, .25f, .37f),
    .lighting = .1f,
  });

  cgi.AddBeam(cgi.view, &(r_beam_t) {
    .start = Vec3_Fmaf(end, 70.f, dir),
    .end = start,
    .color = Vec3(.06f, .25f, .37f),
    .image = cg_beam_tail,
    .size = 5.0f,
    .translate = cgi.client->unclamped_time * RandomRangef(.003f, .009f),
    .lighting = 1.f,
  });

  Cg_AddLight(&(cg_light_t) {
    .origin = ent->origin,
    .radius = 150.f,
    .color = Vec3(.4f, .7f, 1.f),
    .intensity = 2.f
  });
}

/**
 * @brief
 */
static void Cg_LightningTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

  vec3_t dir = Vec3_Direction(end, start);
  const float dist = Vec3_Distance(start, end);

  // World-space oscillation axes — reads clearly from any viewing angle
  const vec3_t world_up = Vec3(0.f, 0.f, 1.f);
  const vec3_t world_right = Vec3_Normalize(Vec3_Cross(dir, world_up));

  // Break beam into oscillating segments
  const float segment_length = 4.f;
  const int32_t num_segments = Maxi(1, (int32_t)(dist / segment_length));
  const float translate = cgi.client->unclamped_time * .006f;

  // Smooth continuous time for fluid wave animation
  const float time = cgi.client->unclamped_time * .01f;

  // Precompute joint positions along the oscillating path
  vec3_t joints[num_segments + 1];
  joints[0] = start;
  joints[num_segments] = end;

  for (int32_t i = 1; i < num_segments; i++) {
    const float frac = (float)i / (float)num_segments;
    vec3_t point = Vec3_Mix(start, end, frac);

    // Taper amplitude — sharper falloff near endpoints
    const float envelope = sinf(M_PI * frac) * sinf(M_PI * frac);
    const float along = frac * dist;

    // Dominant up/down wave in world Z
    const float offset_z = sinf(along * .056f - time * 3.f) * 12.f * envelope;

    // Small random sideways jitter in world horizontal
    const float hash = sinf(along * .37f + time * 7.f) * sinf(along * .71f - time * 3.3f);
    const float offset_h = hash * 5.f * envelope;
    point = Vec3_Fmaf(point, offset_h, world_right);
    point.z += offset_z;

    joints[i] = point;
  }

  // Draw segments with slight overlap to hide joints
  for (int32_t i = 0; i < num_segments; i++) {
    const vec3_t seg_start = joints[i];
    const vec3_t seg_end = joints[i + 1];
    const vec3_t seg_dir = Vec3_Normalize(Vec3_Subtract(seg_end, seg_start));

    // Extend each segment slightly to hide joints
    const vec3_t draw_start = i > 0 ? Vec3_Fmaf(seg_start, -2.f, seg_dir) : seg_start;
    const vec3_t draw_end = i < num_segments - 1 ? Vec3_Fmaf(seg_end, 2.f, seg_dir) : seg_end;

    cgi.AddBeam(cgi.view, &(const r_beam_t) {
      .start = draw_start,
      .end = draw_end,
      .color = Vec3(1.f, 1.f, 1.f),
      .image = cg_beam_lightning,
      .size = 5.5f,
      .flags = SPRITE_BEAM_REPEAT,
      .translate = translate,
    });
  }

  // beam endpoint cap
  Cg_AddSprite(&(cg_sprite_t) {
    .atlas_image = cg_sprite_electro_02,
    .origin = Vec3_Fmaf(end, -10.f, dir),
    .lifetime = 30.f,
    .size = 50.f,
    .rotation = RandomRadian(),
    .color = Vec3(1.f, 1.f, 1.f),
  });

  // lights and flying sparks
  const int32_t seed = (int32_t) cgi.client->unclamped_time % 96;
  for (float f = seed; f < Vec3_Distance(start, end); f += 128.f) {
    Cg_AddLight(&(const cg_light_t) {
      .origin = Vec3_Fmaf(start, f + seed, dir),
      .radius = 192.f + RandomRangef(-32.f, 32.f),
      .color = Vec3(.8f, .4f, .8f),
      .intensity = 3.f,
    });

    Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_particle3,
      .origin = Vec3_Fmaf(start, f + seed, dir),
      .velocity = Vec3_Scale(Vec3_Add(dir, Vec3_RandomRange(-.2f, .2f)), RandomRangef(50, 200)),
      .acceleration.z = -SPRITE_GRAVITY * 3.0,
      .lifetime = 200 + Randomf() * 800,
      .bounce = 0.2f,
      .size = 1.f + RandomRangef(1.f, 2.f),
      .color = Vec3(1.f, .5f, 1.f),
    });
  }

  if (ent->current.animation1 != LIGHTNING_SOLID_HIT) {
    return;
  }

  //hit face decal
  Cg_AddDecal(&(r_decal_t) {
      .image = cg_decal_bullet[Randomi() % lengthof(cg_decal_bullet)],
          .origin = end,
          .radius = RandomRangef(2.f, 4.f),
          .color = Color3f(0.02f, 0.01f, 0.02f),
          .lifetime = 7500 + Randomf() * 7500,
          .rotation = RandomRadian()
  });

  if (ent->timestamp < cgi.client->unclamped_time) {

    vec3_t dir;
    Vec3_Vectors(ent->angles, &dir, NULL, NULL);

    if ((cgi.PointContents(ent->termination) & CONTENTS_MASK_LIQUID) == 0) {

      // hit billboards
      for (int32_t i = 0; i < 2; i++) {
        Cg_AddSprite(&(cg_sprite_t) {
          .atlas_image = cg_sprite_electro_02,
          .origin = end,
          .lifetime = 60 * (i + 1),
          .size = RandomRangef(100.f, 200.f),
          .size_velocity = 400.f,
          .rotation = RandomRadian(),
          .dir = Vec3_RandomRange(-1.f, 1.f),
          .color = Vec3(1.f, 1.f, 1.f),
        });
      }

      // hit decal
      Cg_AddSprite(&(cg_sprite_t) {
        .atlas_image = cg_sprite_electro_02,
        .origin = Vec3_Add(end, dir),
        .lifetime = 120,
        .size = RandomRangef(100.f, 200.f),
        .size_velocity = 400.f,
        .rotation = RandomRadian(),
        .dir = dir,
        .color = Vec3(1.f, 1.f, 1.f),
      });

      // hit sparks
      for (int32_t i = 0; i < 2; i++) {
        Cg_AddSprite(&(cg_sprite_t) {
          .atlas_image = cg_sprite_particle3,
          .origin = end,
          .velocity = Vec3_Scale(Vec3_Add(dir, Vec3_RandomRange(-.2f, .2f)), RandomRangef(50, 200)),
          .acceleration.z = -SPRITE_GRAVITY * 3.0,
          .lifetime = 200 + Randomf() * 800,
          .bounce = 0.2f,
          .size = 2.0f + RandomRangef(1.0f, 2.0f),
          .color = Vec3(1.f, .5f, 1.f),
        });
      }
    }

    ent->timestamp = cgi.client->unclamped_time + 25; // 40hz
  }
}

/**
 * @brief
 */
static void Cg_HookTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

  vec3_t forward;
  Vec3_Vectors(ent->angles, &forward, NULL, NULL);

  const vec3_t color = Cg_ClientEffectColor(ent->current.client, NULL, color_hue_green);

  cgi.AddBeam(cgi.view, &(const r_beam_t) {
    .start = start,
    .end = Vec3_Fmaf(end, -3.f, forward),
    .color = color,
    .image = cg_beam_hook,
    .size = 1.f,
    .flags = SPRITE_BEAM_REPEAT,
  });
}

#define BFG_BALLS_SPEED  400.f

/**
 * @brief
 */
static void Cg_BfgTrail_Think(cg_sprite_t *sprite, float life, float delta) {

  if (!(sprite->flags & SPRITE_FOLLOW_ENTITY)) {
    sprite->Think = NULL;
    sprite->acceleration = Vec3(0.f, 0.f, -3.f * SPRITE_GRAVITY);
    const float lifetime = RandomRangef(2000, 3500);
    sprite->lifetime = lifetime;
    sprite->size_velocity = -sprite->size / MILLIS_TO_SECONDS(lifetime);
    sprite->time = sprite->timestamp = cgi.client->unclamped_time;
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
 * @brief
 */
static void Cg_BfgTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {
  const float mod = fmodf((float)cgi.client->unclamped_time, 100.f) / 100.f;

  if (Cg_TrailContents(start, end) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(ent, ent->prev.origin, ent->origin, .5f);
  }

  // projectile core
  cgi.AddSprite(cgi.view, &(r_sprite_t) {
    .origin = ent->origin,
    .size = 100.f,
    .media = (r_media_t*) cg_sprite_hyperball_01,
    .rotation = mod * 200.f * M_PI,
    .color = Vec3(1.f, 1.f, 1.f),
    .life = fmod(cgi.client->unclamped_time * 0.001f, 1.0f),
  });

  if (ent->timestamp < cgi.client->unclamped_time) {
    ent->timestamp = cgi.client->unclamped_time + 4;
  
    Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_particle,
      .origin = Vec3_Zero(),
      .size = 6.f,
      .color = Vec3(.5f, 1.f, 0.5f),
      .lifetime = 1000,
      .flags = SPRITE_FOLLOW_ENTITY | SPRITE_DATA_NOFREE | SPRITE_ENTITY_UNLINK_ON_DEATH,
      .bounce = 1.0f,
      .Think = Cg_BfgTrail_Think,
      .entity = Cg_GetSpriteEntity(ent),
      .data = ent,
      .velocity = Vec3_Scale(Vec3_RandomDir(), BFG_BALLS_SPEED)
    });
  }

  Cg_AddLight(&(cg_light_t) {
    .origin = ent->origin,
    .radius = 240.0,
    .color = Vec3(.4f, 1.f, .4f),
    .intensity = 2.f,
  });
}

/**
 * @brief
 */
static void Cg_TeleporterTrail(cl_entity_t *ent) {
  const float value = .7f + (sinf(cgi.client->unclamped_time * .02f) / M_PI) * .3f;
  
  Cg_AddSprite(&(cg_sprite_t) {
    .atlas_image = cg_sprite_splash_02_03,
    .origin = Vec3_Fmaf(ent->origin, 8.f, Vec3_Up()),
    .size = 64.f,
    .color = Vec3(value, value, value),
    .axis = SPRITE_AXIS_X | SPRITE_AXIS_Y,
  });

  if (ent->timestamp <= cgi.client->unclamped_time) {
    ent->timestamp = cgi.client->unclamped_time + 100;
    
    Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_ring,
      .dir = Vec3_Up(),
      .origin = Vec3_Fmaf(ent->origin, 16.f, Vec3_Down()),
      .velocity.z = RandomRangef(80.f, 120.f),
      .lifetime = 450,
      .size = 64.f,
      .size_velocity = 48.f,
      .color = Vec3(1.f, 1.f, 1.f),
    });
  }
}

/**
 * @brief
 */
static inline float Cg_Oscillate(const float freq, const float amplitude, const float base, const float phase) {
  const float seconds = MILLIS_TO_SECONDS(cgi.client->unclamped_time);
  return base + sinf((phase + seconds * 2 * freq * 2)) * (amplitude * 0.5);
}

/**
 * @brief
 */
static void Cg_PlayerSpawnTrail(const cl_entity_t *ent) {

  const vec3_t color = Cg_ClientEffectColor(ent->current.client, NULL, color_hue_yellow);

  cgi.AddSprite(cgi.view, &(r_sprite_t) {
    .media = (r_media_t *) cg_sprite_ring,
    .origin = Vec3_Fmaf(ent->origin, 16.f, Vec3_Down()),
    .size = 48.f + Cg_Oscillate(1, 12.f, 1.f, 0.f),
    .color = color,
    .dir = Vec3_Up()
  });
}

/**
 * @brief
 */
static void Cg_GibTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

  if (cgi.PointContents(end) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(ent, start, end, 4.f);
    return;
  }

  vec3_t origin, dir;
  const int32_t count = Cg_TrailCount(end, 8.f, ent, TRAIL_PRIMARY, &origin, &dir);

  if (!count) {
    return;
  }

  float step = 1.f / count;

  for (int32_t i = 0; i <= count; i++) {

    if (!Cg_AddSprite(&(cg_sprite_t) {
        .animation = cg_sprite_blood_01,
        .lifetime = Cg_AnimationLifetime(cg_sprite_blood_01, 30) + Randomf() * 500,
        .size = RandomRangef(40.f, 64.f),
        .rotation = RandomRadian(),
        .origin = Vec3_Mix(end, origin, step * i),
        .velocity = Vec3_Scale(dir, 20.0),
        .acceleration.z = -SPRITE_GRAVITY / 2.0,
        .color = Vec3(1.f, 1.f, .1f),
        .lighting = 1.f
      })) {
      break;
    }

    Cg_AddDecal(&(r_decal_t) {
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
 * @brief
 */
static void Cg_FireballTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

  if (cgi.PointContents(end) & CONTENTS_MASK_LIQUID) {
    return;
  }

  cg_light_t l = {
    .origin = end,
    .radius = 185.f,
    .color = Vec3(.9f, .3f, .1f),
    .intensity = 3.f,
    .source = ent
  };

  if (ent->current.effects & EF_DESPAWN) {
    const float decay = Clampf01((cgi.client->unclamped_time - ent->timestamp) / 1000.f);
    l.radius *= (1.f - decay);
  } else {
    Cg_SmokeTrail(ent, start, end);
    Cg_FlameTrail(ent, start, end);
    ent->timestamp = cgi.client->unclamped_time;
  }

  Cg_AddLight(&l);
}

/**
 * @brief
 */
static void Cg_CtfEffectTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

  const cg_team_info_t *team = cg_state.teams;
  for (size_t i = 0; i < lengthof(cg_state.teams); i++, team++) {
    if (ent->current.effects & (EF_CTF_RED << i)) {
      break;
    }
  }
  assert(team);

  const vec3_t color = ColorHSV(team->hue, 1.f, 1.f).vec3;
  const vec3_t velocity = Vec3_Scale(Vec3_Subtract(end, start), 100.f / cgi.client->frame_msec);

  const int32_t count = Cg_TrailCount(end, 1.f, ent, TRAIL_PRIMARY, NULL, NULL);
  for (int32_t i = 0; i < count; i++) {

    if (!Cg_AddSprite(&(cg_sprite_t) {
        .atlas_image = cg_sprite_particle,
        .lifetime = RandomRangeu(800, 2000),
        .size = RandomRangef(1.f, 2.f),
        .size_velocity = RandomRangef(-2.f, 0.f),
        .origin = Vec3_Add(start, Vec3_RandomRanges(-18.f, 18.f, -18.f, 18.f, 8.f, 32.f)),
        .velocity = Vec3_Add(velocity, Vec3_RandomRanges(-24.f, 24.f, -24.f, 24.f, 0.f, 24.f)),
        .acceleration = Vec3_RandomizeDir(Vec3_Scale(Vec3_Up(), 30.f), .33f),
        .friction = 50.f,
        .color = color,
        .lighting = .5f
      })) {
      break;
    };
  }
}

/**
 * @brief Determines the initial position and directional vectors of a projectile. This is a copy of G_InitProjectile.
 * If that function changes, make sure this one is modified too.
 */
static void Cg_InitProjectile(const cl_entity_t *ent, vec3_t *forward, vec3_t *right, vec3_t *up, vec3_t *org, float hand) {

  // resolve the projectile destination
  const vec3_t start = cgi.view->origin;
  const vec3_t end = Vec3_Fmaf(start, MAX_WORLD_DIST, cgi.view->forward);
  const cm_trace_t tr = cgi.Trace(start, end, Box3_Zero(), ent, CONTENTS_MASK_CLIP_PROJECTILE);

  // resolve the projectile origin
  *org = Vec3_Fmaf(start, 12.f, cgi.view->forward);

  switch (cg_hand->integer) {
    case HAND_RIGHT:
      *org = Vec3_Fmaf(*org, 6.f * hand, cgi.view->right);
      break;
    case HAND_LEFT:
      *org = Vec3_Fmaf(*org, -6.f * hand, cgi.view->right);
      break;
    default:
      break;
  }

  if ((cgi.client->frame.ps.pm_state.flags & PMF_DUCKED)) {
    *org = Vec3_Fmaf(*org, -6.f, cgi.view->up);
  } else {
    *org = Vec3_Fmaf(*org, -12.f, cgi.view->up);
  }

  // if the projected origin is invalid, use the entity's origin
  if (cgi.Trace(*org, *org, Box3_Zero(), ent, CONTENTS_MASK_CLIP_PROJECTILE).start_solid) {
    *org = ent->origin;
  }

  if (forward) {
    // return the projectile's directional vectors
    *forward = Vec3_Subtract(tr.end, *org);
    *forward = Vec3_Normalize(*forward);

    const vec3_t euler = Vec3_Euler(*forward);
    Vec3_Vectors(euler, NULL, right, up);
  }
}

/**
 * @brief Apply unique trails to entities between their previous packet origin
 * and their current interpolated origin. Beam trails are a special case: the
 * old origin field is overridden to specify the endpoint of the beam.
 */
void Cg_EntityTrail(cl_entity_t *ent) {
  const entity_state_t *s = &ent->current;

  vec3_t start, end;
  start = ent->previous_origin;

  // beams have two origins, most entities have just one
  if (s->effects & EF_BEAM) {

    end = ent->termination;

    // client is overridden to specify owner of the beam
    if (ent->current.client == cgi.client->frame.ps.client && !cgi.client->third_person) {

      // we own this beam (lightning, grapple, etc..)
      // project start & end points based on our current view origin
      float dist = Vec3_Distance(start, end);

      if (s->trail == TRAIL_LIGHTNING) {
        vec3_t forward;

        Cg_InitProjectile(ent, &forward, NULL, NULL, &start, 1.0);

        end = Vec3_Fmaf(start, dist, forward);
      } else {

        start = Vec3_Fmaf(cgi.view->origin, 8.f, cgi.view->forward);

        const float hand_scale = (ent->current.trail == TRAIL_HOOK ? -1.0 : 1.0);

        switch (cg_hand->integer) {
          case HAND_LEFT:
            start = Vec3_Fmaf(start, -5.5 * hand_scale, cgi.view->right);
            break;
          case HAND_RIGHT:
            start = Vec3_Fmaf(start, 5.5 * hand_scale, cgi.view->right);
            break;
          default:
            break;
        }

        start = Vec3_Fmaf(start, -8.f, cgi.view->up);
      }
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
    case TRAIL_ROCKET:
      Cg_RocketTrail(ent, start, end);
      break;
    case TRAIL_HYPERBLASTER:
      Cg_HyperblasterTrail(ent, start, end);
      break;
    case TRAIL_LIGHTNING:
      Cg_LightningTrail(ent, start, end);
      break;
    case TRAIL_HOOK:
      Cg_HookTrail(ent, start, end);
      break;
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
    default:
      break;
  }

  if (s->effects & EF_CTF_MASK) {
    Cg_CtfEffectTrail(ent, start, end);
  }
}
