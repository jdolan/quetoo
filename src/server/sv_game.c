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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "sv_local.h"
#include <Objectively/RESTClient.h>

#include <Objectively/JSONContext.h>

/**
 * @brief Fetch the active debug mask.
 */
static DebugFlags Sv_DebugMask(void) {
  return quetoo.debugMask;
}

/**
 * @brief Forwards debug output from the game module to the common debug system.
 */
static void Sv_GameDebug(const DebugFlags debug, const char *func, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void Sv_GameDebug(const DebugFlags debug, const char *func, const char *fmt, ...) {

  va_list args;
  va_start(args, fmt);

  Com_Debugv_(debug, func, fmt, args);

  va_end(args);
}

/**
 * @brief Abort the server with a game error, always emitting `ERROR_DROP`.
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
static void Sv_SetModel(GameEntity *ent, const char *name) {

  if (!name) {
    Com_Warn("%s: NULL\n", etos(ent));
    return;
  }

  ent->s.model1 = Sv_ModelIndex(name);

  // if it is an inline model, get the size information for it
  if (name[0] == '*') {
    const CmBspModel *mod = Cm_Model(name);
    ent->bounds = mod->bounds;
    Sv_LinkEntity(ent);
  }
}

/**
 * @brief Sets a config string by index and multicasts the update to all clients.
 */
void Sv_SetConfigString(const int32_t index, const char *val) {

  if (index >= MAX_CONFIG_STRINGS) {
    Com_Warn("Bad index %u\n", index);
    return;
  }

  if (!val) {
    val = "";
  }

  // make sure it's actually changed
  if (!q_strcmp(sv.configStrings[index], val)) {
    return;
  }

  // change the string in sv.configStrings
  q_strlcpy(sv.configStrings[index], val, sizeof(sv.configStrings[0]));

  if (svs.state >= SV_ACTIVE_GAME) { // send the update to everyone
    Mem_ClearBuffer(&sv.multicast);
    Net_WriteByte(&sv.multicast, SV_CMD_CONFIG_STRING);
    Net_WriteShort(&sv.multicast, index);
    Net_WriteString(&sv.multicast, val);

    Sv_Multicast(Vec3_Zero(), MULTICAST_ALL_R);
  }
}

/**
 * @brief Returns the config string at the given index.
 */
const char *Sv_GetConfigString(const int32_t index) {

  if (index >= MAX_CONFIG_STRINGS) {
    Com_Warn("Bad index %u\n", index);
    return NULL;
  }

  return sv.configStrings[index];
}

/**
 * @brief Writes raw data to the server multicast buffer.
 */
static void Sv_WriteData(const void *data, size_t len) {
  Net_WriteData(&sv.multicast, data, len);
}

/**
 * @brief Writes a character to the server multicast buffer.
 */
static void Sv_WriteChar(const int32_t c) {
  Net_WriteChar(&sv.multicast, c);
}

/**
 * @brief Writes a byte to the server multicast buffer.
 */
static void Sv_WriteByte(const int32_t c) {
  Net_WriteByte(&sv.multicast, c);
}

/**
 * @brief Writes a short integer to the server multicast buffer.
 */
static void Sv_WriteShort(const int32_t c) {
  Net_WriteShort(&sv.multicast, c);
}

/**
 * @brief Writes a long integer to the server multicast buffer.
 */
static void Sv_WriteLong(const int32_t c) {
  Net_WriteLong(&sv.multicast, c);
}

/**
 * @brief Writes a string to the server multicast buffer.
 */
static void Sv_WriteString(const char *s) {
  Net_WriteString(&sv.multicast, s);
}

/**
 * @brief Writes a floating-point vector component to the server multicast buffer.
 */
static void Sv_WriteVector(const float v) {
  Net_WriteFloat(&sv.multicast, v);
}

/**
 * @brief Writes a position vector to the server multicast buffer.
 */
static void Sv_WritePosition(const Vec3 pos) {
  Net_WritePosition(&sv.multicast, pos);
}

/**
 * @brief Writes a direction vector to the server multicast buffer.
 */
static void Sv_WriteDir(const Vec3 dir) {
  Net_WriteDir(&sv.multicast, dir);
}

/**
 * @brief Writes an angle value to the server multicast buffer.
 */
static void Sv_WriteAngle(const float v) {
  Net_WriteAngle(&sv.multicast, v);
}

/**
 * @brief Writes an angles vector to the server multicast buffer.
 */
static void Sv_WriteAngles(const Vec3 angles) {
  Net_WriteAngles(&sv.multicast, angles);
}

static void *gameHandle;

/**
 * @brief `RESTClientCompletion` for `Sv_PostStats`.
 */
static void Sv_PostStatsCallback(int32_t status, Data *data, void *userData) {
  const char *url = userData;

  if (status < 200 || status >= 300) {
    Com_Warn("Sv_PostStatsCallback: POST to %s failed (HTTP %d): %.*s\n", url, status,
             data ? (int) data->length : 0, data ? (const char *) data->bytes : "");
  } else {
    Com_Print("POST to %s: HTTP %d\n", url, status);
  }
}

/**
 * @brief Serializes frag events from the game module to JSON and POSTs them
 * asynchronously to `sv_statsUrl`. Gated on `sv_public` and on the URL naming an endpoint.
 *
 * @remarks `0` disables stats, and so does the empty string. The empty string cannot actually be
 * typed: `Cvar_Set_f` wants three tokens and an empty quoted argument does not produce one, so
 * `set sv_statsUrl ""` silently does nothing. `0` is what the cvar description documents.
 *
 * Each request also carries `X-Quetoo-Port` and `X-Quetoo-Hostname` headers so
 * that the stats service can disambiguate multiple server instances sharing a
 * single public IP, which it cannot otherwise distinguish (payloads only ever
 * identify a player, not the reporting server).
 */
static void Sv_PostStats(const GameFrag *frags, size_t fragsLen, const GameCapture *captures, size_t capturesLen) {

  if (!sv_statsUrl->string[0] || !q_strcmp(sv_statsUrl->string, "0") || sv_public->integer <= 0) {
    return;
  }

  const Cvar *netPort = Cvar_Get("net_port");

  const char *headers[] = {
    "X-Quetoo-Port",     netPort->string,
    "X-Quetoo-Hostname", sv_hostname->string,
    NULL
  };

  if (fragsLen) {

    const JSONProperties svFragProperties = MakeJSONProperties(GameFrag,
      MakeJSONProperty(GameFrag, level,         JSONSerializeCharacters, NULL, NULL),
      MakeJSONProperty(GameFrag, attacker,      JSONSerializeCharacters, NULL, NULL),
      MakeJSONPropertyWithKey(GameFrag, attackerGuid, "attacker_guid", JSONSerializeCharacters, NULL, NULL),
      MakeJSONPropertyWithKey(GameFrag, attackerAi,   "attacker_ai",   JSONSerializeBoole,      NULL, NULL),
      MakeJSONProperty(GameFrag, target,        JSONSerializeCharacters, NULL, NULL),
      MakeJSONPropertyWithKey(GameFrag, targetGuid,   "target_guid",   JSONSerializeCharacters, NULL, NULL),
      MakeJSONPropertyWithKey(GameFrag, targetAi,     "target_ai",     JSONSerializeBoole,      NULL, NULL),
      MakeJSONProperty(GameFrag, weapon,        JSONSerializeCharacters, NULL, NULL),
      MakeJSONProperty(GameFrag, mod,           JSONSerializeInt32,      NULL, NULL),
      MakeJSONProperty(GameFrag, time,          JSONSerializeInt32,      NULL, NULL)
    );

    static char fragsUrl[MAX_STRING_CHARS];
    q_snprintf(fragsUrl, sizeof(fragsUrl), "%s/api/frags", sv_statsUrl->string);

    JSONContext *ctx = $(alloc(JSONContext), init);
    Data *data = $(ctx, dataFromStructs, &svFragProperties, (ident) frags, fragsLen);
    release(ctx);
    assert(data);

    Com_Print("POSTing %zd frags to %s\n", fragsLen, fragsUrl);
    $($$(RESTClient, sharedInstance), postAsync, fragsUrl, data, headers, Sv_PostStatsCallback, fragsUrl);

    release(data);
  }

  if (capturesLen) {

    const JSONProperties svCaptureProperties = MakeJSONProperties(GameCapture,
      MakeJSONProperty(GameCapture, level,       JSONSerializeCharacters, NULL, NULL),
      MakeJSONProperty(GameCapture, player,      JSONSerializeCharacters, NULL, NULL),
      MakeJSONPropertyWithKey(GameCapture, playerGuid, "player_guid", JSONSerializeCharacters, NULL, NULL),
      MakeJSONPropertyWithKey(GameCapture, playerAi,   "player_ai",   JSONSerializeBoole,      NULL, NULL),
      MakeJSONProperty(GameCapture, team,        JSONSerializeCharacters, NULL, NULL),
      MakeJSONProperty(GameCapture, time,        JSONSerializeInt32,      NULL, NULL)
    );

    static char capturesUrl[MAX_STRING_CHARS];
    q_snprintf(capturesUrl, sizeof(capturesUrl), "%s/api/captures", sv_statsUrl->string);

    JSONContext *ctx = $(alloc(JSONContext), init);
    Data *data = $(ctx, dataFromStructs, &svCaptureProperties, (ident) captures, capturesLen);
    release(ctx);
    assert(data);

    Com_Print("POSTing %zd captures to %s\n", capturesLen, capturesUrl);
    $($$(RESTClient, sharedInstance), postAsync, capturesUrl, data, headers, Sv_PostStatsCallback, capturesUrl);

    release(data);
  }
}

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
  GameImport import;

  if (svs.game) {
    Com_Error(ERROR_FATAL, "Game already loaded\n");
  }

  Com_Print("Game initialization...\n");

  memset(&import, 0, sizeof(import));

  import.Print = Com_Print;
  import.DebugMask = Sv_DebugMask;
  import.Debug = Sv_GameDebug;
  import.Warn = Com_Warn_;
  import.Error = Sv_GameError;
  import.Backtrace = Sys_Backtrace;

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
  import.ForceSetCvarString = Cvar_ForceSetString;
  import.ForceSetCvarValue = Cvar_ForceSetValue;
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

  import.Bsp = Cm_Bsp;
  import.Worldspawn = Cm_Worldspawn;
  import.EntityValue = Cm_EntityValue;
  import.EntityBrushes = Cm_EntityBrushes;
  import.LoadEntities = Cm_LoadEntities;
  import.FreeEntity = Cm_FreeEntity;
  import.MapList = Sv_MapList;
  import.MapIndex = Sv_MapIndex;
  import.SetNextMap = Sv_SetNextMap;
  import.PointContents = Sv_PointContents;
  import.BoxContents = Sv_BoxContents;
  import.PointInsideBrush = Cm_PointInsideBrush;
  import.Trace = Sv_Trace;
  import.Clip = Sv_Clip;
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

  import.MuteVoice = Sv_MuteVoice;
  import.BroadcastPrint = Sv_BroadcastPrint;
  import.ClientPrint = Sv_ClientPrint;

  import.PostStats = Sv_PostStats;

  const char *dir = Sys_LibraryDir(Com_Game(), "game");
  if (!dir) {
    Com_Error(ERROR_DROP, "Neither %s nor %s provides a game module\n", Com_Game(), DEFAULT_GAME);
  }

  gameHandle = Sys_OpenLibrary(dir, "game");
  if (!gameHandle) {
    Com_Error(ERROR_DROP, "Failed to open %s's game module\n", dir);
  }
  
  GameExport *game = (GameExport *) Sys_LoadLibrary(gameHandle, "G_LoadGame", &import);

  if (!game) {
    gameHandle = Sys_CloseLibrary(gameHandle);
    Com_Error(ERROR_DROP, "Failed to load %s's game module\n", dir);
  }

  if (game->apiVersion != GAME_API_VERSION) {
    const int32_t version = game->apiVersion;
    gameHandle = Sys_CloseLibrary(gameHandle);
    Com_Error(ERROR_DROP, "%s's game module is version %i, not %i\n", dir, version, GAME_API_VERSION);
  }

  svs.game = game;
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

  Com_Print("Game shutdown...\n");

  svs.game->Shutdown();
  svs.game = NULL;

  Cmd_RemoveAll(CMD_GAME);
  Cmd_RemoveAll(CMD_AI);

  // the game module code should call this, but lets not assume
  Mem_FreeTag(MEM_TAG_GAME_LEVEL);
  Mem_FreeTag(MEM_TAG_GAME);

  Com_Print("Game down\n");
  Com_QuitSubsystem(QUETOO_GAME);

  gameHandle = Sys_CloseLibrary(gameHandle);
}
