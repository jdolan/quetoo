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

#include "g_local.h"

#define LIGHT_START_ON 1

/**
 * @brief For singular lights, simply toggle them. For teamed lights,
 * advance through the team, toggling two at a time.
 */
static void G_target_light_Cycle(g_entity_t *ent) {

  g_entity_t *master = ent->team_master;
  if (master) {
    G_Debug("Cycling %s\n", etos(master->enemy));

    master->enemy->s.effects ^= EF_LIGHT;
    master->enemy = master->enemy->team_next;

    if (master->enemy == NULL) {
      master->enemy = master;
    }

    master->enemy->s.effects ^= EF_LIGHT;
  } else {
    ent->s.effects ^= EF_LIGHT;
  }
}

/**
 * @brief Handles use activation of a `target_light`, toggling or cycling the light after an optional delay.
 */
static void G_target_light_Use(g_entity_t *ent, g_entity_t *other, g_entity_t *activator) {

  if (ent->delay) {
    ent->Think = G_target_light_Cycle;
    ent->next_think = g_level.time + ent->delay * 1000.0;
  } else {
    G_target_light_Cycle(ent);
  }

  if (ent->wait) {
    ent->Think = G_target_light_Cycle;
    ent->next_think = g_level.time + (ent->delay + ent->wait) * 1000.0;
  }
}

/*QUAKED target_light (1 1 1) (-4 -4 -4) (4 4 4) start_on
 Emits a user-defined light when used. Lights can be chained with teams.

 -------- Keys --------
 color : The light color (default 1.0 1.0 1.0).
 radius : The radius of the light in units (default 300).
 delay : The delay before activating, in seconds (default 0).
 targetname : The target name of this entity.
 team : The team name for alternating lights.
 wait : If specified, an additional cycle will fire after this interval.

 -------- Spawn flags --------
 start_on : The light will start on.

 -------- Notes --------
 Use this entity to add switched lights. Use the wait key to synchronize
 color cycles with other entities.
*/
void G_target_light(g_entity_t *ent) {

  vec3_t color = gi.EntityValue(ent->def, "color")->vec3;
  if (Vec3_Equal(color, Vec3_Zero())) {
    color = Vec3_One();
  }

  float radius = gi.EntityValue(ent->def, "radius")->value;
  radius = radius ?: 300.f;

  ent->s.color = Color_Color32(Color3fv(color));
  ent->s.termination.x = radius;

  if (ent->spawn_flags & LIGHT_START_ON) {
    ent->s.effects |= EF_LIGHT;
  }

  ent->enemy = ent;
  ent->Use = G_target_light_Use;

  gi.LinkEntity(ent);
}

#define SPEAKER_LOOP_ON 1
#define SPEAKER_LOOP_OFF 2

#define SPEAKER_LOOP (SPEAKER_LOOP_ON | SPEAKER_LOOP_OFF)

/**
 * @brief Handles use activation of a `target_speaker`, toggling looping sounds or playing a one-shot sound.
 */
static void G_target_speaker_Use(g_entity_t *ent, g_entity_t *other, g_entity_t *activator) {

  if (ent->spawn_flags & SPEAKER_LOOP) { // looping sound toggles
    if (ent->s.sound) {
      ent->s.sound = 0;
    } else {
      ent->s.sound = ent->sound;
    }
  } else { // intermittent sound
    G_MulticastSound(&(const g_play_sound_t) {
      .index = ent->sound,
      .origin = &ent->s.origin,
    }, MULTICAST_PHS);
  }
}

