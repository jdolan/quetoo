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
#include "ai/default/ai_types.h"

cvar_t *g_ai_max_clients;

/**
 * @brief Calculate the number of empty client slots.
 */
static uint8_t G_Ai_EmptySlots(void) {
	uint8_t empty_slots = 0;

	for (int32_t i = 0; i < sv_max_clients->integer; i++) {

		g_entity_t *ent = &g_game.entities[i + 1];

		if (!ent->in_use && !ent->client->connected) {
			empty_slots++;
		}
	}

	return empty_slots;
}

/**
 * @brief Calculate the number of bots.
 */
static uint8_t G_Ai_NumberOfBots(void) {
	uint8_t filled_slots = 0;

	for (int32_t i = 0; i < sv_max_clients->integer; i++) {

		g_entity_t *ent = &g_game.entities[i + 1];

		if (ent->in_use && ent->client->connected && ent->client->ai) {
			filled_slots++;
		}
	}

	return filled_slots;
}

/**
 * @brief Calculate the number of connected clients (bots and players alike).
 */
static uint8_t G_Ai_NumberOfClients(void) {
	uint8_t filled_slots = 0;

	for (int32_t i = 0; i < sv_max_clients->integer; i++) {

		g_entity_t *ent = &g_game.entities[i + 1];

		if (ent->in_use) {
			filled_slots++;
		}
	}

	return filled_slots;
}

/**
 * @brief MAYBE TEMPORARY
 */
static uint16_t G_Ai_ItemIndex(const g_item_t *item) {
	return item->index;
}

/**
 * @brief
 */
static _Bool G_Ai_CanPickupItem(const g_entity_t *self, const g_entity_t *other) {
	const g_item_t *item = other->locals.item;

	if (!item) {
		return false;
	}

	if (item->type == ITEM_HEALTH) {
		// stimpack/mega is always gettable
		if (item->tag == HEALTH_SMALL || item->tag == HEALTH_MEGA) {
			return true;
		}

		return self->locals.health < self->locals.max_health;
	} else if (item->type == ITEM_ARMOR) {
		const g_item_t *current_armor = G_ClientArmor(self);

		// no armor or shard or not filled up, can get.
		if (!current_armor ||
		        item->tag == ARMOR_SHARD ||
		        self->client->locals.inventory[current_armor->index] < current_armor->max) {
			return true;
		}

		return false;
	} else if (item->type == ITEM_AMMO) { // just if we need the ammo
		return self->client->locals.inventory[item->index] < item->max;
	} else if (item->type == ITEM_WEAPON) {

		if (self->client->locals.inventory[item->index]) { // we have the weapon

			if (item->ammo) {
				const g_item_t *ammo = item->ammo_item;
				return self->client->locals.inventory[ammo->index] < ammo->max;
			}

			return false;
		}

		return true;
	} else if (item->type == ITEM_TECH) {

		if (G_CarryingTech(self)) {
			return false;
		}

		return true;
	} else if (item->type == ITEM_FLAG) {

		g_team_t *team = G_TeamForFlag(other);

		// if it's our flag, recover it if dropped, or tag it if carrying enemy flag
		if (team == self->client->locals.persistent.team) {
			return (other->locals.spawn_flags & SF_ITEM_DROPPED) || G_IsFlagBearer(self);
		}

		// otherwise, only if we don't have a flag
		return !G_IsFlagBearer(self);
	}

	return true;
}

/**
 * @brief
 */
