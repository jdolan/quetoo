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
 * @brief Make a tasteless death announcement.
 */
static void G_ClientObituary(g_entity_t *self, g_entity_t *attacker, uint32_t mod) {

	const _Bool friendy_fire = (mod & MOD_FRIENDLY_FIRE) == MOD_FRIENDLY_FIRE;
	mod &= ~MOD_FRIENDLY_FIRE;

	if (!g_level.warmup) { // insert to db
		G_MySQL_ClientObituary(self, attacker, mod);
	}

	char *message = NULL;
	char *message2 = "";

	switch (mod) {
		case MOD_SUICIDE:
			message = "suicides";
			break;
		case MOD_FALLING:
			message = "cratered";
			break;
		case MOD_CRUSH:
			message = "was squished";
			break;
		case MOD_WATER:
			message = "sleeps with the fishes";
			break;
		case MOD_SLIME:
			message = "melted";
			break;
		case MOD_LAVA:
			message = "did a back flip into the lava";
			break;
		case MOD_FIREBALL:
			message = "tasted the lava rainbow";
			break;
		case MOD_TRIGGER_HURT:
			message = "was in the wrong place";
			break;
	}

	if (attacker == self) {
		switch (mod) {
			case MOD_GRENADE_SPLASH:
				message = "went pop";
				break;
			case MOD_HANDGRENADE_KAMIKAZE:
				message = "tried to put the pin back in";
				break;
			case MOD_HANDGRENADE_SPLASH:
				message = "has no hair left";
				break;
			case MOD_ROCKET_SPLASH:
				message = "blew up";
				break;
			case MOD_LIGHTNING_DISCHARGE:
				message = "took a toaster bath";
				break;
			case MOD_BFG_BLAST:
				message = "should have used a smaller gun";
				break;
			default:
				message = "sucks at life";
				break;
		}
	}

	if (message) { // suicide
		gi.BroadcastPrint(PRINT_HIGH, "%s %s.\n", self->client->locals.persistent.net_name, message);

		if (g_level.warmup)
			return;

		self->client->locals.persistent.score--;

		if ((g_level.teams || g_level.ctf) && self->client->locals.persistent.team)
			self->client->locals.persistent.team->score--;

		return;
	}

	if (attacker && attacker->client) {
		switch (mod) {
			case MOD_BLASTER:
				message = "was humiliated by";
				message2 = "'s blaster";
				break;
			case MOD_SHOTGUN:
				message = "was gunned down by";
				message2 = "'s shotgun";
				break;
			case MOD_SUPER_SHOTGUN:
				message = "was blown away by";
				message2 = "'s super shotgun";
				break;
			case MOD_MACHINEGUN:
				message = "was perforated by";
				message2 = "'s machinegun";
				break;
			case MOD_GRENADE:
				message = "was popped by";
				message2 = "'s grenade";
				break;
			case MOD_GRENADE_SPLASH:
				message = "was shredded by";
				message2 = "'s shrapnel";
				break;
			case MOD_HANDGRENADE_HIT:
				message = "caught";
				message2 = "'s handgrenade";
				break;
			case MOD_HANDGRENADE_SPLASH:
				message = "felt the burn from";
				message2 = "'s handgrenade";
				break;
			case MOD_HANDGRENADE_KAMIKAZE:
				message = "felt";
				message2 = "'s pain";
				break;
			case MOD_ROCKET:
				message = "ate";
				message2 = "'s rocket";
				break;
			case MOD_ROCKET_SPLASH:
				message = "almost dodged";
				message2 = "'s rocket";
				break;
			case MOD_HYPERBLASTER:
				message = "was melted by";
				message2 = "'s hyperblaster";
				break;
			case MOD_LIGHTNING:
				message = "got a charge out of";
				message2 = "'s lightning";
				break;
			case MOD_LIGHTNING_DISCHARGE:
				message = "was shocked by";
				message2 = "'s discharge";
				break;
			case MOD_RAILGUN:
				message = "was railed by";
				break;
			case MOD_BFG_LASER:
				message = "saw the pretty lights from";
				message2 = "'s BFG";
				break;
			case MOD_BFG_BLAST:
				message = "was disintegrated by";
				message2 = "'s BFG blast";
				break;
			case MOD_TELEFRAG:
				message = "tried to invade";
				message2 = "'s personal space";
				break;
		}

		if (message) {

			gi.BroadcastPrint(PRINT_HIGH, "%s%s %s %s%s\n", (friendy_fire ? "^1TEAMKILL^7 " : ""),
					self->client->locals.persistent.net_name, message,
					attacker->client->locals.persistent.net_name, message2);

			if (g_show_attacker_stats->integer) {
				int16_t a = 0;
				const g_item_t *armor = G_ClientArmor(attacker);
				if (armor) {
					a = attacker->client->locals.inventory[ITEM_INDEX(armor)];
				}
				gi.ClientPrint(self, PRINT_MEDIUM, "%s had %d health and %d armor\n",
						attacker->client->locals.persistent.net_name, attacker->locals.health, a);
			}

			if (g_level.warmup)
				return;

			if (friendy_fire)
				attacker->client->locals.persistent.score--;
			else
				attacker->client->locals.persistent.score++;

			if ((g_level.teams || g_level.ctf) && attacker->client->locals.persistent.team) { // handle team scores too
				if (friendy_fire)
					attacker->client->locals.persistent.team->score--;
				else
					attacker->client->locals.persistent.team->score++;
			}
		}
	}
}

/**
 * @brief Play a sloppy sound when impacting the world.
 */
