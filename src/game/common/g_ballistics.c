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

/**
 * @brief Adds a fraction of the player's velocity to the given projectile.
 */
static void G_PlayerProjectile(GameEntity *ent, const float scale) {

  if (ent->owner) {
    const float s = scale * g_playerProjectile->value;
    ent->velocity = Vec3_Fmaf(ent->velocity, s, ent->owner->velocity);
  } else {
    G_Debug("No owner for %s\n", etos(ent));
  }
}

/**
 * @brief Returns true if the specified entity takes damage.
 */
static bool G_TakesDamage(GameEntity *ent) {
  return (ent && ent->takeDamage);
}

/**
 * @brief Resolves an explosive projectile that spawns flush against a wall or
 * an enemy. Against world geometry the projectile is pulled back to the
 * shooter's origin so it detonates against the surface on its first move.
 * Against a point-blank enemy it is detonated immediately, rather than
 * suspending inside the entity with no valid trajectory (#867).
 * @return True if the projectile detonated and was freed, in which case the
 * caller must not link or otherwise touch it.
 */
static bool G_ImmediateImpact(GameEntity *ent, GameEntity *projectile) {

  const CmTrace tr = gi.Trace(ent->s.origin, projectile->s.origin, projectile->bounds,
                                 ent, CONTENTS_MASK_SOLID);

  if (tr.fraction == 1.0) {
    return false;
  }

  if (tr.ent != ent && G_TakesDamage(tr.ent)) {
    gi.LinkEntity(projectile);
    projectile->Touch(projectile, tr.ent, &tr);
    return true;
  }

  projectile->s.origin = ent->s.origin;
  return false;
}

/**
 * @brief Used to add generic bubble trails to shots.
 */
static void G_BubbleTrail(const Vec3 start, CmTrace *tr, float freq) {
  Vec3 dir, pos;

  if (Vec3_Equal(tr->end, start)) {
    return;
  }

  dir = Vec3_Subtract(tr->end, start);
  dir = Vec3_Normalize(dir);
  pos = Vec3_Fmaf(tr->end, -2.f, dir);

  if (gi.PointContents(pos) & CONTENTS_MASK_LIQUID) {
    tr->end = pos;
  } else {
    const CmTrace trace = gi.Trace(pos, start, Box3_Zero(), tr->ent, CONTENTS_MASK_LIQUID);
    tr->end = trace.end;
  }

  pos = Vec3_Add(start, tr->end);
  pos = Vec3_Scale(pos, .5f);

  gi.WriteByte(SV_CMD_TEMP_ENTITY);
  gi.WriteByte(TE_BUBBLES);
  gi.WritePosition(start);
  gi.WritePosition(tr->end);
  gi.WriteByte((uint8_t) freq);
  gi.Multicast(pos, MULTICAST_PHS);
}

/**
 * @brief Used to add tracer trails to bullet shots.
 */
static void G_Tracer(const Vec3 start, const Vec3 end) {
  Vec3 dir, mid;
  float len;

  dir = Vec3_Subtract(end, start);
  len = Vec3_Length(dir);

  if (len < 128.f) {
    return;
  }

  dir = Vec3_Normalize(dir);
  mid = Vec3_Fmaf(end, -len + (Randomf() * .05f * len), dir);

  gi.WriteByte(SV_CMD_TEMP_ENTITY);
  gi.WriteByte(TE_TRACER);
  gi.WritePosition(mid);
  gi.WritePosition(end);
  gi.Multicast(start, MULTICAST_PHS);
}

/**
 * @brief Used to add impact marks on surfaces hit by bullets.
 */
static void G_BulletImpact(const CmTrace *trace) {

  if (trace->surface & SURF_ALPHA_TEST) {
    return;
  }

  gi.WriteByte(SV_CMD_TEMP_ENTITY);
  gi.WriteByte(TE_BULLET);
  gi.WritePosition(trace->end);
  gi.WriteDir(trace->plane.normal);

  gi.Multicast(trace->end, MULTICAST_PHS);
}

/**
 * @brief Touch callback for the blaster projectile; deals energy damage and emits an impact effect.
 */
static void G_BlasterProjectile_Touch(GameEntity *ent, GameEntity *other, const CmTrace *trace) {

  if (other == ent->owner) {
    return;
  }

  if (other->solid < SOLID_DEAD) {
    return;
  }

  if (trace == NULL) {
    return;
  }

  if (!G_IsSky(trace)) {

    G_Damage(&(GameDamage) {
      .target = other,
      .inflictor = ent,
      .attacker = ent->owner,
      .dir = ent->velocity,
      .point = ent->s.origin,
      .normal = trace->plane.normal,
      .damage = ent->damage,
      .knockback = ent->knockback,
      .flags = DMG_ENERGY,
      .mod = ent->mod ?: MOD_BLASTER
    });

    if (G_IsStructural(trace)) {

      gi.WriteByte(SV_CMD_TEMP_ENTITY);
      gi.WriteByte(TE_BLASTER);
      gi.WritePosition(ent->s.origin);
      gi.WriteDir(trace->plane.normal);
      gi.WriteByte(ent->owner->s.client);
      gi.Multicast(ent->s.origin, MULTICAST_PHS);
    }
  }

  G_FreeEntity(ent);
}

/**
 * @brief Fires a blaster projectile from the specified entity in the given direction.
 * @param emitter The entity the projectile leaves, providing its origin and effect color.
 * @param attacker The entity credited with any damage the projectile inflicts.
 */
void G_BlasterProjectile(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t speed, int32_t damage, int32_t knockback, uint32_t mod) {

  const Box3 bounds = Box3f(2.f, 2.f, 2.f);

  GameEntity *projectile = G_AllocEntity(__func__);
  projectile->owner = attacker;
  projectile->mod = mod;

  projectile->s.origin = start;
  projectile->bounds = bounds;
  projectile->s.angles = Vec3_Euler(dir);
  projectile->velocity = Vec3_Scale(dir, speed);

  if (G_ImmediateWall(emitter, projectile)) {
    projectile->s.origin = emitter->s.origin;
  }

  projectile->solid = SOLID_PROJECTILE;
  projectile->clipMask = CONTENTS_MASK_CLIP_PROJECTILE;
  projectile->damage = damage;
  projectile->knockback = knockback;
  projectile->moveType = MOVE_TYPE_FLY;
  projectile->nextThink = gLevel.time + 8000;
  projectile->Think = G_FreeEntity;
  projectile->Touch = G_BlasterProjectile_Touch;
  projectile->s.client = emitter->s.client;
  projectile->s.trail = TRAIL_BLASTER;

  gi.LinkEntity(projectile);
}

