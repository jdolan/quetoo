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
 * @brief Make a tasteless death announcement and record scores.
 */
static void G_ClientObituary(g_entity_t *self, g_entity_t *attacker, uint32_t mod) {
	char buffer[MAX_PRINT_MSG];

	const _Bool frag = attacker && (attacker != self) && attacker->client;

	const _Bool friendly_fire = (mod & MOD_FRIENDLY_FIRE) == MOD_FRIENDLY_FIRE;
	mod &= ~MOD_FRIENDLY_FIRE;

	if (frag) { // killed by another player

		const char *msg = "%s was killed by %s";

		switch (mod) {
			case MOD_BLASTER:
				msg = "%s was humiliated by %s's blaster :blaster:";
				break;
			case MOD_SHOTGUN:
				msg = "%s was gunned down by %s's shotgun :shotgun:";
				break;
			case MOD_SUPER_SHOTGUN:
				msg = "%s was blown away by %s's super shotgun :sshotgun:";
				break;
			case MOD_MACHINEGUN:
				msg = "%s was perforated by %s's machinegun :machinegun:";
				break;
			case MOD_GRENADE:
				msg = "%s was popped by %s's grenade :grenade:";
				break;
			case MOD_GRENADE_SPLASH:
				msg = "%s was shredded by %s's shrapnel :grenade:";
				break;
			case MOD_HANDGRENADE:
				msg = "%s caught %s's handgrenade :handgrenade:";
				break;
			case MOD_HANDGRENADE_SPLASH:
				msg = "%s felt the burn from %s's handgrenade :handgrenade:";
				break;
			case MOD_HANDGRENADE_KAMIKAZE:
				msg = "%s felt %s's pain :handgrenade:";
				break;
			case MOD_ROCKET:
				msg = "%s ate %s's rocket :rocket:";
				break;
			case MOD_ROCKET_SPLASH:
				msg = "%s almost dodged %s's rocket :rocket:";
				break;
			case MOD_HYPERBLASTER:
				msg = "%s was melted by %s's hyperblaster :hyperblaster:";
				break;
			case MOD_LIGHTNING:
				msg = "%s got a charge out of %s's lightning :lightning:";
				break;
			case MOD_LIGHTNING_DISCHARGE:
				msg = "%s was shocked by %s's discharge :lightning:";
				break;
			case MOD_RAILGUN:
				msg = "%s was railed by %s :railgun:";
				break;
			case MOD_BFG_LASER:
				msg = "%s saw the pretty lights from %s's BFG :bfg:";
				break;
			case MOD_BFG_BLAST:
				msg = "%s was disintegrated by %s's BFG blast :bfg:";
				break;
			case MOD_TELEFRAG:
				msg = "%s tried to invade %s's personal space :telefrag:";
				break;
			case MOD_HOOK:
				msg = "%s had their intestines shredded by %s's grappling hook :hook:";
				break;
		}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
		g_snprintf(buffer, sizeof(buffer), msg, self->client->locals.persistent.net_name,
		           attacker->client->locals.persistent.net_name);
#pragma clang diagnostic pop

		if (friendly_fire) {
			g_strlcat(buffer, " (^1TEAMKILL^7)", sizeof(buffer));
		}

	} else { // killed by self or world

		const char *msg = "%s sucks at life :suicide:";

		switch (mod) {
			case MOD_SUICIDE:
				msg = "%s suicides :suicide:";
				break;
			case MOD_FALLING:
				msg = "%s cratered :falldamage:";
				break;
			case MOD_CRUSH:
				msg = "%s was squished :crush:";
				break;
			case MOD_WATER:
				msg = "%s sleeps with the fishes :drown:";
				break;
			case MOD_SLIME:
				msg = "%s melted :slime:";
				break;
			case MOD_LAVA:
				msg = "%s did a back flip into the lava :lava:";
				break;
			case MOD_FIREBALL:
				msg = "%s tasted the lava rainbow :lava:";
				break;
			case MOD_TRIGGER_HURT:
				msg = "%s was in the wrong place :actofgod:";
				break;
			case MOD_ACT_OF_GOD:
				msg = "%s was killed by an act of god :actofgod:";
				break;
		}

		if (attacker == self) {
			switch (mod) {
				case MOD_GRENADE_SPLASH:
					msg = "%s went pop :explosive:";
					break;
				case MOD_HANDGRENADE_KAMIKAZE:
					msg = "%s tried to put the pin back in :handgrenade:";
					break;
				case MOD_HANDGRENADE_SPLASH:
					msg = "%s has no hair left :explosive:";
					break;
				case MOD_ROCKET_SPLASH:
					msg = "%s blew up :explosive:";
					break;
				case MOD_HYPERBLASTER_CLIMB:
					msg = "%s forgot how to climb :death:";
					break;
				case MOD_LIGHTNING_DISCHARGE:
					msg = "%s took a toaster bath :death:";
					break;
				case MOD_BFG_BLAST:
					msg = "%s should have used a smaller gun :death:";
					break;
			}
		}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
		g_snprintf(buffer, sizeof(buffer), msg, self->client->locals.persistent.net_name);
#pragma clang diagnostic pop
	}

	gi.BroadcastPrint(PRINT_HIGH, "%s\n", buffer);

	if (frag) {

		if (g_show_attacker_stats->integer) {
			int16_t a = 0;
			const g_item_t *armor = G_ClientArmor(attacker);
			if (armor) {
				a = attacker->client->locals.inventory[armor->index];
			}
			gi.ClientPrint(self, PRINT_MEDIUM, "%s had %d health and %d armor\n",
			               attacker->client->locals.persistent.net_name, attacker->locals.health, a);
		}
	}

	if (!g_level.warmup) {

		if (frag) {
			if (friendly_fire) {
				attacker->client->locals.persistent.score--;
			} else {
				attacker->client->locals.persistent.score++;
			}

			if ((g_level.teams || g_level.ctf) &&
			        self->client->locals.persistent.team &&
			        attacker->client->locals.persistent.team) {

				if (friendly_fire) {
					attacker->client->locals.persistent.team->score--;
				} else {
					attacker->client->locals.persistent.team->score++;
				}
			}
		} else {
			self->client->locals.persistent.score--;

			if ((g_level.teams || g_level.ctf) && self->client->locals.persistent.team) {
				self->client->locals.persistent.team->score--;
			}
		}
	}
}

/**
 * @brief Play a sloppy sound when impacting the world.
 */
