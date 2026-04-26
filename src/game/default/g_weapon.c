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
 * @brief Switches the client to a new weapon, animating the deselect/select sequence.
 */
static void G_ChangeWeapon(g_client_t *cl, const g_item_t *item) {

  if (cl->weapon == NULL) {
    cl->weapon = item;

    if (item) {
      cl->entity->s.model2 = item->model_index;

      if (item->ammo) {
        cl->ammo_index = item->ammo_item->index;
      } else {
        cl->ammo_index = 0;
      }
    } else {
      cl->entity->s.model2 = 0;
    }

    cl->weapon_change_time = 0;
    return;
  }

  if (cl->weapon_change_time > g_level.time) {
    return;
  }

  cl->weapon_change_time = g_level.time + 500;

  cl->next_weapon = item;
  cl->prev_weapon = cl->weapon;

  cl->weapon_fire_time = g_level.time + 100; // enable fire
  cl->grenade_hold_time = 0; // put the pin back in

  if (cl->held_grenade) {
    G_FreeEntity(cl->held_grenade);
    cl->held_grenade = NULL;
  }

  G_SetAnimation(cl, ANIM_TORSO_DROP, true);

  G_MulticastSound(&(const g_play_sound_t) {
    .index = g_media.sounds.weapon_switch,
    .entity = cl->entity,
    .atten = SOUND_ATTEN_LINEAR
  }, MULTICAST_PHS);
}

/**
 * @brief Gives the client the weapon item if eligible, staying it in place if g_weapon_stay is set.
 * @return True if the weapon was successfully picked up, false otherwise.
 */
bool G_PickupWeapon(g_client_t *cl, g_entity_t *ent) {

  if (g_weapon_stay->integer && ent->team == NULL && cl->inventory[ent->item->index]) {
    return false;
  }

  const int16_t had_weapon = cl->inventory[ent->item->index];

  // add the weapon to inventory
  cl->inventory[ent->item->index]++;

  const g_item_t *ammo = ent->item->ammo_item;
  if (ammo) {
    const int16_t *stock = &cl->inventory[ammo->index];

    if (*stock >= ent->health) {
      G_AddAmmo(cl, ammo, ent->health / 2);
    } else {
      G_AddAmmo(cl, ammo, ent->health);
    }
  }

  // if this is an map-placed item, not a dropped one
  if (!(ent->spawn_flags & SF_ITEM_DROPPED)) {

    // if weapons stay is disabled, or this is a team weapon, setup respawn
    if (!g_weapon_stay->integer || ent->team) {
      G_SetItemRespawn(ent, SECONDS_TO_MILLIS(g_weapon_respawn_time->value));
    }
  }

  // auto-switch the weapon if applicable
  const uint16_t auto_switch = cl->persistent.auto_switch;

  if (auto_switch == 1) { // switch from blaster
    if (cl->weapon == g_media.items.weapons[WEAPON_BLASTER]) {
      G_ChangeWeapon(cl, ent->item);
    }
  } else if (auto_switch == 2) { // switch to all
    G_ChangeWeapon(cl, ent->item);
  } else if (auto_switch == 3) { // switch to new
    if (!had_weapon) {
      G_ChangeWeapon(cl, ent->item);
    }
  }

  return true;
}

/**
 * @return True if the client has both weapon and ammo, false otherwise.
 */
static bool G_HasWeapon(const g_client_t *cl, const g_item_t *weapon) {

  if (!cl->inventory[weapon->index]) {
    return false;
  }

  if (weapon->ammo &&cl->inventory[weapon->ammo_item->index] < weapon->quantity) {
    return false;
  }

  return true;
}

/**
 * @brief Selects the best available weapon for the client, preferring more powerful options.
 */
