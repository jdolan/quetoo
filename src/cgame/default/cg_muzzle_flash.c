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
  * @brief Returns the muzzle origin for the given entity, using the cached
  * weapon tag origin and angles. The offset is in weapon-local space
  * (x = forward, y = right, z = up).
  */
static vec3_t Cg_MuzzleOrigin(const cl_entity_t *ent, const vec3_t offset) {

  const cg_client_info_t *ci = &cg_state.clients[ent->current.client];

  vec3_t forward, right, up;
  Vec3_Vectors(ci->weapon_angles, &forward, &right, &up);

  vec3_t org = ci->weapon_origin;
  org = Vec3_Fmaf(org, offset.x, forward);
  org = Vec3_Fmaf(org, offset.y, right);
  org = Vec3_Fmaf(org, offset.z, up);

  const cm_trace_t tr = cgi.Trace(ent->origin, org, Box3_Zero(), NULL, CONTENTS_MASK_CLIP_PROJECTILE);

  if (tr.fraction < 1.0) {
    const vec3_t dir = Vec3_Subtract(ent->origin, tr.end);
    org = Vec3_Fmaf(ent->origin, .75f, dir);
  }

  return org;
}

/**
 * @brief
 */
static void Cg_BlasterFlash(const cl_entity_t *ent) {

  const vec3_t color = Cg_ClientEffectColor(ent->current.client, NULL, color_hue_orange);

  vec3_t org = Cg_MuzzleOrigin(ent, Vec3(18.f, 0.f, 1.f));

  Cg_AddLight(&(cg_light_t) {
    .origin = org,
    .radius = 120.0,
    .color = color,
    .intensity = 3.f,
    .decay = 300
  });

  vec3_t org2, forward, right;
  Vec3_Vectors(ent->angles, &forward, &right, NULL);
  if (cgi.PointContents(ent->origin) & CONTENTS_MASK_LIQUID) {
    org2 = Vec3_Fmaf(ent->origin, 40.f, forward);
    Cg_BubbleTrail(NULL, org, org2, 8.f);
    return;
  }

  const int32_t np = 5;
  const float flashlen = 2.f;

  for (int32_t i = 0; i < np; i++) {

    Cg_AddSprite(&(cg_sprite_t) {
      .animation = cg_sprite_blaster_flame,
      .lifetime = Cg_AnimationLifetime(cg_sprite_blaster_flame, 70) + (i / flashlen * 20.f),
      .origin = Vec3_Fmaf(org, 3.f * (i / flashlen), forward),
      .rotation = RandomRadian(),
      .size = 1.f + 2.f * (np - i / flashlen),
      .color = color,
    });
  }

  Cg_AddSprite(&(cg_sprite_t) {
    .image = (r_image_t *) cg_sprite_blaster_flash,
    .lifetime = 200,
    .origin = Vec3_Fmaf(org, 3.f, forward),
    .size = 30.f,
    .size_velocity = 30.f,
    .color = color,
  });
}

/**
 * @brief Shotgun muzzle flash.
 */
static void Cg_ShotgunFlash(const cl_entity_t *ent) {

  vec3_t org = Cg_MuzzleOrigin(ent, Vec3(18.f, 0.f, 2.f));

  vec3_t forward, right;
  Vec3_Vectors(ent->angles, &forward, &right, NULL);

  Cg_AddLight(&(cg_light_t) {
    .origin = org,
    .radius = 120.f,
    .color = Vec3(.9f, .7f, .4f),
    .intensity = 3.f,
    .decay = 250,
  });

  if (cgi.PointContents(ent->origin) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(NULL, org, Vec3_Fmaf(ent->origin, 40.f, forward), 4.f);
    return;
  }

  // flash burst
  Cg_AddSprite(&(cg_sprite_t) {
    .animation = cg_sprite_impact_spark_01,
    .origin = Vec3_Fmaf(org, 4.f, forward),
    .rotation = RandomRadian(),
    .size = 30.f,
    .lifetime = 180.f,
    .color = Vec3(1.f, .9f, .7f),
  });

  // flame lick
  Cg_AddSprite(&(cg_sprite_t) {
    .atlas_image = cg_sprite_flame,
    .origin = Vec3_Fmaf(org, 6.f, forward),
    .velocity = Vec3_Scale(forward, 60.f),
    .lifetime = 200.f,
    .size = 10.f,
    .size_velocity = -30.f,
    .rotation = RandomRadian(),
    .color = ColorHSV(RandomRangef(25.f, 45.f), 1.f, 1.f).vec3,
  });

  // spark dots
  for (int32_t i = 0; i < 4; i++) {
    Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_impact_spark_01_dot,
      .origin = Vec3_Fmaf(org, 4.f, forward),
      .velocity = Vec3_Scale(Vec3_Mix(Vec3_RandomDir(), forward, .5f), RandomRangef(60.f, 120.f)),
      .size = RandomRangef(2.f, 4.f),
      .size_velocity = -8.f,
      .lifetime = RandomRangef(150.f, 300.f),
      .color = Vec3(1.f, .9f, .7f),
    });
  }

  // smoke
  Cg_AddSprite(&(cg_sprite_t) {
    .atlas_image = cg_sprite_smoke,
    .origin = org,
    .velocity = Vec3_Scale(forward, 20.f),
    .lifetime = 800,
    .size = 3.f,
    .size_velocity = 20.f,
    .rotation = RandomRadian(),
    .rotation_velocity = RandomRangef(.2f, .6f),
    .color = Vec3(.6f, .6f, .6f),
    .lighting = 1.f,
  });
}

