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

static void G_ItemDropToFloor(g_edict_t *ent);

/*
 * @brief
 */
const g_item_t *G_ItemByIndex(uint16_t index) {

	if (index == 0 || index >= g_num_items)
		return NULL;

	return &g_items[index];
}

/*
 * @brief
 */
const g_item_t *G_FindItemByClassName(const char *class_name) {
	int32_t i;

	const g_item_t *it = g_items;
	for (i = 0; i < g_num_items; i++, it++) {

		if (!it->class_name)
			continue;

		if (!g_strcmp0(it->class_name, class_name))
			return it;
	}

	return NULL;
}

/*
 * @brief
 */
const g_item_t *G_FindItem(const char *name) {
	int32_t i;

	if (!name)
		return NULL;

	const g_item_t *it = g_items;
	for (i = 0; i < g_num_items; i++, it++) {

		if (!it->name)
			continue;

		if (!strcasecmp(it->name, name))
			return it;
	}

	return NULL;
}

/*
 * @brief
 */
static void G_ItemRespawn(g_edict_t *ent) {
	g_edict_t *master;
	int32_t count, choice;
	vec3_t origin;

	if (ent->locals.team) { // pick a random member from the team

		master = ent->locals.team_master;
		VectorCopy(master->locals.map_origin, origin);

		for (count = 0, ent = master; ent; ent = ent->locals.chain, count++)
			;

		choice = Random() % count;

		for (count = 0, ent = master; count < choice; ent = ent->locals.chain, count++)
			;
	} else {
		VectorCopy(ent->locals.map_origin, origin);
	}

	VectorCopy(origin, ent->s.origin);
	G_ItemDropToFloor(ent);

	ent->sv_flags &= ~SVF_NO_CLIENT;
	ent->solid = SOLID_TRIGGER;
	gi.LinkEdict(ent);

	// send an effect
	ent->s.event = EV_ITEM_RESPAWN;
}

/*
 * @brief
 */
void G_SetItemRespawn(g_edict_t *ent, uint32_t delay) {

	ent->locals.flags |= FL_RESPAWN;
	ent->sv_flags |= SVF_NO_CLIENT;
	ent->solid = SOLID_NOT;
	ent->locals.next_think = g_level.time + delay;
	ent->locals.Think = G_ItemRespawn;

	gi.LinkEdict(ent);
}

/*
 * @brief
 */
static _Bool G_PickupAdrenaline(g_edict_t *ent, g_edict_t *other) {

	if (other->locals.health < other->locals.max_health)
		other->locals.health = other->locals.max_health;

	if (!(ent->locals.spawn_flags & SF_ITEM_DROPPED))
		G_SetItemRespawn(ent, 60000);

	return true;
}

/*
 * @brief
 */
static _Bool G_PickupQuadDamage(g_edict_t *ent, g_edict_t *other) {

	if (other->client->locals.persistent.inventory[g_level.media.quad_damage])
		return false; // already have it

	other->client->locals.persistent.inventory[g_level.media.quad_damage] = 1;

	if (ent->locals.spawn_flags & SF_ITEM_DROPPED) { // receive only the time left
		other->client->locals.quad_damage_time = ent->locals.next_think;
	} else {
		other->client->locals.quad_damage_time = g_level.time + 20000;
		G_SetItemRespawn(ent, 90000);
	}

	other->s.effects |= EF_QUAD;
	return true;
}

/*
 * @brief
 */
void G_TossQuadDamage(g_edict_t *ent) {
	g_edict_t *quad;

	if (!ent->client->locals.persistent.inventory[g_level.media.quad_damage])
		return;

	quad = G_DropItem(ent, G_FindItemByClassName("item_quad"));

	if (quad)
		quad->locals.timestamp = ent->client->locals.quad_damage_time;

	ent->client->locals.quad_damage_time = 0.0;
	ent->client->locals.persistent.inventory[g_level.media.quad_damage] = 0;
}

/*
 * @brief
 */
_Bool G_AddAmmo(g_edict_t *ent, const g_item_t *item, int16_t count) {
	uint16_t index;
	int16_t max;

	if (item->tag == AMMO_SHELLS)
		max = ent->client->locals.persistent.max_shells;
	else if (item->tag == AMMO_BULLETS)
		max = ent->client->locals.persistent.max_bullets;
	else if (item->tag == AMMO_GRENADES)
		max = ent->client->locals.persistent.max_grenades;
	else if (item->tag == AMMO_ROCKETS)
		max = ent->client->locals.persistent.max_rockets;
	else if (item->tag == AMMO_CELLS)
		max = ent->client->locals.persistent.max_cells;
	else if (item->tag == AMMO_BOLTS)
		max = ent->client->locals.persistent.max_bolts;
	else if (item->tag == AMMO_SLUGS)
		max = ent->client->locals.persistent.max_slugs;
	else if (item->tag == AMMO_NUKES)
		max = ent->client->locals.persistent.max_nukes;
	else
		return false;

	index = ITEM_INDEX(item);

	ent->client->locals.persistent.inventory[index] += count;

	if (ent->client->locals.persistent.inventory[index] > max)
		ent->client->locals.persistent.inventory[index] = max;
	else if (ent->client->locals.persistent.inventory[index] < 0)
		ent->client->locals.persistent.inventory[index] = 0;

	return true;
}