/**
 * @brief Touch callback for nail projectiles.
 */
static void G_NailProjectile_Touch(GameEntity *ent, GameEntity *other, const CmTrace *trace) {

  if (other == ent->owner) {
    return;
  }

  if (other->solid < SOLID_DEAD) {
    return;
  }

  if (trace == NULL) {
    return;
  }

  if (!G_IsSky(trace)) {

    G_Damage(&(GameDamage) {
      .target = other,
      .inflictor = ent,
      .attacker = ent->owner,
      .dir = ent->velocity,
      .point = ent->s.origin,
      .normal = trace->plane.normal,
      .damage = ent->damage,
      .knockback = ent->knockback,
      .mod = ent->mod ?: MOD_QUAKE_NAILGUN
    });

    if (G_IsStructural(trace)) {
      gi.WriteByte(SV_CMD_TEMP_ENTITY);
      gi.WriteByte(TE_NAIL);
      gi.WritePosition(ent->s.origin);
      gi.WriteDir(trace->plane.normal);
      gi.Multicast(ent->s.origin, MULTICAST_PHS);
    }
  }

  G_FreeEntity(ent);
}

/**
 * @brief Fires a nail projectile from the specified entity in the given direction.
 * @param emitter The entity the projectile leaves, providing its origin and effect color.
 * @param attacker The entity credited with any damage the projectile inflicts.
 */
void G_NailProjectile(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t speed, int32_t damage, int32_t knockback, uint32_t mod) {

  const Box3 bounds = Box3f(1.f, 1.f, 1.f);

  GameEntity *projectile = G_AllocEntity(__func__);
  projectile->owner = attacker;
  projectile->mod = mod;

  projectile->s.origin = start;
  projectile->bounds = bounds;
  projectile->s.angles = Vec3_Euler(dir);
  projectile->velocity = Vec3_Scale(dir, speed);

  if (G_ImmediateWall(emitter, projectile)) {
    projectile->s.origin = emitter->s.origin;
  }

  projectile->solid = SOLID_PROJECTILE;
  projectile->clipMask = CONTENTS_MASK_CLIP_PROJECTILE;
  projectile->damage = damage;
  projectile->knockback = knockback;
  projectile->moveType = MOVE_TYPE_FLY;
  projectile->nextThink = gLevel.time + 8000;
  projectile->Think = G_FreeEntity;
  projectile->Touch = G_NailProjectile_Touch;
  projectile->s.client = emitter->s.client;
  projectile->s.model1 = gMedia.models.quakeNail;
  projectile->s.trail = TRAIL_QUAKE_NAIL;

  gi.LinkEntity(projectile);
}

/**
 * @brief Fires a single bullet projectile with randomized spread, dealing damage and emitting impact effects.
 */
void G_BulletProjectile(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t damage, int32_t knockback, int32_t hspread, int32_t vspread, int32_t mod) {

  CmTrace tr = gi.Trace(emitter->s.origin, start, Box3f(1.f, 1.f, 1.f), emitter, CONTENTS_MASK_CLIP_PROJECTILE);
  if (tr.fraction == 1.0) {
    Vec3 angles, forward, right, up, end;

    angles = Vec3_Euler(dir);
    Vec3_Vectors(angles, &forward, &right, &up);

    end = Vec3_Fmaf(start, MAX_WORLD_DIST, forward);
    end = Vec3_Fmaf(end, RandomRangef(-hspread, hspread), right);
    end = Vec3_Fmaf(end, RandomRangef(-vspread, vspread), up);

    tr = gi.Trace(start, end, Box3_Zero(), emitter, CONTENTS_MASK_CLIP_PROJECTILE);

    G_Tracer(start, tr.end);
  }

  if (tr.fraction < 1.0) {

    G_Damage(&(GameDamage) {
      .target = tr.ent,
      .inflictor = emitter,
      .attacker = attacker,
      .dir = dir,
      .point = tr.end,
      .normal = tr.plane.normal,
      .damage = damage,
      .knockback = knockback,
      .flags = DMG_BULLET,
      .mod = mod
    });

    if (G_IsStructural(&tr)) {
      G_BulletImpact(&tr);
    }

    if (gi.PointContents(start) & CONTENTS_MASK_LIQUID) {
      G_Ripple(NULL, tr.end, start, 8.f, false);
      G_BubbleTrail(start, &tr, 12.f);
    } else if (gi.PointContents(tr.end) & CONTENTS_MASK_LIQUID) {
      G_Ripple(NULL, start, tr.end, 8.f, true);
      G_BubbleTrail(start, &tr, 12.f);
    }
  }
}

/**
 * @brief Fires multiple bullet projectiles to simulate shotgun pellet spread.
 */
void G_ShotgunProjectiles(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t damage, int32_t knockback, int32_t hspread, int32_t vspread, int32_t count, int32_t mod) {

  for (int32_t i = 0; i < count; i++) {
    G_BulletProjectile(emitter, attacker, start, dir, damage, knockback, hspread, vspread, mod);
  }
}

#define HAND_GRENADE 1
#define HAND_GRENADE_HELD 2
#define QUAKE_GRENADE 4

/**
 * @brief Detonates a grenade projectile, dealing direct and splash radius damage.
 */
