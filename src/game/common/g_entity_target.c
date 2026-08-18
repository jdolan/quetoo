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
#define BALLISTICS_BLASTER  0x4
#define BALLISTICS_NAIL     0x8
#define BALLISTICS_ROCKET   0x10
#define BALLISTICS_GRENADE  0x20
#define BALLISTICS_LASER    0x40
#define BALLISTICS_GIBLETS  0x80

#define BALLISTICS_PROJECTILE (BALLISTICS_BLASTER | BALLISTICS_NAIL | BALLISTICS_ROCKET | \
                               BALLISTICS_GRENADE | BALLISTICS_LASER | BALLISTICS_GIBLETS)

/**
 * @brief Fires a blaster bolt from a `target_ballistics` or `target_turret`.
 */
static void G_target_ballistics_Blaster(g_entity_t *ent, g_entity_t *attacker, const vec3_t start, const vec3_t dir, uint32_t mod) {
  G_BlasterProjectile(ent, attacker, start, dir, ent->speed, ent->damage, ent->knockback, mod);
}

/**
 * @brief Fires a spike from a `target_ballistics` or `target_turret`.
 */
static void G_target_ballistics_Nail(g_entity_t *ent, g_entity_t *attacker, const vec3_t start, const vec3_t dir, uint32_t mod) {
  G_NailProjectile(ent, attacker, start, dir, ent->speed, ent->damage, ent->knockback, mod);
}

/**
 * @brief Fires a rocket from a `target_ballistics` or `target_turret`.
 */
static void G_target_ballistics_Rocket(g_entity_t *ent, g_entity_t *attacker, const vec3_t start, const vec3_t dir, uint32_t mod) {
  G_RocketProjectile(ent, attacker, start, dir, ent->speed, ent->damage, ent->knockback, ent->damage_radius, mod);
}

/**
 * @brief Fires a grenade from a `target_ballistics` or `target_turret`.
 */
static void G_target_ballistics_Grenade(g_entity_t *ent, g_entity_t *attacker, const vec3_t start, const vec3_t dir, uint32_t mod) {
  G_GrenadeProjectile(ent, attacker, start, dir, ent->speed, ent->damage, ent->knockback,
    ent->damage_radius, SECONDS_TO_MILLIS(g_balance_grenadelauncher_timer->value), mod);
}

/**
 * @brief Fires an instant traced beam from a `target_ballistics` or `target_turret`.
 */
static void G_target_ballistics_Laser(g_entity_t *ent, g_entity_t *attacker, const vec3_t start, const vec3_t dir, uint32_t mod) {
  G_RailgunProjectile(ent, attacker, start, dir, ent->damage, ent->knockback, mod);
}

/**
 * @brief Scatters giblets from a `target_ballistics` or `target_turret`.
 */
static void G_target_ballistics_Giblets(g_entity_t *ent, g_entity_t *attacker, const vec3_t start, const vec3_t dir, uint32_t mod) {

  G_Giblets(&(const g_giblets_t) {
    .origin = start,
    .velocity = Vec3_Scale(dir, ent->speed),
    .count = RandomRangei(2, 5),
    .head = true,
    .damage = ent->damage,
    .attacker = attacker,
    .mod = mod
  });
}

typedef struct {
  uint32_t spawn_flag;
  void (*Fire)(g_entity_t *ent, g_entity_t *attacker, const vec3_t start, const vec3_t dir, uint32_t mod);
  g_muzzle_flash_t flash;
  uint32_t trap_mod;
  uint32_t turret_mod;
  cvar_t **damage;
  cvar_t **knockback;
  cvar_t **speed;
  cvar_t **radius;
  int32_t default_damage;
  int32_t default_speed;
} g_ballistics_type_t;