/*
 * @brief
 */
_Bool G_SetAmmo(g_edict_t *ent, const g_item_t *item, int16_t count) {
	uint16_t index;
	int16_t max;

	if (item->tag == AMMO_SHELLS)
		max = ent->client->locals.persistent.max_shells;
	else if (item->tag == AMMO_BULLETS)
		max = ent->client->locals.persistent.max_bullets;
	else if (item->tag == AMMO_GRENADES)
		max = ent->client->locals.persistent.max_grenades;
	else if (item->tag == AMMO_ROCKETS)
		max = ent->client->locals.persistent.max_rockets;
	else if (item->tag == AMMO_CELLS)
		max = ent->client->locals.persistent.max_cells;
	else if (item->tag == AMMO_BOLTS)
		max = ent->client->locals.persistent.max_bolts;
	else if (item->tag == AMMO_SLUGS)
		max = ent->client->locals.persistent.max_slugs;
	else if (item->tag == AMMO_NUKES)
		max = ent->client->locals.persistent.max_nukes;
	else
		return false;

	index = ITEM_INDEX(item);

	ent->client->locals.persistent.inventory[index] = count;

	if (ent->client->locals.persistent.inventory[index] > max)
		ent->client->locals.persistent.inventory[index] = max;
	else if (ent->client->locals.persistent.inventory[index] < 0)
		ent->client->locals.persistent.inventory[index] = 0;

	return true;
}

/*
 * @brief
 */
static _Bool G_PickupAmmo(g_edict_t *ent, g_edict_t *other) {
	int32_t count;

	if (ent->locals.count)
		count = ent->locals.count;
	else
		count = ent->locals.item->quantity;

	if (!G_AddAmmo(other, ent->locals.item, count))
		return false;

	if (!(ent->locals.spawn_flags & SF_ITEM_DROPPED))
		G_SetItemRespawn(ent, g_ammo_respawn_time->value * 1000);

	return true;
}

/*
 * @brief
 */
static _Bool G_PickupHealth(g_edict_t *ent, g_edict_t *other) {
	int32_t h, max;

	const _Bool always_add = ent->locals.item->tag == HEALTH_SMALL;
	const _Bool always_pickup = ent->locals.item->tag == HEALTH_SMALL || ent->locals.item->tag
			== HEALTH_MEGA;

	if (other->locals.health < other->locals.max_health || always_add || always_pickup) {

		h = other->locals.health + ent->locals.item->quantity; // target health points
		max = other->locals.max_health;

		if (always_pickup) { // resolve max
			if (other->locals.health > 200)
				max = other->locals.health;
			else
				max = 200;
		} else if (always_add)
			max = 9999;

		if (h > max) // and enforce it
			h = max;

		other->locals.health = other->client->locals.persistent.health = h;

		if (ent->locals.count >= 75) // respawn the item
			G_SetItemRespawn(ent, 90000);
		else if (ent->locals.count >= 50)
			G_SetItemRespawn(ent, 60000);
		else
			G_SetItemRespawn(ent, 20000);

		return true;
	}

	return false;
}

/*
 * @brief
 */
static _Bool G_PickupArmor(g_edict_t *ent, g_edict_t *other) {
	_Bool taken = true;

	if (ent->locals.item->tag == ARMOR_SHARD) { // take it, ignoring cap
		other->client->locals.persistent.armor += ent->locals.item->quantity;
	} else if (other->client->locals.persistent.armor < other->client->locals.persistent.max_armor) {

		// take it, but enforce cap
		other->client->locals.persistent.armor += ent->locals.item->quantity;

		if (other->client->locals.persistent.armor > other->client->locals.persistent.max_armor)
			other->client->locals.persistent.armor = other->client->locals.persistent.max_armor;
	} else { // don't take it
		taken = false;
	}

	if (taken && !(ent->locals.spawn_flags & SF_ITEM_DROPPED))
		G_SetItemRespawn(ent, 20000);

	return taken;
}

/*
 * @brief A dropped flag has been idle for 30 seconds, return it.
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
 * @brief Return own flag, or capture on it if enemy's flag is in inventory.
 * Take the enemy's flag.
 */