static void G_GrenadeProjectile_Explode(GameEntity *ent) {
  uint32_t mod;

  if (ent->enemy) { // direct hit

    if (ent->mod) {
      mod = ent->mod;
    } else if (ent->spawnFlags & HAND_GRENADE) {
      mod = MOD_HANDGRENADE;
    } else if (ent->spawnFlags & QUAKE_GRENADE) {
      mod = MOD_QUAKE_GRENADE;
    } else {
      mod = MOD_GRENADE;
    }

    G_Damage(&(GameDamage) {
      .target = ent->enemy,
      .inflictor = ent,
      .attacker = ent->owner,
      .dir = ent->velocity,
      .point = ent->s.origin,
      .normal = Vec3_Negate(ent->velocity),
      .damage = ent->damage,
      .knockback = ent->knockback,
      .flags = 0,
      .mod = mod
    });
  }

  if (ent->mod) {
    mod = ent->mod;
  } else if (ent->spawnFlags & HAND_GRENADE) {
    if (ent->spawnFlags & HAND_GRENADE_HELD) {
      mod = MOD_HANDGRENADE_KAMIKAZE;
    } else {
      mod = MOD_HANDGRENADE_SPLASH;
    }
  } else if (ent->spawnFlags & QUAKE_GRENADE) {
    mod = MOD_QUAKE_GRENADE_SPLASH;
  } else {
    mod = MOD_GRENADE_SPLASH;
  }

  // hurt anything else nearby
  G_RadiusDamage(ent, ent->owner, ent->enemy, ent->damage, ent->knockback, ent->damageRadius, mod);

  gi.WriteByte(SV_CMD_TEMP_ENTITY);
  gi.WriteByte(TE_EXPLOSION);
  gi.WritePosition(ent->s.origin);
  gi.WriteDir(Vec3_Up());
  gi.Multicast(ent->s.origin, MULTICAST_PHS);

  G_FreeEntity(ent);
}

/**
 * @brief Touch callback for the grenade projectile; bounces off structures or explodes on contact with damageable entities.
 */
void G_GrenadeProjectile_Touch(GameEntity *ent, GameEntity *other, const CmTrace *trace) {

  if (other == ent->owner) {
    return;
  }

  if (other->solid < SOLID_DEAD) {
    return;
  }

  if (trace == NULL) {
    return;
  }

  if (!G_TakesDamage(other)) { // bounce off of structural solids

    if (G_IsStructural(trace)) {
      if (gLevel.time - ent->touchTime > 200) {
        if (Vec3_Length(ent->velocity) > 40.0) {
          G_MulticastSound(&(const GamePlaySound) {
            .index = ent->hitSound,
            .entity = ent,
            .pitch = (int8_t) (Randomf() * 5.0)
          }, MULTICAST_PHS);
          ent->touchTime = gLevel.time;
        }
      }
    } else if (G_IsSky(trace)) {
      G_FreeEntity(ent);
    }

    return;
  }
  ent->enemy = other;
  G_GrenadeProjectile_Explode(ent);
}

static void G_QuakeGrenadeProjectile_Touch(GameEntity *ent, GameEntity *other, const CmTrace *trace) {

  if (other == ent->owner) {
    return;
  }

  if (other->solid < SOLID_DEAD) {
    return;
  }

  if (trace == NULL) {
    return;
  }

  if (!G_TakesDamage(other)) { // bounce off of structural solids

    if (G_IsStructural(trace)) {
      if (gLevel.time - ent->touchTime > 200) {
        if (Vec3_Length(ent->velocity) > 40.0) {
          G_MulticastSound(&(const GamePlaySound) {
            .index = ent->hitSound,
            .entity = ent,
          }, MULTICAST_PHS);
          ent->touchTime = gLevel.time;
        }
      }
    } else if (G_IsSky(trace)) {
      G_FreeEntity(ent);
    }

    return;
  }
  ent->enemy = other;
  G_GrenadeProjectile_Explode(ent);
}

/**
 * @brief Fires a grenade projectile with bounce physics and a timed fuse.
 * @param emitter The entity the projectile leaves, providing its origin and effect color.
 * @param attacker The entity credited with any damage the projectile inflicts.
 */
void G_GrenadeProjectile(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t speed, int32_t damage, int32_t knockback, float damageRadius, uint32_t timer, uint32_t mod) {

  const Box3 bounds = Box3f(6.f, 6.f, 6.f);

  Vec3 forward, right, up;

  GameEntity *projectile = G_AllocEntity(__func__);
  projectile->owner = attacker;
  projectile->mod = mod;

  projectile->s.origin = start;
  projectile->bounds = bounds;
  projectile->s.angles = Vec3_Euler(dir);

  Vec3_Vectors(projectile->s.angles, &forward, &right, &up);
  projectile->velocity = Vec3_Scale(dir, speed);

  projectile->velocity = Vec3_Fmaf(projectile->velocity, RandomRangef(90.f, 110.f), up);
  projectile->velocity = Vec3_Fmaf(projectile->velocity, RandomRangef(-10.f, 10.f), right);

  G_PlayerProjectile(projectile, 0.33);

  projectile->solid = SOLID_PROJECTILE;
  projectile->mass = 75.f;
  projectile->avelocity.x = RandomRangef(-310.f, -290.f);
  projectile->avelocity.y = RandomRangef(-50.f, 50.f);
  projectile->avelocity.z = RandomRangef(-25.f, 25.f);
  projectile->clipMask = CONTENTS_MASK_CLIP_PROJECTILE;
  projectile->damage = damage;
  projectile->damageRadius = damageRadius;
  projectile->knockback = knockback;
  projectile->moveType = MOVE_TYPE_BOUNCE;
  projectile->nextThink = gLevel.time + timer;
  projectile->takeDamage = true;
  projectile->Think = G_GrenadeProjectile_Explode;
  projectile->Touch = G_GrenadeProjectile_Touch;
  projectile->touchTime = gLevel.time;
  projectile->hitSound = gMedia.sounds.grenadeHit;
  projectile->s.trail = TRAIL_GRENADE;
  projectile->s.model1 = gMedia.models.grenade;

  if (G_ImmediateImpact(emitter, projectile)) {
    return;
  }

  gi.LinkEntity(projectile);
}

/**
 * @brief Fires a Quake grenade projectile with bounce physics and a timed fuse.
 */
