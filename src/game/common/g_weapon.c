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

/**
 * @brief Switches the client to a new weapon, animating the deselect/select sequence.
 */
static void G_ChangeWeapon(GameClient *cl, const GameItem *item) {

  if (cl->weapon == NULL) {
    cl->weapon = item;

    if (item) {
      cl->entity->s.model2 = item->modelIndex;

      if (item->def.ammo) {
        cl->ammoIndex = item->def.ammo;
      } else {
        cl->ammoIndex = 0;
      }
    } else {
      cl->entity->s.model2 = 0;
    }

    cl->weaponChangeTime = 0;
    return;
  }

  if (cl->weaponChangeTime > gLevel.time) {
    return;
  }

  cl->weaponChangeTime = gLevel.time + 500;

  cl->nextWeapon = item;
  cl->prevWeapon = cl->weapon;

  cl->weaponFireTime = gLevel.time + 100; // enable fire
  cl->grenadeHoldTime = 0; // put the pin back in

  if (cl->heldGrenade) {
    G_FreeEntity(cl->heldGrenade);
    cl->heldGrenade = NULL;
  }

  G_SetAnimation(cl, ANIM_TORSO_DROP, true);

  G_MulticastSound(&(const GamePlaySound) {
    .index = gMedia.sounds.weaponSwitch,
    .entity = cl->entity,
  }, MULTICAST_PHS);
}

/**
 * @brief Gives the client the weapon item if eligible, staying it in place if `g_weaponStay` is set.
 * @return True if the weapon was successfully picked up, false otherwise.
 */
bool G_PickupWeapon(GameClient *cl, GameEntity *ent) {

  if (g_weaponStay->integer && ent->team == NULL && cl->inventory[ent->item->def.tag]) {
    return false;
  }

  const int16_t hadWeapon = cl->inventory[ent->item->def.tag];

  // add the weapon to inventory
  cl->inventory[ent->item->def.tag]++;

  const GameItem *ammo = ent->item->def.ammo ? &gItems[ent->item->def.ammo] : NULL;
  if (ammo) {
    const int16_t *stock = &cl->inventory[ammo->def.tag];

    if (*stock >= ent->health) {
      G_AddAmmo(cl, ammo, ent->health / 2);
    } else {
      G_AddAmmo(cl, ammo, ent->health);
    }
  }

  // if this is an map-placed item, not a dropped one
  if (!(ent->spawnFlags & SF_ITEM_DROPPED)) {

    // if weapons stay is disabled, or this is a team weapon, setup respawn
    if (!g_weaponStay->integer || ent->team) {
      G_SetItemRespawn(ent, SECONDS_TO_MILLIS(g_weaponRespawnTime->value));
    }
  }

  // auto-switch the weapon if applicable
  const uint16_t autoSwitch = cl->persistent.autoSwitch;
  if (autoSwitch == 1) { // switch from starting weapon

    const GameItemTag tag = (gLevel.items == ITEMS_QUAKE)
      ? WEAPON_QUAKE_SHOTGUN
      : WEAPON_BLASTER;

    if (cl->weapon == &gItems[tag]) {
      G_ChangeWeapon(cl, ent->item);
    }
  } else if (autoSwitch == 2) { // switch to all
    G_ChangeWeapon(cl, ent->item);
  } else if (autoSwitch == 3) { // switch to new
    if (!hadWeapon) {
      G_ChangeWeapon(cl, ent->item);
    }
  }

  return true;
}

/**
 * @return True if the client has both weapon and ammo, false otherwise.
 */
static bool G_HasWeapon(const GameClient *cl, const GameItem *weapon) {

  if (!cl->inventory[weapon->def.tag]) {
    return false;
  }

  if (weapon->def.ammo && cl->inventory[weapon->def.ammo] < weapon->def.quantity) {
    return false;
  }

  return true;
}

/**
 * @brief Selects the best available weapon for the client, preferring more powerful options.
 */
void G_UseBestWeapon(GameClient *cl) {

  const GameItem *item = NULL;

  for (GameItemTag t = WEAPON_FIRST; t < WEAPON_LAST; t++) {
    const GameItem *weapon = &gItems[t];

    if (!weapon) {
      continue;
    }

    if (!G_HasWeapon(cl, weapon)) {
      continue;
    }

    if (!item || weapon->def.priority > item->def.priority) {
      item = weapon;
    }
  }

  if (item) {
    G_ChangeWeapon(cl, item);
  }
}

