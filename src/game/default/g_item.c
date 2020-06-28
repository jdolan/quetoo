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

const vec3_t ITEM_MINS = { { -16.0, -16.0, -16.0 } };
const vec3_t ITEM_MAXS = { {  16.0,  16.0,  32.0 } };

#define ITEM_SCALE 1.0

/**
 * @brief
 */
const g_item_t *G_FindItemByClassName(const char *class_name) {

	for (int32_t i = 0; i < g_num_items; i++) {
		const g_item_t *it = G_ItemByIndex(i);

		if (!it->class_name) {
			continue;
		}

		if (!g_strcmp0(it->class_name, class_name)) {
			return it;
		}
	}

	return NULL;
}

/**
 * @brief
 */
const g_item_t *G_FindItem(const char *name) {

	if (!name) {
		return NULL;
	}

	for (int32_t i = 0; i < g_num_items; i++) {
		const g_item_t *it = G_ItemByIndex(i);

		if (!it->name) {
			continue;
		}

		if (!g_ascii_strcasecmp(it->name, name)) {
			return it;
		}
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

		for (g_armor_t armor = ARMOR_BODY; armor > ARMOR_SHARD; armor--) {

			if (ent->client->locals.inventory[g_media.items.armor[armor]->index]) {
				return g_media.items.armor[armor];
			}
		}
	}

	return NULL;
}

/**
 * @brief
 */
static void G_ItemRespawn(g_entity_t *ent) {

	if (ent->locals.team) {
		if (ent->locals.team_next) {
			ent = ent->locals.team_next;
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

	if (other->locals.health < other->locals.max_health) {
		other->locals.health = other->locals.max_health;
	}

	if (!(ent->locals.spawn_flags & SF_ITEM_DROPPED)) {
		G_SetItemRespawn(ent, 40000);
	}

	return true;
}

/**
 * @brief
 */
static _Bool G_PickupQuadDamage(g_entity_t *ent, g_entity_t *other) {

	if (other->client->locals.inventory[g_media.items.powerups[POWERUP_QUAD]->index]) {
		return false; // already have it
	}

	other->client->locals.inventory[g_media.items.powerups[POWERUP_QUAD]->index] = 1;

	uint32_t delta = 3000;

	if (ent->locals.spawn_flags & SF_ITEM_DROPPED) { // receive only the time left
		delta = Maxf(0, Step((ent->locals.next_think + 1000) - g_level.time, 1000));

		other->client->locals.quad_damage_time = ent->locals.next_think;
		other->client->locals.quad_countdown_time = ent->locals.next_think - delta;
	} else {
		other->client->locals.quad_damage_time = g_level.time + SECONDS_TO_MILLIS(g_quad_damage_time->value);
		other->client->locals.quad_countdown_time = other->client->locals.quad_damage_time - delta;
		G_SetItemRespawn(ent, SECONDS_TO_MILLIS(g_quad_damage_respawn_time->value));
	}

	other->s.effects |= EF_QUAD;
	return true;
}

/**
 * @brief
 */
g_entity_t *G_TossQuadDamage(g_entity_t *ent) {
	g_entity_t *quad;

	if (!ent->client->locals.inventory[g_media.items.powerups[POWERUP_QUAD]->index]) {
		return NULL;
	}

	quad = G_DropItem(ent, g_media.items.powerups[POWERUP_QUAD]);

	if (quad) {
		quad->locals.timestamp = ent->client->locals.quad_damage_time;
	}

	ent->client->locals.quad_damage_time = 0.0;
	ent->client->locals.inventory[g_media.items.powerups[POWERUP_QUAD]->index] = 0;

	return quad;
}

/**
 * @brief
 */
_Bool G_AddAmmo(g_entity_t *ent, const g_item_t *item, int16_t count) {
	uint16_t index;
	int16_t max = item->max;

	if (!max) {
		return false;
	}

	index = item->index;

	ent->client->locals.inventory[index] += count;

	if (ent->client->locals.inventory[index] > max) {
		ent->client->locals.inventory[index] = max;
	} else if (ent->client->locals.inventory[index] < 0) {
		ent->client->locals.inventory[index] = 0;
	}

	return true;
}

/**
 * @brief
 */
_Bool G_SetAmmo(g_entity_t *ent, const g_item_t *item, int16_t count) {
	uint16_t index;
	int16_t max = item->max;

	if (!max) {
		return false;
	}

	index = item->index;

	ent->client->locals.inventory[index] = count;

	if (ent->client->locals.inventory[index] > max) {
		ent->client->locals.inventory[index] = max;
	} else if (ent->client->locals.inventory[index] < 0) {
		ent->client->locals.inventory[index] = 0;
	}

	return true;
}

/**
 * @brief
 */
static _Bool G_PickupAmmo(g_entity_t *ent, g_entity_t *other) {
	int32_t count;

	if (ent->locals.count) {
		count = ent->locals.count;
	} else {
		count = ent->locals.item->quantity;
	}

	if (!G_AddAmmo(other, ent->locals.item, count)) {
		return false;
	}

	if (!(ent->locals.spawn_flags & SF_ITEM_DROPPED)) {
		G_SetItemRespawn(ent, g_ammo_respawn_time->value * 1000);
	}

	return true;
}

/**
 * @brief When picking up grenades, give the hand grenades weapon in addition to the ammo.
 */
static _Bool G_PickupGrenades(g_entity_t *ent, g_entity_t *other) {

	const _Bool pickup = G_PickupAmmo(ent, other);
	if (pickup) {
		if (!other->client->locals.inventory[g_media.items.weapons[WEAPON_HAND_GRENADE]->index]) {
			other->client->locals.inventory[g_media.items.weapons[WEAPON_HAND_GRENADE]->index]++;
		}

		if (other->client->locals.weapon == g_media.items.weapons[WEAPON_BLASTER]) {
			G_UseWeapon(other, g_media.items.weapons[WEAPON_HAND_GRENADE]);
		}
	}

	return pickup;
}

/**
 * @brief When picking up the grenade launcher, give the hand grenades weapon as well.
 */
static _Bool G_PickupGrenadeLauncher(g_entity_t *ent, g_entity_t *other) {

	const _Bool pickup = G_PickupWeapon(ent, other);
	if (pickup) {
		if (!other->client->locals.inventory[g_media.items.weapons[WEAPON_HAND_GRENADE]->index]) {
			other->client->locals.inventory[g_media.items.weapons[WEAPON_HAND_GRENADE]->index]++;
		}
	}

	return pickup;
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
			if (h > max && other->client) {
				other->client->locals.boost_time = g_level.time + 750;
				max = other->client->locals.max_boost_health;
			}
		} else if (always_add) {
			max = INT16_MAX;
		}
		if (h > max) { // and enforce it
			h = max;
		}

		other->locals.health = h;

		if (tag == HEALTH_MEGA) { // respawn the item
			G_SetItemRespawn(ent, 60000);
		} else if (tag == HEALTH_LARGE) {
			G_SetItemRespawn(ent, 30000);
		} else {
			G_SetItemRespawn(ent, 20000);
		}

		return true;
	}

	return false;
}

/**
 * @return The g_armor_info_t for the specified item.
 */
