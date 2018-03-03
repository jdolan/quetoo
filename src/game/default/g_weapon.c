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
 * @brief
 */
static void G_ChangeWeapon(g_entity_t *ent, const g_item_t *item) {

	if (ent->client->locals.weapon == NULL) {
		ent->client->locals.weapon = item;

		if (item) {

			ent->s.model2 = item->model_index;

			if (item->ammo) {
				ent->client->locals.ammo_index = item->ammo_item->index;
			} else {
				ent->client->locals.ammo_index = 0;
			}
		} else {
			ent->s.model2 = 0;
		}

		ent->client->locals.weapon_change_time = 0;
		return;
	}

	if (ent->client->locals.weapon_change_time > g_level.time) {
		return;
	}

	ent->client->locals.weapon_change_time = g_level.time + 500;

	ent->client->locals.next_weapon = item;
	ent->client->locals.prev_weapon = ent->client->locals.weapon;

	ent->client->locals.weapon_fire_time = g_level.time + 100; // enable fire
	ent->client->locals.grenade_hold_time = 0; // put the pin back in

	G_SetAnimation(ent, ANIM_TORSO_DROP, true);

	gi.Sound(ent, g_media.sounds.weapon_switch, ATTEN_NORM | S_SET_Z_ORIGIN_OFFSET(3), 0);
}

/**
 * @brief
 */
_Bool G_PickupWeapon(g_entity_t *ent, g_entity_t *other) {

	if (g_weapon_stay->integer && other->client->locals.inventory[ent->locals.item->index]) {
		return false;
	}

	const int16_t had_weapon = other->client->locals.inventory[ent->locals.item->index];

	// add the weapon to inventory
	other->client->locals.inventory[ent->locals.item->index]++;

	const g_item_t *ammo = ent->locals.item->ammo_item;
	if (ammo) {
		const int16_t *stock = &other->client->locals.inventory[ammo->index];

		if (*stock >= ent->locals.health) {
			G_AddAmmo(other, ammo, ent->locals.health / 2);
		} else {
			G_AddAmmo(other, ammo, ent->locals.health);
		}
	}

	// setup respawn if it's not a dropped item
	if (!(ent->locals.spawn_flags & SF_ITEM_DROPPED) && !g_weapon_stay->integer) {
		G_SetItemRespawn(ent, SECONDS_TO_MILLIS(g_weapon_respawn_time->value));
	}

	// auto-switch the weapon if applicable
	const uint16_t auto_switch = other->client->locals.persistent.auto_switch;

	if (auto_switch == 1) { // switch from blaster
		if (other->client->locals.weapon == g_media.items.weapons[WEAPON_BLASTER]) {
			G_ChangeWeapon(other, ent->locals.item);
		}
	} else if (auto_switch == 2) { // switch to all
		G_ChangeWeapon(other, ent->locals.item);
	} else if (auto_switch == 3) { // switch to new
		if (!had_weapon) {
			G_ChangeWeapon(other, ent->locals.item);
		}
	}

	return true;
}

/**
 * @return True if the client has both weapon and ammo, false otherwise.
 */
static _Bool G_HasWeapon(const g_entity_t *ent, const g_item_t *weapon) {

	if (!ent->client->locals.inventory[weapon->index]) {
		return false;
	}

	if (weapon->ammo && ent->client->locals.inventory[weapon->ammo_item->index] < weapon->quantity) {
		return false;
	}

	return true;
}

/**
 * @brief
 */
