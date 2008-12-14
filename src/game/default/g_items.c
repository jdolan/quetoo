/*
* Copyright(c) 1997-2001 Id Software, Inc.
* Copyright(c) 2002 The Quakeforge Project.
* Copyright(c) 2006 Quake2World.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or(at your option) any later version.
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

static void G_DropToFloor(edict_t *ent);

// maintain indexes for frequently used models and sounds
int grenade_index, grenade_hit_index;
int rocket_index, rocket_fly_index;
int lightning_fly_index;


/*
G_ItemByIndex
*/
gitem_t *G_ItemByIndex(int index){
	if(index == 0 || index >= game.num_items)
		return NULL;

	return &itemlist[index];
}


/*
G_FindItemByClassname

*/
gitem_t *G_FindItemByClassname(const char *classname){
	int i;
	gitem_t *it;

	it = itemlist;
	for(i = 0; i < game.num_items; i++, it++){
		if(!it->classname)
			continue;
		if(!strcasecmp(it->classname, classname))
			return it;
	}

	return NULL;
}


/*
G_FindItem
*/
gitem_t *G_FindItem(const char *pickup_name){
	int i;
	gitem_t *it;

	it = itemlist;
	for(i = 0; i < game.num_items; i++, it++){
		if(!it->pickup_name)
			continue;
		if(!strcasecmp(it->pickup_name, pickup_name))
			return it;
	}

	return NULL;
}


/*
G_DoRespawn
*/
static void G_DoRespawn(edict_t *ent){
	edict_t *master;
	int count, choice;
	vec3_t origin;

	if(ent->team){  // pick a random member from the team

		master = ent->teammaster;
		VectorCopy(master->map_origin, origin);

		for(count = 0, ent = master; ent; ent = ent->chain, count++)
			;

		choice = rand() % count;

		for(count = 0, ent = master; count < choice; ent = ent->chain, count++)
			;
	}
	else {
		VectorCopy(ent->map_origin, origin);
	}

	VectorCopy(origin, ent->s.origin);
	G_DropToFloor(ent);

	ent->svflags &= ~SVF_NOCLIENT;
	ent->solid = SOLID_TRIGGER;
	gi.LinkEntity(ent);

	// send an effect
	ent->s.event = EV_ITEM_RESPAWN;
}


/*
G_SetRespawn
*/
void G_SetRespawn(edict_t *ent, float delay){
	ent->flags |= FL_RESPAWN;
	ent->svflags |= SVF_NOCLIENT;
	ent->solid = SOLID_NOT;
	ent->nextthink = level.time + delay;
	ent->think = G_DoRespawn;
	gi.LinkEntity(ent);
}


/*
PickupAdrenaline
*/
static qboolean PickupAdrenaline(edict_t *ent, edict_t *other){

	if(other->health < other->max_health)
		other->health = other->max_health;

	if(!(ent->spawnflags & SF_ITEM_DROPPED))
		G_SetRespawn(ent, 30);

	return true;
}


/*
G_AddAmmo
*/
qboolean G_AddAmmo(edict_t *ent, gitem_t *item, int count){
	int index;
	int max;

	if(!ent->client)
		return false;

	if(item->tag == AMMO_BULLETS)
		max = ent->client->locals.max_bullets;
	else if(item->tag == AMMO_SHELLS)
		max = ent->client->locals.max_shells;
	else if(item->tag == AMMO_ROCKETS)
		max = ent->client->locals.max_rockets;
	else if(item->tag == AMMO_GRENADES)
		max = ent->client->locals.max_grenades;
	else if(item->tag == AMMO_CELLS)
		max = ent->client->locals.max_cells;
	else if(item->tag == AMMO_SLUGS)
		max = ent->client->locals.max_slugs;
	else
		return false;

	index = ITEM_INDEX(item);

	if(ent->client->locals.inventory[index] == max)
		return false;

	ent->client->locals.inventory[index] += count;

	if(ent->client->locals.inventory[index] > max)
		ent->client->locals.inventory[index] = max;

	return true;
}


/*
PickupAmmo
*/
static qboolean PickupAmmo(edict_t *ent, edict_t *other){
	int count;

	if(ent->count)
		count = ent->count;
	else
		count = ent->item->quantity;

	if(!G_AddAmmo(other, ent->item, count))
		return false;

	if(!(ent->spawnflags & SF_ITEM_DROPPED))
		G_SetRespawn(ent, 20);
	return true;
}


