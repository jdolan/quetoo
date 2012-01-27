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

#include "cvar.h"
#include "filesystem.h"

cvar_t *cvar_vars;
boolean_t user_info_modified;

/*
 * Cvar_InfoValidate
 */
static boolean_t Cvar_InfoValidate(const char *s) {
	if (strstr(s, "\\"))
		return false;
	if (strstr(s, "\""))
		return false;
	if (strstr(s, ";"))
		return false;
	return true;
}

/*
 * Cvar_Get_
 */
static cvar_t *Cvar_Get_(const char *name) {
	cvar_t *var;

	// TODO: hash table
	for (var = cvar_vars; var; var = var->next)
		if (!strcmp(name, var->name))
			return var;

	return NULL;
}

/*
 * Cvar_GetValue
 */
float Cvar_GetValue(const char *name) {
	cvar_t *var;

	var = Cvar_Get_(name);

	if (!var)
		return 0.0f;

	return atof(var->string);
}

/*
 * Cvar_GetString
 */
char *Cvar_GetString(const char *name) {
	cvar_t *var;

	var = Cvar_Get_(name);

	if (!var)
		return "";

	return var->string;
}

/*
 * Cvar_CompleteVar
 */
int Cvar_CompleteVar(const char *partial, const char *matches[]) {
	cvar_t *cvar;
	int len;
	int m;

	len = strlen(partial);
	m = 0;

	// check for partial matches
	for (cvar = cvar_vars; cvar; cvar = cvar->next) {
		if (!strncmp(partial, cvar->name, len)) {
			Com_Print("^2%s^7 is \"%s\"\n", cvar->name, cvar->string);
			if (cvar->description)
				Com_Print("\t%s\n", cvar->description);
			matches[m] = cvar->name;
			m++;
		}
	}

	return m;
}

/*
 * Cvar_Get
 *
 * If the variable already exists, the value will not be set. The flags,
 * however, are always OR'ed into the variable.
 */
cvar_t *Cvar_Get(const char *name, const char *value, int flags,
		const char *description) {
	cvar_t *v, *var;

	if (flags & (CVAR_USER_INFO | CVAR_SERVER_INFO)) {
		if (!Cvar_InfoValidate(name)) {
			Com_Print("Invalid variable name: %s\n", name);
			return NULL;
		}
	}

	var = Cvar_Get_(name);
	if (var) {
		var->flags |= flags;
		if (description)
			var->description = description;
		return var;
	}

	if (!value)
		return NULL;

	if (flags & (CVAR_USER_INFO | CVAR_SERVER_INFO)) {
		if (!Cvar_InfoValidate(value)) {
			Com_Print("Invalid variable value: %s\n", value);
			return NULL;
		}
	}

	var = Z_Malloc(sizeof(*var));
	var->name = Z_CopyString(name);
	var->default_value = Z_CopyString(value);
	var->string = Z_CopyString(value);
	var->modified = true;
	var->value = atof(var->string);
	var->integer = atoi(var->string);
	var->flags = flags;
	var->description = description;

	// link the variable in
	if (!cvar_vars) {
		cvar_vars = var;
		return var;
	}

	v = cvar_vars;
	while (v->next) {
		if (strcmp(var->name, v->next->name) < 0) { // insert it
			var->next = v->next;
			v->next = var;
			return var;
		}
		v = v->next;
	}

	v->next = var;
	return var;
}

/*
 * Cvar_Set_
 */
static cvar_t *Cvar_Set_(const char *name, const char *value, boolean_t force) {
	cvar_t *var;

	var = Cvar_Get_(name);
	if (!var) { // create it
		return Cvar_Get(name, value, 0, NULL);
	}

	if (var->flags & (CVAR_USER_INFO | CVAR_SERVER_INFO)) {
		if (!Cvar_InfoValidate(value)) {
			Com_Print("Invalid info value\n");
			return var;
		}
	}

	if (!force) {
		if (var->flags & CVAR_LO_ONLY) {
			if ((Com_WasInit(Q2W_CLIENT) && !Com_WasInit(Q2W_SERVER))
					|| Cvar_GetValue("sv_max_clients") > 1.0) {
				Com_Print("%s is only available offline.\n", name);
				return var;
			}
		}
		if (var->flags & CVAR_NO_SET) {
			Com_Print("%s is write protected.\n", name);
			return var;
		}

		if (var->flags & CVAR_LATCH) {
			if (var->latched_string) {
				if (strcmp(value, var->latched_string) == 0)
					return var;
				Z_Free(var->latched_string);
			} else {
				if (strcmp(value, var->string) == 0)
					return var;
			}

			if (Com_WasInit(Q2W_SERVER)) {
				Com_Print("%s will be changed for next game.\n", name);
				var->latched_string = Z_CopyString(value);
			} else {
				var->string = Z_CopyString(value);
				var->value = atof(var->string);
				var->integer = atoi(var->string);
			}
			return var;
		}
	} else {
		if (var->latched_string) {
			Z_Free(var->latched_string);
			var->latched_string = NULL;
		}
	}

	if (var->flags & CVAR_R_MASK)
		Com_Print("%s will be changed on ^3r_restart^7.\n", name);

	if (var->flags & CVAR_S_MASK)
		Com_Print("%s will be changed on ^3s_restart^7.\n", name);

	if (!strcmp(value, var->string))
		return var; // not changed

	var->modified = true;

	if (var->flags & CVAR_USER_INFO)
		user_info_modified = true; // transmit at next opportunity

	Z_Free(var->string); // free the old value string

	var->string = Z_CopyString(value);
	var->value = atof(var->string);
	var->integer = atoi(var->string);

	return var;
}