void G_UseBestWeapon(g_entity_t *ent) {

	const g_item_t *item = NULL;

	if (G_HasWeapon(ent, g_media.items.weapons[WEAPON_BFG10K])) {
		item = g_media.items.weapons[WEAPON_BFG10K];
	} else if (G_HasWeapon(ent, g_media.items.weapons[WEAPON_RAILGUN])) {
		item = g_media.items.weapons[WEAPON_RAILGUN];
	} else if (G_HasWeapon(ent, g_media.items.weapons[WEAPON_LIGHTNING])) {
		item = g_media.items.weapons[WEAPON_LIGHTNING];
	} else if (G_HasWeapon(ent, g_media.items.weapons[WEAPON_HYPERBLASTER])) {
		item = g_media.items.weapons[WEAPON_HYPERBLASTER];
	} else if (G_HasWeapon(ent, g_media.items.weapons[WEAPON_ROCKET_LAUNCHER])) {
		item = g_media.items.weapons[WEAPON_ROCKET_LAUNCHER];
	} else if (G_HasWeapon(ent, g_media.items.weapons[WEAPON_GRENADE_LAUNCHER])) {
		item = g_media.items.weapons[WEAPON_GRENADE_LAUNCHER];
	} else if (G_HasWeapon(ent, g_media.items.weapons[WEAPON_HAND_GRENADE])) {
		item = g_media.items.weapons[WEAPON_HAND_GRENADE];
	} else if (G_HasWeapon(ent, g_media.items.weapons[WEAPON_MACHINEGUN])) {
		item = g_media.items.weapons[WEAPON_MACHINEGUN];
	} else if (G_HasWeapon(ent, g_media.items.weapons[WEAPON_SUPER_SHOTGUN])) {
		item = g_media.items.weapons[WEAPON_SUPER_SHOTGUN];
	} else if (G_HasWeapon(ent, g_media.items.weapons[WEAPON_SHOTGUN])) {
		item = g_media.items.weapons[WEAPON_SHOTGUN];
	} else if (G_HasWeapon(ent, g_media.items.weapons[WEAPON_BLASTER])) {
		item = g_media.items.weapons[WEAPON_BLASTER];
	}

	G_ChangeWeapon(ent, item);
}

/**
 * @brief
 */
void G_UseWeapon(g_entity_t *ent, const g_item_t *item) {

	// see if we're already using it
	if (item == ent->client->locals.weapon) {
		return;
	}

	if (item->ammo) { // ensure we have ammo

		if (!ent->client->locals.inventory[item->ammo_item->index]) {
			gi.ClientPrint(ent, PRINT_HIGH, "Not enough ammo for %s\n", item->name);
			return;
		}
	}

	// change to this weapon when down
	G_ChangeWeapon(ent, item);
}

/**
 * @brief Drop the specified weapon if the client has sufficient ammo. Does not remove quantity,
 * that must be handled by the caller.
 */
g_entity_t *G_DropWeapon(g_entity_t *ent, const g_item_t *item) {
	g_client_locals_t *cl = &ent->client->locals;

	const g_item_t *ammo = item->ammo_item;
	const uint16_t ammo_index = ammo->index;

	g_entity_t *dropped = G_DropItem(ent, item);

	if (dropped) {
		// now adjust dropped ammo quantity to reflect what we actually had available
		if (cl->inventory[ammo_index] < ammo->quantity) {
			dropped->locals.health = cl->inventory[ammo_index];
		}

		if (dropped->locals.health) {
			G_AddAmmo(ent, ammo, -dropped->locals.health);
		}
	} else {
		gi.Debug("Failed to drop %s\n", item->name);
	}

	return dropped;
}

/**
 * @brief Toss the currently held weapon when dead.
 */
g_entity_t *G_TossWeapon(g_entity_t *ent) {

	const g_item_t *weapon = ent->client->locals.weapon;

	if (!weapon || !weapon->Drop || !weapon->ammo) { // don't drop if not holding
		return NULL;
	}

	const int16_t ammo = ent->client->locals.inventory[ent->client->locals.ammo_index];

	if (!ammo) { // don't drop when out of ammo
		return NULL;
	}

	g_entity_t *dropped = G_DropItem(ent, ent->client->locals.weapon);

	if (dropped) {
		if (dropped->locals.health > ammo) {
			dropped->locals.health = ammo;
		}
	}

	return dropped;
}

/**
 * @brief Returns true if the specified client can fire their weapon, false
 * otherwise.
 */
static _Bool G_FireWeapon(g_entity_t *ent) {

	const uint32_t buttons = (ent->client->locals.latched_buttons | ent->client->locals.buttons);

	if (!(buttons & BUTTON_ATTACK)) {
		return false;
	}

	ent->client->locals.latched_buttons &= ~BUTTON_ATTACK;

	// use small epsilon for low server frame rates
	if (ent->client->locals.weapon_fire_time > g_level.time + 1) {
		return false;
	}

	// determine if ammo is required, and if the quantity is sufficient
	int16_t ammo;
	if (ent->client->locals.ammo_index) {
		ammo = ent->client->locals.inventory[ent->client->locals.ammo_index];
	} else {
		ammo = 0;
	}

	const uint16_t ammo_needed = ent->client->locals.weapon->quantity;

	// if the client does not have enough ammo, change weapons
	if (ent->client->locals.ammo_index && ammo < ammo_needed) {

		if (g_level.time >= ent->client->locals.pain_time) { // play a click sound
			gi.Sound(ent, g_media.sounds.weapon_no_ammo, ATTEN_NORM | S_SET_Z_ORIGIN_OFFSET(3), 0);
			ent->client->locals.pain_time = g_level.time + 1000;
		}

		G_UseBestWeapon(ent);
		return false;
	}

	// you may fire when ready, commander
	return true;
}

