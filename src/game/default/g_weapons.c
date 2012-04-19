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
bool G_PickupWeapon(g_edict_t *ent, g_edict_t *other) {
	int index, ammoindex, delta;
	g_item_t *ammo;

	index = ITEM_INDEX(ent->item);
	ammo = G_FindItem(ent->item->ammo);
	ammoindex = ITEM_INDEX(ammo);

	delta = ent->health - other->client->persistent.inventory[ammoindex];
	if (delta <= 0)
		G_AddAmmo(other, ammo, ent->health / 2);
	else
		G_SetAmmo(other, ammo, ent->health + other->client->persistent.inventory[ammoindex]/ 2);

	// setup respawn if it's not a dropped item
	if (!(ent->spawn_flags & SF_ITEM_DROPPED)) {
		if(strcmp(ent->item->pickup_name, "BFG10K"))
			G_SetItemRespawn(ent, 5000);
		else
			G_SetItemRespawn(ent, 30000);
	}

	// add the weapon to inventory
	other->client->persistent.inventory[index]++;

	// auto-change if it's the first weapon we pick up
	if (other->client->persistent.weapon != ent->item && (other->client->persistent.weapon
			== G_FindItem("Blaster")))
		other->client->new_weapon = ent->item;

	return true;
}

/**
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
	if (ent->client->weapon_fire_time < g_level.time + 400) {
		ent->client->weapon_fire_time = g_level.time + 400;
	}

	// resolve ammo
	if (ent->client->persistent.weapon && ent->client->persistent.weapon->ammo)
		ent->client->ammo_index = ITEM_INDEX(G_FindItem(ent->client->persistent.weapon->ammo));
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
	if (client->persistent.inventory[ITEM_INDEX(G_FindItem("shells"))]
			&& client->persistent.inventory[ITEM_INDEX(G_FindItem("shotgun"))]) {
		client->new_weapon = G_FindItem("shotgun");
		return;
	}

	client->new_weapon = G_FindItem("blaster");
}

/*
 * G_UseWeapon
 */
void G_UseWeapon(g_edict_t *ent, g_item_t *item) {

	// see if we're already using it
	if (item == ent->client->persistent.weapon)
		return;

	if (item->ammo) { // ensure we have ammo
		int index = ITEM_INDEX(G_FindItem(item->ammo));

		if (!ent->client->persistent.inventory[index]) {
			gi.ClientPrint(ent, PRINT_HIGH, "Not enough ammo for %s\n", item->pickup_name);
			return;
		}
	}

	// change to this weapon when down
	ent->client->new_weapon = item;
}

/*
 * G_DropWeapon
 */
void G_DropWeapon(g_edict_t *ent, g_item_t *item) {
	int index, ammo_index;
	g_edict_t *dropped;
	g_item_t *ammo;

	index = ITEM_INDEX(item);

	// see if we're already using it and we only have one
	if ((item == ent->client->persistent.weapon || item == ent->client->new_weapon)
			&& (ent->client->persistent.inventory[index] == 1)) {
		gi.ClientPrint(ent, PRINT_HIGH, "Can't drop current weapon\n");
		return;
	}

	ammo = G_FindItem(item->ammo);
	ammo_index = ITEM_INDEX(ammo);
	if(ent->client->persistent.inventory[ammo_index] <= 0) {
		gi.ClientPrint(ent, PRINT_HIGH, "Can't drop a weapon without ammo\n");
		return;
	}


	dropped = G_DropItem(ent, item);
	ent->client->persistent.inventory[index]--;

	//now adjust dropped ammo quantity to reflect what we actually had avalible
	if(ent->client->persistent.inventory[ammo_index] < ammo->quantity)
		dropped->health = ent->client->persistent.inventory[ammo_index];
	G_AddAmmo(ent, ammo, -1 * ammo->quantity);

}

/*
 * G_TossWeapon
 */
void G_TossWeapon(g_edict_t *ent) {
	g_edict_t *dropped;
	g_item_t *item;
	short ammo;

	// don't drop weapon when falling into void
	if (means_of_death == MOD_TRIGGER_HURT)
		return;

	item = ent->client->persistent.weapon;

	ammo = ent->client->persistent.inventory[ent->client->ammo_index];

	if (!ammo)
		return; // don't drop when out of ammo

	dropped = G_DropItem(ent, item);
	if(dropped->health > ammo)
		dropped->health = ammo;
}

