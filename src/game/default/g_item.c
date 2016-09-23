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

const vec3_t ITEM_MINS = { -16.0, -16.0, -16.0 };
const vec3_t ITEM_MAXS = { 16.0, 16.0, 16.0 };

#define ITEM_SCALE 1.0

/**
 * @brief
 */
const g_item_t *G_ItemByIndex(uint16_t index) {

	if (index == 0 || index >= g_num_items)
		return NULL;

	return &g_items[index];
}

/**
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

/**
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

		if (!g_ascii_strcasecmp(it->name, name))
			return it;
	}

	return NULL;
}

/**
 * @return The strongest armor item held by the specified client, or `NULL`. This
 * will never return the shard armor, because shards are added to the currently
 * held armor type, or to jacket armor if no armor is held.
 */
const g_item_t *G_ClientArmor(const g_entity_t *ent) {

	if (ent->client) {

		if (ent->client->locals.inventory[g_media.items.body_armor]) {
			return &g_items[g_media.items.body_armor];
		}

		if (ent->client->locals.inventory[g_media.items.combat_armor]) {
			return &g_items[g_media.items.combat_armor];
		}

		if (ent->client->locals.inventory[g_media.items.jacket_armor]) {
			return &g_items[g_media.items.jacket_armor];
		}
	}

	return NULL;
}

/**
 * @brief
 */
static void G_ItemRespawn(g_entity_t *ent) {

	if (ent->locals.team) {
		if (ent->locals.team_chain) {
			ent = ent->locals.team_chain;
		} else {
			ent = ent->locals.team_master;
		}
	}

	ent->sv_flags &= ~SVF_NO_CLIENT;
	ent->solid = SOLID_TRIGGER;

	gi.LinkEntity(ent);

	// send an effect
	ent->s.event = EV_ITEM_RESPAWN;
}

/**
 * @brief
 */
void G_SetItemRespawn(g_entity_t *ent, uint32_t delay) {

	ent->locals.next_think = g_level.time + delay;
	ent->locals.Think = G_ItemRespawn;

	ent->solid = SOLID_NOT;
	ent->sv_flags |= SVF_NO_CLIENT;

	gi.LinkEntity(ent);
}

/**
 * @brief
 */
static _Bool G_PickupAdrenaline(g_entity_t *ent, g_entity_t *other) {

	if (other->locals.health < other->locals.max_health)
		other->locals.health = other->locals.max_health;

	if (!(ent->locals.spawn_flags & SF_ITEM_DROPPED))
		G_SetItemRespawn(ent, 60000);

	return true;
}

/**
 * @brief
 */
static _Bool G_PickupQuadDamage(g_entity_t *ent, g_entity_t *other) {

	if (other->client->locals.inventory[g_media.items.quad_damage])
		return false; // already have it

	other->client->locals.inventory[g_media.items.quad_damage] = 1;

	if (ent->locals.spawn_flags & SF_ITEM_DROPPED) { // receive only the time left
		other->client->locals.quad_damage_time = ent->locals.next_think;
	} else {
		other->client->locals.quad_damage_time = g_level.time + 20000;
		G_SetItemRespawn(ent, 90000);
	}

	other->s.effects |= EF_QUAD;
	return true;
}

/**
 * @brief
 */
g_entity_t *G_TossQuadDamage(g_entity_t *ent) {
	g_entity_t *quad;

	if (!ent->client->locals.inventory[g_media.items.quad_damage])
		return NULL;

	quad = G_DropItem(ent, G_FindItemByClassName("item_quad"));

	if (quad)
		quad->locals.timestamp = ent->client->locals.quad_damage_time;

	ent->client->locals.quad_damage_time = 0.0;
	ent->client->locals.inventory[g_media.items.quad_damage] = 0;

	return quad;
}

/**
 * @brief
 */