void G_UseBestWeapon(g_client_t *cl) {

  const g_item_t *item = NULL;

  if (G_HasWeapon(cl, g_media.items.weapons[WEAPON_BFG10K])) {
    item = g_media.items.weapons[WEAPON_BFG10K];
  } else if (G_HasWeapon(cl, g_media.items.weapons[WEAPON_RAILGUN])) {
    item = g_media.items.weapons[WEAPON_RAILGUN];
  } else if (G_HasWeapon(cl, g_media.items.weapons[WEAPON_LIGHTNING])) {
    item = g_media.items.weapons[WEAPON_LIGHTNING];
  } else if (G_HasWeapon(cl, g_media.items.weapons[WEAPON_HYPERBLASTER])) {
    item = g_media.items.weapons[WEAPON_HYPERBLASTER];
  } else if (G_HasWeapon(cl, g_media.items.weapons[WEAPON_ROCKET_LAUNCHER])) {
    item = g_media.items.weapons[WEAPON_ROCKET_LAUNCHER];
  } else if (G_HasWeapon(cl, g_media.items.weapons[WEAPON_GRENADE_LAUNCHER])) {
    item = g_media.items.weapons[WEAPON_GRENADE_LAUNCHER];
  } else if (G_HasWeapon(cl, g_media.items.weapons[WEAPON_HAND_GRENADE])) {
    item = g_media.items.weapons[WEAPON_HAND_GRENADE];
  } else if (G_HasWeapon(cl, g_media.items.weapons[WEAPON_MACHINEGUN])) {
    item = g_media.items.weapons[WEAPON_MACHINEGUN];
  } else if (G_HasWeapon(cl, g_media.items.weapons[WEAPON_SUPER_SHOTGUN])) {
    item = g_media.items.weapons[WEAPON_SUPER_SHOTGUN];
  } else if (G_HasWeapon(cl, g_media.items.weapons[WEAPON_SHOTGUN])) {
    item = g_media.items.weapons[WEAPON_SHOTGUN];
  } else if (G_HasWeapon(cl, g_media.items.weapons[WEAPON_BLASTER])) {
    item = g_media.items.weapons[WEAPON_BLASTER];
  }

  G_ChangeWeapon(cl, item);
}

/**
 * @brief Initiates a weapon change to @p item, or toggles back to the previous weapon if already active.
 */
