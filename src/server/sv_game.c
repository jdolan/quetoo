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

#include "sv_local.h"

/**
 * @brief Fetch the active debug mask.
 */
static debug_t Sv_DebugMask(void) {
	return quetoo.debug_mask;
}

/**
 * @brief
 */
static void Sv_GameDebug(const debug_t debug, const char *func, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void Sv_GameDebug(const debug_t debug, const char *func, const char *fmt, ...) {

	va_list args;
	va_start(args, fmt);

	Com_Debugv_(debug, func, fmt, args);

	va_end(args);
}

/**
 * @brief Abort the server with a game error, always emitting ERROR_DROP.
 */
static void Sv_GameError(const char *func, const char *fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));
static void Sv_GameError(const char *func, const char *fmt, ...) {

	va_list args;
	va_start(args, fmt);

	Com_Errorv_(ERROR_DROP, func, fmt, args);

	va_end(args);
}

/**
 * @brief Also sets mins and maxs for inline bsp models.
 */
static void Sv_SetModel(g_entity_t *ent, const char *name) {

	if (!name) {
		Com_Warn("%d: NULL\n", (int32_t) NUM_FOR_ENTITY(ent));
		return;
	}

	ent->s.model1 = Sv_ModelIndex(name);

	// if it is an inline model, get the size information for it
	if (name[0] == '*') {
		const cm_bsp_model_t *mod = Cm_Model(name);
		ent->bounds = mod->bounds;
		Sv_LinkEntity(ent);
	}
}

/**
 * @brief
 */
static void Sv_SetConfigString(const int32_t index, const char *val) {

	if (index >= MAX_CONFIG_STRINGS) {
		Com_Warn("Bad index %u\n", index);
		return;
	}

	if (!val) {
		val = "";
	}

	// make sure it's actually changed
	if (!g_strcmp0(sv.config_strings[index], val)) {
		return;
	}

	// change the string in sv.config_strings
	g_strlcpy(sv.config_strings[index], val, sizeof(sv.config_strings[0]));

	if (sv.state != SV_LOADING) { // send the update to everyone
		Mem_ClearBuffer(&sv.multicast);
		Net_WriteByte(&sv.multicast, SV_CMD_CONFIG_STRING);
		Net_WriteShort(&sv.multicast, index);
		Net_WriteString(&sv.multicast, val);

		Sv_Multicast(Vec3_Zero(), MULTICAST_ALL_R, NULL);
	}
}

/**
 * @brief
 */
static const char *Sv_GetConfigString(const int32_t index) {

	if (index >= MAX_CONFIG_STRINGS) {
		Com_Warn("Bad index %u\n", index);
		return NULL;
	}

	return sv.config_strings[index];
}

/**
 * @brief
 */
static void Sv_WriteData(const void *data, size_t len) {
	Net_WriteData(&sv.multicast, data, len);
}

/**
 * @brief
 */
static void Sv_WriteChar(const int32_t c) {
	Net_WriteChar(&sv.multicast, c);
}

/**
 * @brief
 */
static void Sv_WriteByte(const int32_t c) {
	Net_WriteByte(&sv.multicast, c);
}

/**
 * @brief
 */
static void Sv_WriteShort(const int32_t c) {
	Net_WriteShort(&sv.multicast, c);
}

/**
 * @brief
 */
static void Sv_WriteLong(const int32_t c) {
	Net_WriteLong(&sv.multicast, c);
}

/**
 * @brief
 */
static void Sv_WriteString(const char *s) {
	Net_WriteString(&sv.multicast, s);
}

/**
 * @brief
 */
static void Sv_WriteVector(const float v) {
	Net_WriteFloat(&sv.multicast, v);
}

/**
 * @brief
 */
static void Sv_WritePosition(const vec3_t pos) {
	Net_WritePosition(&sv.multicast, pos);
}

/**
 * @brief
 */
static void Sv_WriteDir(const vec3_t dir) {
	Net_WriteDir(&sv.multicast, dir);
}