static void G_ClientGiblet_Touch(g_entity_t *self, g_entity_t *other,
		const cm_bsp_plane_t *plane __attribute__((unused)), const cm_bsp_surface_t *surf) {

	if (surf && (surf->flags & SURF_SKY)) {
		G_FreeEntity(self);
	} else {
		const vec_t speed = VectorLength(self->locals.velocity);
		if (speed > 40.0 && G_IsStructural(other, surf)) {

			if (g_level.time - self->locals.touch_time > 200) {
				gi.Sound(self, self->locals.noise_index, ATTEN_IDLE);
				self->locals.touch_time = g_level.time;
			}
		}
	}
}

/**
 * @brief Sink into the floor after a few seconds, providing a window of time for us to be made into
 * giblets or knocked around. This is called by corpses and giblets alike.
 */
static void G_ClientCorpse_Think(g_entity_t *self) {

	const uint32_t age = g_level.time - self->locals.timestamp;

	if (self->s.model1 == MODEL_CLIENT) {
		if (age > 6000) {
			const int16_t dmg = self->locals.health;

			if (self->locals.water_type & CONTENTS_LAVA) {
				G_Damage(self, NULL, NULL, NULL, NULL, NULL, dmg, 0, DMG_NO_ARMOR, MOD_LAVA);
			}

			if (self->locals.water_type & CONTENTS_SLIME) {
				G_Damage(self, NULL, NULL, NULL, NULL, NULL, dmg, 0, DMG_NO_ARMOR, MOD_SLIME);
			}
		}
	} else {
		const vec_t speed = VectorLength(self->locals.velocity);

		if (!(self->s.effects & EF_DESPAWN) && speed > 30.0) {
			self->s.trail = TRAIL_GIB;
		} else {
			self->s.trail = TRAIL_NONE;
		}
	}

	if (age > 33000) {
		G_FreeEntity(self);
		return;
	}

	// sink into the floor after a few seconds
	if (age > 30000) {

		self->s.effects |= EF_DESPAWN;

		self->locals.move_type = MOVE_TYPE_NONE;
		self->locals.take_damage = false;

		self->solid = SOLID_NOT;

		if (self->locals.ground_entity) {
			self->s.origin[2] -= gi.frame_seconds * 8.0;
		}

		gi.LinkEntity(self);
	}

	self->locals.next_think = g_level.time + gi.frame_millis;
}

/**
 * @brief Corpses explode into giblets when killed. Giblets receive the
 * velocity of the corpse, and bounce when damaged. They eventually sink
 * through the floor and disappear.
 */
static void G_ClientCorpse_Die(g_entity_t *self, g_entity_t *attacker __attribute__((unused)),
		uint32_t mod __attribute__((unused))) {

	const vec3_t mins[] = { { -3.0, -3.0, -3.0 }, { -6.0, -6.0, -6.0 }, { -9.0, -9.0, -9.0 } };
	const vec3_t maxs[] = { { 3.0, 3.0, 3.0 }, { 6.0, 6.0, 6.0 }, { 9.0, 9.0, 9.0 } };

	uint16_t i, count = 3 + Random() % 3;

	for (i = 0; i < count; i++) {
		g_entity_t *ent = G_AllocEntity(__func__);

		VectorCopy(self->s.origin, ent->s.origin);

		VectorCopy(mins[i % NUM_GIB_MODELS], ent->mins);
		VectorCopy(maxs[i % NUM_GIB_MODELS], ent->maxs);

		ent->solid = SOLID_DEAD;

		ent->s.model1 = g_media.models.gibs[i % NUM_GIB_MODELS];
		ent->locals.noise_index = g_media.sounds.gib_hits[i % NUM_GIB_MODELS];

		VectorCopy(self->locals.velocity, ent->locals.velocity);

		const int16_t h = Clamp(-5.0 * self->locals.health, 100, 500);

		ent->locals.velocity[0] += h * Randomc();
		ent->locals.velocity[1] += h * Randomc();
		ent->locals.velocity[2] += 100.0 + (h * Randomf());

		ent->locals.clip_mask = MASK_CLIP_CORPSE;
		ent->locals.dead = true;
		ent->locals.mass = ((i % NUM_GIB_MODELS) + 1) * 20.0;
		ent->locals.move_type = MOVE_TYPE_BOUNCE;
		ent->locals.next_think = g_level.time + gi.frame_millis;
		ent->locals.take_damage = true;
		ent->locals.Think = G_ClientCorpse_Think;
		ent->locals.Touch = G_ClientGiblet_Touch;

		gi.LinkEntity(ent);
	}

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_GIB);
	gi.WritePosition(self->s.origin);
	gi.Multicast(self->s.origin, MULTICAST_PVS, NULL);

	if (self->client) {
		VectorCopy(mins[2], self->mins);
		VectorCopy(maxs[2], self->maxs);

		const int16_t h = Clamp(-5.0 * self->locals.health, 100, 500);

		self->locals.velocity[0] += h * Randomc();
		self->locals.velocity[1] += h * Randomc();
		self->locals.velocity[2] += 100.0 + (h * Randomf());

		self->locals.Die = NULL;

		self->client->ps.pm_state.flags |= PMF_GIBLET;

		self->s.model1 = g_media.models.gibs[2];

		self->locals.mass = 20.0;

		gi.LinkEntity(self);
	} else {
		G_FreeEntity(self);
	}
}

/**
 * @brief Spawns a corpse for the specified client. The corpse will eventually sink into the floor
 * and disappear if not over-killed.
 */