static const g_ballistics_type_t g_ballistics_types[] = {
  {
    .spawn_flag = BALLISTICS_BLASTER,
    .Fire = G_target_ballistics_Blaster,
    .flash = MZ_BLASTER,
    .trap_mod = MOD_TRAP_BLASTER,
    .turret_mod = MOD_TURRET_BLASTER,
    .damage = &g_balance_blaster_damage,
    .knockback = &g_balance_blaster_knockback,
    .speed = &g_balance_blaster_speed,
  }, {
    .spawn_flag = BALLISTICS_NAIL,
    .Fire = G_target_ballistics_Nail,
    .flash = MZ_QUAKE_NAILGUN,
    .trap_mod = MOD_TRAP_NAIL,
    .turret_mod = MOD_TURRET_NAIL,
    .damage = &g_balance_quake_nailgun_damage,
    .knockback = &g_balance_quake_nailgun_knockback,
    .speed = &g_balance_quake_nailgun_speed,
  }, {
    .spawn_flag = BALLISTICS_ROCKET,
    .Fire = G_target_ballistics_Rocket,
    .flash = MZ_ROCKET_LAUNCHER,
    .trap_mod = MOD_TRAP_ROCKET,
    .turret_mod = MOD_TURRET_ROCKET,
    .damage = &g_balance_rocketlauncher_damage,
    .knockback = &g_balance_rocketlauncher_knockback,
    .speed = &g_balance_rocketlauncher_speed,
    .radius = &g_balance_rocketlauncher_radius,
  }, {
    .spawn_flag = BALLISTICS_GRENADE,
    .Fire = G_target_ballistics_Grenade,
    .flash = MZ_GRENADE_LAUNCHER,
    .trap_mod = MOD_TRAP_GRENADE,
    .turret_mod = MOD_TURRET_GRENADE,
    .damage = &g_balance_grenadelauncher_damage,
    .knockback = &g_balance_grenadelauncher_knockback,
    .speed = &g_balance_grenadelauncher_speed,
    .radius = &g_balance_grenadelauncher_radius,
  }, {
    .spawn_flag = BALLISTICS_LASER,
    .Fire = G_target_ballistics_Laser,
    .flash = MZ_RAILGUN,
    .trap_mod = MOD_TRAP_LASER,
    .turret_mod = MOD_TURRET_LASER,
    .damage = &g_balance_railgun_damage,
    .knockback = &g_balance_railgun_knockback,
  }, {
    .spawn_flag = BALLISTICS_GIBLETS,
    .Fire = G_target_ballistics_Giblets,
    .flash = MZ_LOGOUT,
    .trap_mod = MOD_TRAP_GIBLETS,
    .turret_mod = MOD_TURRET_GIBLETS,
    .default_damage = 10,
    .default_speed = 500,
  }
};

/**
 * @brief Resolves the projectile the entity was configured to fire, defaulting to the blaster.
 */
static const g_ballistics_type_t *G_target_ballistics_Type(const g_entity_t *ent) {

  for (size_t i = 0; i < lengthof(g_ballistics_types); i++) {
    if (ent->spawn_flags & g_ballistics_types[i].spawn_flag) {
      return &g_ballistics_types[i];
    }
  }

  return &g_ballistics_types[0];
}

/**
 * @brief Emits a single projectile along the given direction, with the muzzle pushed clear of the
 * brushwork the entity is typically embedded in.
 */
static void G_target_ballistics_Fire(g_entity_t *ent, g_entity_t *attacker, const vec3_t dir, uint32_t mod) {

  const g_ballistics_type_t *type = G_target_ballistics_Type(ent);

  const vec3_t aim = Vec3_RandomizeDir(dir, ent->accel);
  const vec3_t start = Vec3_Fmaf(ent->s.origin, 8.f, aim);

  type->Fire(ent, attacker, start, aim, mod);

  G_WorldMuzzleFlash(start, aim, type->flash);
}

/**
 * @brief Resolves the direction a trap fires in, tracking its target entity if it has one.
 */
static vec3_t G_target_ballistics_Dir(g_entity_t *ent) {

  if (ent->target && ent->enemy == NULL) {
    ent->enemy = G_PickTarget(ent->target);

    if (ent->enemy == NULL) {
      G_Warn("%s has invalid target %s\n", etos(ent), ent->target);
      ent->target = NULL;
    }
  }

  if (ent->enemy) {
    return Vec3_Normalize(Vec3_Subtract(Box3_Center(ent->enemy->abs_bounds), ent->s.origin));
  }

  return ent->move_dir;
}

/**
 * @brief Think callback for a free-running trap; fires and re-arms itself.
 */
static void G_target_ballistics_Think(g_entity_t *ent) {

  G_target_ballistics_Fire(ent, ent, G_target_ballistics_Dir(ent), G_target_ballistics_Type(ent)->trap_mod);

  if (ent->count) {
    const float wait = ent->wait * 1000.f + ent->random * 1000.f * RandomRangef(-1.f, 1.f);
    ent->next_think = g_level.time + (uint32_t) Maxf(wait, QUETOO_TICK_MILLIS);
  } else {
    ent->next_think = 0;
  }
}

/**
 * @brief Handles use activation of a `target_ballistics`, toggling a free-running trap or firing a
 * single shot after an optional delay.
 */
static void G_target_ballistics_Use(g_entity_t *ent, g_entity_t *other, g_entity_t *activator) {

  if (ent->spawn_flags & BALLISTICS_TOGGLE) {

    ent->count = !ent->count;

    if (ent->count) {
      ent->next_think = g_level.time + (uint32_t) Maxf(ent->delay * 1000.f, QUETOO_TICK_MILLIS);
    } else {
      ent->next_think = 0;
    }

    return;
  }

  if (ent->timestamp > g_level.time) {
    return;
  }

  ent->timestamp = g_level.time + ent->wait * 1000.f;

  if (ent->delay) {
    ent->next_think = g_level.time + (uint32_t) Maxf(ent->delay * 1000.f, QUETOO_TICK_MILLIS);
  } else {
    G_target_ballistics_Fire(ent, ent, G_target_ballistics_Dir(ent), G_target_ballistics_Type(ent)->trap_mod);
  }
}

