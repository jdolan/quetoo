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
 * @brief
 */_Bool G_PickupWeapon(g_edict_t *ent, g_edict_t *other) {
	int32_t delta;

	const uint16_t index = ITEM_INDEX(ent->locals.item);
	const g_item_t *ammo = G_FindItem(ent->locals.item->ammo);
	const uint16_t ammo_index = ITEM_INDEX(ammo);

	delta = ent->locals.health - other->client->locals.persistent.inventory[ammo_index];
	if (delta <= 0)
		G_AddAmmo(other, ammo, ent->locals.health / 2);
	else
		G_SetAmmo(other, ammo,
				ent->locals.health + other->client->locals.persistent.inventory[ammo_index] / 2);

	// setup respawn if it's not a dropped item
	if (!(ent->locals.spawn_flags & SF_ITEM_DROPPED)) {
		if (!strcmp(ent->locals.item->name, "BFG10K"))
			G_SetItemRespawn(ent, g_weapon_respawn_time->value * 3 * 1000);
		else
			G_SetItemRespawn(ent, g_weapon_respawn_time->value * 1000);
	}

	// add the weapon to inventory
	other->client->locals.persistent.inventory[index]++;

	// auto-change if it's the first weapon we pick up
	if (other->client->locals.persistent.weapon != ent->locals.item
			&& (other->client->locals.persistent.weapon == G_FindItem("Blaster")))
		other->client->locals.new_weapon = ent->locals.item;

	return true;
}

/*
 * @brief The old weapon has been put away, so make the new one current
 */
void G_ChangeWeapon(g_edict_t *ent) {

	// change weapon
	ent->client->locals.persistent.last_weapon = ent->client->locals.persistent.weapon;
	ent->client->locals.persistent.weapon = ent->client->locals.new_weapon;
	ent->client->locals.new_weapon = NULL;

	// update weapon state
	if (ent->client->locals.weapon_fire_time < g_level.time + 400) {
		ent->client->locals.weapon_fire_time = g_level.time + 400;
	}

	// resolve ammo
	if (ent->client->locals.persistent.weapon && ent->client->locals.persistent.weapon->ammo)
		ent->client->locals.ammo_index
				= ITEM_INDEX(G_FindItem(ent->client->locals.persistent.weapon->ammo));
	else
		ent->client->locals.ammo_index = 0;

	// set visible model
	if (ent->client->locals.persistent.weapon)
		ent->s.model2 = gi.ModelIndex(ent->client->locals.persistent.weapon->model);
	else
		ent->s.model2 = 0;

	if (ent->locals.health < 1)
		return;

	G_SetAnimation(ent, ANIM_TORSO_DROP, false);

	// play a sound
	gi.Sound(ent, gi.SoundIndex("weapons/common/switch"), ATTN_NORM);
}

/*
 * @brief
 */
void G_UseBestWeapon(g_client_t *client) {

	if (client->locals.persistent.inventory[ITEM_INDEX(G_FindItem("nukes"))]
			&& client->locals.persistent.inventory[ITEM_INDEX(G_FindItem("bfg10k"))]) {
		client->locals.new_weapon = G_FindItem("bfg10k");
		return;
	}
	if (client->locals.persistent.inventory[ITEM_INDEX(G_FindItem("slugs"))]
			&& client->locals.persistent.inventory[ITEM_INDEX(G_FindItem("railgun"))]) {
		client->locals.new_weapon = G_FindItem("railgun");
		return;
	}
	if (client->locals.persistent.inventory[ITEM_INDEX(G_FindItem("bolts"))]
			&& client->locals.persistent.inventory[ITEM_INDEX(G_FindItem("lightning"))]) {
		client->locals.new_weapon = G_FindItem("lightning");
		return;
	}
	if (client->locals.persistent.inventory[ITEM_INDEX(G_FindItem("cells"))]
			&& client->locals.persistent.inventory[ITEM_INDEX(G_FindItem("hyperblaster"))]) {
		client->locals.new_weapon = G_FindItem("hyperblaster");
		return;
	}
	if (client->locals.persistent.inventory[ITEM_INDEX(G_FindItem("rockets"))]
			&& client->locals.persistent.inventory[ITEM_INDEX(G_FindItem("rocket launcher"))]) {
		client->locals.new_weapon = G_FindItem("rocket launcher");
		return;
	}
	if (client->locals.persistent.inventory[ITEM_INDEX(G_FindItem("grenades"))]
			&& client->locals.persistent.inventory[ITEM_INDEX(G_FindItem("grenade launcher"))]) {
		client->locals.new_weapon = G_FindItem("grenade launcher");
		return;
	}
	if (client->locals.persistent.inventory[ITEM_INDEX(G_FindItem("bullets"))]
			&& client->locals.persistent.inventory[ITEM_INDEX(G_FindItem("machinegun"))]) {
		client->locals.new_weapon = G_FindItem("machinegun");
		return;
	}
	if (client->locals.persistent.inventory[ITEM_INDEX(G_FindItem("shells"))] > 1
			&& client->locals.persistent.inventory[ITEM_INDEX(G_FindItem("super shotgun"))]) {
		client->locals.new_weapon = G_FindItem("super shotgun");
		return;
	}
	if (client->locals.persistent.inventory[ITEM_INDEX(G_FindItem("shells"))]
			&& client->locals.persistent.inventory[ITEM_INDEX(G_FindItem("shotgun"))]) {
		client->locals.new_weapon = G_FindItem("shotgun");
		return;
	}

	client->locals.new_weapon = G_FindItem("blaster");
}

