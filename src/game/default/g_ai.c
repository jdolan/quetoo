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
void G_Ai_RegisterItems(void) {

	if (!aix) {
		return;
	}

	for (uint16_t i = 0; i < g_num_items; i++) {
		aix->RegisterItem(G_ItemByIndex(i));
	}
}

#define ENTITY_PTR_OFFSET(m) \
	entity.m = (typeof(entity.m)) offsetof(g_entity_locals_t, m)

#define CLIENT_PTR_OFFSET(m) \
	client.m = (typeof(client.m)) offsetof(g_client_locals_t, m)

#define ITEM_PTR_OFFSET(m) \
	item.m = (typeof(item.m)) offsetof(g_item_t, m)

/**
 * @brief
 */
static void G_Ai_SetDataPointers(void) {
	static ai_entity_data_t entity;
	static ai_client_data_t client;
	static ai_item_data_t item;

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

	ITEM_PTR_OFFSET(class_name);
	ITEM_PTR_OFFSET(index);
	ITEM_PTR_OFFSET(type);
	ITEM_PTR_OFFSET(tag);
	ITEM_PTR_OFFSET(flags);
	ITEM_PTR_OFFSET(name);
	ITEM_PTR_OFFSET(ammo);
	ITEM_PTR_OFFSET(quantity);
	ITEM_PTR_OFFSET(max);
	ITEM_PTR_OFFSET(priority);

	aix->SetDataPointers(&entity, &client, &item);
}

/**
 * @brief
 */
void G_Ai_Init(void) {

	ai_import_t import;

	memset(&import, 0, sizeof(import));

	import.gi = &gi;
	import.ge = &ge;

	import.OnSameTeam = G_OnSameTeam;

	// SCRATCH
	import.CanPickupItem = G_Ai_CanPickupItem;

	aix = gi.LoadAi(&import);

	G_Ai_SetDataPointers();

	g_ai_max_clients = gi.AddCvar("g_ai_max_clients", "0", CVAR_SERVER_INFO,
								  "The minimum amount player slots that will always be filled. Specify -1 to fill all available slots.");

	gi.AddCmd("g_ai_add", G_Ai_Add_f, CMD_GAME, "Add one or more AI to the game");
	gi.AddCmd("g_ai_remove", G_Ai_Remove_f, CMD_GAME, "Remove one or more AI from the game");
}

/**
 * @brief
 */
void G_Ai_Shutdown(void) {

}
