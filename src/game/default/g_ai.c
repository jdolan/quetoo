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

/**
 * @brief
 */
static void G_Ai_ClientThink(g_entity_t *self) {
	pm_cmd_t cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.msec = QUETOO_TICK_MILLIS;

	aie.Think(self, &cmd);
	G_ClientThink(self, &cmd);

	self->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief
 */
static void G_Ai_ClientBegin(g_entity_t *self) {

	G_ClientBegin(self);

	aie.Begin(self);

	gi.Debug("Spawned %s at %s", self->client->locals.persistent.net_name, vtos(self->s.origin));

	self->locals.Think = G_Ai_ClientThink;
	self->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief
 */
static void G_Ai_Spawn(g_entity_t *self, const uint32_t time_offset) {

	self->ai = true; // and away we go!

	char userinfo[MAX_USER_INFO_STRING];
	aie.GetUserInfo(self, userinfo);

	G_ClientConnect(self, userinfo);
	
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
 * @brief
 */
static void G_Ai_Add_f(void) {
	int32_t i, count = 1;

	if (gi.Argc() > 1) {
		count = Clamp(atoi(gi.Argv(1)), 1, sv_max_clients->integer);
	}

	for (i = 0; i < count; i++) {

		g_entity_t *ent = &g_game.entities[1];
		int32_t j;

		for (j = 1; j <= sv_max_clients->integer; j++, ent++) {
			if (!ent->in_use) {
				G_Ai_Spawn(ent, i * (QUETOO_TICK_MILLIS * 20.0));
				break;
			}
		}

		if (j > sv_max_clients->integer) {
			gi.Print("No client slots available, increase sv_max_clients\n");
			break;
		}
	}
}

/**
 * @brief
 */
static void G_Ai_Remove_f(void) {
	int32_t i, count = 1;

	if (gi.Argc() > 1) {
		count = Clamp(atoi(gi.Argv(1)), 1, sv_max_clients->integer);
	}
	
	for (i = 0; i < count; i++) {

		g_entity_t *ent = &g_game.entities[1];
		int32_t j;

		for (j = 1; j <= sv_max_clients->integer; j++, ent++) {
			if (ent->in_use && ent->ai) {
				G_ClientDisconnect(ent);
				break;
			}
		}
	}
}

/**
 * @brief Temporary, until we figure out how to expose this
 */
static const g_entity_t *G_Ai_GroundEntity(const g_entity_t *self) {
	return self->locals.ground_entity;
}

/**
 * @brief Temporary, until we figure out how to expose this
 */
static const vec_t *G_Ai_ViewAngles(const g_entity_t *self) {
	return self->client->locals.angles;
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

	// SCRATCH
	import.entities = ge.entities;
	import.entity_size = ge.entity_size;
	import.G_GroundEntity = G_Ai_GroundEntity;
	import.G_ViewAngles = G_Ai_ViewAngles;
	
	ai_export_t *exports = gi.LoadAi(&import);

	if (!exports) {
		g_game.ai_loaded = false;
		return;
	}

	g_game.ai_loaded = true;

	aie = *exports;
	
	gi.Cmd("g_ai_add", G_Ai_Add_f, CMD_GAME, "Add one or more AI to the game");
	gi.Cmd("g_ai_remove", G_Ai_Remove_f, CMD_GAME, "Remove one or more AI from the game");
}

/**
 * @brief
 */
void G_Ai_Shutdown(void) {

}