/**
 * @brief Initiates a weapon change to @p item, or toggles back to the previous weapon if already active.
 */
void G_UseWeapon(GameClient *cl, const GameItem *item) {

  // see if we're already using it
  if (item == cl->weapon) {
    return;
  }

  if (item->def.ammo) { // ensure we have ammo

    if (!cl->inventory[item->def.ammo]) {
      gi.ClientPrint(cl, PRINT_HIGH, "Not enough ammo for %s\n", item->def.name);
      return;
    }
  }

  // change to this weapon when down
  G_ChangeWeapon(cl, item);
}

/**
 * @brief Drop the specified weapon if the client has sufficient ammo. Does not remove quantity,
 * that must be handled by the caller.
 */
GameEntity *G_DropWeapon(GameClient *cl, const GameItem *item) {

  if (!item->def.ammo) {
    return NULL;
  }

  const GameItem *ammo = &gItems[item->def.ammo];
  const uint16_t ammoIndex = item->def.ammo;

  GameEntity *dropped = G_DropItem(cl, item);

  if (dropped) {
    // now adjust dropped ammo quantity to reflect what we actually had available
    if (cl->inventory[ammoIndex] < ammo->def.quantity) {
      dropped->health = cl->inventory[ammoIndex];
    }

    if (dropped->health) {
      G_AddAmmo(cl, ammo, -dropped->health);
    }
  } else {
    G_Debug("Failed to drop %s\n", item->def.name);
  }

  return dropped;
}

/**
 * @brief Toss the currently held weapon when dead.
 */
GameEntity *G_TossWeapon(GameClient *cl) {

  const GameItem *weapon = cl->weapon;

  if (!weapon || !weapon->Drop || !weapon->def.ammo) { // don't drop if not holding
    return NULL;
  }

  const int16_t ammo = cl->inventory[cl->ammoIndex];

  if (!ammo) { // don't drop when out of ammo
    return NULL;
  }

  GameEntity *dropped = G_DropItem(cl, weapon);

  if (dropped) {
    if (dropped->health > ammo) {
      dropped->health = ammo;
    }
  }

  return dropped;
}

/**
 * @brief Returns true if the specified client can fire their weapon, false
 * otherwise.
 */
static bool G_FireWeapon(GameClient *cl) {

  const uint32_t buttons = (cl->latchedButtons | cl->buttons);

  if (!(buttons & BUTTON_ATTACK)) {
    return false;
  }

  cl->latchedButtons &= ~BUTTON_ATTACK;

  // use small epsilon for low server frame rates
  if (cl->weaponFireTime > gLevel.time + 1) {
    return false;
  }

  // determine if ammo is required, and if the quantity is sufficient
  int16_t ammo;
  if (cl->ammoIndex) {
    ammo = cl->inventory[cl->ammoIndex];
  } else {
    ammo = 0;
  }

  const uint16_t ammoNeeded = cl->weapon->def.quantity;

  // if the client does not have enough ammo, change weapons
  if (cl->ammoIndex && ammo < ammoNeeded) {

    if (gLevel.time >= cl->painTime) { // play a click sound
      G_MulticastSound(&(const GamePlaySound) {
        .index = gMedia.sounds.weaponNoAmmo,
        .entity = cl->entity,
      }, MULTICAST_PHS);
      cl->painTime = gLevel.time + 1000;
    }

    G_UseBestWeapon(cl);
    return false;
  }

  // you may fire when ready, commander
  return true;
}

/**
 * @brief Records that the weapon was fired, decrementing ammo and advancing the attack animation.
 */
