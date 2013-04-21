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

#ifndef __CMD_H__
#define __CMD_H__

#include "filesystem.h"

// command buffer functions
void Cbuf_AddText(const char *text);
void Cbuf_InsertText(const char *text);
void Cbuf_AddEarlyCommands(bool clear);
void Cbuf_AddLateCommands(void);
void Cbuf_Execute(void);
void Cbuf_CopyToDefer(void);
void Cbuf_InsertFromDefer(void);

// command parsing functions
int32_t Cmd_Argc(void);
const char *Cmd_Argv(int32_t arg);
const char *Cmd_Args(void);
void Cmd_TokenizeString(const char *text);
void Cmd_ExecuteString(const char *text);

typedef void (*cmd_enumerate_func)(cmd_t *cmd, void *data);

// general command management
cmd_t *Cmd_Get(const char *name);
void Cmd_Enumerate(cmd_enumerate_func func, void *data);
void Cmd_AddCommand(const char *name, cmd_function_t func, uint32_t flags, const char *description);
void Cmd_RemoveCommand(const char *name);
void Cmd_CompleteCommand(const char *pattern, GList **matches);
void Cmd_Init(void);
void Cmd_Shutdown(void);

extern void (*Cmd_ForwardToServer)(void);

#endif /* __CMD_H__ */