/*QUAKED target_speaker (1 0 0) (-8 -8 -8) (8 8 8) loop_on loop_off
 Plays a sound each time it is used, or in loop if requested.

 -------- Keys --------
 sound : The name of the sample to play, e.g. voices/haunting.
 targetname : The target name of this entity.

 -------- Spawn flags --------
 loop_on : The sound can be toggled, and will play in loop until used.
 loop_off : The sound can be toggled, and will begin playing when used.

 -------- Notes --------
 Use this entity only when a sound must be triggered by another entity. For
 ambient sounds, use the client-side version, misc_sound.
*/
void G_target_speaker(g_entity_t *ent) {

  const char *sound = gi.EntityValue(ent->def, "sound")->string;
  if (!q_strlen(sound)) {
    G_Warn("No sound specified for %s\n", etos(ent));
    return;
  }

  ent->sound = gi.SoundIndex(sound);

  const int32_t spawn_flags = gi.EntityValue(ent->def, "spawnflags")->integer;

  // check for looping sound
  if (spawn_flags & SPEAKER_LOOP_ON) {
    ent->s.sound = ent->sound;
  }

  ent->Use = G_target_speaker_Use;

  // link the entity so the server can determine who to send updates to
  gi.LinkEntity(ent);
}

/*QUAKED target_string (0 0 1) (-8 -8 -8) (8 8 8)
 Displays a center-printed message to the player when used.
 -------- KEYS --------
 message : The message to display.
 targetname : The target name of this entity.
 */
void G_target_string(g_entity_t *ent) {

  if (!ent->message) {
    ent->message = "";
  }

  // the rest is handled by G_UseTargets
}

#define BALLISTICS_START_ON 0x1
#define BALLISTICS_TOGGLE   0x2

#define BALLISTICS_CLASSNAME "ballistics_"
#define TURRET_CLASSNAME     "turret_"

typedef struct g_ballistics_type_s g_ballistics_type_t;

/**
 * @brief One projectile a `ballistics_*` or `turret_*` entity may fire.
 */
struct g_ballistics_type_s {

  /**
   * @brief The classname suffix that selects this projectile.
   */
  const char *name;

  void (*Fire)(const g_ballistics_type_t *type, g_entity_t *ent, g_entity_t *attacker, const vec3_t start, const vec3_t dir, uint32_t mod);

  g_muzzle_flash_t flash;
  uint32_t ballistics_mod;
  uint32_t turret_mod;

  /**
   * @brief The shortest interval in millis this projectile may be fired at, below which it would
   * exhaust the entity pool or drown itself in sound.
   */
  uint32_t min_wait;

  /**
   * @brief Milliseconds between muzzle flashes, throttling types that fire faster than their
   * flash and its sample can be tolerated at. Zero flashes on every shot.
   */
  uint32_t flash_interval;

  /**
   * @brief True for a beam that persists between thinks rather than a discrete shot. Sustained
   * types hold their beam for as long as they are on, and "wait" is the damage interval.
   */
  bool sustained;

  cvar_t **damage;
  cvar_t **knockback;
  cvar_t **speed;
  cvar_t **radius;
  cvar_t **spread_x;
  cvar_t **spread_y;
  cvar_t **pellets;

  int32_t default_damage;
  int32_t default_speed;
};

/**
 * @brief Fires a blaster bolt.
 */
static void G_ballistics_Blaster(const g_ballistics_type_t *type, g_entity_t *ent, g_entity_t *attacker, const vec3_t start, const vec3_t dir, uint32_t mod) {
  G_BlasterProjectile(ent, attacker, start, dir, ent->speed, ent->damage, ent->knockback, mod);
}

/**
 * @brief Fires a burst of pellets.
 */
static void G_ballistics_Shotgun(const g_ballistics_type_t *type, g_entity_t *ent, g_entity_t *attacker, const vec3_t start, const vec3_t dir, uint32_t mod) {
  G_ShotgunProjectiles(ent, attacker, start, dir, ent->damage, ent->knockback,
    (*type->spread_x)->integer, (*type->spread_y)->integer, (*type->pellets)->integer, mod);
}

/**
 * @brief Fires a single bullet.
 */
static void G_ballistics_Machinegun(const g_ballistics_type_t *type, g_entity_t *ent, g_entity_t *attacker, const vec3_t start, const vec3_t dir, uint32_t mod) {
  G_BulletProjectile(ent, attacker, start, dir, ent->damage, ent->knockback,
    (*type->spread_x)->integer, (*type->spread_y)->integer, mod);
}

/**
 * @brief Fires a spike.
 */