/**
 * @brief Super shotgun muzzle flash.
 */
static void Cg_SuperShotgunFlash(const cl_entity_t *ent) {

  vec3_t org = Cg_MuzzleOrigin(ent, Vec3(18.f, 0.f, 2.f));

  vec3_t forward, right;
  Vec3_Vectors(ent->angles, &forward, &right, NULL);

  Cg_AddLight(&(cg_light_t) {
    .origin = org,
    .radius = 160.f,
    .color = Vec3(.9f, .7f, .4f),
    .intensity = 4.f,
    .decay = 300,
  });

  if (cgi.PointContents(ent->origin) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(NULL, org, Vec3_Fmaf(ent->origin, 40.f, forward), 8.f);
    return;
  }

  // flash burst (double barrel = bigger)
  Cg_AddSprite(&(cg_sprite_t) {
    .animation = cg_sprite_impact_spark_01,
    .origin = Vec3_Fmaf(org, 4.f, forward),
    .rotation = RandomRadian(),
    .size = 40.f,
    .lifetime = 220.f,
    .color = Vec3(1.f, .9f, .7f),
  });

  // flame burst
  for (int32_t i = 0; i < 2; i++) {
    Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_flame,
      .origin = Vec3_Fmaf(org, 4.f + i * 4.f, forward),
      .velocity = Vec3_Scale(forward, RandomRangef(60.f, 100.f)),
      .lifetime = RandomRangef(200.f, 350.f),
      .size = RandomRangef(10.f, 16.f),
      .size_velocity = -30.f,
      .rotation = RandomRadian(),
      .color = ColorHSV(RandomRangef(20.f, 50.f), 1.f, 1.f).vec3,
    });
  }

  // spark dots (more than single shotgun)
  for (int32_t i = 0; i < 8; i++) {
    Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_impact_spark_01_dot,
      .origin = Vec3_Fmaf(org, 4.f, forward),
      .velocity = Vec3_Scale(Vec3_Mix(Vec3_RandomDir(), forward, .4f), RandomRangef(80.f, 160.f)),
      .size = RandomRangef(2.f, 5.f),
      .size_velocity = -8.f,
      .lifetime = RandomRangef(200.f, 400.f),
      .color = Vec3(1.f, .9f, .7f),
    });
  }

  // smoke (heavier for double barrel)
  for (int32_t i = 0; i < 2; i++) {
    Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_smoke,
      .origin = Vec3_Fmaf(org, i * 4.f, forward),
      .velocity = Vec3_Add(Vec3_Scale(forward, 25.f), Vec3_RandomRange(-5.f, 5.f)),
      .lifetime = 1000,
      .size = 4.f,
      .size_velocity = 24.f,
      .rotation = RandomRadian(),
      .rotation_velocity = RandomRangef(.2f, .8f),
      .color = Vec3(.5f, .5f, .5f),
      .lighting = 1.f,
    });
  }
}

/**
 * @brief Machinegun muzzle flash.
 */
