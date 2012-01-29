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

static void G_DropToFloor(g_edict_t *ent);

// maintain indexes for frequently used models and sounds
unsigned short grenade_index, grenade_hit_index;
unsigned short rocket_index, rocket_fly_index;
unsigned short lightning_fly_index;
unsigned short quad_damage_index;

/*
 * G_ItemByIndex
 */
g_item_t *G_ItemByIndex(unsigned short index) {

	if (index == 0 || index >= g_game.num_items)
		return NULL;

	return &g_items[index];
}

/*
 * G_FindItemByClassname
 *
 */
g_item_t *G_FindItemByClassname(const char *class_name) {
	int i;
	g_item_t *it;

	it = g_items;
	for (i = 0; i < g_game.num_items; i++, it++) {

		if (!it->class_name)
			continue;

		if (!strcasecmp(it->class_name, class_name))
			return it;
	}

	return NULL;
}

/*
 * G_FindItem
 */
g_item_t *G_FindItem(const char *pickup_name) {
	int i;
	g_item_t *it;

	it = g_items;

	for (i = 0; i < g_game.num_items; i++, it++) {

		if (!it->pickup_name)
			continue;

		if (!strcasecmp(it->pickup_name, pickup_name))
			return it;
	}

	return NULL;
}

/*
 * G_ItemRespawn
 */
static void G_ItemRespawn(g_edict_t *ent) {
	g_edict_t *master;
	int count, choice;
	vec3_t origin;

	if (ent->team) { // pick a random member from the team

		master = ent->team_master;
		VectorCopy(master->map_origin, origin);

		for (count = 0, ent = master; ent; ent = ent->chain, count++)
			;

		choice = rand() % count;

		for (count = 0, ent = master; count < choice; ent = ent->chain, count++)
			;
	} else {
		VectorCopy(ent->map_origin, origin);
	}

	VectorCopy(origin, ent->s.origin);
	G_DropToFloor(ent);

	ent->sv_flags &= ~SVF_NO_CLIENT;
	ent->solid = SOLID_TRIGGER;
	gi.LinkEntity(ent);

	// send an effect
	ent->s.event = EV_ITEM_RESPAWN;
}

/*
 * G_SetItemRespawn
 */
void G_SetItemRespawn(g_edict_t *ent, float delay) {
	ent->flags |= FL_RESPAWN;
	ent->sv_flags |= SVF_NO_CLIENT;
	ent->solid = SOLID_NOT;
	ent->next_think = g_level.time + delay;
	ent->think = G_ItemRespawn;
	gi.LinkEntity(ent);
}

/*
 * G_PickupAdrenaline
 */
static boolean_t G_PickupAdrenaline(g_edict_t *ent, g_edict_t *other) {

	if (other->health < other->max_health)
		other->health = other->max_health;

	if (!(ent->spawn_flags & SF_ITEM_DROPPED))
		G_SetItemRespawn(ent, 30);

	return true;
}

/*
 * G_PickupQuadDamage
 */
static boolean_t G_PickupQuadDamage(g_edict_t *ent, g_edict_t *other) {

	if (other->client->persistent.inventory[quad_damage_index])
		return false; // already have it

	other->client->persistent.inventory[quad_damage_index] = 1;

	if (ent->spawn_flags & SF_ITEM_DROPPED) { // receive only the time left
		other->client->quad_damage_time = ent->next_think;
	} else {
		other->client->quad_damage_time = g_level.time + 30.0;
		G_SetItemRespawn(ent, ent->item->quantity);
	}

	other->s.effects |= EF_QUAD;
	return true;
}

/*
 * G_TossQuadDamage
 */
void G_TossQuadDamage(g_edict_t *ent) {
	g_edict_t *quad;

	if (!ent->client->persistent.inventory[quad_damage_index])
		return;

	quad = G_DropItem(ent, G_FindItemByClassname("item_quad"));

	if (quad)
		quad->timestamp = ent->client->quad_damage_time;

	ent->client->quad_damage_time = 0.0;
	ent->client->persistent.inventory[quad_damage_index] = 0;
}

/*
 * G_AddAmmo
 */
boolean_t G_AddAmmo(g_edict_t *ent, g_item_t *item, short count) {
	unsigned short index;
	short max;

	if (item->tag == AMMO_SHELLS)
		max = ent->client->persistent.max_shells;
	else if (item->tag == AMMO_BULLETS)
		max = ent->client->persistent.max_bullets;
	else if (item->tag == AMMO_GRENADES)
		max = ent->client->persistent.max_grenades;
	else if (item->tag == AMMO_ROCKETS)
		max = ent->client->persistent.max_rockets;
	else if (item->tag == AMMO_CELLS)
		max = ent->client->persistent.max_cells;
	else if (item->tag == AMMO_BOLTS)
		max = ent->client->persistent.max_bolts;
	else if (item->tag == AMMO_SLUGS)
		max = ent->client->persistent.max_slugs;
	else if (item->tag == AMMO_NUKES)
		max = ent->client->persistent.max_nukes;
	else
		return false;

	index = ITEM_INDEX(item);

	if (ent->client->persistent.inventory[index] == max)
		return false;

	ent->client->persistent.inventory[index] += count;

	if (ent->client->persistent.inventory[index] > max)
		ent->client->persistent.inventory[index] = max;

	return true;
}