/**
 * @brief
 */
void G_PlayTechSound(g_entity_t *ent) {
	const g_item_t *tech = G_CarryingTech(ent);

	if (!tech) {
		return;
	}

	if (ent->client->locals.tech_sound_time < g_level.time) {
		gi.Sound(ent, g_media.sounds.techs[tech->tag], ATTEN_NORM | S_SET_Z_ORIGIN_OFFSET(3), 0);
		ent->client->locals.tech_sound_time = g_level.time + 1000;
	}
}

/**
 * @brief
 */
static void G_WeaponFired(g_entity_t *ent, uint32_t interval, uint32_t ammo_needed) {

	// set the attack animation
	G_SetAnimation(ent, ANIM_TORSO_ATTACK1, true);

	if (G_HasTech(ent, TECH_HASTE)) {
		interval *= TECH_HASTE_FACTOR;
		G_PlayTechSound(ent);
	}

	// push the next fire time out by the interval
	ent->client->locals.weapon_fire_time = g_level.time + interval;
	ent->client->locals.weapon_fired_time = g_level.time;

	// and decrease their inventory
	if (g_level.gameplay != GAME_INSTAGIB) {
		if (ent->client->locals.ammo_index) {
			ent->client->locals.inventory[ent->client->locals.ammo_index] -=
			    ammo_needed;
		}
	}

	// play a quad damage sound if applicable
	if (ent->client->locals.inventory[g_media.items.powerups[POWERUP_QUAD]->index]) {

		if (ent->client->locals.quad_attack_time < g_level.time) {
			gi.Sound(ent, g_media.sounds.quad_attack, ATTEN_NORM | S_SET_Z_ORIGIN_OFFSET(3), 0);
			ent->client->locals.quad_attack_time = g_level.time + 500;
		}
	}
}

/**
 * @brief
 */
void G_ClientWeaponThink(g_entity_t *ent) {

	if (ent->locals.dead) {
		return;
	}

	if (ent->client->locals.persistent.spectator) {
		return;
	}

	ent->client->locals.weapon_think_time = g_level.time;

	// if changing weapons, carry out the change and re-enable firing
	if (ent->client->locals.weapon_change_time > g_level.time) {

		const uint32_t delta = ent->client->locals.weapon_change_time - g_level.time;
		if (delta <= 250) {
			if (ent->client->locals.weapon != ent->client->locals.next_weapon) {
				ent->client->locals.weapon = ent->client->locals.next_weapon;

				const g_item_t *item = ent->client->locals.weapon;
				if (item) {

					ent->s.model2 = item->model_index;

					if (item->ammo) {
						ent->client->locals.ammo_index = item->ammo_item->index;
					} else {
						ent->client->locals.ammo_index = 0;
					}
				} else {
					ent->s.model2 = 0;
				}
			}
		}
	} else {

		// if the change sequence is complete, clear the next weapon, and reset the animation
		if (G_IsAnimation(ent, ANIM_TORSO_DROP) || G_IsAnimation(ent, ANIM_TORSO_RAISE)) {

			ent->client->locals.next_weapon = NULL;
			G_SetAnimation(ent, ANIM_TORSO_STAND1, false);

			// if the attack animation is complete, go back to standing
		} else if (G_IsAnimation(ent, ANIM_TORSO_ATTACK1)) {
			if (g_level.time - ent->client->locals.weapon_fired_time > 400) {
				G_SetAnimation(ent, ANIM_TORSO_STAND1, false);
			}
		}

		// call active weapon think routine
		if (ent->client->locals.weapon && ent->client->locals.weapon->Think) {
			ent->client->locals.weapon->Think(ent);
		}
	}
}

/**
 * @brief
 */