static void Cg_MachinegunFlash(const cl_entity_t *ent) {

  vec3_t org = Cg_MuzzleOrigin(ent, Vec3(22.f, 0.f, 0.f));

  vec3_t forward, right;
  Vec3_Vectors(ent->angles, &forward, &right, NULL);

  Cg_AddLight(&(cg_light_t) {
    .origin = org,
    .radius = 80.f,
    .color = Vec3(.9f, .8f, .5f),
    .intensity = 2.5f,
    .decay = 150,
  });

  if (cgi.PointContents(ent->origin) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(NULL, org, Vec3_Fmaf(ent->origin, 40.f, forward), 2.f);
    return;
  }

  // quick flash burst
  Cg_AddSprite(&(cg_sprite_t) {
    .animation = cg_sprite_impact_spark_01,
    .origin = Vec3_Fmaf(org, 4.f, forward),
    .rotation = RandomRadian(),
    .size = 20.f,
    .lifetime = 120.f,
    .color = Vec3(1.f, .9f, .7f),
  });

  // spark dots
  for (int32_t i = 0; i < 3; i++) {
    Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_impact_spark_01_dot,
      .origin = Vec3_Fmaf(org, 4.f, forward),
      .velocity = Vec3_Scale(Vec3_Mix(Vec3_RandomDir(), forward, .5f), RandomRangef(50.f, 100.f)),
      .size = RandomRangef(2.f, 4.f),
      .size_velocity = -10.f,
      .lifetime = RandomRangef(100.f, 250.f),
      .color = Vec3(1.f, .9f, .7f),
    });
  }

  // occasional smoke wisp
  if (Randomb()) {
    Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_smoke,
      .origin = org,
      .velocity = Vec3_Scale(forward, 15.f),
      .lifetime = 600,
      .size = 2.f,
      .size_velocity = 12.f,
      .rotation = RandomRadian(),
      .rotation_velocity = RandomRangef(.3f, .8f),
      .color = Vec3(.6f, .6f, .6f),
      .lighting = 1.f,
    });
  }
}

/**
 * @brief Grenade launcher muzzle flash.
 */
static void Cg_GrenadeFlash(const cl_entity_t *ent) {

  vec3_t org = Cg_MuzzleOrigin(ent, Vec3(20.f, 0.f, 6.f));

  vec3_t forward, right;
  Vec3_Vectors(ent->angles, &forward, &right, NULL);

  Cg_AddLight(&(cg_light_t) {
    .origin = org,
    .radius = 120.f,
    .color = Vec3(.9f, .6f, .3f),
    .intensity = 3.5f,
    .decay = 350,
  });

  if (cgi.PointContents(ent->origin) & CONTENTS_MASK_LIQUID) {
    Cg_BubbleTrail(NULL, org, Vec3_Fmaf(ent->origin, 40.f, forward), 4.f);
    return;
  }

  // muzzle glow
  Cg_AddSprite(&(cg_sprite_t) {
    .atlas_image = cg_sprite_explosion_glow,
    .origin = Vec3_Fmaf(org, 6.f, forward),
    .lifetime = 250,
    .size = 40.f,
    .color = Vec3(.9f, .6f, .3f),
  });

  // flame
  Cg_AddSprite(&(cg_sprite_t) {
    .atlas_image = cg_sprite_flame,
    .origin = Vec3_Fmaf(org, 4.f, forward),
    .velocity = Vec3_Scale(forward, 40.f),
    .lifetime = 250.f,
    .size = 12.f,
    .size_velocity = -30.f,
    .rotation = RandomRadian(),
    .color = ColorHSV(RandomRangef(20.f, 45.f), 1.f, 1.f).vec3,
  });

  // heavy smoke (grenade launchers are smoky)
  for (int32_t i = 0; i < 2; i++) {
    Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_smoke,
      .origin = Vec3_Fmaf(org, i * 3.f, forward),
      .velocity = Vec3_Add(Vec3_Scale(forward, RandomRangef(10.f, 25.f)),
                           Vec3(0.f, 0.f, RandomRangef(5.f, 15.f))),
      .lifetime = 1200,
      .size = 4.f,
      .size_velocity = 28.f,
      .rotation = RandomRadian(),
      .rotation_velocity = RandomRangef(.2f, .6f),
      .color = Vec3(.45f, .45f, .45f),
      .lighting = 1.f,
    });
  }
}

/**
 * @brief
 */