void G_UseWeapon(g_client_t *cl, const g_item_t *item) {

  // see if we're already using it
  if (item == cl->weapon) {
    return;
  }

  if (item->ammo) { // ensure we have ammo

    if (!cl->inventory[item->ammo_item->index]) {
      gi.ClientPrint(cl, PRINT_HIGH, "Not enough ammo for %s\n", item->name);
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
g_entity_t *G_DropWeapon(g_client_t *cl, const g_item_t *item) {

  const g_item_t *ammo = item->ammo_item;
  const uint16_t ammo_index = ammo->index;

  g_entity_t *dropped = G_DropItem(cl, item);

  if (dropped) {
    // now adjust dropped ammo quantity to reflect what we actually had available
    if (cl->inventory[ammo_index] < ammo->quantity) {
      dropped->health = cl->inventory[ammo_index];
    }

    if (dropped->health) {
      G_AddAmmo(cl, ammo, -dropped->health);
    }
  } else {
    G_Debug("Failed to drop %s\n", item->name);
  }

  return dropped;
}

/**
 * @brief Toss the currently held weapon when dead.
 */
g_entity_t *G_TossWeapon(g_client_t *cl) {

  const g_item_t *weapon = cl->weapon;

  if (!weapon || !weapon->Drop || !weapon->ammo) { // don't drop if not holding
    return NULL;
  }

  const int16_t ammo = cl->inventory[cl->ammo_index];

  if (!ammo) { // don't drop when out of ammo
    return NULL;
  }

  g_entity_t *dropped = G_DropItem(cl, weapon);

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
static bool G_FireWeapon(g_client_t *cl) {

  const uint32_t buttons = (cl->latched_buttons | cl->buttons);

  if (!(buttons & BUTTON_ATTACK)) {
    return false;
  }

  cl->latched_buttons &= ~BUTTON_ATTACK;

  // use small epsilon for low server frame rates
  if (cl->weapon_fire_time > g_level.time + 1) {
    return false;
  }

  // determine if ammo is required, and if the quantity is sufficient
  int16_t ammo;
  if (cl->ammo_index) {
    ammo = cl->inventory[cl->ammo_index];
  } else {
    ammo = 0;
  }

  const uint16_t ammo_needed = cl->weapon->quantity;

  // if the client does not have enough ammo, change weapons
  if (cl->ammo_index && ammo < ammo_needed) {

    if (g_level.time >= cl->pain_time) { // play a click sound
      G_MulticastSound(&(const g_play_sound_t) {
        .index = g_media.sounds.weapon_no_ammo,
        .entity = cl->entity,
        .atten = SOUND_ATTEN_LINEAR
      }, MULTICAST_PHS);
      cl->pain_time = g_level.time + 1000;
    }

    G_UseBestWeapon(cl);
    return false;
  }

  // you may fire when ready, commander
  return true;
}

/**
 * @brief Plays the activation or ambient sound for the client's currently held tech powerup.
 */
void G_PlayTechSound(g_client_t *cl) {

  const g_item_t *tech = G_GetTech(cl);

  if (!tech) {
    return;
  }

  if (cl->tech_sound_time < g_level.time) {
    G_MulticastSound(&(const g_play_sound_t) {
      .index = g_media.sounds.techs[tech->tag],
      .entity = cl->entity,
      .atten = SOUND_ATTEN_LINEAR
    }, MULTICAST_PHS);
    cl->tech_sound_time = g_level.time + 1000;
  }
}

/**
 * @brief Records that the weapon was fired, decrementing ammo and advancing the attack animation.
 */
static void G_WeaponFired(g_client_t *cl, uint32_t interval, uint32_t ammo_needed) {

  // set the attack animation
  G_SetAnimation(cl, ANIM_TORSO_ATTACK1, true);

  if (G_HasTech(cl, TECH_HASTE)) {
    interval *= TECH_HASTE_FACTOR;
    G_PlayTechSound(cl);
  }

  // push the next fire time out by the interval
  cl->weapon_fire_time = g_level.time + interval;
  cl->weapon_fired_time = g_level.time;

  // and decrease their inventory
  if (g_level.gameplay != GAME_INSTAGIB) {
    if (cl->ammo_index) {
      cl->inventory[cl->ammo_index] -= ammo_needed;
    }
  }

  // play a quad damage sound if applicable
  if (cl->inventory[g_media.items.powerups[POWERUP_QUAD]->index]) {

    if (cl->quad_attack_time < g_level.time) {
      G_MulticastSound(&(const g_play_sound_t) {
        .index = g_media.sounds.quad_attack,
        .entity = cl->entity,
        .atten = SOUND_ATTEN_LINEAR
      }, MULTICAST_PHS);

      cl->quad_attack_time = g_level.time + 500;
    }
  }
}

/**
 * @brief Runs weapon state logic for the client each frame, including firing, switching, and animations.
 */
void G_ClientWeaponThink(g_client_t *cl) {

  if (cl->entity->dead) {
    return;
  }

  if (cl->persistent.spectator) {
    return;
  }

  cl->weapon_think_time = g_level.time;

  // if changing weapons, carry out the change and re-enable firing
  if (cl->weapon_change_time > g_level.time) {

    const uint32_t delta = cl->weapon_change_time - g_level.time;
    if (delta <= 250) {
      if (cl->weapon != cl->next_weapon) {
        cl->weapon = cl->next_weapon;

        const g_item_t *item = cl->weapon;
        if (item) {

          cl->entity->s.model2 = item->model_index;

          if (item->ammo) {
            cl->ammo_index = item->ammo_item->index;
          } else {
            cl->ammo_index = 0;
          }
        } else {
          cl->entity->s.model2 = 0;
        }
      }
    }
  } else {

    // if the change sequence is complete, clear the next weapon, and reset the animation
    if (G_IsAnimation(cl, ANIM_TORSO_DROP) || G_IsAnimation(cl, ANIM_TORSO_RAISE)) {

      cl->next_weapon = NULL;
      G_SetAnimation(cl, ANIM_TORSO_STAND1, false);

      // if the attack animation is complete, go back to standing
    } else if (G_IsAnimation(cl, ANIM_TORSO_ATTACK1)) {
      if (g_level.time - cl->weapon_fired_time > 400) {
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
static void G_MuzzleFlash(g_entity_t *ent, g_muzzle_flash_t flash) {

  gi.WriteByte(SV_CMD_MUZZLE_FLASH);
  gi.WriteShort(ent->s.number);
  gi.WriteByte(flash);

  gi.Multicast(ent->s.origin, MULTICAST_PHS);
}

/**
 * @brief Detach the player's hook if it's still attached.
 */
void G_HookDetach(g_client_t *cl) {

  if (!g_level.hook) {
    return;
  }

  if (!cl->hook_entity) {
    return;
  }

  // free entity
  G_FreeEntity(cl->hook_entity->target_ent);
  G_FreeEntity(cl->hook_entity);

  cl->hook_entity = NULL;

  // prevent hook spam
  if (!cl->hook_pull) {
    cl->hook_fire_time = g_level.time + SECONDS_TO_MILLIS(g_hook_refire->value);
  } else {
    // don't get hurt from sweet-ass hooking
    cl->land_time = g_level.time;
  }

  cl->hook_pull = false;

  G_MulticastSound(&(const g_play_sound_t) {
    .index = g_media.sounds.hook_detach,
    .entity = cl->entity,
    .atten = SOUND_ATTEN_LINEAR,
    .pitch = RandomRangei(-4, 5)
  }, MULTICAST_PHS);

  // see if we can backflip for style points
  if (cl->entity->in_use && cl->entity->health > 0) {

    const vec3_t velocity = Vec3(cl->entity->velocity.x, cl->entity->velocity.y, 0.0);
    const float fwd_speed = Vec3_Length(velocity) / 1.75;

    if (cl->entity->velocity.z > 50 && cl->entity->velocity.z > fwd_speed) {
      G_SetAnimation(cl, ANIM_LEGS_JUMP2, true);
    }
  }
}

/**
 * @brief Handles the firing of the hook.
 */
static void G_HookCheckFire(g_client_t *cl, const bool refire) {

  // hook can fire, see if we should
  if (!refire && !(cl->latched_buttons & BUTTON_HOOK)) {
    return;
  }

  if (!refire) {

    // use small epsilon for low server frame rates
    if (cl->hook_fire_time > g_level.time + 1) {
      return;
    }

    cl->latched_buttons &= ~BUTTON_HOOK;
  } else {

    G_HookDetach(cl);
  }

  // fire away!
  vec3_t forward, right, up, org;
  G_ClientProjectile(cl, &forward, &right, &up, &org, -1.0);

  cl->hook_pull = false;
  cl->hook_entity = G_HookProjectile(cl->entity, org, forward);

  G_MulticastSound(&(const g_play_sound_t) {
    .index = g_media.sounds.hook_fire,
    .entity = cl->entity,
    .atten = SOUND_ATTEN_LINEAR,
    .pitch = RandomRangei(-4, 5)
  }, MULTICAST_PHS);

  cl->hook_think_time = g_level.time;
}

/**
 * @brief Handles management of the hook for a given player.
 */
void G_HookThink(g_client_t *cl, const bool refire) {

  // sanity checks
  if (!g_level.hook) {
    return;
  }

  if (cl->entity->dead) {
    return;
  }

  if (cl->persistent.spectator) {
    return;
  }

  // send off to the proper sub-function

  if (refire) {
    G_HookCheckFire(cl, true);
    return;
  }

  if (cl->hook_entity) {

    const bool is_manual_hook_swing = cl->persistent.hook_style == HOOK_SWING_MANUAL;
    const bool is_holding_hook = (cl->buttons & BUTTON_HOOK);
    const bool is_pressing_hook = (cl->latched_buttons & BUTTON_HOOK);

    if ((!is_manual_hook_swing && !is_holding_hook) || (is_manual_hook_swing && is_pressing_hook)) {

      G_HookDetach(cl);

      cl->latched_buttons &= ~BUTTON_HOOK;
      cl->hook_think_time = g_level.time;
    }
  } else {
    G_HookCheckFire(cl, false);
  }
}

/**
 * @brief Fires a blaster bolt from the client's weapon.
 */
void G_FireBlaster(g_client_t *cl) {

  if (G_FireWeapon(cl)) {
    vec3_t forward, right, up, org;

    G_ClientProjectile(cl, &forward, &right, &up, &org, 1.0);

    G_BlasterProjectile(cl->entity, org, forward, g_balance_blaster_speed->integer,
      g_balance_blaster_damage->integer, g_balance_blaster_knockback->integer);

    G_MuzzleFlash(cl->entity, MZ_BLASTER);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balance_blaster_refire->value), cl->weapon->quantity);
  }
}

/**
 * @brief Fires a spread of pellets from the shotgun.
 */
void G_FireShotgun(g_client_t *cl) {

  if (G_FireWeapon(cl)) {
    vec3_t forward, right, up, org;

    G_ClientProjectile(cl, &forward, &right, &up, &org, 1.0);

    G_ShotgunProjectiles(cl->entity, org, forward, g_balance_shotgun_damage->integer,
      g_balance_shotgun_knockback->integer, g_balance_shotgun_spread_x->integer,
      g_balance_shotgun_spread_y->integer, g_balance_shotgun_pellets->integer, MOD_SHOTGUN);

    G_MuzzleFlash(cl->entity, MZ_SHOTGUN);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balance_shotgun_refire->value), cl->weapon->quantity);
  }
}

/**
 * @brief Fires a wider, denser spread of pellets from the super shotgun.
 */
void G_FireSuperShotgun(g_client_t *cl) {

  if (G_FireWeapon(cl)) {
    vec3_t forward, right, up, org;

    G_ClientProjectile(cl, &forward, &right, &up, &org, 1.0);

    G_ShotgunProjectiles(cl->entity, org, forward, g_balance_supershotgun_damage->integer,
      g_balance_supershotgun_knockback->integer, g_balance_supershotgun_spread_x->integer,
      g_balance_supershotgun_spread_y->integer, g_balance_supershotgun_pellets->integer, MOD_SUPER_SHOTGUN);

    G_MuzzleFlash(cl->entity, MZ_SUPER_SHOTGUN);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balance_supershotgun_refire->value), cl->weapon->quantity);
  }
}

/**
 * @brief Fires a single machinegun bullet with random spread.
 */
void G_FireMachinegun(g_client_t *cl) {

  if (G_FireWeapon(cl)) {
    vec3_t forward, right, up, org;

    G_ClientProjectile(cl, &forward, &right, &up, &org, 1.0);

    G_BulletProjectile(cl->entity, org, forward, g_balance_machinegun_damage->integer,
      g_balance_machinegun_knockback->integer, g_balance_machinegun_spread_x->integer,
      g_balance_machinegun_spread_y->integer, MOD_MACHINEGUN);

    G_MuzzleFlash(cl->entity, MZ_MACHINEGUN);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balance_machinegun_refire->value), cl->weapon->quantity);
  }
}