_Bool G_AddAmmo(g_entity_t *ent, const g_item_t *item, int16_t count) {
	uint16_t index;
	int16_t max;

	if (item->tag == AMMO_SHELLS)
		max = ent->client->locals.max_shells;
	else if (item->tag == AMMO_BULLETS)
		max = ent->client->locals.max_bullets;
	else if (item->tag == AMMO_GRENADES)
		max = ent->client->locals.max_grenades;
	else if (item->tag == AMMO_ROCKETS)
		max = ent->client->locals.max_rockets;
	else if (item->tag == AMMO_CELLS)
		max = ent->client->locals.max_cells;
	else if (item->tag == AMMO_BOLTS)
		max = ent->client->locals.max_bolts;
	else if (item->tag == AMMO_SLUGS)
		max = ent->client->locals.max_slugs;
	else if (item->tag == AMMO_NUKES)
		max = ent->client->locals.max_nukes;
	else
		return false;

	index = ITEM_INDEX(item);

	ent->client->locals.inventory[index] += count;

	if (ent->client->locals.inventory[index] > max)
		ent->client->locals.inventory[index] = max;
	else if (ent->client->locals.inventory[index] < 0)
		ent->client->locals.inventory[index] = 0;

	return true;
}

/**
 * @brief
 */
_Bool G_SetAmmo(g_entity_t *ent, const g_item_t *item, int16_t count) {
	uint16_t index;
	int16_t max;

	if (item->tag == AMMO_SHELLS)
		max = ent->client->locals.max_shells;
	else if (item->tag == AMMO_BULLETS)
		max = ent->client->locals.max_bullets;
	else if (item->tag == AMMO_GRENADES)
		max = ent->client->locals.max_grenades;
	else if (item->tag == AMMO_ROCKETS)
		max = ent->client->locals.max_rockets;
	else if (item->tag == AMMO_CELLS)
		max = ent->client->locals.max_cells;
	else if (item->tag == AMMO_BOLTS)
		max = ent->client->locals.max_bolts;
	else if (item->tag == AMMO_SLUGS)
		max = ent->client->locals.max_slugs;
	else if (item->tag == AMMO_NUKES)
		max = ent->client->locals.max_nukes;
	else
		return false;

	index = ITEM_INDEX(item);

	ent->client->locals.inventory[index] = count;

	if (ent->client->locals.inventory[index] > max)
		ent->client->locals.inventory[index] = max;
	else if (ent->client->locals.inventory[index] < 0)
		ent->client->locals.inventory[index] = 0;

	return true;
}

/**
 * @brief
 */