static void G_WeaponFired(GameClient *cl, uint32_t interval, uint32_t ammoNeeded) {

  // set the attack animation
  G_SetAnimation(cl, ANIM_TORSO_ATTACK1, true);

#if defined(G_TECH)
  if (G_HasTech(cl, TECH_HASTE)) {
    interval *= TECH_HASTE_FACTOR;
    G_PlayTechSound(cl);
  } else if (G_HasTech(cl, TECH_STRENGTH)) {
    G_PlayTechSound(cl);
  }
#endif

  // push the next fire time out by the interval
  cl->weaponFireTime = gLevel.time + interval;
  cl->weaponFiredTime = gLevel.time;

  // and decrease their inventory
  if ((gLevel.gameplay & ~GAMEPLAY_TEAMS) != GAMEPLAY_INSTAGIB) {
    if (cl->ammoIndex) {
      cl->inventory[cl->ammoIndex] -= ammoNeeded;
    }
  }

  // play a quad damage sound if applicable
  if (cl->inventory[POWERUP_QUAD]) {

    if (cl->quadAttackTime < gLevel.time) {
      G_MulticastSound(&(const GamePlaySound) {
        .index = gMedia.sounds.quadAttack,
        .entity = cl->entity,
      }, MULTICAST_PHS);

      cl->quadAttackTime = gLevel.time + 500;
    }
  }
}

/**
 * @brief Runs weapon state logic for the client each frame, including firing, switching, and animations.
 */
void G_ClientWeaponThink(GameClient *cl) {

  if (cl->entity->dead) {
    return;
  }

  if (cl->persistent.spectator) {
    return;
  }

  cl->weaponThinkTime = gLevel.time;

  // if changing weapons, carry out the change and re-enable firing
  if (cl->weaponChangeTime > gLevel.time) {

    const uint32_t delta = cl->weaponChangeTime - gLevel.time;
    if (delta <= 250) {
      if (cl->weapon != cl->nextWeapon) {
        cl->weapon = cl->nextWeapon;

        const GameItem *item = cl->weapon;
        if (item) {

          cl->entity->s.model2 = item->modelIndex;

          if (item->def.ammo) {
            cl->ammoIndex = item->def.ammo;
          } else {
            cl->ammoIndex = 0;
          }
        } else {
          cl->entity->s.model2 = 0;
        }
      }
    }
  } else {

    // if the change sequence is complete, clear the next weapon, and reset the animation
    if (G_IsAnimation(cl, ANIM_TORSO_DROP) || G_IsAnimation(cl, ANIM_TORSO_RAISE)) {

      cl->nextWeapon = NULL;
      G_SetAnimation(cl, ANIM_TORSO_STAND1, false);

      // if the attack animation is complete, go back to standing
    } else if (G_IsAnimation(cl, ANIM_TORSO_ATTACK1)) {
      if (gLevel.time - cl->weaponFiredTime > 400) {
        G_SetAnimation(cl, ANIM_TORSO_STAND1, false);
      }
    }

    // call active weapon think routine
    if (cl->weapon && cl->weapon->Think) {
      cl->weapon->Think(cl);
    }
  }
}

/**
 * @brief Broadcasts a muzzle flash event for the entity's current weapon.
 */
static void G_ClientMuzzleFlash(GameEntity *ent, GameMuzzleFlash flash) {

  gi.WriteByte(SV_CMD_MUZZLE_FLASH);
  gi.WriteShort(ent->s.number);
  gi.WriteByte(flash);

  gi.Multicast(ent->s.origin, MULTICAST_PHS);
}

/**
 * @brief Broadcasts a muzzle flash for a shooter the server does not transmit, such as a
 * `ballistics_*` entity. The origin and direction accompany the flash, as there is no entity for
 * the client to infer them from.
 * @param client The client whose colors the flash takes, being the operator of a `turret_*`
 * entity, or `MAX_CLIENTS` for anything firing on its own account.
 */
void G_WorldMuzzleFlash(const Vec3 org, const Vec3 dir, GameMuzzleFlash flash, uint8_t client) {

  gi.WriteByte(SV_CMD_MUZZLE_FLASH);
  gi.WriteShort(MUZZLE_FLASH_WORLD);
  gi.WriteByte(flash);
  gi.WritePosition(org);
  gi.WriteDir(dir);
  gi.WriteByte(client);

  gi.Multicast(org, MULTICAST_PHS);
}

/**
 * @brief Fires a blaster bolt from the client's weapon.
 */
