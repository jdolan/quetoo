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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "g_local.h"
#include "bg_pmove.h"
#include <time.h>

/**
 * @brief Returns true if ent1 and ent2 are on the same team.
 */
bool G_OnSameTeam(const GameClient *a, const GameClient *b) {

  if (!a || !b) {
    return false;
  }

  if (a->persistent.spectator && b->persistent.spectator) {
    return true;
  }

  if (!gLevel.teams) {
    return false;
  }

  return a->persistent.team == b->persistent.team;
}

/**
 * @brief Returns a human-readable weapon name for a means of death, for stats recording.
 */
static const char *G_WeaponNameForMod(g_means_of_death mod) {

  switch (mod & ~MOD_FRIENDLY_FIRE) {
    case MOD_BLASTER:
      return "Blaster";
    case MOD_SHOTGUN:
      return "Shotgun";
    case MOD_SUPER_SHOTGUN:
      return "Super Shotgun";
    case MOD_MACHINEGUN:
      return "Machinegun";
    case MOD_GRENADE:
    case MOD_GRENADE_SPLASH:
      return "Grenade Launcher";
    case MOD_HANDGRENADE:
    case MOD_HANDGRENADE_SPLASH:
    case MOD_HANDGRENADE_KAMIKAZE:
    case MOD_HANDGRENADE_SUICIDE:
      return "Hand Grenade";
    case MOD_ROCKET:
    case MOD_ROCKET_SPLASH:
      return "Rocket Launcher";
    case MOD_HYPERBLASTER:
    case MOD_HYPERBLASTER_CLIMB:
      return "Hyperblaster";
    case MOD_LIGHTNING:
    case MOD_LIGHTNING_DISCHARGE:
      return "Lightning";
    case MOD_RAILGUN:
      return "Railgun";
    case MOD_BFG_LASER:
    case MOD_BFG_BLAST:
      return "BFG10K";
    case MOD_QUAKE_SHOTGUN:
      return "Quake Shotgun";
    case MOD_QUAKE_SUPER_SHOTGUN:
      return "Quake Super Shotgun";
    case MOD_QUAKE_NAILGUN:
      return "Quake Nailgun";
    case MOD_QUAKE_SUPER_NAILGUN:
      return "Quake Super Nailgun";
    case MOD_QUAKE_GRENADE:
    case MOD_QUAKE_GRENADE_SPLASH:
      return "Quake Grenade Launcher";
    case MOD_QUAKE_ROCKET:
    case MOD_QUAKE_ROCKET_SPLASH:
      return "Quake Rocket Launcher";
    case MOD_QUAKE_THUNDERBOLT:
    case MOD_QUAKE_THUNDERBOLT_DISCHARGE:
      return "Quake Thunderbolt";
    case MOD_FIREBALL:
      return "Fireball";
    case MOD_BALLISTICS_BLASTER:
    case MOD_TURRET_BLASTER:
      return "Blaster";
    case MOD_BALLISTICS_SHOTGUN:
    case MOD_TURRET_SHOTGUN:
      return "Shotgun";
    case MOD_BALLISTICS_SUPER_SHOTGUN:
    case MOD_TURRET_SUPER_SHOTGUN:
      return "Super Shotgun";
    case MOD_BALLISTICS_MACHINEGUN:
    case MOD_TURRET_MACHINEGUN:
      return "Machinegun";
    case MOD_BALLISTICS_GRENADE:
    case MOD_TURRET_GRENADE:
      return "Grenade Launcher";
    case MOD_BALLISTICS_ROCKET:
    case MOD_TURRET_ROCKET:
      return "Rocket Launcher";
    case MOD_BALLISTICS_HYPERBLASTER:
    case MOD_TURRET_HYPERBLASTER:
      return "Hyperblaster";
    case MOD_BALLISTICS_LIGHTNING:
    case MOD_TURRET_LIGHTNING:
      return "Lightning";
    case MOD_BALLISTICS_RAILGUN:
    case MOD_TURRET_RAILGUN:
      return "Railgun";
    case MOD_BALLISTICS_BFG:
    case MOD_TURRET_BFG:
      return "BFG10K";
    case MOD_BALLISTICS_QUAKE_SHOTGUN:
    case MOD_TURRET_QUAKE_SHOTGUN:
      return "Quake Shotgun";
    case MOD_BALLISTICS_QUAKE_SUPER_SHOTGUN:
    case MOD_TURRET_QUAKE_SUPER_SHOTGUN:
      return "Quake Super Shotgun";
    case MOD_BALLISTICS_QUAKE_NAILGUN:
    case MOD_TURRET_QUAKE_NAILGUN:
      return "Nailgun";
    case MOD_BALLISTICS_QUAKE_SUPER_NAILGUN:
    case MOD_TURRET_QUAKE_SUPER_NAILGUN:
      return "Super Nailgun";
    case MOD_BALLISTICS_QUAKE_GRENADE:
    case MOD_TURRET_QUAKE_GRENADE:
      return "Quake Grenade Launcher";
    case MOD_BALLISTICS_QUAKE_ROCKET:
    case MOD_TURRET_QUAKE_ROCKET:
      return "Quake Rocket Launcher";
    case MOD_BALLISTICS_QUAKE_THUNDERBOLT:
    case MOD_TURRET_QUAKE_THUNDERBOLT:
      return "Quake Thunderbolt";
    case MOD_BALLISTICS_LASER:
    case MOD_TURRET_LASER:
      return "Laser";
    case MOD_BALLISTICS_GIBLETS:
    case MOD_TURRET_GIBLETS:
      return "Giblets";
#if defined(G_HOOK)
    case MOD_HOOK:
      return "Hook";
#endif
    case MOD_TELEFRAG:
      return "Telefrag";
    case MOD_EXPLOSIVE:
      return "Explosive";
    default:
      return "Unknown";
  }
}

