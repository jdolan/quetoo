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

#include "cl_local.h"

#include <Objectively/RESTClient.h>
#include <Objectively/URLCache.h>

static void *cgameHandle;

/**
 * @brief Fetch the active debug mask.
 */
static DebugFlags Cl_CgameDebugMask(void) {
  return quetoo.debugMask;
}

/**
 * @brief Forwards a debug message from the client game module to the engine console.
 */
static void Cl_CgameDebug(const DebugFlags debug, const char *func, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void Cl_CgameDebug(const DebugFlags debug, const char *func, const char *fmt, ...) {

  va_list args;
  va_start(args, fmt);

  Com_Debugv_(debug, func, fmt, args);

  va_end(args);
}

/**
 * @brief Forwards a warning message from the client game module to the engine console.
 */
static void Cl_CgameWarn(const char *func, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void Cl_CgameWarn(const char *func, const char *fmt, ...) {

  va_list args;
  va_start(args, fmt);

  Com_Warnv_(func, fmt, args);

  va_end(args);
}

/**
 * @brief Handle a client game error. This wraps `Com_Error`, always emitting `ERROR_DROP`.
 */
static void Cl_CgameError(const char *func, const char *fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));
static void Cl_CgameError(const char *func, const char *fmt, ...) {

  va_list args;
  va_start(args, fmt);

  Com_Errorv_(ERROR_DROP, func, fmt, args);

  va_end(args);
}

/**
 * @brief Message parsing facilities.
 */

static void Cl_ReadData(void *data, size_t len) {
  Net_ReadData(&netMessage, data, len);
}

static int32_t Cl_ReadChar(void) {
  return Net_ReadChar(&netMessage);
}

static int32_t Cl_ReadByte(void) {
  return Net_ReadByte(&netMessage);
}

static int32_t Cl_ReadShort(void) {
  return Net_ReadShort(&netMessage);
}

static int32_t Cl_ReadLong(void) {
  return Net_ReadLong(&netMessage);
}

static char *Cl_ReadString(void) {
  return Net_ReadString(&netMessage);
}

static float Cl_ReadFloat(void) {
  return Net_ReadFloat(&netMessage);
}

static Vec3 Cl_ReadPosition(void) {
  return Net_ReadPosition(&netMessage);
}

static Vec3 Cl_ReadDir(void) {
  return Net_ReadDir(&netMessage);
}

static float Cl_ReadAngle(void) {
  return Net_ReadAngle(&netMessage);
}

static Vec3 Cl_ReadAngles(void) {
  return Net_ReadAngles(&netMessage);
}

/**
 * @brief Returns the current list of known servers for the client game module.
 */
static PointerArray *Cl_Servers(void) {
  return cls.servers;
}

/**
 * @brief Returns the config string at the given index for the client game module.
 */
static char *Cl_ConfigString(int32_t index) {

  if (index < 0 || index > MAX_CONFIG_STRINGS) {
    Com_Warn("Bad index %i\n", index);
    return "";
  }

  return cl.configStrings[index];
}

/**
 * @brief Appends a formatted message to the consoles at the given level.
 */
static void Cl_CgamePrintLevel(int32_t level, const char *fmt, ...) {
  char string[MAX_PRINT_MSG];

  va_list args;
  va_start(args, fmt);
  vsnprintf(string, sizeof(string), fmt, args);
  va_end(args);

  Con_Append(level, string);
}

/**
 * @brief Initializes the client game subsystem, running the client game that
 * `Com_Cgame` names.
 */
void Cl_InitCgame(void) {
  CGameImport import;

  const char *dir = Com_Cgame();

  if (!*dir) {
    Com_Error(ERROR_DROP, "Neither %s nor %s provides a client game module\n", Com_Game(), DEFAULT_GAME);
  }

  void *handle = Sys_OpenLibrary(dir, "cgame");
  if (!handle) {
    Com_Error(ERROR_DROP, "Failed to open %s's client game module\n", dir);
  }

  Com_Print("Client game initialization...\n");

  memset(&import, 0, sizeof(import));

  import.client = &cl;
  import.state = &cls.state;
  import.server = &cls.server;
  import.demo = &cls.demo;
  import.context = &rContext;
  import.view = &clView;
  import.stage = &clStage;

  import.Print = Com_Print;
  import.PrintLevel = Cl_CgamePrintLevel;
  import.Debug = Cl_CgameDebug;
  import.DebugMask = Cl_CgameDebugMask;
  import.Warn = Cl_CgameWarn;
  import.Error = Cl_CgameError;
  import.Backtrace = Sys_Backtrace;
  import.Tail = Con_Tail;

  import.Malloc = Mem_TagMalloc;
  import.LinkMalloc = Mem_LinkMalloc;
  import.Realloc = Mem_Realloc;
  import.Free = Mem_Free;
  import.FreeTag = Mem_FreeTag;

  import.Thread = Thread_Create_;
  import.Wait = Thread_Wait;

  {
    RESTClient *client = $$(RESTClient, sharedInstance);
    if (client->session->configuration->urlCache == NULL) {
      client->session->configuration->urlCache = $(alloc(URLCache), init);
    }
    import.restClient = client;
  }

  import.StatFile = Fs_Stat;
  import.OpenFile = Fs_OpenRead;
  import.SeekFile = Fs_Seek;
  import.ReadFile = Fs_Read;
  import.OpenFileWrite = Fs_OpenWrite;
  import.WriteFileAt = Fs_WriteAt;
  import.WriteFile = Fs_Write;
  import.CloseFile = Fs_Close;
  import.DeleteFile = Fs_Delete;
  import.LoadFile = Fs_Load;
  import.FreeFile = Fs_Free;
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
  import.ToggleCvar = Cvar_Toggle;
  import.AddCmd = Cmd_Add;
  import.Argv = Cmd_Argv;
  import.AutocompleteMatch = Con_AutocompleteMatch;
  import.Cbuf = Cbuf_AddText;

  import.InstallerConsent = Installer_Consent;

  import.Theme = Ui_Theme;
  import.TopViewController = Ui_TopViewController;
  import.PushViewController = Ui_PushViewController;
  import.PopToViewController = Ui_PopToViewController;
  import.PopViewController = Ui_PopViewController;
  import.PopAllViewControllers = Ui_PopAllViewControllers;
  import.SetHudViewController = Ui_SetHudViewController;
  import.LoadingProgress = Cl_LoadingProgress;

  import.KeyForBind = Cl_KeyForBind;
  import.KeyName = Cl_KeyName;
  import.BindKey = Cl_Bind;
  import.SetKeyDest = Cl_SetKeyDest;
  import.GetKeyDest = Cl_GetKeyDest;
  import.KeyDown = Cl_KeyDown;
  import.KeyUp = Cl_KeyUp;
  import.KeyState = Cl_KeyState;

  import.Servers = Cl_Servers;
  import.GetServers = Cl_Servers_f;
  import.ServerInfo = Cl_ServerInfo;
  import.Connect = Cl_Connect;
  import.Mapshots = Cl_Mapshots;

  import.ConfigString = Cl_ConfigString;
  import.ReadData = Cl_ReadData;
  import.ReadChar = Cl_ReadChar;
  import.ReadByte = Cl_ReadByte;
  import.ReadShort = Cl_ReadShort;
  import.ReadLong = Cl_ReadLong;
  import.ReadString = Cl_ReadString;
  import.ReadFloat = Cl_ReadFloat;
  import.ReadPosition = Cl_ReadPosition;
  import.ReadDir = Cl_ReadDir;
  import.ReadAngle = Cl_ReadAngle;
  import.ReadAngles = Cl_ReadAngles;
  import.WriteEntityInfoCommand = Cl_WriteEntityInfoCommand;

  import.Bsp = Cm_Bsp;
  import.Worldspawn = Cm_Worldspawn;
  import.EntityValue = Cm_EntityValue;
  import.EntityAssign = Cm_EntityAssign;
  import.EntityBrushes = Cm_EntityBrushes;
  import.AllocEntity = Cm_AllocEntity;
  import.FreeEntity = Cm_FreeEntity;
  import.ParseEntity = Cm_ParseEntity;
  import.SetEntityKeyValue = Cm_EntitySetKeyValue;
  import.EntityToInfoString = Cm_EntityToInfoString;
  import.EntityFromInfoString = Cm_EntityFromInfoString;
  import.PointContents = Cl_PointContents;
  import.BoxContents = Cl_BoxContents;
  import.BoxLeafnums = Cm_BoxLeafnums;
  import.PointInsideBrush = Cm_PointInsideBrush;
  import.Trace = Cl_Trace;
  import.TraceToBrush = Cm_TraceToBrush;
  import.PointLeafnum = Cm_PointLeafnum;

  import.LoadSample = S_LoadSample;
  import.LoadClientModelSample = S_LoadClientModelSample;
  import.LoadClientModelSamples = S_LoadClientModelSamples;
  import.AddSample = S_AddSample;
  import.StartVoice = S_StartVoice;
  import.StopVoice = S_StopVoice;

  import.CreateFramebuffer = R_CreateFramebuffer;
  import.DestroyFramebuffer = R_DestroyFramebuffer;

  import.LoadSurface = Img_LoadSurface;
  import.LoadSurfaceFromData = Img_LoadSurfaceFromData;
  import.BlurSurface = Img_BlurSurface;
  import.LoadImage = R_LoadImage;
  import.LoadAtlas = R_LoadAtlas;
  import.LoadAtlasImage = R_LoadAtlasImage;
  import.CompileAtlas = R_CompileAtlas;
  import.CreateAnimation = R_CreateAnimation;
  import.LoadMaterial = R_LoadMaterial;
  import.MaterialLightStage = Cm_MaterialLightStage;
  import.LoadModel = R_LoadModel;
  import.WorldModel = R_WorldModel;

  import.InitView = R_InitView;
  import.AddEntity = R_AddEntity;
  import.AddLight = R_AddLight;
  import.AddSprite = R_AddSprite;
  import.AddBeam = R_AddBeam;
  import.AddDecal = R_AddDecal;
  import.AddPortal = R_AddPortal;
  import.DrawPlayerModelView = R_DrawPlayerModelView;
  import.Draw3DLines = R_Draw3DLines;
  import.Draw3DBox = R_Draw3DBox;

  // the module we hold comes down before the new one is asked for anything, so
  // that its Cg_LoadCgame cannot have work undone by the outgoing module's
  // teardown. Nothing below may fail without leaving us no client game at all
  Cl_ShutdownCgame();

  cgameHandle = handle;

  CGameExport *cgame = Sys_LoadLibrary(cgameHandle, "Cg_LoadCgame", &import);

  if (!cgame) {
    cgameHandle = Sys_CloseLibrary(cgameHandle);
    Com_Error(ERROR_FATAL, "Failed to load %s's client game\n", dir);
  }

  if (cgame->apiVersion != CGAME_API_VERSION) {
    const int32_t version = cgame->apiVersion;
    cgameHandle = Sys_CloseLibrary(cgameHandle);
    Com_Error(ERROR_FATAL, "%s's client game is version %i, not %i\n", dir, version, CGAME_API_VERSION);
  }

  // ObjectivelyMVC binds Views by class name, from JSON hierarchies and CSS
  // selectors, and the module is opened RTLD_LOCAL so that two modules' symbols
  // cannot coalesce - which leaves it out of the namespace Objectively would
  // otherwise search, and Windows has no such namespace at all. The export
  // table is a static within the module, which is the address Objectively
  // resolves the module by, so that it can drop its Classes when it comes down.
  addClassImage(cgameHandle, cgame);

  cls.cgame = cgame;
  cls.cgame->Init();

  Com_Print("Client game initialized\n");
  Com_InitSubsystem(QUETOO_CGAME);
}

/**
 * @brief Shuts down the client game subsystem.
 */
void Cl_ShutdownCgame(void) {

  if (!cls.cgame) {
    return;
  }

  Com_Print("Client game shutdown...\n");

  cls.cgame->Shutdown();
  cls.cgame = NULL;

  Cmd_RemoveAll(CMD_CGAME);

  Mem_FreeTag(MEM_TAG_CGAME_LEVEL);
  Mem_FreeTag(MEM_TAG_CGAME);

  Com_Print("Client game down\n");
  Com_QuitSubsystem(QUETOO_CGAME);

  // last, and while the handle is still open: the menus are gone by now, and
  // the Classes this image declared must not outlive it
  removeClassImage(cgameHandle);

  cgameHandle = Sys_CloseLibrary(cgameHandle);
}
