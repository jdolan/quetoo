/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

/*
 * G_PickupWeapon
 */
boolean_t G_PickupWeapon(g_edict_t *ent, g_edict_t *other) {
	int index, ammoindex;
	g_item_t *ammo;

	index = ITEM_INDEX(ent->item);

	// add ammo
	ammo = G_FindItem(ent->item->ammo);
	ammoindex = ITEM_INDEX(ammo);

	if (!(ent->spawn_flags & SF_ITEM_DROPPED)
			&& other->client->persistent.inventory[index]) {
		if (other->client->persistent.inventory[ammoindex] >= ammo->quantity)
			G_AddAmmo(other, ammo, ent->item->quantity); // q3 style
		else
			G_AddAmmo(other, ammo, ammo->quantity);
	} else
		G_AddAmmo(other, ammo, ammo->quantity);

	// setup respawn if it's not a dropped item
	if (!(ent->spawn_flags & SF_ITEM_DROPPED))
		G_SetItemRespawn(ent, 5);

	// add the weapon to inventory
	other->client->persistent.inventory[index]++;

	// auto-change if it's the first weapon we pick up
	if (other->client->persistent.weapon != ent->item
			&& (other->client->persistent.weapon == G_FindItem("Shotgun")))
		other->client->new_weapon = ent->item;

	return true;
}

/*
 * G_ChangeWeapon
 *
 * The old weapon has been put away, so make the new one current
 */
void G_ChangeWeapon(g_edict_t *ent) {

	// change weapon
	ent->client->persistent.last_weapon = ent->client->persistent.weapon;
	ent->client->persistent.weapon = ent->client->new_weapon;
	ent->client->new_weapon = NULL;

	// update weapon state
	ent->client->weapon_fire_time = g_level.time + 0.4;

	// resolve ammo
	if (ent->client->persistent.weapon && ent->client->persistent.weapon->ammo)
		ent->client->ammo_index
				= ITEM_INDEX(G_FindItem(ent->client->persistent.weapon->ammo));
	else
		ent->client->ammo_index = 0;

	// set visible model
	if (ent->client->persistent.weapon)
		ent->s.model2 = gi.ModelIndex(ent->client->persistent.weapon->model);
	else
		ent->s.model2 = 0;

	if (ent->health < 1)
		return;

	G_SetAnimation(ent, ANIM_TORSO_DROP, false);

	// play a sound
	gi.Sound(ent, gi.SoundIndex("weapons/common/switch"), ATTN_NORM);
}

/*
 * G_UseBestWeapon
 */
void G_UseBestWeapon(g_client_t *client) {

	if (client->persistent.inventory[ITEM_INDEX(G_FindItem("nukes"))]
			&& client->persistent.inventory[ITEM_INDEX(G_FindItem("bfg10k"))]) {
		client->new_weapon = G_FindItem("bfg10k");
		return;
	}
	if (client->persistent.inventory[ITEM_INDEX(G_FindItem("slugs"))]
			&& client->persistent.inventory[ITEM_INDEX(G_FindItem("railgun"))]) {
		client->new_weapon = G_FindItem("railgun");
		return;
	}
	if (client->persistent.inventory[ITEM_INDEX(G_FindItem("bolts"))]
			&& client->persistent.inventory[ITEM_INDEX(G_FindItem("lightning"))]) {
		client->new_weapon = G_FindItem("lightning");
		return;
	}
	if (client->persistent.inventory[ITEM_INDEX(G_FindItem("cells"))]
			&& client->persistent.inventory[ITEM_INDEX(G_FindItem("hyperblaster"))]) {
		client->new_weapon = G_FindItem("hyperblaster");
		return;
	}
	if (client->persistent.inventory[ITEM_INDEX(G_FindItem("rockets"))]
			&& client->persistent.inventory[ITEM_INDEX(G_FindItem("rocket launcher"))]) {
		client->new_weapon = G_FindItem("rocket launcher");
		return;
	}
	if (client->persistent.inventory[ITEM_INDEX(G_FindItem("grenades"))]
			&& client->persistent.inventory[ITEM_INDEX(G_FindItem("grenade launcher"))]) {
		client->new_weapon = G_FindItem("grenade launcher");
		return;
	}
	if (client->persistent.inventory[ITEM_INDEX(G_FindItem("bullets"))]
			&& client->persistent.inventory[ITEM_INDEX(G_FindItem("machinegun"))]) {
		client->new_weapon = G_FindItem("machinegun");
		return;
	}
	if (client->persistent.inventory[ITEM_INDEX(G_FindItem("shells"))] > 1
			&& client->persistent.inventory[ITEM_INDEX(G_FindItem("super shotgun"))]) {
		client->new_weapon = G_FindItem("super shotgun");
		return;
	}
	if (client->persistent.inventory[ITEM_INDEX(G_FindItem("shells"))]) {
		client->new_weapon = G_FindItem("shotgun");
		return;
	}
}

/*
 * G_UseWeapon
 */
