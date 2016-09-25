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
 * @brief
 */
static void G_ChangeWeapon(g_entity_t *ent, const g_item_t *item) {

	if (ent->client->locals.weapon == NULL) {
		ent->client->locals.weapon = item;

		if (item) {
			
			ent->s.model2 = gi.ModelIndex(item->model);
			
			if (item->ammo) {
				ent->client->locals.ammo_index = ITEM_INDEX(G_FindItem(item->ammo));
			} else {
				ent->client->locals.ammo_index = 0;
			}
		} else {
			ent->s.model2 = 0;
		}

		ent->client->locals.weapon_change_time = 0;
		ent->client->locals.weapon_fire_time = 0;

		return;
	}

	if (ent->client->locals.weapon_change_time > g_level.time)
		return;

	ent->client->locals.weapon_change_time = g_level.time + 500;

	ent->client->locals.next_weapon = item;
	ent->client->locals.prev_weapon = ent->client->locals.weapon;

	ent->client->locals.grenade_hold_time = 0; // put the pin back in

	G_SetAnimation(ent, ANIM_TORSO_DROP, true);

	gi.Sound(ent, g_media.sounds.weapon_switch, ATTEN_NORM);
}

/**
 * @brief
 */
_Bool G_PickupWeapon(g_entity_t *ent, g_entity_t *other) {

	// add the weapon to inventory
	const uint16_t index = ITEM_INDEX(ent->locals.item);

	other->client->locals.inventory[index]++;

	const g_item_t *ammo = G_FindItem(ent->locals.item->ammo);
	if (ammo) {
		const int16_t *stock = &other->client->locals.inventory[ITEM_INDEX(ammo)];

		if (*stock >= ent->locals.health)
			G_AddAmmo(other, ammo, ent->locals.health / 2);
		else
			G_AddAmmo(other, ammo, ent->locals.health);
	}

	// setup respawn if it's not a dropped item
	if (!(ent->locals.spawn_flags & SF_ITEM_DROPPED)) {
		G_SetItemRespawn(ent, g_weapon_respawn_time->value * 1000);
	}

	// auto-change if it's the first weapon we pick up
	if (other->client->locals.weapon == G_FindItem("Blaster"))
		G_ChangeWeapon(other, ent->locals.item);

	return true;
}

/**
 * @return True if the client has both weapon and ammo, false otherwise.
 */
static _Bool G_HasItem(const g_entity_t *ent, const char *item, const int16_t quantity) {

	const uint16_t it = ITEM_INDEX(G_FindItem(item));

	return ent->client->locals.inventory[it] >= quantity;
}

/**
 * @brief
 */
void G_UseBestWeapon(g_entity_t *ent) {

	const g_item_t *item = NULL;

	if (G_HasItem(ent, "bfg10k", 1) && G_HasItem(ent, "nukes", 1)) {
		item = G_FindItem("bfg10k");
	} else if (G_HasItem(ent, "railgun", 1) && G_HasItem(ent, "slugs", 1)) {
		item = G_FindItem("railgun");
	} else if (G_HasItem(ent, "lightning", 1) && G_HasItem(ent, "bolts", 1)) {
		item = G_FindItem("lightning");
	} else if (G_HasItem(ent, "hyperblaster", 1) && G_HasItem(ent, "cells", 1)) {
		item = G_FindItem("hyperblaster");
	} else if (G_HasItem(ent, "rocket launcher", 1) && G_HasItem(ent, "rockets", 1)) {
		item = G_FindItem("rocket launcher");
	} else if (G_HasItem(ent, "grenade launcher", 1) && G_HasItem(ent, "grenades", 1)) {
		item = G_FindItem("grenade launcher");
	} else if (G_HasItem(ent, "grenades", 1)) {
		item = G_FindItem("grenades");
	} else if (G_HasItem(ent, "machinegun", 1) && G_HasItem(ent, "bullets", 1)) {
		item = G_FindItem("machinegun");
	} else if (G_HasItem(ent, "super shotgun", 1) && G_HasItem(ent, "shells", 2)) {
		item = G_FindItem("super shotgun");
	} else if (G_HasItem(ent, "shotgun", 1) && G_HasItem(ent, "shells", 1)) {
		item = G_FindItem("shotgun");
	} else if (G_HasItem(ent, "blaster", 1)) {
		item = G_FindItem("blaster");
	}

	G_ChangeWeapon(ent, item);
}