/**
 * @brief
 */
static void Sv_WriteAngle(const float v) {
	Net_WriteAngle(&sv.multicast, v);
}

/**
 * @brief
 */
static void Sv_WriteAngles(const vec3_t angles) {
	Net_WriteAngles(&sv.multicast, angles);
}

/**
 * @brief
 */
static _Bool Sv_InPVS(const vec3_t p1, const vec3_t p2) {

	// TODO: Traces?

	return true;
}

/**
 * @brief
 */
static _Bool Sv_InPHS(const vec3_t p1, const vec3_t p2) {

	// TODO: Distance threshold?

	return true;
}

/**
 * @brief
 */
static void Sv_Sound(const g_entity_t *ent, int32_t index, sound_atten_t atten, int8_t pitch) {

	assert(ent);

	Sv_PositionedSound(ent->s.origin, ent, index, atten, pitch);
}

static void *ai_handle;

/**
 * @brief
 */
static ai_export_t *Sv_LoadAi(ai_import_t *import) {

	Com_Print("Ai initialization...\n");

	ai_handle = Sys_OpenLibrary("ai", false);
	assert(ai_handle);

	svs.ai = Sys_LoadLibrary(ai_handle, "Ai_LoadAi", import);

	if (!svs.ai) {
		Com_Error(ERROR_DROP, "Failed to load ai\n");
	}

	if (svs.ai->api_version != AI_API_VERSION) {
		Com_Error(ERROR_DROP, "Ai is version %i, not %i\n", svs.ai->api_version, AI_API_VERSION);
	}

	svs.ai->Init();

	Com_Print("Ai initialized\n");
	Com_InitSubsystem(QUETOO_AI);

	return svs.ai;
}

/**
 * @brief Called when the AI needs to be killed.
 */
static void Sv_ShutdownAi(void) {

	if (!svs.ai) {
		return;
	}

	Com_Print("Ai shutdown...\n");

	svs.ai->Shutdown();
	svs.ai = NULL;

	Cmd_RemoveAll(CMD_AI);

	// the game module code should call this, but lets not assume
	Mem_FreeTag(MEM_TAG_AI);

	Com_Print("Ai down\n");
	Com_QuitSubsystem(QUETOO_AI);

	Sys_CloseLibrary(ai_handle);
	ai_handle = NULL;
}


static void *game_handle;

/**
 * @brief Initializes the game module by exposing a subset of server functionality
 * through function pointers. In return, the game module allocates memory for
 * entities and returns a few pointers of its own.
 *
 * Note that the terminology here is worded from the game module's perspective;
 * that is, "import" is what we give to the game, and "export" is what the game
 * returns to us. This distinction seems a bit backwards, but it was likely
 * deemed less confusing to "mod" authors back in the day.
 */