static _Bool G_PickupAmmo(g_entity_t *ent, g_entity_t *other) {
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

/**
 * @brief
 */
static _Bool G_PickupHealth(g_entity_t *ent, g_entity_t *other) {
	int32_t h, max;

	const uint16_t tag = ent->locals.item->tag;

	const _Bool always_add = tag == HEALTH_SMALL;
	const _Bool always_pickup = (tag == HEALTH_SMALL || tag == HEALTH_MEGA);

	if (other->locals.health < other->locals.max_health || always_add || always_pickup) {

		h = other->locals.health + ent->locals.item->quantity; // target health points
		max = other->locals.max_health;

		if (always_pickup) { // resolve max
			if (h > other->locals.max_health) {
				max = other->client->locals.max_boost_health;
}
		} else if (always_add)
			max = INT16_MAX;
		if (h > max) // and enforce it
			h = max;

		other->locals.health = h;

		if (tag == HEALTH_MEGA) // respawn the item
			G_SetItemRespawn(ent, 90000);
		else if (tag == HEALTH_LARGE)
			G_SetItemRespawn(ent, 60000);
		else
			G_SetItemRespawn(ent, 20000);

		return true;
	}

	other->client->locals.boost_time = g_level.time + 750;

	return false;
}

/**
 * @return The g_armor_info_t for the specified item.
 */
const g_armor_info_t *G_ArmorInfo(const g_item_t *armor) {
	static const g_armor_info_t armor_info[] = {
		{ ARMOR_BODY, 100, 200, 0.8, 0.6 },
		{ ARMOR_COMBAT, 50, 100, 0.6, 0.3 },
		{ ARMOR_JACKET, 25, 50, 0.3, 0.0 }
	};

	if (!armor)
		return NULL;

	for (size_t i = 0; i < lengthof(armor_info); i++) {
		if (armor->tag == armor_info[i].tag) {
			return &armor_info[i];
		}
	}

	return NULL;
}

/**
 * @brief
 */
static _Bool G_PickupArmor(g_entity_t *ent, g_entity_t *other) {

	const g_item_t *new_armor = ent->locals.item;
	const g_item_t *current_armor = G_ClientArmor(other);

	const g_armor_info_t *new_info = G_ArmorInfo(new_armor);
	const g_armor_info_t *current_info = G_ArmorInfo(current_armor);
	
	_Bool taken = false;

	if (new_armor->tag == ARMOR_SHARD) { // take it, ignoring cap
		if (current_armor) {
			other->client->locals.inventory[ITEM_INDEX(current_armor)] += new_armor->quantity;
		} else {
			other->client->locals.inventory[g_media.items.jacket_armor] = new_armor->quantity;
		}
		taken = true;
	} else if (!current_armor) { // no current armor, take it
		other->client->locals.inventory[ITEM_INDEX(new_armor)] = new_armor->quantity;
		taken = true;
	} else {
		// we picked up stronger armor than we currently had
		if (new_info->normal_protection > current_info->normal_protection) {

			// get the ratio between the new and old armor to add a portion to
			// new armor pickup. Ganked from q2pro (thanks skuller) 
			const vec_t salvage = current_info->normal_protection / new_info->normal_protection;
			const int16_t salvage_count = salvage * other->client->locals.inventory[ITEM_INDEX(current_armor)];

			const int16_t new_count = Clamp(salvage_count + new_armor->quantity, 0, new_info->max_count);

			other->client->locals.inventory[ITEM_INDEX(new_armor)] = new_count;
			other->client->locals.inventory[ITEM_INDEX(current_armor)] = 0;
			taken = true;
		} else {
			// we picked up the same, or weaker
			const vec_t salvage = new_info->normal_protection / current_info->normal_protection;
			const int16_t salvage_count = salvage * new_armor->quantity;

			int16_t new_count = salvage_count + other->client->locals.inventory[ITEM_INDEX(current_armor)];
			new_count = Clamp(new_count, 0, current_info->max_count);

			// take it
			if (other->client->locals.inventory[ITEM_INDEX(current_armor)] < new_count) {
				other->client->locals.inventory[ITEM_INDEX(current_armor)] = new_count;
				taken = true;
			}
		}
	}

	if (taken && !(ent->locals.spawn_flags & SF_ITEM_DROPPED)) {
		switch (new_armor->tag) {
			case ARMOR_SHARD:
				G_SetItemRespawn(ent, 20000);
				break;
			case ARMOR_JACKET:
			case ARMOR_COMBAT:
				G_SetItemRespawn(ent, 60000);
				break;
			case ARMOR_BODY:
				G_SetItemRespawn(ent, 90000);
				break;
			default:
				gi.Debug("Invalid armor tag: %d\n", new_armor->tag);
				break;
		}
	}

	return taken;
}

/**
 * @brief A dropped flag has been idle for 30 seconds, return it.
 */
void G_ResetDroppedFlag(g_entity_t *ent) {
	g_team_t *t;
	g_entity_t *f;

	if (!(t = G_TeamForFlag(ent)))
		return;

	if (!(f = G_FlagForTeam(t)))
		return;

	f->sv_flags &= ~SVF_NO_CLIENT;
	f->s.event = EV_ITEM_RESPAWN;

	gi.LinkEntity(f);

	gi.Sound(ent, gi.SoundIndex("ctf/return"), ATTEN_NONE);

	gi.BroadcastPrint(PRINT_HIGH, "The %s flag has been returned\n", t->name);

	G_FreeEntity(ent);
}

/**
 * @brief Return own flag, or capture on it if enemy's flag is in inventory.
 * Take the enemy's flag.
 */
static _Bool G_PickupFlag(g_entity_t *ent, g_entity_t *other) {
	g_team_t *t, *ot;
	g_entity_t *f, *of;
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

			f->solid = SOLID_TRIGGER;
			f->sv_flags &= ~SVF_NO_CLIENT;

			gi.LinkEntity(f);

			f->s.event = EV_ITEM_RESPAWN;

			gi.Sound(other, gi.SoundIndex("ctf/return"), ATTEN_NONE);

			gi.BroadcastPrint(PRINT_HIGH, "%s returned the %s flag\n",
					other->client->locals.persistent.net_name, t->name);

			return true;
		}

		index = ITEM_INDEX(of->locals.item);
		if (other->client->locals.inventory[index]) { // capture

			other->client->locals.inventory[index] = 0;
			other->s.effects &= ~G_EffectForTeam(ot);
			other->s.model3 = 0;

			of->solid = SOLID_TRIGGER;
			of->sv_flags &= ~SVF_NO_CLIENT; // reset the other flag

			gi.LinkEntity(of);

			of->s.event = EV_ITEM_RESPAWN;

			gi.Sound(other, gi.SoundIndex("ctf/capture"), ATTEN_NONE);

			gi.BroadcastPrint(PRINT_HIGH, "%s captured the %s flag\n",
					other->client->locals.persistent.net_name, ot->name);

			t->captures++;
			other->client->locals.persistent.captures++;

			return false;
		}

		// touching our own flag for no particular reason
		return false;
	}

	// it's enemy's flag, so take it

	f->solid = SOLID_NOT;
	f->sv_flags |= SVF_NO_CLIENT;

	gi.LinkEntity(f);

	index = ITEM_INDEX(f->locals.item);
	other->client->locals.inventory[index] = 1;

	// link the flag model to the player
	other->s.model3 = gi.ModelIndex(f->locals.item->model);

	gi.Sound(other, gi.SoundIndex("ctf/steal"), ATTEN_NONE);

	gi.BroadcastPrint(PRINT_HIGH, "%s stole the %s flag\n",
			other->client->locals.persistent.net_name, t->name);

	other->s.effects |= G_EffectForTeam(t);
	return true;
}