/*
 * G_PickupAmmo
 */
static boolean_t G_PickupAmmo(g_edict_t *ent, g_edict_t *other) {
	int count;

	if (ent->count)
		count = ent->count;
	else
		count = ent->item->quantity;

	if (!G_AddAmmo(other, ent->item, count))
		return false;

	if (!(ent->spawn_flags & SF_ITEM_DROPPED))
		G_SetItemRespawn(ent, 20);

	return true;
}

/*
 * G_DropAmmo
 */
static void G_DropAmmo(g_edict_t *ent, g_item_t *item) {
	g_edict_t *dropped;
	int index;

	index = ITEM_INDEX(item);
	dropped = G_DropItem(ent, item);

	if (!dropped)
		return;

	if (ent->client->persistent.inventory[index] >= item->quantity)
		dropped->count = item->quantity;
	else
		dropped->count = ent->client->persistent.inventory[index];

	ent->client->persistent.inventory[index] -= dropped->count;
}

/*
 * G_PickupHealth
 */
static boolean_t G_PickupHealth(g_edict_t *ent, g_edict_t *other) {
	int h, max;
	boolean_t always_add, always_pickup;

	always_add = ent->item->tag == HEALTH_SMALL;
	always_pickup = ent->item->tag == HEALTH_SMALL || ent->item->tag
			== HEALTH_MEGA;

	if (other->health < other->max_health || always_add || always_pickup) {

		h = other->health + ent->item->quantity; // target health points
		max = other->max_health;

		if (always_pickup) { // resolve max
			if (other->health > 200)
				max = other->health;
			else
				max = 200;
		} else if (always_add)
			max = 99999;

		if (h > max) // and enforce it
			h = max;

		other->health = other->client->persistent.health = h;

		if (ent->count >= 50) // respawn the item
			G_SetItemRespawn(ent, 60);
		else
			G_SetItemRespawn(ent, 20);

		return true;
	}

	return false;
}

/*
 * G_PickupArmor
 */
static boolean_t G_PickupArmor(g_edict_t *ent, g_edict_t *other) {
	boolean_t taken = true;

	if (ent->item->tag == ARMOR_SHARD) { // take it, ignoring cap
		other->client->persistent.armor += ent->item->quantity;
	} else if (other->client->persistent.armor < other->client->persistent.max_armor) {

		// take it, but enforce cap
		other->client->persistent.armor += ent->item->quantity;

		if (other->client->persistent.armor > other->client->persistent.max_armor)
			other->client->persistent.armor = other->client->persistent.max_armor;
	} else { // don't take it
		taken = false;
	}

	if (taken && !(ent->spawn_flags & SF_ITEM_DROPPED))
		G_SetItemRespawn(ent, 20);

	return taken;
}

/*
 * G_ResetFlag
 *
 * A dropped flag has been idle for 30 seconds, return it.
 */
void G_ResetFlag(g_edict_t *ent) {
	g_team_t *t;
	g_edict_t *f;

	if (!(t = G_TeamForFlag(ent)))
		return;

	if (!(f = G_FlagForTeam(t)))
		return;

	f->sv_flags &= ~SVF_NO_CLIENT;
	f->s.event = EV_ITEM_RESPAWN;

	gi.Sound(ent, gi.SoundIndex("ctf/return"), ATTN_NONE);

	gi.BroadcastPrint(PRINT_HIGH, "The %s flag has been returned\n", t->name);

	G_FreeEdict(ent);
}

/*
 * G_PickupFlag
 *
 * Return own flag, or capture on it if enemy's flag is in inventory.
 * Take the enemy's flag.
 */