void Sv_InitGame(void) {
	g_import_t import;

	if (svs.game) {
		Com_Error(ERROR_FATAL, "Game already loaded");
	}

	Com_Print("Game initialization...\n");

	memset(&import, 0, sizeof(import));

	import.Print = Com_Print;
	import.DebugMask = Sv_DebugMask;
	import.Debug_ = Sv_GameDebug;
	import.Warn_ = Com_Warn_;
	import.Error_ = Sv_GameError;

	import.Malloc = Mem_TagMalloc;
	import.LinkMalloc = Mem_LinkMalloc;
	import.Free = Mem_Free;
	import.FreeTag = Mem_FreeTag;

	import.OpenFile = Fs_OpenRead;
	import.SeekFile = Fs_Seek;
	import.ReadFile = Fs_Read;
	import.OpenFileWrite = Fs_OpenWrite;
	import.WriteFile = Fs_Write;
	import.CloseFile = Fs_Close;
	import.FileExists = Fs_Exists;
	import.LoadFile = Fs_Load;
	import.FreeFile = Fs_Free;
	import.Mkdir = Fs_Mkdir;
	import.RealPath = Fs_RealPath;
	import.EnumerateFiles = Fs_Enumerate;

	import.AddCvar = Cvar_Add;
	import.GetCvar = Cvar_Get;
	import.GetCvarInteger = Cvar_GetInteger;
	import.GetCvarString = Cvar_GetString;
	import.GetCvarValue = Cvar_GetValue;
	import.SetCvarInteger = Cvar_SetInteger;
	import.SetCvarString = Cvar_SetString;
	import.SetCvarValue = Cvar_SetValue;
	import.AddCmd = Cmd_Add;
	import.Argc = Cmd_Argc;
	import.Argv = Cmd_Argv;
	import.Args = Cmd_Args;
	import.TokenizeString = Cmd_TokenizeString;
	import.Cbuf = Cbuf_AddText;

	import.SetConfigString = Sv_SetConfigString;
	import.GetConfigString = Sv_GetConfigString;
	import.ModelIndex = Sv_ModelIndex;
	import.SoundIndex = Sv_SoundIndex;
	import.ImageIndex = Sv_ImageIndex;

	import.Sound = Sv_Sound;
	import.PositionedSound = Sv_PositionedSound;

	import.Bsp = Cm_Bsp;
	import.EntityValue = Cm_EntityValue;
	import.EntityBrushes = Cm_EntityBrushes;
	import.PointContents = Sv_PointContents;
	import.PointInsideBrush = Cm_PointInsideBrush;
	import.Trace = Sv_Trace;
	import.Clip = Sv_Clip;
	import.inPVS = Sv_InPVS;
	import.inPHS = Sv_InPHS;
	import.SetModel = Sv_SetModel;
	import.LinkEntity = Sv_LinkEntity;
	import.UnlinkEntity = Sv_UnlinkEntity;
	import.BoxEntities = Sv_BoxEntities;

	import.Multicast = Sv_Multicast;
	import.Unicast = Sv_Unicast;
	import.WriteData = Sv_WriteData;
	import.WriteChar = Sv_WriteChar;
	import.WriteByte = Sv_WriteByte;
	import.WriteShort = Sv_WriteShort;
	import.WriteLong = Sv_WriteLong;
	import.WriteString = Sv_WriteString;
	import.WriteVector = Sv_WriteVector;
	import.WritePosition = Sv_WritePosition;
	import.WriteDir = Sv_WriteDir;
	import.WriteAngle = Sv_WriteAngle;
	import.WriteAngles = Sv_WriteAngles;

	import.BroadcastPrint = Sv_BroadcastPrint;
	import.ClientPrint = Sv_ClientPrint;

	import.LoadAi = Sv_LoadAi;

	game_handle = Sys_OpenLibrary("game", false);
	assert(game_handle);
	
	svs.game = (g_export_t *) Sys_LoadLibrary(game_handle, "G_LoadGame", &import);

	if (!svs.game) {
		Com_Error(ERROR_DROP, "Failed to load game module\n");
	}

	if (svs.game->api_version != GAME_API_VERSION) {
		Com_Error(ERROR_DROP, "Game is version %i, not %i\n", svs.game->api_version, GAME_API_VERSION);
	}

	svs.game->Init();

	Com_Print("Game initialized, starting...\n");
	Com_InitSubsystem(QUETOO_GAME);
}

/**
 * @brief Called when either the entire server is being killed, or it is changing to a
 * different game directory.
 */
void Sv_ShutdownGame(void) {

	if (!svs.game) {
		return;
	}

	// shutdown AI first
	Sv_ShutdownAi();

	Com_Print("Game shutdown...\n");

	svs.game->Shutdown();
	svs.game = NULL;

	Cmd_RemoveAll(CMD_GAME);

	// the game module code should call this, but lets not assume
	Mem_FreeTag(MEM_TAG_GAME_LEVEL);
	Mem_FreeTag(MEM_TAG_GAME);

	Com_Print("Game down\n");
	Com_QuitSubsystem(QUETOO_GAME);

	Sys_CloseLibrary(&game_handle);
}