void G_QuakeGrenadeProjectile(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t speed, int32_t damage, int32_t knockback, float damageRadius, uint32_t timer) {

  const Box3 bounds = Box3f(6.f, 6.f, 3.f);

  Vec3 forward, right, up;

  GameEntity *projectile = G_AllocEntity(__func__);
  projectile->owner = attacker;
  projectile->spawnFlags = QUAKE_GRENADE;

  projectile->s.origin = start;
  projectile->bounds = bounds;
  projectile->s.angles = Vec3_Euler(dir);

  Vec3_Vectors(projectile->s.angles, &forward, &right, &up);
  projectile->velocity = Vec3_Scale(dir, speed);

  projectile->velocity = Vec3_Fmaf(projectile->velocity, RandomRangef(90.f, 110.f), up);
  projectile->velocity = Vec3_Fmaf(projectile->velocity, RandomRangef(-10.f, 10.f), right);

  G_PlayerProjectile(projectile, 0.33);

  if (G_ImmediateWall(emitter, projectile)) {
    projectile->s.origin = emitter->s.origin;
  }

  projectile->solid = SOLID_PROJECTILE;
  projectile->mass = 75.f;
  projectile->avelocity.x = RandomRangef(-310.f, -290.f);
  projectile->avelocity.y = RandomRangef(-50.f, 50.f);
  projectile->avelocity.z = RandomRangef(-25.f, 25.f);
  projectile->clipMask = CONTENTS_MASK_CLIP_PROJECTILE;
  projectile->damage = damage;
  projectile->damageRadius = damageRadius;
  projectile->knockback = knockback;
  projectile->moveType = MOVE_TYPE_BOUNCE;
  projectile->nextThink = gLevel.time + timer;
  projectile->takeDamage = true;
  projectile->Think = G_GrenadeProjectile_Explode;
  projectile->Touch = G_QuakeGrenadeProjectile_Touch;
  projectile->touchTime = gLevel.time;
  projectile->hitSound = gMedia.sounds.quakeGrenadeHit;
  projectile->s.trail = TRAIL_QUAKE_GRENADE;
  projectile->s.model1 = gMedia.models.quakeGrenade;
}

// tossing a hand grenade
void G_HandGrenadeProjectile(GameEntity *ent, GameEntity *projectile, Vec3 const start, const Vec3 dir, int32_t speed, int32_t damage, int32_t knockback, float damageRadius, uint32_t timer) {

  const Box3 bounds = Box3f(4.f, 4.f, 4.f);

  Vec3 forward, right, up;

  projectile->s.origin = start;
  projectile->bounds = bounds;
  projectile->s.angles = Vec3_Euler(dir);

  Vec3_Vectors(projectile->s.angles, &forward, &right, &up);
  projectile->velocity = Vec3_Scale(dir, speed);

  projectile->velocity = Vec3_Fmaf(projectile->velocity, RandomRangef(190.f, 210.f), up);
  projectile->velocity = Vec3_Fmaf(projectile->velocity, RandomRangef(-10.f, 10.f), right);

  // add some of the player's velocity to the projectile
  G_PlayerProjectile(projectile, 0.33);

  projectile->spawnFlags = HAND_GRENADE;

  // if client is holding it, let the nade know it's being held
  if (ent->client->grenadeHoldTime) {
    projectile->spawnFlags |= HAND_GRENADE_HELD;
  }

  projectile->mass = 75.f;
  projectile->avelocity.x = RandomRangef(-310.f, -290.f);
  projectile->avelocity.y = RandomRangef(-50.f, 50.f);
  projectile->avelocity.z = RandomRangef(-25.f, 25.f);
  projectile->damage = damage;
  projectile->damageRadius = damageRadius;
  projectile->knockback = knockback;
  projectile->nextThink = gLevel.time + timer;
  projectile->solid = SOLID_PROJECTILE;
  projectile->svFlags &= ~SVF_NO_CLIENT;
  projectile->moveType = MOVE_TYPE_BOUNCE;
  projectile->Think = G_GrenadeProjectile_Explode;
  projectile->Touch = G_GrenadeProjectile_Touch;
  projectile->hitSound = gMedia.sounds.grenadeHit;
  projectile->s.sound = 0;

  if (G_ImmediateImpact(ent, projectile)) {
    return;
  }

  gi.LinkEntity(projectile);
}

/**
 * @brief Touch callback for the rocket projectile; deals direct and radius explosion damage on impact.
 */
#define QUAKE_ROCKET 1

static void G_RocketProjectile_Touch(GameEntity *ent, GameEntity *other, const CmTrace *trace) {
  const bool quakeRocket = (ent->spawnFlags & QUAKE_ROCKET) != 0;
  const uint32_t directMod = ent->mod ?: (quakeRocket ? MOD_QUAKE_ROCKET : MOD_ROCKET);
  const uint32_t splashMod = ent->mod ?: (quakeRocket ? MOD_QUAKE_ROCKET_SPLASH : MOD_ROCKET_SPLASH);

  if (other == ent->owner) {
    return;
  }

  if (other->solid < SOLID_DEAD) {
    return;
  }

  if (trace == NULL) {
    return;
  }

  if (!G_IsSky(trace)) {

    if (G_IsStructural(trace) || G_IsMeat(other)) {

      G_Damage(&(GameDamage) {
        .target = other,
        .inflictor = ent,
        .attacker = ent->owner,
        .dir = ent->velocity,
        .point = ent->s.origin,
        .normal = trace->plane.normal,
        .damage = ent->damage,
        .knockback = ent->knockback,
        .flags = 0,
        .mod = directMod
      });

      G_RadiusDamage(ent, ent->owner, other, ent->damage, ent->knockback, ent->damageRadius, splashMod);

      gi.WriteByte(SV_CMD_TEMP_ENTITY);
      gi.WriteByte(TE_EXPLOSION);
      gi.WritePosition(ent->s.origin);
      gi.WriteDir(trace->plane.normal);
      gi.Multicast(ent->s.origin, MULTICAST_PHS);
    }
  }

  G_FreeEntity(ent);
}

/**
 * @brief Fires a rocket projectile that explodes with radius damage on impact.
 * @param emitter The entity the projectile leaves, providing its origin and effect color.
 * @param attacker The entity credited with any damage the projectile inflicts.
 */
void G_RocketProjectile(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t speed, int32_t damage, int32_t knockback, float damageRadius, uint32_t mod) {

  const Box3 bounds = Box3f(8.f, 8.f, 8.f);

  GameEntity *projectile = G_AllocEntity(__func__);
  projectile->owner = attacker;
  projectile->mod = mod;

  projectile->s.origin = start;
  projectile->bounds = bounds;
  projectile->s.angles = Vec3_Euler(dir);
  projectile->velocity = Vec3_Scale(dir, speed);
  projectile->avelocity = MakeVec3(0.0, 0.0, 600.0);

  projectile->solid = SOLID_PROJECTILE;
  projectile->clipMask = CONTENTS_MASK_CLIP_PROJECTILE;
  projectile->damage = damage;
  projectile->damageRadius = damageRadius;
  projectile->knockback = knockback;
  projectile->rippleSize = 32.0;
  projectile->moveType = MOVE_TYPE_FLY;
  projectile->nextThink = gLevel.time + 8000;
  projectile->Think = G_FreeEntity;
  projectile->Touch = G_RocketProjectile_Touch;
  projectile->s.model1 = gMedia.models.rocket;
  projectile->s.sound = gMedia.sounds.rocketFly;
  projectile->s.trail = TRAIL_ROCKET;

  if (G_ImmediateImpact(emitter, projectile)) {
    return;
  }

  gi.LinkEntity(projectile);
}