static _Bool G_PickupFlag(g_edict_t *ent, g_edict_t *other) {
	g_team_t *t, *ot;
	g_edict_t *f, *of;
	int32_t index;

	if (!other->client->locals.persistent.team)
		return false;

	if (!(t = G_TeamForFlag(ent)))
		return false;

	if (!(f = G_FlagForTeam(t)))
		return false;

	if (!(ot = G_OtherTeam(t)))
		return false;

	if (!(of = G_FlagForTeam(ot)))
		return false;

	if (t == other->client->locals.persistent.team) { // our flag

		if (ent->locals.spawn_flags & SF_ITEM_DROPPED) { // return it if necessary

			f->sv_flags &= ~SVF_NO_CLIENT; // and toggle the static one
			f->s.event = EV_ITEM_RESPAWN;

			gi.Sound(other, gi.SoundIndex("ctf/return"), ATTN_NONE);

			gi.BroadcastPrint(PRINT_HIGH, "%s returned the %s flag\n",
					other->client->locals.persistent.net_name, t->name);

			return true;
		}

		index = ITEM_INDEX(of->locals.item);
		if (other->client->locals.persistent.inventory[index]) { // capture

			other->client->locals.persistent.inventory[index] = 0;
			other->s.effects &= ~G_EffectForTeam(ot);
			other->s.model3 = 0;

			of->sv_flags &= ~SVF_NO_CLIENT; // reset the other flag
			of->s.event = EV_ITEM_RESPAWN;

			gi.Sound(other, gi.SoundIndex("ctf/capture"), ATTN_NONE);

			gi.BroadcastPrint(PRINT_HIGH, "%s captured the %s flag\n",
					other->client->locals.persistent.net_name, ot->name);

			t->captures++;
			other->client->locals.persistent.captures++;

			return false;
		}

		// touching our own flag for no particular reason
		return false;
	}

	// enemy's flag
	if (ent->sv_flags & SVF_NO_CLIENT) // already taken
		return false;

	// take it
	index = ITEM_INDEX(f->locals.item);
	other->client->locals.persistent.inventory[index] = 1;

	// link the flag model to the player
	other->s.model3 = gi.ModelIndex(f->locals.item->model);

	gi.Sound(other, gi.SoundIndex("ctf/steal"), ATTN_NONE);

	gi.BroadcastPrint(PRINT_HIGH, "%s stole the %s flag\n",
			other->client->locals.persistent.net_name, t->name);

	other->s.effects |= G_EffectForTeam(t);
	return true;
}

/*
 * @brief
 */
void G_TossFlag(g_edict_t *ent) {
	g_team_t *ot;
	g_edict_t *of;
	int32_t index;

	if (!(ot = G_OtherTeam(ent->client->locals.persistent.team)))
		return;

	if (!(of = G_FlagForTeam(ot)))
		return;

	index = ITEM_INDEX(of->locals.item);

	if (!ent->client->locals.persistent.inventory[index])
		return;

	ent->client->locals.persistent.inventory[index] = 0;

	ent->s.model3 = 0;
	ent->s.effects &= ~(EF_CTF_RED | EF_CTF_BLUE);

	gi.BroadcastPrint(PRINT_HIGH, "%s dropped the %s flag\n",
			ent->client->locals.persistent.net_name, ot->name);

	G_DropItem(ent, of->locals.item);
}

/*
 * @brief
 */
static void G_DropFlag(g_edict_t *ent, const g_item_t *item __attribute__((unused))) {
	G_TossFlag(ent);
}

/*
 * @brief
 */
void G_TouchItem(g_edict_t *ent, g_edict_t *other, c_bsp_plane_t *plane __attribute__((unused)), c_bsp_surface_t *surf __attribute__((unused))) {
	_Bool taken;

	if (!other->client)
		return;

	if (other->locals.health < 1)
		return; // dead people can't pickup

	if (!ent->locals.item->Pickup)
		return; // item can't be picked up

	if (g_level.warmup)
		return; // warmup mode

	if ((taken = ent->locals.item->Pickup(ent, other))) {
		// show icon and name on status bar
		uint16_t icon = gi.ImageIndex(ent->locals.item->icon);

		if (other->client->ps.stats[STAT_PICKUP_ICON] == icon)
			icon |= STAT_TOGGLE_BIT;

		other->client->ps.stats[STAT_PICKUP_ICON] = icon;
		other->client->ps.stats[STAT_PICKUP_STRING] = CS_ITEMS + ITEM_INDEX(ent->locals.item);

		other->client->locals.pickup_msg_time = g_level.time + 3000;

		if (ent->locals.item->pickup_sound) { // play pickup sound
			gi.Sound(other, gi.SoundIndex(ent->locals.item->pickup_sound), ATTN_NORM);
		}

		other->s.event = EV_ITEM_PICKUP;
	}

	if (!(ent->locals.spawn_flags & SF_ITEM_TARGETS_USED)) {
		G_UseTargets(ent, other);
		ent->locals.spawn_flags |= SF_ITEM_TARGETS_USED;
	}

	if (!taken)
		return;

	if (ent->locals.spawn_flags & SF_ITEM_DROPPED) {
		if (ent->locals.flags & FL_RESPAWN)
			ent->locals.flags &= ~FL_RESPAWN;
		else
			G_FreeEdict(ent);
	} else if (ent->locals.item->type == ITEM_FLAG) // if a flag has been taken, hide it
		ent->sv_flags |= SVF_NO_CLIENT;
}