/*
 * @brief
 */
void G_UseWeapon(g_edict_t *ent, const g_item_t *item) {

	// see if we're already using it
	if (item == ent->client->locals.persistent.weapon)
		return;

	if (item->ammo) { // ensure we have ammo
		uint16_t index = ITEM_INDEX(G_FindItem(item->ammo));

		if (!ent->client->locals.persistent.inventory[index]) {
			gi.ClientPrint(ent, PRINT_HIGH, "Not enough ammo for %s\n", item->name);
			return;
		}
	}

	// change to this weapon when down
	ent->client->locals.new_weapon = item;
}

/*
 * @brief
 */
void G_DropWeapon(g_edict_t *ent, const g_item_t *item) {

	const uint16_t index = ITEM_INDEX(item);

	// see if we're already using it and we only have one
	if ((item == ent->client->locals.persistent.weapon || item == ent->client->locals.new_weapon)
			&& (ent->client->locals.persistent.inventory[index] == 1)) {
		gi.ClientPrint(ent, PRINT_HIGH, "Can't drop current weapon\n");
		return;
	}

	const g_item_t *ammo = G_FindItem(item->ammo);
	const uint16_t ammo_index = ITEM_INDEX(ammo);
	if (ent->client->locals.persistent.inventory[ammo_index] <= 0) {
		gi.ClientPrint(ent, PRINT_HIGH, "Can't drop a weapon without ammo\n");
		return;
	}

	g_edict_t *dropped = G_DropItem(ent, item);
	ent->client->locals.persistent.inventory[index]--;

	//now adjust dropped ammo quantity to reflect what we actually had available
	if (ent->client->locals.persistent.inventory[ammo_index] < ammo->quantity)
		dropped->locals.health = ent->client->locals.persistent.inventory[ammo_index];

	G_AddAmmo(ent, ammo, -1 * ammo->quantity);

}

/*
 * @brief
 */
void G_TossWeapon(g_edict_t *ent) {

	// don't drop weapon when falling into void
	if (means_of_death == MOD_TRIGGER_HURT)
		return;

	const int16_t ammo = ent->client->locals.persistent.inventory[ent->client->locals.ammo_index];

	if (!ammo)
		return; // don't drop when out of ammo

	g_edict_t *dropped = G_DropItem(ent, ent->client->locals.persistent.weapon);

	if (dropped->locals.health > ammo)
		dropped->locals.health = ammo;
}

/*
 * @brief
 */