static void G_MuzzleFlash(g_entity_t *ent, g_muzzle_flash_t flash) {

	gi.WriteByte(SV_CMD_MUZZLE_FLASH);
	gi.WriteShort((int32_t) (ptrdiff_t) (ent - g_game.entities));
	gi.WriteByte(flash);

	if (flash == MZ_BLASTER) {
		gi.WriteByte(ent->s.number);
	}

	gi.Multicast(ent->s.origin, MULTICAST_PHS, NULL);
}

/**
 * @brief Detach the player's hook if it's still attached.
 */
void G_ClientHookDetach(g_entity_t *ent) {

	if (!g_level.hook_allowed) {
		return;
	}

	if (!ent->client->locals.hook_entity) {
		return;
	}

	// free entity
	G_FreeEntity(ent->client->locals.hook_entity->locals.target_ent);
	G_FreeEntity(ent->client->locals.hook_entity);

	ent->client->locals.hook_entity = NULL;

	// prevent hook spam
	if (!ent->client->locals.hook_pull) {
		ent->client->locals.hook_fire_time = g_level.time + SECONDS_TO_MILLIS(g_hook_refire->value);
	} else {
		// don't get hurt from sweet-ass hooking
		ent->client->locals.land_time = g_level.time;
	}

	ent->client->locals.hook_pull = false;

	gi.Sound(ent, g_media.sounds.hook_detach, ATTEN_NORM, (int8_t) (Randomc() * 4.0));

	// see if we can backflip for style
	if (ent->in_use && ent->locals.health > 0) {

		const vec3_t velocity = { ent->locals.velocity[0], ent->locals.velocity[1], 0.0 };
		const vec_t fwd_speed = VectorLength(velocity) / 1.75;

		if (ent->locals.velocity[2] > 50 &&
		        ent->locals.velocity[2] > fwd_speed) {
			G_SetAnimation(ent, ANIM_LEGS_JUMP2, true);
		}
	}
}

/**
 * @brief Handles the firing of the hook.
 */
static void G_ClientHookCheckFire(g_entity_t *ent, const _Bool refire) {

	// hook can fire, see if we should
	if (!refire && !(ent->client->locals.latched_buttons & BUTTON_HOOK)) {
		return;
	}

	if (!refire) {

		// use small epsilon for low server frame rates
		if (ent->client->locals.hook_fire_time > g_level.time + 1) {
			return;
		}

		ent->client->locals.latched_buttons &= ~BUTTON_HOOK;
	} else {

		G_ClientHookDetach(ent);
	}

	// fire away!
	vec3_t forward, right, up, org;
	G_InitProjectile(ent, forward, right, up, org, -1.0);

	ent->client->locals.hook_pull = false;
	ent->client->locals.hook_entity = G_HookProjectile(ent, org, forward);

	gi.Sound(ent, g_media.sounds.hook_fire, ATTEN_NORM | S_SET_Z_ORIGIN_OFFSET(3), (int8_t) (Randomc() * 4.0));
	ent->client->locals.hook_think_time = g_level.time;
}

/**
 * @brief Handles management of the hook for a given player.
 */
void G_ClientHookThink(g_entity_t *ent, const _Bool refire) {

	// sanity checks
	if (!g_level.hook_allowed) {
		return;
	}

	if (ent->locals.dead) {
		return;
	}

	if (ent->client->locals.persistent.spectator) {
		return;
	}

	// send off to the proper sub-function

	if (refire) {

		G_ClientHookCheckFire(ent, true);

		return;
	}

	if (ent->client->locals.hook_entity) {

		if ((ent->client->locals.persistent.hook_style == HOOK_PULL && !(ent->client->locals.buttons & BUTTON_HOOK)) ||
		        (ent->client->locals.persistent.hook_style == HOOK_SWING && (ent->client->locals.latched_buttons & BUTTON_HOOK))) {

			G_ClientHookDetach(ent);
			ent->client->locals.latched_buttons &= ~BUTTON_HOOK;
			ent->client->locals.hook_think_time = g_level.time;
		}
	} else {

		G_ClientHookCheckFire(ent, false);
	}
}

/**
 * @brief
 */