static void Cg_RocketFlash(const cl_entity_t *ent) {

  vec3_t org = Cg_MuzzleOrigin(ent, Vec3(24.f, 0.f, 4.f));

  vec3_t forward, right;
  Vec3_Vectors(ent->angles, &forward, &right, NULL);

  Cg_AddLight(&(cg_light_t) {
    .origin = org,
    .radius = 200.f,
    .color = Vec3(.9f, .6f, .3f),
    .intensity = 4.f,
    .decay = 400
  });

  if (cgi.PointContents(ent->origin) & CONTENTS_MASK_LIQUID) {
    const vec3_t org2 = Vec3_Fmaf(ent->origin, 40.f, forward);
    Cg_BubbleTrail(NULL, org, org2, 2.f);
    return;
  }

  // muzzle flash glow
  Cg_AddSprite(&(cg_sprite_t) {
    .atlas_image = cg_sprite_explosion_glow,
    .origin = Vec3_Fmaf(org, 8.f, forward),
    .lifetime = 300,
    .size = 60.f,
    .color = Vec3(.9f, .6f, .3f),
  });

  // muzzle flame burst
  for (int32_t i = 0; i < 3; i++) {
    Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_flame,
      .origin = Vec3_Fmaf(org, 4.f + i * 4.f, forward),
      .velocity = Vec3_Scale(forward, RandomRangef(40.f, 80.f)),
      .lifetime = RandomRangef(200.f, 400.f),
      .size = RandomRangef(10.f, 18.f),
      .size_velocity = -20.f,
      .rotation = RandomRadian(),
      .color = ColorHSV(RandomRangef(20.f, 50.f), 1.f, 1.f).vec3,
    });
  }

  // smoke puff
  Cg_AddSprite(&(cg_sprite_t) {
    .atlas_image = cg_sprite_smoke,
    .origin = org,
    .velocity = Vec3_Scale(forward, RandomRangef(15.f, 30.f)),
    .lifetime = 800,
    .size = 4.f,
    .size_velocity = 36.f,
    .rotation = RandomRadian(),
    .rotation_velocity = RandomRangef(.2f, .8f),
    .color = Vec3(.5f, .5f, .5f),
    .lighting = 1.f,
  });

  // embers
  for (int32_t i = 0; i < 24; i++) {
    Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_particle2,
      .origin = Vec3_Fmaf(org, 6.f, forward),
      .velocity = Vec3_Add(Vec3_Scale(forward, RandomRangef(100.f, 250.f)),
                           Vec3_RandomRange(-60.f, 60.f)),
      .acceleration.z = -SPRITE_GRAVITY,
      .lifetime = RandomRangef(400.f, 800.f),
      .size = RandomRangef(1.f, 2.f),
      .color = ColorHSV(RandomRangef(15.f, 45.f), 1.f, 1.f).vec3,
    });
  }
}

/**
 * @brief Hyperblaster muzzle flash.
 */
static void Cg_HyperblasterFlash(const cl_entity_t *ent) {

  vec3_t org = Cg_MuzzleOrigin(ent, Vec3(26.f, 0.f, 3.f));

  const vec3_t color = Vec3(.4f, .7f, .9f);

  Cg_AddLight(&(cg_light_t) {
    .origin = org,
    .radius = 120.f,
    .color = color,
    .intensity = 2.f,
    .decay = 300,
  });
}

/**
 * @brief BFG muzzle flash.
 */