/*
 * @brief
 */
static void G_DropItemUntouchable(g_edict_t *ent, g_edict_t *other, c_bsp_plane_t *plane,
		c_bsp_surface_t *surf) {

	if (other == ent->owner) // prevent the dropper from picking it right back up
		return;

	G_TouchItem(ent, other, plane, surf);
}

/*
 * @brief
 */
static void G_DropItem_Think(g_edict_t *ent) {
	int32_t contents;
	uint32_t i;

	if (!ent->locals.ground_entity) { // keep falling in valid space
		ent->locals.next_think = g_level.time + gi.frame_millis;
		return;
	}

	// see if we've landed on a trigger_hurt
	G_TouchTriggers(ent);

	if (!ent->in_use) // we have
		return;

	// restore full entity effects (e.g. EF_BOB)
	ent->s.effects = ent->locals.item->effects;

	// allow anyone to pick it up
	ent->locals.Touch = G_TouchItem;

	// setup the next think function and time

	if (ent->locals.item->type == ITEM_FLAG) // flags go back to base
		ent->locals.Think = G_ResetFlag;
	else
		// everything else just gets freed
		ent->locals.Think = G_FreeEdict;

	if (ent->locals.item->type == ITEM_POWERUP) // expire from last touch
		i = ent->locals.timestamp - g_level.time;
	else
		// general case
		i = 30000;

	contents = gi.PointContents(ent->s.origin);

	if (contents & CONTENTS_LAVA) // expire more quickly in lava
		i *= 300;
	if (contents & CONTENTS_SLIME) // and slime
		i *= 500;

	ent->locals.next_think = g_level.time + i;
}

/*
 * @brief Handles the mechanics of dropping items, but does not adjust the client's
 * inventory. That is left to the caller.
 */
g_edict_t *G_DropItem(g_edict_t *ent, const g_item_t *item) {
	g_edict_t *dropped;
	vec3_t forward;
	vec3_t v;
	c_trace_t trace;

	dropped = G_Spawn();

	dropped->class_name = item->class_name;
	dropped->locals.item = item;
	dropped->locals.spawn_flags = SF_ITEM_DROPPED;
	dropped->s.effects = (item->effects & ~EF_BOB);
	VectorSet(dropped->mins, -15, -15, -15);
	VectorSet(dropped->maxs, 15, 15, 15);
	gi.SetModel(dropped, dropped->locals.item->model);
	dropped->solid = SOLID_TRIGGER;
	dropped->locals.move_type = MOVE_TYPE_TOSS;
	dropped->locals.Touch = G_DropItemUntouchable;
	dropped->owner = ent;

	if (ent->client && ent->locals.health <= 0) { // randomize the direction we toss in
		VectorSet(v, 0.0, ent->client->locals.angles[1] + Randomc() * 45.0, 0.0);
		AngleVectors(v, forward, NULL, NULL);

		VectorMA(ent->s.origin, 24.0, forward, dropped->s.origin);

		trace = gi.Trace(ent->s.origin, dropped->mins, dropped->maxs, dropped->s.origin, ent,
				CONTENTS_SOLID);

		VectorCopy(trace.end, dropped->s.origin);
	} else {
		AngleVectors(ent->s.angles, forward, NULL, NULL);

		trace = gi.Trace(ent->s.origin, dropped->mins, dropped->maxs, ent->s.origin, ent,
				CONTENTS_SOLID);

		VectorCopy(ent->s.origin, dropped->s.origin);
	}

	if (item->type == ITEM_WEAPON) {
		const g_item_t *ammo = G_FindItem(item->ammo);
		if (ammo)
			dropped->locals.health = ammo->quantity;
		else
			dropped->locals.health = 0;
	}

	// we're in a bad spot, forget it
	if (trace.start_solid) {

		if (item->type == ITEM_FLAG)
			G_ResetFlag(dropped);
		else
			G_FreeEdict(dropped);

		return NULL;
	}

	VectorScale(forward, 100.0, dropped->locals.velocity);
	dropped->locals.velocity[2] = 200.0 + (Randomf() * 150.0);

	dropped->locals.Think = G_DropItem_Think;
	dropped->locals.next_think = g_level.time + gi.frame_millis;

	gi.LinkEdict(dropped);

	return dropped;
}