static void G_FireWeapon(g_edict_t *ent, uint32_t interval, void(*fire)(g_edict_t *ent)) {
	int32_t n, m;
	int32_t buttons;

	buttons = (ent->client->locals.latched_buttons | ent->client->locals.buttons);

	if (!(buttons & BUTTON_ATTACK))
		return;

	ent->client->locals.latched_buttons &= ~BUTTON_ATTACK;

	// use small epsilon for low server_frame rates
	if (ent->client->locals.weapon_fire_time > g_level.time + 1)
		return;

	ent->client->locals.weapon_fire_time = g_level.time + interval;

	// determine if ammo is required, and if the quantity is sufficient
	if (ent->client->locals.ammo_index)
		n = ent->client->locals.persistent.inventory[ent->client->locals.ammo_index];
	else
		n = 0;
	m = ent->client->locals.persistent.weapon->quantity;

	// they are out of ammo
	if (ent->client->locals.ammo_index && n < m) {

		if (g_level.time >= ent->client->locals.pain_time) { // play a click sound
			gi.Sound(ent, gi.SoundIndex("weapons/common/no_ammo"), ATTN_NORM);
			ent->client->locals.pain_time = g_level.time + 1000;
		}

		G_UseBestWeapon(ent->client);
		return;
	}

	// they've pressed their fire button, and have ammo, so fire
	G_SetAnimation(ent, ANIM_TORSO_ATTACK1, true);

	if (ent->client->locals.persistent.inventory[g_level.media.quad_damage]) { // quad sound

		if (ent->client->locals.quad_attack_time < g_level.time) {
			gi.Sound(ent, gi.SoundIndex("quad/attack"), ATTN_NORM);
			ent->client->locals.quad_attack_time = g_level.time + 500;
		}
	}

	fire(ent); // fire the weapon

	// and decrease their inventory
	if (ent->client->locals.ammo_index)
		ent->client->locals.persistent.inventory[ent->client->locals.ammo_index] -= m;
}

/*
 * @brief
 */
void G_ClientWeaponThink(g_edict_t *ent) {

	if (ent->locals.health < 1)
		return;

	ent->client->locals.weapon_think_time = g_level.time;

	if (ent->client->locals.new_weapon) {
		G_ChangeWeapon(ent);
		return;
	}

	// see if we should raise the weapon
	if (ent->client->locals.weapon_fire_time >= g_level.time) {
		if (G_IsAnimation(ent, ANIM_TORSO_DROP))
			G_SetAnimation(ent, ANIM_TORSO_RAISE, false);
	}

	// call active weapon fire routine
	if (ent->client->locals.persistent.weapon && ent->client->locals.persistent.weapon->Think)
		ent->client->locals.persistent.weapon->Think(ent);
}

/*
 * @brief
 */
static void G_MuzzleFlash(g_edict_t *ent, muzzle_flash_t flash) {

	gi.WriteByte(SV_CMD_MUZZLE_FLASH);
	gi.WriteShort(ent - g_game.edicts);
	gi.WriteByte(flash);

	if (flash == MZ_BLASTER) {
		gi.WriteByte(ent->client ? ent->client->locals.persistent.color : 0);
	}

	gi.Multicast(ent->s.origin, MULTICAST_PHS);
}

/*
 * @brief
 */
static void G_FireBlaster_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	G_InitProjectile(ent, forward, right, up, org);

	G_BlasterProjectile(ent, org, forward, 1000, 15, 2);

	G_MuzzleFlash(ent, MZ_BLASTER);

	G_ClientWeaponKick(ent, 0.08);
}

void G_FireBlaster(g_edict_t *ent) {
	G_FireWeapon(ent, 500, G_FireBlaster_);
}

/*
 * @brief
 */
static void G_FireShotgun_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	G_InitProjectile(ent, forward, right, up, org);

	G_ShotgunProjectiles(ent, org, forward, 6, 4, 750, 350, 12, MOD_SHOTGUN);

	G_MuzzleFlash(ent, MZ_SHOTGUN);

	G_ClientWeaponKick(ent, 0.15);
}

void G_FireShotgun(g_edict_t *ent) {
	G_FireWeapon(ent, 1000, G_FireShotgun_);
}

/*
 * @brief
 */
static void G_FireSuperShotgun_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	ent->client->locals.angles[YAW] -= 6.0;

	G_InitProjectile(ent, forward, right, up, org);

	G_ShotgunProjectiles(ent, org, forward, 6, 4, 1000, 500, 12, MOD_SUPER_SHOTGUN);

	ent->client->locals.angles[YAW] += 12.0;

	G_InitProjectile(ent, forward, right, up, org);

	G_ShotgunProjectiles(ent, org, forward, 4, 4, 1000, 500, 12, MOD_SUPER_SHOTGUN);

	ent->client->locals.angles[YAW] -= 6.0;

	G_MuzzleFlash(ent, MZ_SSHOTGUN);

	G_ClientWeaponKick(ent, 0.2);
}