/**
 * @brief Fires a Quake rocket projectile that explodes with radius damage on impact.
 */
void G_QuakeRocketProjectile(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t speed, int32_t damage, int32_t knockback, float damageRadius) {

  const Box3 bounds = Box3f(8.f, 8.f, 8.f);

  GameEntity *projectile = G_AllocEntity(__func__);
  projectile->owner = attacker;
  projectile->spawnFlags = QUAKE_ROCKET;

  projectile->s.origin = start;
  projectile->bounds = bounds;
  projectile->s.angles = Vec3_Euler(dir);
  projectile->velocity = Vec3_Scale(dir, speed);
  projectile->avelocity = Vec3_Zero();

  projectile->solid = SOLID_PROJECTILE;
  projectile->clipMask = CONTENTS_MASK_CLIP_PROJECTILE;
  projectile->damage = damage;
  projectile->damageRadius = damageRadius;
  projectile->knockback = knockback;
  projectile->rippleSize = 32.0;
  projectile->moveType = MOVE_TYPE_FLY;
  projectile->nextThink = gLevel.time + 8000;
  projectile->Think = G_FreeEntity;
  projectile->Touch = G_RocketProjectile_Touch;
  projectile->s.model1 = gMedia.models.quakeRocket;
  projectile->s.sound = gMedia.sounds.rocketFly;
  projectile->s.trail = TRAIL_ROCKET;

  if (G_ImmediateImpact(emitter, projectile)) {
    return;
  }

  gi.LinkEntity(projectile);
}

/**
 * @brief Touch callback for the hyperblaster projectile; deals energy damage and handles hyperblaster climb mechanics.
 */
static void G_HyperblasterProjectile_Touch(GameEntity *ent, GameEntity *other, const CmTrace *trace) {

  if (other == ent->owner) {
    return;
  }

  if (other->solid < SOLID_DEAD) {
    return;
  }

  if (trace == NULL) {
    return;
  }

  if (!G_IsSky(trace)) {

    if (G_IsStructural(trace) || G_IsMeat(other)) {

      G_Damage(&(GameDamage) {
        .target = other,
        .inflictor = ent,
        .attacker = ent->owner,
        .dir = ent->velocity,
        .point = ent->s.origin,
        .normal = trace->plane.normal,
        .damage = ent->damage,
        .knockback = ent->knockback,
        .flags = DMG_ENERGY,
        .mod = MOD_HYPERBLASTER
      });

      if (G_IsStructural(trace)) {

        Vec3 v;
        v = Vec3_Subtract(ent->s.origin, ent->owner->s.origin);

        if (Vec3_Length(v) < 32.0) { // hyperblaster climbing
          G_Damage(&(GameDamage) {
            .target = ent->owner,
            .inflictor = ent,
            .attacker = ent->owner,
            .dir = Vec3_Zero(),
            .point = ent->s.origin,
            .normal = trace->plane.normal,
            .damage = g_balanceHyperblasterClimbDamage->integer,
            .knockback = 0,
            .flags = DMG_ENERGY,
            .mod = MOD_HYPERBLASTER_CLIMB
          });

          ent->owner->velocity.z += g_balanceHyperblasterClimbKnockback->value;
        }
      }

      gi.WriteByte(SV_CMD_TEMP_ENTITY);
      gi.WriteByte(TE_HYPERBLASTER);
      gi.WritePosition(ent->s.origin);
      gi.WriteDir(trace->plane.normal);
      gi.Multicast(ent->s.origin, MULTICAST_PHS);
    }
  }

  G_FreeEntity(ent);
}

/**
 * @brief Fires a hyperblaster energy projectile in the given direction.
 */
void G_HyperblasterProjectile(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t speed, int32_t damage, int32_t knockback) {

  const Box3 bounds = Box3f(6.f, 6.f, 6.f);

  GameEntity *projectile = G_AllocEntity(__func__);
  projectile->owner = attacker;

  projectile->s.origin = start;
  projectile->bounds = bounds;
  projectile->s.angles = Vec3_Euler(dir);
  projectile->velocity = Vec3_Scale(dir, speed);

  if (G_ImmediateWall(emitter, projectile)) {
    projectile->s.origin = emitter->s.origin;
  }

  projectile->solid = SOLID_PROJECTILE;
  projectile->clipMask = CONTENTS_MASK_CLIP_PROJECTILE;
  projectile->damage = damage;
  projectile->knockback = knockback;
  projectile->rippleSize = 22.0;
  projectile->moveType = MOVE_TYPE_FLY;
  projectile->nextThink = gLevel.time + 6000;
  projectile->Think = G_FreeEntity;
  projectile->Touch = G_HyperblasterProjectile_Touch;
  projectile->s.trail = TRAIL_HYPERBLASTER;

  gi.LinkEntity(projectile);
}

/**
 * @brief Discharges the lightning gun into water, killing the owner and dealing scaled damage to all nearby entities.
 */