const g_armor_info_t *G_ArmorInfo(const g_item_t *armor) {
	static const g_armor_info_t armor_info[] = {
		{ ARMOR_BODY, 0.8, 0.6 },
		{ ARMOR_COMBAT, 0.6, 0.3 },
		{ ARMOR_JACKET, 0.3, 0.0 }
	};

	if (!armor) {
		return NULL;
	}

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

	if (new_armor->tag == ARMOR_SHARD) { // always take it, ignoring cap
		if (current_armor) {
			other->client->locals.inventory[current_armor->index] =
			    Clampf(other->client->locals.inventory[current_armor->index] + new_armor->quantity,
			          0, other->client->locals.max_armor);
		} else {
			other->client->locals.inventory[g_media.items.armor[ARMOR_JACKET]->index] =
			    Clampf((int16_t) new_armor->quantity, 0, other->client->locals.max_armor);
		}

		taken = true;
	} else if (!current_armor) { // no current armor, take it
		other->client->locals.inventory[new_armor->index] =
		    Clampf((int16_t) new_armor->quantity, 0, other->client->locals.max_armor);

		taken = true;
	} else {
		// we picked up stronger armor than we currently had
		if (new_info->normal_protection > current_info->normal_protection) {

			// get the ratio between the new and old armor to add a portion to
			// new armor pickup. Ganked from q2pro (thanks skuller)
			const float salvage = current_info->normal_protection / new_info->normal_protection;
			const int16_t salvage_count = salvage * other->client->locals.inventory[current_armor->index];

			const int16_t new_count = Clampf(salvage_count + new_armor->quantity, 0, new_armor->max);

			if (new_count < other->client->locals.max_armor) {
				other->client->locals.inventory[current_armor->index] = 0;

				other->client->locals.inventory[new_armor->index] =
				    Clampf(new_count, 0, other->client->locals.max_armor);
			}

			taken = true;
		} else {
			// we picked up the same, or weaker
			const float salvage = new_info->normal_protection / current_info->normal_protection;
			const int16_t salvage_count = salvage * new_armor->quantity;

			int16_t new_count = salvage_count + other->client->locals.inventory[current_armor->index];
			new_count = Clampf(new_count, 0, current_armor->max);

			// take it
			if (other->client->locals.inventory[current_armor->index] < new_count &&
			        other->client->locals.inventory[current_armor->index] < other->client->locals.max_armor) {
				other->client->locals.inventory[current_armor->index] =
				    Clampf(new_count, 0, other->client->locals.max_armor);

				taken = true;
			}
		}
	}

	if (taken && !(ent->locals.spawn_flags & SF_ITEM_DROPPED)) {
		switch (new_armor->tag) {
			case ARMOR_SHARD:
				G_SetItemRespawn(ent, 15000);
				break;
			case ARMOR_JACKET:
				G_SetItemRespawn(ent, 20000);
				break;
			case ARMOR_COMBAT:
				G_SetItemRespawn(ent, 30000);
				break;
			case ARMOR_BODY:
				G_SetItemRespawn(ent, 40000);
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

	if (!(t = G_TeamForFlag(ent))) {
		return;
	}

	if (!(f = G_FlagForTeam(t))) {
		return;
	}

	f->sv_flags &= ~SVF_NO_CLIENT;
	f->s.event = EV_ITEM_RESPAWN;
	f->solid = SOLID_TRIGGER;

	gi.LinkEntity(f);

	gi.Sound(ent, gi.SoundIndex("ctf/return"), ATTEN_NONE, 0);

	gi.BroadcastPrint(PRINT_HIGH, "The %s flag has been returned\n", t->name);

	G_FreeEntity(ent);
}

/**
 * @brief Return own flag, or capture on it if enemy's flag is in inventory.
 * Take the enemy's flag.
 */
static _Bool G_PickupFlag(g_entity_t *ent, g_entity_t *other) {
	g_team_t *t;
	g_entity_t *f;
	int32_t index;

	if (!other->client->locals.persistent.team) {
		return false;
	}

	if (!(t = G_TeamForFlag(ent))) {
		return false;
	}

	if (!(f = G_FlagForTeam(t))) {
		return false;
	}

	const g_item_t *ofi = G_IsFlagBearer(other);

	if (t == other->client->locals.persistent.team) { // our flag

		if (ent->locals.spawn_flags & SF_ITEM_DROPPED) { // return it if necessary

			f->solid = SOLID_TRIGGER;
			f->sv_flags &= ~SVF_NO_CLIENT;

			gi.LinkEntity(f);

			f->s.event = EV_ITEM_RESPAWN;

			gi.Sound(other, gi.SoundIndex("ctf/return"), ATTEN_NONE, 0);

			gi.BroadcastPrint(PRINT_HIGH, "%s returned the %s flag\n",
			                  other->client->locals.persistent.net_name, t->name);

			return true;
		}

		if (ofi) {
			const g_team_t *ot = &g_teamlist[ofi->tag];
			g_entity_t *of = G_FlagForTeam(ot);

			index = of->locals.item->index;
			if (other->client->locals.inventory[index]) { // capture

				other->client->locals.inventory[index] = 0;
				other->s.effects &= ~G_EffectForTeam(ot);
				other->s.model3 = 0;

				of->solid = SOLID_TRIGGER;
				of->sv_flags &= ~SVF_NO_CLIENT; // reset the other flag

				gi.LinkEntity(of);

				of->s.event = EV_ITEM_RESPAWN;

				gi.Sound(other, gi.SoundIndex("ctf/capture"), ATTEN_NONE, 0);

				gi.BroadcastPrint(PRINT_HIGH, "%s captured the %s flag\n",
								  other->client->locals.persistent.net_name, ot->name);

				t->captures++;
				other->client->locals.persistent.captures++;

				return false;
			}
		}

		// touching our own flag for no particular reason
		return false;
	}

	// it's enemy's flag, so take it if we can
	if (ofi) {
		return false; // we have one already
	}

	f->solid = SOLID_NOT;
	f->sv_flags |= SVF_NO_CLIENT;

	gi.LinkEntity(f);

	index = f->locals.item->index;
	other->client->locals.inventory[index] = 1;

	// link the flag model to the player
	other->s.model3 = f->locals.item->model_index;

	gi.Sound(other, gi.SoundIndex("ctf/steal"), ATTEN_NONE, 0);

	gi.BroadcastPrint(PRINT_HIGH, "%s stole the %s flag\n",
	                  other->client->locals.persistent.net_name, t->name);

	other->s.effects |= G_EffectForTeam(t);
	return true;
}

/**
 * @brief
 */
g_entity_t *G_TossFlag(g_entity_t *ent) {
	const g_item_t *ofi;

	if (!(ofi = G_IsFlagBearer(ent))) {
		return NULL;
	}

	const g_team_t *ot = &g_teamlist[ofi->tag];
	const int32_t index = ofi->index;

	if (!ent->client->locals.inventory[index]) {
		return NULL;
	}

	ent->client->locals.inventory[index] = 0;

	ent->s.model3 = 0;
	ent->s.effects &= ~EF_CTF_MASK;

	gi.BroadcastPrint(PRINT_HIGH, "%s dropped the %s flag\n",
	                  ent->client->locals.persistent.net_name, ot->name);

	return G_DropItem(ent, ofi);
}

/**
 * @brief
 */
static g_entity_t *G_DropFlag(g_entity_t *ent, const g_item_t *item) {
	return G_TossFlag(ent);
}

/**
 * @brief
 */
static void G_DropItem_SetExpiration(g_entity_t *ent) {

	if (ent->locals.item->type == ITEM_FLAG) { // flags go back to base
		ent->locals.Think = G_ResetDroppedFlag;
	} else if (ent->locals.item->type == ITEM_TECH) {
		ent->locals.Think = G_ResetDroppedTech;
	} else { // everything else just gets freed
		ent->locals.Think = G_FreeEntity;
	}

	uint32_t expiration;
	if (ent->locals.item->type == ITEM_POWERUP) { // expire from last touch
		expiration = ent->locals.timestamp - g_level.time;
	} else { // general case
		expiration = 30000;
	}

	const int32_t contents = gi.PointContents(ent->s.origin);

	if (contents & CONTENTS_LAVA) { // expire more quickly in lava
		expiration /= 5;
	}
	if (contents & CONTENTS_SLIME) { // and slime
		expiration /= 2;
	}

	ent->locals.next_think = g_level.time + expiration;
}

/**
 * @brief
 */
static void G_DropItem_Think(g_entity_t *ent) {

	// continue to think as we drop to the floor
	if (ent->locals.ground_entity || (gi.PointContents(ent->s.origin) & CONTENTS_MASK_LIQUID)) {
		G_DropItem_SetExpiration(ent);
	} else {
		ent->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
	}
}

/**
 * @brief
 */
void G_TouchItem(g_entity_t *ent, g_entity_t *other,
                 const cm_bsp_plane_t *plane,
                 const cm_bsp_texinfo_t *surf) {

	if (other == ent->owner) {
		if (ent->locals.touch_time > g_level.time) {
			return;
		}
	}

	if (!other->client) {
		return;
	}

	if (other->locals.dead) {
		return; // dead people can't pickup
	}

	if (!ent->locals.item->Pickup) {
		return; // item can't be picked up
	}

	if (g_level.warmup) {
		return; // warmup mode
	}

	// if we still haven't thunk from being dropped, set the expiration now
	if (ent->locals.Think == G_DropItem_Think) {
		G_DropItem_SetExpiration(ent);
	}

	const _Bool pickup = ent->locals.item->Pickup(ent, other);
	if (pickup) {
		// show icon and name on status bar
		uint16_t icon = ent->locals.item->icon_index;

		if (other->client->ps.stats[STAT_PICKUP_ICON] == icon) {
			icon |= STAT_TOGGLE_BIT;
		}

		other->client->ps.stats[STAT_PICKUP_ICON] = icon;
		other->client->ps.stats[STAT_PICKUP_STRING] = CS_ITEMS + ent->locals.item->index;

		other->client->locals.pickup_msg_time = g_level.time + 3000;

		if (ent->locals.item->pickup_sound) { // play pickup sound
			gi.Sound(other, ent->locals.item->pickup_sound_index, ATTEN_NORM, 0);
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
 * @brief Handles the mechanics of dropping items, but does not adjust the client's
 * inventory. That is left to the caller.
 */
g_entity_t *G_DropItem(g_entity_t *ent, const g_item_t *item) {
	vec3_t forward;
	cm_trace_t tr;

	g_entity_t *it = G_AllocEntity_(item->class_name);
	it->owner = ent;

	it->mins = Vec3_Scale(ITEM_MINS, ITEM_SCALE);
	it->maxs = Vec3_Scale(ITEM_MAXS, ITEM_SCALE);

	it->solid = SOLID_TRIGGER;

	// resolve forward direction and project origin
	if (ent->client && ent->locals.dead) {
		Vec3_Vectors(Vec3(0.0, ent->client->locals.angles.y, 0.0), &forward, NULL, NULL);
		it->s.origin = Vec3_Add(ent->s.origin, Vec3_Scale(forward, 24.0));
	} else {
		Vec3_Vectors(ent->s.angles, &forward, NULL, NULL);
		it->s.origin = ent->s.origin;
		it->s.origin.z -= it->mins.z;
	}

	tr = gi.Trace(it->s.origin, it->s.origin, it->mins, it->maxs, ent, CONTENTS_MASK_SOLID);

	it->locals.item = item;

	// we're in a bad spot, forget it
	if (tr.start_solid) {
		if (item->type == ITEM_TECH) {
			G_ResetDroppedTech(it);
		}
		else {
			if (item->type == ITEM_FLAG) {
				G_ResetDroppedFlag(it);
			} else {
				G_FreeEntity(it);
			}

			return NULL;
		}
	}

	it->s.origin = tr.end;

	it->locals.spawn_flags |= SF_ITEM_DROPPED;
	it->locals.move_type = MOVE_TYPE_BOUNCE;
	it->locals.Touch = G_TouchItem;
	it->s.effects = item->effects;
	it->locals.touch_time = g_level.time + 1000;

	it->s.model1 = item->model_index;

	if (item->type == ITEM_WEAPON) {
		const g_item_t *ammo = item->ammo_item;
		if (ammo) {
			it->locals.health = ammo->quantity;
		}
	}

	it->locals.velocity = Vec3_Scale(forward, 100.0);
	it->locals.velocity.z = 300.0 + (Randomf() * 50.0);

	it->locals.Think = G_DropItem_Think;
	it->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;

	gi.LinkEntity(it);

	return it;
}

/**
 * @brief
 */
static void G_UseItem(g_entity_t *ent, g_entity_t *other, g_entity_t *activator) {

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
 * @brief
 */
static g_entity_t *G_TechEntity(const g_tech_t tech) {
	return G_FindPtr(NULL, LOFS(item), g_media.items.techs[tech]);
}

/**
 * @brief Returns the distance to the nearest enemy from the given spot.
 */
static float G_TechRangeFromSpot(const g_entity_t *spot) {
	float best_dist = FLT_MAX;
	_Bool any = false;

	for (g_tech_t i = TECH_HASTE; i < TECH_TOTAL; i++) {
		vec3_t v;
		g_entity_t *tech = G_TechEntity(i);

		if (!tech) {
			continue;
		}

		v = Vec3_Subtract(spot->s.origin, tech->s.origin);
		const float dist = Vec3_Length(v);

		if (dist < best_dist) {
			best_dist = dist;
		}

		any = true;
	}

	if (!any) {
		return Randomf() * MAX_WORLD_DIST;
	}

	return best_dist;
}

/**
 * @brief
 */
static void G_SelectFarthestTechSpawnPoint(const g_spawn_points_t *spawn_points, g_entity_t **point, float *point_dist) {

	for (size_t i = 0; i < spawn_points->count; i++) {
		g_entity_t *spot = spawn_points->spots[i];
		float dist = G_TechRangeFromSpot(spot);

		if (dist > *point_dist) {
			*point = spot;
			*point_dist = dist;
		}
	}
}

/**
 * @brief
 */
g_entity_t *G_SelectTechSpawnPoint(void) {
	float point_dist = -FLT_MAX;
	g_entity_t *point = NULL;

	if (g_level.teams || g_level.ctf) {
		for (int32_t i = 0; i < g_level.num_teams; i++) {
			G_SelectFarthestTechSpawnPoint(&g_teamlist[i].spawn_points, &point, &point_dist);
		}
	} else {
		G_SelectFarthestTechSpawnPoint(&g_level.spawn_points, &point, &point_dist);
	}

	if (!point) {
		G_SelectFarthestTechSpawnPoint(&g_level.spawn_points, &point, &point_dist);
	}

	return point;
}

/**
 * @brief
 */
void G_ResetDroppedTech(g_entity_t *ent) {

	G_SpawnTech(ent->locals.item);

	G_FreeEntity(ent);
}

/**
 * @brief Check if a player has the specified tech.
 */
_Bool G_HasTech(const g_entity_t *player, const g_tech_t tech) {
	return !!player->client->locals.inventory[g_media.items.techs[tech]->index];
}

/**
 * @brief Pickup function for techs. Can only hold one tech at a time.
 */
static _Bool G_PickupTech(g_entity_t *ent, g_entity_t *other) {

	for (g_tech_t tech = TECH_HASTE; tech < TECH_TOTAL; tech++) {

		if (G_HasTech(other, tech)) {
			return false;
		}
	}

	// add the weapon to inventory
	other->client->locals.inventory[ent->locals.item->index]++;

	return true;
}

/**
 * @brief
 */
const g_item_t *G_CarryingTech(const g_entity_t *ent) {

	for (g_tech_t i = TECH_HASTE; i < TECH_TOTAL; i++) {

		if (G_HasTech(ent, i)) {
			return g_media.items.techs[i];
		}
	}

	return NULL;
}

/**
 * @brief
 */
g_entity_t *G_TossTech(g_entity_t *ent) {
	const g_item_t *tech = G_CarryingTech(ent);

	if (!tech) {
		return NULL;
	}

	ent->client->locals.inventory[tech->index] = 0;

	return G_DropItem(ent, tech);
}

/**
 * @brief Drop the tech.
 */
static g_entity_t *G_DropTech(g_entity_t *ent, const g_item_t *item) {
	return G_TossTech(ent);
}

/**
 * @brief Reset the item's interaction state based on the current game state.
 */
void G_ResetItem(g_entity_t *ent) {

	ent->solid = SOLID_TRIGGER;
	ent->sv_flags &= ~SVF_NO_CLIENT;
	ent->locals.Touch = G_TouchItem;

	if (ent->locals.item->type == ITEM_FLAG) {
		if (g_level.ctf == false ||
			ent->locals.item->tag > g_level.num_teams) {
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

	const _Bool inhibited = (g_level.gameplay == GAME_ARENA || g_level.gameplay == GAME_INSTAGIB)
	                        && ent->locals.item->type != ITEM_FLAG;

	if (inhibited || (ent->locals.flags & FL_TEAM_SLAVE)) {
		ent->sv_flags |= SVF_NO_CLIENT;
		ent->solid = SOLID_NOT;
	}

	// if we were mid-respawn, get us out of it
	if (ent->locals.Think == G_ItemRespawn) {
		ent->locals.next_think = 0;
		ent->locals.Think = NULL;
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

	ent->locals.velocity = Vec3_Zero();
	dest = ent->s.origin;

	if (!(ent->locals.spawn_flags & SF_ITEM_HOVER)) {
		ent->locals.move_type = MOVE_TYPE_BOUNCE;
	} else {
		ent->locals.move_type = MOVE_TYPE_FLY;
	}

	tr = gi.Trace(ent->s.origin, dest, ent->mins, ent->maxs, ent, CONTENTS_MASK_SOLID);
	if (tr.start_solid) {
		// try thinner box
		gi.Debug("%s in too small of a spot for large box, correcting..\n", etos(ent));
		ent->maxs.z /= 2.0;

		tr = gi.Trace(ent->s.origin, dest, ent->mins, ent->maxs, ent, CONTENTS_MASK_SOLID);
		if (tr.start_solid) {

			gi.Debug("%s still can't fit, trying Q2 box..\n", etos(ent));

			ent->mins = Vec3_Add(ent->mins, Vec3(2.f, 2.f, 2.f));
			ent->maxs = Vec3_Add(ent->maxs, Vec3(-2.f, -2.f, -2.f));

			// try Quake 2 box
			tr = gi.Trace(ent->s.origin, dest, ent->mins, ent->maxs, ent, CONTENTS_MASK_SOLID);
			if (tr.start_solid) {

				gi.Debug("%s trying higher, last attempt..\n", etos(ent));

				ent->s.origin.z += 8.0;

				// make an effort to come up out of the floor (broken maps)
				tr = gi.Trace(ent->s.origin, ent->s.origin, ent->mins, ent->maxs, ent, CONTENTS_MASK_SOLID);
				if (tr.start_solid) {
					gi.Warn("%s start_solid\n", etos(ent));
					G_FreeEntity(ent);
					return;
				}
			}
		}
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
	ptrdiff_t len;

	if (!it) {
		return;
	}

	if (it->pickup_sound) {
		gi.SoundIndex(it->pickup_sound);
	}
	if (it->model) {
		gi.ModelIndex(it->model);
	}
	if (it->icon) {
		gi.ImageIndex(it->icon);
	}

	// parse everything for its ammo
	if (it->ammo) {
		const g_item_t *ammo = it->ammo_item;

		if (ammo != it) {
			G_PrecacheItem(ammo);
		}
	}

	// parse the space-separated precache string for other items
	s = it->precaches;
	if (!s || !s[0]) {
		return;
	}

	while (*s) {
		start = s;
		while (*s && *s != ' ') {
			s++;
		}

		len = s - start;
		if (len >= MAX_QPATH || len < 5) {
			gi.Error("%s has bad precache string\n", it->class_name);
		}
		memcpy(data, start, len);
		data[len] = '\0';
		if (*s) {
			s++;
		}

		// determine type based on extension
		if (g_str_has_suffix(data, ".md3") || g_str_has_suffix(data, ".obj")) {
			gi.ModelIndex(data);
		} else if (g_str_has_suffix(data, ".wav") || g_str_has_suffix(data, ".ogg")) {
			gi.SoundIndex(data);
		} else if (g_str_has_suffix(data, ".tga") || g_str_has_suffix(data, ".png")
		           || g_str_has_suffix(data, ".jpg") || g_str_has_suffix(data, ".pcx")) {
			gi.ImageIndex(data);
		} else {
			gi.Error("%s has unknown data type\n", it->class_name);
		}
	}
}

static void G_InitItem(g_item_t *item);

/**
 * @brief Sets the clipping size and plants the object on the floor.
 *
 * Items can't be immediately dropped to floor, because they might
 * be on an entity that hasn't spawned yet.
 */
void G_SpawnItem(g_entity_t *ent, const g_item_t *item) {

	ent->locals.item = item;
	G_PrecacheItem(ent->locals.item);

	ent->mins = Vec3_Scale(ITEM_MINS, ITEM_SCALE);
	ent->maxs = Vec3_Scale(ITEM_MAXS, ITEM_SCALE);

	if (ent->model) {
		ent->s.model1 = gi.ModelIndex(ent->model);
	} else {

		G_InitItem((g_item_t *) ent->locals.item);
		ent->s.model1 = ent->locals.item->model_index;
	}

	ent->s.effects = item->effects;

	// weapons override the health field to store their ammo count
	if (ent->locals.item->type == ITEM_WEAPON) {
		const g_item_t *ammo = ent->locals.item->ammo_item;
		if (ammo) {
			ent->locals.health = ammo->quantity;
		} else {
			ent->locals.health = 0;
		}
	}

	ent->locals.next_think = g_level.time + QUETOO_TICK_MILLIS * 2;
	ent->locals.Think = G_ItemDropToFloor;
}

/**
 * @brief This is the magical list of items that the game runs by. All structs created here
 * should use the syntax provided below (order not necessary) so that it's easy to read and
 * protects against future struct changes. Note that there are a few private, auto-generated
 * members at the bottom of g_item_t that you shouldn't initialize here, since they'll be
 * overwritten by G_InitItems.
 */
static g_item_t g_items[] = {
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
		.class_name = "item_armor_body",
		.Pickup = G_PickupArmor,
		.Use = NULL,
		.Drop = NULL,
		.Think = NULL,
		.pickup_sound = "armor/body/pickup.wav",
		.model = "models/armor/body/tris.md3",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/i_bodyarmor",
		.name = "Body Armor",
		.quantity = 100,
		.max = 200,
		.ammo = NULL,
		.type = ITEM_ARMOR,
		.tag = ARMOR_BODY,
		.priority = 0.80,
		.precaches = ""
	},

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
		.class_name = "item_armor_combat",
		.Pickup = G_PickupArmor,
		.Use = NULL,
		.Drop = NULL,
		.Think = NULL,
		.pickup_sound = "armor/combat/pickup.wav",
		.model = "models/armor/combat/tris.md3",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/i_combatarmor",
		.name = "Combat Armor",
		.quantity = 50,
		.max = 100,
		.ammo = NULL,
		.type = ITEM_ARMOR,
		.tag = ARMOR_COMBAT,
		.priority = 0.66,
		.precaches = ""
	},

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
		.class_name = "item_armor_jacket",
		.Pickup = G_PickupArmor,
		.Use = NULL,
		.Drop = NULL,
		.Think = NULL,
		.pickup_sound = "armor/jacket/pickup.wav",
		.model = "models/armor/jacket/tris.md3",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/i_jacketarmor",
		.name = "Jacket Armor",
		.quantity = 25,
		.max = 50,
		.ammo = NULL,
		.type = ITEM_ARMOR,
		.tag = ARMOR_JACKET,
		.priority = 0.50,
		.precaches = ""
	},

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
		.class_name = "item_armor_shard",
		.Pickup = G_PickupArmor,
		.Use = NULL,
		.Drop = NULL,
		.Think = NULL,
		.pickup_sound = "armor/shard/pickup.wav",
		.model = "models/armor/shard/tris.md3",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/i_shard",
		.name = "Armor Shard",
		.quantity = 3,
		.ammo = NULL,
		.type = ITEM_ARMOR,
		.tag = ARMOR_SHARD,
		.priority = 0.10,
		.precaches = ""
	},

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
		.class_name = "weapon_blaster",
		.Pickup = G_PickupWeapon,
		.Use = G_UseWeapon,
		.Drop = NULL,
		.Think = G_FireBlaster,
		.pickup_sound = "weapons/common/pickup.wav",
		.model = "models/weapons/blaster/tris.md3",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/w_blaster",
		.name = "Blaster",
		.quantity = 0,
		.ammo = NULL,
		.type = ITEM_WEAPON,
		.tag = WEAPON_BLASTER,
		.flags = WF_PROJECTILE,
		.priority = 0.10,
		.precaches = "weapons/blaster/fire.wav"
	},

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
		.class_name = "weapon_shotgun",
		.Pickup = G_PickupWeapon,
		.Use = G_UseWeapon,
		.Drop = G_DropWeapon,
		.Think = G_FireShotgun,
		.pickup_sound = "weapons/common/pickup.wav",
		.model = "models/weapons/shotgun/tris.obj",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/w_shotgun",
		.name = "Shotgun",
		.quantity = 1,
		.ammo = "Shells",
		.type = ITEM_WEAPON,
		.tag = WEAPON_SHOTGUN,
		.flags = WF_HITSCAN | WF_SHORT_RANGE | WF_MED_RANGE,
		.priority = 0.15,
		.precaches = "weapons/shotgun/fire.wav"
	},

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
		.class_name = "weapon_supershotgun",
		.Pickup = G_PickupWeapon,
		.Use = G_UseWeapon,
		.Drop = G_DropWeapon,
		.Think = G_FireSuperShotgun,
		.pickup_sound = "weapons/common/pickup.wav",
		.model = "models/weapons/supershotgun/tris.obj",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/w_sshotgun",
		.name = "Super Shotgun",
		.quantity = 2,
		.ammo = "Shells",
		.type = ITEM_WEAPON,
		.tag = WEAPON_SUPER_SHOTGUN,
		.flags = WF_HITSCAN | WF_SHORT_RANGE,
		.priority = 0.25,
		.precaches = "weapons/supershotgun/fire.wav"
	},

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
		.class_name = "weapon_machinegun",
		.Pickup = G_PickupWeapon,
		.Use = G_UseWeapon,
		.Drop = G_DropWeapon,
		.Think = G_FireMachinegun,
		.pickup_sound = "weapons/common/pickup.wav",
		.model = "models/weapons/machinegun/tris.obj",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/w_machinegun",
		.name = "Machinegun",
		.quantity = 1,
		.ammo = "Bullets",
		.type = ITEM_WEAPON,
		.tag = WEAPON_MACHINEGUN,
		.flags = WF_HITSCAN | WF_SHORT_RANGE | WF_MED_RANGE,
		.priority = 0.30,
		.precaches = "weapons/machinegun/fire_1.wav weapons/machinegun/fire_2.wav "
		"weapons/machinegun/fire_3.wav weapons/machinegun/fire_4.wav"
	},

	/*QUAKED weapon_handgrenades (.2 .8 .2) (-16 -16 -16) (16 16 16) triggered no_touch hover
	 Hand Grenades.

	 -------- Keys --------
	 team : The team name for alternating item spawns.

	 -------- Spawn flags --------
	 triggered : Item will not appear until triggered.
	 no_touch : Item will interact as solid instead of being picked up by player.
	 hover : Item will spawn where it was placed in the map and won't drop the floor.

	 -------- Radiant config --------
	 model="models/objects/grenade/tris.md3"
	 */
	{
		.class_name = "weapon_handgrenades",
		.Pickup = G_PickupWeapon,
		.Use = G_UseWeapon,
		.Drop = NULL,
		.Think = G_FireHandGrenade,
		.pickup_sound = "weapons/common/pickup.wav",
		.model = "models/objects/grenade/tris.md3",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/a_handgrenades",
		.name = "Hand Grenades",
		.quantity = 1,
		.ammo = "Grenades",
		.type = ITEM_WEAPON,
		.tag = WEAPON_HAND_GRENADE,
		.flags = WF_PROJECTILE | WF_EXPLOSIVE | WF_TIMED | WF_MED_RANGE,
		.priority = 0.30,
		.precaches = "weapons/handgrenades/hg_throw.wav weapons/handgrenades/hg_clang.ogg "
		"weapons/handgrenades/hg_tick.ogg"
	},

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
		.class_name = "weapon_grenadelauncher",
		.Pickup = G_PickupGrenadeLauncher,
		.Use = G_UseWeapon,
		.Drop = G_DropWeapon,
		.Think = G_FireGrenadeLauncher,
		.pickup_sound = "weapons/common/pickup.wav",
		.model = "models/weapons/grenadelauncher/tris.obj",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/w_glauncher",
		.name = "Grenade Launcher",
		.quantity = 1,
		.ammo = "Grenades",
		.type = ITEM_WEAPON,
		.tag = WEAPON_GRENADE_LAUNCHER,
		.flags = WF_PROJECTILE | WF_EXPLOSIVE,
		.priority = 0.40,
		.precaches = "models/objects/grenade/tris.md3 weapons/grenadelauncher/fire.wav"
	},

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
		.class_name = "weapon_rocketlauncher",
		.Pickup = G_PickupWeapon,
		.Use = G_UseWeapon,
		.Drop = G_DropWeapon,
		.Think = G_FireRocketLauncher,
		.pickup_sound = "weapons/common/pickup.wav",
		.model = "models/weapons/rocketlauncher/tris.md3",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/w_rlauncher",
		.name = "Rocket Launcher",
		.quantity = 1,
		.ammo = "Rockets",
		.type = ITEM_WEAPON,
		.tag = WEAPON_ROCKET_LAUNCHER,
		.flags = WF_PROJECTILE | WF_EXPLOSIVE | WF_MED_RANGE | WF_LONG_RANGE,
		.priority = 0.50,
		.precaches = "models/objects/rocket/tris.obj objects/rocket/fly.wav "
		"weapons/rocketlauncher/fire.wav"
	},

	/*QUAKED weapon_hyperblaster (.4 .7 1) (-16 -16 -16) (16 16 16) triggered no_touch hover
	 Hyperblaster.

	 -------- Keys --------
	 team : The team name for alternating item spawns.

	 -------- Spawn flags --------
	 triggered : Item will not appear until triggered.
	 no_touch : Item will interact as solid instead of being picked up by player.
	 hover : Item will spawn where it was placed in the map and won't drop the floor.

	 -------- Radiant config --------
	 model="models/weapons/hyperblaster/tris.obj"
	 */
	{
		.class_name = "weapon_hyperblaster",
		.Pickup = G_PickupWeapon,
		.Use = G_UseWeapon,
		.Drop = G_DropWeapon,
		.Think = G_FireHyperblaster,
		.pickup_sound = "weapons/common/pickup.wav",
		.model = "models/weapons/hyperblaster/tris.obj",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/w_hyperblaster",
		.name = "Hyperblaster",
		.quantity = 1,
		.ammo = "Cells",
		.type = ITEM_WEAPON,
		.tag = WEAPON_HYPERBLASTER,
		.flags = WF_PROJECTILE | WF_MED_RANGE,
		.priority = 0.50,
		.precaches = "weapons/hyperblaster/fire.wav weapons/hyperblaster/hit.wav"
	},

	/*QUAKED weapon_lightning (.9 .9 .9) (-16 -16 -16) (16 16 16) triggered no_touch hover
	 Lightning.

	 -------- Keys --------
	 team : The team name for alternating item spawns.

	 -------- Spawn flags --------
	 triggered : Item will not appear until triggered.
	 no_touch : Item will interact as solid instead of being picked up by player.
	 hover : Item will spawn where it was placed in the map and won't drop the floor.

	 -------- Radiant config --------
	 model="models/weapons/lightning/tris.obj"
	 */
	{
		.class_name = "weapon_lightning",
		.Pickup = G_PickupWeapon,
		.Use = G_UseWeapon,
		.Drop = G_DropWeapon,
		.Think = G_FireLightning,
		.pickup_sound = "weapons/common/pickup.wav",
		.model = "models/weapons/lightning/tris.obj",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/w_lightning",
		.name = "Lightning Gun",
		.quantity = 1,
		.ammo = "Bolts",
		.type = ITEM_WEAPON,
		.tag = WEAPON_LIGHTNING,
		.flags = WF_HITSCAN | WF_SHORT_RANGE,
		.priority = 0.50,
		.precaches = "weapons/lightning/fire.wav weapons/lightning/fly.wav "
		"weapons/lightning/discharge.wav"
	},

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
		.class_name = "weapon_railgun",
		.Pickup = G_PickupWeapon,
		.Use = G_UseWeapon,
		.Drop = G_DropWeapon,
		.Think = G_FireRailgun,
		.pickup_sound = "weapons/common/pickup.wav",
		.model = "models/weapons/railgun/tris.obj",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/w_railgun",
		.name = "Railgun",
		.quantity = 1,
		.ammo = "Slugs",
		.type = ITEM_WEAPON,
		.tag = WEAPON_RAILGUN,
		.flags = WF_HITSCAN | WF_LONG_RANGE,
		.priority = 0.60,
		.precaches = "weapons/railgun/fire.wav"
	},

	/*QUAKED weapon_bfg (.4 1 .5) (-16 -16 -16) (16 16 16) triggered no_touch hover
	 BFG10K.

	 -------- Keys --------
	 team : The team name for alternating item spawns.

	 -------- Spawn flags --------
	 triggered : Item will not appear until triggered.
	 no_touch : Item will interact as solid instead of being picked up by player.
	 hover : Item will spawn where it was placed in the map and won't drop the floor.

	 -------- Radiant config --------
	 model="models/weapons/bfg/tris.obj"
	 */
	{
		.class_name = "weapon_bfg",
		.Pickup = G_PickupWeapon,
		.Use = G_UseWeapon,
		.Drop = G_DropWeapon,
		.Think = G_FireBfg,
		.pickup_sound = "weapons/common/pickup.wav",
		.model = "models/weapons/bfg/tris.obj",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/w_bfg",
		.name = "BFG10K",
		.quantity = 1,
		.ammo = "Nukes",
		.type = ITEM_WEAPON,
		.tag = WEAPON_BFG10K,
		.flags = WF_PROJECTILE | WF_MED_RANGE | WF_LONG_RANGE,
		.priority = 0.66,
		.precaches = "weapons/bfg/prime.wav weapons/bfg/hit.wav"
	},

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
		.class_name = "ammo_shells",
		.Pickup = G_PickupAmmo,
		.Use = NULL,
		.Drop = G_DropItem,
		.Think = NULL,
		.pickup_sound = "ammo/common/pickup.wav",
		.model = "models/ammo/shells/tris.md3",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/a_shells",
		.name = "Shells",
		.quantity = 10,
		.max = 80,
		.ammo = NULL,
		.type = ITEM_AMMO,
		.tag = AMMO_SHELLS,
		.priority = 0.15,
		.precaches = ""
	},

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
		.class_name = "ammo_bullets",
		.Pickup = G_PickupAmmo,
		.Use = NULL,
		.Drop = G_DropItem,
		.Think = NULL,
		.pickup_sound = "ammo/common/pickup.wav",
		.model = "models/ammo/bullets/tris.md3",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/a_bullets",
		.name = "Bullets",
		.quantity = 50,
		.max = 200,
		.ammo = NULL,
		.type = ITEM_AMMO,
		.tag = AMMO_BULLETS,
		.priority = 0.15,
		.precaches = ""
	},

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
		.class_name = "ammo_grenades",
		.Pickup = G_PickupGrenades,
		.Use = NULL,
		.Drop = G_DropItem,
		.Think = NULL,
		.pickup_sound = "ammo/common/pickup.wav",
		.model = "models/ammo/grenades/tris.md3",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/a_handgrenades",
		.name = "Grenades",
		.quantity = 10,
		.max = 50,
		.ammo = "grenades",
		.type = ITEM_AMMO,
		.tag = AMMO_GRENADES,
		.priority = 0.15,
		.precaches = "weapons/handgrenades/hg_throw.wav weapons/handgrenades/hg_clang.ogg "
		"weapons/handgrenades/hg_tick.ogg"
	},

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
		.class_name = "ammo_rockets",
		.Pickup = G_PickupAmmo,
		.Use = NULL,
		.Drop = G_DropItem,
		.Think = NULL,
		.pickup_sound = "ammo/common/pickup.wav",
		.model = "models/ammo/rockets/tris.md3",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/a_rockets",
		.name = "Rockets",
		.quantity = 10,
		.max = 50,
		.ammo = NULL,
		.type = ITEM_AMMO,
		.tag = AMMO_ROCKETS,
		.priority = 0.15,
		.precaches = ""
	},

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
		.class_name = "ammo_cells",
		.Pickup = G_PickupAmmo,
		.Use = NULL,
		.Drop = G_DropItem,
		.Think = NULL,
		.pickup_sound = "ammo/common/pickup.wav",
		.model = "models/ammo/cells/tris.md3",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/a_cells",
		.name = "Cells",
		.quantity = 50,
		.max = 200,
		.ammo = NULL,
		.type = ITEM_AMMO,
		.tag = AMMO_CELLS,
		.priority = 0.15,
		.precaches = ""
	},

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
		.class_name = "ammo_bolts",
		.Pickup = G_PickupAmmo,
		.Use = NULL,
		.Drop = G_DropItem,
		.Think = NULL,
		.pickup_sound = "ammo/common/pickup.wav",
		.model = "models/ammo/bolts/tris.md3",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/a_bolts",
		.name = "Bolts",
		.quantity = 25,
		.max = 150,
		.ammo = NULL,
		.type = ITEM_AMMO,
		.tag = AMMO_BOLTS,
		.priority = 0.15,
		.precaches = ""
	},

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
		.class_name = "ammo_slugs",
		.Pickup = G_PickupAmmo,
		.Use = NULL,
		.Drop = G_DropItem,
		.Think = NULL,
		.pickup_sound = "ammo/common/pickup.wav",
		.model = "models/ammo/slugs/tris.md3",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/a_slugs",
		.name = "Slugs",
		.quantity = 10,
		.max = 50,
		.ammo = NULL,
		.type = ITEM_AMMO,
		.tag = AMMO_SLUGS,
		.priority = 0.15,
		.precaches = ""
	},

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
		.class_name = "ammo_nukes",
		.Pickup = G_PickupAmmo,
		.Use = NULL,
		.Drop = G_DropItem,
		.Think = NULL,
		.pickup_sound = "ammo/common/pickup.wav",
		.model = "models/ammo/nukes/tris.md3",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/a_nukes",
		.name = "Nukes",
		.quantity = 2,
		.max = 10,
		.ammo = NULL,
		.type = ITEM_AMMO,
		.tag = AMMO_NUKES,
		.priority = 0.15,
		.precaches = ""
	},

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
		.class_name = "item_adrenaline",
		.Pickup = G_PickupAdrenaline,
		.Use = NULL,
		.Drop = NULL,
		.Think = NULL,
		.pickup_sound = "adren/pickup.wav",
		.model = "models/powerups/adren/tris.obj",
		.effects = EF_ROTATE,
		.icon = "pics/p_adrenaline",
		.name = "Adrenaline",
		.quantity = 0,
		.ammo = NULL,
		.type = ITEM_POWERUP,
		.tag = POWERUP_ADRENALINE,
		.priority = 0.45,
		.precaches = ""
	},

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
		.class_name = "item_health_small",
		.Pickup = G_PickupHealth,
		.Use = NULL,
		.Drop = NULL,
		.Think = NULL,
		.pickup_sound = "health/small/pickup.wav",
		.model = "models/health/small/tris.obj",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/i_small_health",
		.name = "Small Health",
		.quantity = 3,
		.ammo = NULL,
		.type = ITEM_HEALTH,
		.tag = HEALTH_SMALL,
		.priority = 0.10,
		.precaches = ""
	},

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
		.class_name = "item_health",
		.Pickup = G_PickupHealth,
		.Use = NULL,
		.Drop = NULL,
		.Think = NULL,
		.pickup_sound = "health/medium/pickup.wav",
		.model = "models/health/medium/tris.obj",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/i_medium_health",
		.name = "Medium Health",
		.quantity = 15,
		.ammo = NULL,
		.type = ITEM_HEALTH,
		.tag = HEALTH_MEDIUM,
		.priority = 0.25,
		.precaches = ""
	},

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
		.class_name = "item_health_large",
		.Pickup = G_PickupHealth,
		.Use = NULL,
		.Drop = NULL,
		.Think = NULL,
		.pickup_sound = "health/large/pickup.wav",
		.model = "models/health/large/tris.obj",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/i_large_health",
		.name = "Large Health",
		.quantity = 25,
		.ammo = NULL,
		.type = ITEM_HEALTH,
		.tag = HEALTH_LARGE,
		.priority = 0.40,
		.precaches = ""
	},

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
		.class_name = "item_health_mega",
		.Pickup = G_PickupHealth,
		.Use = NULL,
		.Drop = NULL,
		.Think = NULL,
		.pickup_sound = "health/mega/pickup.wav",
		.model = "models/health/mega/tris.obj",
		.effects = EF_ROTATE | EF_BOB,
		.icon = "pics/i_mega_health",
		.name = "Mega Health",
		.quantity = 75,
		.ammo = NULL,
		.type = ITEM_HEALTH,
		.tag = HEALTH_MEGA,
		.priority = 0.60,
		.precaches = ""
	},

	/*QUAKED item_flag_team1 (.2 .2 1) (-16 -16 -24) (16 16 32) triggered no_touch hover

	 -------- Spawn flags --------
	 triggered : Item will not appear until triggered.
	 no_touch : Item will interact as solid instead of being picked up by player.
	 hover : Item will spawn where it was placed in the map and won't drop the floor.

	 -------- Radiant config --------
	 model="models/ctf/flag1/tris.obj"
	 */
	{
		.class_name = "item_flag_team1",
		.Pickup = G_PickupFlag,
		.Use = NULL,
		.Drop = G_DropFlag,
		.Think = NULL,
		.pickup_sound = NULL,
		.model = "models/ctf/flag1/tris.obj",
		.effects = EF_BOB | EF_ROTATE,
		.icon = "pics/i_flag1",
		.name = "Enemy Flag",
		.quantity = 0,
		.ammo = NULL,
		.type = ITEM_FLAG,
		.tag = TEAM_RED,
		.priority = 0.75,
		.precaches = "ctf/capture.wav ctf/steal.wav ctf/return.wav"
	},

	/*QUAKED item_flag_team2 (1 .2 .2) (-16 -16 -24) (16 16 32) triggered no_touch hover

	 -------- Spawn flags --------
	 triggered : Item will not appear until triggered.
	 no_touch : Item will interact as solid instead of being picked up by player.
	 hover : Item will spawn where it was placed in the map and won't drop the floor.

	 -------- Radiant config --------
	 model="models/ctf/flag2/tris.obj"
	 */
	{
		.class_name = "item_flag_team2",
		.Pickup = G_PickupFlag,
		.Use = NULL,
		.Drop = G_DropFlag,
		.Think = NULL,
		.pickup_sound = NULL,
		.model = "models/ctf/flag2/tris.obj",
		.effects = EF_BOB | EF_ROTATE,
		.icon = "pics/i_flag2",
		.name = "Enemy Flag",
		.quantity = 0,
		.ammo = NULL,
		.type = ITEM_FLAG,
		.tag = TEAM_BLUE,
		.priority = 0.75,
		.precaches = "ctf/capture.wav ctf/steal.wav ctf/return.wav"
	},

	/*QUAKED item_flag_team3 (1 .2 .2) (-16 -16 -24) (16 16 32) triggered no_touch hover

	 -------- Spawn flags --------
	 triggered : Item will not appear until triggered.
	 no_touch : Item will interact as solid instead of being picked up by player.
	 hover : Item will spawn where it was placed in the map and won't drop the floor.

	 -------- Radiant config --------
	 model="models/ctf/flag3/tris.obj"
	 */
	{
		.class_name = "item_flag_team3",
		.Pickup = G_PickupFlag,
		.Use = NULL,
		.Drop = G_DropFlag,
		.Think = NULL,
		.pickup_sound = NULL,
		.model = "models/ctf/flag3/tris.obj",
		.effects = EF_BOB | EF_ROTATE,
		.icon = "pics/i_flag3",
		.name = "Enemy Flag",
		.quantity = 0,
		.ammo = NULL,
		.type = ITEM_FLAG,
		.tag = TEAM_GREEN,
		.priority = 0.75,
		.precaches = "ctf/capture.wav ctf/steal.wav ctf/return.wav"
	},

	/*QUAKED item_flag_team4 (1 .2 .2) (-16 -16 -24) (16 16 32) triggered no_touch hover

	 -------- Spawn flags --------
	 triggered : Item will not appear until triggered.
	 no_touch : Item will interact as solid instead of being picked up by player.
	 hover : Item will spawn where it was placed in the map and won't drop the floor.

	 -------- Radiant config --------
	 model="models/ctf/flag4/tris.obj"
	 */
	{
		.class_name = "item_flag_team4",
		.Pickup = G_PickupFlag,
		.Use = NULL,
		.Drop = G_DropFlag,
		.Think = NULL,
		.pickup_sound = NULL,
		.model = "models/ctf/flag4/tris.obj",
		.effects = EF_BOB | EF_ROTATE,
		.icon = "pics/i_flag4",
		.name = "Enemy Flag",
		.quantity = 0,
		.ammo = NULL,
		.type = ITEM_FLAG,
		.tag = TEAM_ORANGE,
		.priority = 0.75,
		.precaches = "ctf/capture.wav ctf/steal.wav ctf/return.wav"
	},

	/*QUAKED item_quad (.2 .4 1) (-16 -16 -16) (16 16 16) triggered no_touch hover
	 Quad damage.

	 -------- Keys --------
	 team : The team name for alternating item spawns.

	 -------- Spawn flags --------
	 triggered : Item will not appear until triggered.
	 no_touch : Item will interact as solid instead of being picked up by player.
	 hover : Item will spawn where it was placed in the map and won't drop the floor.

	 -------- Radiant config --------
	 model="models/powerups/quad/tris.obj"
	 */
	{
		.class_name = "item_quad",
		.Pickup = G_PickupQuadDamage,
		.Use = NULL,
		.Drop = NULL,
		.Think = NULL,
		.pickup_sound = "quad/pickup.wav",
		.model = "models/powerups/quad/tris.obj",
		.effects = EF_BOB | EF_ROTATE,
		.icon = "pics/i_quad",
		.name = "Quad Damage",
		.quantity = 0,
		.ammo = NULL,
		.type = ITEM_POWERUP,
		.tag = POWERUP_QUAD,
		.priority = 0.80,
		.precaches = "quad/attack.wav quad/expire.wav"
	},

	{
		.class_name = "item_tech_haste",
		.Pickup = G_PickupTech,
		.Use = NULL,
		.Drop = G_DropTech,
		.Think = NULL,
		.pickup_sound = "tech/pickup.wav",
		.model = "models/techs/haste/tris.obj",
		.effects = EF_BOB | EF_ROTATE,
		.icon = "pics/t_haste",
		.name = "Haste",
		.quantity = 0,
		.ammo = NULL,
		.type = ITEM_TECH,
		.tag = TECH_HASTE,
		.priority = 0.80,
		.precaches = ""
	},

	{
		.class_name = "item_tech_regen",
		.Pickup = G_PickupTech,
		.Use = NULL,
		.Drop = G_DropTech,
		.Think = NULL,
		.pickup_sound = "tech/pickup.wav",
		.model = "models/techs/regen/tris.obj",
		.effects = EF_BOB | EF_ROTATE,
		.icon = "pics/t_regen",
		.name = "Regeneration",
		.quantity = 0,
		.ammo = NULL,
		.type = ITEM_TECH,
		.tag = TECH_REGEN,
		.priority = 0.80,
		.precaches = ""
	},

	{
		.class_name = "item_tech_resist",
		.Pickup = G_PickupTech,
		.Use = NULL,
		.Drop = G_DropTech,
		.Think = NULL,
		.pickup_sound = "tech/pickup.wav",
		.model = "models/techs/resist/tris.obj",
		.effects = EF_BOB | EF_ROTATE,
		.icon = "pics/t_resist",
		.name = "Resist",
		.quantity = 0,
		.ammo = NULL,
		.type = ITEM_TECH,
		.tag = TECH_RESIST,
		.priority = 0.80,
		.precaches = ""
	},

	{
		.class_name = "item_tech_strength",
		.Pickup = G_PickupTech,
		.Use = NULL,
		.Drop = G_DropTech,
		.Think = NULL,
		.pickup_sound = "tech/pickup.wav",
		.model = "models/techs/strength/tris.obj",
		.effects = EF_BOB | EF_ROTATE,
		.icon = "pics/t_strength",
		.name = "Strength",
		.quantity = 0,
		.ammo = NULL,
		.type = ITEM_TECH,
		.tag = TECH_STRENGTH,
		.priority = 0.80,
		.precaches = ""
	},

	{
		.class_name = "item_tech_vampire",
		.Pickup = G_PickupTech,
		.Use = NULL,
		.Drop = G_DropTech,
		.Think = NULL,
		.pickup_sound = "tech/pickup.wav",
		.model = "models/techs/vampire/tris.obj",
		.effects = EF_BOB | EF_ROTATE,
		.icon = "pics/t_vampire",
		.name = "Vampire",
		.quantity = 0,
		.ammo = NULL,
		.type = ITEM_TECH,
		.tag = TECH_VAMPIRE,
		.priority = 0.80,
		.precaches = ""
	},
};