void G_FireBlaster(GameClient *cl) {

  if (G_FireWeapon(cl)) {
    Vec3 forward, right, up, org;

    G_ClientProjectile(cl, &forward, &right, &up, &org, 1.0);

    G_BlasterProjectile(cl->entity, cl->entity, org, forward, g_balanceBlasterSpeed->integer,
      g_balanceBlasterDamage->integer, g_balanceBlasterKnockback->integer, MOD_BLASTER);

    G_ClientMuzzleFlash(cl->entity, MZ_BLASTER);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balanceBlasterRefire->value), cl->weapon->def.quantity);
  }
}

/**
 * @brief Fires a spread of pellets from the shotgun.
 */
void G_FireShotgun(GameClient *cl) {

  if (G_FireWeapon(cl)) {
    Vec3 forward, right, up, org;

    G_ClientProjectile(cl, &forward, &right, &up, &org, 1.0);

    G_ShotgunProjectiles(cl->entity, cl->entity, org, forward, g_balanceShotgunDamage->integer,
      g_balanceShotgunKnockback->integer, g_balanceShotgunSpreadX->integer,
      g_balanceShotgunSpreadY->integer, g_balanceShotgunPellets->integer, MOD_SHOTGUN);

    G_ClientMuzzleFlash(cl->entity, MZ_SHOTGUN);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balanceShotgunRefire->value), cl->weapon->def.quantity);
  }
}

/**
 * @brief Fires a wider, denser spread of pellets from the super shotgun.
 */
void G_FireSuperShotgun(GameClient *cl) {

  if (G_FireWeapon(cl)) {
    Vec3 forward, right, up, org;

    G_ClientProjectile(cl, &forward, &right, &up, &org, 1.0);

    G_ShotgunProjectiles(cl->entity, cl->entity, org, forward, g_balanceSupershotgunDamage->integer,
      g_balanceSupershotgunKnockback->integer, g_balanceSupershotgunSpreadX->integer,
      g_balanceSupershotgunSpreadY->integer, g_balanceSupershotgunPellets->integer, MOD_SUPER_SHOTGUN);

    G_ClientMuzzleFlash(cl->entity, MZ_SUPER_SHOTGUN);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balanceSupershotgunRefire->value), cl->weapon->def.quantity);
  }
}

/**
 * @brief Fires a single machinegun bullet with random spread.
 */
void G_FireMachinegun(GameClient *cl) {

  if (G_FireWeapon(cl)) {
    Vec3 forward, right, up, org;

    G_ClientProjectile(cl, &forward, &right, &up, &org, 1.0);

    G_BulletProjectile(cl->entity, cl->entity, org, forward, g_balanceMachinegunDamage->integer,
      g_balanceMachinegunKnockback->integer, g_balanceMachinegunSpreadX->integer,
      g_balanceMachinegunSpreadY->integer, MOD_MACHINEGUN);

    G_ClientMuzzleFlash(cl->entity, MZ_MACHINEGUN);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balanceMachinegunRefire->value), cl->weapon->def.quantity);
  }
}

/**
 * @brief Defensive no-op think callback for held grenades.
 */
static void G_HeldGrenadeThink(GameEntity *ent) {
  (void) ent;
}

/**
 * @brief Spawns a grenade entity attached to the client that ticks until thrown or released.
 */
static void G_PullGrenadePin(GameClient *cl) {
  if (cl->heldGrenade) {
    G_FreeEntity(cl->heldGrenade);
    cl->heldGrenade = NULL;
  }

  GameEntity *nade = G_AllocEntity(__func__);
  cl->heldGrenade = nade;
  nade->owner = cl->entity;
  nade->s.origin = cl->entity->s.origin;
  nade->solid = SOLID_NOT;
  nade->svFlags |= SVF_NO_CLIENT;
  nade->moveType = MOVE_TYPE_NONE;
  nade->clipMask = CONTENTS_MASK_CLIP_PROJECTILE;
  nade->takeDamage = true;
  nade->nextThink = 0;
  nade->Think = G_HeldGrenadeThink;
  nade->Touch = G_GrenadeProjectile_Touch;
  nade->touchTime = gLevel.time;
  nade->s.trail = TRAIL_GRENADE;
  nade->s.model1 = gMedia.models.grenade;
  nade->s.sound = gMedia.sounds.grenadeTick;
  gi.LinkEntity(nade);
}

/**
 * @brief Checks button status and hold time to determine if we're still holding
 * a primed grenade
 */
