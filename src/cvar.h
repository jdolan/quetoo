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

#ifndef __CVAR_H__
#define __CVAR_H__

#include "quake2world.h"

/*

cvar_t variables are used to hold scalar or string variables that
can be changed or displayed at the console or prog code as well
as accessed directly in C code.

The user can access cvars from the console in three ways:
r_draworder			prints the current value
r_draworder 0		sets the current value to 0
set r_draworder 0	as above, but creates the cvar if not present

Cvars are restricted from having the same names as commands to keep this
interface from being ambiguous.
*/

extern cvar_t *cvar_vars;

cvar_t *Cvar_Get(const char *name, const char *value, int flags, const char *description);
// creates the variable if it doesn't exist, or returns the existing one
// if it exists, the value will not be changed, but flags will be ORed in
// that allows variables to be unarchived without needing bitflags

cvar_t *Cvar_Set(const char *name, const char *value);
// will create the variable if it doesn't exist

cvar_t *Cvar_ForceSet(const char *name, const char *value);
// will set the variable even if NOSET or LATCH

cvar_t *Cvar_FullSet(const char *name, const char *value, int flags);

void Cvar_SetValue(const char *name, float value);
// expands value to a string and calls Cvar_Set

void Cvar_Toggle(const char *name);
// toggles value between 0 and 1

float Cvar_GetValue(const char *name);
// returns 0 if not defined or non numeric

char *Cvar_GetString(const char *name);
// returns an empty string if not defined

int Cvar_CompleteVar(const char *partial, const char *matches[]);
// attempts to match a partial variable name for command line completion

void Cvar_ResetLocalVars(void);
// reset CVAR_LO_ONLY variables to their default values

boolean_t Cvar_PendingLatchedVars(void);
// are there pending latch changes?

void Cvar_UpdateLatchedVars(void);
// any CVAR_LATCHED variables that have been set will now take effect

boolean_t Cvar_PendingVars(int flags);
// are there pending changes?

void Cvar_ClearVars(int flags);
// clear modified booleans on vars

boolean_t Cvar_Command(void);
// called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
// command.  Returns true if the command was a variable reference that
// was handled.(print or change)

void Cvar_WriteVars(const char *path);
// appends lines containing "set variable value" for all variables
// with the archive flag set to true.

void Cvar_Init(void);

char *Cvar_UserInfo(void);
// returns an info string containing all the CVAR_USERINFO cvars

char *Cvar_ServerInfo(void);
// returns an info string containing all the CVAR_SERVERINFO cvars

extern boolean_t user_info_modified;
// this is set each time a CVAR_USERINFO variable is changed
// so that the client knows to send it to the server

#endif /* __CVAR_H__ */
