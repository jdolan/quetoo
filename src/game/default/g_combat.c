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
#include "bg_pmove.h"

/**
 * @brief Returns true if ent1 and ent2 are on the same team.
 */
bool G_OnSameTeam(const g_client_t *a, const g_client_t *b) {

  if (!a || !b) {
    return false;
  }

  if (a->persistent.spectator && b->persistent.spectator) {
    return true;
  }

  if (!g_level.teams && !g_level.ctf) {
    return false;
  }

  return a->persistent.team == b->persistent.team;
}

/**
 * @brief Returns true if the inflictor can directly damage the target. Used for
 * explosions and melee attacks.
 */
bool G_CanDamage(const g_entity_t *targ, const g_entity_t *inflictor) {
  vec3_t dest;
  cm_trace_t tr;

  // BSP sub-models need special checking because their origin is 0,0,0
  if (targ->solid == SOLID_BSP) {
    dest = Box3_Center(targ->abs_bounds);
    tr = gi.Trace(inflictor->s.origin, dest, Box3_Zero(), inflictor, CONTENTS_MASK_SOLID);
    if (tr.fraction == 1.0) {
      return true;
    }
    if (tr.ent == targ) {
      return true;
    }
    return false;
  }

  tr = gi.Trace(inflictor->s.origin, targ->s.origin, Box3_Zero(), inflictor, CONTENTS_MASK_SOLID);
  if (tr.fraction == 1.0) {
    return true;
  }

  dest = targ->s.origin;
  dest.x += 15.0;
  dest.y += 15.0;
  tr = gi.Trace(inflictor->s.origin, dest, Box3_Zero(), inflictor, CONTENTS_MASK_SOLID);
  if (tr.fraction == 1.0) {
    return true;
  }

  dest = targ->s.origin;
  dest.x += 15.0;
  dest.y -= 15.0;
  tr = gi.Trace(inflictor->s.origin, dest, Box3_Zero(), inflictor, CONTENTS_MASK_SOLID);
  if (tr.fraction == 1.0) {
    return true;
  }

  dest = targ->s.origin;
  dest.x -= 15.0;
  dest.y += 15.0;
  tr = gi.Trace(inflictor->s.origin, dest, Box3_Zero(), inflictor, CONTENTS_MASK_SOLID);
  if (tr.fraction == 1.0) {
    return true;
  }

  dest = targ->s.origin;
  dest.x -= 15.0;
  dest.y -= 15.0;
  tr = gi.Trace(inflictor->s.origin, dest, Box3_Zero(), inflictor, CONTENTS_MASK_SOLID);
  if (tr.fraction == 1.0) {
    return true;
  }

  return false;
}

/**
 * @brief Get the origin of an entity, whether brush or not
 */
vec3_t G_GetOrigin(const g_entity_t *ent) {

  if (ent->solid == SOLID_BSP) {
    return Box3_Center(ent->abs_bounds);
  } else {
    return ent->s.origin;
  }
}

/**
 * @brief
 */
static void G_SpawnDamage(g_temp_entity_t type, const vec3_t pos, const vec3_t normal, int32_t damage) {

  if (damage < 1) {
    return;
  }

  gi.WriteByte(SV_CMD_TEMP_ENTITY);
  gi.WriteByte(type);
  gi.WritePosition(pos);
  gi.WriteDir(normal);
  
  if (type != TE_BULLET) {
    gi.WriteByte(Clampf(damage, 1, 255));
  }
  
  gi.Multicast(pos, MULTICAST_PVS);
}

/**
 * @brief Absorbs damage with the strongest armor the specified client holds.
 *
 * @return The amount of damage absorbed, which is not necessarily the amount
 * of armor consumed.
 */
static int32_t G_CheckArmor(g_entity_t *ent, const vec3_t pos, const vec3_t normal, int32_t damage, uint32_t dflags) {

  if (dflags & DMG_NO_ARMOR) {
    return 0;
  }

  if (!ent->client) {
    return 0;
  }

  const g_item_t *armor = G_ClientArmor(ent->client);
  const g_armor_info_t *armor_info = G_ArmorInfo(armor);

  if (!armor) {
    return 0;
  }

  const int32_t quantity = ent->client->inventory[armor->index];
  int32_t saved;

  if (dflags & DMG_ENERGY) {
    saved = Clampf(damage * armor_info->energy_protection, 0, quantity);
  } else {
    saved = Clampf(damage * armor_info->normal_protection, 0, quantity);
  }

  ent->client->inventory[armor->index] -= saved;

  G_SpawnDamage(TE_BLOOD, pos, normal, saved);

  return saved;
}