static bool G_CheckGrenadeHold(GameClient *cl, uint32_t buttons) {
  bool currentHold = buttons & BUTTON_ATTACK;

  // just pulled the pin
  if (!cl->grenadeHoldTime && currentHold) {
    G_PullGrenadePin(cl);
    cl->grenadeHoldTime = gLevel.time;
    cl->grenadeHoldFrame = gLevel.frameNum;
    return true;
  }
  // already pulled the pin and holding it
  else if (cl->grenadeHoldTime && currentHold) {
    return true;
  }

  return false;
}

/**
 * @brief Handles hand grenade pin-pulling, priming, and throwing logic.
 */
void G_FireHandGrenade(GameClient *cl) {

  uint32_t buttons = (cl->latchedButtons | cl->buttons);

  // didn't touch fire button or holding a grenade
  if (!(buttons & BUTTON_ATTACK) && !cl->grenadeHoldTime) {
    return;
  }

  const uint32_t nadeTime = 3 * 1000; // 3 seconds before boom
  float throwSpeed = 500.0; // minimum

  // use small epsilon for low server frame rates
  if (cl->weaponFireTime > gLevel.time + 1) {
    return;
  }

  int16_t ammo;
  if (cl->ammoIndex) {
    ammo = cl->inventory[cl->ammoIndex];
  } else {
    ammo = 0;
  }

  // override quantity needed from GameItem since grenades are both ammo and weapon
  const uint16_t ammoNeeded = 1;

  // if the client does not have enough ammo, change weapons
  if (cl->ammoIndex && ammo < ammoNeeded) {

    if (gLevel.time >= cl->painTime) { // play a click sound
      G_MulticastSound(&(const GamePlaySound) {
        .index = gMedia.sounds.weaponNoAmmo,
        .entity = cl->entity,
      }, MULTICAST_PHS);
      
      cl->painTime = gLevel.time + 1000;
    }

    G_UseBestWeapon(cl);
    return;
  }

  // are we holding a primed grenade?
  bool holding = G_CheckGrenadeHold(cl, buttons);

  // how long have we been holding it?
  uint32_t holdTime = gLevel.time - cl->grenadeHoldTime;

  // continue holding if time allows
  if (holding && (int32_t)(nadeTime - holdTime) > 0) {

    // play the timer sound if we're holding once every second
    if ((gLevel.frameNum - cl->grenadeHoldFrame) % QUETOO_TICK_RATE == 0) {
      G_MulticastSound(&(const GamePlaySound) {
        .index = gMedia.sounds.grenadeClang,
        .entity = cl->entity,
      }, MULTICAST_PHS);
    }
    return;
  }

  // to tell if it went off in player's hand or not
  if (!holding) {
    cl->grenadeHoldTime = 0;
  }

  // figure out how fast/far to throw
  throwSpeed *= (float) holdTime / 1000;
  throwSpeed = Clampf(throwSpeed, 500, 1200);
  const int32_t fuse = Clampf((int32_t) nadeTime - (int32_t) holdTime, 1, (int32_t) nadeTime);

  Vec3 forward, right, up, org;

  G_ClientProjectile(cl, &forward, &right, &up, &org, 1.0);
  G_HandGrenadeProjectile(
      cl->entity,             // player
      cl->heldGrenade,       // the grenade
      org,                    // starting point
      forward,                // direction
      (uint32_t) throwSpeed, // how fast does it fly
      120,                    // damage dealt
      120,                    // knockback
      185.0,                  // blast radius
      fuse                    // time before explode (next think)
  );

  // play the sound if we throw it
  G_MulticastSound(&(const GamePlaySound) {
    .index = gMedia.sounds.grenadeThrow,
    .entity = cl->entity,
  }, MULTICAST_PHS);

  // push the next fire time out by the interval (2 secs)
  G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balanceHandgrenadeRefire->value), ammoNeeded);

  cl->grenadeHoldTime = 0;
  cl->grenadeHoldFrame = 0;
  cl->heldGrenade = NULL;
}

/**
 * @brief Fires a bouncing grenade from the grenade launcher.
 */