static void G_ballistics_Nail(const g_ballistics_type_t *type, g_entity_t *ent, g_entity_t *attacker, const vec3_t start, const vec3_t dir, uint32_t mod) {
  G_NailProjectile(ent, attacker, start, dir, ent->speed, ent->damage, ent->knockback, mod);
}

/**
 * @brief Fires a rocket.
 */
static void G_ballistics_Rocket(const g_ballistics_type_t *type, g_entity_t *ent, g_entity_t *attacker, const vec3_t start, const vec3_t dir, uint32_t mod) {
  G_RocketProjectile(ent, attacker, start, dir, ent->speed, ent->damage, ent->knockback, ent->damage_radius, mod);
}

/**
 * @brief Fires a Quake rocket.
 */
static void G_ballistics_QuakeRocket(const g_ballistics_type_t *type, g_entity_t *ent, g_entity_t *attacker, const vec3_t start, const vec3_t dir, uint32_t mod) {
  G_QuakeRocketProjectile(ent, attacker, start, dir, ent->speed, ent->damage, ent->knockback, ent->damage_radius);
}

/**
 * @brief Fires a grenade.
 */
static void G_ballistics_Grenade(const g_ballistics_type_t *type, g_entity_t *ent, g_entity_t *attacker, const vec3_t start, const vec3_t dir, uint32_t mod) {
  G_GrenadeProjectile(ent, attacker, start, dir, ent->speed, ent->damage, ent->knockback,
    ent->damage_radius, SECONDS_TO_MILLIS(g_balance_grenadelauncher_timer->value), mod);
}

/**
 * @brief Fires a Quake grenade.
 */
static void G_ballistics_QuakeGrenade(const g_ballistics_type_t *type, g_entity_t *ent, g_entity_t *attacker, const vec3_t start, const vec3_t dir, uint32_t mod) {
  G_QuakeGrenadeProjectile(ent, attacker, start, dir, ent->speed, ent->damage, ent->knockback,
    ent->damage_radius, SECONDS_TO_MILLIS(g_balance_quake_grenadelauncher_timer->value));
}

/**
 * @brief Fires a hyperblaster bolt.
 */
static void G_ballistics_Hyperblaster(const g_ballistics_type_t *type, g_entity_t *ent, g_entity_t *attacker, const vec3_t start, const vec3_t dir, uint32_t mod) {
  G_HyperblasterProjectile(ent, attacker, start, dir, ent->speed, ent->damage, ent->knockback);
}

/**
 * @brief Fires a BFG orb.
 */
static void G_ballistics_Bfg(const g_ballistics_type_t *type, g_entity_t *ent, g_entity_t *attacker, const vec3_t start, const vec3_t dir, uint32_t mod) {
  G_BfgProjectile(ent, attacker, start, dir, ent->speed, ent->damage, ent->knockback, ent->damage_radius);
}

/**
 * @brief Fires a railgun slug.
 */
static void G_ballistics_Rail(const g_ballistics_type_t *type, g_entity_t *ent, g_entity_t *attacker, const vec3_t start, const vec3_t dir, uint32_t mod) {
  G_RailgunProjectile(ent, attacker, start, dir, ent->damage, ent->knockback, mod);
}

/**
 * @brief Creates or refreshes a laser beam.
 */
static void G_ballistics_Laser(const g_ballistics_type_t *type, g_entity_t *ent, g_entity_t *attacker, const vec3_t start, const vec3_t dir, uint32_t mod) {
  G_BeamProjectile(ent, attacker, start, dir, ent->damage, ent->knockback, mod, TRAIL_LASER);
}

/**
 * @brief Creates or refreshes a lightning beam.
 */
static void G_ballistics_Lightning(const g_ballistics_type_t *type, g_entity_t *ent, g_entity_t *attacker, const vec3_t start, const vec3_t dir, uint32_t mod) {
  G_BeamProjectile(ent, attacker, start, dir, ent->damage, ent->knockback, mod, TRAIL_LIGHTNING);
}

/**
 * @brief Scatters giblets.
 */