static boolean_t G_PickupFlag(g_edict_t *ent, g_edict_t *other) {
	g_team_t *t, *ot;
	g_edict_t *f, *of;
	int index;

	if (!other->client->persistent.team)
		return false;

	if (!(t = G_TeamForFlag(ent)))
		return false;

	if (!(f = G_FlagForTeam(t)))
		return false;

	if (!(ot = G_OtherTeam(t)))
		return false;

	if (!(of = G_FlagForTeam(ot)))
		return false;

	if (t == other->client->persistent.team) { // our flag

		if (ent->spawn_flags & SF_ITEM_DROPPED) { // return it if necessary

			f->sv_flags &= ~SVF_NO_CLIENT; // and toggle the static one
			f->s.event = EV_ITEM_RESPAWN;

			gi.Sound(other, gi.SoundIndex("ctf/return"), ATTN_NONE);

			gi.BroadcastPrint(PRINT_HIGH, "%s returned the %s flag\n",
					other->client->persistent.net_name, t->name);

			return true;
		}

		index = ITEM_INDEX(of->item);
		if (other->client->persistent.inventory[index]) { // capture

			other->client->persistent.inventory[index] = 0;
			other->s.effects &= ~G_EffectForTeam(ot);
			other->s.model3 = 0;

			of->sv_flags &= ~SVF_NO_CLIENT; // reset the other flag
			of->s.event = EV_ITEM_RESPAWN;

			gi.Sound(other, gi.SoundIndex("ctf/capture"), ATTN_NONE);

			gi.BroadcastPrint(PRINT_HIGH, "%s captured the %s flag\n",
					other->client->persistent.net_name, ot->name);

			t->captures++;
			other->client->persistent.captures++;

			return false;
		}

		// touching our own flag for no particular reason
		return false;
	}

	// enemy's flag
	if (ent->sv_flags & SVF_NO_CLIENT) // already taken
		return false;

	// take it
	index = ITEM_INDEX(f->item);
	other->client->persistent.inventory[index] = 1;

	// link the flag model to the player
	other->s.model3 = gi.ModelIndex(f->item->model);

	gi.Sound(other, gi.SoundIndex("ctf/steal"), ATTN_NONE);

	gi.BroadcastPrint(PRINT_HIGH, "%s stole the %s flag\n",
			other->client->persistent.net_name, t->name);

	other->s.effects |= G_EffectForTeam(t);
	return true;
}

/*
 * G_TossFlag
 */
void G_TossFlag(g_edict_t *ent) {
	g_team_t *ot;
	g_edict_t *of;
	int index;

	if (!(ot = G_OtherTeam(ent->client->persistent.team)))
		return;

	if (!(of = G_FlagForTeam(ot)))
		return;

	index = ITEM_INDEX(of->item);

	if (!ent->client->persistent.inventory[index])
		return;

	ent->client->persistent.inventory[index] = 0;

	ent->s.model3 = 0;
	ent->s.effects &= ~(EF_CTF_RED | EF_CTF_BLUE);

	gi.BroadcastPrint(PRINT_HIGH, "%s dropped the %s flag\n",
			ent->client->persistent.net_name, ot->name);

	G_DropItem(ent, of->item);
}

/*
 * G_DropFlag
 */
static void G_DropFlag(g_edict_t *ent, g_item_t *item) {
	G_TossFlag(ent);
}

/*
 * G_TouchItem
 */
void G_TouchItem(g_edict_t *ent, g_edict_t *other, c_bsp_plane_t *plane,
		c_bsp_surface_t *surf) {
	boolean_t taken;

	if (!other->client)
		return;

	if (other->health < 1)
		return; // dead people can't pickup

	if (!ent->item->pickup)
		return; // item can't be picked up

	if (g_level.warmup)
		return; // warmup mode

	if ((taken = ent->item->pickup(ent, other))) {
		// show icon and name on status bar
		other->client->ps.stats[STAT_PICKUP_ICON] = gi.ImageIndex(
				ent->item->icon);
		other->client->ps.stats[STAT_PICKUP_STRING] = CS_ITEMS
				+ ITEM_INDEX(ent->item);
		other->client->pickup_msg_time = g_level.time + 3.0;

		if (ent->item->pickup_sound) { // play pickup sound
			gi.Sound(other, gi.SoundIndex(ent->item->pickup_sound), ATTN_NORM);
		}

		other->s.event = EV_ITEM_PICKUP;
	}

	if (!(ent->spawn_flags & SF_ITEM_TARGETS_USED)) {
		G_UseTargets(ent, other);
		ent->spawn_flags |= SF_ITEM_TARGETS_USED;
	}

	if (!taken)
		return;

	if (ent->spawn_flags & SF_ITEM_DROPPED) {
		if (ent->flags & FL_RESPAWN)
			ent->flags &= ~FL_RESPAWN;
		else
			G_FreeEdict(ent);
	} else if (ent->item->type == ITEM_FLAG) // if a flag has been taken, hide it
		ent->sv_flags |= SVF_NO_CLIENT;
}

/*
 * G_DropItemUntouchable
 */
static void G_DropItemUntouchable(g_edict_t *ent, g_edict_t *other,
		c_bsp_plane_t *plane, c_bsp_surface_t *surf) {

	if (other == ent->owner) // prevent the dropper from picking it right back up
		return;

	G_TouchItem(ent, other, plane, surf);
}

/*
 * G_DropItemThink
 */
static void G_DropItemThink(g_edict_t *ent) {
	int contents;
	float f;

	if (!ent->ground_entity) { // keep falling in valid space
		ent->next_think = g_level.time + gi.server_frame;
		return;
	}

	// see if we've landed on a trigger_hurt
	G_TouchTriggers(ent);

	if (!ent->in_use) // we have
		return;

	// restore full entity effects (e.g. EF_BOB)
	ent->s.effects = ent->item->effects;

	// allow anyone to pick it up
	ent->touch = G_TouchItem;

	// setup the next think function and time

	if (ent->item->type == ITEM_FLAG) // flags go back to base
		ent->think = G_ResetFlag;
	else
		// everything else just gets freed
		ent->think = G_FreeEdict;

	if (ent->item->type == ITEM_POWERUP) // expire from last touch
		f = ent->timestamp - g_level.time;
	else
		// general case
		f = 30.0;

	contents = gi.PointContents(ent->s.origin);

	if (contents & CONTENTS_LAVA) // expire more quickly in lava
		f *= 0.3;
	if (contents & CONTENTS_SLIME) // and slime
		f *= 0.5;

	ent->next_think = g_level.time + f;
}