void G_FireGrenadeLauncher(GameClient *cl) {

  if (G_FireWeapon(cl)) {
    Vec3 forward, right, up, org;

    G_ClientProjectile(cl, &forward, &right, &up, &org, 1.0);

    G_GrenadeProjectile(cl->entity, cl->entity, org, forward, g_balanceGrenadelauncherSpeed->integer,
      g_balanceGrenadelauncherDamage->integer, g_balanceGrenadelauncherKnockback->integer,
      g_balanceGrenadelauncherRadius->value, SECONDS_TO_MILLIS(g_balanceGrenadelauncherTimer->value), 0);

    G_ClientMuzzleFlash(cl->entity, MZ_GRENADE_LAUNCHER);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balanceGrenadelauncherRefire->value), cl->weapon->def.quantity);
  }
}

/**
 * @brief Fires a rocket from the rocket launcher.
 */
void G_FireRocketLauncher(GameClient *cl) {

  if (G_FireWeapon(cl)) {
    Vec3 forward, right, up, org;

    G_ClientProjectile(cl, &forward, &right, &up, &org, 1.0);

    G_RocketProjectile(cl->entity, cl->entity, org, forward, g_balanceRocketlauncherSpeed->integer,
      g_balanceRocketlauncherDamage->integer, g_balanceRocketlauncherKnockback->integer,
      g_balanceRocketlauncherRadius->value, 0);

    G_ClientMuzzleFlash(cl->entity, MZ_ROCKET_LAUNCHER);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balanceRocketlauncherRefire->value), cl->weapon->def.quantity);
  }
}

/**
 * @brief Fires a hyperblaster bolt.
 */
void G_FireHyperblaster(GameClient *cl) {

  if (G_FireWeapon(cl)) {
    Vec3 forward, right, up, org;

    G_ClientProjectile(cl, &forward, &right, &up, &org, 1.0);

    G_HyperblasterProjectile(cl->entity, cl->entity, org, forward, g_balanceHyperblasterSpeed->integer,
      g_balanceHyperblasterDamage->integer, g_balanceHyperblasterKnockback->value);

    G_ClientMuzzleFlash(cl->entity, MZ_HYPERBLASTER);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balanceHyperblasterRefire->value), cl->weapon->def.quantity);
  }
}

/**
 * @brief Fires a continuous lightning beam, dealing damage along its path.
 */
void G_FireLightning(GameClient *cl) {

  if (G_FireWeapon(cl)) {
    Vec3 forward, right, up, org;

    GameEntity *projectile = NULL;

    while ((projectile = G_Find(projectile, EOFS(classname), "G_LightningProjectile"))) {
      if (projectile->owner == cl->entity) {
        break;
      }
    }

    if (projectile == NULL) {
      G_ClientMuzzleFlash(cl->entity, MZ_LIGHTNING);
    }

    G_ClientProjectile(cl, &forward, &right, &up, &org, 1.0);

    G_LightningProjectile(cl->entity, org, forward, g_balanceLightningDamage->integer,
      g_balanceLightningKnockback->integer, MOD_LIGHTNING, MOD_LIGHTNING_DISCHARGE);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balanceLightningRefire->value), cl->weapon->def.quantity);
  }
}

/**
 * @brief Fires a railgun slug that instantly hits the first target along its path.
 */
void G_FireRailgun(GameClient *cl) {

  if (G_FireWeapon(cl)) {
    Vec3 forward, right, up, org;

    G_ClientProjectile(cl, &forward, &right, &up, &org, 1.0);

    const int16_t damage = ((gLevel.gameplay & ~GAMEPLAY_TEAMS) == GAMEPLAY_INSTAGIB) ? 999 : g_balanceRailgunDamage->integer;

    G_RailgunProjectile(cl->entity, cl->entity, org, forward, damage, g_balanceRailgunKnockback->integer, MOD_RAILGUN);

    G_ClientMuzzleFlash(cl->entity, MZ_RAILGUN);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balanceRailgunRefire->value), cl->weapon->def.quantity);
  }
}

/**
 * @brief Fires a wider, Quake-style super shotgun spread.
 */
/**
 * @brief Fires a spread of pellets from the Quake Shotgun.
 */