static void G_ballistics_Giblets(const g_ballistics_type_t *type, g_entity_t *ent, g_entity_t *attacker, const vec3_t start, const vec3_t dir, uint32_t mod) {

  const cm_trace_t tr = gi.Trace(ent->s.origin, start, Box3_Zero(), ent, CONTENTS_MASK_SOLID);

  G_Giblets(&(const g_giblets_t) {
    .origin = tr.fraction < 1.f ? ent->s.origin : start,
    .velocity = Vec3_Scale(dir, ent->speed),
    .count = RandomRangei(2, 5),
    .head = true,
    .damage = ent->damage,
    .knockback = ent->knockback,
    .attacker = attacker,
    .mod = mod,
    .lifetime = 3000
  });
}

static const g_ballistics_type_t g_ballistics_types[] = {
  {
    .name = "blaster",
    .min_wait = 200,
    .Fire = G_ballistics_Blaster,
    .flash = MZ_BLASTER,
    .ballistics_mod = MOD_BALLISTICS_BLASTER,
    .turret_mod = MOD_TURRET_BLASTER,
    .damage = &g_balance_blaster_damage,
    .knockback = &g_balance_blaster_knockback,
    .speed = &g_balance_blaster_speed,
  }, {
    .name = "shotgun",
    .min_wait = 200,
    .Fire = G_ballistics_Shotgun,
    .flash = MZ_SHOTGUN,
    .ballistics_mod = MOD_BALLISTICS_SHOTGUN,
    .turret_mod = MOD_TURRET_SHOTGUN,
    .damage = &g_balance_shotgun_damage,
    .knockback = &g_balance_shotgun_knockback,
    .spread_x = &g_balance_shotgun_spread_x,
    .spread_y = &g_balance_shotgun_spread_y,
    .pellets = &g_balance_shotgun_pellets,
  }, {
    .name = "supershotgun",
    .min_wait = 200,
    .Fire = G_ballistics_Shotgun,
    .flash = MZ_SUPER_SHOTGUN,
    .ballistics_mod = MOD_BALLISTICS_SUPER_SHOTGUN,
    .turret_mod = MOD_TURRET_SUPER_SHOTGUN,
    .damage = &g_balance_supershotgun_damage,
    .knockback = &g_balance_supershotgun_knockback,
    .spread_x = &g_balance_supershotgun_spread_x,
    .spread_y = &g_balance_supershotgun_spread_y,
    .pellets = &g_balance_supershotgun_pellets,
  }, {
    .name = "machinegun",
    .min_wait = 100,
    .Fire = G_ballistics_Machinegun,
    .flash = MZ_MACHINEGUN,
    .ballistics_mod = MOD_BALLISTICS_MACHINEGUN,
    .turret_mod = MOD_TURRET_MACHINEGUN,
    .damage = &g_balance_machinegun_damage,
    .knockback = &g_balance_machinegun_knockback,
    .spread_x = &g_balance_machinegun_spread_x,
    .spread_y = &g_balance_machinegun_spread_y,
  }, {
    .name = "grenadelauncher",
    .min_wait = 400,
    .Fire = G_ballistics_Grenade,
    .flash = MZ_GRENADE_LAUNCHER,
    .ballistics_mod = MOD_BALLISTICS_GRENADE,
    .turret_mod = MOD_TURRET_GRENADE,
    .damage = &g_balance_grenadelauncher_damage,
    .knockback = &g_balance_grenadelauncher_knockback,
    .speed = &g_balance_grenadelauncher_speed,
    .radius = &g_balance_grenadelauncher_radius,
  }, {
    .name = "rocketlauncher",
    .min_wait = 400,
    .Fire = G_ballistics_Rocket,
    .flash = MZ_ROCKET_LAUNCHER,
    .ballistics_mod = MOD_BALLISTICS_ROCKET,
    .turret_mod = MOD_TURRET_ROCKET,
    .damage = &g_balance_rocketlauncher_damage,
    .knockback = &g_balance_rocketlauncher_knockback,
    .speed = &g_balance_rocketlauncher_speed,
    .radius = &g_balance_rocketlauncher_radius,
  }, {
    .name = "hyperblaster",
    .min_wait = 100,
    .Fire = G_ballistics_Hyperblaster,
    .flash = MZ_HYPERBLASTER,
    .ballistics_mod = MOD_BALLISTICS_HYPERBLASTER,
    .turret_mod = MOD_TURRET_HYPERBLASTER,
    .damage = &g_balance_hyperblaster_damage,
    .knockback = &g_balance_hyperblaster_knockback,
    .speed = &g_balance_hyperblaster_speed,
  }, {
    .name = "lightning",
    .Fire = G_ballistics_Lightning,
    .ballistics_mod = MOD_BALLISTICS_LIGHTNING,
    .turret_mod = MOD_TURRET_LIGHTNING,
    .sustained = true,
    .damage = &g_balance_lightning_damage,
    .knockback = &g_balance_lightning_knockback,
  }, {
    .name = "railgun",
    .min_wait = 400,
    .Fire = G_ballistics_Rail,
    .flash = MZ_RAILGUN,
    .ballistics_mod = MOD_BALLISTICS_RAILGUN,
    .turret_mod = MOD_TURRET_RAILGUN,
    .damage = &g_balance_railgun_damage,
    .knockback = &g_balance_railgun_knockback,
  }, {
    .name = "bfg",
    .min_wait = 1000,
    .Fire = G_ballistics_Bfg,
    .flash = MZ_BFG10K,
    .ballistics_mod = MOD_BALLISTICS_BFG,
    .turret_mod = MOD_TURRET_BFG,
    .damage = &g_balance_bfg_damage,
    .knockback = &g_balance_bfg_knockback,
    .speed = &g_balance_bfg_speed,
    .radius = &g_balance_bfg_radius,
  }, {
    .name = "quake_shotgun",
    .min_wait = 200,
    .Fire = G_ballistics_Shotgun,
    .flash = MZ_QUAKE_SHOTGUN,
    .ballistics_mod = MOD_BALLISTICS_QUAKE_SHOTGUN,
    .turret_mod = MOD_TURRET_QUAKE_SHOTGUN,
    .damage = &g_balance_quake_shotgun_damage,
    .knockback = &g_balance_quake_shotgun_knockback,
    .spread_x = &g_balance_quake_shotgun_spread_x,
    .spread_y = &g_balance_quake_shotgun_spread_y,
    .pellets = &g_balance_quake_shotgun_pellets,
  }, {
    .name = "quake_supershotgun",
    .min_wait = 200,
    .Fire = G_ballistics_Shotgun,
    .flash = MZ_QUAKE_SUPER_SHOTGUN,
    .ballistics_mod = MOD_BALLISTICS_QUAKE_SUPER_SHOTGUN,
    .turret_mod = MOD_TURRET_QUAKE_SUPER_SHOTGUN,
    .damage = &g_balance_quake_supershotgun_damage,
    .knockback = &g_balance_quake_supershotgun_knockback,
    .spread_x = &g_balance_quake_supershotgun_spread_x,
    .spread_y = &g_balance_quake_supershotgun_spread_y,
    .pellets = &g_balance_quake_supershotgun_pellets,
  }, {
    .name = "quake_nailgun",
    .min_wait = 100,
    .Fire = G_ballistics_Nail,
    .flash = MZ_QUAKE_NAILGUN,
    .ballistics_mod = MOD_BALLISTICS_QUAKE_NAILGUN,
    .turret_mod = MOD_TURRET_QUAKE_NAILGUN,
    .damage = &g_balance_quake_nailgun_damage,
    .knockback = &g_balance_quake_nailgun_knockback,
    .speed = &g_balance_quake_nailgun_speed,
  }, {
    .name = "quake_supernailgun",
    .min_wait = 100,
    .Fire = G_ballistics_Nail,
    .flash = MZ_QUAKE_SUPER_NAILGUN,
    .ballistics_mod = MOD_BALLISTICS_QUAKE_SUPER_NAILGUN,
    .turret_mod = MOD_TURRET_QUAKE_SUPER_NAILGUN,
    .damage = &g_balance_quake_supernailgun_damage,
    .knockback = &g_balance_quake_supernailgun_knockback,
    .speed = &g_balance_quake_supernailgun_speed,
  }, {
    .name = "quake_grenadelauncher",
    .min_wait = 400,
    .Fire = G_ballistics_QuakeGrenade,
    .flash = MZ_QUAKE_GRENADE_LAUNCHER,
    .ballistics_mod = MOD_BALLISTICS_QUAKE_GRENADE,
    .turret_mod = MOD_TURRET_QUAKE_GRENADE,
    .damage = &g_balance_quake_grenadelauncher_damage,
    .knockback = &g_balance_quake_grenadelauncher_knockback,
    .speed = &g_balance_quake_grenadelauncher_speed,
    .radius = &g_balance_quake_grenadelauncher_radius,
  }, {
    .name = "quake_rocketlauncher",
    .min_wait = 400,
    .Fire = G_ballistics_QuakeRocket,
    .flash = MZ_QUAKE_ROCKET_LAUNCHER,
    .ballistics_mod = MOD_BALLISTICS_QUAKE_ROCKET,
    .turret_mod = MOD_TURRET_QUAKE_ROCKET,
    .damage = &g_balance_quake_rocketlauncher_damage,
    .knockback = &g_balance_quake_rocketlauncher_knockback,
    .speed = &g_balance_quake_rocketlauncher_speed,
    .radius = &g_balance_quake_rocketlauncher_radius,
  }, {
    .name = "quake_thunderbolt",
    .Fire = G_ballistics_Lightning,
    .ballistics_mod = MOD_BALLISTICS_QUAKE_THUNDERBOLT,
    .turret_mod = MOD_TURRET_QUAKE_THUNDERBOLT,
    .sustained = true,
    .damage = &g_balance_quake_thunderbolt_damage,
    .knockback = &g_balance_quake_thunderbolt_knockback,
  }, {
    .name = "laser",
    .Fire = G_ballistics_Laser,
    .ballistics_mod = MOD_BALLISTICS_LASER,
    .turret_mod = MOD_TURRET_LASER,
    .sustained = true,
    .default_damage = 20,
  }, {
    .name = "giblets",
    .Fire = G_ballistics_Giblets,
    .flash = MZ_LOGOUT,
    .ballistics_mod = MOD_BALLISTICS_GIBLETS,
    .turret_mod = MOD_TURRET_GIBLETS,
    .min_wait = 250,
    .flash_interval = 500,
    .default_damage = 10,
    .default_speed = 500,
  }
};