static void G_LightningProjectile_Discharge(GameEntity *ent) {
  const g_means_of_death mod = ent->count ? ent->count : MOD_LIGHTNING_DISCHARGE;

  // kill ourselves
  G_Damage(&(GameDamage) {
    .target = ent->owner,
    .inflictor = ent,
    .attacker = ent->owner,
    .dir = Vec3_Zero(),
    .point = ent->s.origin,
    .normal = Vec3_Zero(),
    .damage = 9999,
    .knockback = 100,
    .flags = DMG_NO_ARMOR,
    .mod = mod
  });

  // and ruin the pool party for everyone else too
  G_ForEachEntity(other, {
    if (other == ent->owner) {
      continue;
    }

    if (!G_IsMeat(other)) {
      continue;
    }

    const float dist = Vec3_Distance(ent->s.origin, other->s.origin);
    const float atten = Clampf01(1.f - (dist / 1024.f));

    if (other->waterLevel > WATER_NONE) {
      const int32_t dmg = 50 * other->waterLevel * atten;

      G_Damage(&(GameDamage) {
        .target = other,
        .inflictor = ent,
        .attacker = ent->owner,
        .dir = Vec3_Zero(),
        .point = other->s.origin,
        .normal = Vec3_Zero(),
          .damage = dmg,
          .knockback = 100 * atten,
          .flags = DMG_NO_ARMOR,
          .mod = mod
        });
    }
  });

  // send discharge event
  gi.WriteByte(SV_CMD_TEMP_ENTITY);
  gi.WriteByte(TE_LIGHTNING_DISCHARGE);
  gi.WritePosition(ent->s.origin);
}

/**
 * @brief Returns true if the lightning projectile should expire due to timeout or owner death.
 */
static bool G_LightningProjectile_Expire(GameEntity *ent) {

  if (ent->timestamp < gLevel.time - (SECONDS_TO_MILLIS(g_balanceLightningRefire->value) + 1)) {
    return true;
  }

  if (ent->owner->dead) {
    return true;
  }

  return false;
}

/**
 * @brief Think callback for the lightning projectile; updates beam endpoints, handles water entry, and deals damage each tick.
 */