static void G_Ai_ClientThink(g_entity_t *self) {
	pm_cmd_t cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.msec = QUETOO_TICK_MILLIS;

	aix->Think(self, &cmd);
	G_ClientThink(self, &cmd);

	// see if we're in a match and need to join
	if (self->client->locals.persistent.spectator) {

		if (g_level.match || g_level.rounds) {

			// see if we can join
			if (!(g_level.time - self->client->locals.respawn_time < 3000) &&
				!(g_level.match_time) &&
				!(g_level.round_time)) {

				if (g_level.teams || g_level.ctf) {
					G_AddClientToTeam(self, G_SmallestTeam()->name);
				} else {
					gi.TokenizeString("spectate");
					G_ClientCommand(self);
				}
			}
		} else {
			gi.Print("AI was stuck in spectator\n");
			G_ClientDisconnect(self);
			return;
		}
	} else {

		// ready up
		if ((g_level.match || g_level.rounds) && !self->client->locals.persistent.ready) {

			// only if all clients are ready
			uint8_t num_players = 0;
			uint8_t ready_slots = 0;

			for (int32_t i = 0; i < sv_max_clients->integer; i++) {

				g_entity_t *ent = &g_game.entities[i + 1];

				if (ent->in_use && ent->client->connected && !ent->client->ai && !ent->client->locals.persistent.spectator) {
					num_players++;

					if (ent->client->locals.persistent.ready) {
						ready_slots++;
					}
				}
			}

			if (ready_slots == num_players) {
				gi.TokenizeString("ready");
				G_ClientCommand(self);
			}
		}
	}

	self->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief
 */
static void G_Ai_ClientBegin(g_entity_t *self) {

	G_ClientBegin(self);

	aix->Begin(self);

	gi.Debug("Spawned %s at %s", self->client->locals.persistent.net_name, vtos(self->s.origin));

	self->locals.Think = G_Ai_ClientThink;
	self->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief
 */
static void G_Ai_Spawn(g_entity_t *self, const uint32_t time_offset) {

	char userinfo[MAX_USER_INFO_STRING];
	aix->GetUserInfo(self, userinfo);

	self->client->ai = true; // and away we go!

	G_ClientConnect(self, userinfo);

	if (!time_offset) {
		G_Ai_ClientBegin(self);
	} else {
		self->locals.Think = G_Ai_ClientBegin;
		self->locals.next_think = g_level.time + time_offset;
	}

	g_game.ai_left_to_spawn--;
}

static void G_Ai_AddBots(const int32_t count) {
	int32_t empty_slots = G_Ai_EmptySlots();

	if (!empty_slots) {
		gi.Print("No client slots available, increase sv_max_clients.\n");
		return;
	}

	g_game.ai_left_to_spawn += Min(count, empty_slots);
}

/**
 * @brief
 */
static void G_Ai_RemoveBots(const int32_t count) {
	int32_t clamped = Min(count, G_Ai_NumberOfBots());

	if (!clamped) {
		return;
	}

	while (clamped) {
		g_entity_t *ent = &g_game.entities[1];
		int32_t j;

		for (j = 1; j <= sv_max_clients->integer; j++, ent++) {
			if (ent->in_use && ent->client->connected && ent->client->ai) {
				G_ClientDisconnect(ent);
				break;
			}
		}

		clamped--;
	}
}

/**
 * @brief
 */
static void G_Ai_Add_f(void) {

	if (g_game.ai_fill_slots) {
		gi.Print("g_ai_max_clients is set - change that instead\n");
		return;
	}

	int32_t count = 1;

	if (gi.Argc() > 1) {
		count = atoi(gi.Argv(1));
	}

	G_Ai_AddBots(count);
}

/**
 * @brief
 */
static void G_Ai_Remove_f(void) {

	if (g_game.ai_fill_slots) {
		gi.Print("g_ai_max_clients is set - change that instead\n");
		return;
	}

	int32_t count = 1;

	if (gi.Argc() > 1) {
		count = atoi(gi.Argv(1));
	}

	G_Ai_RemoveBots(count);
}

/**
 * @brief
 */
void G_Ai_ClientConnect(g_entity_t *ent) {

	if (!aix) {
		return;
	}

	// see if this overfilled the server
	if (g_game.ai_fill_slots) {

		uint8_t num_bots = G_Ai_NumberOfBots();
		uint8_t num_clients = G_Ai_NumberOfClients();

		if (num_bots && num_clients > g_game.ai_fill_slots) {
			G_Ai_RemoveBots(1);
		}
	}
}

/**
 * @brief
 */
void G_Ai_ClientDisconnect(g_entity_t *ent) {

	if (!aix) {
		return;
	}

	if (g_game.ai_fill_slots) {
		// see if this opened up a slot for a bot
		uint8_t empty_slots = G_Ai_EmptySlots();

		if (empty_slots && G_Ai_NumberOfClients() < g_game.ai_fill_slots) {
			g_game.ai_left_to_spawn++;
		}
	}
}

/**
 * @brief
 */
void G_Ai_Frame(void) {

	if (!aix) {
		return;
	}

	// don't run bots for a few frames so that the game has settled
	if (g_level.frame_num <= 5) {
		return;
	}

	if (g_ai_max_clients->modified) {
		g_ai_max_clients->modified = false;

		if (g_ai_max_clients->integer == -1) {
			g_ai_max_clients->integer = sv_max_clients->integer;
		} else {
			g_game.ai_fill_slots = Clamp(g_ai_max_clients->integer, 0, sv_max_clients->integer);
		}

		int32_t slot_diff = g_ai_max_clients->integer - G_Ai_NumberOfClients();

		if (slot_diff > 0) {
			G_Ai_AddBots(slot_diff);
		} else if (slot_diff < 0) {
			G_Ai_RemoveBots(abs(slot_diff));
		}
	}

	// spawn as many bots as we have left to go
	while (g_game.ai_left_to_spawn) {
		g_entity_t *ent = &g_game.entities[1];
		int32_t j;

		for (j = 1; j <= sv_max_clients->integer; j++, ent++) {
			if (!ent->in_use && !ent->client->connected) {
				G_Ai_Spawn(ent, (g_game.ai_left_to_spawn - 1) * 500);
				break;
			}
		}

		if (j > sv_max_clients->integer) { // should never happen
			gi.Print("Desync in ai_left_to_spawn?\n");
			g_game.ai_left_to_spawn = 0;
			break;
		}
	}

	// run AI think functions
	g_entity_t *ent = &g_game.entities[1];
	for (uint16_t i = 1; i <= sv_max_clients->integer; i++, ent++) {

		if (!ent->client->connected) {
			continue;
		}

		if (ent->client->ai) {
			G_RunThink(ent);
		}
	}

	aix->Frame();
}

/**
 * @brief
 */
static void G_Ai_RegisterItem(const g_item_t *item) {

	if (!item || item->type == ITEM_WEAPON) {
		gi.Warn("Invalid item registration\n");
		return;
	}

	ai_item_t ai_item;

	ai_item.class_name = item->class_name;

	switch (item->type) {
		default:
			gi.Warn("Invalid item registration\n");
			break;
		case ITEM_AMMO:
			ai_item.flags = AI_ITEM_AMMO;
			break;
		case ITEM_ARMOR:
			ai_item.flags = AI_ITEM_ARMOR;
			break;
		case ITEM_FLAG:
			ai_item.flags = AI_ITEM_FLAG;
			break;
		case ITEM_HEALTH:
			ai_item.flags = AI_ITEM_HEALTH;
			break;
		case ITEM_POWERUP:
			ai_item.flags = AI_ITEM_POWERUP;
			break;
		case ITEM_TECH:
			ai_item.flags = AI_ITEM_TECH;
			break;
	}

	ai_item.name = item->name;
	ai_item.priority = item->priority;
	ai_item.quantity = item->quantity;
	ai_item.tag = item->tag;
	ai_item.max = item->max;

	ai_item.ammo = 0;
	ai_item.speed = 0;
	ai_item.time = 0;

	aix->RegisterItem(item->index, &ai_item);
}

/**
 * @brief
 */
static void G_Ai_RegisterWeapon(const g_item_t *item, const ai_item_flags_t weapon_flags, const int32_t speed,
                                const uint32_t time) {

	if (!item || item->type != ITEM_WEAPON) {
		gi.Warn("Invalid item registration\n");
		return;
	}

	ai_item_t ai_item;

	ai_item.class_name = item->class_name;
	if (item->ammo) {
		const g_item_t *ammo = item->ammo_item;
		ai_item.ammo = ammo->index;
		ai_item.max = ammo->max;
	} else {
		ai_item.ammo = 0;
	}
	ai_item.flags = AI_ITEM_WEAPON | weapon_flags;

	ai_item.name = item->name;
	ai_item.priority = item->priority;
	ai_item.quantity = item->quantity;
	ai_item.tag = item->tag;

	ai_item.speed = speed;
	ai_item.time = time;

	aix->RegisterItem(item->index, &ai_item);
}

/**
 * @brief
 */
void G_Ai_RegisterItems(void) {

	if (!aix) {
		return;
	}

	for (uint16_t i = 0; i < g_num_items; i++) {

		const g_item_t *item = G_ItemByIndex(i);

		if (item->type == ITEM_WEAPON) { // items are registered below
			continue;
		}

		G_Ai_RegisterItem(item);
	}

	G_Ai_RegisterWeapon(g_media.items.weapons[WEAPON_BLASTER], AI_WEAPON_PROJECTILE, 1000, 0);
	G_Ai_RegisterWeapon(g_media.items.weapons[WEAPON_SHOTGUN], AI_WEAPON_HITSCAN | AI_WEAPON_SHORT_RANGE | AI_WEAPON_MED_RANGE, 0, 0);
	G_Ai_RegisterWeapon(g_media.items.weapons[WEAPON_SUPER_SHOTGUN], AI_WEAPON_HITSCAN | AI_WEAPON_SHORT_RANGE, 0, 0);
	G_Ai_RegisterWeapon(g_media.items.weapons[WEAPON_MACHINEGUN], AI_WEAPON_HITSCAN | AI_WEAPON_SHORT_RANGE | AI_WEAPON_MED_RANGE, 0, 0);
	G_Ai_RegisterWeapon(g_media.items.weapons[WEAPON_GRENADE_LAUNCHER], AI_WEAPON_PROJECTILE | AI_WEAPON_EXPLOSIVE, 700, 0);
	G_Ai_RegisterWeapon(g_media.items.weapons[WEAPON_HAND_GRENADE],
	                    AI_WEAPON_PROJECTILE | AI_WEAPON_EXPLOSIVE | AI_WEAPON_TIMED | AI_WEAPON_MED_RANGE, 1000, 3000);
	G_Ai_RegisterWeapon(g_media.items.weapons[WEAPON_ROCKET_LAUNCHER],
	                    AI_WEAPON_PROJECTILE | AI_WEAPON_EXPLOSIVE | AI_WEAPON_MED_RANGE | AI_WEAPON_LONG_RANGE, 1000, 0);
	G_Ai_RegisterWeapon(g_media.items.weapons[WEAPON_HYPERBLASTER], AI_WEAPON_PROJECTILE | AI_WEAPON_MED_RANGE, 1800, 0);
	G_Ai_RegisterWeapon(g_media.items.weapons[WEAPON_LIGHTNING], AI_WEAPON_HITSCAN | AI_WEAPON_SHORT_RANGE, 0, 0);
	G_Ai_RegisterWeapon(g_media.items.weapons[WEAPON_RAILGUN], AI_WEAPON_HITSCAN | AI_WEAPON_LONG_RANGE, 0, 0);
	G_Ai_RegisterWeapon(g_media.items.weapons[WEAPON_BFG10K], AI_WEAPON_PROJECTILE | AI_WEAPON_MED_RANGE | AI_WEAPON_LONG_RANGE, 720, 0);
}

#define ENTITY_PTR_OFFSET(m) \
			entity.m = (typeof(entity.m)) offsetof(g_entity_locals_t, m)

#define CLIENT_PTR_OFFSET(m) \
			client.m = (typeof(client.m)) offsetof(g_client_locals_t, m)

/**
 * @brief
 */
static void G_Ai_SetDataPointers(void) {
	static ai_entity_data_t entity;
	static ai_client_data_t client;

	ENTITY_PTR_OFFSET(ground_entity);
	ENTITY_PTR_OFFSET(item);
	ENTITY_PTR_OFFSET(velocity);
	ENTITY_PTR_OFFSET(health);
	ENTITY_PTR_OFFSET(max_health);
	ENTITY_PTR_OFFSET(max_armor);
	ENTITY_PTR_OFFSET(water_level);

	CLIENT_PTR_OFFSET(angles);
	CLIENT_PTR_OFFSET(inventory);
	CLIENT_PTR_OFFSET(weapon);

	aix->SetDataPointers(&entity, &client);
}

/**
 * @brief
 */
void G_Ai_Init(void) {

	ai_import_t import;

	memset(&import, 0, sizeof(import));

	g_strlcpy(import.write_dir, gi.write_dir, MAX_OS_PATH);

	import.Print = gi.Print;
	import.Debug_ = gi.Debug_;
	import.PmDebug_ = gi.PmDebug_;
	import.Warn_ = gi.Warn_;
	import.Error_ = gi.Error_;

	import.Malloc = gi.Malloc;
	import.LinkMalloc = gi.LinkMalloc;
	import.Free = gi.Free;
	import.FreeTag = gi.FreeTag;

	import.LoadFile = gi.LoadFile;
	import.FreeFile = gi.FreeFile;
	import.EnumerateFiles = gi.EnumerateFiles;

	import.Cvar = gi.Cvar;
	import.CvarString = gi.CvarString;
	import.CvarValue = gi.CvarValue;
	import.CvarGet = gi.CvarGet;
	import.CvarSet = gi.CvarSet;
	import.CvarSetValue = gi.CvarSetValue;
	import.Cmd = gi.Cmd;
	import.Argc = gi.Argc;
	import.Argv = gi.Argv;
	import.Args = gi.Args;
	import.TokenizeString = gi.TokenizeString;

	import.Trace = gi.Trace;
	import.PointContents = gi.PointContents;
	import.inPVS = gi.inPVS;
	import.inPHS = gi.inPHS;
	import.AreasConnected = gi.AreasConnected;

	import.BoxEntities = gi.BoxEntities;

	import.GetConfigString = gi.GetConfigString;

	import.OnSameTeam = G_OnSameTeam;
	import.ClientCommand = G_ClientCommand;

	import.Multicast = gi.Multicast;
	import.Unicast = gi.Unicast;
	import.WriteData = gi.WriteData;
	import.WriteChar = gi.WriteChar;
	import.WriteByte = gi.WriteByte;
	import.WriteShort = gi.WriteShort;
	import.WriteLong = gi.WriteLong;
	import.WriteString = gi.WriteString;
	import.WriteVector = gi.WriteVector;
	import.WritePosition = gi.WritePosition;
	import.WriteDir = gi.WriteDir;
	import.WriteAngle = gi.WriteAngle;
	import.WriteAngles = gi.WriteAngles;

	// SCRATCH
	import.entities = ge.entities;
	import.entity_size = ge.entity_size;
	import.ItemIndex = G_Ai_ItemIndex;
	import.CanPickupItem = G_Ai_CanPickupItem;

	ai_export_t *exports = gi.LoadAi(&import);

	if (!exports) {
		return;
	}

	aix = exports;

	G_Ai_SetDataPointers();

	g_ai_max_clients = gi.Cvar("g_ai_max_clients", "0", CVAR_SERVER_INFO,
	                           "The minimum amount player slots that will always be filled. Specify -1 to fill all available slots.");

	gi.Cmd("g_ai_add", G_Ai_Add_f, CMD_GAME, "Add one or more AI to the game");
	gi.Cmd("g_ai_remove", G_Ai_Remove_f, CMD_GAME, "Remove one or more AI from the game");
}

/**
 * @brief
 */
void G_Ai_Shutdown(void) {

}