/*
DropAmmo
*/
static void DropAmmo(edict_t *ent, gitem_t *item){
	edict_t *dropped;
	int index;

	index = ITEM_INDEX(item);
	dropped = G_DropItem(ent, item);
	if(ent->client->locals.inventory[index] >= item->quantity)
		dropped->count = item->quantity;
	else
		dropped->count = ent->client->locals.inventory[index];

	ent->client->locals.inventory[index] -= dropped->count;
}


/*
PickupHealth
*/
static qboolean PickupHealth(edict_t *ent, edict_t *other){
	int h, max;
	qboolean always_add, always_pickup;

	always_add = ent->item->tag == HEALTH_SMALL;
	always_pickup = ent->item->tag == HEALTH_SMALL || ent->item->tag == HEALTH_MEGA;

	if(other->health < other->max_health || always_add || always_pickup){

		h = other->health + ent->item->quantity;  // target health points
		max = other->max_health;

		if(always_pickup){  // resolve max
			if(other->health > 200)
				max = other->health;
			else
				max = 200;
		}
		else if(always_add)
			max = 99999;

		if(h > max)  // and enforce it
			h = max;

		other->health = other->client->locals.health = h;

		if(ent->count >= 50)  // respawn the item
			G_SetRespawn(ent, 60);
		else
			G_SetRespawn(ent, 20);

		return true;
	}

	return false;
}


/*
PickupArmor
*/
static qboolean PickupArmor(edict_t *ent, edict_t *other){
	qboolean taken = true;

	if(ent->item->tag == ARMOR_SHARD){  // take it, ignoring cap
		other->client->locals.armor += ent->item->quantity;
	}
	else if(other->client->locals.armor < other->client->locals.max_armor){

		// take it, but enforce cap
		other->client->locals.armor += ent->item->quantity;

		if(other->client->locals.armor > other->client->locals.max_armor)
			other->client->locals.armor = other->client->locals.max_armor;
	}
	else {  // don't take it
		taken = false;
	}

	if(taken && !(ent->spawnflags & SF_ITEM_DROPPED))
		G_SetRespawn(ent, 20);

	return taken;
}


/*
ResetFlag

A dropped flag has been idle for 30 seconds, return it.
*/
static void ResetFlag(edict_t *ent){
	team_t *t;
	edict_t *f;

	if(!(t = G_TeamForFlag(ent)))
		return;

	if(!(f = G_FlagForTeam(t)))
		return;

	f->svflags &= ~SVF_NOCLIENT;
	f->s.event = EV_ITEM_RESPAWN;

	gi.Sound(ent, CHAN_AUTO, gi.SoundIndex("ctf/return.wav"), 1, ATTN_NONE, 0);

	gi.Bprintf(PRINT_HIGH, "The %s flag has been returned\n", t->name);

	G_FreeEdict(ent);
}


/*
PickupFlag

Return own flag, or capture on it if enemy's flag is in inventory.
Take the enemy's flag.
*/
static qboolean PickupFlag(edict_t *ent, edict_t *other){
	team_t *t, *ot;
	edict_t *f, *of;
	int index;

	if(!other->client->locals.team)
		return false;

	if(!(t = G_TeamForFlag(ent)))
		return false;

	if(!(f = G_FlagForTeam(t)))
		return false;

	if(!(ot = G_OtherTeam(t)))
		return false;

	if(!(of = G_FlagForTeam(ot)))
		return false;

	if(t == other->client->locals.team){  // our flag

		if(ent->spawnflags & SF_ITEM_DROPPED){  // return it if necessary

			f->svflags &= ~SVF_NOCLIENT;  // and toggle the static one
			f->s.event = EV_ITEM_RESPAWN;

			gi.Sound(other, CHAN_AUTO, gi.SoundIndex("ctf/return.wav"), 1, ATTN_NONE, 0);

			gi.Bprintf(PRINT_HIGH, "%s returned the %s flag\n",
					other->client->locals.netname, t->name);

			return true;
		}

		index = ITEM_INDEX(of->item);
		if(other->client->locals.inventory[index]){  // capture

			other->client->locals.inventory[index] = 0;
			other->s.effects &= ~G_EffectForTeam(ot);

			of->svflags &= ~SVF_NOCLIENT;  // reset the other flag
			of->s.event = EV_ITEM_RESPAWN;

			gi.Sound(other, CHAN_AUTO, gi.SoundIndex("ctf/capture.wav"), 1, ATTN_NONE, 0);

			gi.Bprintf(PRINT_HIGH, "%s captured the %s flag\n",
					other->client->locals.netname, ot->name);

			t->captures++;
			other->client->locals.captures++;

			return false;
		}

		// touching our own flag for no particular reason
		return false;
	}

	// enemy's flag
	if(ent->svflags & SVF_NOCLIENT)  // already taken
		return false;

	// take it
	index = ITEM_INDEX(f->item);
	other->client->locals.inventory[index] = 1;

	gi.Sound(other, CHAN_AUTO, gi.SoundIndex("ctf/steal.wav"), 1, ATTN_NONE, 0);

	gi.Bprintf(PRINT_HIGH, "%s stole the %s flag\n",
			other->client->locals.netname, t->name);

	other->s.effects |= G_EffectForTeam(t);
	return true;
}