void G_FireBlaster(g_entity_t *ent) {

	if (G_FireWeapon(ent)) {
		vec3_t forward, right, up, org;

		G_InitProjectile(ent, forward, right, up, org, 1.0);

		G_BlasterProjectile(ent, org, forward, g_balance_blaster_speed->integer,
			g_balance_blaster_damage->integer, g_balance_blaster_knockback->integer);

		G_MuzzleFlash(ent, MZ_BLASTER);

		G_WeaponFired(ent, SECONDS_TO_MILLIS(g_balance_blaster_refire->value), ent->client->locals.weapon->quantity);
	}
}

/**
 * @brief
 */
void G_FireShotgun(g_entity_t *ent) {

	if (G_FireWeapon(ent)) {
		vec3_t forward, right, up, org;

		G_InitProjectile(ent, forward, right, up, org, 1.0);

		G_ShotgunProjectiles(ent, org, forward, g_balance_shotgun_damage->integer,
			g_balance_shotgun_knockback->integer, g_balance_shotgun_spread_x->integer,
			g_balance_shotgun_spread_y->integer, g_balance_shotgun_pellets->integer, MOD_SHOTGUN);

		G_MuzzleFlash(ent, MZ_SHOTGUN);

		G_WeaponFired(ent, SECONDS_TO_MILLIS(g_balance_shotgun_refire->value), ent->client->locals.weapon->quantity);
	}
}

/**
 * @brief
 */
void G_FireSuperShotgun(g_entity_t *ent) {

	if (G_FireWeapon(ent)) {
		vec3_t forward, right, up, org;

		G_InitProjectile(ent, forward, right, up, org, 1.0);

		G_ShotgunProjectiles(ent, org, forward, g_balance_supershotgun_damage->integer,
			g_balance_supershotgun_knockback->integer, g_balance_supershotgun_spread_x->integer,
			g_balance_supershotgun_spread_y->integer, g_balance_supershotgun_pellets->integer, MOD_SUPER_SHOTGUN);

		G_MuzzleFlash(ent, MZ_SUPER_SHOTGUN);

		G_WeaponFired(ent, SECONDS_TO_MILLIS(g_balance_supershotgun_refire->value), ent->client->locals.weapon->quantity);
	}
}

/**
 * @brief
 */
void G_FireMachinegun(g_entity_t *ent) {

	if (G_FireWeapon(ent)) {
		vec3_t forward, right, up, org;

		G_InitProjectile(ent, forward, right, up, org, 1.0);

		G_BulletProjectile(ent, org, forward, g_balance_machinegun_damage->integer,
			g_balance_machinegun_knockback->integer, g_balance_machinegun_spread_x->integer,
			g_balance_machinegun_spread_y->integer, MOD_MACHINEGUN);

		G_MuzzleFlash(ent, MZ_MACHINEGUN);

		G_WeaponFired(ent, SECONDS_TO_MILLIS(g_balance_machinegun_refire->value), ent->client->locals.weapon->quantity);
	}
}

/*
 *  Create a grenade entity that will follow the player
 *  while playing the ticking sound
 */
static void G_PullGrenadePin(g_entity_t *ent) {
	g_entity_t *nade = G_AllocEntity();
	ent->client->locals.held_grenade = nade;
	nade->owner = ent;
	nade->solid = SOLID_NOT;
	nade->sv_flags |= SVF_NO_CLIENT;
	nade->locals.move_type = MOVE_TYPE_NONE;
	nade->locals.clip_mask = MASK_CLIP_PROJECTILE;
	nade->locals.take_damage = true;
	nade->locals.Touch = G_GrenadeProjectile_Touch;
	nade->locals.touch_time = g_level.time;
	nade->s.trail = TRAIL_GRENADE;
	nade->s.model1 = g_media.models.grenade;
	nade->s.sound = gi.SoundIndex("weapons/handgrenades/hg_tick.ogg");
	gi.LinkEntity(nade);
}

/**
 * @brief Checks button status and hold time to determine if we're still holding
 * a primed grenade
 */
static _Bool G_CheckGrenadeHold(g_entity_t *ent, uint32_t buttons) {
	_Bool current_hold = buttons & BUTTON_ATTACK;

	// just pulled the pin
	if (!ent->client->locals.grenade_hold_time && current_hold) {
		G_PullGrenadePin(ent);
		ent->client->locals.grenade_hold_time = g_level.time;
		ent->client->locals.grenade_hold_frame = g_level.frame_num;
		return true;
	}
	// already pulled the pin and holding it
	else if (ent->client->locals.grenade_hold_time && current_hold) {
		return true;
	}

	return false;
}