/**
 * @brief
 */
g_entity_t *G_TossFlag(g_entity_t *ent) {
	g_team_t *ot;
	g_entity_t *of;

	if (!(ot = G_OtherTeam(ent->client->locals.persistent.team)))
		return NULL;

	if (!(of = G_FlagForTeam(ot)))
		return NULL;

	const int32_t index = ITEM_INDEX(of->locals.item);

	if (!ent->client->locals.inventory[index])
		return NULL;

	ent->client->locals.inventory[index] = 0;

	ent->s.model3 = 0;
	ent->s.effects &= ~(EF_CTF_RED | EF_CTF_BLUE);

	gi.BroadcastPrint(PRINT_HIGH, "%s dropped the %s flag\n",
			ent->client->locals.persistent.net_name, ot->name);

	return G_DropItem(ent, of->locals.item);
}

/**
 * @brief
 */
static g_entity_t *G_DropFlag(g_entity_t *ent, const g_item_t *item __attribute__((unused))) {
	return G_TossFlag(ent);
}

/**
 * @brief
 */
void G_TouchItem(g_entity_t *ent, g_entity_t *other,
		const cm_bsp_plane_t *plane __attribute__((unused)),
		const cm_bsp_surface_t *surf __attribute__((unused))) {

	if (other == ent->owner) {
		if (ent->locals.ground_entity == NULL)
			return;
	}

	if (!other->client)
		return;

	if (other->locals.dead)
		return; // dead people can't pickup

	if (!ent->locals.item->Pickup)
		return; // item can't be picked up

	if (g_level.warmup)
		return; // warmup mode

	const _Bool pickup = ent->locals.item->Pickup(ent, other);
	if (pickup) {
		// show icon and name on status bar
		uint16_t icon = gi.ImageIndex(ent->locals.item->icon);

		if (other->client->ps.stats[STAT_PICKUP_ICON] == icon)
			icon |= STAT_TOGGLE_BIT;

		other->client->ps.stats[STAT_PICKUP_ICON] = icon;
		other->client->ps.stats[STAT_PICKUP_STRING] = CS_ITEMS + ITEM_INDEX(ent->locals.item);

		other->client->locals.pickup_msg_time = g_level.time + 3000;

		if (ent->locals.item->pickup_sound) { // play pickup sound
			gi.Sound(other, gi.SoundIndex(ent->locals.item->pickup_sound), ATTEN_NORM);
		}

		other->s.event = EV_ITEM_PICKUP;
	}

	if (!(ent->locals.spawn_flags & SF_ITEM_TARGETS_USED)) {
		G_UseTargets(ent, other);
		ent->locals.spawn_flags |= SF_ITEM_TARGETS_USED;
	}

	if (pickup) {
		if (ent->locals.spawn_flags & SF_ITEM_DROPPED) {
			G_FreeEntity(ent);
		}
	}
}