static void G_LightningProjectile_Think(GameEntity *ent) {
  Vec3 forward, right, up;
  Vec3 start, end;
  Vec3 waterStart;
  CmTrace tr;

  if (G_LightningProjectile_Expire(ent)) {
    G_FreeEntity(ent);
    return;
  }

  // re-calculate end points based on owner's movement
  G_ClientProjectile(ent->owner->client, &forward, &right, &up, &start, 1.0);
  ent->s.origin = start;

  if (G_ImmediateWall(ent->owner, ent)) { // resolve start
    start = ent->owner->s.origin;
  }

  if (gi.PointContents(start) & CONTENTS_MASK_LIQUID) { // discharge and return
    G_LightningProjectile_Discharge(ent);
    G_FreeEntity(ent);
    return;
  }

  end = Vec3_Fmaf(start, g_balanceLightningLength->value, forward); // resolve end
  end = Vec3_Fmaf(end, 2.f * sinf(gLevel.time / 4.f), up);
  end = Vec3_Fmaf(end, RandomRangef(-2.f, 2.f), right);

  tr = gi.Trace(start, end, Box3_Zero(), ent, CONTENTS_MASK_CLIP_PROJECTILE | CONTENTS_MASK_LIQUID);

  if (tr.contents & CONTENTS_MASK_LIQUID) { // entered water, play sound, leave trail
    waterStart = tr.end;

    if (!ent->waterLevel) {
      G_MulticastSound(&(const GamePlaySound) {
        .index = gMedia.sounds.waterIn,
        .origin = &waterStart,
      }, MULTICAST_PHS);
      ent->waterLevel = WATER_FEET;
    }

    tr = gi.Trace(waterStart, end, Box3_Zero(), ent, CONTENTS_MASK_CLIP_PROJECTILE);
    G_BubbleTrail(waterStart, &tr, 4.f);

    G_Ripple(NULL, start, end, 16.f, true);
  } else {
    if (ent->waterLevel) { // exited water, play sound, no trail
      G_MulticastSound(&(const GamePlaySound) {
        .index = gMedia.sounds.waterOut,
        .origin = &start,
      }, MULTICAST_PHS);
      ent->waterLevel = WATER_NONE;
    }
  }

  // clear the angles for impact effects
  ent->s.angles = Vec3_Zero();
  ent->s.animation1 = LIGHTNING_NO_HIT;

  if (ent->damage) { // shoot, removing our damage until it is renewed
    if (G_TakesDamage(tr.ent)) { // try to damage what we hit
      G_Damage(&(GameDamage) {
        .target = tr.ent,
        .inflictor = ent,
        .attacker = ent->owner,
        .dir = forward,
        .point = tr.end,
        .normal = tr.plane.normal,
        .damage = ent->damage,
        .knockback = ent->knockback,
        .flags = DMG_ENERGY,
        .mod = ent->mod ?: MOD_LIGHTNING
      });
      ent->damage = 0;
    } else { // or leave a mark
      if (tr.contents & CONTENTS_MASK_SOLID) {
        if (G_IsStructural(&tr)) {
          ent->s.angles = Vec3_Euler(tr.plane.normal);
          ent->s.animation1 = LIGHTNING_SOLID_HIT;
        }
      }
    }
  }

  ent->s.origin = start; // update end points
  ent->s.termination = tr.end;

  gi.LinkEntity(ent);

  ent->nextThink = gLevel.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief Creates or updates the lightning beam projectile entity for the given owner.
 */
void G_LightningProjectile(GameEntity *ent, const Vec3 start, const Vec3 dir, int32_t damage, int32_t knockback, int32_t mod, int32_t dischargeMod) {

  GameEntity *projectile = NULL;

  while ((projectile = G_Find(projectile, EOFS(classname), __func__))) {
    if (projectile->owner == ent) {
      break;
    }
  }

  if (!projectile) { // ensure a valid lightning entity exists
    projectile = G_AllocEntity(__func__);

    projectile->s.origin = start;

    if (G_ImmediateWall(ent, projectile)) {
      projectile->s.origin = ent->s.origin;
    }

    projectile->s.termination = Vec3_Fmaf(start, g_balanceLightningLength->value, dir);

    projectile->owner = ent;
    projectile->solid = SOLID_NOT;
    projectile->clipMask = CONTENTS_MASK_CLIP_PROJECTILE;
    projectile->moveType = MOVE_TYPE_THINK;
    projectile->Think = G_LightningProjectile_Think;
    projectile->knockback = knockback;
    projectile->s.client = ent->s.client;
    projectile->s.effects = EF_BEAM;
    projectile->s.sound = gMedia.sounds.lightningFly;
    projectile->s.trail = TRAIL_LIGHTNING;

    gi.LinkEntity(projectile);
  }

  // set the damage and think time
  projectile->damage = damage;
  projectile->nextThink = gLevel.time + 1;
  projectile->timestamp = gLevel.time;
  projectile->waterLevel = WATER_NONE;
  projectile->mod = mod;
  projectile->count = dischargeMod;
}

/**
 * @brief Locates the sustained beam belonging to the given emitter, if it has one.
 * @remarks Callers use this to tell striking a beam up from sustaining one, the two sounding
 * differently: it is struck up once and hums thereafter.
 */
GameEntity *G_FindBeamProjectile(const GameEntity *emitter) {

  GameEntity *projectile = NULL;

  while ((projectile = G_Find(projectile, EOFS(classname), "G_BeamProjectile"))) {
    if (projectile->owner == emitter) {
      return projectile;
    }
  }

  return NULL;
}

/**
 * @brief Re-traces the beam from its emitter and burns whatever it lands on.
 * @remarks This runs every tick, rather than only when the emitter refreshes the beam. An
 * operator holding a turret aims continuously, and a trigger_multiple only re-uses the turret as
 * often as its own wait allows, so a beam that took its direction from the refresh would lag the
 * operator's view badly. `touch_time` holds the deadline the emitter keeps pushing forward; the
 * beam frees itself once the emitter stops asking.
 */
static void G_BeamProjectile_Think(GameEntity *ent) {

  const GameEntity *emitter = ent->owner;

  if (gLevel.time >= ent->touchTime || !emitter->inUse) {
    G_FreeEntity(ent);
    return;
  }

  Vec3 dir = ent->moveDir;

  if (ent->activator && ent->activator->client) {
    dir = ent->activator->client->forward;
  }

  const Vec3 start = Vec3_Fmaf(emitter->s.origin, 8.f, dir);
  const Vec3 end = Vec3_Fmaf(start, MAX_WORLD_DIST, dir);

  const CmTrace tr = gi.Trace(start, end, Box3_Zero(), ent, CONTENTS_MASK_CLIP_PROJECTILE);

  if (gLevel.time >= ent->timestamp && G_TakesDamage(tr.ent)) {

    ent->timestamp = gLevel.time + (uint32_t) Maxf(SECONDS_TO_MILLIS(emitter->wait), QUETOO_TICK_MILLIS);

    G_Damage(&(GameDamage) {
      .target = tr.ent,
      .inflictor = ent,
      .attacker = ent->activator ?: ent->owner,
      .dir = dir,
      .point = tr.end,
      .normal = tr.plane.normal,
      .damage = ent->damage,
      .knockback = ent->knockback,
      .flags = DMG_ENERGY,
      .mod = ent->mod
    });
  }

  ent->s.origin = start;
  ent->s.termination = tr.end;

  gi.LinkEntity(ent);

  ent->nextThink = gLevel.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief Creates or refreshes the sustained beam belonging to the given emitter.
 * @param emitter The entity the beam emanates from, providing its color and damage interval.
 * @param attacker The entity credited with any damage the beam inflicts, and whose view aims it
 * when it is a player operating a turret.
 * @param trail The trail the client renders the beam with, which is what distinguishes a laser
 * from lightning; the two differ in appearance and in nothing else here.
 * @param sound The looping sound the beam carries while it exists, or zero for a silent one.
 * @remarks The beam is a persistent entity rather than an effect repeated per shot, so it costs a
 * delta only on the ticks its endpoint actually moves. Its own think does the work; this only
 * hands over the parameters and pushes the deadline out.
 * @remarks `G_LightningProjectile` cannot serve here: its think resolves the muzzle through
 * `ent->owner->client`, which a trap does not have.
 */
void G_BeamProjectile(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir,
             int32_t damage, int32_t knockback, uint32_t mod, uint8_t trail, uint16_t sound) {

  GameEntity *projectile = G_FindBeamProjectile(emitter);

  if (!projectile) {
    projectile = G_AllocEntity(__func__);

    projectile->owner = emitter;
    projectile->solid = SOLID_NOT;
    projectile->clipMask = CONTENTS_MASK_CLIP_PROJECTILE;
    projectile->moveType = MOVE_TYPE_THINK;
    projectile->Think = G_BeamProjectile_Think;
    projectile->s.effects = EF_BEAM;
    projectile->s.trail = trail;
    projectile->s.sound = sound;
    projectile->s.color = emitter->s.color;

    // the beam leaves its emitter, never a player, so it must not be attributed to one;
    // Cg_EntityTrail anchors an owned beam to that client's muzzle
    projectile->s.client = MAX_CLIENTS;

    projectile->timestamp = 0;
  }

  projectile->activator = attacker && attacker->client ? attacker : NULL;
  projectile->moveDir = dir;
  projectile->damage = damage;
  projectile->knockback = knockback;
  projectile->mod = mod;

  // the deadline the emitter keeps pushing out; it must comfortably exceed the interval its
  // refresher runs at, a trap being every tick but a turret only as often as its trigger fires
  projectile->touchTime = gLevel.time + 500;

  G_BeamProjectile_Think(projectile);
}

/**
 * @brief Frees the sustained beam belonging to the given emitter, if it has one.
 */
void G_FreeBeamProjectile(GameEntity *emitter) {

  GameEntity *projectile = G_FindBeamProjectile(emitter);

  if (projectile) {
    G_FreeEntity(projectile);
  }
}

/**
 * @brief Fires a railgun slug that traces through multiple targets, dealing damage to each.
 * @param emitter The entity the slug leaves, providing its origin and effect color.
 * @param attacker The entity credited with any damage the slug inflicts.
 */
void G_RailgunProjectile(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t damage,
             int32_t knockback, uint32_t mod) {
  Vec3 pos, end;

  pos = start;

  CmTrace tr = gi.Trace(emitter->s.origin, pos, Box3_Zero(), emitter, CONTENTS_MASK_CLIP_PROJECTILE);
  if (tr.fraction < 1.0) {
    pos = emitter->s.origin;
  }

  int32_t contentMask = CONTENTS_MASK_CLIP_PROJECTILE | CONTENTS_MASK_LIQUID;
  bool liquid = false;

  // are we starting in water?
  if (gi.PointContents(pos) & CONTENTS_MASK_LIQUID) {
    contentMask &= ~CONTENTS_MASK_LIQUID;
    liquid = true;
  }

  end = Vec3_Fmaf(pos, MAX_WORLD_DIST, dir);

  G_Ripple(NULL, pos, end, 24.f, true);

  GameEntity *ignore = emitter;
  while (ignore) {
    tr = gi.Trace(pos, end, Box3_Zero(), ignore, contentMask);
    if (!tr.ent) {
      break;
    }

    if ((tr.contents & CONTENTS_MASK_LIQUID) && !liquid) {

      contentMask &= ~CONTENTS_MASK_LIQUID;
      liquid = true;

      G_MulticastSound(&(const GamePlaySound) {
        .index = gMedia.sounds.waterIn,
        .origin = &tr.end,
      }, MULTICAST_PHS);

      ignore = emitter;
      continue;
    }

    GameEntity *other = tr.ent;
    if (other->client || other->solid == SOLID_BOX) {
      ignore = other;
    } else {
      ignore = NULL;
    }

    // we've hit something, so damage it
    if ((tr.ent != emitter) && (tr.ent != attacker) && G_TakesDamage(tr.ent)) {
      G_Damage(&(GameDamage) {
        .target = tr.ent,
        .inflictor = emitter,
        .attacker = attacker,
        .dir = dir,
        .point = tr.end,
        .normal = tr.plane.normal,
        .damage = damage,
        .knockback = knockback,
        .flags = 0,
        .mod = mod ?: MOD_RAILGUN
      });
    }

    pos = tr.end;
  }

  // send rail trail
  gi.WriteByte(SV_CMD_TEMP_ENTITY);
  gi.WriteByte(TE_RAIL);
  gi.WritePosition(start);
  gi.WritePosition(tr.end);
  gi.WriteDir(tr.plane.normal);
  gi.WriteLong(tr.surface);
  gi.WriteByte(emitter->s.client);

  gi.Multicast(start, MULTICAST_PHS);
}

/**
 * @brief Touch callback for the BFG projectile; deals energy and radius blast damage on impact.
 */
static void G_BfgProjectile_Touch(GameEntity *ent, GameEntity *other, const CmTrace *trace) {

  if (other == ent->owner) {
    return;
  }

  if (other->solid < SOLID_DEAD) {
    return;
  }

  if (trace == NULL) {
    return;
  }

  if (!G_IsSky(trace)) {

    if (G_IsStructural(trace) || G_IsMeat(other)) {

      G_Damage(&(GameDamage) {
        .target = other,
        .inflictor = ent,
        .attacker = ent->owner,
        .dir = ent->velocity,
        .point = ent->s.origin,
        .normal = trace->plane.normal,
        .damage = ent->damage,
        .knockback = ent->knockback,
        .flags = DMG_ENERGY,
        .mod = MOD_BFG_BLAST
      });

      G_RadiusDamage(ent, ent->owner, other, ent->damage, ent->knockback, ent->damageRadius, MOD_BFG_BLAST);

      gi.WriteByte(SV_CMD_TEMP_ENTITY);
      gi.WriteByte(TE_BFG);
      gi.WritePosition(ent->s.origin);
      gi.Multicast(ent->s.origin, MULTICAST_PHS);
    }
  }

  G_FreeEntity(ent);
}

/**
 * @brief Think callback for the BFG projectile; deals continuous radius laser damage to nearby entities each tick.
 */
static void G_BfgProjectile_Think(GameEntity *ent) {

  // every other projectile frees itself on a timer; this one only ever re-armed, so a ball that
  // never touched anything, having been spawned inside the brushwork it left, hung about forever
  if (gLevel.time >= ent->timestamp) {
    G_FreeEntity(ent);
    return;
  }

  const int32_t frameDamage = ent->damage * QUETOO_TICK_SECONDS;
  const int32_t frameKnockback = ent->knockback * QUETOO_TICK_SECONDS;

  G_ForEachEntity(other, {

    if (other == ent || other == ent->owner) {
      continue;
    }

    if (!other->takeDamage) {
      continue;
    }

    if (!G_CanDamage(other, ent)) {
      continue;
    }

    const Vec3 end = G_GetOrigin(other);
    const Vec3 dir = Vec3_Subtract(end, ent->s.origin);
    const float dist = Vec3_Length(dir) - Box3_Radius(ent->bounds);
    const Vec3 normal = Vec3_Normalize(Vec3_Negate(dir));

    const float f = 1.0 - dist / ent->damageRadius;

    if (f <= 0.f) {
      continue;
    }

    G_Damage(&(GameDamage) {
      .target = other,
      .inflictor = ent,
      .attacker = ent->owner,
      .dir = dir,
      .point = other->s.origin,
      .normal = normal,
      .damage = frameDamage * f,
      .knockback = frameKnockback * f,
      .flags = DMG_RADIUS,
      .mod = MOD_BFG_LASER
    });

    gi.WriteByte(SV_CMD_TEMP_ENTITY);
    gi.WriteByte(other->dead ? TE_BFG_LASER_DEAD : TE_BFG_LASER);
    gi.WriteShort(ent->s.number);
    gi.WriteShort(other->s.number);
    gi.Multicast(ent->s.origin, MULTICAST_PVS);
  });

  ent->nextThink = gLevel.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief Fires a BFG projectile with continuous area-effect damage and a large on-impact explosion.
 */
void G_BfgProjectile(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t speed, int32_t damage, int32_t knockback, float damageRadius) {

  const Box3 bounds = Box3f(24.f, 24.f, 24.f);

  GameEntity *projectile = G_AllocEntity(__func__);
  projectile->owner = attacker;

  projectile->s.origin = start;
  projectile->bounds = bounds;
  projectile->velocity = Vec3_Scale(dir, speed);

  if (G_ImmediateWall(emitter, projectile)) {
    projectile->s.origin = emitter->s.origin;
  }

  projectile->solid = SOLID_PROJECTILE;
  projectile->clipMask = CONTENTS_MASK_CLIP_PROJECTILE;
  projectile->damage = damage;
  projectile->damageRadius = damageRadius;
  projectile->knockback = knockback;
  projectile->moveType = MOVE_TYPE_FLY;
  projectile->nextThink = gLevel.time + QUETOO_TICK_MILLIS;
  projectile->timestamp = gLevel.time + 8000;
  projectile->Think = G_BfgProjectile_Think;
  projectile->Touch = G_BfgProjectile_Touch;
  projectile->s.trail = TRAIL_BFG;

  gi.LinkEntity(projectile);
}

