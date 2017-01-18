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

cvar_t *g_ai_fill_slots;

/**
 * @brief MAYBE TEMPORARY
 */
static uint16_t G_Ai_ItemIndex(const g_item_t *item) {
	return item - g_items;
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

	self->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief
 */
void G_Ai_SetClientLocals(g_client_t *client) {

	client->ai_locals.inventory = client->locals.inventory;
	client->ai_locals.angles = client->locals.angles;
	client->ai_locals.weapon = &client->locals.weapon;
}

/**
 * @brief
 */
void G_Ai_SetEntityLocals(g_entity_t *ent) {
	
	ent->ai_locals.ground_entity = &ent->locals.ground_entity;
	ent->ai_locals.item = &ent->locals.item;
	ent->ai_locals.velocity = ent->locals.velocity;
	ent->ai_locals.health = &ent->locals.health;
	ent->ai_locals.max_health = &ent->locals.max_health;
	ent->ai_locals.max_armor = &ent->locals.max_armor;
	ent->ai_locals.water_level = &ent->locals.water_level;
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

	G_ClientConnect(self, userinfo);

	self->ai = true; // and away we go!
	
	if (!time_offset) {
		G_Ai_ClientBegin(self);
	} else {
		self->in_use = true;
		self->locals.move_type = MOVE_TYPE_THINK;
		self->locals.Think = G_Ai_ClientBegin;
		self->locals.next_think = g_level.time + time_offset;
	}
}

/**
 * @brief Calculate the number of empty client slots.
 */
static uint8_t G_Ai_EmptySlots(void) {
	uint8_t empty_slots = 0;

	for (int32_t i = 0; i < sv_max_clients->integer; i++) {

		g_entity_t *ent = &g_game.entities[i + 1];

		if (!ent->in_use) {
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

		if (ent->in_use && ent->ai) {
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
	int32_t clamped = Min(G_Ai_NumberOfBots(), sv_max_clients->integer);

	if (!clamped) {
		return;
	}
	
	while (clamped) {
		g_entity_t *ent = &g_game.entities[1];
		int32_t j;

		for (j = 1; j <= sv_max_clients->integer; j++, ent++) {
			if (ent->in_use && ent->ai) {
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

	// decrement bots waiting to spawn
	if (g_game.ai_left_to_spawn) {
		g_game.ai_left_to_spawn--;
	}
}

/**
 * @brief
 */
void G_Ai_ClientDisconnect(g_entity_t *ent) {

	if (!aix) {
		return;
	}

	// see if this opened up a slot for a bot
	uint8_t empty_slots = G_Ai_EmptySlots();

	if (empty_slots && G_Ai_NumberOfClients() < g_game.ai_fill_slots) {
		g_game.ai_left_to_spawn++;
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

	if (g_ai_fill_slots->modified) {
		g_ai_fill_slots->modified = false;

		if (g_ai_fill_slots->integer == -1) {
			g_ai_fill_slots->integer = sv_max_clients->integer;
		} else {
			g_game.ai_fill_slots = Clamp(g_ai_fill_slots->integer, 0, sv_max_clients->integer);
		}

		int32_t slot_diff = g_ai_fill_slots->integer - G_Ai_NumberOfClients();

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
			if (!ent->in_use) {
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
	}

	ai_item.name = item->name;
	ai_item.priority = item->priority;
	ai_item.quantity = item->quantity;
	ai_item.tag = item->tag;
	ai_item.max = item->max;

	ai_item.ammo = 0;
	ai_item.speed = 0;
	ai_item.time = 0;

	aix->RegisterItem(G_Ai_ItemIndex(item), &ai_item);
}

/**
 * @brief
 */
static void G_Ai_RegisterWeapon(const g_item_t *item, const ai_item_flags_t weapon_flags, const int32_t speed, const uint32_t time) {

	if (!item || item->type != ITEM_WEAPON) {
		gi.Warn("Invalid item registration\n");
		return;
	}

	ai_item_t ai_item;
	
	ai_item.class_name = item->class_name;
	if (item->ammo) {
		const g_item_t *ammo = G_FindItem(item->ammo);
		ai_item.ammo = G_Ai_ItemIndex(ammo);
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

	aix->RegisterItem(G_Ai_ItemIndex(item), &ai_item);
}

/**
 * @brief 
 */
void G_Ai_RegisterItems(void) {

	if (!aix) {
		return;
	}

	for (uint16_t i = 0; i < g_num_items; i++) {

		if (g_items[i].type == ITEM_WEAPON) {
			continue;
		}

		G_Ai_RegisterItem(&g_items[i]);
	}
	
	G_Ai_RegisterWeapon(G_FindItem("Blaster"), AI_WEAPON_PROJECTILE, 1000, 0);
	G_Ai_RegisterWeapon(G_FindItem("Shotgun"), AI_WEAPON_HITSCAN | AI_WEAPON_SHORT_RANGE | AI_WEAPON_MED_RANGE, 0, 0);
	G_Ai_RegisterWeapon(G_FindItem("Super Shotgun"), AI_WEAPON_HITSCAN | AI_WEAPON_SHORT_RANGE, 0, 0);
	G_Ai_RegisterWeapon(G_FindItem("Machinegun"), AI_WEAPON_HITSCAN | AI_WEAPON_SHORT_RANGE | AI_WEAPON_MED_RANGE, 0, 0);
	G_Ai_RegisterWeapon(G_FindItem("Grenade Launcher"), AI_WEAPON_PROJECTILE | AI_WEAPON_EXPLOSIVE, 700, 0);
	G_Ai_RegisterWeapon(G_FindItem("Hand Grenades"), AI_WEAPON_PROJECTILE | AI_WEAPON_EXPLOSIVE | AI_WEAPON_TIMED | AI_WEAPON_MED_RANGE, 1000, 3000);
	G_Ai_RegisterWeapon(G_FindItem("Rocket Launcher"), AI_WEAPON_PROJECTILE | AI_WEAPON_EXPLOSIVE | AI_WEAPON_MED_RANGE | AI_WEAPON_LONG_RANGE, 1000, 0);
	G_Ai_RegisterWeapon(G_FindItem("Hyperblaster"), AI_WEAPON_PROJECTILE | AI_WEAPON_MED_RANGE, 1800, 0);
	G_Ai_RegisterWeapon(G_FindItem("Lightning Gun"), AI_WEAPON_HITSCAN | AI_WEAPON_SHORT_RANGE, 0, 0);
	G_Ai_RegisterWeapon(G_FindItem("Railgun"), AI_WEAPON_HITSCAN | AI_WEAPON_LONG_RANGE, 0, 0);
	G_Ai_RegisterWeapon(G_FindItem("BFG10K"), AI_WEAPON_PROJECTILE | AI_WEAPON_MED_RANGE | AI_WEAPON_LONG_RANGE, 720, 0);
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

	// SCRATCH
	import.entities = ge.entities;
	import.entity_size = ge.entity_size;
	import.ItemIndex = G_Ai_ItemIndex;
	
	ai_export_t *exports = gi.LoadAi(&import);

	if (!exports) {
		return;
	}

	aix = exports;
	
	g_ai_fill_slots = gi.Cvar("g_ai_fill_slots", "0", CVAR_SERVER_INFO, "The number of bots to automatically fill in empty slots in the server. Specify -1 to fill all available slots.");

	gi.Cmd("g_ai_add", G_Ai_Add_f, CMD_GAME, "Add one or more AI to the game");
	gi.Cmd("g_ai_remove", G_Ai_Remove_f, CMD_GAME, "Remove one or more AI from the game");
}

/**
 * @brief
 */
void G_Ai_Shutdown(void) {

}