/**
 * @brief Damage routine. The inflictor imparts damage on the target on behalf
 * of the attacker.
 *
 * @param damage The damage parameters:
 *   target The target may receive damage.
 *   inflictor The entity inflicting the damage (projectile, optional).
 *   attacker The entity taking credit for the damage (client, optional).
 *   dir The direction of the attack (optional).
 *   point The point at which damage is being inflicted (optional).
 *   normal The normal vector from that point (optional).
 *   damage The damage to be inflicted.
 *   knockback Velocity added to target in the direction of the normal.
 *   flags Damage flags:
 *
 *     DAMAGE_RADIUS        damage was indirect (from a nearby explosion)
 *     DAMAGE_NO_ARMOR      armor does not protect from this damage
 *     DAMAGE_ENERGY        damage is from an energy based weapon
 *     DAMAGE_BULLET        damage is from a bullet
 *     DAMAGE_NO_PROTECTION kills god mode, armor, everything
 *
 *   mod The means of death, used by the obituaries routine.
 */
void G_Damage(const g_damage_t *dmg) {

  g_entity_t *target = dmg->target;
  g_entity_t *inflictor = dmg->inflictor ?: ge.entities[0];
  g_entity_t *attacker = dmg->attacker ?: ge.entities[0];
	const vec3_t dir = dmg->dir;
	const vec3_t pos = dmg->point;
	const vec3_t normal = dmg->normal;
	int32_t damage = dmg->damage;
	int32_t knockback = dmg->knockback;
	int32_t dflags = dmg->flags;
	g_means_of_death mod = dmg->mod;

  assert(target);
  assert(inflictor);
  assert(attacker);
  assert(damage >= 0);
  assert(damage <= INT16_MAX);
  assert(knockback >= 0);
  assert(knockback <= INT16_MAX);

  if (!target->take_damage) {
    return;
  }

  if (target->client) { // respawn protection
    if (target->client->respawn_protection_time > g_level.time) {
      return;
    }
  }

  if (target->client && G_HasTech(target->client, TECH_RESIST)) {
    damage *= TECH_RESIST_DAMAGE_FACTOR;
    knockback *= TECH_RESIST_KNOCKBACK_FACTOR;

    G_PlayTechSound(target->client);
  }

  if (attacker->client) {
    if (attacker->client->inventory[g_media.items.powerups[POWERUP_QUAD]->index]) {
      damage *= QUAD_DAMAGE_FACTOR;
      knockback *= QUAD_KNOCKBACK_FACTOR;
    }

    if (G_HasTech(attacker->client, TECH_STRENGTH)) {
      damage *= TECH_STRENGTH_DAMAGE_FACTOR;
      knockback *= TECH_STRENGTH_KNOCKBACK_FACTOR;

      G_PlayTechSound(attacker->client);
    }
  }

  // friendly fire avoidance
  if (target != attacker && (g_level.teams || g_level.ctf)) {
    if (G_OnSameTeam(target->client, attacker->client)) {

      if (mod == MOD_TELEFRAG) { // telefrags can not be avoided
        mod |= MOD_FRIENDLY_FIRE;
      } else {
        if (g_friendly_fire->value) {
          damage *= g_friendly_fire->value;
          mod |= MOD_FRIENDLY_FIRE;
        } else {
          damage = 0;
        }
      }
    }
  }

  // there is no self damage in instagib or arena, but there is knockback
  if (target == attacker) {
    switch (g_level.gameplay) {
      case GAME_INSTAGIB:
      case GAME_ARENA:
        damage = 0;
        break;
      default:
        damage *= g_self_damage->value;
        break;
    }
  }

  g_client_t *client = target->client;

  // calculate velocity change due to knockback
  if (knockback && (target->move_type >= MOVE_TYPE_WALK)) {
    vec3_t ndir, knockback_vel, knockback_avel;

    ndir = dir;
    ndir = Vec3_Normalize(ndir);

    // knock the target upwards at least a bit; it's fun
    if (ndir.z >= -0.25) {
      ndir.z = Maxf(0.33, ndir.z);
      ndir = Vec3_Normalize(ndir);
    }

    // ensure the target has valid mass for knockback calculation
    const float mass = Clampf(target->mass, 1.0, 1000.0);

    if (target == attacker) { // self knockback (rocket jump / grenade jump / plasma climb)
      knockback *= g_self_knockback->value;
    }

    knockback_vel = Vec3_Scale(ndir, knockback * 100.f / sqrtf(mass));

    target->velocity = Vec3_Add(target->velocity, knockback_vel);

    // apply angular velocity (rotate)
    if (client == NULL || (client->ps.pm_state.flags & PMF_GIBLET)) {
      knockback_avel = Vec3(knockback, knockback, knockback);
      target->avelocity = Vec3_Fmaf(target->avelocity, 100.f / mass, knockback_avel);
    }

    if (client && target->velocity.z >= PM_STEP_HEIGHT) { // make sure the client can leave the ground
      client->ps.pm_state.flags |= PMF_TIME_PUSHED;
      client->ps.pm_state.time = 120;
    }
  }

  int32_t damage_armor = 0, damage_health = 0;

  // check for god mode protection
  if ((target->flags & FL_GOD_MODE) && !(dflags & DMG_NO_GOD)) {
    damage_armor = damage;
    damage_health = 0;
    G_SpawnDamage(TE_BLOOD, pos, normal, damage);
  } else { // or armor protection
    damage_armor = G_CheckArmor(target, pos, normal, damage, dflags);
    damage_health = damage - damage_armor;
  }

  const bool was_dead = target->dead;

  // do the damage
  if (damage_health && (target->health || target->dead)) {
    if (G_IsMeat(target)) {
      G_SpawnDamage(TE_BLOOD, pos, normal, damage_health);
    } else if (dflags & DMG_BULLET) {
      G_SpawnDamage(TE_BULLET, pos, normal, damage_health);
    } else {
      G_SpawnDamage(TE_SPARKS, pos, normal, damage_health);
    }

    if (attacker->client && G_HasTech(attacker->client, TECH_VAMPIRE)) {
      if (!target->dead && attacker != target && !G_OnSameTeam(attacker->client, target->client)) {
        attacker->health = Minf(attacker->health + (damage * TECH_VAMPIRE_DAMAGE_FACTOR), attacker->max_health);
        G_PlayTechSound(attacker->client);
      }
    }

    target->health -= damage_health;

    // for hit sound
    if (!was_dead && attacker->client && attacker->client != client) {
      attacker->client->damage_inflicted += damage_health + damage_armor;
    }

    // kill target if he has *excessive blood loss*
    if (target->health <= 0 && !G_Ai_InDeveloperMode()) {
      target->dead = true;

      if (target->Die) {
        target->Die(target, attacker, mod);
      } else {
        G_Debug("No die function for %s\n", target->class_name);
      }

      return;
    }
  }

  // if the target was already dead, we're done
  if (was_dead) {
    return;
  }

  // invoke the pain callback
  if ((damage_health || knockback) && target->Pain) {
    target->Pain(target, attacker, damage_health, knockback);
  }

  // add view kick on a player this frame
  if (client) {
    client->damage_armor += damage_armor;
    client->damage_health += damage_health;

    float kick = (damage_armor + damage_health) / 50.0;

    if (kick > 1.0) {
      kick = 1.0;
    }

    G_ClientDamageKick(client, dir, kick * 10.0);
  }
}