static void G_ClientCorpse(g_entity_t *self) {

	if (self->sv_flags & SVF_NO_CLIENT)
		return;

	if (self->locals.dead == false)
		return;

	if (self->s.model1 == 0)
		return;

	g_entity_t *ent = G_AllocEntity(__func__);

	ent->solid = SOLID_DEAD;

	VectorCopy(self->s.origin, ent->s.origin);
	VectorCopy(self->s.angles, ent->s.angles);

	VectorCopy(self->mins, ent->mins);
	VectorCopy(self->maxs, ent->maxs);

	ent->s.client = self->s.client;
	ent->s.model1 = self->s.model1;

	ent->s.animation1 = self->s.animation1;
	ent->s.animation2 = self->s.animation2;

	ent->s.effects = EF_CORPSE;

	VectorCopy(self->locals.velocity, ent->locals.velocity);

	ent->locals.clip_mask = MASK_CLIP_CORPSE;
	ent->locals.dead = true;
	ent->locals.mass = self->locals.mass;
	ent->locals.move_type = MOVE_TYPE_BOUNCE;
	ent->locals.take_damage = true;
	ent->locals.health = self->locals.health;
	ent->locals.Die = ent->locals.health > 0 ? G_ClientCorpse_Die : NULL;
	ent->locals.Think = G_ClientCorpse_Think;
	ent->locals.next_think = g_level.time + gi.frame_millis;

	gi.LinkEntity(ent);
}

#define CLIENT_CORPSE_HEALTH 80

/**
 * @brief A client's health is less than or equal to zero. Render death effects, drop
 * certain items we're holding and force the client into a temporary spectator
 * state with the scoreboard shown.
 */
static void G_ClientDie(g_entity_t *self, g_entity_t *attacker, uint32_t mod) {

	G_ClientObituary(self, attacker, mod);

	if (g_level.warmup == false) {
		if ((g_level.gameplay == GAME_DEATHMATCH || g_level.gameplay == GAME_DUEL) && mod != MOD_TRIGGER_HURT) {
			G_TossWeapon(self);
			G_TossQuadDamage(self);
		}

		if (g_level.ctf) {
			G_TossFlag(self);
		}
	}

	if (self->locals.health <= -CLIENT_CORPSE_HEALTH) {
		G_ClientCorpse_Die(self, attacker, mod);
	} else {
		gi.Sound(self, gi.SoundIndex("*death_1"), ATTEN_NORM);

		const vec_t r = Randomf();
		if (r < 0.33)
			G_SetAnimation(self, ANIM_BOTH_DEATH1, true);
		else if (r < 0.66)
			G_SetAnimation(self, ANIM_BOTH_DEATH2, true);
		else
			G_SetAnimation(self, ANIM_BOTH_DEATH3, true);

		self->maxs[2] = 0.0; // corpses are laying down

		self->locals.health = CLIENT_CORPSE_HEALTH;
		self->locals.Die = G_ClientCorpse_Die;
	}

	self->solid = SOLID_DEAD;

	self->s.event = EV_NONE;
	self->s.effects = EF_NONE;
	self->s.trail = TRAIL_NONE;
	self->s.model2 = 0;
	self->s.model3 = 0;
	self->s.model4 = 0;
	self->s.sound = 0;

	self->locals.take_damage = true;

	self->client->locals.respawn_time = g_level.time + 1800; // respawn after death animation finishes
	self->client->locals.show_scores = true;

	 self->client->locals.persistent.deaths++;

	gi.LinkEntity(self);
}

/**
 * @brief Stocks client's inventory with specified item. Weapons receive
 * specified quantity of ammo, while health and armor are set to
 * the specified quantity.
 */
static void G_Give(g_entity_t *ent, char *it, int16_t quantity) {

	if (!g_ascii_strcasecmp(it, "Health")) {
		ent->locals.health = quantity;
		return;
	}

	const g_item_t *item = G_FindItem(it);

	if (!item)
		return;

	const uint16_t index = ITEM_INDEX(item);

	if (item->type == ITEM_WEAPON) { // weapons receive quantity as ammo
		ent->client->locals.inventory[index]++;

		if (item->ammo) {
			const g_item_t *ammo = G_FindItem(item->ammo);
			if (ammo) {
				const uint16_t ammo_index = ITEM_INDEX(ammo);

				if (quantity > -1)
					ent->client->locals.inventory[ammo_index] = quantity;
				else
					ent->client->locals.inventory[ammo_index] = ammo->quantity;
			}
		}
	} else { // while other items receive quantity directly
		if (quantity > -1)
			ent->client->locals.inventory[index] = quantity;
		else
			ent->client->locals.inventory[index] = item->quantity;
	}
}

/**
 * @brief
 */
static _Bool G_GiveLevelLocals(g_entity_t *ent) {
	char buf[512], *it, *q;
	int32_t quantity;

	if (*g_level.give == '\0')
		return false;

	g_strlcpy(buf, g_level.give, sizeof(buf));

	it = strtok(buf, ",");

	while (true) {

		if (!it)
			break;

		it = g_strstrip(it);

		if (*it != '\0') {

			if ((q = strrchr(it, ' '))) {
				quantity = atoi(q + 1);

				if (quantity > -1) // valid quantity
					*q = '\0';
			} else
				quantity = -1;

			G_Give(ent, it, quantity);
		}

		it = strtok(NULL, ",");
	}

	return true;
}

/**
 * @brief
 */
static void G_InitClientInventory(g_entity_t *ent) {
	const g_item_t *item;

	// set max inventory levels
	ent->client->locals.max_shells = 80;
	ent->client->locals.max_bullets = 200;
	ent->client->locals.max_grenades = 50;
	ent->client->locals.max_rockets = 50;
	ent->client->locals.max_cells = 200;
	ent->client->locals.max_bolts = 150;
	ent->client->locals.max_slugs = 50;
	ent->client->locals.max_nukes = 10;

	// instagib gets railgun and slugs, both in normal mode and warmup
	if (g_level.gameplay == GAME_INSTAGIB) {
		G_Give(ent, "Railgun", 1000);
		G_Give(ent, "Grenades", 50);
		item = G_FindItem("Railgun");
	}
	// arena or dm warmup yields all weapons, health, etc..
	else if ((g_level.gameplay == GAME_ARENA) || g_level.warmup) {
		G_Give(ent, "Railgun", 50);
		G_Give(ent, "Lightning", 200);
		G_Give(ent, "Hyperblaster", 200);
		G_Give(ent, "Rocket Launcher", 50);
		G_Give(ent, "Grenade Launcher", 50);
		G_Give(ent, "Machinegun", 200);
		G_Give(ent, "Super Shotgun", 80);
		G_Give(ent, "Shotgun", 80);
		G_Give(ent, "Blaster", 0);

		G_Give(ent, "Body Armor", -1);

		item = G_FindItem("Rocket Launcher");
	}
	// dm gets the blaster
	else {
		G_Give(ent, "Blaster", -1);
		item = G_FindItem("Blaster");
	}

	// use the best weapon given by the level
	if (G_GiveLevelLocals(ent)) {
		G_UseBestWeapon(ent);
	} else { // or the one given by the gameplay type above
		G_UseWeapon(ent, item);
	}
}