void G_FireSuperShotgun(g_edict_t *ent) {
	G_FireWeapon(ent, 1000, G_FireSuperShotgun_);
}

/*
 * @brief
 */
static void G_FireMachinegun_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	G_InitProjectile(ent, forward, right, up, org);

	G_BulletProjectile(ent, org, forward, 6, 6, 100, 200, MOD_MACHINEGUN);

	G_MuzzleFlash(ent, MZ_MACHINEGUN);

	G_ClientWeaponKick(ent, 0.125);
}

void G_FireMachinegun(g_edict_t *ent) {
	G_FireWeapon(ent, 75, G_FireMachinegun_);
}

/*
 * @brief
 */
static void G_FireGrenadeLauncher_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	const int32_t dmg = 120 + Randomc() * 30.0;
	const int32_t knockback = 120 + Randomf() * 20.0;

	G_InitProjectile(ent, forward, right, up, org);

	G_GrenadeProjectile(ent, org, forward, 700, dmg, knockback, 185.0, 2000);

	G_MuzzleFlash(ent, MZ_GRENADE);

	G_ClientWeaponKick(ent, 0.225);
}

void G_FireGrenadeLauncher(g_edict_t *ent) {
	G_FireWeapon(ent, 1000, G_FireGrenadeLauncher_);
}

/*
 * @brief
 */
static void G_FireRocketLauncher_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	const int32_t dmg = 110 + Randomf() * 20.0;
	const int32_t knockback = 110 + Randomf() * 20.0;

	G_InitProjectile(ent, forward, right, up, org);

	G_RocketProjectile(ent, org, forward, 900, dmg, knockback, 150.0);

	G_MuzzleFlash(ent, MZ_ROCKET);

	G_ClientWeaponKick(ent, 0.275);
}

void G_FireRocketLauncher(g_edict_t *ent) {
	G_FireWeapon(ent, 1000, G_FireRocketLauncher_);
}

/*
 * @brief
 */
static void G_FireHyperblaster_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	G_InitProjectile(ent, forward, right, up, org);

	G_HyperblasterProjectile(ent, org, forward, 1400, 12, 6);

	G_MuzzleFlash(ent, MZ_HYPERBLASTER);

	G_ClientWeaponKick(ent, 0.15);
}

void G_FireHyperblaster(g_edict_t *ent) {
	G_FireWeapon(ent, 100, G_FireHyperblaster_);
}

/*
 * @brief
 */
static void G_FireLightning_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	const _Bool muzzle_flash = !ent->locals.lightning;

	G_InitProjectile(ent, forward, right, up, org);

	G_LightningProjectile(ent, org, forward, 16, 12);

	if (muzzle_flash) {
		G_MuzzleFlash(ent, MZ_LIGHTNING);
	}

	G_ClientWeaponKick(ent, 0.15);
}

void G_FireLightning(g_edict_t *ent) {
	G_FireWeapon(ent, 100, G_FireLightning_);
}

/*
 * @brief
 */
static void G_FireRailgun_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	G_InitProjectile(ent, forward, right, up, org);

	G_RailgunProjectile(ent, org, forward, 120, 80);

	G_MuzzleFlash(ent, MZ_RAILGUN);

	G_ClientWeaponKick(ent, 0.3);
}

void G_FireRailgun(g_edict_t *ent) {
	G_FireWeapon(ent, 1800, G_FireRailgun_);
}

/*
 * @brief
 */
static void G_FireBfg_(g_edict_t *ent) {
	vec3_t forward, right, up, org;

	G_InitProjectile(ent, forward, right, up, org);

	G_BfgProjectiles(ent, org, forward, 600, 100, 100, 256.0);

	G_MuzzleFlash(ent, MZ_BFG);

	G_ClientWeaponKick(ent, 0.5);
}

void G_FireBfg(g_edict_t *ent) {
	G_FireWeapon(ent, 2000, G_FireBfg_);
}