/*
 * G_DropItem
 *
 * Handles the mechanics of dropping items, but does not adjust the client's
 * inventory. That is left to the caller.
 */
g_edict_t *G_DropItem(g_edict_t *ent, g_item_t *item) {
	g_edict_t *dropped;
	vec3_t forward;
	vec3_t v;
	c_trace_t trace;

	dropped = G_Spawn();

	dropped->class_name = item->class_name;
	dropped->item = item;
	dropped->spawn_flags = SF_ITEM_DROPPED;
	dropped->s.effects = (item->effects & ~EF_BOB);
	VectorSet(dropped->mins, -15, -15, -15);
	VectorSet(dropped->maxs, 15, 15, 15);
	gi.SetModel(dropped, dropped->item->model);
	dropped->solid = SOLID_TRIGGER;
	dropped->move_type = MOVE_TYPE_TOSS;
	dropped->touch = G_DropItemUntouchable;
	dropped->owner = ent;

	if (ent->client) { // randomize the direction we toss in
		VectorSet(v, 0.0, ent->client->angles[1] + crand() * 45.0, 0.0);
		AngleVectors(v, forward, NULL, NULL);

		VectorMA(ent->s.origin, 24.0, forward, dropped->s.origin);

		trace = gi.Trace(ent->s.origin, dropped->mins, dropped->maxs,
				dropped->s.origin, ent, CONTENTS_SOLID);

		VectorCopy(trace.end, dropped->s.origin);
	} else {
		AngleVectors(ent->s.angles, forward, NULL, NULL);

		trace = gi.Trace(ent->s.origin, dropped->mins, dropped->maxs,
				ent->s.origin, ent, CONTENTS_SOLID);

		VectorCopy(ent->s.origin, dropped->s.origin);
	}

	// we're in a bad spot, forget it
	if (trace.start_solid) {

		if (item->type == ITEM_FLAG)
			G_ResetFlag(dropped);
		else
			G_FreeEdict(dropped);

		return NULL;
	}

	VectorScale(forward, 100.0, dropped->velocity);
	dropped->velocity[2] = 200.0 + (frand() * 150.0);

	dropped->think = G_DropItemThink;
	dropped->next_think = g_level.time + gi.server_frame;

	gi.LinkEntity(dropped);

	return dropped;
}

/*
 * G_UseItem
 */
static void G_UseItem(g_edict_t *ent, g_edict_t *other, g_edict_t *activator) {
	ent->sv_flags &= ~SVF_NO_CLIENT;
	ent->use = NULL;

	if (ent->spawn_flags & SF_ITEM_NO_TOUCH) {
		ent->solid = SOLID_BOX;
		ent->touch = NULL;
	} else {
		ent->solid = SOLID_TRIGGER;
		ent->touch = G_TouchItem;
	}

	gi.LinkEntity(ent);
}

/*
 * G_DropToFloor
 */
static void G_DropToFloor(g_edict_t *ent) {
	c_trace_t tr;
	vec3_t dest;

	VectorClear(ent->velocity);

	if (ent->spawn_flags & SF_ITEM_HOVER) { // some items should not fall
		ent->move_type = MOVE_TYPE_FLY;
		ent->ground_entity = NULL;
	} else { // while most do, and will also be pushed by their ground
		VectorCopy(ent->s.origin, dest);
		dest[2] -= 8192;

		tr = gi.Trace(ent->s.origin, ent->mins, ent->maxs, dest, ent,
				MASK_SOLID);
		if (tr.start_solid) {
			gi.Debug("G_DropToFloor: %s start_solid at %s\n", ent->class_name,
					vtos(ent->s.origin));
			G_FreeEdict(ent);
			return;
		}

		VectorCopy(tr.end, ent->s.origin);
		ent->ground_entity = tr.ent;
	}

	gi.LinkEntity(ent);
}

/*
 * G_PrecacheItem
 *
 * Precaches all data needed for a given item.
 * This will be called for each item spawned in a level,
 * and for each item in each client's inventory.
 */