/**
 * @brief The total number of items in the item list.
 */
const uint16_t g_num_items = lengthof(g_items);

/**
 * @brief Fetch the item list.
 */
const g_item_t *G_ItemList(void) {

	return g_items;
}

/**
 * @brief Fetch an item by index.
 */
const g_item_t *G_ItemByIndex(uint16_t index) {

	if (index >= g_num_items) {
		return NULL;
	}

	return &g_items[index];
}

/**
 * @brief Called to set up the special string for weapons usable by the player.
 */
static void G_InitWeapons(void) {
	char weapon_info[MAX_STRING_CHARS] = { '\0' };

	for (int32_t t = WEAPON_NONE + 1; t < WEAPON_TOTAL; t++) {

		if (t != WEAPON_NONE + 1) {
			strcat(weapon_info, "\\");
		}

		const g_item_t *weapon = g_media.items.weapons[t];

		strcat(weapon_info, va("%i\\%i", weapon->icon_index, weapon->index));
	}

	gi.SetConfigString(CS_WEAPONS, weapon_info);
}

/**
 * @brief Called to set up a specific item in the item list. This might be called
 * during item spawning (since bmodels have to spawn before any modelindex calls
 * can safely be made) but will be called for every other item once that is done.
 */
static void G_InitItem(g_item_t *item) {

	item->index = (uint16_t) (ptrdiff_t) (item - g_items);

	if (item->ammo) {
		item->ammo_item = G_FindItem(item->ammo);

		if (!item->ammo_item) {
			gi.Error("Invalid ammo specified for weapon\n");
		}
	}

	item->icon_index = gi.ImageIndex(item->icon);
	item->model_index = gi.ModelIndex(item->model);
	item->pickup_sound_index = gi.SoundIndex(item->pickup_sound);

	gi.SetConfigString(CS_ITEMS + item->index, item->name);
}