/*
G_TouchItem
*/
void G_TouchItem(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf){
	qboolean taken;

	if(!other->client)
		return;

	if(other->health < 1)
		return;  // dead people can't pickup

	if(!ent->item->pickup)
		return;  // not a grabbable item?

	if(level.warmup)
		return;  // warmup mode

	if((taken = ent->item->pickup(ent, other))){
		// show icon and name on status bar
		other->client->ps.stats[STAT_PICKUP_ICON] = gi.ImageIndex(ent->item->icon);
		other->client->ps.stats[STAT_PICKUP_STRING] = CS_ITEMS + ITEM_INDEX(ent->item);
		other->client->pickup_msg_time = level.time + 3.0;

		if(ent->item->pickup_sound)  // play pickup sound
			gi.Sound(other, CHAN_ITEM, gi.SoundIndex(ent->item->pickup_sound), 1, ATTN_NORM, 0);
	}

	if(!(ent->spawnflags & SF_ITEM_TARGETS_USED)){
		G_UseTargets(ent, other);
		ent->spawnflags |= SF_ITEM_TARGETS_USED;
	}

	if(!taken)
		return;

	if(ent->spawnflags & SF_ITEM_DROPPED){
		if(ent->flags & FL_RESPAWN)
			ent->flags &= ~FL_RESPAWN;
		else
			G_FreeEdict(ent);
	}
	else if(ent->item->flags & IT_FLAG)  // if a flag has been taken, hide it
		ent->svflags |= SVF_NOCLIENT;
}


/*
G_DropItem
*/
static void G_DropItemUntouchable(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf){
	if(other == ent->owner)  // prevent the dropper from picking it right back up
		return;

	G_TouchItem(ent, other, plane, surf);
}

static void G_DropItemThink(edict_t *ent){

	if(!ent->groundentity){
		ent->nextthink = level.time + gi.serverframe;
		return;
	}

	// restore full entity effects (e.g. EF_BOB)
	ent->s.effects = ent->item->effects;

	// allow anyone to pick it up
	ent->touch = G_TouchItem;

	// free it in 30 seconds
	ent->nextthink = level.time + 30.0;

	if(ent->item->flags & IT_FLAG)
		ent->think = ResetFlag;
	else
		ent->think = G_FreeEdict;
}