void G_PrecacheItem(g_item_t *it) {
	char *s, *start;
	char data[MAX_QPATH];
	int len;
	g_item_t *ammo;

	if (!it)
		return;

	if (it->pickup_sound)
		gi.SoundIndex(it->pickup_sound);
	if (it->model)
		gi.ModelIndex(it->model);
	if (it->icon)
		gi.ImageIndex(it->icon);

	// parse everything for its ammo
	if (it->ammo && it->ammo[0]) {
		ammo = G_FindItem(it->ammo);
		if (ammo != it)
			G_PrecacheItem(ammo);
	}

	// parse the space seperated precache string for other items
	s = it->precaches;
	if (!s || !s[0])
		return;

	while (*s) {
		start = s;
		while (*s && *s != ' ')
			s++;

		len = s - start;
		if (len >= MAX_QPATH || len < 5)
			gi.Error("G_PrecacheItem: %s has bad precache string.",
					it->class_name);
		memcpy(data, start, len);
		data[len] = 0;
		if (*s)
			s++;

		// determine type based on extension
		if (!strcmp(data + len - 3, "md3") || !strcmp(data + len - 3, "obj"))
			gi.ModelIndex(data);
		else if (!strcmp(data + len - 3, "wav"))
			gi.SoundIndex(data);
		else if (!strcmp(data + len - 3, "tga") || !strcmp(data + len - 3,
				"png"))
			gi.ImageIndex(data);
		else
			gi.Error("G_PrecacheItem: %s has unknown data type.",
					it->class_name);
	}
}

/*
 * G_SpawnItem
 *
 * Sets the clipping size and plants the object on the floor.
 *
 * Items can't be immediately dropped to floor, because they might
 * be on an entity that hasn't spawned yet.
 */
void G_SpawnItem(g_edict_t *ent, g_item_t *item) {

	G_PrecacheItem(item);

	if (ent->spawn_flags) {
		gi.Debug("%s at %s has spawnflags %d\n", ent->class_name,
				vtos(ent->s.origin), ent->spawn_flags);
	}

	VectorSet(ent->mins, -15, -15, -15);
	VectorSet(ent->maxs, 15, 15, 15);

	ent->solid = SOLID_TRIGGER;
	ent->move_type = MOVE_TYPE_TOSS;
	ent->touch = G_TouchItem;
	ent->think = G_DropToFloor;
	ent->next_think = g_level.time + 2 * gi.server_frame; // items start after other solids

	ent->item = item;
	ent->s.effects = item->effects;

	if (ent->model)
		gi.SetModel(ent, ent->model);
	else
		gi.SetModel(ent, ent->item->model);

	if (ent->team) {
		ent->flags &= ~FL_TEAM_SLAVE;
		ent->chain = ent->team_chain;
		ent->team_chain = NULL;

		ent->sv_flags |= SVF_NO_CLIENT;
		ent->solid = SOLID_NOT;
		if (ent == ent->team_master) {
			ent->next_think = g_level.time + gi.server_frame;
			ent->think = G_ItemRespawn;
		}
	}

	// hide flags unless ctf is enabled
	if (!g_level.ctf && (!strcmp(ent->class_name, "item_flag_team1")
			|| !strcmp(ent->class_name, "item_flag_team2"))) {

		ent->sv_flags |= SVF_NO_CLIENT;
		ent->solid = SOLID_NOT;
	}

	if (ent->spawn_flags & SF_ITEM_NO_TOUCH) {
		ent->solid = SOLID_BOX;
		ent->touch = NULL;
	}

	if (ent->spawn_flags & SF_ITEM_TRIGGER) {
		ent->sv_flags |= SVF_NO_CLIENT;
		ent->solid = SOLID_NOT;
		ent->use = G_UseItem;
	}
}