/**
 * @brief
 */
void G_RadiusDamage(g_entity_t *inflictor, g_entity_t *attacker, g_entity_t *ignore, int32_t damage,
                    int32_t knockback, float radius, g_means_of_death mod) {

  G_ForEachEntity(ent, {
    if (ent == ignore) {
      continue;
    }

    if (!ent->take_damage) {
      continue;
    }

    vec3_t dir = Vec3_Subtract(ent->s.origin, inflictor->s.origin);
    const float dist = Vec3_Length(dir) - Box3_Radius(ent->bounds);

    float d = Maxf(damage - 0.5 * dist, 0.f);
    const float k = Maxf(knockback - 0.5 * dist, 0.f);

    if (d == 0.f && k == 0.f) { // too far away to be damaged
      continue;
    }

    if (ent == attacker) { // reduce self damage
      if (mod == MOD_BFG_BLAST) {
        d *= 0.25;
      } else {
        d *= 0.5;
      }
    }

    if (!G_CanDamage(ent, inflictor)) {
      continue;
    }

    // find closest point to inflictor
    const vec3_t point = Box3_ClampPoint(ent->abs_bounds, inflictor->s.origin);

    G_Damage(&(g_damage_t) {
      .target = ent,
      .inflictor = inflictor,
      .attacker = attacker,
      .dir = dir,
      .point = point,
      .normal = dir,
      .damage = d,
      .knockback = k,
      .flags = DMG_RADIUS,
      .mod = mod
    });
  });
}