void G_UseWeapon(g_edict_t *ent, g_item_t *item) {

	// see if we're already using it
	if (item == ent->client->persistent.weapon)
		return;

	// change to this weapon when down
	ent->client->new_weapon = item;
}

/*
 * G_DropWeapon
 */
void G_DropWeapon(g_edict_t *ent, g_item_t *item) {
	int index;

	index = ITEM_INDEX(item);

	// see if we're already using it and we only have one
	if ((item == ent->client->persistent.weapon || item == ent->client->new_weapon)
			&& (ent->client->persistent.inventory[index] == 1)) {
		gi.ClientPrint(ent, PRINT_HIGH, "Can't drop current weapon\n");
		return;
	}

	G_DropItem(ent, item);
	ent->client->persistent.inventory[index]--;
}

/*
 * G_TossWeapon
 */
void G_TossWeapon(g_edict_t *ent) {
	g_item_t *item;

	// don't drop weapon when falling into void
	if (means_of_death == MOD_TRIGGER_HURT)
		return;

	item = ent->client->persistent.weapon;

	if (!ent->client->persistent.inventory[ent->client->ammo_index])
		return; // don't drop when out of ammo

	G_DropItem(ent, item);
}

/*
 * G_FireWeapon
 */
static void G_FireWeapon(g_edict_t *ent, float interval,
		void(*fire)(g_edict_t *ent)) {
	int n, m;
	int buttons;

	buttons = (ent->client->latched_buttons | ent->client->buttons);

	if (!(buttons & BUTTON_ATTACK))
		return;

	ent->client->latched_buttons &= ~BUTTON_ATTACK;

	// use small epsilon for low server_frame rates
	if (ent->client->weapon_fire_time > g_level.time + 0.001)
		return;

	ent->client->weapon_fire_time = g_level.time + interval;

	// determine if ammo is required, and if the quantity is sufficient
	n = ent->client->persistent.inventory[ent->client->ammo_index];
	m = ent->client->persistent.weapon->quantity;

	// they are out of ammo
	if (ent->client->ammo_index && n < m) {

		if (g_level.time >= ent->client->pain_time) { // play a click sound
			gi.Sound(ent, gi.SoundIndex("weapons/common/no_ammo"), ATTN_NORM);
			ent->client->pain_time = g_level.time + 1;
		}

		G_UseBestWeapon(ent->client);
		return;
	}

	// they've pressed their fire button, and have ammo, so fire
	G_SetAnimation(ent, ANIM_TORSO_ATTACK1, true);

	if (ent->client->persistent.inventory[quad_damage_index]) { // quad sound

		if (ent->client->quad_attack_time < g_level.time) {
			gi.Sound(ent, gi.SoundIndex("quad/attack"), ATTN_NORM);

			ent->client->quad_attack_time = g_level.time + 0.5;
		}
	}

	fire(ent); // fire the weapon

	// and decrease their inventory
	ent->client->persistent.inventory[ent->client->ammo_index] -= m;
}

/*
 * G_WeaponThink
 */
void G_WeaponThink(g_edict_t *ent) {

	if (ent->health < 1)
		return;

	ent->client->weapon_think_time = g_level.time;

	if (ent->client->new_weapon) {
		G_ChangeWeapon(ent);
		return;
	}

	// see if we should raise the weapon
	if (ent->client->weapon_fire_time >= g_level.time) {
		if (G_IsAnimation(ent, ANIM_TORSO_DROP))
			G_SetAnimation(ent, ANIM_TORSO_RAISE, false);
	}

	// call active weapon think routine
	if (ent->client->persistent.weapon && ent->client->persistent.weapon->weapon_think)
		ent->client->persistent.weapon->weapon_think(ent);
}

/*
 * G_FireShotgun
 */
static void G_FireShotgun_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	G_InitProjectile(ent, forward, right, up, org);

	G_ShotgunProjectiles(ent, org, forward, 6, 4, 1000, 500, 12, MOD_SHOTGUN);

	// send muzzle flash
	gi.WriteByte(SV_CMD_MUZZLE_FLASH);
	gi.WriteShort(ent - g_game.edicts);
	gi.WriteByte(MZ_SHOTGUN);
	gi.Multicast(ent->s.origin, MULTICAST_PVS);
}

void G_FireShotgun(g_edict_t *ent) {
	G_FireWeapon(ent, 0.65, G_FireShotgun_);
}

/*
 * G_FireSuperShotgun
 */
static void G_FireSuperShotgun_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	ent->s.angles[YAW] -= 5.0;

	G_InitProjectile(ent, forward, right, up, org);

	G_ShotgunProjectiles(ent, org, forward, 4, 4, 1000, 500, 12,
			MOD_SUPER_SHOTGUN);

	ent->s.angles[YAW] += 10.0;

	G_InitProjectile(ent, forward, right, up, org);

	G_ShotgunProjectiles(ent, org, forward, 4, 4, 100, 500, 12,
			MOD_SUPER_SHOTGUN);

	ent->s.angles[YAW] -= 5.0;

	// send muzzle flash
	gi.WriteByte(SV_CMD_MUZZLE_FLASH);
	gi.WriteShort(ent - g_game.edicts);
	gi.WriteByte(MZ_SSHOTGUN);
	gi.Multicast(ent->s.origin, MULTICAST_PVS);
}