/**
 * @brief Returns the distance to the nearest enemy from the given spot.
 */
static vec_t G_EnemyRangeFromSpot(g_entity_t *ent, g_entity_t *spot) {
	vec_t dist, best_dist;
	vec3_t v;

	best_dist = 9999999.0;

	for (int32_t i = 1; i <= sv_max_clients->integer; i++) {
		const g_entity_t *other = &g_game.entities[i];

		if (!other->in_use)
			continue;

		if (other->locals.health <= 0)
			continue;

		if (other->client->locals.persistent.spectator)
			continue;

		VectorSubtract(spot->s.origin, other->s.origin, v);
		dist = VectorLength(v);

		if (g_level.teams || g_level.ctf) { // avoid collision with team mates

			if (other->client->locals.persistent.team == ent->client->locals.persistent.team) {
				if (dist > 64.0) // if they're far away, ignore them
					continue;
			}
		}

		if (dist < best_dist)
			best_dist = dist;
	}

	return best_dist;
}

/**
 * @brief
 */
static g_entity_t *G_SelectRandomSpawnPoint(const char *class_name) {
	g_entity_t *spot;
	int32_t count = 0;

	spot = NULL;

	while ((spot = G_Find(spot, EOFS(class_name), class_name)) != NULL)
		count++;

	if (!count)
		return NULL;

	count = Random() % count;

	while (count-- >= 0)
		spot = G_Find(spot, EOFS(class_name), class_name);

	return spot;
}

/**
 * @brief
 */
static g_entity_t *G_SelectFarthestSpawnPoint(g_entity_t *ent, const char *class_name) {
	g_entity_t *spot, *best_spot;
	vec_t dist, best_dist;

	spot = best_spot = NULL;
	best_dist = 0.0;

	while ((spot = G_Find(spot, EOFS(class_name), class_name)) != NULL) {

		dist = G_EnemyRangeFromSpot(ent, spot);

		if (dist > best_dist) {
			best_spot = spot;
			best_dist = dist;
		}
	}

	if (best_spot)
		return best_spot;

	// if there is an enemy just spawned on each and every start spot
	// we have no choice to turn one into a telefrag meltdown
	spot = G_Find(NULL, EOFS(class_name), class_name);

	return spot;
}

/**
 * @brief
 */
static g_entity_t *G_SelectDeathmatchSpawnPoint(g_entity_t *ent) {

	if (g_spawn_farthest->value)
		return G_SelectFarthestSpawnPoint(ent, "info_player_deathmatch");

	return G_SelectRandomSpawnPoint("info_player_deathmatch");
}

/**
 * @brief
 */
static g_entity_t *G_SelectTeamSpawnPoint(g_entity_t *ent) {

	if (!ent->client->locals.persistent.team)
		return NULL;

	const char *class_name = ent->client->locals.persistent.team == &g_team_good ?
			"info_player_team1" : "info_player_team2";

	if (g_spawn_farthest->value)
		return G_SelectFarthestSpawnPoint(ent, class_name);

	return G_SelectRandomSpawnPoint(class_name);
}

/**
 * @brief Selects the most appropriate spawn point for the given client.
 */
static g_entity_t *G_SelectSpawnPoint(g_entity_t *ent) {
	g_entity_t *spawn = NULL;

	if (g_level.teams || g_level.ctf) // try team spawns first if applicable
		spawn = G_SelectTeamSpawnPoint(ent);

	if (spawn == NULL) // fall back on DM spawns (e.g CTF games on DM maps)
		spawn = G_SelectDeathmatchSpawnPoint(ent);

	// and lastly fall back on single player start
	if (spawn == NULL) {
		while ((spawn = G_Find(spawn, EOFS(class_name), "info_player_start")) != NULL) {
			if (spawn->locals.target_name == NULL) // hopefully without a target
				break;
		}

		if (spawn == NULL) { // last resort, find any
			if ((spawn = G_Find(spawn, EOFS(class_name), "info_player_start")) == NULL)
				gi.Error("Couldn't find spawn point\n");
		}
	}

	return spawn;
}

/**
 * @brief The grunt work of putting the client into the server on [re]spawn.
 */