/*
 * Cvar_ForceSet
 */
cvar_t *Cvar_ForceSet(const char *name, const char *value) {
	return Cvar_Set_(name, value, true);
}

/*
 * Cvar_Set
 */
cvar_t *Cvar_Set(const char *name, const char *value) {
	return Cvar_Set_(name, value, false);
}

/*
 * Cvar_FullSet
 */
cvar_t *Cvar_FullSet(const char *name, const char *value, int flags) {
	cvar_t *var;

	var = Cvar_Get_(name);
	if (!var) { // create it
		return Cvar_Get(name, value, flags, NULL);
	}

	var->modified = true;

	if (var->flags & CVAR_USER_INFO)
		user_info_modified = true; // transmit at next opportunity

	Z_Free(var->string); // free the old value string

	var->string = Z_CopyString(value);
	var->value = atof(var->string);
	var->integer = atoi(var->string);
	var->flags = flags;

	return var;
}

/*
 * Cvar_SetValue
 */
void Cvar_SetValue(const char *name, float value) {
	char val[32];

	if (value == (int) value)
		snprintf(val, sizeof(val), "%i",(int)value);
	else
		snprintf(val, sizeof(val), "%f", value);

	Cvar_Set(name, val);
}

/*
 * Cvar_Toggle
 */
void Cvar_Toggle(const char *name) {
	cvar_t *var;

	var = Cvar_Get_(name);

	if (!var) {
		Com_Print("\"%s\" is not set\n", name);
		return;
	}

	if (var->value)
		Cvar_SetValue(name, 0.0);
	else
		Cvar_SetValue(name, 1.0);
}

/*
 * Cvar_ResetLocalVars
 *
 * Reset CVAR_LO_ONLY to their default values.
 */
void Cvar_ResetLocalVars(void) {
	cvar_t *var;

	if ((Com_WasInit(Q2W_CLIENT) && !Com_WasInit(Q2W_SERVER))
			|| Cvar_GetValue("sv_max_clients") > 1.0) {

		for (var = cvar_vars; var; var = var->next) {
			if (var->flags & CVAR_LO_ONLY) {
				if (var->default_value) {
					Cvar_FullSet(var->name, var->default_value, var->flags);
				}
			}
		}
	}
}

/*
 * Cvar_PendingLatchedVars
 *
 * Returns true if there are any CVAR_LATCH variables pending.
 */
boolean_t Cvar_PendingLatchedVars(void) {
	cvar_t *var;

	for (var = cvar_vars; var; var = var->next) {
		if (var->latched_string)
			return true;
	}

	return false;
}

/*
 * Cvar_UpdateLatchedVars
 *
 * Apply any pending latched changes.
 */
void Cvar_UpdateLatchedVars(void) {
	cvar_t *var;

	for (var = cvar_vars; var; var = var->next) {

		if (!var->latched_string)
			continue;

		Z_Free(var->string);
		var->string = var->latched_string;
		var->latched_string = NULL;
		var->value = atof(var->string);

		// a little hack here to add new game modules to the searchpath
		if (!strcmp(var->name, "game")) {
			Fs_SetGame(var->string);
			Fs_ExecAutoexec();
		}
	}
}

/*
 * Cvar_PendingVars
 */
boolean_t Cvar_PendingVars(int flags) {
	cvar_t *var;

	for (var = cvar_vars; var; var = var->next) {
		if ((var->flags & flags) && var->modified)
			return true;
	}

	return false;
}

/*
 * Cvar_ClearVars
 */