/**
 * @brief
 */
void G_UseWeapon(g_entity_t *ent, const g_item_t *item) {

	// see if we're already using it
	if (item == ent->client->locals.weapon)
		return;

	if (item->ammo) { // ensure we have ammo
		uint16_t index = ITEM_INDEX(G_FindItem(item->ammo));

		if (!ent->client->locals.inventory[index]) {
			gi.ClientPrint(ent, PRINT_HIGH, "Not enough ammo for %s\n", item->name);
			return;
		}
	}

	// change to this weapon when down
	G_ChangeWeapon(ent, item);
}

/**
 * @brief Drop the specified weapon if the client has sufficient ammo.
 */
g_entity_t *G_DropWeapon(g_entity_t *ent, const g_item_t *item) {

	const uint16_t index = ITEM_INDEX(item);

	g_client_locals_t *cl = &ent->client->locals;

	// see if we're already using it and we only have one
	if ((item == cl->weapon || item == cl->next_weapon) && (cl->inventory[index] == 1)) {
		gi.ClientPrint(ent, PRINT_HIGH, "Can't drop current weapon\n");
		return NULL;
	}

	const g_item_t *ammo = G_FindItem(item->ammo);
	const uint16_t ammo_index = ITEM_INDEX(ammo);

	if (cl->inventory[ammo_index] <= 0) {
		gi.ClientPrint(ent, PRINT_HIGH, "Can't drop a weapon without ammo\n");
		return NULL;
	}

	g_entity_t *dropped = G_DropItem(ent, item);

	if (dropped) {
		cl->inventory[index]--;

		// now adjust dropped ammo quantity to reflect what we actually had available
		if (cl->inventory[ammo_index] < ammo->quantity)
			dropped->locals.health = cl->inventory[ammo_index];

		G_AddAmmo(ent, ammo, -dropped->locals.health);
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

	if (!weapon || !weapon->ammo) // don't drop if not holding
		return NULL;

	const int16_t ammo = ent->client->locals.inventory[ent->client->locals.ammo_index];

	if (!ammo) // don't drop when out of ammo
		return NULL;

	g_entity_t *dropped = G_DropItem(ent, ent->client->locals.weapon);

	if (dropped) {
		if (dropped->locals.health > ammo)
			dropped->locals.health = ammo;
	}

	return dropped;
}

typedef void (*G_FireWeaponFunc)(g_entity_t *ent);

/**
 * @brief Returns true if the specified client can fire their weapon, false
 * otherwise.
 */
static _Bool G_FireWeapon(g_entity_t *ent) {

	uint32_t buttons = (ent->client->locals.latched_buttons | ent->client->locals.buttons);

	if (!(buttons & BUTTON_ATTACK))
		return false;

	ent->client->locals.latched_buttons &= ~BUTTON_ATTACK;

	// use small epsilon for low server frame rates
	if (ent->client->locals.weapon_fire_time > g_level.time + 1)
		return false;

	// determine if ammo is required, and if the quantity is sufficient
	int16_t ammo;
	if (ent->client->locals.ammo_index)
		ammo = ent->client->locals.inventory[ent->client->locals.ammo_index];
	else
		ammo = 0;

	const uint16_t ammo_needed = ent->client->locals.weapon->quantity;

	// if the client does not have enough ammo, change weapons
	if (ent->client->locals.ammo_index && ammo < ammo_needed) {

		if (g_level.time >= ent->client->locals.pain_time) { // play a click sound
			gi.Sound(ent, g_media.sounds.weapon_no_ammo, ATTEN_NORM);
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
static void G_WeaponFired(g_entity_t *ent, uint32_t interval) {

	// set the attack animation
	G_SetAnimation(ent, ANIM_TORSO_ATTACK1, true);

	// push the next fire time out by the interval
	ent->client->locals.weapon_fire_time = g_level.time + interval;

	// and decrease their inventory
	if (ent->client->locals.ammo_index) {
		ent->client->locals.inventory[ent->client->locals.ammo_index] -=
				ent->client->locals.weapon->quantity;
	}

	// play a quad damage sound if applicable
	if (ent->client->locals.inventory[g_media.items.quad_damage]) {

		if (ent->client->locals.quad_attack_time < g_level.time) {
			gi.Sound(ent, g_media.sounds.quad_attack, ATTEN_NORM);
			ent->client->locals.quad_attack_time = g_level.time + 500;
		}
	}
}

/**
 * @brief
 */
void G_ClientWeaponThink(g_entity_t *ent) {

	if (ent->locals.dead)
		return;

	if (ent->client->locals.persistent.spectator)
		return;

	ent->client->locals.weapon_think_time = g_level.time;

	// if changing weapons, carry out the change and re-enable firing
	if (ent->client->locals.weapon_change_time > g_level.time) {

		const uint32_t delta = ent->client->locals.weapon_change_time - g_level.time;
		if (delta <= 250) {

			if (ent->client->locals.weapon != ent->client->locals.next_weapon) {
				ent->client->locals.weapon = ent->client->locals.next_weapon;

				const g_item_t *item = ent->client->locals.weapon;
				if (item) {
					
					// special case for grenades since they're ammo and weapon
					if (g_strcmp0(item->class_name, "ammo_grenades") == 0) {
						ent->s.model2 = g_media.models.grenade;
					} else {
						ent->s.model2 = gi.ModelIndex(item->model);
					}

					if (item->ammo) {
						ent->client->locals.ammo_index = ITEM_INDEX(G_FindItem(item->ammo));
					} else {
						ent->client->locals.ammo_index = 0;
					}
				} else {
					ent->s.model2 = 0;
				}
			} else if (delta <= gi.frame_millis) {

				ent->client->locals.next_weapon = NULL;
				ent->client->locals.weapon_fire_time = 0;

				G_SetAnimation(ent, ANIM_TORSO_STAND1, false);
			}
		}

		return;
	}

	// call active weapon fire routine
	if (ent->client->locals.weapon && ent->client->locals.weapon->Think)
		ent->client->locals.weapon->Think(ent);
}

/**
 * @brief
 */
static void G_MuzzleFlash(g_entity_t *ent, g_muzzle_flash_t flash) {

	gi.WriteByte(SV_CMD_MUZZLE_FLASH);
	gi.WriteShort(ent - g_game.entities);
	gi.WriteByte(flash);

	if (flash == MZ_BLASTER) {
		gi.WriteByte(ent->client ? ent->client->locals.persistent.color : 0);
	}

	gi.Multicast(ent->s.origin, MULTICAST_PHS, NULL);
}

/**
 * @brief
 */
void G_FireBlaster(g_entity_t *ent) {

	if (G_FireWeapon(ent)) {
		vec3_t forward, right, up, org;

		G_InitProjectile(ent, forward, right, up, org);

		G_BlasterProjectile(ent, org, forward, 1000, 15, 2);

		G_MuzzleFlash(ent, MZ_BLASTER);

		G_ClientWeaponKick(ent, 0.08);

		G_WeaponFired(ent, 400);
	}
}

/**
 * @brief
 */
void G_FireShotgun(g_entity_t *ent) {

	if (G_FireWeapon(ent)) {
		vec3_t forward, right, up, org;

		G_InitProjectile(ent, forward, right, up, org);

		G_ShotgunProjectiles(ent, org, forward, 6, 4, 700, 300, 12, MOD_SHOTGUN);

		G_MuzzleFlash(ent, MZ_SHOTGUN);

		G_ClientWeaponKick(ent, 0.12);

		G_WeaponFired(ent, 500);
	}
}

/**
 * @brief
 */
void G_FireSuperShotgun(g_entity_t *ent) {

	if (G_FireWeapon(ent)) {
		vec3_t forward, right, up, org;

		ent->client->locals.angles[YAW] -= 4.0;

		G_InitProjectile(ent, forward, right, up, org);

		G_ShotgunProjectiles(ent, org, forward, 6, 4, 1400, 600, 12, MOD_SUPER_SHOTGUN);

		ent->client->locals.angles[YAW] += 8.0;

		G_InitProjectile(ent, forward, right, up, org);

		G_ShotgunProjectiles(ent, org, forward, 6, 4, 1400, 600, 12, MOD_SUPER_SHOTGUN);

		ent->client->locals.angles[YAW] -= 4.0;

		G_MuzzleFlash(ent, MZ_SSHOTGUN);

		G_ClientWeaponKick(ent, 0.16);

		G_WeaponFired(ent, 800);
	}
}

/**
 * @brief
 */
void G_FireMachinegun(g_entity_t *ent) {

	if (G_FireWeapon(ent)) {
		vec3_t forward, right, up, org;

		G_InitProjectile(ent, forward, right, up, org);

		G_BulletProjectile(ent, org, forward, 6, 6, 100, 200, MOD_MACHINEGUN);

		G_MuzzleFlash(ent, MZ_MACHINEGUN);

		G_ClientWeaponKick(ent, 0.125);

		G_WeaponFired(ent, 48);
	}
}

/*
 *  Create a grenade entity that will follow the player 
 *  while playing the ticking sound
 */
static void G_PullGrenadePin(g_entity_t *ent) {
	g_entity_t *nade = G_AllocEntity(__func__);
	ent->client->locals.held_grenade = nade;
	nade->owner = ent;
	nade->solid = SOLID_PROJECTILE;
	nade->locals.clip_mask = MASK_CLIP_PROJECTILE;
	nade->locals.move_type = MOVE_TYPE_BOUNCE;
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
static _Bool G_CheckGrenadeHold(g_entity_t *ent, uint32_t buttons)
{
	_Bool current_hold = buttons & BUTTON_ATTACK;
	
	// just pulled the pin
	if (!ent->client->locals.grenade_hold_time && current_hold)
	{
		G_PullGrenadePin(ent);
		ent->client->locals.grenade_hold_time = g_level.time;
		ent->client->locals.grenade_hold_frame = g_level.frame_num;
		return true;
	}
	// already pulled the pin and holding it
	else if (ent->client->locals.grenade_hold_time && current_hold)
	{
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
	if (!(buttons & BUTTON_ATTACK) && !ent->client->locals.grenade_hold_time)
		return;
	
	const uint32_t nade_time = 3 * 1000;	// 3 seconds before boom
	vec_t throw_speed = 500.0; // minimum
	
	// use small epsilon for low server frame rates
	if (ent->client->locals.weapon_fire_time > g_level.time + 1)
		return;

	int16_t ammo;
	if (ent->client->locals.ammo_index)
		ammo = ent->client->locals.inventory[ent->client->locals.ammo_index];
	else
		ammo = 0;

	// override quantity needed from g_item_t since grenades are both ammo and weapon
	const uint16_t ammo_needed = 1;
	
	// if the client does not have enough ammo, change weapons
	if (ent->client->locals.ammo_index && ammo < ammo_needed) {
	
		if (g_level.time >= ent->client->locals.pain_time) { // play a click sound
			gi.Sound(ent, g_media.sounds.weapon_no_ammo, ATTEN_NORM);
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
		if ((g_level.frame_num - ent->client->locals.grenade_hold_frame) % gi.frame_rate == 0) {
			gi.Sound(ent, gi.SoundIndex("weapons/handgrenades/hg_clang.ogg"), ATTEN_NORM);
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
	
	G_InitProjectile(ent, forward, right, up, org);
	G_HandGrenadeProjectile(
		ent,					// player
		ent->client->locals.held_grenade,	// the grenade
		org,					// starting point
		forward,				// direction
		(uint32_t)throw_speed,	// how fast does it fly
		120,					// damage dealt
		120,					// knockback
		185.0,					// blast radius 
		nade_time-hold_time		// time before explode (next think)
	);
		
	// play the sound if we throw it
	gi.Sound(ent, gi.SoundIndex("weapons/handgrenades/hg_throw.wav"), ATTEN_NORM);
	
	// set the attack animation
	G_SetAnimation(ent, ANIM_TORSO_ATTACK1, true);

	// push the next fire time out by the interval (2 secs)
	ent->client->locals.weapon_fire_time = g_level.time + (2 * 1000);

	// and decrease their inventory
	if (ent->client->locals.ammo_index) {
		ent->client->locals.inventory[ent->client->locals.ammo_index] -= ammo_needed;
	}

	// play a quad damage sound if applicable
	if (ent->client->locals.inventory[g_media.items.quad_damage]) {

		if (ent->client->locals.quad_attack_time < g_level.time) {
			gi.Sound(ent, g_media.sounds.quad_attack, ATTEN_NORM);
			ent->client->locals.quad_attack_time = g_level.time + 500;
		}
	}
	
	ent->client->locals.grenade_hold_time = 0;
	ent->client->locals.grenade_hold_frame = 0;
}

/**
 * @brief
 */
void G_FireGrenadeLauncher(g_entity_t *ent) {

	if (G_FireWeapon(ent)) {
		vec3_t forward, right, up, org;

		G_InitProjectile(ent, forward, right, up, org);

		G_GrenadeProjectile(ent, org, forward, 700, 100, 100, 185.0, 2500.0);

		G_MuzzleFlash(ent, MZ_GRENADE);

		G_ClientWeaponKick(ent, 0.2);

		G_WeaponFired(ent, 1000);
	}
}

/**
 * @brief
 */
void G_FireRocketLauncher(g_entity_t *ent) {

	if (G_FireWeapon(ent)) {
		vec3_t forward, right, up, org;

		G_InitProjectile(ent, forward, right, up, org);

		G_RocketProjectile(ent, org, forward, 1000, 100, 100, 150.0);

		G_MuzzleFlash(ent, MZ_ROCKET);

		G_ClientWeaponKick(ent, 0.25);

		G_WeaponFired(ent, 1000);
	}
}

/**
 * @brief
 */
void G_FireHyperblaster(g_entity_t *ent) {

	if (G_FireWeapon(ent)) {
		vec3_t forward, right, up, org;

		G_InitProjectile(ent, forward, right, up, org);

		G_HyperblasterProjectile(ent, org, forward, 1800, 12, 6);

		G_MuzzleFlash(ent, MZ_HYPERBLASTER);

		G_ClientWeaponKick(ent, 0.15);

		G_WeaponFired(ent, 100);
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

		G_InitProjectile(ent, forward, right, up, org);

		G_LightningProjectile(ent, org, forward, 8, 12);

		G_ClientWeaponKick(ent, 0.15);

		G_WeaponFired(ent, 100);
	}
}

/**
 * @brief
 */
void G_FireRailgun(g_entity_t *ent) {

	if (G_FireWeapon(ent)) {
		vec3_t forward, right, up, org;

		G_InitProjectile(ent, forward, right, up, org);

		const int16_t damage = (g_level.gameplay == GAME_INSTAGIB) ? 999 : 120;

		G_RailgunProjectile(ent, org, forward, damage, 80);

		G_MuzzleFlash(ent, MZ_RAILGUN);

		G_ClientWeaponKick(ent, 0.3);

		G_WeaponFired(ent, 1800);
	}
}

/**
 * @brief
 */
static void G_FireBfg_(g_entity_t *ent) {

	if (ent->owner->locals.dead == false) {
		if (ent->owner->client->locals.weapon == G_FindItemByClassName("weapon_bfg")) {
			vec3_t forward, right, up, org;

			G_InitProjectile(ent->owner, forward, right, up, org);

			G_BfgProjectile(ent->owner, org, forward, 720, 180, 140, 512.0);

			G_MuzzleFlash(ent->owner, MZ_BFG);

			G_ClientWeaponKick(ent->owner, 1.0);

			G_WeaponFired(ent->owner, 2000);
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
		ent->client->locals.weapon_fire_time = g_level.time + 3000;

		g_entity_t *timer = G_AllocEntity(__func__);
		timer->owner = ent;

		timer->locals.Think = G_FireBfg_;
		timer->locals.next_think = g_level.time + 1000 - gi.frame_millis;

		gi.Sound(ent, g_media.sounds.bfg_prime, ATTEN_NORM);
	}
}