static void G_ClientRespawn_(g_entity_t *ent) {
	vec3_t cmd_angles, delta_angles;

	gi.UnlinkEntity(ent);

	// leave a corpse in our place if applicable
	G_ClientCorpse(ent);

	// retain the persistent structure
	g_client_persistent_t persistent = ent->client->locals.persistent;

	// and the last angles for calculating delta to spawn point
	VectorCopy(ent->client->locals.cmd_angles, cmd_angles);

	// clear the client and restore the persistent state
	memset(ent->client, 0, sizeof(*ent->client));
	ent->client->locals.persistent = persistent;

	// find a spawn point
	const g_entity_t *spawn = G_SelectSpawnPoint(ent);

	// move to the spawn origin
	VectorCopy(spawn->s.origin, ent->s.origin);
	ent->s.origin[2] += PM_STEP_HEIGHT;

	// and calculate delta angles
	VectorClear(ent->s.angles);
	VectorSubtract(spawn->s.angles, cmd_angles, delta_angles);

	// pack the new origin and delta angles into the player state
	VectorCopy(ent->s.origin, ent->client->ps.pm_state.origin);
	PackAngles(delta_angles, ent->client->ps.pm_state.delta_angles);

	VectorClear(ent->locals.velocity);

	ent->s.effects = 0;
	ent->s.model1 = 0;
	ent->s.model2 = 0;
	ent->s.model3 = 0;
	ent->s.model4 = 0;

	if (ent->client->locals.persistent.spectator) { // spawn a spectator
		ent->class_name = "spectator";

		VectorClear(ent->mins);
		VectorClear(ent->maxs);

		ent->solid = SOLID_NOT;
		ent->sv_flags = SVF_NO_CLIENT;

		ent->locals.move_type = MOVE_TYPE_NO_CLIP;
		ent->locals.dead = true;
		ent->locals.take_damage = false;

		ent->client->locals.chase_target = NULL;
		ent->client->locals.weapon = NULL;

		ent->client->locals.persistent.team = NULL;
		ent->client->locals.persistent.ready = false;
	}
	else { // spawn an active client
		ent->class_name = "client";

		ent->solid = SOLID_BOX;
		ent->sv_flags = 0;

		VectorScale(PM_MINS, PM_SCALE, ent->mins);
		VectorScale(PM_MAXS, PM_SCALE, ent->maxs);

		ent->s.model1 = MODEL_CLIENT;
		ent->s.client = ent - g_game.entities - 1;
		ent->s.event = EV_CLIENT_TELEPORT;

		G_SetAnimation(ent, ANIM_TORSO_STAND1, true);
		G_SetAnimation(ent, ANIM_LEGS_JUMP1, true);

		ent->locals.clip_mask = MASK_CLIP_PLAYER;
		ent->locals.dead = false;
		ent->locals.Die = G_ClientDie;
		ent->locals.ground_entity = NULL;
		ent->locals.health = ent->locals.max_health = 100;
		ent->client->locals.max_boost_health = 200;
		ent->locals.move_type = MOVE_TYPE_WALK;
		ent->locals.mass = 200.0;
		ent->locals.take_damage = true;
		ent->locals.water_level = ent->locals.old_water_level = 0;
		ent->locals.water_type = 0;

		// hold in place briefly
		ent->client->ps.pm_state.flags = PMF_TIME_TELEPORT;
		ent->client->ps.pm_state.time = 20;

		// setup inventory, max health, weapon, etc
		G_InitClientInventory(ent);

		// briefly disable firing, since we likely just clicked to respawn
		ent->client->locals.weapon_fire_time = g_level.time + 250;

		G_KillBox(ent); // kill anyone in our spot
	}

	gi.LinkEntity(ent);
}

/**
 * @brief In this case, voluntary means that the client has explicitly requested
 * a respawn by changing their spectator status.
 */
void G_ClientRespawn(g_entity_t *ent, _Bool voluntary) {

	G_ClientRespawn_(ent);

	// clear scores and match/round on voluntary changes
	if (ent->client->locals.persistent.spectator && voluntary) {
		ent->client->locals.persistent.score = ent->client->locals.persistent.captures = 0;
		ent->client->locals.persistent.match_num = ent->client->locals.persistent.round_num = 0;
	} else {
		ent->client->locals.persistent.match_num = g_level.match_num;
		ent->client->locals.persistent.round_num = g_level.round_num;
	}

	ent->client->locals.respawn_time = g_level.time;
	ent->client->locals.respawn_protection_time = g_level.time + g_respawn_protection->value * 1000;

	if (!voluntary) // don't announce involuntary spectator changes
		return;

	if (ent->client->locals.persistent.spectator)
		gi.BroadcastPrint(PRINT_HIGH, "%s likes to watch\n",
				ent->client->locals.persistent.net_name);
	else if (ent->client->locals.persistent.team)
		gi.BroadcastPrint(PRINT_HIGH, "%s has joined %s\n", ent->client->locals.persistent.net_name,
				ent->client->locals.persistent.team->name);
	else
		gi.BroadcastPrint(PRINT_HIGH, "%s wants some\n", ent->client->locals.persistent.net_name);
}

/**
 * @brief Called when a client has finished connecting, and is ready
 * to be placed into the game. This will happen every level load.
 */
void G_ClientBegin(g_entity_t *ent) {
	char welcome[MAX_STRING_CHARS];

	G_InitEntity(ent, "client");

	const int32_t entity_num = ent - g_game.entities - 1;
	ent->client = g_game.clients + entity_num;

	VectorClear(ent->client->locals.cmd_angles);
	ent->client->locals.persistent.first_frame = g_level.frame_num;

	// force spectator if match or rounds
	if (g_level.match || g_level.rounds)
		ent->client->locals.persistent.spectator = true;
	else if (g_level.teams || g_level.ctf) {
		if (g_auto_join->value)
			G_AddClientToTeam(ent, G_SmallestTeam()->name);
		else
			ent->client->locals.persistent.spectator = true;
	}

	// spawn them in
	G_ClientRespawn(ent, true);

	if (g_level.intermission_time) {
		G_ClientToIntermission(ent);
	} else {
		g_snprintf(welcome, sizeof(welcome), "^2Welcome to ^7%s", sv_hostname->string);

		if (*g_motd->string) {
			char motd[MAX_QPATH];
			g_snprintf(motd, sizeof(motd), "\n%s^7", g_motd->string);

			strncat(welcome, motd, sizeof(welcome) - strlen(welcome) - 1);
		}

		strncat(welcome, "\n^2Gameplay is ^1", sizeof(welcome) - strlen(welcome) - 1);
		strncat(welcome, G_GameplayName(g_level.gameplay), sizeof(welcome) - strlen(welcome) - 1);

		if (g_level.teams)
			strncat(welcome, "\n^2Teams are enabled", sizeof(welcome) - strlen(welcome) - 1);

		if (g_level.ctf)
			strncat(welcome, "\n^2CTF is enabled", sizeof(welcome) - strlen(welcome) - 1);

		if (g_voting->value)
			strncat(welcome, "\n^2Voting is enabled", sizeof(welcome) - strlen(welcome) - 1);

		gi.WriteByte(SV_CMD_CENTER_PRINT);
		gi.WriteString(welcome);
		gi.Unicast(ent, true);

		if (G_TIMEOUT) {	// joined during a match timeout
			ent->client->ps.pm_state.type = PM_FREEZE;
		}
	}

	// make sure all view stuff is valid
	G_ClientEndFrame(ent);
}