/**
 * @brief Resolves the projectile named by the suffix of a `ballistics_*` or `turret_*` classname.
 */
static const g_ballistics_type_t *G_ballistics_Type(const char *name) {

  for (size_t i = 0; i < lengthof(g_ballistics_types); i++) {
    if (!q_strcmp(g_ballistics_types[i].name, name)) {
      return &g_ballistics_types[i];
    }
  }

  return NULL;
}

/**
 * @brief Resolves the direction a trap fires in, tracking its target entity if it has one.
 */
static vec3_t G_ballistics_Dir(g_entity_t *ent) {

  if (ent->target) {
    const g_entity_t *target = G_PickTarget(ent->target);

    if (target) {
      return Vec3_Normalize(Vec3_Subtract(Box3_Center(target->abs_bounds), ent->s.origin));
    }

    G_Warn("%s has invalid target %s\n", etos(ent), ent->target);
    ent->target = NULL;
  }

  return ent->move_dir;
}

/**
 * @brief Emits a single projectile along the given direction, with the muzzle pushed clear of the
 * brushwork the entity is typically embedded in.
 * @remarks The muzzle flash is throttled per projectile type, independently of the fire rate. A
 * hitscan type may legitimately fire on every tick, which no flash and its sample survive. A
 * sustained beam is not a shot at all, so it never flashes.
 */