/**
 * @brief Spawns a grenade entity attached to the client that ticks until thrown or released.
 */
static void G_PullGrenadePin(g_client_t *cl) {
  g_entity_t *nade = G_AllocEntity(__func__);
  cl->held_grenade = nade;
  nade->owner = cl->entity;
  nade->solid = SOLID_NOT;
  nade->sv_flags |= SVF_NO_CLIENT;
  nade->move_type = MOVE_TYPE_NONE;
  nade->clip_mask = CONTENTS_MASK_CLIP_PROJECTILE;
  nade->take_damage = true;
  nade->Touch = G_GrenadeProjectile_Touch;
  nade->touch_time = g_level.time;
  nade->s.trail = TRAIL_GRENADE;
  nade->s.model1 = g_media.models.grenade;
  nade->s.sound = gi.SoundIndex("weapons/handgrenades/hg_tick.ogg");
  gi.LinkEntity(nade);
}

/**
 * @brief Checks button status and hold time to determine if we're still holding
 * a primed grenade
 */
static bool G_CheckGrenadeHold(g_client_t *cl, uint32_t buttons) {
  bool current_hold = buttons & BUTTON_ATTACK;

  // just pulled the pin
  if (!cl->grenade_hold_time && current_hold) {
    G_PullGrenadePin(cl);
    cl->grenade_hold_time = g_level.time;
    cl->grenade_hold_frame = g_level.frame_num;
    return true;
  }
  // already pulled the pin and holding it
  else if (cl->grenade_hold_time && current_hold) {
    return true;
  }

  return false;
}