/**
 * @brief
 */
void G_ClientUserInfoChanged(g_entity_t *ent, const char *user_info) {
	char name[MAX_NET_NAME];

	// check for malformed or illegal info strings
	if (!ValidateUserInfo(user_info)) {
		user_info = DEFAULT_USER_INFO;
	}

	gi.Debug("%s\n", user_info);

	// set name, use a temp buffer to compute length and crutch up bad names
	const char *s = GetUserInfo(user_info, "name");

	g_strlcpy(name, s, sizeof(name));

	_Bool color = false;
	char *c = name;
	int32_t i = 0;

	// trim to 15 printable chars
	while (i < MAX_NET_NAME_PRINTABLE) {

		if (!*c)
			break;

		if (IS_COLOR(c)) {
			color = true;
			c += 2;
			continue;
		}

		c++;
		i++;
	}
	name[c - name] = '\0';

	if (!i) // name had nothing printable
		strcpy(name, "newbie");

	if (color) // reset to white
		strcat(name, "^7");

	g_client_t *cl = ent->client;
	if (strncmp(cl->locals.persistent.net_name, name, sizeof(cl->locals.persistent.net_name))) {

		if (*cl->locals.persistent.net_name != '\0') {
			gi.BroadcastPrint(PRINT_MEDIUM, "%s changed name to %s\n", cl->locals.persistent.net_name,
					name);
		}

		g_strlcpy(cl->locals.persistent.net_name, name, sizeof(cl->locals.persistent.net_name));
	}

	// set skin
	if ((g_level.teams || g_level.ctf) && cl->locals.persistent.team) // players must use team_skin to change
		s = cl->locals.persistent.team->skin;
	else
		s = GetUserInfo(user_info, "skin");

	if (strlen(s) && !strstr(s, "..")) // something valid-ish was provided
		g_strlcpy(cl->locals.persistent.skin, s, sizeof(cl->locals.persistent.skin));
	else {
		g_strlcpy(cl->locals.persistent.skin, "qforcer/default",
				sizeof(cl->locals.persistent.skin));
	}

	// set color
	s = GetUserInfo(user_info, "color");
	cl->locals.persistent.color = G_ColorByName(s, EFFECT_COLOR_DEFAULT);

	// combine name and skin into a config_string
	gi.ConfigString(CS_CLIENTS + (cl - g_game.clients),
			va("%s\\%s", cl->locals.persistent.net_name, cl->locals.persistent.skin));

	// set hand, if anything should go wrong, it defaults to 0 (centered)
	cl->locals.persistent.hand = strtoul(GetUserInfo(user_info, "hand"), NULL, 10);

	s = GetUserInfo(user_info, "active");
	if (g_strcmp0(s, "0") == 0)
		ent->s.effects = ent->s.effects | EF_INACTIVE;
	else
		ent->s.effects &= ~(EF_INACTIVE);

	// save off the user_info in case we want to check something later
	g_strlcpy(ent->client->locals.persistent.user_info, user_info,
			  sizeof(ent->client->locals.persistent.user_info));
}

/**
 * @brief Called when a player begins connecting to the server.
 * The game can refuse entrance to a client by returning false.
 * If the client is allowed, the connection process will continue
 * and eventually get to G_Begin()
 * Changing levels will NOT cause this to be called again.
 */
_Bool G_ClientConnect(g_entity_t *ent, char *user_info) {

	// check password
	if (strlen(g_password->string) && !ent->ai) {
		if (g_strcmp0(g_password->string, GetUserInfo(user_info, "password"))) {
			SetUserInfo(user_info, "rejmsg", "Password required or incorrect.");
			return false;
		}
	}

	// they can connect
	ent->client = g_game.clients + (ent - g_game.entities - 1);

	// clean up locals things which are not reset on spawns
	ent->client->locals.persistent.score = 0;
	ent->client->locals.persistent.team = NULL;
	ent->client->locals.persistent.vote = VOTE_NO_OP;
	ent->client->locals.persistent.spectator = false;
	ent->client->locals.persistent.net_name[0] = 0;
	ent->client->locals.persistent.deaths = 0;

	// set name, skin, etc..
	G_ClientUserInfoChanged(ent, user_info);

	if (sv_max_clients->integer > 1)
		gi.BroadcastPrint(PRINT_HIGH, "%s connected\n", ent->client->locals.persistent.net_name);

	ent->sv_flags = 0; // make sure we start with known default
	return true;
}

/**
 * @brief Called when a player drops from the server. Not be called between levels.
 */