/**
 * @brief
 */
void G_FireHandGrenade(g_entity_t *ent) {

	uint32_t buttons = (ent->client->locals.latched_buttons | ent->client->locals.buttons);

	// didn't touch fire button or holding a grenade
	if (!(buttons & BUTTON_ATTACK) && !ent->client->locals.grenade_hold_time) {
		return;
	}

	const uint32_t nade_time = 3 * 1000; // 3 seconds before boom
	vec_t throw_speed = 500.0; // minimum

	// use small epsilon for low server frame rates
	if (ent->client->locals.weapon_fire_time > g_level.time + 1) {
		return;
	}

	int16_t ammo;
	if (ent->client->locals.ammo_index) {
		ammo = ent->client->locals.inventory[ent->client->locals.ammo_index];
	} else {
		ammo = 0;
	}

	// override quantity needed from g_item_t since grenades are both ammo and weapon
	const uint16_t ammo_needed = 1;

	// if the client does not have enough ammo, change weapons
	if (ent->client->locals.ammo_index && ammo < ammo_needed) {

		if (g_level.time >= ent->client->locals.pain_time) { // play a click sound
			gi.Sound(ent, g_media.sounds.weapon_no_ammo, ATTEN_NORM, 0);
			ent->client->locals.pain_time = g_level.time + 1000;
		}

		G_UseBestWeapon(ent);
		return;
	}

	// are we holding a primed grenade?
	_Bool holding = G_CheckGrenadeHold(ent, buttons);

	// how long have we been holding it?
	uint32_t hold_time = g_level.time - ent->client->locals.grenade_hold_time;

	// continue holding if time allows
	if (holding && (int32_t)(nade_time - hold_time) > 0) {

		// play the timer sound if we're holding once every second
		if ((g_level.frame_num - ent->client->locals.grenade_hold_frame) % QUETOO_TICK_RATE == 0) {
			gi.Sound(ent, gi.SoundIndex("weapons/handgrenades/hg_clang.ogg"), ATTEN_NORM | S_SET_Z_ORIGIN_OFFSET(3), 0);
		}
		return;
	}

	// to tell if it went off in player's hand or not
	if (!holding) {
		ent->client->locals.grenade_hold_time = 0;
	}

	// figure out how fast/far to throw
	throw_speed *= (vec_t) hold_time / 1000;
	throw_speed = Clamp(throw_speed, 500, 1200);

	vec3_t forward, right, up, org;

	G_InitProjectile(ent, forward, right, up, org, 1.0);
	G_HandGrenadeProjectile(
	    ent,					// player
	    ent->client->locals.held_grenade, // the grenade
	    org,					// starting point
	    forward,				// direction
	    (uint32_t) throw_speed,	// how fast does it fly
	    120,					// damage dealt
	    120,					// knockback
	    185.0,					// blast radius
	    nade_time - hold_time	// time before explode (next think)
	);

	// play the sound if we throw it
	gi.Sound(ent, g_media.sounds.grenade_throw, ATTEN_NORM | S_SET_Z_ORIGIN_OFFSET(3), 0);

	// push the next fire time out by the interval (2 secs)
	G_WeaponFired(ent, SECONDS_TO_MILLIS(g_balance_handgrenade_refire->value), ammo_needed);

	ent->client->locals.grenade_hold_time = 0;
	ent->client->locals.grenade_hold_frame = 0;
}

/**
 * @brief
 */
void G_FireGrenadeLauncher(g_entity_t *ent) {

	if (G_FireWeapon(ent)) {
		vec3_t forward, right, up, org;

		G_InitProjectile(ent, forward, right, up, org, 1.0);

		G_GrenadeProjectile(ent, org, forward, g_balance_grenadelauncher_speed->integer,
			g_balance_grenadelauncher_damage->integer, g_balance_grenadelauncher_knockback->integer,
			g_balance_grenadelauncher_radius->value, SECONDS_TO_MILLIS(g_balance_grenadelauncher_timer->value));

		G_MuzzleFlash(ent, MZ_GRENADE_LAUNCHER);

		G_WeaponFired(ent, SECONDS_TO_MILLIS(g_balance_grenadelauncher_refire->value), ent->client->locals.weapon->quantity);
	}
}

/**
 * @brief
 */