/**
 * @brief
 */
static void G_DropItem_Think(g_entity_t *ent) {

	// continue to think as we drop to the floor
	if (ent->locals.ground_entity) {

		if (ent->locals.item->type == ITEM_FLAG) // flags go back to base
			ent->locals.Think = G_ResetDroppedFlag;
		else // everything else just gets freed
			ent->locals.Think = G_FreeEntity;

		uint32_t expiration;
		if (ent->locals.item->type == ITEM_POWERUP) // expire from last touch
			expiration = ent->locals.timestamp - g_level.time;
		else // general case
			expiration = 30000;

		const int32_t contents = gi.PointContents(ent->s.origin);

		if (contents & CONTENTS_LAVA) // expire more quickly in lava
			expiration /= 5;
		if (contents & CONTENTS_SLIME) // and slime
			expiration /= 2;

		ent->locals.next_think = g_level.time + expiration;
	} else {
		ent->locals.next_think = g_level.time + gi.frame_millis;
	}
}

/**
 * @brief Handles the mechanics of dropping items, but does not adjust the client's
 * inventory. That is left to the caller.
 */
g_entity_t *G_DropItem(g_entity_t *ent, const g_item_t *item) {
	vec3_t forward;
	cm_trace_t tr;

	g_entity_t *it = G_AllocEntity(item->class_name);
	it->owner = ent;

	VectorScale(ITEM_MINS, ITEM_SCALE, it->mins);
	VectorScale(ITEM_MAXS, ITEM_SCALE, it->maxs);

	it->solid = SOLID_TRIGGER;

	// resolve forward direction and project origin
	if (ent->client && ent->locals.dead) {
		vec3_t tmp;

		VectorSet(tmp, 0.0, ent->client->locals.angles[1], 0.0);
		AngleVectors(tmp, forward, NULL, NULL);

		VectorMA(ent->s.origin, 24.0, forward, it->s.origin);
	} else {
		AngleVectors(ent->s.angles, forward, NULL, NULL);
		VectorCopy(ent->s.origin, it->s.origin);
	}

	tr = gi.Trace(ent->s.origin, it->s.origin, it->mins, it->maxs, ent, MASK_CLIP_PLAYER);

	// we're in a bad spot, forget it
	if (tr.start_solid) {
		if (item->type == ITEM_FLAG)
			G_ResetDroppedFlag(it);
		else
			G_FreeEntity(it);

		return NULL;
	}

	VectorCopy(tr.end, it->s.origin);

	it->locals.item = item;
	it->locals.spawn_flags |= SF_ITEM_DROPPED;
	it->locals.move_type = MOVE_TYPE_BOUNCE;
	it->locals.Touch = G_TouchItem;
	it->s.effects = item->effects;

	gi.SetModel(it, item->model);

	if (item->type == ITEM_WEAPON) {
		const g_item_t *ammo = G_FindItem(item->ammo);
		if (ammo)
			it->locals.health = ammo->quantity;
	}

	VectorScale(forward, 100.0, it->locals.velocity);
	it->locals.velocity[2] = 300.0 + (Randomf() * 50.0);

	it->locals.Think = G_DropItem_Think;
	it->locals.next_think = g_level.time + gi.frame_millis;

	gi.LinkEntity(it);

	return it;
}