void G_FireQuakeShotgun(GameClient *cl) {

  if (G_FireWeapon(cl)) {
    Vec3 forward, right, up, org;

    G_ClientProjectile(cl, &forward, &right, &up, &org, 0.0);

    G_ShotgunProjectiles(cl->entity, cl->entity, org, forward, g_balanceQuakeShotgunDamage->integer,
      g_balanceQuakeShotgunKnockback->integer, g_balanceQuakeShotgunSpreadX->integer,
      g_balanceQuakeShotgunSpreadY->integer, g_balanceQuakeShotgunPellets->integer,
      MOD_QUAKE_SHOTGUN);

    G_ClientMuzzleFlash(cl->entity, MZ_QUAKE_SHOTGUN);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balanceQuakeShotgunRefire->value), cl->weapon->def.quantity);
  }
}

/**
 * @brief Fires a wider spread of shells from the Quake Super Shotgun.
 */
void G_FireQuakeSuperShotgun(GameClient *cl) {

  if (G_FireWeapon(cl)) {
    Vec3 forward, right, up, org;

    G_ClientProjectile(cl, &forward, &right, &up, &org, 0.0);

    G_ShotgunProjectiles(cl->entity, cl->entity, org, forward, g_balanceQuakeSupershotgunDamage->integer,
      g_balanceQuakeSupershotgunKnockback->integer, g_balanceQuakeSupershotgunSpreadX->integer,
      g_balanceQuakeSupershotgunSpreadY->integer, g_balanceQuakeSupershotgunPellets->integer,
      MOD_QUAKE_SUPER_SHOTGUN);

    G_ClientMuzzleFlash(cl->entity, MZ_QUAKE_SUPER_SHOTGUN);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balanceQuakeSupershotgunRefire->value), cl->weapon->def.quantity);
  }
}

/**
 * @brief Fires a single nail from the Quake nailgun.
 */
void G_FireQuakeNailgun(GameClient *cl) {

  if (G_FireWeapon(cl)) {
    Vec3 forward, right, up, org;

    G_ClientProjectile(cl, &forward, &right, &up, &org, 0.0);

    const float barrelOffset = (cl->quakeNailgunBarrel & 1) ? 2.0f : -2.0f;
    org = Vec3_Fmaf(org, barrelOffset, right);
    cl->quakeNailgunBarrel++;

    G_NailProjectile(cl->entity, cl->entity, org, forward, g_balanceQuakeNailgunSpeed->integer,
      g_balanceQuakeNailgunDamage->integer, g_balanceQuakeNailgunKnockback->integer, MOD_QUAKE_NAILGUN);

    G_ClientMuzzleFlash(cl->entity, MZ_QUAKE_NAILGUN);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balanceQuakeNailgunRefire->value), cl->weapon->def.quantity);
  }
}

/**
 * @brief Fires two nails per shot from the Quake super nailgun, offset from parallel barrels.
 */
void G_FireQuakeSuperNailgun(GameClient *cl) {

  if (G_FireWeapon(cl)) {
    Vec3 forward, right, up, org;

    G_ClientProjectile(cl, &forward, &right, &up, &org, 0.0);

    // Cycle through 4 barrels: up, right, down, left.
    switch (cl->quakeNailgunBarrel % 4) {
      case 0: org = Vec3_Fmaf(org,  2.0f, up);    break;
      case 1: org = Vec3_Fmaf(org,  2.0f, right); break;
      case 2: org = Vec3_Fmaf(org, -2.0f, up);    break;
      case 3: org = Vec3_Fmaf(org, -2.0f, right); break;
    }
    cl->quakeNailgunBarrel++;

    G_NailProjectile(cl->entity, cl->entity, org, forward,
      g_balanceQuakeSupernailgunSpeed->integer, g_balanceQuakeSupernailgunDamage->integer,
      g_balanceQuakeSupernailgunKnockback->integer, MOD_QUAKE_SUPER_NAILGUN);

    G_ClientMuzzleFlash(cl->entity, MZ_QUAKE_SUPER_NAILGUN);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balanceQuakeSupernailgunRefire->value), cl->weapon->def.quantity);
  }
}

/**
 * @brief Fires a bouncing grenade from the Quake grenade launcher.
 */