edict_t *G_DropItem(edict_t *ent, gitem_t *item){
	edict_t *dropped;
	vec3_t forward, right;
	vec3_t offset;
	vec3_t v;
	trace_t trace;

	dropped = G_Spawn();

	dropped->classname = item->classname;
	dropped->item = item;
	dropped->spawnflags = SF_ITEM_DROPPED;
	dropped->s.effects = (item->effects & ~EF_BOB);
	VectorSet(dropped->mins, -15, -15, -15);
	VectorSet(dropped->maxs, 15, 15, 15);
	gi.SetModel(dropped, dropped->item->model);
	dropped->solid = SOLID_TRIGGER;
	dropped->movetype = MOVETYPE_TOSS;
	dropped->touch = G_DropItemUntouchable;
	dropped->owner = ent;

	if(ent->client){
		VectorCopy(ent->client->angles, v);
		v[1] += crand() * 45;  // randomize the direction we toss in
		v[0] = 0;

		AngleVectors(v, forward, right, NULL);

		VectorSet(offset, 24, 0, -16);
		G_ProjectSource(ent->s.origin, offset, forward, right, dropped->s.origin);

		trace = gi.Trace(ent->s.origin, dropped->mins, dropped->maxs,
						 dropped->s.origin, ent, CONTENTS_SOLID);

		VectorCopy(trace.endpos, dropped->s.origin);
	} else {
		AngleVectors(ent->s.angles, forward, right, NULL);
		VectorCopy(ent->s.origin, dropped->s.origin);
	}

	VectorScale(forward, 100, dropped->velocity);
	dropped->velocity[2] = 200 + (frand() * 150);

	dropped->think = G_DropItemThink;
	dropped->nextthink = level.time + gi.serverframe;

	gi.LinkEntity(dropped);

	return dropped;
}


/*
G_UseItem
*/
static void G_UseItem(edict_t *ent, edict_t *other, edict_t *activator){
	ent->svflags &= ~SVF_NOCLIENT;
	ent->use = NULL;

	if(ent->spawnflags & SF_ITEM_NO_TOUCH){
		ent->solid = SOLID_BBOX;
		ent->touch = NULL;
	} else {
		ent->solid = SOLID_TRIGGER;
		ent->touch = G_TouchItem;
	}

	gi.LinkEntity(ent);
}


/*
G_DropToFloor
*/
static void G_DropToFloor(edict_t *ent){
	trace_t tr;
	vec3_t dest;

	VectorClear(ent->velocity);

	if(ent->spawnflags & SF_ITEM_HOVER){  // some items should not fall
		ent->movetype = MOVETYPE_FLY;
		ent->groundentity = NULL;
	}
	else {  // while most do, and will also be pushed by their ground
		VectorCopy(ent->s.origin, dest);
		dest[2] -= 8192;

		tr = gi.Trace(ent->s.origin, ent->mins, ent->maxs, dest, ent, MASK_SOLID);
		if(tr.startsolid){
			gi.Dprintf("G_DropToFloor: %s startsolid at %s\n", ent->classname, vtos(ent->s.origin));
			G_FreeEdict(ent);
			return;
		}

		VectorCopy(tr.endpos, ent->s.origin);
		ent->groundentity = tr.ent;
	}

	gi.LinkEntity(ent);
}


/*
G_PrecacheItem

Precaches all data needed for a given item.
This will be called for each item spawned in a level,
and for each item in each client's inventory.
*/
void G_PrecacheItem(gitem_t *it){
	char *s, *start;
	char data[MAX_QPATH];
	int len;
	gitem_t *ammo;

	if(!it)
		return;

	if(it->pickup_sound)
		gi.SoundIndex(it->pickup_sound);
	if(it->model)
		gi.ModelIndex(it->model);
	if(it->icon)
		gi.ImageIndex(it->icon);

	// parse everything for its ammo
	if(it->ammo && it->ammo[0]){
		ammo = G_FindItem(it->ammo);
		if(ammo != it)
			G_PrecacheItem(ammo);
	}

	// parse the space seperated precache string for other items
	s = it->precaches;
	if(!s || !s[0])
		return;

	while(*s){
		start = s;
		while(*s && *s != ' ')
			s++;

		len = s - start;
		if(len >= MAX_QPATH || len < 5)
			gi.Error("G_PrecacheItem: %s has bad precache string.", it->classname);
		memcpy(data, start, len);
		data[len] = 0;
		if(*s)
			s++;

		// determine type based on extension
		if(!strcmp(data + len - 3, "md2") || !strcmp(data + len - 3, "md3"))
			gi.ModelIndex(data);
		else if(!strcmp(data + len - 3, "wav"))
			gi.SoundIndex(data);
		else if(!strcmp(data + len - 3, "pcx") || !strcmp(data + len - 3, "tga"))
			gi.ImageIndex(data);
		else gi.Error("G_PrecacheItem: %s has unknown data type.", it->classname);
	}
}