/*
 * G_FireWeapon
 */
static void G_FireWeapon(g_edict_t *ent, unsigned int interval, void (*fire)(g_edict_t *ent)) {
	int n, m;
	int buttons;

	buttons = (ent->client->latched_buttons | ent->client->buttons);

	if (!(buttons & BUTTON_ATTACK))
		return;

	ent->client->latched_buttons &= ~BUTTON_ATTACK;

	// use small epsilon for low server_frame rates
	if (ent->client->weapon_fire_time > g_level.time + 1)
		return;

	ent->client->weapon_fire_time = g_level.time + interval;

	// determine if ammo is required, and if the quantity is sufficient
	if (ent->client->ammo_index)
		n = ent->client->persistent.inventory[ent->client->ammo_index];
	else
		n = 0;
	m = ent->client->persistent.weapon->quantity;

	// they are out of ammo
	if (ent->client->ammo_index && n < m) {

		if (g_level.time >= ent->client->pain_time) { // play a click sound
			gi.Sound(ent, gi.SoundIndex("weapons/common/no_ammo"), ATTN_NORM);
			ent->client->pain_time = g_level.time + 1000;
		}

		G_UseBestWeapon(ent->client);
		return;
	}

	// they've pressed their fire button, and have ammo, so fire
	G_SetAnimation(ent, ANIM_TORSO_ATTACK1, true);

	if (ent->client->persistent.inventory[quad_damage_index]) { // quad sound

		if (ent->client->quad_attack_time < g_level.time) {
			gi.Sound(ent, gi.SoundIndex("quad/attack"), ATTN_NORM);
			ent->client->quad_attack_time = g_level.time + 500;
		}
	}

	fire(ent); // fire the weapon

	// and decrease their inventory
	if (ent->client->ammo_index)
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
 * G_MuzzleFlash
 */
static void G_MuzzleFlash(g_edict_t *ent, muzzle_flash_t flash) {

	gi.WriteByte(SV_CMD_MUZZLE_FLASH);
	gi.WriteShort(ent - g_game.edicts);
	gi.WriteByte(flash);

	if (flash == MZ_BLASTER) {
		int color = DEFAULT_WEAPON_EFFECT_COLOR;
		if (ent->client) {
			color = ent->client->persistent.color;
		}
		gi.WriteByte(color);
	}

	gi.Multicast(ent->s.origin, MULTICAST_PHS);
}

/*
 * G_FireBlaster
 */
static void G_FireBlaster_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	G_InitProjectile(ent, forward, right, up, org);

	G_BlasterProjectile(ent, org, forward, 1000, 15, 2);

	G_MuzzleFlash(ent, MZ_BLASTER);

	G_ClientWeaponKick(ent, -3);
}

void G_FireBlaster(g_edict_t *ent) {
	G_FireWeapon(ent, 500, G_FireBlaster_);
}

/*
 * G_FireShotgun
 */
static void G_FireShotgun_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	G_InitProjectile(ent, forward, right, up, org);

	G_ShotgunProjectiles(ent, org, forward, 6, 4, 750, 350, 12, MOD_SHOTGUN);

	G_MuzzleFlash(ent, MZ_SHOTGUN);

	G_ClientWeaponKick(ent, -6);
}

void G_FireShotgun(g_edict_t *ent) {
	G_FireWeapon(ent, 750, G_FireShotgun_);
}

/*
 * G_FireSuperShotgun
 */
static void G_FireSuperShotgun_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	ent->client->angles[YAW] -= 5.0;

	G_InitProjectile(ent, forward, right, up, org);

	G_ShotgunProjectiles(ent, org, forward, 6, 4, 1000, 500, 12, MOD_SUPER_SHOTGUN);

	ent->client->angles[YAW] += 10.0;

	G_InitProjectile(ent, forward, right, up, org);

	G_ShotgunProjectiles(ent, org, forward, 4, 4, 1000, 500, 12, MOD_SUPER_SHOTGUN);

	ent->client->angles[YAW] -= 5.0;

	G_MuzzleFlash(ent, MZ_SSHOTGUN);

	G_ClientWeaponKick(ent, -8);
}