/**
 * @brief Called to setup special private data for the item list.
 */
void G_InitItems(void) {

	for (uint16_t i = 0; i < g_num_items; i++) {

		g_item_t *item = &g_items[i];
		G_InitItem(item);

		// set up media pointers
		const g_item_t **array = NULL;

		switch (item->type) {
		default:
			gi.Error("Item %s has an invalid type\n", item->name);
		case ITEM_AMMO:
			array = g_media.items.ammo;
			break;
		case ITEM_ARMOR:
			array = g_media.items.armor;
			break;
		case ITEM_FLAG:
			array = g_media.items.flags;
			break;
		case ITEM_HEALTH:
			array = g_media.items.health;
			break;
		case ITEM_POWERUP:
			array = g_media.items.powerups;
			break;
		case ITEM_WEAPON:
			array = g_media.items.weapons;
			break;
		case ITEM_TECH:
			array = g_media.items.techs;
			break;
		}

		if (array[item->tag]) {
			gi.Error("Item %s has the same tag as %s\n", item->name, array[item->tag]->name);
		}

		array[item->tag] = item;

		// precache all weapons/health/armor, even if the map doesn't contain them
		if (item->type == ITEM_WEAPON ||
			item->type == ITEM_HEALTH ||
			item->type == ITEM_ARMOR) {
			G_PrecacheItem(item);
		}
	}

	G_InitWeapons();
}