/*
G_SpawnItem

Sets the clipping size and plants the object on the floor.

Items can't be immediately dropped to floor, because they might
be on an entity that hasn't spawned yet.
*/
void G_SpawnItem(edict_t *ent, gitem_t *item){

	G_PrecacheItem(item);

	if(ent->spawnflags){
		gi.Dprintf("%s at %s has spawnflags %d\n", ent->classname,
				vtos(ent->s.origin), ent->spawnflags);
	}

	// don't spawn flags unless ctf is enabled
	if(!level.ctf && (!strcmp(ent->classname, "item_flag_team1") ||
				!strcmp(ent->classname, "item_flag_team2"))){
		G_FreeEdict(ent);
		return;
	}

	VectorSet(ent->mins, -15, -15, -15);
	VectorSet(ent->maxs, 15, 15, 15);

	ent->solid = SOLID_TRIGGER;
	ent->movetype = MOVETYPE_TOSS;
	ent->touch = G_TouchItem;
	ent->think = G_DropToFloor;
	ent->nextthink = level.time + 2 * gi.serverframe;  // items start after other solids

	ent->item = item;
	ent->s.effects = item->effects;

	if(ent->model)
		gi.SetModel(ent, ent->model);
	else
		gi.SetModel(ent, ent->item->model);

	if(ent->team){
		ent->flags &= ~FL_TEAMSLAVE;
		ent->chain = ent->teamchain;
		ent->teamchain = NULL;

		ent->svflags |= SVF_NOCLIENT;
		ent->solid = SOLID_NOT;
		if(ent == ent->teammaster){
			ent->nextthink = level.time + gi.serverframe;
			ent->think = G_DoRespawn;
		}
	}

	if(ent->spawnflags & SF_ITEM_NO_TOUCH){
		ent->solid = SOLID_BBOX;
		ent->touch = NULL;
	}

	if(ent->spawnflags & SF_ITEM_TRIGGER){
		ent->svflags |= SVF_NOCLIENT;
		ent->solid = SOLID_NOT;
		ent->use = G_UseItem;
	}
}