g_item_t
		g_items[] =
				{
						{ NULL, }, // leave index 0 alone

						//
						// ARMOR
						//

						/*QUAKED item_armor_body(.3 .3 1)(-16 -16 -16)(16 16 16)
						 */
						{ "item_armor_body", G_PickupArmor, NULL, NULL, NULL,
								"armor/body/pickup.wav",
								"models/armor/body/tris.md3", EF_ROTATE
										| EF_BOB | EF_PULSE, "i_bodyarmor",
								"Body Armor", 200, NULL, ITEM_ARMOR,
								ARMOR_BODY, "" },

						/*QUAKED item_armor_combat(.3 .3 1)(-16 -16 -16)(16 16 16)
						 */
						{ "item_armor_combat", G_PickupArmor, NULL, NULL, NULL,
								"armor/combat/pickup.wav",
								"models/armor/combat/tris.md3", EF_ROTATE
										| EF_BOB | EF_PULSE, "i_combatarmor",
								"Combat Armor", 100, NULL, ITEM_ARMOR,
								ARMOR_COMBAT, "" },

						/*QUAKED item_armor_jacket(.3 .3 1)(-16 -16 -16)(16 16 16)
						 */
						{ "item_armor_jacket", G_PickupArmor, NULL, NULL, NULL,
								"armor/jacket/pickup.wav",
								"models/armor/jacket/tris.md3", EF_ROTATE
										| EF_BOB | EF_PULSE, "i_jacketarmor",
								"Jacket Armor", 50, NULL, ITEM_ARMOR,
								ARMOR_JACKET, "" },

						/*QUAKED item_armor_shard(.3 .3 1)(-16 -16 -16)(16 16 16)
						 */
						{ "item_armor_shard", G_PickupArmor, NULL, NULL, NULL,
								"armor/shard/pickup.wav",
								"models/armor/shard/tris.md3", EF_ROTATE
										| EF_BOB | EF_PULSE, "i_shard",
								"Armor Shard", 3, NULL, ITEM_ARMOR,
								ARMOR_SHARD, "" },

						//
						// WEAPONS
						//

						/*QUAKED weapon_shotgun(.3 .3 1)(-16 -16 -16)(16 16 16)
						 */
						{ "weapon_shotgun", G_PickupWeapon, G_UseWeapon,
								G_DropWeapon, G_FireShotgun,
								"weapons/common/pickup.wav",
								"models/weapons/shotgun/tris.obj", EF_ROTATE
										| EF_BOB | EF_PULSE, "w_shotgun",
								"Shotgun", 1, "Shells", ITEM_WEAPON, 0,
								"weapons/shotgun/fire.wav" },

						/*QUAKED weapon_supershotgun(.3 .3 1)(-16 -16 -16)(16 16 16)
						 */
						{ "weapon_supershotgun", G_PickupWeapon, G_UseWeapon,
								G_DropWeapon, G_FireSuperShotgun,
								"weapons/common/pickup.wav",
								"models/weapons/supershotgun/tris.obj",
								EF_ROTATE | EF_BOB | EF_PULSE, "w_sshotgun",
								"Super Shotgun", 2, "Shells", ITEM_WEAPON, 0,
								"weapons/supershotgun/fire.wav" },

						/*QUAKED weapon_machinegun(.3 .3 1)(-16 -16 -16)(16 16 16)
						 */
						{ "weapon_machinegun", G_PickupWeapon, G_UseWeapon,
								G_DropWeapon, G_FireMachinegun,
								"weapons/common/pickup.wav",
								"models/weapons/machinegun/tris.obj", EF_ROTATE
										| EF_BOB | EF_PULSE, "w_machinegun",
								"Machinegun", 1, "Bullets", ITEM_WEAPON, 0,
								"weapons/machinegun/fire_1.wav weapons/machinegun/fire_2.wav "
									"weapons/machinegun/fire_3.wav weapons/machinegun/fire_4.wav" },

						/*QUAKED weapon_grenadelauncher(.3 .3 1)(-16 -16 -16)(16 16 16)
						 */
						{ "weapon_grenadelauncher", G_PickupWeapon,
								G_UseWeapon, G_DropWeapon,
								G_FireGrenadeLauncher,
								"weapons/common/pickup.wav",
								"models/weapons/grenadelauncher/tris.obj",
								EF_ROTATE | EF_BOB | EF_PULSE, "w_glauncher",
								"Grenade Launcher", 1, "Grenades", ITEM_WEAPON,
								0,
								"models/objects/grenade/tris.md3 weapons/grenadelauncher/fire.wav" },

						/*QUAKED weapon_rocketlauncher(.3 .3 1)(-16 -16 -16)(16 16 16)
						 */
						{ "weapon_rocketlauncher", G_PickupWeapon, G_UseWeapon,
								G_DropWeapon, G_FireRocketLauncher,
								"weapons/common/pickup.wav",
								"models/weapons/rocketlauncher/tris.md3",
								EF_ROTATE | EF_BOB | EF_PULSE, "w_rlauncher",
								"Rocket Launcher", 1, "Rockets", ITEM_WEAPON,
								0,
								"models/objects/rocket/tris.md3 objects/rocket/fly.wav "
									"weapons/rocketlauncher/fire.wav" },

						/*QUAKED weapon_hyperblaster(.3 .3 1)(-16 -16 -16)(16 16 16)
						 */
						{ "weapon_hyperblaster", G_PickupWeapon, G_UseWeapon,
								G_DropWeapon, G_FireHyperblaster,
								"weapons/common/pickup.wav",
								"models/weapons/hyperblaster/tris.md3",
								EF_ROTATE | EF_BOB | EF_PULSE,
								"w_hyperblaster", "Hyperblaster", 1, "Cells",
								ITEM_WEAPON, 0,
								"weapons/hyperblaster/fire.wav weapons/hyperblaster/hit.wav" },

						/*QUAKED weapon_lightning(.3 .3 1)(-16 -16 -16)(16 16 16)
						 */
						{ "weapon_lightning", G_PickupWeapon, G_UseWeapon,
								G_DropWeapon, G_FireLightning,
								"weapons/common/pickup.wav",
								"models/weapons/lightning/tris.md3", EF_ROTATE
										| EF_BOB | EF_PULSE, "w_lightning",
								"Lightning", 1, "Bolts", ITEM_WEAPON, 0,
								"weapons/lightning/fire.wav weapons/lightning/fly.wav "
									"weapons/lightning/discharge.wav" },

						/*QUAKED weapon_railgun(.3 .3 1)(-16 -16 -16)(16 16 16)
						 */
						{ "weapon_railgun", G_PickupWeapon, G_UseWeapon,
								G_DropWeapon, G_FireRailgun,
								"weapons/common/pickup.wav",
								"models/weapons/railgun/tris.obj", EF_ROTATE
										| EF_BOB | EF_PULSE, "w_railgun",
								"Railgun", 1, "Slugs", ITEM_WEAPON, 0,
								"weapons/railgun/fire.wav" },

						/*QUAKED weapon_bfg(.3 .3 1)(-16 -16 -16)(16 16 16)
						 */
						{ "weapon_bfg", G_PickupWeapon, G_UseWeapon,
								G_DropWeapon, G_FireBfg,
								"weapons/common/pickup.wav",
								"models/weapons/bfg/tris.md3", EF_ROTATE
										| EF_BOB | EF_PULSE, "w_bfg", "BFG10K",
								1, "Nukes", ITEM_WEAPON, 0,
								"weapons/bfg/fire.wav weapons/bfg/hit.wav" },

						//
						// AMMO ITEMS
						//

						/*QUAKED ammo_shells(.3 .3 1)(-16 -16 -16)(16 16 16)
						 */
						{ "ammo_shells", G_PickupAmmo, NULL, G_DropAmmo, NULL,
								"ammo/common/pickup.wav",
								"models/ammo/shells/tris.md3", EF_ROTATE
										| EF_BOB | EF_PULSE, "a_shells",
								"Shells", 10, NULL, ITEM_AMMO, AMMO_SHELLS, "" },

						/*QUAKED ammo_bullets(.3 .3 1)(-16 -16 -16)(16 16 16)
						 */
						{ "ammo_bullets", G_PickupAmmo, NULL, G_DropAmmo, NULL,
								"ammo/common/pickup.wav",
								"models/ammo/bullets/tris.md3", EF_ROTATE
										| EF_BOB | EF_PULSE, "a_bullets",
								"Bullets", 50, NULL, ITEM_AMMO, AMMO_BULLETS,
								"" },

						/*QUAKED ammo_grenades(.3 .3 1)(-16 -16 -16)(16 16 16)
						 */
						{ "ammo_grenades", G_PickupAmmo, NULL, G_DropAmmo,
								NULL, "ammo/common/pickup.wav",
								"models/ammo/grenades/tris.md3", EF_ROTATE
										| EF_BOB | EF_PULSE, "a_grenades",
								"Grenades", 10, "grenades", ITEM_AMMO,
								AMMO_GRENADES, "" },

						/*QUAKED ammo_rockets(.3 .3 1)(-16 -16 -16)(16 16 16)
						 */
						{ "ammo_rockets", G_PickupAmmo, NULL, G_DropAmmo, NULL,
								"ammo/common/pickup.wav",
								"models/ammo/rockets/tris.md3", EF_ROTATE
										| EF_BOB | EF_PULSE, "a_rockets",
								"Rockets", 10, NULL, ITEM_AMMO, AMMO_ROCKETS,
								"" },

						/*QUAKED ammo_cells(.3 .3 1)(-16 -16 -16)(16 16 16)
						 */
						{ "ammo_cells", G_PickupAmmo, NULL, G_DropAmmo, NULL,
								"ammo/common/pickup.wav",
								"models/ammo/cells/tris.md3", EF_ROTATE
										| EF_BOB | EF_PULSE, "a_cells",
								"Cells", 50, NULL, ITEM_AMMO, AMMO_CELLS, "" },

						/*QUAKED ammo_bolts(.3 .3 1)(-16 -16 -16)(16 16 16)
						 */
						{ "ammo_bolts", G_PickupAmmo, NULL, G_DropAmmo, NULL,
								"ammo/common/pickup.wav",
								"models/ammo/bolts/tris.md3", EF_ROTATE
										| EF_BOB | EF_PULSE, "a_bolts",
								"Bolts", 25, NULL, ITEM_AMMO, AMMO_BOLTS, "" },

						/*QUAKED ammo_slugs(.3 .3 1)(-16 -16 -16)(16 16 16)
						 */
						{ "ammo_slugs", G_PickupAmmo, NULL, G_DropAmmo, NULL,
								"ammo/common/pickup.wav",
								"models/ammo/slugs/tris.md3", EF_ROTATE
										| EF_BOB | EF_PULSE, "a_slugs",
								"Slugs", 10, NULL, ITEM_AMMO, AMMO_SLUGS, "" },

						/*QUAKED ammo_nukes(.3 .3 1)(-16 -16 -16)(16 16 16)
						 */
						{ "ammo_nukes", G_PickupAmmo, NULL, G_DropAmmo, NULL,
								"ammo/common/pickup.wav",
								"models/ammo/nukes/tris.md3", EF_ROTATE
										| EF_BOB | EF_PULSE, "a_nukes",
								"Nukes", 2, NULL, ITEM_AMMO, AMMO_NUKES, "" },

						/*QUAKED item_adrenaline(.3 .3 1)(-16 -16 -16)(16 16 16)
						 gives +1 to maximum health
						 */
						{ "item_adrenaline", G_PickupAdrenaline, NULL, NULL,
								NULL, "adren/pickup.wav",
								"models/powerups/adren/tris.obj", EF_ROTATE
										| EF_PULSE, "p_adrenaline",
								"Adrenaline", 0, NULL, 0, 0, "" },

						/*QUAKED item_health_small (.3 .3 1) (-16 -16 -16) (16 16 16)
						 */
						{ "item_health_small", G_PickupHealth, NULL, NULL,
								NULL, "health/small/pickup.wav",
								"models/health/small/tris.obj", EF_ROTATE
										| EF_BOB | EF_PULSE, "i_health",
								"Small Health", 3, NULL, ITEM_HEALTH,
								HEALTH_SMALL, "" },

						/*QUAKED item_health (.3 .3 1) (-16 -16 -16) (16 16 16)
						 */
						{ "item_health", G_PickupHealth, NULL, NULL, NULL,
								"health/medium/pickup.wav",
								"models/health/medium/tris.obj", EF_ROTATE
										| EF_BOB | EF_PULSE, "i_health",
								"Medium Health", 15, NULL, ITEM_HEALTH,
								HEALTH_MEDIUM, "" },

						/*QUAKED item_health_large (.3 .3 1) (-16 -16 -16) (16 16 16)
						 */
						{ "item_health_large", G_PickupHealth, NULL, NULL,
								NULL, "health/large/pickup.wav",
								"models/health/large/tris.obj", EF_ROTATE
										| EF_BOB | EF_PULSE, "i_health",
								"Large Health", 25, NULL, ITEM_HEALTH,
								HEALTH_LARGE, "" },

						/*QUAKED item_health_mega (.3 .3 1) (-16 -16 -16) (16 16 16)
						 */
						{ "item_health_mega", G_PickupHealth, NULL, NULL, NULL,
								"health/mega/pickup.wav",
								"models/health/mega/tris.obj", EF_ROTATE
										| EF_BOB | EF_PULSE, "i_health",
								"Mega Health", 75, NULL, ITEM_HEALTH,
								HEALTH_MEGA, "" },

						/*QUAKED item_flag_team1(1 0.2 0)(-16 -16 -24)(16 16 32)
						 */
						{ "item_flag_team1", G_PickupFlag, NULL, G_DropFlag,
								NULL, NULL, "models/ctf/flag1/tris.md3", EF_BOB
										| EF_ROTATE, "i_flag1", "Flag", 0,
								NULL, ITEM_FLAG, 0,
								"ctf/capture.wav ctf/steal.wav ctf/return.wav" },

						/*QUAKED item_flag_team2(1 0.2 0)(-16 -16 -24)(16 16 32)
						 */
						{ "item_flag_team2", G_PickupFlag, NULL, G_DropFlag,
								NULL, NULL, "models/ctf/flag2/tris.md3", EF_BOB
										| EF_ROTATE, "i_flag2", "Flag", 0,
								NULL, ITEM_FLAG, 0,
								"ctf/capture.wav ctf/steal.wav ctf/return.wav" },

						/*QUAKED item_quad(.3 .3 1)(-16 -16 -16)(16 16 16)
						 */
						{ "item_quad", G_PickupQuadDamage, NULL, NULL, NULL,
								"quad/pickup.wav",
								"models/powerups/quad/tris.md3", EF_BOB
										| EF_ROTATE, "i_quad", "Quad Damage",
								60, NULL, ITEM_POWERUP, 0,
								"quad/attack.wav quad/expire.wav" },

						// end of list marker
						{ NULL } };