void G_ClientDisconnect(g_entity_t *ent) {
	int32_t entity_num;

	if (!ent->client)
		return;

	G_TossQuadDamage(ent);
	G_TossFlag(ent);

	gi.BroadcastPrint(PRINT_HIGH, "%s bitched out\n", ent->client->locals.persistent.net_name);

	// send effect
	gi.WriteByte(SV_CMD_MUZZLE_FLASH);
	gi.WriteShort(ent->s.number);
	gi.WriteByte(MZ_LOGOUT);
	gi.Multicast(ent->s.origin, MULTICAST_PHS, NULL);

	gi.UnlinkEntity(ent);

	ent->class_name = "disconnected";
	ent->in_use = false;
	ent->solid = SOLID_NOT;
	ent->sv_flags = SVF_NO_CLIENT;

	memset(&ent->s, 0, sizeof(ent->s));
	ent->s.number = ent - g_game.entities;

	memset(ent->client, 0, sizeof(g_client_t));

	entity_num = ent - g_game.entities - 1;
	gi.ConfigString(CS_CLIENTS + entity_num, "");
}

/**
 * @brief Ignore ourselves, clipping to the correct mask based on our status.
 */
static cm_trace_t G_ClientMove_Trace(const vec3_t start, const vec3_t end, const vec3_t mins,
		const vec3_t maxs) {

	const g_entity_t *self = g_level.current_entity;

	if (self->locals.dead)
		return gi.Trace(start, end, mins, maxs, self, MASK_CLIP_CORPSE);
	else
		return gi.Trace(start, end, mins, maxs, self, MASK_CLIP_PLAYER);

}

/**
 * @brief Debug messages for Pm_Move.
 */
static void G_ClientMove_Debug(const char *msg) {
	gi.Debug("!Server: %u %s", g_level.time, msg);
}

/**
 * @brief Process the movement command, call Pm_Move and act on the result.
 */
static void G_ClientMove(g_entity_t *ent, pm_cmd_t *cmd) {
	vec3_t old_velocity, velocity;
	pm_move_t pm;

	g_client_t *cl = ent->client;

	// save the raw angles sent over in the command
	UnpackAngles(cmd->angles, cl->locals.cmd_angles);

	// set the move type
	if (ent->locals.move_type == MOVE_TYPE_NO_CLIP) {
		cl->ps.pm_state.type = PM_SPECTATOR;
	} else if (ent->locals.dead) {
		cl->ps.pm_state.type = PM_DEAD;
	} else {
		cl->ps.pm_state.type = PM_NORMAL;
	}

	// copy the current gravity in
	cl->ps.pm_state.gravity = g_level.gravity;

	memset(&pm, 0, sizeof(pm));
	pm.s = cl->ps.pm_state;

	VectorCopy(ent->s.origin, pm.s.origin);
	VectorCopy(ent->locals.velocity, pm.s.velocity);

	pm.cmd = *cmd;
	pm.ground_entity = ent->locals.ground_entity;

	pm.PointContents = gi.PointContents;
	pm.Trace = G_ClientMove_Trace;

	pm.Debug = G_ClientMove_Debug;

	// perform a move
	Pm_Move(&pm);

	// save results of move
	cl->ps.pm_state = pm.s;

	VectorCopy(ent->locals.velocity, old_velocity);

	VectorCopy(pm.s.origin, ent->s.origin);
	VectorCopy(pm.s.velocity, ent->locals.velocity);

	VectorCopy(pm.mins, ent->mins);
	VectorCopy(pm.maxs, ent->maxs);

	// copy the clamped angles out
	VectorCopy(pm.angles, cl->locals.angles);

	// update the directional vectors based on new view angles
	AngleVectors(cl->locals.angles, cl->locals.forward, cl->locals.right, cl->locals.up);

	// update the horizontal speed scalar based on new velocity
	VectorCopy(ent->locals.velocity, velocity);
	velocity[2] = 0.0;
	cl->locals.speed = VectorNormalize(velocity);

	// blend animations for live players
	if (ent->locals.dead == false) {

		if (pm.s.flags & PMF_JUMPED) {
			if (g_level.time - 100 > cl->locals.jump_time) {
				vec3_t angles, forward, point;
				cm_trace_t tr;

				VectorSet(angles, 0.0, ent->s.angles[YAW], 0.0);
				AngleVectors(angles, forward, NULL, NULL);

				VectorMA(ent->s.origin, cl->locals.speed * 0.4, velocity, point);

				// trace towards our jump destination to see if we have room to backflip
				tr = gi.Trace(ent->s.origin, point, ent->mins, ent->maxs, ent, MASK_CLIP_PLAYER);

				if (DotProduct(velocity, forward) < -0.1 && tr.fraction == 1.0 && cl->locals.speed > 200.0)
					G_SetAnimation(ent, ANIM_LEGS_JUMP2, true);
				else
					G_SetAnimation(ent, ANIM_LEGS_JUMP1, true);

				// landing events take priority over jump events
				if (pm.water_level < 3 && ent->s.event != EV_CLIENT_LAND) {
					ent->s.event = EV_CLIENT_JUMP;
				}

				cl->locals.jump_time = g_level.time;
			}
		} else if (pm.s.flags & PMF_TIME_WATER_JUMP) {
			if (g_level.time - 2000 > cl->locals.jump_time) {

				G_SetAnimation(ent, ANIM_LEGS_JUMP1, true);

				ent->s.event = EV_CLIENT_JUMP;
				cl->locals.jump_time = g_level.time;
			}
		} else if (pm.s.flags & PMF_TIME_LAND) {
			if (g_level.time - 800 > cl->locals.land_time) {
				g_entity_event_t event = EV_CLIENT_LAND;

				if (old_velocity[2] <= PM_SPEED_FALL) { // player will take damage
					int16_t damage = ((int16_t) -((old_velocity[2] - PM_SPEED_FALL) * 0.05));

					damage >>= pm.water_level; // water breaks the fall

					if (damage < 1)
						damage = 1;

					if (old_velocity[2] <= PM_SPEED_FALL_FAR)
						event = EV_CLIENT_FALL_FAR;
					else
						event = EV_CLIENT_FALL;

					cl->locals.pain_time = g_level.time; // suppress pain sound

					G_Damage(ent, NULL, NULL, vec3_up, NULL, NULL, damage, 0, DMG_NO_ARMOR, MOD_FALLING);
				}

				if (G_IsAnimation(ent, ANIM_LEGS_JUMP2))
					G_SetAnimation(ent, ANIM_LEGS_LAND2, true);
				else
					G_SetAnimation(ent, ANIM_LEGS_LAND1, true);

				ent->s.event = event;
				cl->locals.land_time = g_level.time;
			}
		} else if (pm.s.flags & PMF_ON_LADDER) {
			if (g_level.time - 400 > cl->locals.jump_time) {
				if (fabs(ent->locals.velocity[2]) > 20.0) {

					G_SetAnimation(ent, ANIM_LEGS_JUMP1, true);

					ent->s.event = EV_CLIENT_JUMP;
					cl->locals.jump_time = g_level.time;
				}
			}
		}

		// detect hitting the ground to help with animation blending
		if (pm.ground_entity && !ent->locals.ground_entity) {
			cl->locals.ground_time = g_level.time;
		}
	}

	// copy ground and water state back into entity
	ent->locals.ground_entity = pm.ground_entity;
	ent->locals.water_level = pm.water_level;
	ent->locals.water_type = pm.water_type;

	// and finally link them back in to collide with others below
	gi.LinkEntity(ent);

	// touch every object we collided with objects
	if (ent->locals.move_type != MOVE_TYPE_NO_CLIP) {

		for (uint16_t i = 0; i < pm.num_touch_ents; i++) {
			g_entity_t *other = pm.touch_ents[i];

			if (!other->locals.Touch)
				continue;

			other->locals.Touch(other, ent, NULL, NULL);
		}

		G_TouchOccupy(ent);
	}
}