/*
 * @brief
 */
static void G_UseItem(g_edict_t *ent, g_edict_t *other __attribute__((unused)), g_edict_t *activator __attribute__((unused))) {
	ent->sv_flags &= ~SVF_NO_CLIENT;
	ent->locals.Use = NULL;

	if (ent->locals.spawn_flags & SF_ITEM_NO_TOUCH) {
		ent->solid = SOLID_BOX;
		ent->locals.Touch = NULL;
	} else {
		ent->solid = SOLID_TRIGGER;
		ent->locals.Touch = G_TouchItem;
	}

	gi.LinkEdict(ent);
}

/*
 * @brief
 */
static void G_ItemDropToFloor(g_edict_t *ent) {
	c_trace_t tr;
	vec3_t dest;

	VectorClear(ent->locals.velocity);

	if (ent->locals.spawn_flags & SF_ITEM_HOVER) { // some items should not fall
		ent->locals.move_type = MOVE_TYPE_FLY;
		ent->locals.ground_entity = NULL;
	} else { // while most do, and will also be pushed by their ground
		VectorCopy(ent->s.origin, dest);
		dest[2] -= 8192;

		tr = gi.Trace(ent->s.origin, ent->mins, ent->maxs, dest, ent, MASK_SOLID);
		if (tr.start_solid) {
			gi.Debug("%s start_solid at %s\n", ent->class_name, vtos(ent->s.origin));
			G_FreeEdict(ent);
			return;
		}

		VectorCopy(tr.end, ent->s.origin);
		ent->locals.ground_entity = tr.ent;
	}

	gi.LinkEdict(ent);
}

/*
 * @brief Precaches all data needed for a given item.
 * This will be called for each item spawned in a level,
 * and for each item in each client's inventory.
 */
void G_PrecacheItem(const g_item_t *it) {
	const char *s, *start;
	char data[MAX_QPATH];
	int32_t len;

	if (!it)
		return;

	gi.ConfigString(CS_ITEMS + ITEM_INDEX(it), it->name);

	if (it->pickup_sound)
		gi.SoundIndex(it->pickup_sound);
	if (it->model)
		gi.ModelIndex(it->model);
	if (it->icon)
		gi.ImageIndex(it->icon);

	// parse everything for its ammo
	if (it->ammo && it->ammo[0]) {
		const g_item_t *ammo = G_FindItem(it->ammo);
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
			gi.Error("%s has bad precache string\n", it->class_name);
		memcpy(data, start, len);
		data[len] = '\0';
		if (*s)
			s++;

		// determine type based on extension
		if (g_str_has_suffix(data, ".md3") || g_str_has_suffix(data, ".obj"))
			gi.ModelIndex(data);
		else if (g_str_has_suffix(data, ".wav") || g_str_has_suffix(data, ".ogg"))
			gi.SoundIndex(data);
		else if (g_str_has_suffix(data, ".tga") || g_str_has_suffix(data, ".png")
				|| g_str_has_suffix(data, ".jpg") || g_str_has_suffix(data, ".pcx"))
			gi.ImageIndex(data);
		else
			gi.Error("%s has unknown data type\n", it->class_name);
	}
}

/*
 * @brief Sets the clipping size and plants the object on the floor.
 *
 * Items can't be immediately dropped to floor, because they might
 * be on an entity that hasn't spawned yet.
 */
void G_SpawnItem(g_edict_t *ent, const g_item_t *item) {

	G_PrecacheItem(item);

	if (ent->locals.spawn_flags) {
		gi.Debug("%s at %s has spawnflags %d\n", ent->class_name, vtos(ent->s.origin),
				ent->locals.spawn_flags);
	}

	VectorSet(ent->mins, -15, -15, -15);
	VectorSet(ent->maxs, 15, 15, 15);

	ent->solid = SOLID_TRIGGER;
	ent->locals.move_type = MOVE_TYPE_TOSS;
	ent->locals.Touch = G_TouchItem;
	ent->locals.Think = G_ItemDropToFloor;
	ent->locals.next_think = g_level.time + 2000 * gi.frame_millis; // items start after other solids

	ent->locals.item = item;
	ent->s.effects = item->effects;

	if (ent->model)
		gi.SetModel(ent, ent->model);
	else
		gi.SetModel(ent, ent->locals.item->model);

	if (ent->locals.team) {
		ent->locals.flags &= ~FL_TEAM_SLAVE;
		ent->locals.chain = ent->locals.team_chain;
		ent->locals.team_chain = NULL;

		ent->sv_flags |= SVF_NO_CLIENT;
		ent->solid = SOLID_NOT;
		if (ent == ent->locals.team_master) {
			ent->locals.next_think = g_level.time + gi.frame_millis;
			ent->locals.Think = G_ItemRespawn;
		}
	}

	if (ent->locals.item->type == ITEM_WEAPON) {
		const g_item_t *ammo = G_FindItem(ent->locals.item->ammo);
		if (ammo)
			ent->locals.health = ammo->quantity;
		else
			ent->locals.health = 0;
	}

	// hide flags unless ctf is enabled
	if (!g_level.ctf && g_str_has_prefix(ent->class_name, "item_flag_team")) {
		ent->sv_flags |= SVF_NO_CLIENT;
		ent->solid = SOLID_NOT;
	}

	if (ent->locals.spawn_flags & SF_ITEM_NO_TOUCH) {
		ent->solid = SOLID_BOX;
		ent->locals.Touch = NULL;
	}

	if (ent->locals.spawn_flags & SF_ITEM_TRIGGER) {
		ent->sv_flags |= SVF_NO_CLIENT;
		ent->solid = SOLID_NOT;
		ent->locals.Use = G_UseItem;
	}
}