void G_FireSuperShotgun(g_edict_t *ent) {
	G_FireWeapon(ent, 1000, G_FireSuperShotgun_);
}

/*
 * G_FireMachinegun
 */
static void G_FireMachinegun_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	G_InitProjectile(ent, forward, right, up, org);

	G_BulletProjectile(ent, org, forward, 4, 4, 100, 200, MOD_MACHINEGUN);

	G_MuzzleFlash(ent, MZ_MACHINEGUN);

	G_ClientWeaponKick(ent, -4);
}

void G_FireMachinegun(g_edict_t *ent) {
	G_FireWeapon(ent, 50, G_FireMachinegun_);
}

/*
 * G_FireGrenadeLauncher
 */
static void G_FireGrenadeLauncher_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	const int dmg = 120 + Randomc() * 30.0;
	const int knockback = 120 + Randomf() * 20.0;

	G_InitProjectile(ent, forward, right, up, org);

	G_GrenadeProjectile(ent, org, forward, 700, dmg, knockback, 185.0, 2000);

	G_MuzzleFlash(ent, MZ_GRENADE);

	G_ClientWeaponKick(ent, -8);
}

void G_FireGrenadeLauncher(g_edict_t *ent) {
	G_FireWeapon(ent, 1000, G_FireGrenadeLauncher_);
}

/*
 * G_FireRocketLauncher
 */
static void G_FireRocketLauncher_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	const int dmg = 110 + Randomf() * 20.0;
	const int knockback = 110 + Randomf() * 20.0;

	G_InitProjectile(ent, forward, right, up, org);

	G_RocketProjectile(ent, org, forward, 900, dmg, knockback, 150.0);

	G_MuzzleFlash(ent, MZ_ROCKET);

	G_ClientWeaponKick(ent, -10);
}

void G_FireRocketLauncher(g_edict_t *ent) {
	G_FireWeapon(ent, 1000, G_FireRocketLauncher_);
}

/*
 * G_FireHyperblaster
 */
static void G_FireHyperblaster_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	G_InitProjectile(ent, forward, right, up, org);

	G_HyperblasterProjectile(ent, org, forward, 1400, 16, 6);

	G_MuzzleFlash(ent, MZ_HYPERBLASTER);

	G_ClientWeaponKick(ent, -6);
}

void G_FireHyperblaster(g_edict_t *ent) {
	G_FireWeapon(ent, 100, G_FireHyperblaster_);
}

/*
 * G_FireLightning
 */
static void G_FireLightning_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	const bool muzzle_flash = !ent->lightning;

	G_InitProjectile(ent, forward, right, up, org);

	G_LightningProjectile(ent, org, forward, 12, 12);

	if (muzzle_flash) {
		G_MuzzleFlash(ent, MZ_LIGHTNING);
	}

	G_ClientWeaponKick(ent, -4);
}

void G_FireLightning(g_edict_t *ent) {
	G_FireWeapon(ent, 100, G_FireLightning_);
}

/*
 * G_FireRailgun
 */
static void G_FireRailgun_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	G_InitProjectile(ent, forward, right, up, org);

	G_RailgunProjectile(ent, org, forward, 120, 80);

	G_MuzzleFlash(ent, MZ_RAILGUN);

	G_ClientWeaponKick(ent, -6);
}

void G_FireRailgun(g_edict_t *ent) {
	G_FireWeapon(ent, 1800, G_FireRailgun_);
}

/*
 * G_FireBfg
 */
static void G_FireBfg_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	G_InitProjectile(ent, forward, right, up, org);

	G_BfgProjectiles(ent, org, forward, 600, 100, 100, 256.0);

	G_MuzzleFlash(ent, MZ_BFG);

	G_ClientWeaponKick(ent, -24);
}

void G_FireBfg(g_edict_t *ent) {
	G_FireWeapon(ent, 2000, G_FireBfg_);
}