/**
 * @brief Handles hand grenade pin-pulling, priming, and throwing logic.
 */
void G_FireHandGrenade(g_client_t *cl) {

  uint32_t buttons = (cl->latched_buttons | cl->buttons);

  // didn't touch fire button or holding a grenade
  if (!(buttons & BUTTON_ATTACK) && !cl->grenade_hold_time) {
    return;
  }

  const uint32_t nade_time = 3 * 1000; // 3 seconds before boom
  float throw_speed = 500.0; // minimum

  // use small epsilon for low server frame rates
  if (cl->weapon_fire_time > g_level.time + 1) {
    return;
  }

  int16_t ammo;
  if (cl->ammo_index) {
    ammo = cl->inventory[cl->ammo_index];
  } else {
    ammo = 0;
  }

  // override quantity needed from g_item_t since grenades are both ammo and weapon
  const uint16_t ammo_needed = 1;

  // if the client does not have enough ammo, change weapons
  if (cl->ammo_index && ammo < ammo_needed) {

    if (g_level.time >= cl->pain_time) { // play a click sound
      G_MulticastSound(&(const g_play_sound_t) {
        .index = g_media.sounds.weapon_no_ammo,
        .entity = cl->entity,
        .atten = SOUND_ATTEN_LINEAR,
      }, MULTICAST_PHS);
      
      cl->pain_time = g_level.time + 1000;
    }

    G_UseBestWeapon(cl);
    return;
  }

  // are we holding a primed grenade?
  bool holding = G_CheckGrenadeHold(cl, buttons);

  // how long have we been holding it?
  uint32_t hold_time = g_level.time - cl->grenade_hold_time;

  // continue holding if time allows
  if (holding && (int32_t)(nade_time - hold_time) > 0) {

    // play the timer sound if we're holding once every second
    if ((g_level.frame_num - cl->grenade_hold_frame) % QUETOO_TICK_RATE == 0) {
      G_MulticastSound(&(const g_play_sound_t) {
        .index = gi.SoundIndex("weapons/handgrenades/hg_clang"),
        .entity = cl->entity,
        .atten = SOUND_ATTEN_LINEAR,
      }, MULTICAST_PHS);
    }
    return;
  }

  // to tell if it went off in player's hand or not
  if (!holding) {
    cl->grenade_hold_time = 0;
  }

  // figure out how fast/far to throw
  throw_speed *= (float) hold_time / 1000;
  throw_speed = Clampf(throw_speed, 500, 1200);

  vec3_t forward, right, up, org;

  G_ClientProjectile(cl, &forward, &right, &up, &org, 1.0);
  G_HandGrenadeProjectile(
      cl->entity,             // player
      cl->held_grenade,       // the grenade
      org,                    // starting point
      forward,                // direction
      (uint32_t) throw_speed, // how fast does it fly
      120,                    // damage dealt
      120,                    // knockback
      185.0,                  // blast radius
      nade_time - hold_time   // time before explode (next think)
  );

  // play the sound if we throw it
  G_MulticastSound(&(const g_play_sound_t) {
    .index = g_media.sounds.grenade_throw,
    .entity = cl->entity,
    .atten = SOUND_ATTEN_LINEAR,
  }, MULTICAST_PHS);

  // push the next fire time out by the interval (2 secs)
  G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balance_handgrenade_refire->value), ammo_needed);

  cl->grenade_hold_time = 0;
  cl->grenade_hold_frame = 0;
  cl->held_grenade = NULL;
}