/**
 * @brief
 */
static void G_UseItem(g_entity_t *ent, g_entity_t *other __attribute__((unused)), g_entity_t *activator __attribute__((unused))) {

	ent->sv_flags &= ~SVF_NO_CLIENT;
	ent->locals.Use = NULL;

	if (ent->locals.spawn_flags & SF_ITEM_NO_TOUCH) {
		ent->solid = SOLID_BOX;
		ent->locals.Touch = NULL;
	} else {
		ent->solid = SOLID_TRIGGER;
		ent->locals.Touch = G_TouchItem;
	}

	gi.LinkEntity(ent);
}

/**
 * @brief Reset the item's interaction state based on the current game state.
 */
void G_ResetItem(g_entity_t *ent) {

	ent->solid = SOLID_TRIGGER;
	ent->sv_flags &= ~SVF_NO_CLIENT;
	ent->locals.Touch = G_TouchItem;

	if (ent->locals.item->type == ITEM_FLAG) {
		if (g_level.ctf == false) {
			ent->sv_flags |= SVF_NO_CLIENT;
			ent->solid = SOLID_NOT;
		}
	}

	if (ent->locals.spawn_flags & SF_ITEM_TRIGGER) {
		ent->sv_flags |= SVF_NO_CLIENT;
		ent->solid = SOLID_NOT;
		ent->locals.Use = G_UseItem;
	}

	if (ent->locals.spawn_flags & SF_ITEM_NO_TOUCH) {
		ent->solid = SOLID_BOX;
		ent->locals.Touch = NULL;
	}

	const _Bool inhibited = g_level.gameplay == GAME_ARENA || g_level.gameplay == GAME_INSTAGIB;
	
	if (inhibited || (ent->locals.flags & FL_TEAM_SLAVE)) {
		ent->sv_flags |= SVF_NO_CLIENT;
		ent->solid = SOLID_NOT;
	}

	gi.LinkEntity(ent);
}

/**
 * @brief Drops the specified item to the floor and sets up interaction
 * properties (Touch, Use, move type, ..).
 */
static void G_ItemDropToFloor(g_entity_t *ent) {
	cm_trace_t tr;
	vec3_t dest;

	VectorClear(ent->locals.velocity);
	VectorCopy(ent->s.origin, dest);

	if (!(ent->locals.spawn_flags & SF_ITEM_HOVER)) {
		ent->locals.move_type = MOVE_TYPE_BOUNCE;

		// make an effort to come up out of the floor (broken maps)
		tr = gi.Trace(ent->s.origin, ent->s.origin, ent->mins, ent->maxs, ent, MASK_SOLID);
		if (tr.start_solid) {
			gi.Debug("%s start_solid, correcting..\n", etos(ent));
			ent->s.origin[2] += 8.0;
		}
	} else {
		ent->locals.move_type = MOVE_TYPE_FLY;
	}

	tr = gi.Trace(ent->s.origin, dest, ent->mins, ent->maxs, ent, MASK_SOLID);
	if (tr.start_solid) {
		gi.Warn("%s start_solid\n", etos(ent));
		G_FreeEntity(ent);
		return;
	}

	G_ResetItem(ent);
}

