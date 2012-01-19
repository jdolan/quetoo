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
void Cl_ShutdownCgame(void){

	if(!cls.cgame)
		return;

	cls.cgame->Shutdown();
	cls.cgame = NULL;

	Sys_CloseLibrary(&cgame_handle);
}


/*
 * Cl_Error
 *
 * Abort the server with a game error
 */
static void Cl_Error(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
static void Cl_Error(const char *fmt, ...){
	char msg[MAX_STRING_CHARS];
	va_list	args;

	va_start(args, fmt);
	vsprintf(msg, fmt, args);
	va_end(args);

	Com_Error(ERR_DROP, "Cgame error: %s.\n", msg);
}


/*
 * Cl_ConfigString
 */
static char *Cl_ConfigString(int index){

	if(index < 0 || index > MAX_CONFIG_STRINGS){
		Com_Warn("Cl_ConfigString: bad index %i.\n", index);
		return "";
	}

	return cl.config_strings[index];
}


/*
 * Cl_InitCgame
 */
void Cl_InitCgame(void){
	cg_import_t import;

	if(cls.cgame){
		Cl_ShutdownCgame();
	}

	memset(&import, 0, sizeof(import));

	import.Print = Com_Print;
	import.Debug = Com_Debug;
	import.Warn = Com_Warn;
	import.Error = Cl_Error;

	import.Cvar = Cvar_Get;

	import.ConfigString = Cl_ConfigString;

	import.width = &r_context.width;
	import.height = &r_context.height;

	import.time = &cl.time;

	import.palette = palette;

	import.LoadPic = R_LoadPic;
	import.DrawPic = R_DrawPic;
	import.DrawFill = R_DrawFill;

	import.BindFont = R_BindFont;
	import.StringWidth = R_StringWidth;
	import.DrawString = R_DrawString;

	cls.cgame = Sys_LoadLibrary("cgame", &cgame_handle, "Cg_LoadCgame", &import);

	if(!cls.cgame){
		Com_Error(ERR_DROP, "Failed to load cgame\n");
	}

	if(cls.cgame->api_version != CGAME_API_VERSION){
		Com_Error(ERR_DROP, "Cgame has wrong version (%d)\n", cls.cgame->api_version);
	}

	cls.cgame->Init();
}