/**
 * @brief Fires a bouncing grenade from the grenade launcher.
 */
void G_FireGrenadeLauncher(g_client_t *cl) {

  if (G_FireWeapon(cl)) {
    vec3_t forward, right, up, org;

    G_ClientProjectile(cl, &forward, &right, &up, &org, 1.0);

    G_GrenadeProjectile(cl->entity, org, forward, g_balance_grenadelauncher_speed->integer,
      g_balance_grenadelauncher_damage->integer, g_balance_grenadelauncher_knockback->integer,
      g_balance_grenadelauncher_radius->value, SECONDS_TO_MILLIS(g_balance_grenadelauncher_timer->value));

    G_MuzzleFlash(cl->entity, MZ_GRENADE_LAUNCHER);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balance_grenadelauncher_refire->value), cl->weapon->quantity);
  }
}

/**
 * @brief Fires a rocket from the rocket launcher.
 */
void G_FireRocketLauncher(g_client_t *cl) {

  if (G_FireWeapon(cl)) {
    vec3_t forward, right, up, org;

    G_ClientProjectile(cl, &forward, &right, &up, &org, 1.0);

    G_RocketProjectile(cl->entity, org, forward, g_balance_rocketlauncher_speed->integer,
      g_balance_rocketlauncher_damage->integer, g_balance_rocketlauncher_knockback->integer,
      g_balance_rocketlauncher_radius->value);

    G_MuzzleFlash(cl->entity, MZ_ROCKET_LAUNCHER);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balance_rocketlauncher_refire->value), cl->weapon->quantity);
  }
}