static void G_ballistics_Fire(g_entity_t *ent, g_entity_t *attacker, const vec3_t dir, uint32_t mod) {

  const g_ballistics_type_t *type = ent->ballistics;

  const vec3_t aim = Vec3_RandomizeDir(dir, ent->accel);
  const vec3_t start = Vec3_Fmaf(ent->s.origin, 8.f, aim);

  type->Fire(type, ent, attacker, start, aim, mod);

  if (type->sustained) {
    return;
  }

  if (g_level.time >= ent->flash_time) {
    ent->flash_time = g_level.time + type->flash_interval;

    G_WorldMuzzleFlash(start, aim, type->flash, ent->s.client);
  }
}

/**
 * @brief Think callback for a free-running trap; fires and re-arms itself.
 */
static void G_ballistics_Think(g_entity_t *ent) {

  const g_ballistics_type_t *type = ent->ballistics;

  G_ballistics_Fire(ent, ent, G_ballistics_Dir(ent), type->ballistics_mod);

  if (ent->count) {
    // a sustained type holds its beam, so it refreshes every tick rather than at "wait"
    const float wait = type->sustained ? 0.f
      : SECONDS_TO_MILLIS(ent->wait) + SECONDS_TO_MILLIS(ent->random) * RandomRangef(-1.f, 1.f);

    ent->next_think = g_level.time + (uint32_t) Maxf(wait, QUETOO_TICK_MILLIS);
  } else {
    ent->next_think = 0;
  }
}

