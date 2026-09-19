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
static void G_target_light_Cycle(GameEntity *ent) {

  GameEntity *master = ent->teamMaster;
  if (master) {
    G_Debug("Cycling %s\n", etos(master->enemy));

    master->enemy->s.effects ^= EF_LIGHT;
    master->enemy = master->enemy->teamNext;

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
static void G_target_light_Use(GameEntity *ent, GameEntity *other, GameEntity *activator) {

  if (ent->delay) {
    ent->Think = G_target_light_Cycle;
    ent->nextThink = gLevel.time + ent->delay * 1000.0;
  } else {
    G_target_light_Cycle(ent);
  }

  if (ent->wait) {
    ent->Think = G_target_light_Cycle;
    ent->nextThink = gLevel.time + (ent->delay + ent->wait) * 1000.0;
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
void G_target_light(GameEntity *ent) {

  Vec3 color = gi.EntityValue(ent->def, "color")->vec3;
  if (Vec3_Equal(color, Vec3_Zero())) {
    color = Vec3_One();
  }

  float radius = gi.EntityValue(ent->def, "radius")->value;
  radius = radius ?: 300.f;

  ent->s.color = Color_Color32(Color3fv(color));
  ent->s.termination.x = radius;

  if (ent->spawnFlags & LIGHT_START_ON) {
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
static void G_target_speaker_Use(GameEntity *ent, GameEntity *other, GameEntity *activator) {

  if (ent->spawnFlags & SPEAKER_LOOP) { // looping sound toggles
    if (ent->s.sound) {
      ent->s.sound = 0;
    } else {
      ent->s.sound = ent->sound;
    }
  } else { // intermittent sound
    G_MulticastSound(&(const GamePlaySound) {
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
void G_target_speaker(GameEntity *ent) {

  const char *sound = gi.EntityValue(ent->def, "sound")->string;
  if (!q_strlen(sound)) {
    G_Warn("No sound specified for %s\n", etos(ent));
    return;
  }

  ent->sound = gi.SoundIndex(sound);

  const int32_t spawnFlags = gi.EntityValue(ent->def, "spawnflags")->integer;

  // check for looping sound
  if (spawnFlags & SPEAKER_LOOP_ON) {
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
void G_target_string(GameEntity *ent) {

  if (!ent->message) {
    ent->message = "";
  }

  // the rest is handled by G_UseTargets
}

#define BALLISTICS_START_ON 0x1
#define BALLISTICS_TOGGLE   0x2

#define BALLISTICS_CLASSNAME "ballistics_"
#define TURRET_CLASSNAME     "turret_"

typedef struct GameBallisticsType GameBallisticsType;

/**
 * @brief One projectile a `ballistics_*` or `turret_*` entity may fire.
 */
struct GameBallisticsType {

  /**
   * @brief The classname suffix that selects this projectile.
   */
  const char *name;

  void (*Fire)(const GameBallisticsType *type, GameEntity *ent, GameEntity *attacker, const Vec3 start, const Vec3 dir, uint32_t mod);

  GameMuzzleFlash flash;
  uint32_t ballisticsMod;
  uint32_t turretMod;

  /**
   * @brief The shortest interval in millis this projectile may be fired at, below which it would
   * exhaust the entity pool or drown itself in sound.
   */
  uint32_t minWait;

  /**
   * @brief The interval an entity fires at when it does not ask for one, taken from the weapon's
   * own refire so that a turret behaves exactly like the weapon it mounts. Mappers tune from
   * there, down to `min_wait`.
   */
  Cvar **refire;

  /**
   * @brief The same, in millis, for the two projectiles that have no weapon to inherit from.
   */
  uint32_t defaultWait;

  /**
   * @brief The weapon's wind-up, if it has one, inserted between being used and firing so that
   * the entity primes exactly as the weapon does. Only the BFG has one.
   */
  Cvar **prefire;

  /**
   * @brief How far in units the projectile leaves the entity, which must clear its own bounds or
   * `G_ImmediateWall` plants it back on the entity, embedded in whatever it was fired past. Eight
   * when unset, which suits everything but the BFG's 48 unit ball.
   */
  uint32_t muzzle;

  /**
   * @brief Milliseconds between muzzle flashes, throttling types that fire faster than their
   * flash and its sample can be tolerated at. Zero flashes on every shot.
   */
  uint32_t flashInterval;

  /**
   * @brief True for a beam that persists between thinks rather than a discrete shot. Sustained
   * types hold their beam for as long as they are on, and "wait" is the damage interval.
   */
  bool sustained;

  Cvar **damage;
  Cvar **knockback;
  Cvar **speed;
  Cvar **radius;
  Cvar **spreadX;
  Cvar **spreadY;
  Cvar **pellets;

  int32_t defaultDamage;
  int32_t defaultSpeed;
};

/**
 * @brief Fires a blaster bolt.
 */
static void G_ballistics_Blaster(const GameBallisticsType *type, GameEntity *ent, GameEntity *attacker, const Vec3 start, const Vec3 dir, uint32_t mod) {
  G_BlasterProjectile(ent, attacker, start, dir, ent->speed, ent->damage, ent->knockback, mod);
}

/**
 * @brief Fires a burst of pellets.
 */
static void G_ballistics_Shotgun(const GameBallisticsType *type, GameEntity *ent, GameEntity *attacker, const Vec3 start, const Vec3 dir, uint32_t mod) {
  G_ShotgunProjectiles(ent, attacker, start, dir, ent->damage, ent->knockback,
    (*type->spreadX)->integer, (*type->spreadY)->integer, (*type->pellets)->integer, mod);
}

/**
 * @brief Fires a single bullet.
 */
static void G_ballistics_Machinegun(const GameBallisticsType *type, GameEntity *ent, GameEntity *attacker, const Vec3 start, const Vec3 dir, uint32_t mod) {
  G_BulletProjectile(ent, attacker, start, dir, ent->damage, ent->knockback,
    (*type->spreadX)->integer, (*type->spreadY)->integer, mod);
}

/**
 * @brief Fires a spike.
 */
static void G_ballistics_Nail(const GameBallisticsType *type, GameEntity *ent, GameEntity *attacker, const Vec3 start, const Vec3 dir, uint32_t mod) {
  G_NailProjectile(ent, attacker, start, dir, ent->speed, ent->damage, ent->knockback, mod);
}

/**
 * @brief Fires a rocket.
 */
static void G_ballistics_Rocket(const GameBallisticsType *type, GameEntity *ent, GameEntity *attacker, const Vec3 start, const Vec3 dir, uint32_t mod) {
  G_RocketProjectile(ent, attacker, start, dir, ent->speed, ent->damage, ent->knockback, ent->damageRadius, mod);
}

/**
 * @brief Fires a Quake rocket.
 */
static void G_ballistics_QuakeRocket(const GameBallisticsType *type, GameEntity *ent, GameEntity *attacker, const Vec3 start, const Vec3 dir, uint32_t mod) {
  G_QuakeRocketProjectile(ent, attacker, start, dir, ent->speed, ent->damage, ent->knockback, ent->damageRadius);
}

/**
 * @brief Fires a grenade.
 */
static void G_ballistics_Grenade(const GameBallisticsType *type, GameEntity *ent, GameEntity *attacker, const Vec3 start, const Vec3 dir, uint32_t mod) {
  G_GrenadeProjectile(ent, attacker, start, dir, ent->speed, ent->damage, ent->knockback,
    ent->damageRadius, SECONDS_TO_MILLIS(g_balanceGrenadelauncherTimer->value), mod);
}

/**
 * @brief Fires a Quake grenade.
 */
static void G_ballistics_QuakeGrenade(const GameBallisticsType *type, GameEntity *ent, GameEntity *attacker, const Vec3 start, const Vec3 dir, uint32_t mod) {
  G_QuakeGrenadeProjectile(ent, attacker, start, dir, ent->speed, ent->damage, ent->knockback,
    ent->damageRadius, SECONDS_TO_MILLIS(g_balanceQuakeGrenadelauncherTimer->value));
}

/**
 * @brief Fires a hyperblaster bolt.
 */
static void G_ballistics_Hyperblaster(const GameBallisticsType *type, GameEntity *ent, GameEntity *attacker, const Vec3 start, const Vec3 dir, uint32_t mod) {
  G_HyperblasterProjectile(ent, attacker, start, dir, ent->speed, ent->damage, ent->knockback);
}

/**
 * @brief Fires a BFG orb.
 */
static void G_ballistics_Bfg(const GameBallisticsType *type, GameEntity *ent, GameEntity *attacker, const Vec3 start, const Vec3 dir, uint32_t mod) {
  G_BfgProjectile(ent, attacker, start, dir, ent->speed, ent->damage, ent->knockback, ent->damageRadius);
}

/**
 * @brief Fires a railgun slug.
 */
static void G_ballistics_Rail(const GameBallisticsType *type, GameEntity *ent, GameEntity *attacker, const Vec3 start, const Vec3 dir, uint32_t mod) {
  G_RailgunProjectile(ent, attacker, start, dir, ent->damage, ent->knockback, mod);
}

/**
 * @brief Creates or refreshes a laser beam.
 */
static void G_ballistics_Laser(const GameBallisticsType *type, GameEntity *ent, GameEntity *attacker, const Vec3 start, const Vec3 dir, uint32_t mod) {
  G_BeamProjectile(ent, attacker, start, dir, ent->damage, ent->knockback, mod, TRAIL_LASER,
    gMedia.sounds.laserFly);
}

/**
 * @brief Creates or refreshes a lightning beam.
 */
static void G_ballistics_Lightning(const GameBallisticsType *type, GameEntity *ent, GameEntity *attacker, const Vec3 start, const Vec3 dir, uint32_t mod) {
  G_BeamProjectile(ent, attacker, start, dir, ent->damage, ent->knockback, mod, TRAIL_LIGHTNING,
    gMedia.sounds.lightningFly);
}

/**
 * @brief Scatters giblets.
 */
static void G_ballistics_Giblets(const GameBallisticsType *type, GameEntity *ent, GameEntity *attacker, const Vec3 start, const Vec3 dir, uint32_t mod) {

  const CmTrace tr = gi.Trace(ent->s.origin, start, Box3_Zero(), ent, CONTENTS_MASK_SOLID);

  G_Giblets(&(const GameGiblets) {
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

static const GameBallisticsType gBallisticsTypes[] = {
  {
    .name = "blaster",
    .refire = &g_balanceBlasterRefire,
    .minWait = 200,
    .Fire = G_ballistics_Blaster,
    .flash = MZ_BLASTER,
    .ballisticsMod = MOD_BALLISTICS_BLASTER,
    .turretMod = MOD_TURRET_BLASTER,
    .damage = &g_balanceBlasterDamage,
    .knockback = &g_balanceBlasterKnockback,
    .speed = &g_balanceBlasterSpeed,
  }, {
    .name = "shotgun",
    .refire = &g_balanceShotgunRefire,
    .minWait = 200,
    .Fire = G_ballistics_Shotgun,
    .flash = MZ_SHOTGUN,
    .ballisticsMod = MOD_BALLISTICS_SHOTGUN,
    .turretMod = MOD_TURRET_SHOTGUN,
    .damage = &g_balanceShotgunDamage,
    .knockback = &g_balanceShotgunKnockback,
    .spreadX = &g_balanceShotgunSpreadX,
    .spreadY = &g_balanceShotgunSpreadY,
    .pellets = &g_balanceShotgunPellets,
  }, {
    .name = "supershotgun",
    .refire = &g_balanceSupershotgunRefire,
    .minWait = 200,
    .Fire = G_ballistics_Shotgun,
    .flash = MZ_SUPER_SHOTGUN,
    .ballisticsMod = MOD_BALLISTICS_SUPER_SHOTGUN,
    .turretMod = MOD_TURRET_SUPER_SHOTGUN,
    .damage = &g_balanceSupershotgunDamage,
    .knockback = &g_balanceSupershotgunKnockback,
    .spreadX = &g_balanceSupershotgunSpreadX,
    .spreadY = &g_balanceSupershotgunSpreadY,
    .pellets = &g_balanceSupershotgunPellets,
  }, {
    .name = "machinegun",
    .refire = &g_balanceMachinegunRefire,
    .minWait = 20,
    .Fire = G_ballistics_Machinegun,
    .flash = MZ_MACHINEGUN,
    .ballisticsMod = MOD_BALLISTICS_MACHINEGUN,
    .turretMod = MOD_TURRET_MACHINEGUN,
    .damage = &g_balanceMachinegunDamage,
    .knockback = &g_balanceMachinegunKnockback,
    .spreadX = &g_balanceMachinegunSpreadX,
    .spreadY = &g_balanceMachinegunSpreadY,
  }, {
    .name = "grenadelauncher",
    .refire = &g_balanceGrenadelauncherRefire,
    .minWait = 400,
    .Fire = G_ballistics_Grenade,
    .flash = MZ_GRENADE_LAUNCHER,
    .ballisticsMod = MOD_BALLISTICS_GRENADE,
    .turretMod = MOD_TURRET_GRENADE,
    .damage = &g_balanceGrenadelauncherDamage,
    .knockback = &g_balanceGrenadelauncherKnockback,
    .speed = &g_balanceGrenadelauncherSpeed,
    .radius = &g_balanceGrenadelauncherRadius,
  }, {
    .name = "rocketlauncher",
    .refire = &g_balanceRocketlauncherRefire,
    .minWait = 400,
    .Fire = G_ballistics_Rocket,
    .flash = MZ_ROCKET_LAUNCHER,
    .ballisticsMod = MOD_BALLISTICS_ROCKET,
    .turretMod = MOD_TURRET_ROCKET,
    .damage = &g_balanceRocketlauncherDamage,
    .knockback = &g_balanceRocketlauncherKnockback,
    .speed = &g_balanceRocketlauncherSpeed,
    .radius = &g_balanceRocketlauncherRadius,
  }, {
    .name = "hyperblaster",
    .refire = &g_balanceHyperblasterRefire,
    .minWait = 40,
    .Fire = G_ballistics_Hyperblaster,
    .flash = MZ_HYPERBLASTER,
    .ballisticsMod = MOD_BALLISTICS_HYPERBLASTER,
    .turretMod = MOD_TURRET_HYPERBLASTER,
    .damage = &g_balanceHyperblasterDamage,
    .knockback = &g_balanceHyperblasterKnockback,
    .speed = &g_balanceHyperblasterSpeed,
  }, {
    .name = "lightning",
    .flash = MZ_LIGHTNING,
    .refire = &g_balanceLightningRefire,
    .Fire = G_ballistics_Lightning,
    .ballisticsMod = MOD_BALLISTICS_LIGHTNING,
    .turretMod = MOD_TURRET_LIGHTNING,
    .sustained = true,
    .damage = &g_balanceLightningDamage,
    .knockback = &g_balanceLightningKnockback,
  }, {
    .name = "railgun",
    .refire = &g_balanceRailgunRefire,
    .minWait = 400,
    .Fire = G_ballistics_Rail,
    .flash = MZ_RAILGUN,
    .ballisticsMod = MOD_BALLISTICS_RAILGUN,
    .turretMod = MOD_TURRET_RAILGUN,
    .damage = &g_balanceRailgunDamage,
    .knockback = &g_balanceRailgunKnockback,
  }, {
    .name = "bfg",
    .refire = &g_balanceBfgRefire,
    .prefire = &g_balanceBfgPrefire,
    .muzzle = 48,
    .minWait = 1000,
    .Fire = G_ballistics_Bfg,
    .flash = MZ_BFG10K,
    .ballisticsMod = MOD_BALLISTICS_BFG,
    .turretMod = MOD_TURRET_BFG,
    .damage = &g_balanceBfgDamage,
    .knockback = &g_balanceBfgKnockback,
    .speed = &g_balanceBfgSpeed,
    .radius = &g_balanceBfgRadius,
  }, {
    .name = "quake_shotgun",
    .refire = &g_balanceQuakeShotgunRefire,
    .minWait = 200,
    .Fire = G_ballistics_Shotgun,
    .flash = MZ_QUAKE_SHOTGUN,
    .ballisticsMod = MOD_BALLISTICS_QUAKE_SHOTGUN,
    .turretMod = MOD_TURRET_QUAKE_SHOTGUN,
    .damage = &g_balanceQuakeShotgunDamage,
    .knockback = &g_balanceQuakeShotgunKnockback,
    .spreadX = &g_balanceQuakeShotgunSpreadX,
    .spreadY = &g_balanceQuakeShotgunSpreadY,
    .pellets = &g_balanceQuakeShotgunPellets,
  }, {
    .name = "quake_supershotgun",
    .refire = &g_balanceQuakeSupershotgunRefire,
    .minWait = 200,
    .Fire = G_ballistics_Shotgun,
    .flash = MZ_QUAKE_SUPER_SHOTGUN,
    .ballisticsMod = MOD_BALLISTICS_QUAKE_SUPER_SHOTGUN,
    .turretMod = MOD_TURRET_QUAKE_SUPER_SHOTGUN,
    .damage = &g_balanceQuakeSupershotgunDamage,
    .knockback = &g_balanceQuakeSupershotgunKnockback,
    .spreadX = &g_balanceQuakeSupershotgunSpreadX,
    .spreadY = &g_balanceQuakeSupershotgunSpreadY,
    .pellets = &g_balanceQuakeSupershotgunPellets,
  }, {
    .name = "quake_nailgun",
    .refire = &g_balanceQuakeNailgunRefire,
    .minWait = 40,
    .Fire = G_ballistics_Nail,
    .flash = MZ_QUAKE_NAILGUN,
    .ballisticsMod = MOD_BALLISTICS_QUAKE_NAILGUN,
    .turretMod = MOD_TURRET_QUAKE_NAILGUN,
    .damage = &g_balanceQuakeNailgunDamage,
    .knockback = &g_balanceQuakeNailgunKnockback,
    .speed = &g_balanceQuakeNailgunSpeed,
  }, {
    .name = "quake_supernailgun",
    .refire = &g_balanceQuakeSupernailgunRefire,
    .minWait = 60,
    .Fire = G_ballistics_Nail,
    .flash = MZ_QUAKE_SUPER_NAILGUN,
    .ballisticsMod = MOD_BALLISTICS_QUAKE_SUPER_NAILGUN,
    .turretMod = MOD_TURRET_QUAKE_SUPER_NAILGUN,
    .damage = &g_balanceQuakeSupernailgunDamage,
    .knockback = &g_balanceQuakeSupernailgunKnockback,
    .speed = &g_balanceQuakeSupernailgunSpeed,
  }, {
    .name = "quake_grenadelauncher",
    .refire = &g_balanceQuakeGrenadelauncherRefire,
    .minWait = 400,
    .Fire = G_ballistics_QuakeGrenade,
    .flash = MZ_QUAKE_GRENADE_LAUNCHER,
    .ballisticsMod = MOD_BALLISTICS_QUAKE_GRENADE,
    .turretMod = MOD_TURRET_QUAKE_GRENADE,
    .damage = &g_balanceQuakeGrenadelauncherDamage,
    .knockback = &g_balanceQuakeGrenadelauncherKnockback,
    .speed = &g_balanceQuakeGrenadelauncherSpeed,
    .radius = &g_balanceQuakeGrenadelauncherRadius,
  }, {
    .name = "quake_rocketlauncher",
    .refire = &g_balanceQuakeRocketlauncherRefire,
    .minWait = 400,
    .Fire = G_ballistics_QuakeRocket,
    .flash = MZ_QUAKE_ROCKET_LAUNCHER,
    .ballisticsMod = MOD_BALLISTICS_QUAKE_ROCKET,
    .turretMod = MOD_TURRET_QUAKE_ROCKET,
    .damage = &g_balanceQuakeRocketlauncherDamage,
    .knockback = &g_balanceQuakeRocketlauncherKnockback,
    .speed = &g_balanceQuakeRocketlauncherSpeed,
    .radius = &g_balanceQuakeRocketlauncherRadius,
  }, {
    .name = "quake_thunderbolt",
    .flash = MZ_LIGHTNING,
    .refire = &g_balanceQuakeThunderboltRefire,
    .Fire = G_ballistics_Lightning,
    .ballisticsMod = MOD_BALLISTICS_QUAKE_THUNDERBOLT,
    .turretMod = MOD_TURRET_QUAKE_THUNDERBOLT,
    .sustained = true,
    .damage = &g_balanceQuakeThunderboltDamage,
    .knockback = &g_balanceQuakeThunderboltKnockback,
  }, {
    .name = "laser",
    .flash = MZ_LASER,
    .defaultWait = 100,
    .Fire = G_ballistics_Laser,
    .ballisticsMod = MOD_BALLISTICS_LASER,
    .turretMod = MOD_TURRET_LASER,
    .sustained = true,
    .defaultDamage = 20,
  }, {
    .name = "giblets",
    .defaultWait = 1000,
    .Fire = G_ballistics_Giblets,
    .flash = MZ_LOGOUT,
    .ballisticsMod = MOD_BALLISTICS_GIBLETS,
    .turretMod = MOD_TURRET_GIBLETS,
    .minWait = 500,
    .flashInterval = 500,
    .defaultDamage = 10,
    .defaultSpeed = 500,
  }
};

/**
 * @brief Resolves the projectile named by the suffix of a `ballistics_*` or `turret_*` classname.
 */
static const GameBallisticsType *G_ballistics_Type(const char *name) {

  for (size_t i = 0; i < lengthof(gBallisticsTypes); i++) {
    if (!q_strcmp(gBallisticsTypes[i].name, name)) {
      return &gBallisticsTypes[i];
    }
  }

  return NULL;
}

/**
 * @brief Resolves the direction a trap fires in, tracking its target entity if it has one.
 */
static Vec3 G_ballistics_Dir(GameEntity *ent) {

  if (ent->target) {
    const GameEntity *target = G_PickTarget(ent->target);

    if (target) {
      return Vec3_Normalize(Vec3_Subtract(Box3_Center(target->absBounds), ent->s.origin));
    }

    G_Warn("%s has invalid target %s\n", etos(ent), ent->target);
    ent->target = NULL;
  }

  return ent->moveDir;
}

/**
 * @brief Emits a single projectile along the given direction, with the muzzle pushed clear of the
 * brushwork the entity is typically embedded in.
 * @remarks The muzzle flash is throttled per projectile type, independently of the fire rate. A
 * hitscan type may legitimately fire on every tick, which no flash and its sample survive. A
 * sustained beam is not a shot at all, so it never flashes.
 */
static void G_ballistics_Fire(GameEntity *ent, GameEntity *attacker, const Vec3 dir, uint32_t mod) {

  const GameBallisticsType *type = ent->ballistics;

  const Vec3 aim = Vec3_RandomizeDir(dir, ent->accel);
  const Vec3 start = Vec3_Fmaf(ent->s.origin, type->muzzle ? (float) type->muzzle : 8.f, aim);

  // a beam is struck up once and hums thereafter, as the lightning weapons do, so it sounds
  // only on the tick that creates it and not on the ticks that sustain it
  const bool sustaining = type->sustained && G_FindBeamProjectile(ent);

  type->Fire(type, ent, attacker, start, aim, mod);

  if (sustaining) {
    return;
  }

  if (gLevel.time >= ent->flashTime) {
    ent->flashTime = gLevel.time + type->flashInterval;

    G_WorldMuzzleFlash(start, aim, type->flash, ent->s.client);
  }
}

/**
 * @brief Resolves the wind-up before a shot leaves, and primes the weapon if it has one.
 * @return The millis to wait before firing, or zero to fire at once.
 */
static uint32_t G_ballistics_Prefire(GameEntity *ent) {

  const GameBallisticsType *type = ent->ballistics;

  if (type->prefire == NULL) {
    return 0;
  }

  const uint32_t prefire = (uint32_t) SECONDS_TO_MILLIS((*type->prefire)->value);

  if (prefire) {
    G_MulticastSound(&(const GamePlaySound) {
      .index = gMedia.sounds.bfgPrime,
      .origin = &ent->s.origin,
    }, MULTICAST_PHS);
  }

  return prefire;
}

/**
 * @brief Think callback for a free-running trap; fires and re-arms itself.
 */
static void G_ballistics_Think(GameEntity *ent) {

  const GameBallisticsType *type = ent->ballistics;

  if (ent->activator && ent->activator->client) {
    G_ballistics_Fire(ent, ent->activator, ent->activator->client->forward, type->turretMod);
    ent->activator = NULL;
  } else {
    G_ballistics_Fire(ent, ent, G_ballistics_Dir(ent), type->ballisticsMod);
  }

  if (ent->count) {
    // a sustained type holds its beam, so it refreshes every tick rather than at "wait"
    const float wait = type->sustained ? 0.f
      : SECONDS_TO_MILLIS(ent->wait) + SECONDS_TO_MILLIS(ent->random) * RandomRangef(-1.f, 1.f);

    ent->nextThink = gLevel.time + (uint32_t) Maxf(wait, QUETOO_TICK_MILLIS);
  } else {
    ent->nextThink = 0;
  }
}

/**
 * @brief Handles use activation of a trap, toggling a free-running one or firing a single shot
 * after an optional delay.
 */
static void G_ballistics_Use(GameEntity *ent, GameEntity *other, GameEntity *activator) {

  if (ent->spawnFlags & BALLISTICS_TOGGLE) {

    ent->count = !ent->count;

    if (ent->count) {
      ent->nextThink = gLevel.time + (uint32_t) Maxf(SECONDS_TO_MILLIS(ent->delay), QUETOO_TICK_MILLIS);
    } else {
      ent->nextThink = 0;

      const GameBallisticsType *type = ent->ballistics;

      if (type->sustained) {
        G_FreeBeamProjectile(ent);
      }
    }

    return;
  }

  if (ent->timestamp > gLevel.time) {
    return;
  }

  ent->timestamp = gLevel.time + SECONDS_TO_MILLIS(ent->wait);

  const uint32_t delay = (uint32_t) SECONDS_TO_MILLIS(ent->delay) + G_ballistics_Prefire(ent);

  if (delay) {
    ent->nextThink = gLevel.time + (uint32_t) Maxi((int32_t) delay, QUETOO_TICK_MILLIS);
  } else {
    const GameBallisticsType *type = ent->ballistics;

    G_ballistics_Fire(ent, ent, G_ballistics_Dir(ent), type->ballisticsMod);
  }
}

/**
 * @brief Handles use activation of a turret, firing along the activator's view and crediting them
 * with any damage.
 */
static void G_turret_Use(GameEntity *ent, GameEntity *other, GameEntity *activator) {

  const GameBallisticsType *type = ent->ballistics;

  // "wait" gates shots; a sustained beam must be refreshed far more often than that or it
  // expires between uses, and its damage interval is enforced by the beam itself
  if (!type->sustained) {

    if (ent->timestamp > gLevel.time) {
      return;
    }

    ent->timestamp = gLevel.time + SECONDS_TO_MILLIS(ent->wait);
  }

  const uint32_t prefire = G_ballistics_Prefire(ent);

  if (activator && activator->client) {
    ent->s.client = activator->s.client;

    if (prefire) {
      // hold the operator, and aim where they are looking when it goes off
      ent->activator = activator;
      ent->nextThink = gLevel.time + prefire;
    } else {
      G_ballistics_Fire(ent, activator, activator->client->forward, type->turretMod);
    }
  } else {
    ent->s.client = MAX_CLIENTS;

    if (prefire) {
      ent->activator = NULL;
      ent->nextThink = gLevel.time + prefire;
    } else {
      G_ballistics_Fire(ent, ent, ent->moveDir, type->ballisticsMod);
    }
  }
}

/**
 * @brief Common initialization for both roles, applying the balance defaults the mapper did not
 * override.
 */
static void G_ballistics_Init(GameEntity *ent, const GameBallisticsType *type) {

  ent->ballistics = type;

  if (ent->damage == 0 && type->damage) {
    ent->damage = (*type->damage)->integer;
  }

  if (ent->speed == 0.f) {
    ent->speed = type->speed ? (*type->speed)->integer : type->defaultSpeed;
  }

  if (ent->damage == 0 && type->damage == NULL) {
    ent->damage = type->defaultDamage;
  }

  if (type->radius) {
    ent->damageRadius = (*type->radius)->value;
  }

  ent->knockback = gi.EntityValue(ent->def, "knockback")->integer;
  if (ent->knockback == 0 && type->knockback) {
    ent->knockback = (*type->knockback)->integer;
  }

  ent->accel = Clampf(gi.EntityValue(ent->def, "spread")->value, 0.f, 1.f);

  // an entity that asks for no interval fires at the weapon's own rate, so that a turret behaves
  // exactly like the weapon it mounts; one that does is honoured, down to the type's floor
  if (!(gi.EntityValue(ent->def, "wait")->parsed & ENTITY_FLOAT)) {
    ent->wait = type->refire ? (*type->refire)->value : MILLIS_TO_SECONDS(type->defaultWait);
  }

  ent->wait = Maxf(ent->wait, MILLIS_TO_SECONDS(type->minWait));
  ent->random = Maxf(ent->random, 0.f);
  ent->delay = Maxf(ent->delay, 0.f);

  ent->count = 0;
  ent->s.client = MAX_CLIENTS;

  if (type->sustained) {
    Vec3 color = gi.EntityValue(ent->def, "color")->vec3;
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
bool G_ballistics(GameEntity *ent) {

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

  const GameBallisticsType *type = G_ballistics_Type(name);
  if (type == NULL) {
    G_Warn("%s has no such projectile \"%s\"\n", etos(ent), name);
    G_FreeEntity(ent);
    return true;
  }

  G_ballistics_Init(ent, type);

  if (turret) {
    ent->Use = G_turret_Use;
    ent->Think = G_ballistics_Think;
  } else {
    ent->Use = G_ballistics_Use;
    ent->Think = G_ballistics_Think;

    if (ent->spawnFlags & BALLISTICS_START_ON) {
      ent->count = 1;
      ent->nextThink = gLevel.time + RandomRangeu(1, 1000);
    }
  }

  return true;
}