static void G_ClientGiblet_Touch(g_entity_t *self, g_entity_t *other, const cm_trace_t *trace) {

	if (trace == NULL) {
		return;
	}

	if (G_IsSky(trace)) {
		G_FreeEntity(self);
	} else {
		const float speed = Vec3_Length(self->locals.velocity);
		if (speed > 40.0 && G_IsStructural(trace)) {

			if (g_level.time - self->locals.touch_time > 200) {
				G_MulticastSound(&(const g_play_sound_t) {
					.index = self->locals.sound,
					.entity = self,
					.atten = SOUND_ATTEN_SQUARE
				}, MULTICAST_PHS, NULL);
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
			const int32_t dmg = self->locals.health;

			if (self->locals.water_type & CONTENTS_LAVA) {
				G_Damage(self, NULL, NULL, Vec3_Zero(), self->s.origin, Vec3_Zero(), dmg, 0, DMG_NO_ARMOR, MOD_LAVA);
			}

			if (self->locals.water_type & CONTENTS_SLIME) {
				G_Damage(self, NULL, NULL, Vec3_Zero(), self->s.origin, Vec3_Zero(), dmg, 0, DMG_NO_ARMOR, MOD_SLIME);
			}
		}
	} else {
		const float speed = Vec3_Length(self->locals.velocity);

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

		if (self->locals.ground.ent) {
			self->s.origin.z -= QUETOO_TICK_SECONDS * 8.0;
		}

		gi.LinkEntity(self);
	}

	self->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief Corpses explode into giblets when killed. Giblets receive the
 * velocity of the corpse, and bounce when damaged. They eventually sink
 * through the floor and disappear.
 */
static void G_ClientCorpse_Die(g_entity_t *self, g_entity_t *attacker,
                               uint32_t mod) {

	const box3_t bounds[NUM_GIB_MODELS] = {
		Box3f(12.f, 12.f, 12.f),
		Box3f(12.f, 12.f, 12.f),
		Box3f(8.f, 8.f, 8.f),
		Box3f(16.f, 16.f, 16.f),
	};

	const int32_t count = RandomRangei(4, 8);
	for (int32_t i = 0; i < count; i++) {

		int32_t gib_index;
		if (i == 0) { // 0 is always chest
			gib_index = (NUM_GIB_MODELS - 1);
		} else if (i == 1 && !self->client) { // if we're not client, drop a head
			gib_index = 2;
		} else { // pick forearm/femur
			gib_index = RandomRangei(0, NUM_GIB_MODELS - 2);
		}

		g_entity_t *ent = G_AllocEntity();

		ent->s.origin = self->s.origin;

		ent->bounds = bounds[gib_index];

		ent->solid = SOLID_DEAD;

		ent->s.model1 = g_media.models.gibs[gib_index];
		ent->locals.sound = g_media.sounds.gib_hits[i % NUM_GIB_SOUNDS];

		ent->locals.velocity = self->locals.velocity;

		const int32_t h = Clampf(-5.0 * self->locals.health, 100, 500);

		ent->locals.velocity.x += RandomRangef(-h, h);
		ent->locals.velocity.y += RandomRangef(-h, h);
		ent->locals.velocity.z += RandomRangef(100.f, 100.f + h);

		for (int32_t i = 0; i < 3; ++i) {
			ent->locals.avelocity.xyz[i] = RandomRangef(-100.f, 100.f);
			ent->s.angles.xyz[i] = RandomRangef(0, 360.f);
		}

		ent->locals.clip_mask = CONTENTS_MASK_CLIP_CORPSE;
		ent->locals.dead = true;
		ent->locals.mass = (gib_index + 1) * 20.0;
		ent->locals.move_type = MOVE_TYPE_BOUNCE;
		ent->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
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
		self->bounds = bounds[2];

		const int32_t h = Clampf(-5.0 * self->locals.health, 100, 500);
		
		self->locals.velocity.x += RandomRangef(-h, h);
		self->locals.velocity.y += RandomRangef(-h, h);
		self->locals.velocity.z += RandomRangef(100.f, 100.f + h);

		for (int32_t i = 0; i < 3; ++i) {
			self->locals.avelocity.xyz[i] = RandomRangef(-100.f, 100.f);
			self->s.angles.xyz[i] = RandomRangef(0, 360.f);
		}

		self->locals.Die = NULL;

		self->client->ps.pm_state.flags |= PMF_GIBLET;

		self->s.model1 = g_media.models.gibs[2];

		self->locals.mass = 20.0;

		self->solid = SOLID_DEAD;

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

	if (self->sv_flags & SVF_NO_CLIENT) {
		return;
	}

	if (self->locals.dead == false) {
		return;
	}

	if (self->s.model1 == 0) {
		return;
	}

	g_entity_t *ent = G_AllocEntity();

	ent->solid = SOLID_DEAD;

	ent->s.origin = self->s.origin;
	ent->s.angles = self->s.angles;

	ent->bounds = self->bounds;

	ent->s.client = self->s.client;
	ent->s.model1 = self->s.model1;

	ent->s.animation1 = self->s.animation1;
	ent->s.animation2 = self->s.animation2;

	ent->s.effects = EF_CORPSE;

	ent->locals.velocity = self->locals.velocity;

	ent->locals.clip_mask = CONTENTS_MASK_CLIP_CORPSE;
	ent->locals.dead = true;
	ent->locals.mass = self->locals.mass;
	ent->locals.move_type = MOVE_TYPE_BOUNCE;
	ent->locals.take_damage = true;
	ent->locals.health = self->locals.health;
	ent->locals.Die = ent->locals.health > 0 ? G_ClientCorpse_Die : NULL;
	ent->locals.Think = G_ClientCorpse_Think;
	ent->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;

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

	G_ClientHookDetach(self);

	if (g_level.warmup == false) {
		if ((g_level.gameplay == GAME_DEATHMATCH || g_level.gameplay == GAME_DUEL) && mod != MOD_TRIGGER_HURT) {
			G_TossWeapon(self);
			G_TossQuadDamage(self);
		}

		if (g_level.ctf) {
			G_TossFlag(self);
		}

		if (g_level.techs) {
			G_TossTech(self);
		}
	}

	if (self->locals.health <= -CLIENT_CORPSE_HEALTH) {
		G_ClientCorpse_Die(self, attacker, mod);
	} else {
		G_MulticastSound(&(const g_play_sound_t) {
			.index = gi.SoundIndex("*death_1"),
			.entity = self,
			.origin = &self->s.origin, // send the origin in case of fast respawn
			.atten = SOUND_ATTEN_LINEAR
		}, MULTICAST_PHS, NULL);

		const float r = Randomf();
		if (r < 0.33) {
			G_SetAnimation(self, ANIM_BOTH_DEATH1, true);
		} else if (r < 0.66) {
			G_SetAnimation(self, ANIM_BOTH_DEATH2, true);
		} else {
			G_SetAnimation(self, ANIM_BOTH_DEATH3, true);
		}

		self->bounds.maxs.z = 0.0; // corpses are laying down

		self->locals.health = CLIENT_CORPSE_HEALTH;
		self->locals.Die = G_ClientCorpse_Die;
	}

	uint32_t nade_hold_time = self->client->locals.grenade_hold_time;

	if (nade_hold_time != 0) {

		G_InitProjectile(self, NULL, NULL, NULL, &self->s.origin, 1.0);

		G_HandGrenadeProjectile(
		    self,					// player
		    self->client->locals.held_grenade, // the grenade
		    self->s.origin,			// starting point
		    Vec3_Up(),				// direction
		    0,						// how fast it flies
		    120,					// damage dealt
		    120,					// knockback
		    185.0,					// blast radius
		    3000 - (g_level.time - nade_hold_time) // time before explode (next think)
		);
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

	self->locals.clip_mask = CONTENTS_MASK_CLIP_CORPSE;
	self->client->locals.respawn_time = g_level.time + 1800; // respawn after death animation finishes
	self->client->locals.show_scores = true;
	self->client->locals.persistent.deaths++;

	G_ClientDamageKick(self, self->client->locals.right, 60.0);

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

	if (!item) {
		return;
	}

	const uint16_t index = item->index;

	if (item->type == ITEM_WEAPON) { // weapons receive quantity as ammo
		ent->client->locals.inventory[index]++;

		if (item->ammo) {
			const g_item_t *ammo = item->ammo_item;
			if (ammo) {
				const uint16_t ammo_index = ammo->index;

				if (quantity > -1) {
					ent->client->locals.inventory[ammo_index] = quantity;
				} else {
					ent->client->locals.inventory[ammo_index] = ammo->quantity;
				}
			}
		}
	} else { // while other items receive quantity directly
		if (quantity > -1) {
			ent->client->locals.inventory[index] = quantity;
		} else {
			ent->client->locals.inventory[index] = item->quantity;
		}
	}
}

/**
 * @brief
 */
static _Bool G_GiveLevelLocals(g_entity_t *ent) {
	char buf[512], *it, *q;
	int32_t quantity;

	if (*g_level.give == '\0') {
		return false;
	}

	g_strlcpy(buf, g_level.give, sizeof(buf));

	it = strtok(buf, ",");

	while (true) {

		if (!it) {
			break;
		}

		it = g_strstrip(it);

		if (*it != '\0') {

			if ((q = strrchr(it, ' '))) {
				quantity = atoi(q + 1);

				if (quantity > -1) { // valid quantity
					*q = '\0';
				}
			} else {
				quantity = -1;
			}

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

	// instagib gets railgun and slugs, both in normal mode and warmup
	if (g_level.gameplay == GAME_INSTAGIB) {
		G_Give(ent, "Railgun", 1);
		G_Give(ent, "Grenades", 1);
		item = g_media.items.weapons[WEAPON_RAILGUN];
	}
	// arena or dm warmup yields all weapons, health, etc..
	else if ((g_level.gameplay == GAME_ARENA) || g_level.warmup) {
		G_Give(ent, "Railgun", 50);
		G_Give(ent, "Lightning Gun", 200);
		G_Give(ent, "Hyperblaster", 200);
		G_Give(ent, "Rocket Launcher", 50);
		G_Give(ent, "Hand Grenades", 1);
		G_Give(ent, "Grenade Launcher", 50);
		G_Give(ent, "Machinegun", 200);
		G_Give(ent, "Super Shotgun", 80);
		G_Give(ent, "Shotgun", 80);
		G_Give(ent, "Blaster", 0);

		G_Give(ent, "Body Armor", -1);

		item = g_media.items.weapons[WEAPON_ROCKET_LAUNCHER];
	}
	// dm gets the blaster
	else {
		G_Give(ent, "Blaster", -1);
		item = g_media.items.weapons[WEAPON_BLASTER];
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
static float G_EnemyRangeFromSpot(g_entity_t *ent, g_entity_t *spot) {
	float dist, best_dist;
	vec3_t v;

	best_dist = 9999999.0;

	for (int32_t i = 1; i <= sv_max_clients->integer; i++) {
		const g_entity_t *other = &g_game.entities[i];

		if (!other->in_use) {
			continue;
		}

		if (other->locals.health <= 0) {
			continue;
		}

		if (other->client->locals.persistent.spectator) {
			continue;
		}

		v = Vec3_Subtract(spot->s.origin, other->s.origin);
		dist = Vec3_Length(v);

		if (g_level.teams || g_level.ctf) { // avoid collision with team mates

			if (other->client->locals.persistent.team == ent->client->locals.persistent.team) {
				if (dist > 64.0) { // if they're far away, ignore them
					continue;
				}
			}
		}

		if (dist < best_dist) {
			best_dist = dist;
		}
	}

	return best_dist;
}

/**
 * @brief Checks if spawning a player in this spot would cause a telefrag.
 */
static _Bool G_WouldTelefrag(const vec3_t spot) {
	g_entity_t *ents[MAX_ENTITIES];
	box3_t bounds = Box3_Translate(PM_BOUNDS, spot);

	bounds.mins.z -= PM_STEP_HEIGHT;
	bounds.maxs.z += PM_STEP_HEIGHT;

	const size_t len = gi.BoxEntities(bounds, ents, lengthof(ents), BOX_COLLIDE);

	for (size_t i = 0; i < len; i++) {

		if (G_IsMeat(ents[i])) {
			return true;
		}
	}

	return false;
}

/**
 * @brief
 */
static g_entity_t *G_SelectRandomSpawnPoint(const g_spawn_points_t *spawn_points) {
	uint32_t empty_spawns[spawn_points->count];
	uint32_t num_empty_spawns = 0;

	for (uint32_t i = 0; i < spawn_points->count; i++) {

		if (!G_WouldTelefrag(spawn_points->spots[i]->s.origin)) {
			empty_spawns[num_empty_spawns++] = i;
		}
	}

	// none empty, pick randomly
	if (!num_empty_spawns) {

		if (!spawn_points->count) {
			return G_SelectRandomSpawnPoint(&g_level.spawn_points);
		}

		return spawn_points->spots[RandomRangeu(0, spawn_points->count)];
	}

	return spawn_points->spots[empty_spawns[RandomRangeu(0, num_empty_spawns)]];
}

/**
 * @brief
 */
static g_entity_t *G_SelectFarthestSpawnPoint(g_entity_t *ent, const g_spawn_points_t *spawn_points) {
	g_entity_t *spot, *best_spot;
	float dist, best_dist;

	spot = best_spot = NULL;
	best_dist = 0.0;

	for (size_t i = 0; i < spawn_points->count; i++) {

		spot = spawn_points->spots[i];
		dist = G_EnemyRangeFromSpot(ent, spot);

		if (dist > best_dist && !G_WouldTelefrag(spot->s.origin)) {
			best_spot = spot;
			best_dist = dist;
		}
	}

	if (best_spot) {
		return best_spot;
	}

	return G_SelectRandomSpawnPoint(spawn_points);
}

/**
 * @brief
 */
static g_entity_t *G_SelectDeathmatchSpawnPoint(g_entity_t *ent) {

	if (g_spawn_farthest->value) {
		return G_SelectFarthestSpawnPoint(ent, &g_level.spawn_points);
	}

	return G_SelectRandomSpawnPoint(&g_level.spawn_points);
}

/**
 * @brief
 */
static g_entity_t *G_SelectTeamSpawnPoint(g_entity_t *ent) {

	if (!ent->client->locals.persistent.team) {
		return NULL;
	}

	if (g_spawn_farthest->value) {
		return G_SelectFarthestSpawnPoint(ent, &ent->client->locals.persistent.team->spawn_points);
	}

	return G_SelectRandomSpawnPoint(&ent->client->locals.persistent.team->spawn_points);
}

/**
 * @brief Selects the most appropriate spawn point for the given client.
 */
static g_entity_t *G_SelectSpawnPoint(g_entity_t *ent) {
	g_entity_t *spawn = NULL;

	if (g_level.teams || g_level.ctf) { // try team spawns first if applicable
		spawn = G_SelectTeamSpawnPoint(ent);
	}

	if (spawn == NULL) { // fall back on DM spawns (e.g CTF games on DM maps)
		spawn = G_SelectDeathmatchSpawnPoint(ent);
	}

	return spawn;
}

/**
 * @brief The grunt work of putting the client into the server on [re]spawn.
 */
static void G_ClientRespawn_(g_entity_t *ent) {
	vec3_t cmd_angles, delta_angles;

	G_ClientHookDetach(ent);

	gi.UnlinkEntity(ent);

	// leave a corpse in our place if applicable
	G_ClientCorpse(ent);

	// retain the persistent structure
	g_client_persistent_t persistent = ent->client->locals.persistent;

	// and the last angles for calculating delta to spawn point
	cmd_angles = ent->client->locals.cmd_angles;

	// clear the client and restore the persistent state

	const g_client_t cl = *ent->client;
	memset(ent->client, 0, sizeof(*ent->client));
	ent->client->locals.persistent = persistent;
	ent->client->ai = cl.ai;
	ent->client->connected = cl.connected;

	// find a spawn point
	const g_entity_t *spawn = G_SelectSpawnPoint(ent);

	// move to the spawn origin
	ent->s.origin = spawn->s.origin;
	ent->s.origin.z += PM_STEP_HEIGHT;

	// and calculate delta angles
	ent->s.angles = Vec3_Zero();
	ent->client->locals.angles = spawn->s.angles;
	delta_angles = Vec3_Subtract(spawn->s.angles, cmd_angles);

	// pack the new origin and delta angles into the player state
	ent->client->ps.pm_state.origin = ent->s.origin;
	ent->client->ps.pm_state.delta_angles = delta_angles;

	ent->locals.velocity = Vec3_Zero();

	ent->s.effects = 0;
	ent->s.model1 = 0;
	ent->s.model2 = 0;
	ent->s.model3 = 0;
	ent->s.model4 = 0;

	if (ent->client->locals.persistent.spectator) { // spawn a spectator
		ent->class_name = "spectator";

		ent->bounds = Box3_Zero();

		ent->solid = SOLID_NOT;
		ent->sv_flags = SVF_NO_CLIENT;

		ent->locals.move_type = MOVE_TYPE_NO_CLIP;
		ent->locals.dead = true;
		ent->locals.take_damage = false;

		ent->client->locals.chase_target = NULL;
		ent->client->locals.weapon = NULL;

		ent->client->locals.persistent.team = NULL;
		ent->client->locals.persistent.ready = false;
	} else { // spawn an active client
		ent->class_name = "client";

		ent->solid = SOLID_BOX;
		ent->sv_flags = 0;

		ent->bounds = Box3_Scale(PM_BOUNDS, PM_SCALE);

		ent->s.model1 = MODEL_CLIENT;
		ent->s.client = ent->s.number - 1;
		ent->s.event = EV_CLIENT_TELEPORT;

		G_SetAnimation(ent, ANIM_TORSO_STAND1, true);
		G_SetAnimation(ent, ANIM_LEGS_JUMP1, true);

		ent->locals.clip_mask = CONTENTS_MASK_CLIP_PLAYER;

		if (ent->client->ai) {
			ent->locals.clip_mask = CONTENTS_MASK_CLIP_MONSTER;
		}

		ent->locals.dead = false;
		ent->locals.Die = G_ClientDie;
		memset(&ent->locals.ground, 0, sizeof(ent->locals.ground));
		ent->client->locals.persistent.handicap = ent->client->locals.persistent.handicap_next;
		ent->locals.max_health = ent->client->locals.persistent.handicap;
		ent->locals.health = ent->locals.max_health + 5;
		ent->client->locals.boost_time = g_level.time + 1000;
		ent->client->locals.max_armor = 200;
		ent->client->locals.max_boost_health = ent->locals.max_health + 100;
		ent->locals.move_type = MOVE_TYPE_WALK;
		ent->locals.mass = 200.0;
		ent->locals.take_damage = true;
		ent->locals.water_level = WATER_UNKNOWN;
		ent->locals.water_type = 0;
		ent->locals.ripple_size = 32.0;

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

	g_client_persistent_t *pers = &ent->client->locals.persistent;

	// clear scores and match/round on voluntary changes
	if (pers->spectator && voluntary) {
		pers->score = pers->deaths = pers->captures = 0;
		pers->match_num = pers->round_num = 0;
	} else {
		pers->match_num = g_level.match_num;
		pers->round_num = g_level.round_num;
	}

	ent->client->locals.respawn_time = g_level.time;
	ent->client->locals.respawn_protection_time = g_level.time + g_respawn_protection->value * 1000;

	if (aix) {
		aix->Spawn(ent);
	}

	if (!voluntary) { // don't announce involuntary spectator changes
		return;
	}

	if (pers->spectator) {
		gi.BroadcastPrint(PRINT_HIGH, "%s likes to watch\n", pers->net_name);
	} else if (pers->team) {
		gi.BroadcastPrint(PRINT_HIGH, "%s has joined %s\n", pers->net_name, pers->team->name);
	} else {
		gi.BroadcastPrint(PRINT_HIGH, "%s wants some\n", pers->net_name);
	}
}

/**
 * @brief Called when a client has finished connecting, and is ready
 * to be placed into the game. This will happen every level load.
 */
void G_ClientBegin(g_entity_t *ent) {
	char welcome[MAX_STRING_CHARS];

	G_InitEntity(ent, "client");

	ent->client->locals.cmd_angles = Vec3_Zero();
	ent->client->locals.persistent.first_frame = g_level.frame_num;

	// force spectator if match or rounds
	if (g_level.match || g_level.rounds) {
		ent->client->locals.persistent.spectator = true;
	} else if (g_level.teams || g_level.ctf) {
		if (g_auto_join->value || ent->client->ai) {
			G_AddClientToTeam(ent, G_SmallestTeam()->name);
		} else {
			ent->client->locals.persistent.spectator = true;
		}
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

		if (g_level.teams) {
			strncat(welcome, "\n^2Teams are enabled", sizeof(welcome) - strlen(welcome) - 1);
		}

		if (g_level.ctf) {
			strncat(welcome, "\n^2CTF is enabled", sizeof(welcome) - strlen(welcome) - 1);
		}

		// FIXME: Move these tidbits into ConfigStrings so that the client can display a menu
#if 0
		gi.WriteByte(SV_CMD_CENTER_PRINT);
		gi.WriteString(welcome);
		gi.Unicast(ent, true);
#endif

		if (G_MatchIsTimeout()) { // joined during a match timeout
			ent->client->ps.pm_state.type = PM_FREEZE;
		}
	}

	// make sure all view stuff is valid
	G_ClientEndFrame(ent);
}

/**
 * @brief Set the hook style of the player, respecting server properties.
 */
void G_SetClientHookStyle(g_entity_t *ent) {

	if (!ent->in_use) {
		return;
	}

	g_hook_style_t hook_style;

	// respect user_info on default
	if (!g_strcmp0(g_hook_style->string, "default")) {
		hook_style = G_HookStyleByName(GetUserInfo(ent->client->locals.persistent.user_info, "hook_style"));
	} else {
		hook_style = G_HookStyleByName(g_hook_style->string);
	}

	ent->client->locals.persistent.hook_style = hook_style;
}

/**
 * @brief
 */
void G_ClientUserInfoChanged(g_entity_t *ent, const char *user_info) {
	char name[MAX_NET_NAME];

	// check for malformed or illegal info strings
	if (!ValidateUserInfo(user_info)) {
		printf("invalid\n");
		user_info = DEFAULT_USER_INFO;
	}

	// save off the user_info in case we want to check something later
	const size_t len = strlen(user_info);
	memmove(ent->client->locals.persistent.user_info, user_info, len + 1);

	G_Debug("%s\n", user_info);

	// set name, use a temp buffer to compute length and crutch up bad names
	const char *s = GetUserInfo(user_info, "name");

	g_strlcpy(name, s, sizeof(name));

	_Bool color = false;
	char *c = name;
	int32_t i = 0;

	// trim to 15 printable chars
	while (i < MAX_NET_NAME_PRINTABLE) {

		if (!*c) {
			break;
		}

		if (StrIsColor(c)) {
			color = true;
			c += 2;
			continue;
		}

		c++;
		i++;
	}
	name[c - name] = '\0';

	if (!i) { // name had nothing printable
		strcpy(name, "newbie");
	}

	if (color) { // reset to white
		strcat(name, "^7");
	}

	g_client_t *cl = ent->client;
	if (strncmp(cl->locals.persistent.net_name, name, sizeof(cl->locals.persistent.net_name))) {

		if (*cl->locals.persistent.net_name != '\0') {
			gi.BroadcastPrint(PRINT_MEDIUM, "%s changed name to %s\n", cl->locals.persistent.net_name,
			                  name);
		}

		g_strlcpy(cl->locals.persistent.net_name, name, sizeof(cl->locals.persistent.net_name));
	}

	const g_team_t *team = cl->locals.persistent.team;

	// set skin
	if (team) { // players must use team_skin to change
		s = GetUserInfo(user_info, "skin");

		char *p;
		if (strlen(s) && (p = strchr(s, '/'))) {
			*p = 0;
			s = va("%s/%s", s, DEFAULT_TEAM_SKIN);
		} else {
			s = va("%s/%s", DEFAULT_USER_MODEL, DEFAULT_TEAM_SKIN);
		}
	} else {
		s = GetUserInfo(user_info, "skin");
	}

	if (strlen(s) && !strstr(s, "..")) { // something valid-ish was provided
		g_strlcpy(cl->locals.persistent.skin, s, sizeof(cl->locals.persistent.skin));
	} else {
		g_strlcpy(cl->locals.persistent.skin, DEFAULT_USER_MODEL "/" DEFAULT_USER_SKIN, sizeof(cl->locals.persistent.skin));
	}

	// set effect color
	if (team) { // players must use team_skin to change
		cl->locals.persistent.color = team->color;
	} else {
		s = GetUserInfo(user_info, "color");

		cl->locals.persistent.color = -1;

		if (strlen(s) && strcmp(s, "default")) { // not default
			const int32_t hue = atoi(s);
			if (hue >= 0) {
				cl->locals.persistent.color = Minf(hue, 361);
			}
		}
	}

	// set shirt, pants and head colors

	cl->locals.persistent.shirt.a = 0;
	cl->locals.persistent.pants.a = 0;
	cl->locals.persistent.helmet.a = 0;

	if (team) {

		cl->locals.persistent.shirt = team->shirt;
		cl->locals.persistent.pants = team->pants;
		cl->locals.persistent.helmet = team->helmet;

	} else {

		s = GetUserInfo(user_info, "shirt");
		if (!Color_Parse(s, &cl->locals.persistent.shirt)) {
			cl->locals.persistent.shirt = color_white;
		}

		s = GetUserInfo(user_info, "pants");
		if (!Color_Parse(s, &cl->locals.persistent.pants)) {
			cl->locals.persistent.pants = color_white;
		}

		s = GetUserInfo(user_info, "helmet");
		if (!Color_Parse(s, &cl->locals.persistent.helmet)) {
			cl->locals.persistent.helmet = color_white;
		}
	}

	gchar client_info[MAX_USER_INFO_STRING] = { '\0' };

	// build the client info string
	g_strlcat(client_info, va("%d", team ? team->id : TEAM_NONE), sizeof(client_info));

	g_strlcat(client_info, "\\", sizeof(client_info));
	g_strlcat(client_info, cl->locals.persistent.net_name, sizeof(client_info));

	g_strlcat(client_info, "\\", sizeof(client_info));
	g_strlcat(client_info, cl->locals.persistent.skin, sizeof(client_info));

	g_strlcat(client_info, "\\", sizeof(client_info));
	g_strlcat(client_info, Color_Unparse(cl->locals.persistent.shirt), sizeof(client_info));

	g_strlcat(client_info, "\\", sizeof(client_info));
	g_strlcat(client_info, Color_Unparse(cl->locals.persistent.pants), sizeof(client_info));

	g_strlcat(client_info, "\\", sizeof(client_info));
	g_strlcat(client_info, Color_Unparse(cl->locals.persistent.helmet), sizeof(client_info));

	g_strlcat(client_info, "\\", sizeof(client_info));
	g_strlcat(client_info, va("%i", cl->locals.persistent.color), sizeof(client_info));

	// send it to clients
	const int32_t index = (int32_t) (ptrdiff_t) (cl - g_game.clients);
	gi.SetConfigString(CS_CLIENTS + index, client_info);

	// set hand, if anything should go wrong, it defaults to 0 (centered)
	cl->locals.persistent.hand = (g_hand_t) strtol(GetUserInfo(user_info, "hand"), NULL, 10);

	s = GetUserInfo(user_info, "active");
	if (g_strcmp0(s, "0") == 0) {
		ent->s.effects = ent->s.effects | EF_INACTIVE;
	} else {
		ent->s.effects &= ~(EF_INACTIVE);
	}

	// handicap
	uint16_t handicap = strtoul(GetUserInfo(user_info, "handicap"), NULL, 10);
	if (handicap == 0) {
		handicap = 100;
	}

	cl->locals.persistent.handicap_next = Clampf(handicap, 50, 100);

	// auto-switch
	uint16_t auto_switch = strtoul(GetUserInfo(user_info, "auto_switch"), NULL, 10);
	cl->locals.persistent.auto_switch = auto_switch;

	// hook style
	G_SetClientHookStyle(ent);

	// auto-spectate from "spectator" key
	if (strtoul(GetUserInfo(user_info, "spectator"), NULL, 10)) {
		ent->client->locals.persistent.spectator = true;
	}
}

/**
 * @brief Return the number of players in server. Used for CS_NUMCLIENTS.
 */
static uint8_t G_NumPlayers(void) {
	uint8_t slots = 0;

	for (int32_t i = 0; i < sv_max_clients->integer; i++) {
		g_entity_t *ent = &g_game.entities[i + 1];

		if (ent->client->connected) {
			slots++;
		}
	}

	return slots;
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
	if (strlen(g_password->string) && !ent->client->ai) {
		if (g_strcmp0(g_password->string, GetUserInfo(user_info, "password"))) {
			SetUserInfo(user_info, "rejmsg", "Password required or incorrect.");
			return false;
		}
	}

	// they can connect
	// clean up locals things which are not reset on spawns
	memset(&ent->client->locals.persistent, 0, sizeof(ent->client->locals.persistent));

	// set name, skin, etc..
	G_ClientUserInfoChanged(ent, user_info);

	if (sv_max_clients->integer > 1) {
		gi.BroadcastPrint(PRINT_HIGH, "%s connected\n", ent->client->locals.persistent.net_name);
	}

	ent->sv_flags = 0; // make sure we start with known default
	ent->client->connected = true;

	G_Ai_ClientConnect(ent); // tell AI a client has connected

	gi.SetConfigString(CS_NUMCLIENTS, va("%d", G_NumPlayers()));

	return true;
}

/**
 * @brief Called when a player drops from the server. Not be called between levels.
 */
void G_ClientDisconnect(g_entity_t *ent) {

	if (!ent->client) {
		return;
	}

	if (g_level.gameplay == GAME_DEATHMATCH || g_level.gameplay == GAME_DUEL) {
		G_TossQuadDamage(ent);
	}

	G_TossFlag(ent);
	G_TossTech(ent);
	G_ClientHookDetach(ent);

	gi.BroadcastPrint(PRINT_HIGH, "%s disconnected\n", ent->client->locals.persistent.net_name);

	// send effect
	if (G_IsMeat(ent)) {
		gi.WriteByte(SV_CMD_MUZZLE_FLASH);
		gi.WriteShort(ent->s.number);
		gi.WriteByte(MZ_LOGOUT);
		gi.Multicast(ent->s.origin, MULTICAST_PHS, NULL);
	}

	gi.UnlinkEntity(ent);

	ent->class_name = "disconnected";
	ent->in_use = false;
	ent->solid = SOLID_NOT;
	ent->sv_flags = SVF_NO_CLIENT;

	memset(&ent->s, 0, sizeof(ent->s));
	ent->s.number = ent - g_game.entities;

	memset(ent->client, 0, sizeof(g_client_t));

	const int32_t entity_num = (int32_t) (ptrdiff_t) (ent - g_game.entities - 1);
	gi.SetConfigString(CS_CLIENTS + entity_num, "");
	gi.SetConfigString(CS_NUMCLIENTS, va("%d", G_NumPlayers()));

	G_Ai_ClientDisconnect(ent); // tell AI
}

/**
 * @brief Ignore ourselves, clipping to the correct mask based on our status.
 */
static cm_trace_t G_ClientMove_Trace(const vec3_t start, const vec3_t end, const box3_t bounds) {
	const g_entity_t *self = g_level.current_entity;

	return gi.Trace(start, end, bounds, self, self->locals.clip_mask);
}

#ifdef _DEBUG
static bool g_recording_pmove = false;
static bool g_play_pmove = false;
static file_t *g_pmove_file;
static uint64_t pmove_frame = 0;
static uint64_t pmove_frames = 0;

void G_RecordPmove(void) {
	if (g_play_pmove) {
		return;
	}

	if (g_recording_pmove) {
		gi.CloseFile(g_pmove_file);
		g_recording_pmove = false;
		gi.Print("Closing pmove recording\n");
		return;
	}

	g_recording_pmove = true;
	g_pmove_file = gi.OpenFileWrite("pmove.deboog");
	gi.Print("Starting pmove recording\n");
}

void G_PlayPmove(void) {
	if (g_recording_pmove) {
		return;
	}

	if (g_play_pmove) {
		gi.CloseFile(g_pmove_file);
		g_play_pmove = false;
		gi.Print("Closing pmove recording\n");
		return;
	}

	g_play_pmove = true;
	pmove_frames = gi.LoadFile("pmove.deboog", NULL) / sizeof(pm_move_t);
	g_pmove_file = gi.OpenFile("pmove.deboog");
	gi.Print("Starting pmove playback\n");

	if (gi.Argc() > 1) {
		pmove_frame = strtoull(gi.Argv(1), NULL, 10);
		gi.SeekFile(g_pmove_file, sizeof(pm_move_t) * pmove_frame);
	} else {
		pmove_frame = 0;
	}
}
#endif

/**
 * @brief Process the movement command, call Pm_Move and act on the result.
 */
static void G_ClientMove(g_entity_t *ent, pm_cmd_t *cmd) {
	vec3_t old_velocity, velocity;
	pm_move_t pm;

	g_client_t *cl = ent->client;

	// save the raw angles sent over in the command
	cl->locals.cmd_angles = cmd->angles;

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

#ifdef _DEBUG
	if (g_play_pmove) {
		if (!gi.ReadFile(g_pmove_file, &pm, sizeof(pm), 1)) {
			g_play_pmove = false;
			gi.CloseFile(g_pmove_file);
			gi.Print("Finished pmove playback\n");
		} else {
			gi.Print("PMove frame %8" PRIu64 " / %8" PRIu64, pmove_frame, pmove_frames);
			pmove_frame++;
		}
	}

	if (!g_play_pmove) {
#endif
		pm.s = cl->ps.pm_state;

		pm.s.origin = ent->s.origin;

		if (ent->client->locals.hook_pull) {
		
			if (ent->client->locals.persistent.hook_style == HOOK_PULL) {
				pm.s.type = PM_HOOK_PULL;
			} else if (ent->client->locals.persistent.hook_style == HOOK_SWING_MANUAL) {
				pm.s.type = PM_HOOK_SWING_MANUAL;
			} else if (ent->client->locals.persistent.hook_style == HOOK_SWING_AUTO) {
				pm.s.type = PM_HOOK_SWING_AUTO;
			} else {
				pm.s.type = PM_HOOK_PULL;
			}
		} else {
			pm.s.velocity = ent->locals.velocity;
		}

		pm.cmd = *cmd;
		pm.ground = ent->locals.ground;
		pm.hook_pull_speed = g_hook_pull_speed->value;
#ifdef _DEBUG
	}
#endif

	pm.PointContents = gi.PointContents;
	pm.Trace = G_ClientMove_Trace;

	pm.Debug = gi.Debug_;
	pm.DebugMask = gi.DebugMask;
	pm.debug_mask = DEBUG_PMOVE_SERVER;

#ifdef _DEBUG
	if (g_recording_pmove) {
		gi.WriteFile(g_pmove_file, &pm, sizeof(pm), 1);
	}
#endif

	// perform a move
	Pm_Move(&pm);

	// save results of move
	cl->ps.pm_state = pm.s;

	old_velocity = ent->locals.velocity;

	ent->s.step_offset = roundf(pm.s.step_offset);
	ent->s.origin = pm.s.origin;
	ent->locals.velocity = pm.s.velocity;

	ent->bounds = pm.bounds;

	// copy the clamped angles out
	cl->locals.angles = pm.angles;

	// update the directional vectors based on new view angles
	Vec3_Vectors(cl->locals.angles, &cl->locals.forward, &cl->locals.right, &cl->locals.up);

	// update the horizontal speed scalar based on new velocity
	velocity = ent->locals.velocity;
	velocity.z = 0.0;

	velocity = Vec3_NormalizeLength(velocity, &cl->locals.speed);

	// blend animations for live players
	if (ent->locals.dead == false) {

		if (pm.s.flags & PMF_JUMPED) {
			if (g_level.time - 100 > cl->locals.jump_time) {
				vec3_t angles, forward, point;
				cm_trace_t tr;

				angles = Vec3(0.0, ent->s.angles.y, 0.0);
				Vec3_Vectors(angles, &forward, NULL, NULL);

				point = Vec3_Fmaf(ent->s.origin, cl->locals.speed * .4f, velocity);

				// trace towards our jump destination to see if we have room to backflip
				tr = gi.Trace(ent->s.origin, point, ent->bounds, ent, CONTENTS_MASK_CLIP_PLAYER);

				if (Vec3_Dot(velocity, forward) < -0.1 && tr.fraction == 1.0 && cl->locals.speed > 200.0) {
					G_SetAnimation(ent, ANIM_LEGS_JUMP2, true);
				} else {
					G_SetAnimation(ent, ANIM_LEGS_JUMP1, true);
				}

				// landing events take priority over jump events
				if (pm.water_level < WATER_UNDER && ent->s.event != EV_CLIENT_LAND) {
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

				if (G_IsAnimation(ent, ANIM_LEGS_JUMP2)) {
					G_SetAnimation(ent, ANIM_LEGS_LAND2, true);
				} else {
					G_SetAnimation(ent, ANIM_LEGS_LAND1, true);
				}

				if (old_velocity.z <= PM_SPEED_FALL) { // player will take damage
					int32_t damage = ((int32_t) - ((old_velocity.z - PM_SPEED_FALL) * 0.05));

					damage >>= pm.water_level; // water breaks the fall

					if (damage < 1) {
						damage = 1;
					}

					if (old_velocity.z <= PM_SPEED_FALL_FAR) {
						event = EV_CLIENT_FALL_FAR;
					} else {
						event = EV_CLIENT_FALL;
					}

					cl->locals.pain_time = g_level.time; // suppress pain sound
					
					// TODO: get normal from what we've landed on
					G_Damage(ent, NULL, NULL, Vec3_Up(), ent->s.origin, Vec3_Zero(), damage, 0, DMG_NO_ARMOR, MOD_FALLING);
				}

				ent->s.event = event;
				cl->locals.land_time = g_level.time;
			}
		} else if (pm.s.flags & PMF_ON_LADDER) {
			if (g_level.time - 400 > cl->locals.jump_time) {
				if (fabs(ent->locals.velocity.z) > 20.0) {

					G_SetAnimation(ent, ANIM_LEGS_JUMP1, true);

					ent->s.event = EV_CLIENT_JUMP;
					cl->locals.jump_time = g_level.time;
				}
			}
		}

		// detect hitting the ground to help with animation blending
		if (pm.ground.ent && !ent->locals.ground.ent) {
			cl->locals.ground_time = g_level.time;
		}
	}

	// copy ground and water state back into entity
	ent->locals.ground = pm.ground;
	ent->locals.water_level = pm.water_level;
	ent->locals.water_type = pm.water_type;

	// and finally link them back in to collide with others below
	gi.LinkEntity(ent);

	// touch every object we collided with objects
	if (ent->locals.move_type != MOVE_TYPE_NO_CLIP) {

		const cm_trace_t *touched = pm.touched;
		for (int32_t i = 0; i < pm.num_touched; i++, touched++) {
			g_entity_t *other = touched->ent;

			if (!other->locals.Touch) {
				continue;
			}

			other->locals.Touch(other, ent, touched);
		}

		G_TouchOccupy(ent);
	}
}

/**
 * @brief Expire any items which are time-sensitive.
 */
static void G_ClientInventoryThink(g_entity_t *ent) {

	if (ent->client->locals.inventory[g_media.items.powerups[POWERUP_QUAD]->index]) { // if they have quad

		if (ent->client->locals.quad_countdown_time && ent->client->locals.quad_countdown_time < g_level.time) { // play the countdown sound			
			G_MulticastSound(&(const g_play_sound_t) {
				.index = g_media.sounds.quad_expire,
				.entity = ent,
				.atten = SOUND_ATTEN_LINEAR
			}, MULTICAST_PHS, NULL);
			
			ent->client->locals.quad_countdown_time += 1000;
		}

		if (ent->client->locals.quad_damage_time < g_level.time) { // expire it

			ent->client->locals.quad_damage_time = 0.0;
			ent->client->locals.inventory[g_media.items.powerups[POWERUP_QUAD]->index] = 0;

			ent->s.effects &= ~EF_QUAD;
		}
	}

	// other power-ups and things can be timed out here as well

	if (ent->client->locals.respawn_protection_time > g_level.time) {
		ent->s.effects |= EF_RESPAWN;
	} else {
		ent->s.effects &= ~EF_RESPAWN;
	}

	// decrement any boosted health from items
	if (ent->client->locals.boost_time != 0 && ent->client->locals.boost_time < g_level.time) {
		if (ent->locals.health > ent->locals.max_health) {
			ent->locals.health -= 1;
			ent->client->locals.boost_time = g_level.time + 1000;
		} else {
			ent->client->locals.boost_time = 0;
		}
	}
}

/**
 * @brief This will be called once for each client frame, which will usually be a
 * couple times for each server frame.
 */
void G_ClientThink(g_entity_t *ent, pm_cmd_t *cmd) {

	if (g_level.intermission_time) {
		return;
	}

	if (g_level.match_status & MSTAT_TIMEOUT) {
		return;
	}

	g_level.current_entity = ent;
	g_client_t *cl = ent->client;

	if (cl->locals.chase_target) { // ensure chase is valid

		if (!G_IsMeat(cl->locals.chase_target)) {

			const g_entity_t *other = cl->locals.chase_target;

			G_ClientChaseNext(ent);

			if (cl->locals.chase_target == other) { // no one to chase
				cl->locals.chase_target = NULL;
			}
		}
	}

	// setup buttons now, since pmove won't modify them
	cl->locals.old_buttons = cl->locals.buttons;
	cl->locals.buttons = cmd->buttons;
	cl->locals.latched_buttons |= cl->locals.buttons & ~cl->locals.old_buttons;

	if (!cl->locals.chase_target) { // move through the world

		// process hook buttons
		if (cl->locals.hook_think_time < g_level.time) {
			G_ClientHookThink(ent, false);
		}

		G_ClientMove(ent, cmd);
	}

	cl->locals.cmd = *cmd;

	// fire weapon if requested
	if (cl->locals.latched_buttons & BUTTON_ATTACK) {
		if (cl->locals.persistent.spectator) {

			cl->locals.latched_buttons = 0;

			if (cl->locals.chase_target) { // toggle chase camera
				cl->locals.chase_target = cl->locals.old_chase_target = NULL;
			} else {
				G_ClientChaseTarget(ent);
			}

			G_ClientChaseThink(ent);
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

	// if we're the first player in a game, send our client over
	// to the AI system in case it needs to make nodes
	if (aix && ent->s.number == 1) {
		aix->PlayerRoam(ent, cmd);
	}
}

/**
 * @brief This will be called once for each server frame, before running
 * any other entities in the world.
 */
void G_ClientBeginFrame(g_entity_t *ent) {

	if (g_level.intermission_time) {
		return;
	}

	if ((G_IsMeat(ent) && ent->locals.dead) ||
			((ent->client->locals.buttons | ent->client->locals.latched_buttons) & BUTTON_SCORE)) {
		ent->client->locals.show_scores = true;
	} else {
		ent->client->locals.show_scores = false;
	}

	g_client_t *cl = ent->client;

	// run weapon think if it hasn't been done by a command
	if (cl->locals.weapon_think_time < g_level.time) {
		G_ClientWeaponThink(ent);
	}

	if (cl->locals.hook_think_time < g_level.time) {
		G_ClientHookThink(ent, false);
	}

	if (ent->locals.dead) {

		// rounds mode implies last-man-standing, force to spectator immediately if round underway
		if (g_level.rounds && g_level.round_time && g_level.time >= g_level.round_time) {

			if (!cl->locals.persistent.spectator) {
				cl->locals.persistent.spectator = true;
				G_ClientRespawn(ent, false);
			}
		} else if (g_level.time > cl->locals.respawn_time) {

			// all other respawns require a click from the player
			if (cl->locals.latched_buttons & BUTTON_ATTACK) {
				G_ClientRespawn(ent, false);
			}
		}
	} else {

		if (G_HasTech(ent, TECH_REGEN)) {

			if (ent->client->locals.regen_time < g_level.time) {
				ent->client->locals.regen_time = g_level.time + TECH_REGEN_TICK_TIME;

				if (ent->locals.health < ent->locals.max_health) {
					ent->locals.health = Minf(ent->locals.health + TECH_REGEN_HEALTH, ent->locals.max_health);
					G_PlayTechSound(ent);
				}
			}
		}
	}

	cl->locals.latched_buttons = 0;
}