const g_item_t g_items[] = {
/*QUAKED item_armor_body(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"item_armor_body",
		G_PickupArmor,
		NULL,
		NULL,
		NULL,
		"armor/body/pickup.wav",
		"models/armor/body/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/i_bodyarmor",
		"Body Armor",
		200,
		NULL,
		ITEM_ARMOR,
		ARMOR_BODY,
		0.80,
		"" },

/*QUAKED item_armor_combat(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"item_armor_combat",
		G_PickupArmor,
		NULL,
		NULL,
		NULL,
		"armor/combat/pickup.wav",
		"models/armor/combat/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/i_combatarmor",
		"Combat Armor",
		100,
		NULL,
		ITEM_ARMOR,
		ARMOR_COMBAT,
		0.66,
		"" },

/*QUAKED item_armor_jacket(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"item_armor_jacket",
		G_PickupArmor,
		NULL,
		NULL,
		NULL,
		"armor/jacket/pickup.wav",
		"models/armor/jacket/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/i_jacketarmor",
		"Jacket Armor",
		50,
		NULL,
		ITEM_ARMOR,
		ARMOR_JACKET,
		0.50,
		"" },

/*QUAKED item_armor_shard(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"item_armor_shard",
		G_PickupArmor,
		NULL,
		NULL,
		NULL,
		"armor/shard/pickup.wav",
		"models/armor/shard/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/i_shard",
		"Armor Shard",
		3,
		NULL,
		ITEM_ARMOR,
		ARMOR_SHARD,
		0.10,
		"" },

/*QUAKED weapon_blaster(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"weapon_blaster",
		G_PickupWeapon,
		G_UseWeapon,
		NULL,
		G_FireBlaster,
		"weapons/common/pickup.wav",
		"models/weapons/blaster/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/w_blaster",
		"Blaster",
		0,
		NULL,
		ITEM_WEAPON,
		0,
		0.10,
		"weapons/blaster/fire.wav" },

/*QUAKED weapon_shotgun(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"weapon_shotgun",
		G_PickupWeapon,
		G_UseWeapon,
		G_DropWeapon,
		G_FireShotgun,
		"weapons/common/pickup.wav",
		"models/weapons/shotgun/tris.obj",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/w_shotgun",
		"Shotgun",
		1,
		"Shells",
		ITEM_WEAPON,
		0,
		0.15,
		"weapons/shotgun/fire.wav" },

/*QUAKED weapon_supershotgun(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"weapon_supershotgun",
		G_PickupWeapon,
		G_UseWeapon,
		G_DropWeapon,
		G_FireSuperShotgun,
		"weapons/common/pickup.wav",
		"models/weapons/supershotgun/tris.obj",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/w_sshotgun",
		"Super Shotgun",
		2,
		"Shells",
		ITEM_WEAPON,
		0,
		0.25,
		"weapons/supershotgun/fire.wav" },

/*QUAKED weapon_machinegun(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"weapon_machinegun",
		G_PickupWeapon,
		G_UseWeapon,
		G_DropWeapon,
		G_FireMachinegun,
		"weapons/common/pickup.wav",
		"models/weapons/machinegun/tris.obj",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/w_machinegun",
		"Machinegun",
		1,
		"Bullets",
		ITEM_WEAPON,
		0,
		0.30,
		"weapons/machinegun/fire_1.wav weapons/machinegun/fire_2.wav "
			"weapons/machinegun/fire_3.wav weapons/machinegun/fire_4.wav" },

/*QUAKED weapon_grenadelauncher(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"weapon_grenadelauncher",
		G_PickupWeapon,
		G_UseWeapon,
		G_DropWeapon,
		G_FireGrenadeLauncher,
		"weapons/common/pickup.wav",
		"models/weapons/grenadelauncher/tris.obj",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/w_glauncher",
		"Grenade Launcher",
		1,
		"Grenades",
		ITEM_WEAPON,
		0,
		0.40,
		"models/objects/grenade/tris.md3 weapons/grenadelauncher/fire.wav" },

/*QUAKED weapon_rocketlauncher(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"weapon_rocketlauncher",
		G_PickupWeapon,
		G_UseWeapon,
		G_DropWeapon,
		G_FireRocketLauncher,
		"weapons/common/pickup.wav",
		"models/weapons/rocketlauncher/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/w_rlauncher",
		"Rocket Launcher",
		1,
		"Rockets",
		ITEM_WEAPON,
		0,
		0.50,
		"models/objects/rocket/tris.md3 objects/rocket/fly.wav "
			"weapons/rocketlauncher/fire.wav" },

/*QUAKED weapon_hyperblaster(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"weapon_hyperblaster",
		G_PickupWeapon,
		G_UseWeapon,
		G_DropWeapon,
		G_FireHyperblaster,
		"weapons/common/pickup.wav",
		"models/weapons/hyperblaster/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/w_hyperblaster",
		"Hyperblaster",
		1,
		"Cells",
		ITEM_WEAPON,
		0,
		0.50,
		"weapons/hyperblaster/fire.wav weapons/hyperblaster/hit.wav" },

/*QUAKED weapon_lightning(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"weapon_lightning",
		G_PickupWeapon,
		G_UseWeapon,
		G_DropWeapon,
		G_FireLightning,
		"weapons/common/pickup.wav",
		"models/weapons/lightning/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/w_lightning",
		"Lightning",
		1,
		"Bolts",
		ITEM_WEAPON,
		0,
		0.50,
		"weapons/lightning/fire.wav weapons/lightning/fly.wav "
			"weapons/lightning/discharge.wav" },

/*QUAKED weapon_railgun(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"weapon_railgun",
		G_PickupWeapon,
		G_UseWeapon,
		G_DropWeapon,
		G_FireRailgun,
		"weapons/common/pickup.wav",
		"models/weapons/railgun/tris.obj",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/w_railgun",
		"Railgun",
		1,
		"Slugs",
		ITEM_WEAPON,
		0,
		0.60,
		"weapons/railgun/fire.wav" },

/*QUAKED weapon_bfg(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"weapon_bfg",
		G_PickupWeapon,
		G_UseWeapon,
		G_DropWeapon,
		G_FireBfg,
		"weapons/common/pickup.wav",
		"models/weapons/bfg/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/w_bfg",
		"BFG10K",
		1,
		"Nukes",
		ITEM_WEAPON,
		0,
		0.66,
		"weapons/bfg/fire.wav weapons/bfg/hit.wav" },

/*QUAKED ammo_shells(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"ammo_shells",
		G_PickupAmmo,
		NULL,
		NULL,
		NULL,
		"ammo/common/pickup.wav",
		"models/ammo/shells/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/a_shells",
		"Shells",
		10,
		NULL,
		ITEM_AMMO,
		AMMO_SHELLS,
		0.15,
		"" },

/*QUAKED ammo_bullets(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"ammo_bullets",
		G_PickupAmmo,
		NULL,
		NULL,
		NULL,
		"ammo/common/pickup.wav",
		"models/ammo/bullets/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/a_bullets",
		"Bullets",
		50,
		NULL,
		ITEM_AMMO,
		AMMO_BULLETS,
		0.15,
		"" },

/*QUAKED ammo_grenades(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"ammo_grenades",
		G_PickupAmmo,
		NULL,
		NULL,
		NULL,
		"ammo/common/pickup.wav",
		"models/ammo/grenades/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/a_grenades",
		"Grenades",
		10,
		"grenades",
		ITEM_AMMO,
		AMMO_GRENADES,
		0.15,
		"" },

/*QUAKED ammo_rockets(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"ammo_rockets",
		G_PickupAmmo,
		NULL,
		NULL,
		NULL,
		"ammo/common/pickup.wav",
		"models/ammo/rockets/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/a_rockets",
		"Rockets",
		10,
		NULL,
		ITEM_AMMO,
		AMMO_ROCKETS,
		0.15,
		"" },

/*QUAKED ammo_cells(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"ammo_cells",
		G_PickupAmmo,
		NULL,
		NULL,
		NULL,
		"ammo/common/pickup.wav",
		"models/ammo/cells/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/a_cells",
		"Cells",
		50,
		NULL,
		ITEM_AMMO,
		AMMO_CELLS,
		0.15,
		"" },

/*QUAKED ammo_bolts(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"ammo_bolts",
		G_PickupAmmo,
		NULL,
		NULL,
		NULL,
		"ammo/common/pickup.wav",
		"models/ammo/bolts/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/a_bolts",
		"Bolts",
		25,
		NULL,
		ITEM_AMMO,
		AMMO_BOLTS,
		0.15,
		"" },

/*QUAKED ammo_slugs(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"ammo_slugs",
		G_PickupAmmo,
		NULL,
		NULL,
		NULL,
		"ammo/common/pickup.wav",
		"models/ammo/slugs/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/a_slugs",
		"Slugs",
		10,
		NULL,
		ITEM_AMMO,
		AMMO_SLUGS,
		0.15,
		"" },

/*QUAKED ammo_nukes(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"ammo_nukes",
		G_PickupAmmo,
		NULL,
		NULL,
		NULL,
		"ammo/common/pickup.wav",
		"models/ammo/nukes/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/a_nukes",
		"Nukes",
		2,
		NULL,
		ITEM_AMMO,
		AMMO_NUKES,
		0.15,
		"" },

/*QUAKED item_adrenaline(.3 .3 1)(-16 -16 -16)(16 16 16)
 gives +1 to maximum health
 */
{
		"item_adrenaline",
		G_PickupAdrenaline,
		NULL,
		NULL,
		NULL,
		"adren/pickup.wav",
		"models/powerups/adren/tris.obj",
		EF_ROTATE | EF_PULSE,
		"pics/p_adrenaline",
		"Adrenaline",
		0,
		NULL,
		0,
		0,
		0.45,
		"" },

/*QUAKED item_health_small (.3 .3 1) (-16 -16 -16) (16 16 16)
 */
{
		"item_health_small",
		G_PickupHealth,
		NULL,
		NULL,
		NULL,
		"health/small/pickup.wav",
		"models/health/small/tris.obj",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/i_health",
		"Small Health",
		3,
		NULL,
		ITEM_HEALTH,
		HEALTH_SMALL,
		0.10,
		"" },

/*QUAKED item_health (.3 .3 1) (-16 -16 -16) (16 16 16)
 */
{
		"item_health",
		G_PickupHealth,
		NULL,
		NULL,
		NULL,
		"health/medium/pickup.wav",
		"models/health/medium/tris.obj",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/i_health",
		"Medium Health",
		15,
		NULL,
		ITEM_HEALTH,
		HEALTH_MEDIUM,
		0.25,
		"" },

/*QUAKED item_health_large (.3 .3 1) (-16 -16 -16) (16 16 16)
 */
{
		"item_health_large",
		G_PickupHealth,
		NULL,
		NULL,
		NULL,
		"health/large/pickup.wav",
		"models/health/large/tris.obj",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/i_health",
		"Large Health",
		25,
		NULL,
		ITEM_HEALTH,
		HEALTH_LARGE,
		0.40,
		"" },

/*QUAKED item_health_mega (.3 .3 1) (-16 -16 -16) (16 16 16)
 */
{
		"item_health_mega",
		G_PickupHealth,
		NULL,
		NULL,
		NULL,
		"health/mega/pickup.wav",
		"models/health/mega/tris.obj",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/i_health",
		"Mega Health",
		75,
		NULL,
		ITEM_HEALTH,
		HEALTH_MEGA,
		0.60,
		"" },

/*QUAKED item_flag_team1(1 0.2 0)(-16 -16 -24)(16 16 32)
 */
{
		"item_flag_team1",
		G_PickupFlag,
		NULL,
		G_DropFlag,
		NULL,
		NULL,
		"models/ctf/flag1/tris.md3",
		EF_BOB | EF_ROTATE,
		"pics/i_flag1",
		"Flag",
		0,
		NULL,
		ITEM_FLAG,
		0,
		0.75,
		"ctf/capture.wav ctf/steal.wav ctf/return.wav" },

/*QUAKED item_flag_team2(1 0.2 0)(-16 -16 -24)(16 16 32)
 */
{
		"item_flag_team2",
		G_PickupFlag,
		NULL,
		G_DropFlag,
		NULL,
		NULL,
		"models/ctf/flag2/tris.md3",
		EF_BOB | EF_ROTATE,
		"pics/i_flag2",
		"Flag",
		0,
		NULL,
		ITEM_FLAG,
		0,
		0.75,
		"ctf/capture.wav ctf/steal.wav ctf/return.wav" },

/*QUAKED item_quad(.3 .3 1)(-16 -16 -16)(16 16 16)
 */
{
		"item_quad",
		G_PickupQuadDamage,
		NULL,
		NULL,
		NULL,
		"quad/pickup.wav",
		"models/powerups/quad/tris.md3",
		EF_BOB | EF_ROTATE,
		"pics/i_quad",
		"Quad Damage",
		0,
		NULL,
		ITEM_POWERUP,
		0,
		0.80,
		"quad/attack.wav quad/expire.wav" }, };

const uint16_t g_num_items = lengthof(g_items);