/**
 * @brief Handles use activation of a trap, toggling a free-running one or firing a single shot
 * after an optional delay.
 */
static void G_ballistics_Use(g_entity_t *ent, g_entity_t *other, g_entity_t *activator) {

  if (ent->spawn_flags & BALLISTICS_TOGGLE) {

    ent->count = !ent->count;

    if (ent->count) {
      ent->next_think = g_level.time + (uint32_t) Maxf(SECONDS_TO_MILLIS(ent->delay), QUETOO_TICK_MILLIS);
    } else {
      ent->next_think = 0;

      const g_ballistics_type_t *type = ent->ballistics;

      if (type->sustained) {
        G_FreeBeamProjectile(ent);
      }
    }

    return;
  }

  if (ent->timestamp > g_level.time) {
    return;
  }

  ent->timestamp = g_level.time + SECONDS_TO_MILLIS(ent->wait);

  if (ent->delay) {
    ent->next_think = g_level.time + (uint32_t) Maxf(SECONDS_TO_MILLIS(ent->delay), QUETOO_TICK_MILLIS);
  } else {
    const g_ballistics_type_t *type = ent->ballistics;

    G_ballistics_Fire(ent, ent, G_ballistics_Dir(ent), type->ballistics_mod);
  }
}

/**
 * @brief Handles use activation of a turret, firing along the activator's view and crediting them
 * with any damage.
 */
static void G_turret_Use(g_entity_t *ent, g_entity_t *other, g_entity_t *activator) {

  const g_ballistics_type_t *type = ent->ballistics;

  // "wait" gates shots; a sustained beam must be refreshed far more often than that or it
  // expires between uses, and its damage interval is enforced by the beam itself
  if (!type->sustained) {

    if (ent->timestamp > g_level.time) {
      return;
    }

    ent->timestamp = g_level.time + SECONDS_TO_MILLIS(ent->wait);
  }

  if (activator && activator->client) {
    ent->s.client = activator->s.client;
    G_ballistics_Fire(ent, activator, activator->client->forward, type->turret_mod);
  } else {
    ent->s.client = MAX_CLIENTS;
    G_ballistics_Fire(ent, ent, ent->move_dir, type->ballistics_mod);
  }
}

/**
 * @brief Common initialization for both roles, applying the balance defaults the mapper did not
 * override.
 */