void G_FireRocketLauncher(g_entity_t *ent) {

	if (G_FireWeapon(ent)) {
		vec3_t forward, right, up, org;

		G_InitProjectile(ent, forward, right, up, org, 1.0);

		G_RocketProjectile(ent, org, forward, g_balance_rocketlauncher_speed->integer,
			g_balance_rocketlauncher_damage->integer, g_balance_rocketlauncher_knockback->integer,
			g_balance_rocketlauncher_radius->integer);

		G_MuzzleFlash(ent, MZ_ROCKET_LAUNCHER);

		G_WeaponFired(ent, SECONDS_TO_MILLIS(g_balance_rocketlauncher_refire->value), ent->client->locals.weapon->quantity);
	}
}

/**
 * @brief
 */
void G_FireHyperblaster(g_entity_t *ent) {

	if (G_FireWeapon(ent)) {
		vec3_t forward, right, up, org;

		G_InitProjectile(ent, forward, right, up, org, 1.0);

		G_HyperblasterProjectile(ent, org, forward, 1800, 16, 4);

		G_MuzzleFlash(ent, MZ_HYPERBLASTER);

		G_WeaponFired(ent, SECONDS_TO_MILLIS(g_balance_hyperblaster_refire->value), ent->client->locals.weapon->quantity);
	}
}

/**
 * @brief
 */
void G_FireLightning(g_entity_t *ent) {

	if (G_FireWeapon(ent)) {
		vec3_t forward, right, up, org;

		g_entity_t *projectile = NULL;

		while ((projectile = G_Find(projectile, EOFS(class_name), "G_LightningProjectile"))) {
			if (projectile->owner == ent) {
				break;
			}
		}

		if (projectile == NULL) {
			G_MuzzleFlash(ent, MZ_LIGHTNING);
		}

		G_InitProjectile(ent, forward, right, up, org, 1.0);

		G_LightningProjectile(ent, org, forward, 12, 6);

		G_WeaponFired(ent, SECONDS_TO_MILLIS(g_balance_lightning_refire->value), ent->client->locals.weapon->quantity);
	}
}

/**
 * @brief
 */
void G_FireRailgun(g_entity_t *ent) {

	if (G_FireWeapon(ent)) {
		vec3_t forward, right, up, org;

		G_InitProjectile(ent, forward, right, up, org, 1.0);

		const int16_t damage = (g_level.gameplay == GAME_INSTAGIB) ? 999 : 100;

		G_RailgunProjectile(ent, org, forward, damage, 80);

		G_MuzzleFlash(ent, MZ_RAILGUN);

		G_WeaponFired(ent, SECONDS_TO_MILLIS(g_balance_railgun_refire->value), ent->client->locals.weapon->quantity);
	}
}

/**
 * @brief
 */
static void G_FireBfg_(g_entity_t *ent) {

	if (ent->owner->locals.dead == false) {
		if (ent->owner->client->locals.weapon == g_media.items.weapons[WEAPON_BFG10K]) {
			vec3_t forward, right, up, org;

			G_InitProjectile(ent->owner, forward, right, up, org, 1.0);

			G_BfgProjectile(ent->owner, org, forward, 720, 180, 140, 512.0);

			G_MuzzleFlash(ent->owner, MZ_BFG10K);

			G_WeaponFired(ent->owner, SECONDS_TO_MILLIS(g_balance_bfg_refire->value), ent->owner->client->locals.weapon->quantity);
		}
	}

	ent->locals.Think = G_FreeEntity;
	ent->locals.next_think = g_level.time + 1;
}

/**
 * @brief
 */
void G_FireBfg(g_entity_t *ent) {

	if (G_FireWeapon(ent)) {
		ent->client->locals.weapon_fire_time = g_level.time + SECONDS_TO_MILLIS(g_balance_bfg_refire->value + g_balance_bfg_prefire->value);

		g_entity_t *timer = G_AllocEntity();
		timer->owner = ent;
		timer->sv_flags = SVF_NO_CLIENT;

		timer->locals.Think = G_FireBfg_;
		timer->locals.next_think = g_level.time + SECONDS_TO_MILLIS(g_balance_bfg_prefire->value) - QUETOO_TICK_MILLIS;

		gi.Sound(ent, g_media.sounds.bfg_prime, ATTEN_NORM | S_SET_Z_ORIGIN_OFFSET(3), 0);
	}
}