/**
 * @brief Handles use activation of a `target_turret`, firing along the activator's view and
 * crediting them with any damage.
 */
static void G_target_turret_Use(g_entity_t *ent, g_entity_t *other, g_entity_t *activator) {

  if (ent->timestamp > g_level.time) {
    return;
  }

  ent->timestamp = g_level.time + ent->wait * 1000.0;

  if (activator && activator->client) {
    ent->s.client = activator->s.client;
    G_target_ballistics_Fire(ent, activator, activator->client->forward, G_target_ballistics_Type(ent)->turret_mod);
  } else {
    ent->s.client = MAX_CLIENTS;
    G_target_ballistics_Fire(ent, ent, ent->move_dir, G_target_ballistics_Type(ent)->trap_mod);
  }
}

/**
 * @brief Common initialization for the ballistics entities, resolving the projectile and applying
 * the balance defaults the mapper did not override.
 */
static void G_target_ballistics_Init(g_entity_t *ent) {

  const uint32_t projectile = ent->spawn_flags & BALLISTICS_PROJECTILE;
  if (projectile & (projectile - 1)) {
    G_Warn("%s has more than one projectile flag set\n", etos(ent));
  }

  const g_ballistics_type_t *type = G_target_ballistics_Type(ent);

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

  ent->s.client = MAX_CLIENTS;

  G_SetMoveDir(ent);

  gi.LinkEntity(ent);
}

/*QUAKED target_ballistics (1 .3 .3) (-8 -8 -8) (8 8 8) start_on toggle blaster nail rocket grenade laser giblets
 Fires projectiles at an interval, or each time it is used. Use these to build wall traps,
 spike shooters and lasers.

 -------- Keys --------
 angles : The angles at which projectiles are fired. Use -1 for up, -2 for down.
 delay : The delay in seconds between being used and firing (default 0).
 dmg : The damage inflicted by each projectile (default varies by projectile).
 knockback : The knockback applied by each projectile (default varies by projectile).
 random : Random time variance in seconds added to "wait" (default 0).
 speed : The projectile speed in units per second (default varies by projectile).
 spread : The cone of fire, from 0.0 for pinpoint to 1.0 for wild (default 0).
 target : An entity to aim at, re-evaluated on every shot. Overrides "angles".
 targetname : The target name of this entity.
 wait : The interval in seconds between shots (default 1.0).

 -------- Spawn flags --------
 start_on : Fires on its own from level start, without being used.
 toggle : Using this entity toggles it on and off, rather than firing one shot.
 blaster : Fires blaster bolts. This is the default.
 nail : Fires spikes.
 rocket : Fires rockets.
 grenade : Fires grenades.
 laser : Fires an instant traced beam. Set "wait" to 0 for a continuous laser.
 giblets : Fires giblets.

 -------- Notes --------
 Place this entity in open air, in front of the brushwork that represents it, not embedded
 within it. Projectiles that would spawn inside a wall are pulled back to the entity origin.

 Set exactly one projectile flag. Traps do not spare whoever triggered them. For a version
 players can aim and take credit for, use target_turret.
*/
void G_target_ballistics(g_entity_t *ent) {

  G_target_ballistics_Init(ent);

  ent->Use = G_target_ballistics_Use;
  ent->Think = G_target_ballistics_Think;

  if (ent->spawn_flags & BALLISTICS_START_ON) {
    ent->count = 1;
    ent->next_think = g_level.time + RandomRangeu(1, 1000);
  }
}

/*QUAKED target_turret (1 .6 .2) (-8 -8 -8) (8 8 8) x x blaster nail rocket grenade laser giblets
 Fires a projectile along the view of whoever uses it, and credits them with the kill.
 Surround it with a trigger_multiple that targets it, and players who step in behind it
 will operate it.

 -------- Keys --------
 angles : The angles to fire at when used by something other than a player.
 dmg : The damage inflicted by each projectile (default varies by projectile).
 knockback : The knockback applied by each projectile (default varies by projectile).
 speed : The projectile speed in units per second (default varies by projectile).
 spread : The cone of fire, from 0.0 for pinpoint to 1.0 for wild (default 0).
 targetname : The target name of this entity.
 wait : The minimum interval in seconds between shots (default 1.0).

 -------- Spawn flags --------
 x
 x
 blaster : Fires blaster bolts. This is the default.
 nail : Fires spikes.
 rocket : Fires rockets.
 grenade : Fires grenades.
 laser : Fires an instant traced beam.
 giblets : Fires giblets.

 -------- Notes --------
 Projectiles leave the turret, not the player, but carry the player's colors and are
 credited to them. The muzzle flash uses the weapon's own colors.

 The operator cannot be struck by their own projectiles, but the rocket and grenade
 types still splash them, so leave room behind the turret.

 Give the trigger_multiple a short "wait" for sustained fire while the player stands in it.
*/
void G_target_turret(g_entity_t *ent) {

  G_target_ballistics_Init(ent);

  ent->Use = G_target_turret_Use;
}
