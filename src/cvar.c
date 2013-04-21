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

#include "console.h"
#include "filesystem.h"

typedef struct {
	GHashTable *vars;
} cvar_state_t;

static cvar_state_t cvar_state;

bool user_info_modified;

/*
 * @return True if the specified string appears to be a valid "info" string.
 */
static bool Cvar_InfoValidate(const char *s) {
	if (strstr(s, "\\"))
		return false;
	if (strstr(s, "\""))
		return false;
	if (strstr(s, ";"))
		return false;
	return true;
}

/*
 * @return The variable by the specified name, or NULL.
 */
static cvar_t *Cvar_Get_(const char *name) {

	if (cvar_state.vars) {
		return (cvar_t *) g_hash_table_lookup(cvar_state.vars, name);
	}

	return NULL;
}

/*
 * @return The numeric value for the specified variable, or 0.
 */
float Cvar_GetValue(const char *name) {
	cvar_t *var;

	var = Cvar_Get_(name);

	if (!var)
		return 0.0f;

	return atof(var->string);
}

/*
 * @return The string for the specified variable, or the empty string.
 */
char *Cvar_GetString(const char *name) {
	cvar_t *var;

	var = Cvar_Get_(name);

	if (!var)
		return "";

	return var->string;
}

static cvar_enumerate_func cvar_enumerate;

/*
 * @brief GHashFunc for Cvar_Enumerate.
 */
static void Cvar_Enumerate_(gpointer key __attribute__((unused)), gpointer value, gpointer data) {
	cvar_enumerate((cvar_t *) value, data);
}

/*
 * @brief Enumerates all known variables with the given function.
 */
void Cvar_Enumerate(cvar_enumerate_func func, void *data) {
	cvar_enumerate = func;
	g_hash_table_foreach(cvar_state.vars, Cvar_Enumerate_, data);
}

static const char *cvar_complete_pattern;

/*
 * @brief Enumeration helper for Cvar_CompleteVar.
 */
static void Cvar_CompleteVar_enumerate(cvar_t *var, void *data) {
	GList **matches = (GList **) data;

	if (GlobMatch(cvar_complete_pattern, var->name)) {
		Com_Print("^2%s^7 is \"%s\"\n", var->name, var->string);

		if (var->description)
			Com_Print("\t%s\n", var->description);

		*matches = g_list_prepend(*matches, Z_CopyString(var->name));
	}
}

/*
 * @brief Console completion for console variables.
 */
void Cvar_CompleteVar(const char *pattern, GList **matches) {
	cvar_complete_pattern = pattern;
	Cvar_Enumerate(Cvar_CompleteVar_enumerate, (void *) matches);
}

/*
 * @brief If the variable already exists, the value will not be set. The flags,
 * however, are always OR'ed into the variable.
 */
cvar_t *Cvar_Get(const char *name, const char *value, uint32_t flags, const char *description) {

	if (!value)
		return NULL;

	if (flags & (CVAR_USER_INFO | CVAR_SERVER_INFO)) {
		if (!Cvar_InfoValidate(name)) {
			Com_Print("Invalid variable name: %s\n", name);
			return NULL;
		}
		if (!Cvar_InfoValidate(value)) {
			Com_Print("Invalid variable value: %s\n", value);
			return NULL;
		}
	}

	cvar_t *var = Cvar_Get_(name);
	if (var) {
		return var;
	}

	var = Z_Malloc(sizeof(*var));

	var->name = Z_Link(var, Z_CopyString(name));
	var->default_value = Z_Link(var, Z_CopyString(value));
	var->string = Z_Link(var, Z_CopyString(value));
	var->modified = true;
	var->value = atof(var->string);
	var->integer = atoi(var->string);
	var->flags = flags;

	if (description) {
		var->description = Z_Link(var, Z_CopyString(description));
	}

	g_hash_table_insert(cvar_state.vars, (char *) var->name, var);
	return var;
}

/*
 * @brief
 */