static void G_ballistics_Init(g_entity_t *ent, const g_ballistics_type_t *type) {

  ent->ballistics = type;

  if (ent->damage == 0 && type->damage) {
    ent->damage = (*type->damage)->integer;
  }

  if (ent->speed == 0.f) {
    ent->speed = type->speed ? (*type->speed)->integer : type->default_speed;
  }

  if (ent->damage == 0 && type->damage == NULL) {
    ent->damage = type->default_damage;
  }

  if (type->radius) {
    ent->damage_radius = (*type->radius)->value;
  }

  ent->knockback = gi.EntityValue(ent->def, "knockback")->integer;
  if (ent->knockback == 0 && type->knockback) {
    ent->knockback = (*type->knockback)->integer;
  }

  ent->accel = Clampf(gi.EntityValue(ent->def, "spread")->value, 0.f, 1.f);

  if (!(gi.EntityValue(ent->def, "wait")->parsed & ENTITY_FLOAT)) {
    ent->wait = 1.f;
  }

  ent->wait = Maxf(ent->wait, MILLIS_TO_SECONDS(type->min_wait));
  ent->random = Maxf(ent->random, 0.f);
  ent->delay = Maxf(ent->delay, 0.f);

  ent->count = 0;
  ent->s.client = MAX_CLIENTS;

  if (type->sustained) {
    vec3_t color = gi.EntityValue(ent->def, "color")->vec3;
    if (Vec3_Equal(color, Vec3_Zero())) {
      color = Vec3_One();
    }

    ent->s.color = Color_Color32(Color3fv(color));
  }

  G_SetMoveDir(ent);

  gi.LinkEntity(ent);
}

/*
 * These entities are documented as data rather than as QUAKED blocks here, the way the items are.
 * One block per classname would mean thirty-eight copies of the same nine keys; instead
 * Quetoo.fgd carries a @BaseClass for each role, exactly as it does for `Item` and `Weapon`, and
 * every classname costs one line. See quetoo-data/target/default/scripts.
 *
 * Keys, shared by every classname:
 *
 *   angles      the direction fired in; a turret uses this only when something other than a
 *               player triggers it. -1 is up, -2 is down
 *   color       the beam color of the sustained types, defaulting to white
 *   delay       seconds between being used and firing
 *   dmg         damage per projectile, defaulting to the weapon's own balance
 *   knockback   knockback per projectile, likewise
 *   random      seconds of variance added to "wait"
 *   speed       projectile speed, likewise
 *   spread      cone of fire, 0.0 for pinpoint through 1.0 for wild
 *   target      an entity to aim at, re-evaluated every shot, overriding "angles"
 *   targetname  the target name of this entity
 *   wait        seconds between shots, or between damage for the sustained types
 *
 * Spawn flags:
 *
 *   start_on    fires from level start, without being used. `ballistics_*` only
 *   toggle      using it toggles it on and off rather than firing one shot. `ballistics_*` only
 */

/**
 * @brief Spawns a `ballistics_*` or `turret_*` entity, resolving its projectile from the suffix
 * of its classname.
 * @return True if the classname named one of these entities, whether or not it could be spawned.
 * @remarks The weapon is a classname rather than a spawnflag because it is a choice of one from
 * nineteen, which flags model as nineteen independent checkboxes. Editors also group these by
 * their prefix, and the mapper picks a `turret_railgun` rather than a turret plus a checkbox.
 */
bool G_ballistics(g_entity_t *ent) {

  const char *name;
  bool turret;

  if (!strncmp(ent->classname, BALLISTICS_CLASSNAME, strlen(BALLISTICS_CLASSNAME))) {
    name = ent->classname + strlen(BALLISTICS_CLASSNAME);
    turret = false;
  } else if (!strncmp(ent->classname, TURRET_CLASSNAME, strlen(TURRET_CLASSNAME))) {
    name = ent->classname + strlen(TURRET_CLASSNAME);
    turret = true;
  } else {
    return false;
  }

  const g_ballistics_type_t *type = G_ballistics_Type(name);
  if (type == NULL) {
    G_Warn("%s has no such projectile \"%s\"\n", etos(ent), name);
    G_FreeEntity(ent);
    return true;
  }

  G_ballistics_Init(ent, type);

  if (turret) {
    ent->Use = G_turret_Use;
  } else {
    ent->Use = G_ballistics_Use;
    ent->Think = G_ballistics_Think;

    if (ent->spawn_flags & BALLISTICS_START_ON) {
      ent->count = 1;
      ent->next_think = g_level.time + RandomRangeu(1, 1000);
    }
  }

  return true;
}