static void Cg_BfgFlash(const cl_entity_t *ent) {

  vec3_t org = Cg_MuzzleOrigin(ent, Vec3(24.f, 0.f, 0.f));

  const vec3_t color = Color3b(75, 91, 39).vec3;

  Cg_AddLight(&(cg_light_t) {
    .origin = org,
    .radius = 200.f,
    .color = color,
    .intensity = 4.f,
    .decay = 500,
  });

  if (cgi.PointContents(ent->origin) & CONTENTS_MASK_LIQUID) {
    vec3_t forward;
    Vec3_Vectors(ent->angles, &forward, NULL, NULL);
    Cg_BubbleTrail(NULL, org, Vec3_Fmaf(ent->origin, 40.f, forward), 4.f);
    return;
  }

  // big energy glow
  Cg_AddSprite(&(cg_sprite_t) {
    .atlas_image = cg_sprite_explosion_glow,
    .origin = org,
    .lifetime = 400,
    .size = 50.f,
    .color = color,
  });

  // BFG plasma burst
  Cg_AddSprite(&(cg_sprite_t) {
    .animation = cg_sprite_bfg_explosion_2,
    .origin = org,
    .lifetime = Cg_AnimationLifetime(cg_sprite_bfg_explosion_2, 60),
    .size = 20.f,
    .rotation = RandomRadian(),
    .color = color,
  });

  // electric tendrils
  for (int32_t i = 0; i < 6; i++) {
    Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = cg_sprite_electro_02,
      .origin = org,
      .velocity = Vec3_Scale(Vec3_RandomDir(), RandomRangef(40.f, 100.f)),
      .lifetime = RandomRangef(200.f, 400.f),
      .size = RandomRangef(6.f, 12.f),
      .size_velocity = -15.f,
      .rotation = RandomRadian(),
      .color = color,
    });
  }

  // plasma sparks
  r_atlas_image_t *plasma_sprites[] = { cg_sprite_plasma_var01, cg_sprite_plasma_var02, cg_sprite_plasma_var03 };
  for (int32_t i = 0; i < 8; i++) {
    Cg_AddSprite(&(cg_sprite_t) {
      .atlas_image = plasma_sprites[i % 3],
      .origin = org,
      .velocity = Vec3_Scale(Vec3_RandomDir(), RandomRangef(60.f, 140.f)),
      .lifetime = RandomRangef(300.f, 600.f),
      .size = RandomRangef(2.f, 4.f),
      .size_velocity = -4.f,
      .color = color,
    });
  }
}

/**
 * @brief FIXME: This should be a tentity instead; would make more sense.
 */
static void Cg_LogoutFlash(const cl_entity_t *ent) {
  Cg_GibEffect(ent->origin, 12);
}

/**
 * @brief
 */
void Cg_ParseMuzzleFlash(void) {

  const int16_t entity = cgi.ReadShort();
  const uint8_t flash = cgi.ReadByte();

  if (entity < 0 || entity >= MAX_ENTITIES) {
    Cg_Warn("Bad entity %u\n", entity);
    return;
  }

  const cl_entity_t *ent = &cgi.client->entities[entity];
  const s_sample_t *sample;
  int32_t pitch = 0;

  switch (flash) {
    case MZ_BLASTER:
      sample = cg_sample_blaster_fire;
      Cg_BlasterFlash(ent);
      pitch = 5;
      break;
    case MZ_SHOTGUN:
      sample = cg_sample_shotgun_fire;
      Cg_ShotgunFlash(ent);
      pitch = 3;
      break;
    case MZ_SUPER_SHOTGUN:
      sample = cg_sample_supershotgun_fire;
      Cg_SuperShotgunFlash(ent);
      pitch = 3;
      break;
    case MZ_MACHINEGUN:
      sample = cg_sample_machinegun_fire[RandomRangeu(0, 4)];
      Cg_MachinegunFlash(ent);
      pitch = 5;
      break;
    case MZ_ROCKET_LAUNCHER:
      sample = cg_sample_rocketlauncher_fire;
      Cg_RocketFlash(ent);
      pitch = 3;
      break;
    case MZ_GRENADE_LAUNCHER:
      sample = cg_sample_grenadelauncher_fire;
      Cg_GrenadeFlash(ent);
      pitch = 3;
      break;
    case MZ_HYPERBLASTER:
      sample = cg_sample_hyperblaster_fire;
      Cg_HyperblasterFlash(ent);
      pitch = 5;
      break;
    case MZ_LIGHTNING:
      sample = cg_sample_lightning_fire;
      pitch = 3;
      break;
    case MZ_RAILGUN:
      sample = cg_sample_railgun_fire;
      pitch = 2;
      break;
    case MZ_BFG10K:
      sample = cg_sample_bfg_fire;
      Cg_BfgFlash(ent);
      pitch = 2;
      break;
    case MZ_LOGOUT:
      sample = cg_sample_teleport;
      Cg_LogoutFlash(ent);
      pitch = 4;
      break;
    default:
      sample = NULL;
      break;
  }

  Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
    .origin = ent->current.origin,
    .sample = sample,
    .entity = ent,
    .atten = SOUND_ATTEN_LINEAR,
    .pitch = RandomRangei(-pitch, pitch + 1)
  });
}