static cvar_t *Cvar_Set_(const char *name, const char *value, bool force) {
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
			if ((Com_WasInit(Q2W_CLIENT) && !Com_WasInit(Q2W_SERVER)) || Cvar_GetValue(
					"sv_max_clients") > 1.0) {
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
				var->latched_string = Z_Link(var, Z_CopyString(value));
			} else {
				if (var->string)
					Z_Free(var->string);
				var->string = Z_Link(var, Z_CopyString(value));
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

	var->string = Z_Link(var, Z_CopyString(value));
	var->value = atof(var->string);
	var->integer = atoi(var->string);

	return var;
}

/*
 * @brief
 */
cvar_t *Cvar_ForceSet(const char *name, const char *value) {
	return Cvar_Set_(name, value, true);
}

/*
 * @brief
 */
cvar_t *Cvar_Set(const char *name, const char *value) {
	return Cvar_Set_(name, value, false);
}

/*
 * @brief
 */
cvar_t *Cvar_FullSet(const char *name, const char *value, uint32_t flags) {
	cvar_t *var;

	var = Cvar_Get_(name);
	if (!var) { // create it
		return Cvar_Get(name, value, flags, NULL);
	}

	var->modified = true;

	if (var->flags & CVAR_USER_INFO)
		user_info_modified = true; // transmit at next opportunity

	Z_Free(var->string); // free the old value string

	var->string = Z_Link(var, Z_CopyString(value));
	var->value = atof(var->string);
	var->integer = atoi(var->string);
	var->flags = flags;

	return var;
}

/*
 * @brief
 */
void Cvar_SetValue(const char *name, float value) {
	char val[32];

	if (value == (int) value)
		g_snprintf(val, sizeof(val), "%i", (int) value);
	else
		g_snprintf(val, sizeof(val), "%f", value);

	Cvar_Set(name, val);
}

/*
 * @brief
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
 * @brief Enumeration helper for Cvar_ResetLocalVars.
 */
void Cvar_ResetLocalVars_enumerate(cvar_t *var, void *data __attribute__((unused))) {

	if (var->flags & CVAR_LO_ONLY) {
		if (var->default_value) {
			Cvar_FullSet(var->name, var->default_value, var->flags);
		}
	}
}

/*
 * @brief Reset CVAR_LO_ONLY to their default values.
 */
void Cvar_ResetLocalVars(void) {

	const int32_t clients = Cvar_GetValue("sv_max_clients");

	if (clients > 1 || (Com_WasInit(Q2W_CLIENT) && !Com_WasInit(Q2W_SERVER))) {
		Cvar_Enumerate(Cvar_ResetLocalVars_enumerate, NULL);
	}
}

/*
 * @brief Enumeration helper for Cvar_PendingLatchedVars.
 */
static void Cvar_PendingLatchedVars_enumerate(cvar_t *var, void *data) {

	if (var->latched_string) {
		*((bool *) data) = true;
	}
}

/*
 * @brief Returns true if there are any CVAR_LATCH variables pending.
 */
bool Cvar_PendingLatchedVars(void) {
	bool pending = false;

	Cvar_Enumerate(Cvar_PendingLatchedVars_enumerate, (void *) &pending);

	return pending;
}

/*
 * @brief Enumeration helper for Cvar_UpdateLatchedVars.
 */
void Cvar_UpdateLatchedVars_enumerate(cvar_t *var, void *data __attribute__((unused))) {

	if (var->latched_string) {
		Z_Free(var->string);

		var->string = var->latched_string;
		var->latched_string = NULL;
		var->value = atof(var->string);
		var->integer = atoi(var->string);

		if (!strcmp(var->name, "game")) {
			Fs_SetGame(var->string);

			if (Fs_Exists("autoexec.cfg")) {
				Cbuf_AddText("exec autoexec.cfg\n");
				Cbuf_Execute();
			}
		}
	}
}

/*
 * @brief Apply any pending latched changes.
 */
void Cvar_UpdateLatchedVars(void) {
	Cvar_Enumerate(Cvar_UpdateLatchedVars_enumerate, NULL);
}

static bool cvar_pending_vars;

/*
 * @brief Enumeration helper for Cvar_PendingVars.
 */
static void Cvar_PendingVars_enumerate(cvar_t *var, void *data) {
	uint32_t flags = *((uint32_t *) data);

	if ((var->flags & flags) && var->modified) {
		cvar_pending_vars = true;
	}
}

/*
 * @brief Returns true if any variables whose flags match the specified mask are pending.
 */
bool Cvar_PendingVars(uint32_t flags) {
	cvar_pending_vars = false;

	Cvar_Enumerate(Cvar_PendingVars_enumerate, (void *) &flags);

	return cvar_pending_vars;
}

/*
 * @brief Enumeration helper for Cvar_ClearVars.
 */
static void Cvar_ClearVars_enumerate(cvar_t *var, void *data) {
	uint32_t flags = *((uint32_t *) data);

	if ((var->flags & flags) && var->modified) {
		var->modified = false;
	}
}

/*
 * @brief Clears the modified flag on all variables matching the specified mask.
 */
void Cvar_ClearVars(uint32_t flags) {
	Cvar_Enumerate(Cvar_ClearVars_enumerate, (void *) &flags);
}

/*
 * @brief Handles variable inspection and changing from the console
 */
bool Cvar_Command(void) {
	cvar_t *var;

	// check variables
	var = Cvar_Get_(Cmd_Argv(0));
	if (!var)
		return false;

	// perform a variable print or set
	if (Cmd_Argc() == 1) {
		Com_Print("\"%s\" is \"%s\"\n", var->name, var->string);
		return true;
	}

	Cvar_Set(var->name, Cmd_Argv(1));
	return true;
}

/*
 * @brief Allows setting and defining of arbitrary cvars from console
 */
static void Cvar_Set_f(void) {
	uint32_t flags;
	const int32_t c = Cmd_Argc();

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
 * @brief Allows toggling of arbitrary cvars from console.
 */
static void Cvar_Toggle_f(void) {
	if (Cmd_Argc() != 2) {
		Com_Print("Usage: toggle <variable>\n");
		return;
	}
	Cvar_Toggle(Cmd_Argv(1));
}

/*
 * @brief Enumeration helper for Cvar_List_f.
 */
static void Cvar_List_f_enumerate(cvar_t *var, void *data __attribute__((unused))) {

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

/*
 * @brief Lists all known console variables.
 */
static void Cvar_List_f(void) {
	Cvar_Enumerate(Cvar_List_f_enumerate, NULL);
}

/*
 * @brief Enumeration helper for Cvar_Userinfo.
 */
static void Cvar_UserInfo_enumerate(cvar_t *var, void *data) {

	if (var->flags & CVAR_USER_INFO) {
		SetUserInfo((char *) data, var->name, var->string);
	}
}

/*
 * @brief Returns an info string containing all the CVAR_USER_INFO cvars.
 */
char *Cvar_UserInfo(void) {
	static char info[MAX_USER_INFO_STRING];

	Cvar_Enumerate(Cvar_UserInfo_enumerate, (void *) info);

	return info;
}

/*
 * @brief Enumeration helper for Cvar_ServerInfo.
 */
static void Cvar_ServerInfo_enumerate(cvar_t *var, void *data) {

	if (var->flags & CVAR_SERVER_INFO) {
		SetUserInfo((char *) data, var->name, var->string);
	}
}

/*
 * @ Returns an info string containing all the CVAR_SERVER_INFO cvars.
 */
char *Cvar_ServerInfo(void) {
	static char info[MAX_USER_INFO_STRING];

	Cvar_Enumerate(Cvar_ServerInfo_enumerate, (void *) info);

	return info;
}

/**
 * @brief Initializes the console variable subsystem.
 */
void Cvar_Init(void) {

	cvar_state.vars = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, Z_Free);

	Cmd_AddCommand("set", Cvar_Set_f, 0, NULL);
	Cmd_AddCommand("toggle", Cvar_Toggle_f, 0, NULL);
	Cmd_AddCommand("cvar_list", Cvar_List_f, 0, NULL);
}

/*
 * Shuts down the console variable subsystem.
 */
void Cvar_Shutdown(void) {

	g_hash_table_destroy(cvar_state.vars);

	Cmd_RemoveCommand("set");
	Cmd_RemoveCommand("toggle");
	Cmd_RemoveCommand("cvar_list");
}
