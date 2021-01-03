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

#include "cl_local.h"

static void *cgame_handle;

/**
 * @brief Fetch the active debug mask.
 */
static debug_t Cl_CgameDebugMask(void) {
	return quetoo.debug_mask;
}

/**
 * @brief
 */
static void Cl_CgameDebug(const debug_t debug, const char *func, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void Cl_CgameDebug(const debug_t debug, const char *func, const char *fmt, ...) {

	va_list args;
	va_start(args, fmt);

	Com_Debugv_(debug, func, fmt, args);

	va_end(args);
}

/**
 * @brief Handle a client game error. This wraps Com_Error, always emitting ERROR_DROP.
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
	Net_ReadData(&net_message, data, len);
}

static int32_t Cl_ReadChar(void) {
	return Net_ReadChar(&net_message);
}

static int32_t Cl_ReadByte(void) {
	return Net_ReadByte(&net_message);
}

static int32_t Cl_ReadShort(void) {
	return Net_ReadShort(&net_message);
}

static int32_t Cl_ReadLong(void) {
	return Net_ReadLong(&net_message);
}

static char *Cl_ReadString(void) {
	return Net_ReadString(&net_message);
}

static float Cl_ReadFloat(void) {
	return Net_ReadFloat(&net_message);
}

static vec3_t Cl_ReadPosition(void) {
	return Net_ReadPosition(&net_message);
}

static vec3_t Cl_ReadDir(void) {
	return Net_ReadDir(&net_message);
}

static float Cl_ReadAngle(void) {
	return Net_ReadAngle(&net_message);
}

static vec3_t Cl_ReadAngles(void) {
	return Net_ReadAngles(&net_message);
}

/**
 * @brief
 */
static GList *Cl_Servers(void) {
	return cls.servers;
}

/**
 * @brief
 */
static char *Cl_ConfigString(int32_t index) {

	if (index < 0 || index > MAX_CONFIG_STRINGS) {
		Com_Warn("Bad index %i\n", index);
		return "";
	}

	return cl.config_strings[index];
}

/**
 * @brief Initializes the client game subsystem
 */
void Cl_InitCgame(void) {
	cg_import_t import;

	if (cls.cgame) {
		Cl_ShutdownCgame();
	}

	Com_Print("Client game initialization...\n");

	memset(&import, 0, sizeof(import));

	import.client = &cl;
	import.state = &cls.state;

	import.context = &r_context;

	import.view = &cl_view;
	import.stage = &cl_stage;

	import.Print = Com_Print;
	import.Debug_ = Cl_CgameDebug;
	import.DebugMask = Cl_CgameDebugMask;
	import.Warn_ = Com_Warn_;
	import.Error_ = Cl_CgameError;

	import.Malloc = Mem_TagMalloc;
	import.LinkMalloc = Mem_LinkMalloc;
	import.Free = Mem_Free;
	import.FreeTag = Mem_FreeTag;

	import.Thread = Thread_Create_;
	import.Wait = Thread_Wait;

	import.BaseDir = Fs_BaseDir;
	import.OpenFile = Fs_OpenRead;
	import.SeekFile = Fs_Seek;
	import.ReadFile = Fs_Read;
	import.OpenFileWrite = Fs_OpenWrite;
	import.WriteFile = Fs_Write;
	import.CloseFile = Fs_Close;
	import.LoadFile = Fs_Load;
	import.FreeFile = Fs_Free;
	import.EnumerateFiles = Fs_Enumerate;
	import.FileExists = Fs_Exists;

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
	import.Cbuf = Cbuf_AddText;

	import.Theme = Ui_Theme;
	import.PushViewController = Ui_PushViewController;
	import.PopToViewController = Ui_PopToViewController;
	import.PopViewController = Ui_PopViewController;
	import.PopAllViewControllers = Ui_PopAllViewControllers;

	import.BindKey = Cl_Bind;
	import.KeyForBind = Cl_KeyForBind;
	import.KeyName = Cl_KeyName;

	import.Servers = Cl_Servers;
	import.GetServers = Cl_Servers_f;
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

	import.EntityValue = Cm_EntityValue;
	import.EntityBrushes = Cm_EntityBrushes;
	import.PointContents = Cl_PointContents;
	import.PointInsideBrush = Cm_PointInsideBrush;
	import.Trace = Cl_Trace;

	import.LeafForPoint = R_LeafForPoint;

	import.KeyDown = Cl_KeyDown;
	import.KeyUp = Cl_KeyUp;
	import.KeyState = Cl_KeyState;

	import.LoadingProgress = Cl_LoadingProgress;
	
	import.LoadSample = S_LoadSample;
	import.LoadClientModelSample = S_LoadClientModelSample;
	import.LoadClientModelSamples = S_LoadClientModelSamples;
	import.AddSample = Cl_AddSample;

	import.LoadSurface = Img_LoadSurface;
	import.LoadImage = R_LoadImage;
	import.LoadAtlas = R_LoadAtlas;
	import.LoadAtlasImage = R_LoadAtlasImage;
	import.CompileAtlas = R_CompileAtlas;
	import.CreateAnimation = R_CreateAnimation;
	import.LoadMaterial = R_LoadMaterial;
	import.LoadMaterials = R_LoadMaterials;
	import.LoadModel = R_LoadModel;
	import.WorldModel = R_WorldModel;

	import.AddEntity = R_AddEntity;
	import.AddLight = R_AddLight;
	import.AddSprite = R_AddSprite;
	import.AddBeam = R_AddBeam;
	import.AddStain = R_AddStain;

	import.BindFont = R_BindFont;
	import.Draw2DFill = R_Draw2DFill;
	import.Draw2DImage = R_Draw2DImage;
	import.Draw2DString = R_Draw2DString;
	import.StringWidth = R_StringWidth;

	cgame_handle = Sys_OpenLibrary("cgame", true);
	assert(cgame_handle);
	
	cls.cgame = Sys_LoadLibrary(cgame_handle, "Cg_LoadCgame", &import);

	if (!cls.cgame) {
		Com_Error(ERROR_DROP, "Failed to load client game\n");
	}

	if (cls.cgame->api_version != CGAME_API_VERSION) {
		Com_Error(ERROR_DROP, "Client game is version %i, not %i\n", cls.cgame->api_version, CGAME_API_VERSION);
	}

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

	Sys_CloseLibrary(cgame_handle);
	cgame_handle = NULL;
}