// override quake2 items for legacy maps
g_override_t g_overrides[] = { { "weapon_chaingun", "weapon_machinegun" }, {
		"item_invulnerability", "item_quad" }, { "item_power_shield",
		"item_armor_combat" }, { "item_power_screen", "item_armor_jacket" }, {
		NULL } };

/*
 * G_InitItems
 */
void G_InitItems(void) {
	g_game.num_items = sizeof(g_items) / sizeof(g_items[0]) - 1;
	g_game.num_overrides = sizeof(g_overrides) / sizeof(g_overrides[0]) - 1;
}

/*
 * G_SetItemNames
 *
 * Called by worldspawn
 */
void G_SetItemNames(void) {
	int i, j;
	g_item_t *it;
	g_override_t *ov;

	for (i = 0; i < g_game.num_items; i++) {
		it = &g_items[i];
		if (it->class_name) {
			for (j = 0; j < g_game.num_overrides; j++) {
				ov = &g_overrides[j];
				if (!strcmp(it->class_name, ov->old)) {
					it = G_FindItemByClassname(ov->new);
					break;
				}
			}
		}
		gi.ConfigString(CS_ITEMS + i, it->pickup_name);
	}

	grenade_index = gi.ModelIndex("models/objects/grenade/tris.md3");
	grenade_hit_index = gi.SoundIndex("objects/grenade/hit");

	rocket_index = gi.ModelIndex("models/objects/rocket/tris.md3");
	rocket_fly_index = gi.SoundIndex("objects/rocket/fly");

	lightning_fly_index = gi.SoundIndex("weapons/lightning/fly");

	quad_damage_index = ITEM_INDEX(G_FindItemByClassname("item_quad"));
}