/**
 * @brief Expire any items which are time-sensitive.
 */
static void G_ClientInventoryThink(g_entity_t *ent) {

	if (ent->client->locals.inventory[g_media.items.quad_damage]) { // if they have quad

		if (ent->client->locals.quad_damage_time < g_level.time) { // expire it

			ent->client->locals.quad_damage_time = 0.0;
			ent->client->locals.inventory[g_media.items.quad_damage] = 0;

			gi.Sound(ent, gi.SoundIndex("quad/expire"), ATTEN_NORM);

			ent->s.effects &= ~EF_QUAD;
		}
	}

	// other power-ups and things can be timed out here as well

	if (ent->client->locals.respawn_protection_time > g_level.time)
		ent->s.effects |= EF_RESPAWN;
	else
		ent->s.effects &= ~EF_RESPAWN;

	// decrement any boosted health from items
	if (ent->locals.health > ent->locals.max_health &&
	    ent->client->locals.boost_time < g_level.time) {
		ent->locals.health -= 1;
		ent->client->locals.boost_time = g_level.time + 750;
	}
}

/**
 * @brief This will be called once for each client frame, which will usually be a
 * couple times for each server frame.
 */
void G_ClientThink(g_entity_t *ent, pm_cmd_t *cmd) {

	if (g_level.intermission_time)
		return;
	
	if (g_level.match_status & MSTAT_TIMEOUT)
		return;

	g_level.current_entity = ent;
	g_client_t *cl = ent->client;

	if (cl->locals.chase_target) { // ensure chase is valid

		cl->ps.pm_state.flags |= PMF_NO_PREDICTION;

		if (!cl->locals.chase_target->in_use
				|| cl->locals.chase_target->client->locals.persistent.spectator) {

			g_entity_t *other = cl->locals.chase_target;

			G_ClientChaseNext(ent);

			if (cl->locals.chase_target == other) { // no one to chase
				cl->locals.chase_target = NULL;
			}
		}
	}

	if (!cl->locals.chase_target) { // move through the world
		G_ClientMove(ent, cmd);
	}

	cl->locals.cmd = *cmd;
	cl->locals.old_buttons = cl->locals.buttons;
	cl->locals.buttons = cmd->buttons;
	cl->locals.latched_buttons |= cl->locals.buttons & ~cl->locals.old_buttons;

	// fire weapon if requested
	if (cl->locals.latched_buttons & BUTTON_ATTACK) {
		if (cl->locals.persistent.spectator) {

			cl->locals.latched_buttons = 0;

			if (cl->locals.chase_target) { // toggle chase camera
				cl->locals.chase_target = cl->locals.old_chase_target = NULL;
				cl->ps.pm_state.flags &= ~PMF_NO_PREDICTION;
			} else {
				G_ClientChaseTarget(ent);
			}
		} else if (cl->locals.weapon_think_time < g_level.time) {
			G_ClientWeaponThink(ent);
		}
	}

	G_ClientInventoryThink(ent);

	// update chase camera if being followed
	for (int32_t i = 1; i <= sv_max_clients->integer; i++) {

		g_entity_t *other = g_game.entities + i;

		if (other->in_use && other->client->locals.chase_target == ent) {
			G_ClientChaseThink(other);
		}
	}
}

/**
 * @brief This will be called once for each server frame, before running
 * any other entities in the world.
 */
void G_ClientBeginFrame(g_entity_t *ent) {

	if (g_level.intermission_time)
		return;

	g_client_t *cl = ent->client;

	// run weapon think if it hasn't been done by a command
	if (cl->locals.weapon_think_time < g_level.time)
		G_ClientWeaponThink(ent);

	if (ent->locals.dead) {

		// rounds mode implies last-man-standing, force to spectator immediately if round underway
		if (g_level.rounds && g_level.round_time && g_level.time >= g_level.round_time) {
			cl->locals.persistent.spectator = true;
			G_ClientRespawn(ent, false);
		} else if (g_level.time > cl->locals.respawn_time) {

			// all other respawns require a click from the player
			if (cl->locals.latched_buttons & BUTTON_ATTACK) {
				G_ClientRespawn(ent, false);
			}
		}
	}

	cl->locals.latched_buttons = 0;
}