void Cvar_ClearVars(int flags) {
	cvar_t *var;

	for (var = cvar_vars; var; var = var->next) {
		if ((var->flags & flags) && var->modified)
			var->modified = false;
	}
}

/*
 * Cvar_Command
 *
 * Handles variable inspection and changing from the console
 */
boolean_t Cvar_Command(void) {
	cvar_t *v;

	// check variables
	v = Cvar_Get_(Cmd_Argv(0));
	if (!v)
		return false;

	// perform a variable print or set
	if (Cmd_Argc() == 1) {
		Com_Print("\"%s\" is \"%s\"\n", v->name, v->string);
		return true;
	}

	Cvar_Set(v->name, Cmd_Argv(1));
	return true;
}

/*
 * Cvar_Set_f
 *
 * Allows setting and defining of arbitrary cvars from console
 */
static void Cvar_Set_f(void) {
	int flags;
	const int c = Cmd_Argc();

	if (c != 3 && c != 4) {
		Com_Print("Usage: %s <variable> <value> [u | s]\n", Cmd_Argv(0));
		return;
	}

	if (c == 4) {
		if (!strcmp(Cmd_Argv(3), "u"))
			flags = CVAR_USER_INFO;
		else if (!strcmp(Cmd_Argv(3), "s"))
			flags = CVAR_SERVER_INFO;
		else {
			Com_Print("Invalid flags\n");
			return;
		}
		Cvar_FullSet(Cmd_Argv(1), Cmd_Argv(2), flags);
	} else
		Cvar_Set(Cmd_Argv(1), Cmd_Argv(2));
}

/*
 * Cvar_Toggle_f
 *
 * Allows toggling of arbitrary cvars from console
 */
static void Cvar_Toggle_f(void) {
	if (Cmd_Argc() != 2) {
		Com_Print("Usage: toggle <variable>\n");
		return;
	}
	Cvar_Toggle(Cmd_Argv(1));
}

/*
 * Cvar_WriteVariables
 *
 * Appends lines containing "set variable value" for all variables
 * with the archive flag set to true.
 */
void Cvar_WriteVars(const char *path) {
	cvar_t *var;
	char buffer[1024];
	FILE *f;

	f = fopen(path, "a");
	for (var = cvar_vars; var; var = var->next) {
		if (var->flags & CVAR_ARCHIVE) {
			snprintf(buffer, sizeof(buffer), "set %s \"%s\"\n", var->name, var->string);
			fprintf(f, "%s", buffer);
		}
	}
	Fs_CloseFile(f);
}

/*
 * Cvar_List_f
 */
static void Cvar_List_f(void) {
	const cvar_t *var;
	int i;

	i = 0;
	for (var = cvar_vars; var; var = var->next, i++) {
		if (var->flags & CVAR_ARCHIVE)
			Com_Print("*");
		else
			Com_Print(" ");
		if (var->flags & CVAR_USER_INFO)
			Com_Print("U");
		else
			Com_Print(" ");
		if (var->flags & CVAR_SERVER_INFO)
			Com_Print("S");
		else
			Com_Print(" ");
		if (var->flags & CVAR_NO_SET)
			Com_Print("-");
		else if (var->flags & CVAR_LATCH)
			Com_Print("L");
		else
			Com_Print(" ");
		Com_Print(" %s \"%s\"\n", var->name, var->string);
		if (var->description)
			Com_Print("   ^2%s\n", var->description);
	}
	Com_Print("%i cvars\n", i);
}

/*
 * Cvar_BitInfo
 */
static char *Cvar_BitInfo(int bit) {
	static char info[MAX_USER_INFO_STRING];
	const cvar_t *var;

	info[0] = 0;

	for (var = cvar_vars; var; var = var->next) {
		if (var->flags & bit)
			SetUserInfo(info, var->name, var->string);
	}

	return info;
}

// returns an info string containing all the CVAR_USER_INFO cvars
char *Cvar_UserInfo(void) {
	return Cvar_BitInfo(CVAR_USER_INFO);
}

// returns an info string containing all the CVAR_SERVER_INFO cvars
char *Cvar_ServerInfo(void) {
	return Cvar_BitInfo(CVAR_SERVER_INFO);
}

/*
 * Cvar_Init
 *
 * Reads in all archived cvars
 */
void Cvar_Init(void) {
	Cmd_AddCommand("set", Cvar_Set_f, NULL);
	Cmd_AddCommand("toggle", Cvar_Toggle_f, NULL);
	Cmd_AddCommand("cvar_list", Cvar_List_f, NULL);
}
