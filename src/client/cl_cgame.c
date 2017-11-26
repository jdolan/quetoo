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
 * @brief
 */
static void Cl_CgameDebug(const char *func, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void Cl_CgameDebug(const char *func, const char *fmt, ...) {

	va_list args;
	va_start(args, fmt);

	Com_Debugv_(DEBUG_CGAME, func, fmt, args);

	va_end(args);
}

/**
 * @brief
 */
static void Cl_CgamePmDebug(const char *func, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void Cl_CgamePmDebug(const char *func, const char *fmt, ...) {

	va_list args;
	va_start(args, fmt);

	Com_Debugv_(DEBUG_PMOVE_CLIENT, func, fmt, args);

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

static vec_t Cl_ReadVector(void) {
	return Net_ReadVector(&net_message);
}

static void Cl_ReadPosition(vec3_t pos) {
	Net_ReadPosition(&net_message, pos);
}

static void Cl_ReadDir(vec3_t dir) {
	Net_ReadDir(&net_message, dir);
}

static vec_t Cl_ReadAngle(void) {
	return Net_ReadAngle(&net_message);
}

static void Cl_ReadAngles(vec3_t angles) {
	Net_ReadAngles(&net_message, angles);
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
static char *Cl_ConfigString(uint16_t index) {

	if (index > MAX_CONFIG_STRINGS) {
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

	Com_Print("Client initialization...\n");

	memset(&import, 0, sizeof(import));

	import.client = &cl;
	import.state = &cls.state;
	import.server_name = cls.server_name;

	import.context = &r_context;

	import.view = &r_view;

	import.Print = Com_Print;
	import.Debug_ = Cl_CgameDebug;
	import.PmDebug = Cl_CgamePmDebug;
	import.Warn_ = Com_Warn_;
	import.Error_ = Cl_CgameError;

	import.Malloc = Mem_TagMalloc;
	import.LinkMalloc = Mem_LinkMalloc;
	import.Free = Mem_Free;
	import.FreeTag = Mem_FreeTag;

	import.Thread = Thread_Create_;

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

	import.Cvar = Cvar_Add;
	import.CvarGet = Cvar_Get;
	import.CvarSet = Cvar_Set;
	import.CvarString = Cvar_GetString;
	import.CvarValue = Cvar_GetValue;
	import.CvarSetValue = Cvar_SetValue;
	import.CvarToggle = Cvar_Toggle;
	import.Cmd = Cmd_Add;
	import.Cbuf = Cbuf_AddText;

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
	import.ReadVector = Cl_ReadVector;
	import.ReadPosition = Cl_ReadPosition;
	import.ReadDir = Cl_ReadDir;
	import.ReadAngle = Cl_ReadAngle;
	import.ReadAngles = Cl_ReadAngles;

	import.EntityString = Cm_EntityString;

	import.PointContents = Cl_PointContents;
	import.Trace = Cl_Trace;

	import.LeafForPoint = R_LeafForPoint;
	import.LeafHearable = R_LeafHearable;
	import.LeafVisible = R_LeafVisible;

	import.KeyDown = Cl_KeyDown;
	import.KeyUp = Cl_KeyUp;
	import.KeyState = Cl_KeyState;

	import.LoadSample = S_LoadSample;
	import.LoadClientSamples = S_LoadClientSamples;
	import.AddSample = S_AddSample;

	import.CullBox = R_CullBox;
	import.CullSphere = R_CullSphere;

	import.ColorFromPalette = Img_ColorFromPalette;
	import.Color = R_Color;

	import.LoadSurface = Img_LoadImage;
	import.LoadImage = R_LoadImage;
	import.CreateAtlas = R_CreateAtlas;
	import.AddImageToAtlas = R_AddImageToAtlas;
	import.GetAtlasImageFromAtlas = R_GetAtlasImageFromAtlas;
	import.CompileAtlas = R_CompileAtlas;
	import.LoadMaterial = R_LoadMaterial;
	import.LoadMaterials = R_LoadMaterials;
	import.LoadModel = R_LoadModel;
	import.WorldModel = R_WorldModel;

	import.AddEntity = R_AddEntity;
	import.MeshModelTag = R_MeshModelTag;
	import.SetMatrixForEntity = R_SetMatrixForEntity;
	import.SetMatrix = R_SetMatrix;
	import.GetMatrix = R_GetMatrix;
	import.PushMatrix = R_PushMatrix;
	import.PopMatrix = R_PopMatrix;
	import.DrawMeshModel = R_DrawMeshModel_default;
	import.DrawMeshModelMaterials = R_DrawMeshModelMaterials_default;
	import.EnableDepthTest = R_EnableDepthTest;
	import.DepthRange = R_DepthRange;
	import.EnableTexture = R_EnableTextureByIdentifier;
	import.SetViewport = R_SetViewport;
	import.AddLight = R_AddLight;
	import.AddParticle = R_AddParticle;
	import.AddSustainedLight = R_AddSustainedLight;
	import.AddStain = R_AddStain;

	import.DrawImage = R_DrawImage;
	import.DrawImageResized = R_DrawImageResized;
	import.DrawFill = R_DrawFill;

	import.BindFont = R_BindFont;
	import.StringWidth = R_StringWidth;
	import.DrawString = R_DrawString;

	cls.cgame = Sys_LoadLibrary("cgame", &cgame_handle, "Cg_LoadCgame", &import);

	if (!cls.cgame) {
		Com_Error(ERROR_DROP, "Failed to load client game\n");
	}

	if (cls.cgame->api_version != CGAME_API_VERSION) {
		Com_Error(ERROR_DROP, "Client game has wrong version (%d)\n", cls.cgame->api_version);
	}

	cls.cgame->Init();

	Com_Print("Client initialized\n");
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

	Sys_CloseLibrary(&cgame_handle);
}
