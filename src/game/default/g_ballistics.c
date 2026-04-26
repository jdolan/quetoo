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
static void G_PlayerProjectile(g_entity_t *ent, const float scale) {

  if (ent->owner) {
    const float s = scale * g_player_projectile->value;
    ent->velocity = Vec3_Fmaf(ent->velocity, s, ent->owner->velocity);
  } else {
    G_Debug("No owner for %s\n", etos(ent));
  }
}

/**
 * @brief Returns true if the entity is facing a wall at too close proximity
 * for the specified projectile.
 */
static bool G_ImmediateWall(g_entity_t *ent, g_entity_t *projectile) {

  const cm_trace_t tr = gi.Trace(ent->s.origin, projectile->s.origin, projectile->bounds,
                                 ent, CONTENTS_MASK_SOLID);

  return tr.fraction < 1.0;
}

/**
 * @brief Returns true if the specified entity takes damage.
 */
static bool G_TakesDamage(g_entity_t *ent) {
  return (ent && ent->take_damage);
}

/**
 * @brief Used to add generic bubble trails to shots.
 */
static void G_BubbleTrail(const vec3_t start, cm_trace_t *tr, float freq) {
  vec3_t dir, pos;

  if (Vec3_Equal(tr->end, start)) {
    return;
  }

  dir = Vec3_Subtract(tr->end, start);
  dir = Vec3_Normalize(dir);
  pos = Vec3_Fmaf(tr->end, -2.f, dir);

  if (gi.PointContents(pos) & CONTENTS_MASK_LIQUID) {
    tr->end = pos;
  } else {
    const cm_trace_t trace = gi.Trace(pos, start, Box3_Zero(), tr->ent, CONTENTS_MASK_LIQUID);
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
static void G_Tracer(const vec3_t start, const vec3_t end) {
  vec3_t dir, mid;
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
 * @brief Emit a water ripple and optional splash effect.
 * @details The caller must pass either an entity, or two points to trace between.
 * @param ent The entity entering the water, or NULL.
 * @param pos1 The start position to trace for liquid from.
 * @param pos2 The end position to trace for liquid to.
 * @param size The ripple size, or 0.0 to use the entity's size.
 * @param splash True to emit a splash effect, false otherwise.
 */
void G_Ripple(g_entity_t *ent, const vec3_t pos1, const vec3_t pos2, float size, bool splash) {

  const cm_trace_t tr = gi.Trace(pos1, pos2, Box3_Zero(), ent, CONTENTS_MASK_LIQUID);
  if (!tr.brush_side) {
    G_Debug("Failed to resolve water brush side for %s\n", etos(ent));
    return;
  }

  const vec3_t pos = Vec3_Add(tr.end, Vec3_Up());
  const vec3_t dir = tr.plane.normal;

  if (ent) {
    if (g_level.time - ent->ripple_time < 400) {
      return;
    }
    
    ent->ripple_time = g_level.time;

    if (size == 0.f) {
      if (ent->ripple_size) {
        size = ent->ripple_size;
      } else {
        size = Clampf(Box3_Distance(ent->bounds), 12.0, 64.0);
      }
    }
  }

  gi.WriteByte(SV_CMD_TEMP_ENTITY);
  gi.WriteByte(TE_RIPPLE);
  gi.WritePosition(pos);
  gi.WriteDir(dir);
  gi.WriteLong((int32_t) (ptrdiff_t) (tr.brush_side - gi.Bsp()->brush_sides));
  gi.WriteByte((uint8_t) size);
  gi.WriteByte((uint8_t) splash);

  gi.Multicast(pos, MULTICAST_PVS);

  if (!(tr.contents & CONTENTS_TRANSLUCENT)) {

    gi.WriteByte(SV_CMD_TEMP_ENTITY);
    gi.WriteByte(TE_RIPPLE);
    gi.WritePosition(Vec3_Add(pos, Vec3_Down()));
    gi.WriteDir(Vec3_Negate(dir));
    gi.WriteLong((int32_t) (ptrdiff_t) (tr.brush_side - gi.Bsp()->brush_sides));
    gi.WriteByte((uint8_t) size);
    gi.WriteByte((uint8_t) false);

    gi.Multicast(pos, MULTICAST_PVS);
  }
}


/**
 * @brief Used to add impact marks on surfaces hit by bullets.
 */
static void G_BulletImpact(const cm_trace_t *trace) {

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
static void G_BlasterProjectile_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

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

    G_Damage(&(g_damage_t) {
      .target = other,
      .inflictor = ent,
      .attacker = ent->owner,
      .dir = ent->velocity,
      .point = ent->s.origin,
      .normal = trace->plane.normal,
      .damage = ent->damage,
      .knockback = ent->knockback,
      .flags = DMG_ENERGY,
      .mod = MOD_BLASTER
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
 */
void G_BlasterProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int32_t speed, int32_t damage, int32_t knockback) {

  const box3_t bounds = Box3f(2.f, 2.f, 2.f);

  g_entity_t *projectile = G_AllocEntity(__func__);
  projectile->owner = ent;

  projectile->s.origin = start;
  projectile->bounds = bounds;
  projectile->s.angles = Vec3_Euler(dir);
  projectile->velocity = Vec3_Scale(dir, speed);

  if (G_ImmediateWall(ent, projectile)) {
    projectile->s.origin = ent->s.origin;
  }

  projectile->solid = SOLID_PROJECTILE;
  projectile->clip_mask = CONTENTS_MASK_CLIP_PROJECTILE;
  projectile->damage = damage;
  projectile->knockback = knockback;
  projectile->move_type = MOVE_TYPE_FLY;
  projectile->next_think = g_level.time + 8000;
  projectile->Think = G_FreeEntity;
  projectile->Touch = G_BlasterProjectile_Touch;
  projectile->s.client = ent->s.client;
  projectile->s.trail = TRAIL_BLASTER;

  gi.LinkEntity(projectile);
}

/**
 * @brief Fires a single bullet projectile with randomized spread, dealing damage and emitting impact effects.
 */
void G_BulletProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int32_t damage, int32_t knockback, int32_t hspread, int32_t vspread, int32_t mod) {

  cm_trace_t tr = gi.Trace(ent->s.origin, start, Box3f(1.f, 1.f, 1.f), ent, CONTENTS_MASK_CLIP_PROJECTILE);
  if (tr.fraction == 1.0) {
    vec3_t angles, forward, right, up, end;

    angles = Vec3_Euler(dir);
    Vec3_Vectors(angles, &forward, &right, &up);

    end = Vec3_Fmaf(start, MAX_WORLD_DIST, forward);
    end = Vec3_Fmaf(end, RandomRangef(-hspread, hspread), right);
    end = Vec3_Fmaf(end, RandomRangef(-vspread, vspread), up);

    tr = gi.Trace(start, end, Box3_Zero(), ent, CONTENTS_MASK_CLIP_PROJECTILE);

    G_Tracer(start, tr.end);
  }

  if (tr.fraction < 1.0) {

    G_Damage(&(g_damage_t) {
      .target = tr.ent,
      .inflictor = ent,
      .attacker = ent,
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
void G_ShotgunProjectiles(g_entity_t *ent, const vec3_t start, const vec3_t dir, int32_t damage, int32_t knockback, int32_t hspread, int32_t vspread, int32_t count, int32_t mod) {

  for (int32_t i = 0; i < count; i++) {
    G_BulletProjectile(ent, start, dir, damage, knockback, hspread, vspread, mod);
  }
}

#define HAND_GRENADE 1
#define HAND_GRENADE_HELD 2

/**
 * @brief Detonates a grenade projectile, dealing direct and splash radius damage.
 */
static void G_GrenadeProjectile_Explode(g_entity_t *ent) {
  int32_t mod;

  if (ent->enemy) { // direct hit

    mod = ent->spawn_flags & HAND_GRENADE ? MOD_HANDGRENADE : MOD_GRENADE;

    G_Damage(&(g_damage_t) {
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

  if (ent->spawn_flags & HAND_GRENADE) {
    if (ent->spawn_flags & HAND_GRENADE_HELD) {
      mod = MOD_HANDGRENADE_KAMIKAZE;
    } else {
      mod = MOD_HANDGRENADE_SPLASH;
    }
  } else {
    mod = MOD_GRENADE_SPLASH;
  }

  // hurt anything else nearby
  G_RadiusDamage(ent, ent->owner, ent->enemy, ent->damage, ent->knockback, ent->damage_radius, mod);

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
void G_GrenadeProjectile_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

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
      if (g_level.time - ent->touch_time > 200) {
        if (Vec3_Length(ent->velocity) > 40.0) {
          G_MulticastSound(&(const g_play_sound_t) {
            .index = g_media.sounds.grenade_hit,
            .entity = ent,
            .atten = SOUND_ATTEN_LINEAR,
            .pitch = (int8_t) (Randomf() * 5.0)
          }, MULTICAST_PHS);
          ent->touch_time = g_level.time;
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
 */
void G_GrenadeProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int32_t speed, int32_t damage, int32_t knockback, float damage_radius, uint32_t timer) {

  const box3_t bounds = Box3f(6.f, 6.f, 6.f);

  vec3_t forward, right, up;

  g_entity_t *projectile = G_AllocEntity(__func__);
  projectile->owner = ent;

  projectile->s.origin = start;
  projectile->bounds = bounds;
  projectile->s.angles = Vec3_Euler(dir);

  Vec3_Vectors(projectile->s.angles, &forward, &right, &up);
  projectile->velocity = Vec3_Scale(dir, speed);

  projectile->velocity = Vec3_Fmaf(projectile->velocity, RandomRangef(90.f, 110.f), up);
  projectile->velocity = Vec3_Fmaf(projectile->velocity, RandomRangef(-10.f, 10.f), right);

  G_PlayerProjectile(projectile, 0.33);

  if (G_ImmediateWall(ent, projectile)) {
    projectile->s.origin = ent->s.origin;
  }

  projectile->solid = SOLID_PROJECTILE;
  projectile->avelocity.x = RandomRangef(-310.f, -290.f);
  projectile->avelocity.y = RandomRangef(-50.f, 50.f);
  projectile->avelocity.z = RandomRangef(-25.f, 25.f);
  projectile->clip_mask = CONTENTS_MASK_CLIP_PROJECTILE;
  projectile->damage = damage;
  projectile->damage_radius = damage_radius;
  projectile->knockback = knockback;
  projectile->move_type = MOVE_TYPE_BOUNCE;
  projectile->next_think = g_level.time + timer;
  projectile->take_damage = true;
  projectile->Think = G_GrenadeProjectile_Explode;
  projectile->Touch = G_GrenadeProjectile_Touch;
  projectile->touch_time = g_level.time;
  projectile->s.trail = TRAIL_GRENADE;
  projectile->s.model1 = g_media.models.grenade;

  gi.LinkEntity(projectile);
}

// tossing a hand grenade
void G_HandGrenadeProjectile(g_entity_t *ent, g_entity_t *projectile, vec3_t const start, const vec3_t dir, int32_t speed, int32_t damage, int32_t knockback, float damage_radius, uint32_t timer) {

  const box3_t bounds = Box3f(4.f, 4.f, 4.f);

  vec3_t forward, right, up;

  projectile->s.origin = start;
  projectile->bounds = bounds;
  projectile->s.angles = Vec3_Euler(dir);

  Vec3_Vectors(projectile->s.angles, &forward, &right, &up);
  projectile->velocity = Vec3_Scale(dir, speed);

  projectile->velocity = Vec3_Fmaf(projectile->velocity, RandomRangef(190.f, 210.f), up);
  projectile->velocity = Vec3_Fmaf(projectile->velocity, RandomRangef(-10.f, 10.f), right);

  // add some of the player's velocity to the projectile
  G_PlayerProjectile(projectile, 0.33);

  if (G_ImmediateWall(ent, projectile)) {
    projectile->s.origin = ent->s.origin;
  }

  projectile->spawn_flags = HAND_GRENADE;

  // if client is holding it, let the nade know it's being held
  if (ent->client->grenade_hold_time) {
    projectile->spawn_flags |= HAND_GRENADE_HELD;
  }

  projectile->avelocity.x = RandomRangef(-310.f, -290.f);
  projectile->avelocity.y = RandomRangef(-50.f, 50.f);
  projectile->avelocity.z = RandomRangef(-25.f, 25.f);
  projectile->damage = damage;
  projectile->damage_radius = damage_radius;
  projectile->knockback = knockback;
  projectile->next_think = g_level.time + timer;
  projectile->solid = SOLID_PROJECTILE;
  projectile->sv_flags &= ~SVF_NO_CLIENT;
  projectile->move_type = MOVE_TYPE_BOUNCE;
  projectile->Think = G_GrenadeProjectile_Explode;

  gi.LinkEntity(projectile);
}

/**
 * @brief Touch callback for the rocket projectile; deals direct and radius explosion damage on impact.
 */
static void G_RocketProjectile_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

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

      G_Damage(&(g_damage_t) {
        .target = other,
        .inflictor = ent,
        .attacker = ent->owner,
        .dir = ent->velocity,
        .point = ent->s.origin,
        .normal = trace->plane.normal,
        .damage = ent->damage,
        .knockback = ent->knockback,
        .flags = 0,
        .mod = MOD_ROCKET
      });

      G_RadiusDamage(ent, ent->owner, other, ent->damage, ent->knockback, ent->damage_radius, MOD_ROCKET_SPLASH);

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
 */
void G_RocketProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int32_t speed, int32_t damage, int32_t knockback, float damage_radius) {

  const box3_t bounds = Box3f(8.f, 8.f, 8.f);

  g_entity_t *projectile = G_AllocEntity(__func__);
  projectile->owner = ent;

  projectile->s.origin = start;
  projectile->bounds = bounds;
  projectile->s.angles = Vec3_Euler(dir);
  projectile->velocity = Vec3_Scale(dir, speed);
  projectile->avelocity = Vec3(0.0, 0.0, 600.0);

  if (G_ImmediateWall(ent, projectile)) {
    projectile->s.origin = ent->s.origin;
  }

  projectile->solid = SOLID_PROJECTILE;
  projectile->clip_mask = CONTENTS_MASK_CLIP_PROJECTILE;
  projectile->damage = damage;
  projectile->damage_radius = damage_radius;
  projectile->knockback = knockback;
  projectile->ripple_size = 32.0;
  projectile->move_type = MOVE_TYPE_FLY;
  projectile->next_think = g_level.time + 8000;
  projectile->Think = G_FreeEntity;
  projectile->Touch = G_RocketProjectile_Touch;
  projectile->s.model1 = g_media.models.rocket;
  projectile->s.sound = g_media.sounds.rocket_fly;
  projectile->s.trail = TRAIL_ROCKET;

  gi.LinkEntity(projectile);
}

/**
 * @brief Touch callback for the hyperblaster projectile; deals energy damage and handles hyperblaster climb mechanics.
 */
static void G_HyperblasterProjectile_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

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

      G_Damage(&(g_damage_t) {
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

        vec3_t v;
        v = Vec3_Subtract(ent->s.origin, ent->owner->s.origin);

        if (Vec3_Length(v) < 32.0) { // hyperblaster climbing
          G_Damage(&(g_damage_t) {
            .target = ent->owner,
            .inflictor = ent,
            .attacker = ent->owner,
            .dir = Vec3_Zero(),
            .point = ent->s.origin,
            .normal = trace->plane.normal,
            .damage = g_balance_hyperblaster_climb_damage->integer,
            .knockback = 0,
            .flags = DMG_ENERGY,
            .mod = MOD_HYPERBLASTER_CLIMB
          });

          ent->owner->velocity.z += g_balance_hyperblaster_climb_knockback->value;
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
void G_HyperblasterProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int32_t speed, int32_t damage, int32_t knockback) {

  const box3_t bounds = Box3f(6.f, 6.f, 6.f);

  g_entity_t *projectile = G_AllocEntity(__func__);
  projectile->owner = ent;

  projectile->s.origin = start;
  projectile->bounds = bounds;
  projectile->s.angles = Vec3_Euler(dir);
  projectile->velocity = Vec3_Scale(dir, speed);

  if (G_ImmediateWall(ent, projectile)) {
    projectile->s.origin = ent->s.origin;
  }

  projectile->solid = SOLID_PROJECTILE;
  projectile->clip_mask = CONTENTS_MASK_CLIP_PROJECTILE;
  projectile->damage = damage;
  projectile->knockback = knockback;
  projectile->ripple_size = 22.0;
  projectile->move_type = MOVE_TYPE_FLY;
  projectile->next_think = g_level.time + 6000;
  projectile->Think = G_FreeEntity;
  projectile->Touch = G_HyperblasterProjectile_Touch;
  projectile->s.trail = TRAIL_HYPERBLASTER;

  gi.LinkEntity(projectile);
}

/**
 * @brief Discharges the lightning gun into water, killing the owner and dealing scaled damage to all nearby entities.
 */
static void G_LightningProjectile_Discharge(g_entity_t *ent) {

  // kill ourselves
  G_Damage(&(g_damage_t) {
    .target = ent->owner,
    .inflictor = ent,
    .attacker = ent->owner,
    .dir = Vec3_Zero(),
    .point = ent->s.origin,
    .normal = Vec3_Zero(),
    .damage = 9999,
    .knockback = 100,
    .flags = DMG_NO_ARMOR,
    .mod = MOD_LIGHTNING_DISCHARGE
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

    if (ent->water_level > WATER_NONE) {
      const int32_t dmg = 50 * ent->water_level * atten;

      G_Damage(&(g_damage_t) {
        .target = other,
        .inflictor = ent,
        .attacker = ent->owner,
        .dir = Vec3_Zero(),
        .point = other->s.origin,
        .normal = Vec3_Zero(),
        .damage = dmg,
        .knockback = 100 * atten,
        .flags = DMG_NO_ARMOR,
        .mod = MOD_LIGHTNING_DISCHARGE
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
static bool G_LightningProjectile_Expire(g_entity_t *ent) {

  if (ent->timestamp < g_level.time - (SECONDS_TO_MILLIS(g_balance_lightning_refire->value) + 1)) {
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
static void G_LightningProjectile_Think(g_entity_t *ent) {
  vec3_t forward, right, up;
  vec3_t start, end;
  vec3_t water_start;
  cm_trace_t tr;

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

  end = Vec3_Fmaf(start, g_balance_lightning_length->value, forward); // resolve end
  end = Vec3_Fmaf(end, 2.f * sinf(g_level.time / 4.f), up);
  end = Vec3_Fmaf(end, RandomRangef(-2.f, 2.f), right);

  tr = gi.Trace(start, end, Box3_Zero(), ent, CONTENTS_MASK_CLIP_PROJECTILE | CONTENTS_MASK_LIQUID);

  if (tr.contents & CONTENTS_MASK_LIQUID) { // entered water, play sound, leave trail
    water_start = tr.end;

    if (!ent->water_level) {
      G_MulticastSound(&(const g_play_sound_t) {
        .index = g_media.sounds.water_in,
        .origin = &water_start,
        .atten = SOUND_ATTEN_LINEAR
      }, MULTICAST_PHS);
      ent->water_level = WATER_FEET;
    }

    tr = gi.Trace(water_start, end, Box3_Zero(), ent, CONTENTS_MASK_CLIP_PROJECTILE);
    G_BubbleTrail(water_start, &tr, 4.f);

    G_Ripple(NULL, start, end, 16.f, true);
  } else {
    if (ent->water_level) { // exited water, play sound, no trail
      G_MulticastSound(&(const g_play_sound_t) {
        .index = g_media.sounds.water_out,
        .origin = &start,
        .atten = SOUND_ATTEN_LINEAR
      }, MULTICAST_PHS);
      ent->water_level = WATER_NONE;
    }
  }

  // clear the angles for impact effects
  ent->s.angles = Vec3_Zero();
  ent->s.animation1 = LIGHTNING_NO_HIT;

  if (ent->damage) { // shoot, removing our damage until it is renewed
    if (G_TakesDamage(tr.ent)) { // try to damage what we hit
      G_Damage(&(g_damage_t) {
        .target = tr.ent,
        .inflictor = ent,
        .attacker = ent->owner,
        .dir = forward,
        .point = tr.end,
        .normal = tr.plane.normal,
        .damage = ent->damage,
        .knockback = ent->knockback,
        .flags = DMG_ENERGY,
        .mod = MOD_LIGHTNING
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

  ent->next_think = g_level.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief Creates or updates the lightning beam projectile entity for the given owner.
 */
void G_LightningProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int32_t damage, int32_t knockback) {

  g_entity_t *projectile = NULL;

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

    projectile->s.termination = Vec3_Fmaf(start, g_balance_lightning_length->value, dir);

    projectile->owner = ent;
    projectile->solid = SOLID_NOT;
    projectile->clip_mask = CONTENTS_MASK_CLIP_PROJECTILE;
    projectile->move_type = MOVE_TYPE_THINK;
    projectile->Think = G_LightningProjectile_Think;
    projectile->knockback = knockback;
    projectile->s.client = ent->s.client;
    projectile->s.effects = EF_BEAM;
    projectile->s.sound = g_media.sounds.lightning_fly;
    projectile->s.trail = TRAIL_LIGHTNING;

    gi.LinkEntity(projectile);
  }

  // set the damage and think time
  projectile->damage = damage;
  projectile->next_think = g_level.time + 1;
  projectile->timestamp = g_level.time;
  projectile->water_level = WATER_NONE;
}

/**
 * @brief Fires a railgun slug that traces through multiple targets, dealing damage to each.
 */
void G_RailgunProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int32_t damage,
             int32_t knockback) {
  vec3_t pos, end;

  pos = start;

  cm_trace_t tr = gi.Trace(ent->s.origin, pos, Box3_Zero(), ent, CONTENTS_MASK_CLIP_PROJECTILE);
  if (tr.fraction < 1.0) {
    pos = ent->s.origin;
  }

  int32_t content_mask = CONTENTS_MASK_CLIP_PROJECTILE | CONTENTS_MASK_LIQUID;
  bool liquid = false;

  // are we starting in water?
  if (gi.PointContents(pos) & CONTENTS_MASK_LIQUID) {
    content_mask &= ~CONTENTS_MASK_LIQUID;
    liquid = true;
  }

  end = Vec3_Fmaf(pos, MAX_WORLD_DIST, dir);

  G_Ripple(NULL, pos, end, 24.f, true);

  g_entity_t *ignore = ent;
  while (ignore) {
    tr = gi.Trace(pos, end, Box3_Zero(), ignore, content_mask);
    if (!tr.ent) {
      break;
    }

    if ((tr.contents & CONTENTS_MASK_LIQUID) && !liquid) {

      content_mask &= ~CONTENTS_MASK_LIQUID;
      liquid = true;

      G_MulticastSound(&(const g_play_sound_t) {
        .index = g_media.sounds.water_in,
        .origin = &tr.end,
        .atten = SOUND_ATTEN_LINEAR
      }, MULTICAST_PHS);

      ignore = ent;
      continue;
    }

    g_entity_t *other = tr.ent;
    if (other->client || other->solid == SOLID_BOX) {
      ignore = other;
    } else {
      ignore = NULL;
    }

    // we've hit something, so damage it
    if ((tr.ent != ent) && G_TakesDamage(tr.ent)) {
      G_Damage(&(g_damage_t) {
        .target = tr.ent,
        .inflictor = ent,
        .attacker = ent,
        .dir = dir,
        .point = tr.end,
        .normal = tr.plane.normal,
        .damage = damage,
        .knockback = knockback,
        .flags = 0,
        .mod = MOD_RAILGUN
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
  gi.WriteByte(ent->s.client);

  gi.Multicast(start, MULTICAST_PHS);
}

/**
 * @brief Touch callback for the BFG projectile; deals energy and radius blast damage on impact.
 */
static void G_BfgProjectile_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

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

      G_Damage(&(g_damage_t) {
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

      G_RadiusDamage(ent, ent->owner, other, ent->damage, ent->knockback, ent->damage_radius, MOD_BFG_BLAST);

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
static void G_BfgProjectile_Think(g_entity_t *ent) {

  const int32_t frame_damage = ent->damage * QUETOO_TICK_SECONDS;
  const int32_t frame_knockback = ent->knockback * QUETOO_TICK_SECONDS;

  G_ForEachEntity(other, {

    if (other == ent || other == ent->owner) {
      continue;
    }

    if (!other->take_damage) {
      continue;
    }

    if (!G_CanDamage(ent, other)) {
      continue;
    }

    const vec3_t end = G_GetOrigin(other);
    const vec3_t dir = Vec3_Subtract(end, ent->s.origin);
    const float dist = Vec3_Length(dir) - Box3_Radius(ent->bounds);
    const vec3_t normal = Vec3_Normalize(Vec3_Negate(dir));

    const float f = 1.0 - dist / ent->damage_radius;

    if (f <= 0.f) {
      continue;
    }

    G_Damage(&(g_damage_t) {
      .target = other,
      .inflictor = ent,
      .attacker = ent->owner,
      .dir = dir,
      .point = other->s.origin,
      .normal = normal,
      .damage = frame_damage * f,
      .knockback = frame_knockback * f,
      .flags = DMG_RADIUS,
      .mod = MOD_BFG_LASER
    });

    gi.WriteByte(SV_CMD_TEMP_ENTITY);
    gi.WriteByte(TE_BFG_LASER);
    gi.WriteShort(ent->s.number);
    gi.WriteShort(other->s.number);
    gi.Multicast(ent->s.origin, MULTICAST_PVS);
  });

  ent->next_think = g_level.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief Fires a BFG projectile with continuous area-effect damage and a large on-impact explosion.
 */
void G_BfgProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int32_t speed, int32_t damage, int32_t knockback, float damage_radius) {

  const box3_t bounds = Box3f(24.f, 24.f, 24.f);

  g_entity_t *projectile = G_AllocEntity(__func__);
  projectile->owner = ent;

  projectile->s.origin = start;
  projectile->bounds = bounds;
  projectile->velocity = Vec3_Scale(dir, speed);

  if (G_ImmediateWall(ent, projectile)) {
    projectile->s.origin = ent->s.origin;
  }

  projectile->solid = SOLID_PROJECTILE;
  projectile->clip_mask = CONTENTS_MASK_CLIP_PROJECTILE;
  projectile->damage = damage;
  projectile->damage_radius = damage_radius;
  projectile->knockback = knockback;
  projectile->move_type = MOVE_TYPE_FLY;
  projectile->next_think = g_level.time + QUETOO_TICK_MILLIS;
  projectile->Think = G_BfgProjectile_Think;
  projectile->Touch = G_BfgProjectile_Touch;
  projectile->s.trail = TRAIL_BFG;

  gi.LinkEntity(projectile);
}

/**
 * @brief Touch callback for the hook projectile; attaches to structural surfaces or deals damage and detaches on hitting enemies.
 */
static void G_HookProjectile_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (other == ent->owner) {
    return;
  }

  if (other->solid < SOLID_DEAD) {
    return;
  }

  if (trace == NULL) {
    return;
  }

  ent->s.sound = 0;

  if (!G_IsSky(trace)) {

    if (G_IsStructural(trace) || (G_IsMeat(other) && G_OnSameTeam(other->client, ent->owner->client))) {

      ent->velocity = Vec3_Zero();
      ent->avelocity = Vec3_Zero();

      ent->owner->client->hook_pull = true;

      ent->move_type = MOVE_TYPE_THINK;
      ent->solid = SOLID_NOT;
      ent->bounds = Box3_Zero();
      ent->enemy = other;

      gi.LinkEntity(ent);

      ent->owner->client->ps.pm_state.hook_position = ent->s.origin;

      if (ent->owner->client->persistent.hook_style != HOOK_PULL) {
        const float distance = Vec3_Distance(ent->owner->s.origin, ent->s.origin);

        ent->owner->client->ps.pm_state.hook_length = Clampf(distance, PM_HOOK_MIN_DIST, g_hook_distance->value);
      }

      gi.WriteByte(SV_CMD_TEMP_ENTITY);
      gi.WriteByte(TE_HOOK_IMPACT);
      gi.WritePosition(ent->s.origin);
      gi.WriteDir(trace->plane.normal);
      gi.Multicast(ent->s.origin, MULTICAST_PHS);
    } else {

      G_MulticastSound(&(const g_play_sound_t) {
        .index = g_media.sounds.hook_gibhit,
        .entity = ent,
        .atten = SOUND_ATTEN_LINEAR,
        .pitch = RandomRangei(-4, 5)
      }, MULTICAST_PHS);

      /*
      if (g_hook_auto_refire->integer) {
        G_HookThink(ent->owner, true);
      } else {*/
        ent->velocity = Vec3_Normalize(ent->velocity);

        G_Damage(&(g_damage_t) {
          .target = other,
          .inflictor = ent,
          .attacker = ent->owner,
          .dir = ent->velocity,
          .point = ent->s.origin,
          .normal = Vec3_Zero(),
          .damage = 5,
          .knockback = 0,
          .flags = 0,
          .mod = MOD_HOOK
        });

        G_HookDetach(ent->owner->client);
//      }
    }
  } else {
    /* Currently disabled due to bugs
    if (g_hook_auto_refire->integer) {
      G_HookThink(ent->owner, true);
    } else {
    */
      G_HookDetach(ent->owner->client);
//    }
  }
}

/**
 * @brief Think callback for the hook cable trail; updates beam endpoints and detaches if the hook exceeds maximum range.
 */
static void G_HookTrail_Think(g_entity_t *ent) {

  const g_entity_t *hook = ent->target_ent;
  g_client_t *cl = ent->owner->client;

  vec3_t forward, right, up, org;

  G_ClientProjectile(cl, &forward, &right, &up, &org, -1.0);

  ent->s.origin = org;
  ent->s.termination = hook->s.origin;

  vec3_t distance;
  distance = Vec3_Subtract(org, hook->s.origin);

  if (Vec3_Length(distance) > g_hook_distance->value) {

    G_HookDetach(cl);
    return;
  }

  ent->next_think = g_level.time + 1;
  gi.LinkEntity(ent);
}

/**
 * @brief Think callback for the hook projectile; tracks attached movers and updates the hook position each tick.
 */
static void G_HookProjectile_Think(g_entity_t *ent) {

  // if we're attached to something, copy velocities
  if (ent->enemy) {
    g_entity_t *mover = ent->enemy;
    vec3_t move, amove, inverse_amove, forward, right, up, rotate, translate, delta;

    move = Vec3_Scale(mover->velocity, QUETOO_TICK_SECONDS);
    amove = Vec3_Scale(mover->avelocity, QUETOO_TICK_SECONDS);

    if (!Vec3_Equal(move, Vec3_Zero()) || !Vec3_Equal(amove, Vec3_Zero())) {
      inverse_amove = Vec3_Negate(amove);
      Vec3_Vectors(inverse_amove, &forward, &right, &up);

      // translate the pushed entity
      ent->s.origin = Vec3_Add(ent->s.origin, move);

      // then rotate the movement to comply with the pusher's rotation
      translate = Vec3_Subtract(ent->s.origin, mover->s.origin);

      rotate.x = Vec3_Dot(translate, forward);
      rotate.y = -Vec3_Dot(translate, right);
      rotate.z = Vec3_Dot(translate, up);

      delta = Vec3_Subtract(rotate, translate);

      ent->s.origin = Vec3_Add(ent->s.origin, delta);

      // FIXME: any way we can have the hook move on all axis?
      ent->s.angles.y += amove.y;
      ent->target_ent->s.angles.y += amove.y;

      gi.LinkEntity(ent);

      ent->owner->client->ps.pm_state.hook_position = ent->s.origin;
    }

    if ((ent->owner->client->persistent.hook_style == HOOK_PULL && Vec3_LengthSquared(ent->owner->velocity) > 128.0) ||
      ent->knockback != ent->owner->client->ps.pm_state.hook_length) {
      ent->s.sound = g_media.sounds.hook_pull;
      ent->knockback = ent->owner->client->ps.pm_state.hook_length;
    } else {
      ent->s.sound = 0;
    }
  }

  ent->next_think = g_level.time + 1;
}

/**
 * @brief Fires a grappling hook projectile from the specified entity in the given direction.
 */
g_entity_t *G_HookProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir) {
  g_entity_t *projectile = G_AllocEntity(__func__);
  projectile->owner = ent;

  projectile->s.origin = start;
  projectile->s.angles = Vec3_Euler(dir);
  projectile->velocity = Vec3_Scale(dir, g_hook_speed->value);
  projectile->avelocity = Vec3(0, 0, 500);

  if (G_ImmediateWall(ent, projectile)) {
    projectile->s.origin = ent->s.origin;
  }

  projectile->solid = SOLID_PROJECTILE;
  projectile->clip_mask = CONTENTS_MASK_CLIP_PROJECTILE;
  projectile->move_type = MOVE_TYPE_FLY;
  projectile->Touch = G_HookProjectile_Touch;
  projectile->s.model1 = g_media.models.hook;
  projectile->Think = G_HookProjectile_Think;
  projectile->next_think = g_level.time + 1;
  projectile->s.sound = g_media.sounds.hook_fly;

  gi.LinkEntity(projectile);

  g_entity_t *trail = G_AllocEntity(__func__);

  projectile->target_ent = trail;
  trail->target_ent = projectile;

  trail->owner = ent;
  trail->solid = SOLID_NOT;
  trail->clip_mask = CONTENTS_MASK_CLIP_PROJECTILE;
  trail->move_type = MOVE_TYPE_THINK;
  trail->s.client = ent->s.client;
  trail->s.effects = EF_BEAM;
  trail->s.trail = TRAIL_HOOK;
  trail->Think = G_HookTrail_Think;
  trail->next_think = g_level.time + 1;

  G_HookTrail_Think(trail);

  // angle is used for rendering on client side
  trail->s.angles = projectile->s.angles;

  gi.LinkEntity(trail);
  return projectile;
}