gitem_t itemlist[] = {
	{
		NULL
	},     // leave index 0 alone

	//
	// ARMOR
	//

	/*QUAKED item_armor_body(.3 .3 1)(-16 -16 -16)(16 16 16)
	*/
	{
		"item_armor_body",
		PickupArmor,
		NULL,
		NULL,
		NULL,
		"armor/body/pickup.wav",
		"models/armor/body/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"i_bodyarmor",
		"Body Armor",
		200,
		NULL,
		IT_ARMOR,
		0,
		ARMOR_BODY,
		""
	},

	/*QUAKED item_armor_combat(.3 .3 1)(-16 -16 -16)(16 16 16)
	*/
	{
		"item_armor_combat",
		PickupArmor,
		NULL,
		NULL,
		NULL,
		"armor/combat/pickup.wav",
		"models/armor/combat/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"i_combatarmor",
		"Combat Armor",
		100,
		NULL,
		IT_ARMOR,
		0,
		ARMOR_COMBAT,
		""
	},

	/*QUAKED item_armor_jacket(.3 .3 1)(-16 -16 -16)(16 16 16)
	*/
	{
		"item_armor_jacket",
		PickupArmor,
		NULL,
		NULL,
		NULL,
		"armor/jacket/pickup.wav",
		"models/armor/jacket/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"i_jacketarmor",
		"Jacket Armor",
		50,
		NULL,
		IT_ARMOR,
		0,
		ARMOR_JACKET,
		""
	},

	/*QUAKED item_armor_shard(.3 .3 1)(-16 -16 -16)(16 16 16)
	*/
	{
		"item_armor_shard",
		PickupArmor,
		NULL,
		NULL,
		NULL,
		"armor/shard/pickup.wav",
		"models/armor/shard/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"i_shard",
		"Armor Shard",
		3,
		NULL,
		IT_ARMOR,
		0,
		ARMOR_SHARD,
		""
	},


	//
	// WEAPONS
	//

	/*QUAKED weapon_shotgun(.3 .3 1)(-16 -16 -16)(16 16 16)
	*/
	{
		"weapon_shotgun",
		P_PickupWeapon,
		P_UseWeapon,
		P_DropWeapon,
		P_FireShotgun,
		"weapons/common/pickup.wav",
		"models/weapons/shotgun/tris.md2",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"w_shotgun",
		"Shotgun",
		1,
		"Shells",
		IT_WEAPON,
		WEAP_SHOTGUN,
		0,
		"weapons/shotgun/fire.wav"
	},

	/*QUAKED weapon_supershotgun(.3 .3 1)(-16 -16 -16)(16 16 16)
	*/
	{
		"weapon_supershotgun",
		P_PickupWeapon,
		P_UseWeapon,
		P_DropWeapon,
		P_FireSuperShotgun,
		"weapons/common/pickup.wav",
		"models/weapons/supershotgun/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"w_sshotgun",
		"Super Shotgun",
		2,
		"Shells",
		IT_WEAPON,
		WEAP_SUPERSHOTGUN,
		0,
		"weapons/supershotgun/fire.wav"
	},

	/*QUAKED weapon_machinegun(.3 .3 1)(-16 -16 -16)(16 16 16)
	*/
	{
		"weapon_machinegun",
		P_PickupWeapon,
		P_UseWeapon,
		P_DropWeapon,
		P_FireMachinegun,
		"weapons/common/pickup.wav",
		"models/weapons/machinegun/tris.md2",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"w_machinegun",
		"Machinegun",
		1,
		"Bullets",
		IT_WEAPON,
		WEAP_MACHINEGUN,
		0,
		"weapons/machinegun/fire_1.wav weapons/machinegun/fire_2.wav "
			"weapons/machinegun/fire_3.wav weapons/machinegun/fire_4.wav"
	},

	/*QUAKED weapon_grenadelauncher(.3 .3 1)(-16 -16 -16)(16 16 16)
	*/
	{
		"weapon_grenadelauncher",
		P_PickupWeapon,
		P_UseWeapon,
		P_DropWeapon,
		P_FireGrenadeLauncher,
		"weapons/common/pickup.wav",
		"models/weapons/grenadelauncher/tris.md2",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"w_glauncher",
		"Grenade Launcher",
		1,
		"Grenades",
		IT_WEAPON,
		WEAP_GRENADELAUNCHER,
		0,
		"models/objects/grenade/tris.md2 weapons/grenadelauncher/fire.wav"
	},

	/*QUAKED weapon_rocketlauncher(.3 .3 1)(-16 -16 -16)(16 16 16)
	*/
	{
		"weapon_rocketlauncher",
		P_PickupWeapon,
		P_UseWeapon,
		P_DropWeapon,
		P_FireRocketLauncher,
		"weapons/common/pickup.wav",
		"models/weapons/rocketlauncher/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"w_rlauncher",
		"Rocket Launcher",
		1,
		"Rockets",
		IT_WEAPON,
		WEAP_ROCKETLAUNCHER,
		0,
		"models/objects/rocket/tris.md2 objects/rocket/fly.wav "
			"weapons/rocketlauncher/fire.wav"
	},

	/*QUAKED weapon_hyperblaster(.3 .3 1)(-16 -16 -16)(16 16 16)
	*/
	{
		"weapon_hyperblaster",
		P_PickupWeapon,
		P_UseWeapon,
		P_DropWeapon,
		P_FireHyperblaster,
		"weapons/common/pickup.wav",
		"models/weapons/hyperblaster/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"w_hyperblaster",
		"Hyperblaster",
		1,
		"Cells",
		IT_WEAPON,
		WEAP_HYPERBLASTER,
		0,
		"weapons/hyperblaster/fire.wav"
	},

	/*QUAKED weapon_lightning(.3 .3 1)(-16 -16 -16)(16 16 16)
	*/
	{
		"weapon_lightning",
		P_PickupWeapon,
		P_UseWeapon,
		P_DropWeapon,
		P_FireLightning,
		"weapons/common/pickup.wav",
		"models/weapons/lightning/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"w_lightning",
		"Lightning",
		1,
		"Cells",
		IT_WEAPON,
		WEAP_LIGHTNING,
		0,
		"weapons/lightning/fire.wav weapons/lightning/fly.wav"
	},

	/*QUAKED weapon_railgun(.3 .3 1)(-16 -16 -16)(16 16 16)
	*/
	{
		"weapon_railgun",
		P_PickupWeapon,
		P_UseWeapon,
		P_DropWeapon,
		P_FireRailgun,
		"weapons/common/pickup.wav",
		"models/weapons/railgun/tris.md2",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"w_railgun",
		"Railgun",
		1,
		"Slugs",
		IT_WEAPON,
		WEAP_RAILGUN,
		0,
		"weapons/railgun/fire.wav"
	},

	/*QUAKED weapon_bfg(.3 .3 1)(-16 -16 -16)(16 16 16)
	*/
	{
		"weapon_bfg",
		P_PickupWeapon,
		P_UseWeapon,
		P_DropWeapon,
		P_FireBFG,
		"weapons/common/pickup.wav",
		"models/weapons/bfg/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"w_bfg",
		"BFG10K",
		25,
		"Cells",
		IT_WEAPON,
		WEAP_BFG,
		0,
		"weapons/bfg/fire.wav weapons/bfg/hit.wav"
	},

	//
	// AMMO ITEMS
	//

	/*QUAKED ammo_shells(.3 .3 1)(-16 -16 -16)(16 16 16)
	*/
	{
		"ammo_shells",
		PickupAmmo,
		NULL,
		DropAmmo,
		NULL,
		"ammo/common/pickup.wav",
		"models/ammo/shells/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"a_shells",
		"Shells",
		10,
		NULL,
		IT_AMMO,
		0,
		AMMO_SHELLS,
		""
	},

	/*QUAKED ammo_bullets(.3 .3 1)(-16 -16 -16)(16 16 16)
	*/
	{
		"ammo_bullets",
		PickupAmmo,
		NULL,
		DropAmmo,
		NULL,
		"ammo/common/pickup.wav",
		"models/ammo/bullets/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"a_bullets",
		"Bullets",
		50,
		NULL,
		IT_AMMO,
		0,
		AMMO_BULLETS,
		""
	},

	/*QUAKED ammo_grenades(.3 .3 1)(-16 -16 -16)(16 16 16)
	*/
	{
		"ammo_grenades",
		PickupAmmo,
		NULL,
		DropAmmo,
		NULL,
		"ammo/common/pickup.wav",
		"models/ammo/grenades/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"a_grenades",
		"Grenades",
		10,
		"grenades",
		IT_AMMO,
		0,
		AMMO_GRENADES,
		""
	},

	/*QUAKED ammo_cells(.3 .3 1)(-16 -16 -16)(16 16 16)
	*/
	{
		"ammo_cells",
		PickupAmmo,
		NULL,
		DropAmmo,
		NULL,
		"ammo/common/pickup.wav",
		"models/ammo/cells/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"a_cells",
		"Cells",
		50,
		NULL,
		IT_AMMO,
		0,
		AMMO_CELLS,
		""
	},

	/*QUAKED ammo_rockets(.3 .3 1)(-16 -16 -16)(16 16 16)
	*/
	{
		"ammo_rockets",
		PickupAmmo,
		NULL,
		DropAmmo,
		NULL,
		"ammo/common/pickup.wav",
		"models/ammo/rockets/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"a_rockets",
		"Rockets",
		10,
		NULL,
		IT_AMMO,
		0,
		AMMO_ROCKETS,
		""
	},

	/*QUAKED ammo_slugs(.3 .3 1)(-16 -16 -16)(16 16 16)
	*/
	{
		"ammo_slugs",
		PickupAmmo,
		NULL,
		DropAmmo,
		NULL,
		"ammo/common/pickup.wav",
		"models/ammo/slugs/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"a_slugs",
		"Slugs",
		10,
		NULL,
		IT_AMMO,
		0,
		AMMO_SLUGS,
		""
	},

	/*QUAKED item_adrenaline(.3 .3 1)(-16 -16 -16)(16 16 16)
	gives +1 to maximum health
	*/
	{
		"item_adrenaline",
		PickupAdrenaline,
		NULL,
		NULL,
		NULL,
		"adren/pickup.wav",
		"models/adren/tris.md2",
		EF_ROTATE | EF_PULSE,
		"p_adrenaline",
		"Adrenaline",
		0,
		NULL,
		0,
		0,
		0,
		""
	},

	/*QUAKED item_health_small (.3 .3 1) (-16 -16 -16) (16 16 16)
	*/
	{
		"item_health_small",
		PickupHealth,
		NULL,
		NULL,
		NULL,
		"health/small/pickup.wav",
		"models/health/small/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"i_health",
		"Small Health",
		3,
		NULL,
		IT_HEALTH,
		0,
		HEALTH_SMALL,
		""
	},

	/*QUAKED item_health (.3 .3 1) (-16 -16 -16) (16 16 16)
	*/
	{
		"item_health",
		PickupHealth,
		NULL,
		NULL,
		NULL,
		"health/medium/pickup.wav",
		"models/health/medium/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"i_health",
		"Medium Health",
		15,
		NULL,
		IT_HEALTH,
		0,
		HEALTH_MEDIUM,
		""
	},

	/*QUAKED item_health_large (.3 .3 1) (-16 -16 -16) (16 16 16)
	*/
	{
		"item_health_large",
		PickupHealth,
		NULL,
		NULL,
		NULL,
		"health/large/pickup.wav",
		"models/health/large/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"i_health",
		"Large Health",
		25,
		NULL,
		IT_HEALTH,
		0,
		HEALTH_LARGE,
		""
	},

	/*QUAKED item_health_mega (.3 .3 1) (-16 -16 -16) (16 16 16)
	*/
	{
		"item_health_mega",
		PickupHealth,
		NULL,
		NULL,
		NULL,
		"health/mega/pickup.wav",
		"models/health/mega/tris.md2",
		EF_PULSE,
		"i_health",
		"Mega Health",
		75,
		NULL,
		IT_HEALTH,
		0,
		HEALTH_MEGA,
		""
	},

	/*QUAKED item_flag_team1(1 0.2 0)(-16 -16 -24)(16 16 32)
	*/
	{
		"item_flag_team1",
		PickupFlag,
		NULL,
		NULL,
		NULL,
		NULL,
		"models/ctf/flag1/tris.md3",
		EF_BOB | EF_ROTATE,
		"i_flag1",
		"Flag",
		0,
		NULL,
		IT_FLAG,
		0,
		0,
		"ctf/capture.wav ctf/steal.wav ctf/return.wav"
	},

	/*QUAKED item_flag_team2(1 0.2 0)(-16 -16 -24)(16 16 32)
	*/
	{
		"item_flag_team2",
		PickupFlag,
		NULL,
		NULL,
		NULL,
		NULL,
		"models/ctf/flag2/tris.md3",
		EF_BOB | EF_ROTATE,
		"i_flag2",
		"Flag",
		0,
		NULL,
		IT_FLAG,
		0,
		0,
		"ctf/capture.wav ctf/steal.wav ctf/return.wav"
	},

	// end of list marker
	{NULL}
};


