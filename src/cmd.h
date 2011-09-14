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

/*

Any number of commands can be added in a frame, from several different sources.
Most commands come from either keybindings or console line input, but remote
servers can also send across commands and entire text files can be execed.

The + command line options are also added to the command buffer.
*/

void Cbuf_Init(void);
// allocates an initial text buffer that will grow as needed

void Cbuf_AddText(const char *text);
// as new commands are generated from the console or keybindings,
// the text is added to the end of the command buffer.

void Cbuf_InsertText(const char *text);
// when a command wants to issue other commands immediately, the text is
// inserted at the beginning of the buffer, before any remaining unexecuted
// commands.

void Cbuf_AddEarlyCommands(boolean_t clear);
// adds all the +set commands from the command line

void Cbuf_AddLateCommands(void);
// adds all the remaining + commands from the command line

void Cbuf_Execute(void);
// Pulls off \n terminated lines of text from the command buffer and sends
// them through Cmd_ExecuteString.  Stops when the buffer is empty.
// Normally called once per frame, but may be explicitly invoked.
// Do not call inside a command function!

void Cbuf_CopyToDefer(void);
void Cbuf_InsertFromDefer(void);
// These two functions are used to defer any pending commands while a map
// is being loaded


/*

Command execution takes a null terminated string, breaks it into tokens,
then searches for a command or variable that matches the first token.

*/

typedef void(*xcommand_t)(void);

void Cmd_Init(void);

boolean_t Cmd_Exists(const char *cmd_name);
void Cmd_AddUserdata(const char *cmd_name, void *userdata);
void* Cmd_GetUserdata(const char *cmd_name);
void *Cmd_Userdata(void);

void Cmd_AddCommand(const char *cmd_name, xcommand_t function, const char *description);
// called by the init functions of other parts of the program to
// register commands and functions to call for them.
// The cmd_name is referenced later, so it should not be in temp memory
// if function is NULL, the command will be forwarded to the server
// as a clc_string_cmd instead of executed locally
void Cmd_RemoveCommand(const char *cmd_name);

int Cmd_CompleteCommand(const char *partial, const char *matches[]);
// attempts to match a partial command for automatic command line completion

int Cmd_Argc(void);
char *Cmd_Argv(int arg);
char *Cmd_Args(void);
// The functions that execute commands get their parameters with these
// functions. Cmd_Argv() will return an empty string, not a NULL
// if arg > argc, so string operations are always safe.

void Cmd_TokenizeString(const char *text);
// Takes a null terminated string.  Does not need to be /n terminated.
// breaks the string up into arg tokens.

void Cmd_ExecuteString(const char *text);
// Parses a single line of text into arguments and tries to execute it
// as if it was typed at the console

extern void (*Cmd_ForwardToServer)(void);
// adds the current command line as a clc_string_cmd to the client message.
// things like godmode, no_clip, etc, are commands directed to the server,
// so when they are typed in at the console, they will need to be forwarded.

#endif /* __CMD_H__ */
