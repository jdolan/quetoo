/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

/*
 * Cl_ShutdownCgame
 */
void Cl_ShutdownCgame(void) {

	if (!cls.cgame)
		return;

	Com_Print("Client game shutdown...\n");

	cls.cgame->Shutdown();
	cls.cgame = NULL;

	Com_Print("Client game down.\n");
	Com_QuitSubsystem(Q2W_CGAME);

	Sys_CloseLibrary(&cgame_handle);
}

/*
 * Cl_Error
 *
 * Abort the server with a game error
 */
static void Cl_Error(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
static void Cl_Error(const char *fmt, ...) {
	char msg[MAX_STRING_CHARS];
	va_list args;

	va_start(args, fmt);
	vsprintf(msg, fmt, args);
	va_end(args);

	Com_Error(ERR_DROP, "Client game error: %s.\n", msg);
}

/*
 * Message parsing facilities.
 */

static void Cl_ReadData(void *data, size_t len) {
	Msg_ReadData(&net_message, data, len);
}

static int Cl_ReadChar(void) {
	return Msg_ReadChar(&net_message);
}

static int Cl_ReadByte(void) {
	return Msg_ReadByte(&net_message);
}

static int Cl_ReadShort(void) {
	return Msg_ReadShort(&net_message);
}

static int Cl_ReadLong(void) {
	return Msg_ReadLong(&net_message);
}

static char *Cl_ReadString(void) {
	return Msg_ReadString(&net_message);
}

static void Cl_ReadPosition(vec3_t pos) {
	Msg_ReadPos(&net_message, pos);
}

static void Cl_ReadDirection(vec3_t dir) {
	Msg_ReadDir(&net_message, dir);
}

static float Cl_ReadAngle(void) {
	return Msg_ReadAngle(&net_message);
}

/*
 * Cl_ConfigString
 */
static char *Cl_ConfigString(unsigned short index) {

	if (index > MAX_CONFIG_STRINGS) {
		Com_Warn("Cl_ConfigString: bad index %i.\n", index);
		return "";
	}

	return cl.config_strings[index];
}

/*
 * Cl_InitCgame
 */
void Cl_InitCgame(void) {
	cg_import_t import;

	if (cls.cgame) {
		Cl_ShutdownCgame();
	}

	Com_Print("Client initialization...\n");

	memset(&import, 0, sizeof(import));

	import.Print = Com_Print;
	import.Debug = Com_Debug;
	import.Warn = Com_Warn;
	import.Error = Cl_Error;

	import.Cvar = Cvar_Get;

	import.ConfigString = Cl_ConfigString;

	import.ReadData = Cl_ReadData;
	import.ReadChar = Cl_ReadChar;
	import.ReadByte = Cl_ReadByte;
	import.ReadShort = Cl_ReadShort;
	import.ReadLong = Cl_ReadLong;
	import.ReadString = Cl_ReadString;
	import.ReadPosition = Cl_ReadPosition;
	import.ReadDirection = Cl_ReadDirection;
	import.ReadAngle = Cl_ReadAngle;

	import.client = &cl;

	import.EntityString = Cm_EntityString;

	import.PointContents = R_PointContents;
	import.Trace = R_Trace;

	import.LeafForPoint = R_LeafForPoint;
	import.LeafInPhs = R_LeafInPhs;
	import.LeafInPvs = R_LeafInPvs;

	import.LoadSample = S_LoadSample;
	import.PlaySample = S_PlaySample;
	import.LoopSample = S_LoopSample;

	import.context = &r_context;

	import.view = &r_view;

	import.palette = palette;
	import.ColorFromPalette = Img_ColorFromPalette;

	import.LoadImage = R_LoadImage;
	import.LoadModel = R_LoadModel;

	import.AddCorona = R_AddCorona;
	import.AddEntity = R_AddEntity;
	import.AddLinkedEntity = R_AddLinkedEntity;
	import.AddLight = R_AddLight;
	import.AddParticle = R_AddParticle;
	import.AddSustainedLight = R_AddSustainedLight;

	import.DrawPic = R_DrawPic;
	import.DrawFill = R_DrawFill;

	import.BindFont = R_BindFont;
	import.StringWidth = R_StringWidth;
	import.DrawString = R_DrawString;

	cls.cgame = Sys_LoadLibrary("cgame", &cgame_handle, "Cg_LoadCgame", &import);

	if (!cls.cgame) {
		Com_Error(ERR_DROP, "Failed to load client game\n");
	}

	if (cls.cgame->api_version != CGAME_API_VERSION) {
		Com_Error(ERR_DROP, "Client game has wrong version (%d)\n", cls.cgame->api_version);
	}

	cls.cgame->Init();

	Com_Print("Client initialized.\n");
	Com_InitSubsystem(Q2W_CGAME);
}