// override quake2 items for legacy maps
override_t overrides[] = {
	{"weapon_chaingun", "weapon_machinegun"},
	{"item_quad", "item_armor_body"},
	{"item_invulnerability", "item_adrenaline"},
	{"item_power_shield", "item_armor_combat"},
	{"item_power_screen", "item_armor_jacket"},
	{NULL}
};


/*
G_InitItems
*/
void G_InitItems(void){
	game.num_items = sizeof(itemlist) / sizeof(itemlist[0]) - 1;
	game.num_overrides = sizeof(overrides) / sizeof(overrides[0]) - 1;
}


/*
G_SetItemNames

Called by worldspawn
*/
void G_SetItemNames(void){
	int i, j;
	gitem_t *it;
	override_t *ov;

	for(i = 0; i < game.num_items; i++){
		it = &itemlist[i];
		if(it->classname){
			for(j = 0; j < game.num_overrides; j++){
				ov = &overrides[j];
				if(!strcmp(it->classname, ov->old)){
					it = G_FindItemByClassname(ov->new);
					break;
				}
			}
		}
		gi.Configstring(CS_ITEMS + i, it->pickup_name);
	}

	grenade_index = gi.ModelIndex("models/objects/grenade/tris.md3");
	grenade_hit_index = gi.SoundIndex("objects/grenade/hit.wav");

	rocket_index = gi.ModelIndex("models/objects/rocket/tris.md3");
	rocket_fly_index = gi.SoundIndex("objects/rocket/fly.wav");

	lightning_fly_index = gi.SoundIndex("weapons/lightning/fly.wav");
}
