/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 * *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
 * *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * *
 * See the GNU General Public License for more details.
 * *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "q2wmap.h"

#include <stdio.h>

// stub out engine facilities for q2wmap to link

void Cmd_Init(void){}
void Cmd_AddCommand(const char *cmd_name, xcommand_t function, const char *description){}

int Cmd_Argc(void){
	return 0;
}

char *Cmd_Argv(int arg){
	return NULL;
}

void Cvar_Init(void){}

cvar_t *Cvar_Get(const char *var_name, const char *value, int flags, const char *description){
	return NULL;
}

cvar_t *Cvar_FullSet(const char *var_name, const char *value, int flags){
	return NULL;
}

void Con_Init(void);
void Con_Shutdown(void);
void Con_Print(const char *text);

void Con_Init(void){}
void Con_Shutdown(void){}
void Con_Print(const char *text){
	fputs(text,stdout);
}

void Cbuf_Init(void){}
void Cbuf_AddText(const char *text){}
void Cbuf_InsertText(const char *text){}
void Cbuf_AddEarlyCommands(qboolean clear){}
void Cbuf_AddLateCommands(void){}
void Cbuf_Execute(void){}
void Cbuf_CopyToDefer(void){}
void Cbuf_InsertFromDefer(void){}

void Sv_Init(void){}
void Sv_Shutdown(const char *finalmsg, qboolean reconnect){}
void Sv_Frame(int msec){}

void Cl_Init(void){}
void Cl_Shutdown(void){}
void Cl_Drop(void){}
void Cl_Frame(int msec){}

void Net_Init(void){}
void Netchan_Init(void){}

int c_pointcontents;
int c_traces;
int c_brush_traces;

#ifdef _WIN32
void *dlopen(const char *filename, int flag){
	return NULL;
}

char *dlerror(void){
	return "";
}

void *dlsym(void *handle, const char *symbol){
	return NULL;
}

void dlclose(void *handle){}
#endif