/**
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

	// parse the space-separated precache string for other items
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

/**
 * @brief Sets the clipping size and plants the object on the floor.
 *
 * Items can't be immediately dropped to floor, because they might
 * be on an entity that hasn't spawned yet.
 */
void G_SpawnItem(g_entity_t *ent, const g_item_t *item) {

	ent->locals.item = item;
	G_PrecacheItem(ent->locals.item);

	VectorScale(ITEM_MINS, ITEM_SCALE, ent->mins);
	VectorScale(ITEM_MAXS, ITEM_SCALE, ent->maxs);

	if (ent->model)
		gi.SetModel(ent, ent->model);
	else
		gi.SetModel(ent, ent->locals.item->model);

	ent->s.effects = item->effects;

	// weapons override the health field to store their ammo count
	if (ent->locals.item->type == ITEM_WEAPON) {
		const g_item_t *ammo = G_FindItem(ent->locals.item->ammo);
		if (ammo)
			ent->locals.health = ammo->quantity;
		else
			ent->locals.health = 0;
	}

	ent->locals.next_think = g_level.time + gi.frame_millis * 2;
	ent->locals.Think = G_ItemDropToFloor;
}

const g_item_t g_items[] = {
/*QUAKED item_armor_body (.8 .2 .2) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Body armor (+200).

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/armor/body/tris.md3"
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
		100,
		NULL,
		ITEM_ARMOR,
		ARMOR_BODY,
		0.80,
		"" },

/*QUAKED item_armor_combat (.8 .8 .2) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Combat armor (+100).

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/armor/combat/tris.md3"
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
		50,
		NULL,
		ITEM_ARMOR,
		ARMOR_COMBAT,
		0.66,
		"" },

/*QUAKED item_armor_jacket (.2 .8 .2) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Jacket armor (+50).

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/armor/jacket/tris.md3"
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
		25,
		NULL,
		ITEM_ARMOR,
		ARMOR_JACKET,
		0.50,
		"" },

/*QUAKED item_armor_shard (.1 .6 .1) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Shard armor (+3).

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/armor/shard/tris.md3"
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

/*QUAKED weapon_blaster (.8 .8 .1) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Blaster.

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/weapons/shotgun/tris.obj"
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

/*QUAKED weapon_shotgun (.6 .6 .1) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Shotgun.

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/weapons/shotgun/tris.obj"
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

/*QUAKED weapon_supershotgun (.6 .6 .1) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Super shotgun.

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/weapons/supershotgun/tris.obj"
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

/*QUAKED weapon_machinegun (.8 .8 .2) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Machinegun.

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/weapons/machinegun/tris.obj"
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

/*QUAKED weapon_grenadelauncher (.2 .8 .2) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Grenade Launcher.

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/weapons/grenadelauncher/tris.obj"
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

/*QUAKED weapon_rocketlauncher (.8 .2 .2) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Rocket Launcher.

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/weapons/rocketlauncher/tris.md3"
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

/*QUAKED weapon_hyperblaster (.4 .7 1) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Hyperblaster.

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/weapons/hyperblaster/tris.md3"
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

/*QUAKED weapon_lightning (.9 .9 .9) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Lightning.

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/weapons/lightning/tris.md3"
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

/*QUAKED weapon_railgun (.1 .1 .8) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Railgun.

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/weapons/railgun/tris.obj"
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

/*QUAKED weapon_bfg (.4 1 .5) (-16 -16 -16) (16 16 16) triggered no_touch hover
 BFG10K.

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/weapons/bfg/tris.md3"
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
		"weapons/bfg/prime.wav weapons/bfg/hit.wav" },

/*QUAKED ammo_shells (.6 .6 .1) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Shells for the Shotgun and Super Shotgun.

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/ammo/shells/tris.md3"
 */
{
		"ammo_shells",
		G_PickupAmmo,
		NULL,
		G_DropItem,
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

/*QUAKED ammo_bullets (.8 .8 .2) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Bullets for the Machinegun.

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/ammo/bullets/tris.md3"
 */
{
		"ammo_bullets",
		G_PickupAmmo,
		NULL,
		G_DropItem,
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

/*QUAKED ammo_grenades (.2 .8 .2) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Grenades for the Grenade Launcher.

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/ammo/grenades/tris.md3"
 */
{
		"ammo_grenades",
		G_PickupAmmo,
		G_UseWeapon,
		G_DropItem,
		G_FireHandGrenade,
		"ammo/common/pickup.wav",
		"models/ammo/grenades/tris.md3",
		EF_ROTATE | EF_BOB | EF_PULSE,
		"pics/a_handgrenades",
		"Grenades",
		10,
		"grenades",
		ITEM_AMMO,
		AMMO_GRENADES,
		0.15,
		"weapons/handgrenades/hg_throw.wav weapons/handgrenades/hg_clang.ogg weapons/handgrenades/hg_tick.ogg" },

/*QUAKED ammo_rockets (.8 .2 .2) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Rockets for the Rocket Launcher.

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/ammo/rockets/tris.md3"
 */
{
		"ammo_rockets",
		G_PickupAmmo,
		NULL,
		G_DropItem,
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

/*QUAKED ammo_cells (.4 .7 1) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Cells for the Hyperblaster.

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/ammo/cells/tris.md3"
 */
{
		"ammo_cells",
		G_PickupAmmo,
		NULL,
		G_DropItem,
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

/*QUAKED ammo_bolts (.9 .9 .9) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Bolts for the Lightning.

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/ammo/bolts/tris.md3"
 */
{
		"ammo_bolts",
		G_PickupAmmo,
		NULL,
		G_DropItem,
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

/*QUAKED ammo_slugs (.1 .1 .8) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Slugs for the Railgun.

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/ammo/slugs/tris.md3"
 */
{
		"ammo_slugs",
		G_PickupAmmo,
		NULL,
		G_DropItem,
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

/*QUAKED ammo_nukes (.4 1 .5) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Nukes for the BFG10K.

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/ammo/nukes/tris.md3"
 */
{
		"ammo_nukes",
		G_PickupAmmo,
		NULL,
		G_DropItem,
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

/*QUAKED item_adrenaline (.3 .3 1) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Adrenaline (=100).

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/powerups/adren/tris.obj"
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

/*QUAKED item_health_small (.3 1 .3) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Small health (+3).

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/health/small/tris.obj"
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

/*QUAKED item_health (.8 .8 0) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Health (+15).

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/health/medium/tris.obj"
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

/*QUAKED item_health_large (1 0 0) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Large health (+25).

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/health/large/tris.obj"
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

/*QUAKED item_health_mega (.3 .3 1) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Mega health (+75).

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/health/mega/tris.obj"
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

/*QUAKED item_flag_team1 (.2 .2 1) (-16 -16 -24) (16 16 32) triggered no_touch hover

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/ctf/flag1/tris.md3"
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
		"Enemy Flag",
		0,
		NULL,
		ITEM_FLAG,
		0,
		0.75,
		"ctf/capture.wav ctf/steal.wav ctf/return.wav" },

/*QUAKED item_flag_team2 (1 .2 .2) (-16 -16 -24) (16 16 32) triggered no_touch hover

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/ctf/flag2/tris.md3"
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
		"Enemy Flag",
		0,
		NULL,
		ITEM_FLAG,
		0,
		0.75,
		"ctf/capture.wav ctf/steal.wav ctf/return.wav" },

/*QUAKED item_quad (.2 .4 1) (-16 -16 -16) (16 16 16) triggered no_touch hover
 Quad damage.

 -------- Keys --------
 team : The team name for alternating item spawns.

 -------- Spawn flags --------
 triggered : Item will not appear until triggered.
 no_touch : Item will interact as solid instead of being picked up by player.
 hover : Item will spawn where it was placed in the map and won't drop the floor.

 -------- Radiant config --------
 model="models/powerups/quad/tris.md3"
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