void G_FireQuakeGrenadeLauncher(GameClient *cl) {

  if (G_FireWeapon(cl)) {
    Vec3 forward, right, up, org;

    G_ClientProjectile(cl, &forward, &right, &up, &org, 0.0);

    G_QuakeGrenadeProjectile(cl->entity, cl->entity, org, forward, g_balanceQuakeGrenadelauncherSpeed->integer,
      g_balanceQuakeGrenadelauncherDamage->integer, g_balanceQuakeGrenadelauncherKnockback->integer,
      g_balanceQuakeGrenadelauncherRadius->value, SECONDS_TO_MILLIS(g_balanceQuakeGrenadelauncherTimer->value));

    G_ClientMuzzleFlash(cl->entity, MZ_QUAKE_GRENADE_LAUNCHER);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balanceQuakeGrenadelauncherRefire->value), cl->weapon->def.quantity);
  }
}

/**
 * @brief Fires a rocket from the Quake rocket launcher.
 */
void G_FireQuakeRocketLauncher(GameClient *cl) {

  if (G_FireWeapon(cl)) {
    Vec3 forward, right, up, org;

    G_ClientProjectile(cl, &forward, &right, &up, &org, 0.0);

    G_QuakeRocketProjectile(cl->entity, cl->entity, org, forward, g_balanceQuakeRocketlauncherSpeed->integer,
      g_balanceQuakeRocketlauncherDamage->integer, g_balanceQuakeRocketlauncherKnockback->integer,
      g_balanceQuakeRocketlauncherRadius->value);

    G_ClientMuzzleFlash(cl->entity, MZ_QUAKE_ROCKET_LAUNCHER);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balanceQuakeRocketlauncherRefire->value), cl->weapon->def.quantity);
  }
}

/**
 * @brief Fires a continuous lightning beam from the Quake thunderbolt.
 */
void G_FireQuakeThunderbolt(GameClient *cl) {

  if (G_FireWeapon(cl)) {
    Vec3 forward, right, up, org;

    GameEntity *projectile = NULL;

    while ((projectile = G_Find(projectile, EOFS(classname), "G_LightningProjectile"))) {
      if (projectile->owner == cl->entity) {
        break;
      }
    }

    if (projectile == NULL) {
      G_ClientMuzzleFlash(cl->entity, MZ_LIGHTNING);
    }

    G_ClientProjectile(cl, &forward, &right, &up, &org, 0.0);

    G_LightningProjectile(cl->entity, org, forward, g_balanceQuakeThunderboltDamage->integer,
      g_balanceQuakeThunderboltKnockback->integer, MOD_QUAKE_THUNDERBOLT, MOD_QUAKE_THUNDERBOLT_DISCHARGE);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balanceQuakeThunderboltRefire->value), cl->weapon->def.quantity);
  }
}

/**
 * @brief Think callback that detonates an in-flight BFG ball on expiry or target impact.
 */
static void G_FireBfg_(GameEntity *ent) {

  GameClient *cl = ent->owner->client;

  if (ent->owner->dead == false) {
    if (cl->weapon == &gItems[WEAPON_BFG10K]) {
      Vec3 forward, right, up, org;

      G_ClientProjectile(cl, &forward, &right, &up, &org, 1.0);

      G_BfgProjectile(ent->owner, ent->owner, org, forward, g_balanceBfgSpeed->integer,
        g_balanceBfgDamage->integer, g_balanceBfgKnockback->integer,
        g_balanceBfgRadius->value);

      G_ClientMuzzleFlash(ent->owner, MZ_BFG10K);

      G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balanceBfgRefire->value), cl->weapon->def.quantity);
    }
  }

  ent->Think = G_FreeEntity;
  ent->nextThink = gLevel.time + 1;
}

/**
 * @brief Fires a BFG energy ball that damages everything along its flight path.
 */
void G_FireBfg(GameClient *cl) {

  if (G_FireWeapon(cl)) {
    cl->weaponFireTime = gLevel.time + SECONDS_TO_MILLIS(g_balanceBfgRefire->value + g_balanceBfgPrefire->value);

    GameEntity *timer = G_AllocEntity(__func__);
    timer->owner = cl->entity;
    timer->svFlags = SVF_NO_CLIENT;

    timer->Think = G_FireBfg_;
    timer->nextThink = gLevel.time + SECONDS_TO_MILLIS(g_balanceBfgPrefire->value) - QUETOO_TICK_MILLIS;

    G_MulticastSound(&(const GamePlaySound) {
      .index = gMedia.sounds.bfgPrime,
      .entity = cl->entity,
    }, MULTICAST_PHS);
  }
}