/**
 * @brief Fires a hyperblaster bolt.
 */
void G_FireHyperblaster(g_client_t *cl) {

  if (G_FireWeapon(cl)) {
    vec3_t forward, right, up, org;

    G_ClientProjectile(cl, &forward, &right, &up, &org, 1.0);

    G_HyperblasterProjectile(cl->entity, org, forward, g_balance_hyperblaster_speed->integer,
      g_balance_hyperblaster_damage->integer, g_balance_hyperblaster_knockback->value);

    G_MuzzleFlash(cl->entity, MZ_HYPERBLASTER);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balance_hyperblaster_refire->value), cl->weapon->quantity);
  }
}

/**
 * @brief Fires a continuous lightning beam, dealing damage along its path.
 */
void G_FireLightning(g_client_t *cl) {

  if (G_FireWeapon(cl)) {
    vec3_t forward, right, up, org;

    g_entity_t *projectile = NULL;

    while ((projectile = G_Find(projectile, EOFS(classname), "G_LightningProjectile"))) {
      if (projectile->owner == cl->entity) {
        break;
      }
    }

    if (projectile == NULL) {
      G_MuzzleFlash(cl->entity, MZ_LIGHTNING);
    }

    G_ClientProjectile(cl, &forward, &right, &up, &org, 1.0);

    G_LightningProjectile(cl->entity, org, forward, g_balance_lightning_damage->integer, g_balance_lightning_knockback->integer);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balance_lightning_refire->value), cl->weapon->quantity);
  }
}

/**
 * @brief Fires a railgun slug that instantly hits the first target along its path.
 */
void G_FireRailgun(g_client_t *cl) {

  if (G_FireWeapon(cl)) {
    vec3_t forward, right, up, org;

    G_ClientProjectile(cl, &forward, &right, &up, &org, 1.0);

    const int16_t damage = (g_level.gameplay == GAME_INSTAGIB) ? 999 : g_balance_railgun_damage->integer;

    G_RailgunProjectile(cl->entity, org, forward, damage, g_balance_railgun_knockback->integer);

    G_MuzzleFlash(cl->entity, MZ_RAILGUN);

    G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balance_railgun_refire->value), cl->weapon->quantity);
  }
}

/**
 * @brief Think callback that detonates an in-flight BFG ball on expiry or target impact.
 */
static void G_FireBfg_(g_entity_t *ent) {

  g_client_t *cl = ent->owner->client;

  if (ent->owner->dead == false) {
    if (cl->weapon == g_media.items.weapons[WEAPON_BFG10K]) {
      vec3_t forward, right, up, org;

      G_ClientProjectile(cl, &forward, &right, &up, &org, 1.0);

      G_BfgProjectile(ent->owner, org, forward, g_balance_bfg_speed->integer,
        g_balance_bfg_damage->integer, g_balance_bfg_knockback->integer,
        g_balance_bfg_radius->value);

      G_MuzzleFlash(ent->owner, MZ_BFG10K);

      G_WeaponFired(cl, SECONDS_TO_MILLIS(g_balance_bfg_refire->value), cl->weapon->quantity);
    }
  }

  ent->Think = G_FreeEntity;
  ent->next_think = g_level.time + 1;
}

/**
 * @brief Fires a BFG energy ball that damages everything along its flight path.
 */
void G_FireBfg(g_client_t *cl) {

  if (G_FireWeapon(cl)) {
    cl->weapon_fire_time = g_level.time + SECONDS_TO_MILLIS(g_balance_bfg_refire->value + g_balance_bfg_prefire->value);

    g_entity_t *timer = G_AllocEntity(__func__);
    timer->owner = cl->entity;
    timer->sv_flags = SVF_NO_CLIENT;

    timer->Think = G_FireBfg_;
    timer->next_think = g_level.time + SECONDS_TO_MILLIS(g_balance_bfg_prefire->value) - QUETOO_TICK_MILLIS;

    G_MulticastSound(&(const g_play_sound_t) {
      .index = g_media.sounds.bfg_prime,
      .entity = cl->entity,
      .atten = SOUND_ATTEN_LINEAR,
    }, MULTICAST_PHS);
  }
}