void G_FireSuperShotgun(g_edict_t *ent) {
	G_FireWeapon(ent, 0.85, G_FireSuperShotgun_);
}

/*
 * G_FireMachinegun
 */
static void G_FireMachinegun_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	G_InitProjectile(ent, forward, right, up, org);

	G_BulletProjectile(ent, org, forward, 8, 4, 100, 200, MOD_MACHINEGUN);

	// send muzzle flash
	gi.WriteByte(SV_CMD_MUZZLE_FLASH);
	gi.WriteShort(ent - g_game.edicts);
	gi.WriteByte(MZ_MACHINEGUN);
	gi.Multicast(ent->s.origin, MULTICAST_PVS);
}

void G_FireMachinegun(g_edict_t *ent) {
	G_FireWeapon(ent, 0.04, G_FireMachinegun_);
}

/*
 * G_FireGrenadeLauncher
 */
static void G_FireGrenadeLauncher_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	G_InitProjectile(ent, forward, right, up, org);

	G_GrenadeProjectile(ent, org, forward, 900, 100, 100, 185.0, 2.0);

	gi.WriteByte(SV_CMD_MUZZLE_FLASH);
	gi.WriteShort(ent - g_game.edicts);
	gi.WriteByte(MZ_GRENADE);
	gi.Multicast(ent->s.origin, MULTICAST_PVS);
}

void G_FireGrenadeLauncher(g_edict_t *ent) {
	G_FireWeapon(ent, 0.6, G_FireGrenadeLauncher_);
}

/*
 * G_FireRocketLauncher
 */
static void G_FireRocketLauncher_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	G_InitProjectile(ent, forward, right, up, org);

	G_RocketProjectile(ent, org, forward, 1250, 120, 120, 150.0);

	// send muzzle flash
	gi.WriteByte(SV_CMD_MUZZLE_FLASH);
	gi.WriteShort(ent - g_game.edicts);
	gi.WriteByte(MZ_ROCKET);
	gi.Multicast(ent->s.origin, MULTICAST_PVS);
}

void G_FireRocketLauncher(g_edict_t *ent) {
	G_FireWeapon(ent, 0.8, G_FireRocketLauncher_);
}

/*
 * G_FireHyperblaster
 */
static void G_FireHyperblaster_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	G_InitProjectile(ent, forward, right, up, org);

	G_HyperblasterProjectile(ent, org, forward, 2000, 16, 6);

	// send muzzle flash
	gi.WriteByte(SV_CMD_MUZZLE_FLASH);
	gi.WriteShort(ent - g_game.edicts);
	gi.WriteByte(MZ_HYPERBLASTER);
	gi.Multicast(ent->s.origin, MULTICAST_PVS);
}

void G_FireHyperblaster(g_edict_t *ent) {
	G_FireWeapon(ent, 0.1, G_FireHyperblaster_);
}

/*
 * G_FireLightning
 */
static void G_FireLightning_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	G_InitProjectile(ent, forward, right, up, org);

	G_LightningProjectile(ent, org, forward, 10, 12);

	// if the client has just begun to attack, send the muzzle flash
	if (ent->client->muzzle_flash_time < g_level.time) {
		gi.WriteByte(SV_CMD_MUZZLE_FLASH);
		gi.WriteShort(ent - g_game.edicts);
		gi.WriteByte(MZ_LIGHTNING);
		gi.Multicast(ent->s.origin, MULTICAST_PVS);

		ent->client->muzzle_flash_time = g_level.time + 0.25;
	}
}

void G_FireLightning(g_edict_t *ent) {
	G_FireWeapon(ent, 0.1, G_FireLightning_);
}

/*
 * G_FireRailgun
 */
static void G_FireRailgun_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	G_InitProjectile(ent, forward, right, up, org);

	G_RailgunProjectile(ent, org, forward, 120, 80);

	// send muzzle flash
	gi.WriteByte(SV_CMD_MUZZLE_FLASH);
	gi.WriteShort(ent - g_game.edicts);
	gi.WriteByte(MZ_RAILGUN);
	gi.Multicast(ent->s.origin, MULTICAST_PVS);
}

void G_FireRailgun(g_edict_t *ent) {
	G_FireWeapon(ent, 1.5, G_FireRailgun_);
}

/*
 * G_FireBfg
 */
static void G_FireBfg_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	G_InitProjectile(ent, forward, right, up, org);

	G_BfgProjectiles(ent, org, forward, 800, 100, 100, 1024.0);

	// send muzzle flash
	gi.WriteByte(SV_CMD_MUZZLE_FLASH);
	gi.WriteShort(ent - g_game.edicts);
	gi.WriteByte(MZ_BFG);
	gi.Multicast(ent->s.origin, MULTICAST_PVS);
}

void G_FireBfg(g_edict_t *ent) {
	G_FireWeapon(ent, 2.0, G_FireBfg_);
}