/**
 * @see g_combat.h
 */
bool G_CanDamage(const GameEntity *targ, const GameEntity *inflictor) {
  Vec3 dest;
  CmTrace tr;

  // BSP sub-models need special checking because their origin is 0,0,0
  if (targ->solid == SOLID_BSP) {
    dest = Box3_Center(targ->absBounds);
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
Vec3 G_GetOrigin(const GameEntity *ent) {

  if (ent->solid == SOLID_BSP) {
    return Box3_Center(ent->absBounds);
  } else {
    return ent->s.origin;
  }
}

/**
 * @brief Emits a temporary damage visual effect at the specified position and surface normal.
 */
static void G_SpawnDamage(GameTempEntity type, const Vec3 pos, const Vec3 normal, int32_t damage) {

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
static int32_t G_CheckArmor(GameEntity *ent, const Vec3 pos, const Vec3 normal, int32_t damage, uint32_t dflags) {

  if (dflags & DMG_NO_ARMOR) {
    return 0;
  }

  if (!ent->client) {
    return 0;
  }

  const GameItem *armor = G_ClientArmor(ent->client);
  const GameArmorInfo *armorInfo = G_ArmorInfo(armor);

  if (!armor) {
    return 0;
  }

  const int32_t quantity = ent->client->inventory[armor->def.tag];
  int32_t saved;

  if (dflags & DMG_ENERGY) {
    saved = Clampf(damage * armorInfo->energyProtection, 0, quantity);
  } else {
    saved = Clampf(damage * armorInfo->normalProtection, 0, quantity);
  }

  ent->client->inventory[armor->def.tag] -= saved;

  G_SpawnDamage(TE_BLOOD, pos, normal, saved);

  return saved;
}

/**
 * @brief The tail of the `G_ModifyDamage` chain, applying the quad damage
 * powerup. Never vetoes. Features holding their own modifiers install over the top.
 */
static bool G_ModifyDamage_Common(GameEntity *target, GameEntity *attacker, int32_t *damage, int32_t *knockback) {

  if (attacker->client) {
    if (attacker->client->inventory[POWERUP_QUAD]) {
      *damage *= QUAD_DAMAGE_FACTOR;
      *knockback *= QUAD_KNOCKBACK_FACTOR;
    }
  }

  return true;
}

ModifyDamage G_ModifyDamage = G_ModifyDamage_Common;

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
 *     `DAMAGE_RADIUS`        damage was indirect (from a nearby explosion)
 *     `DAMAGE_NO_ARMOR`      armor does not protect from this damage
 *     `DAMAGE_ENERGY`        damage is from an energy based weapon
 *     `DAMAGE_BULLET`        damage is from a bullet
 *     `DAMAGE_NO_PROTECTION` kills god mode, armor, everything
 *
 *   mod The means of death, used by the obituaries routine.
 */
void G_Damage(const GameDamage *dmg) {

  GameEntity *target = dmg->target;
  GameEntity *attacker = dmg->attacker ?: ge.entities[0];
	const Vec3 dir = dmg->dir;
	const Vec3 pos = dmg->point;
	const Vec3 normal = dmg->normal;
	int32_t damage = dmg->damage;
	int32_t knockback = dmg->knockback;
	int32_t dflags = dmg->flags;
	g_means_of_death mod = dmg->mod;

  assert(target);
  assert(attacker);
  assert(damage >= 0);
  assert(damage <= INT16_MAX);
  assert(knockback >= 0);
  assert(knockback <= INT16_MAX);

  if (!target->takeDamage) {
    return;
  }

  if (target->client) { // respawn protection
    if (target->client->respawnProtectionTime > gLevel.time) {
      return;
    }
  }

  if (!G_ModifyDamage(target, attacker, &damage, &knockback)) {
    return;
  }

  if (target->client && !(dflags & DMG_NO_GOD)) { // invulnerability
    if (target->client->inventory[POWERUP_INVULNERABILITY]) {
      G_MulticastSound(&(const GamePlaySound) {
        .index = gMedia.sounds.invulnerabilityProtect,
        .entity = target,
      }, MULTICAST_PHS);
      damage = 0;
      // knockback is intentionally preserved so rocket jumping still works
    }
  }

  // friendly fire avoidance
  if (target != attacker && gLevel.teams) {
    if (G_OnSameTeam(target->client, attacker->client)) {

      if (mod == MOD_TELEFRAG) { // telefrags can not be avoided
        mod |= MOD_FRIENDLY_FIRE;
      } else {
        if (g_friendlyFire->value) {
          damage *= g_friendlyFire->value;
          mod |= MOD_FRIENDLY_FIRE;
        } else {
          damage = 0;
        }
      }
    }
  }

  // there is no self damage in instagib or arena, but there is knockback
  if (target == attacker) {
    switch (gLevel.gameplay & ~GAMEPLAY_TEAMS) {
      case GAMEPLAY_INSTAGIB:
      case GAMEPLAY_ARENA:
        damage = 0;
        break;
      default:
        damage *= g_selfDamage->value;
        break;
    }
  }

  GameClient *client = target->client;

  // calculate velocity change due to knockback
  if (knockback && (target->moveType >= MOVE_TYPE_WALK)) {
    Vec3 ndir, knockbackVel, knockbackAvel;

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
      knockback *= g_selfKnockback->value;
    }

    knockbackVel = Vec3_Scale(ndir, knockback * 100.f / sqrtf(mass));

    target->velocity = Vec3_Add(target->velocity, knockbackVel);

    // apply angular velocity (rotate)
    if (client == NULL || (client->ps.pmState.flags & PMF_GIBLET)) {
      knockbackAvel = MakeVec3(knockback, knockback, knockback);
      target->avelocity = Vec3_Fmaf(target->avelocity, 100.f / mass, knockbackAvel);
    }

    if (client && target->velocity.z >= PM_STEP_HEIGHT) { // make sure the client can leave the ground
      client->ps.pmState.flags |= PMF_TIME_PUSHED;
      client->ps.pmState.time = 120;
    }
  }

  int32_t damageArmor = 0, damageHealth = 0;

  // check for god mode protection
  if ((target->flags & FL_GOD_MODE) && !(dflags & DMG_NO_GOD)) {
    damageArmor = damage;
    damageHealth = 0;
    G_SpawnDamage(TE_BLOOD, pos, normal, damage);
  } else { // or armor protection
    damageArmor = G_CheckArmor(target, pos, normal, damage, dflags);
    damageHealth = damage - damageArmor;
  }

  const bool wasDead = target->dead;

  // do the damage
  if (damageHealth && (target->health || target->dead)) {
    if (G_IsMeat(target)) {
      G_SpawnDamage(TE_BLOOD, pos, normal, damageHealth);
    } else if (dflags & DMG_BULLET) {
      G_SpawnDamage(TE_BULLET, pos, normal, damageHealth);
    } else {
      G_SpawnDamage(TE_SPARKS, pos, normal, damageHealth);
    }

#if defined(G_TECH)
    if (attacker->client && G_HasTech(attacker->client, TECH_VAMPIRE)) {
      if (!target->dead && attacker != target && !G_OnSameTeam(attacker->client, target->client)) {
        attacker->health = Minf(attacker->health + (damage * TECH_VAMPIRE_DAMAGE_FACTOR), attacker->maxHealth);
        G_PlayTechSound(attacker->client);
      }
    }
#endif

    target->health -= damageHealth;

    // for hit sound
    if (!wasDead && attacker->client && attacker->client != client) {
      attacker->client->damageInflicted += damageHealth + damageArmor;
    }

    // kill target if he has *excessive blood loss*
    if (target->health <= 0 && !G_Ai_InDeveloperMode()) {
      target->dead = true;

      if (attacker->client && target->client) {
        const bool attackerAi = attacker->client->ai != NULL;
        const bool targetAi = target->client->ai != NULL;

        if (!attackerAi || !targetAi) { // drop ai-on-ai frags
          GameFrag frag = {
            .mod = (int32_t) mod,
            .time = (uint32_t) time(NULL),
            .attackerAi = attackerAi,
            .targetAi = targetAi,
          };
          q_strlcpy(frag.level, gLevel.name, sizeof(frag.level));
          q_strlcpy(frag.attacker, attacker->client->persistent.netName, sizeof(frag.attacker));
          q_strlcpy(frag.attackerGuid, attacker->client->persistent.guid, sizeof(frag.attackerGuid));
          q_strlcpy(frag.target, target->client->persistent.netName, sizeof(frag.target));
          q_strlcpy(frag.targetGuid, target->client->persistent.guid, sizeof(frag.targetGuid));
          q_strlcpy(frag.weapon, G_WeaponNameForMod(mod), sizeof(frag.weapon));

          if (frag.attackerGuid[0] && frag.targetGuid[0]) {
            $(gLevel.frags, add, &frag);
          }
        }
      }

      if (target->Die) {
        target->Die(target, attacker, mod);
      } else {
        G_Debug("No die function for %s\n", target->classname);
      }

      return;
    }
  }

  // if the target was already dead, invoke pain (if any) and we're done
  if (wasDead) {
    if (damageHealth && target->Pain) {
      target->Pain(target, attacker, damageHealth, knockback);
    }
    return;
  }

  // invoke the pain callback
  if ((damageHealth || knockback) && target->Pain) {
    target->Pain(target, attacker, damageHealth, knockback);
  }

  // add view kick on a player this frame
  if (client) {
    client->damageArmor += damageArmor;
    client->damageHealth += damageHealth;

    float kick = (damageArmor + damageHealth) / 50.0;

    if (kick > 1.0) {
      kick = 1.0;
    }

    G_ClientDamageKick(client, dir, kick * 10.0);
  }
}

/**
 * @brief Deals damage and knockback to all damageable entities within the specified radius of the inflictor.
 */
void G_RadiusDamage(GameEntity *inflictor, GameEntity *attacker, GameEntity *ignore, int32_t damage,
                    int32_t knockback, float radius, g_means_of_death mod) {

  G_ForEachEntity(ent, {
    if (ent == ignore) {
      continue;
    }

    if (!ent->takeDamage) {
      continue;
    }

    Vec3 dir = Vec3_Subtract(ent->s.origin, inflictor->s.origin);
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
    const Vec3 point = Box3_ClampPoint(ent->absBounds, inflictor->s.origin);

    G_Damage(&(GameDamage) {
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
