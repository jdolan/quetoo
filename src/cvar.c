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

#include "console.h"
#include "filesystem.h"

static GHashTable *cvar_vars;

_Bool cvar_user_info_modified;

/**
 * @return True if the specified string appears to be a valid "info" string.
 */
static _Bool Cvar_InfoValidate(const char *s) {
	if (!s) {
		return false;
	}
	if (strstr(s, "\\")) {
		return false;
	}
	if (strstr(s, "\"")) {
		return false;
	}
	if (strstr(s, ";")) {
		return false;
	}
	return true;
}

/**
 * @return The variable by the specified name, or `NULL`.
 */
static cvar_t *Cvar_Get_(const char *name, const _Bool case_sensitive) {

	if (cvar_vars) {
		const GQueue *queue = (GQueue *) g_hash_table_lookup(cvar_vars, name);

		if (queue) {
			if (queue->length == 1) { // only 1 entry, return it
				cvar_t *cvar = (cvar_t *) queue->head->data;

				if (!case_sensitive || g_strcmp0(cvar->name, name) == 0) {
					return cvar;
				}
			} else {
				// only return the exact match
				for (const GList *list = queue->head; list; list = list->next) {
					cvar_t *cvar = (cvar_t *) list->data;

					if (!g_strcmp0(cvar->name, name)) {
						return cvar;
					}
				}
			}
		}
	}

	return NULL;
}

/**
 * @return The variable by the specified name, or `NULL`.
 */
cvar_t *Cvar_Get(const char *name) {

	return Cvar_Get_(name, false);
}

/**
 * @return The numeric value for the specified variable, or 0.
 */
vec_t Cvar_GetValue(const char *name) {
	cvar_t *var;

	var = Cvar_Get(name);

	if (!var) {
		return 0.0f;
	}

	return atof(var->string);
}

/**
 * @return The string for the specified variable, or the empty string.
 */
const char *Cvar_GetString(const char *name) {
	cvar_t *var;

	var = Cvar_Get(name);

	if (!var) {
		return "";
	}

	return var->string;
}

/**
 * @brief Print a cvar to the console.
 */
static const char *Cvar_Stringify(const cvar_t *var) {
	GList *modifiers = NULL;

	if (var->flags & CVAR_ARCHIVE) {
		modifiers = g_list_append(modifiers, "^2archived^7");
	}

	if (var->flags & CVAR_USER_INFO) {
		modifiers = g_list_append(modifiers, "^4user^7");
	}

	if (var->flags & CVAR_SERVER_INFO) {
		modifiers = g_list_append(modifiers, "^5server^7");
	}

	if (var->flags & CVAR_DEVELOPER) {
		modifiers = g_list_append(modifiers, "^1developer^7");
	}

	if (var->flags & CVAR_NO_SET) {
		modifiers = g_list_append(modifiers, "^3readonly^7");
	}

	if (var->flags & CVAR_LATCH) {
		modifiers = g_list_append(modifiers, "^6latched^7");
	}

	static char str[MAX_STRING_CHARS];
	g_snprintf(str, sizeof(str), "%s \"^3%s^7\"", var->name, var->string);

	if (g_strcmp0(var->string, var->default_value)) {
		g_strlcat(str, va(" [\"^3%s^7\"]", var->default_value), sizeof(str));
	}

	if (modifiers) {
		g_strlcat(str, " (", sizeof(str));

		const guint len = g_list_length(modifiers);
		for (guint i = 0; i < len; i++) {
			if (i) {
				g_strlcat(str, ", ", sizeof(str));
			}
			g_strlcat(str, (char *) g_list_nth_data(modifiers, i), sizeof(str));
		}

		g_strlcat(str, ")", sizeof(str));
		g_list_free(modifiers);
	}

	if (var->description) {
		g_strlcat(str, va("\n  ^2%s^7", var->description), sizeof(str));
	}

	return str;
}

/**
 * @brief Enumerates all known variables with the given function.
 */
void Cvar_Enumerate(CvarEnumerateFunc func, void *data) {
	GHashTableIter iter;
	gpointer key, value;
	g_hash_table_iter_init(&iter, cvar_vars);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		const GQueue *queue = (GQueue *) value;

		for (const GList *list = queue->head; list; list = list->next) {
			func((cvar_t *) list->data, data);
		}
	}
}

static char cvar_complete_pattern[MAX_STRING_CHARS];

/**
 * @brief Enumeration helper for Cvar_CompleteVar.
 */
static void Cvar_CompleteVar_enumerate(cvar_t *var, void *data) {
	GList **matches = (GList **) data;

	if (GlobMatch(cvar_complete_pattern, var->name, GLOB_CASE_INSENSITIVE)) {
		*matches = g_list_insert_sorted(*matches, Com_AllocMatch(var->name, Cvar_Stringify(var)), Com_MatchCompare);
	}
}

/**
 * @brief Console completion for console variables.
 */
void Cvar_CompleteVar(const char *pattern, GList **matches) {
	g_strlcpy(cvar_complete_pattern, pattern, sizeof(cvar_complete_pattern));
	Cvar_Enumerate(Cvar_CompleteVar_enumerate, (void *) matches);
}

/**
 * @brief If the variable already exists, the value will not be modified. The
 * default value, flags, and description will, however, be updated. This way,
 * variables set at the command line can receive their meta data through the
 * various subsystem initialization routines.
 */
cvar_t *Cvar_Add(const char *name, const char *value, uint32_t flags, const char *description) {

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

	// update existing variables with meta data from owning subsystem
	cvar_t *var = Cvar_Get_(name, true);
	if (var) {
		if (value) {
			if (var->default_value) {
				Mem_Free((void *) var->default_value);
			}
			var->default_value = Mem_Link(Mem_TagCopyString(value, MEM_TAG_CVAR), var);
		}
		var->flags |= flags;
		if (description) {
			if (var->description) {
				Mem_Free((void *) var->description);
			}
			var->description = Mem_Link(Mem_TagCopyString(description, MEM_TAG_CVAR), var);
		}
		return var;
	}

	if (!value) { // don't create variables if a value isn't provided
		return NULL;
	}

	// create a new variable
	var = Mem_TagMalloc(sizeof(*var), MEM_TAG_CVAR);

	var->name = Mem_Link(Mem_TagCopyString(name, MEM_TAG_CVAR), var);
	var->default_value = Mem_Link(Mem_TagCopyString(value, MEM_TAG_CVAR), var);
	var->string = Mem_Link(Mem_TagCopyString(value, MEM_TAG_CVAR), var);
	var->value = strtof(var->string, NULL);
	var->integer = (int32_t) strtol(var->string, NULL, 0);
	var->modified = true;
	var->flags = flags;

	if (description) {
		var->description = Mem_Link(Mem_TagCopyString(description, MEM_TAG_CVAR), var);
	}

	gpointer key = (gpointer) var->name;
	GQueue *queue = (GQueue *) g_hash_table_lookup(cvar_vars, key);

	if (!queue) {
		queue = g_queue_new();
		g_hash_table_insert(cvar_vars, key, queue);
	}

	g_queue_push_head(queue, var);
	return var;
}

/**
 * @brief
 */
static cvar_t *Cvar_Set_(const char *name, const char *value, _Bool force) {
	cvar_t *var;

	var = Cvar_Get_(name, true);
	if (!var) { // create it
		return Cvar_Add(name, value, 0, NULL);
	}

	if (var->flags & (CVAR_USER_INFO | CVAR_SERVER_INFO)) {
		if (!Cvar_InfoValidate(value)) {
			Com_Print("Invalid info value for %s: %s\n", var->name, value);
			return var;
		}
	}

	// if not forced, honor flags-based logic
	if (!force) {

		// command line variables retain their value through initialization
		if (var->flags & CVAR_CLI) {
			if (!Com_WasInit(QUETOO_CLIENT) && !Com_WasInit(QUETOO_SERVER)) {
				Com_Debug(DEBUG_CONSOLE, "%s: retaining value \"%s\" from command line.\n", name, var->string);
				return var;
			}
		}

		// developer variables can not be modified when in multiplayer mode
		if (var->flags & CVAR_DEVELOPER) {
			if (!Com_WasInit(QUETOO_SERVER)) {
				Com_Print("%s is only available offline.\n", name);
				return var;
			}
		}

		// write-protected variables can never be modified
		if (var->flags & CVAR_NO_SET) {
			Com_Print("%s is write protected.\n", name);
			return var;
		}

		// while latched variables can only be changed on map load
		if (var->flags & CVAR_LATCH) {
			if (var->latched_string) {
				if (!g_strcmp0(value, var->latched_string)) {
					return var;
				}
				Mem_Free(var->latched_string);
			} else {
				if (!g_strcmp0(value, var->string)) {
					return var;
				}
			}

			if (Com_WasInit(QUETOO_SERVER)) {
				Com_Print("%s will be changed for next game.\n", name);
				var->latched_string = Mem_Link(Mem_TagCopyString(value, MEM_TAG_CVAR), var);
			} else {
				if (var->string) {
					Mem_Free(var->string);
				}
				var->string = Mem_Link(Mem_TagCopyString(value, MEM_TAG_CVAR), var);
				var->value = strtof(var->string, NULL);
				var->integer = (int32_t) strtol(var->string, NULL, 0);
				var->modified = true;
			}
			return var;
		}
	} else {
		if (var->latched_string) {
			Mem_Free(var->latched_string);
			var->latched_string = NULL;
		}
	}

	if (!g_strcmp0(value, var->string)) {
		return var;    // not changed
	}

	if (!force) {

		if (var->flags & CVAR_R_MASK) {
			Com_Print("%s will be changed on ^3r_restart^7.\n", name);
		}

		if (var->flags & CVAR_S_MASK) {
			Com_Print("%s will be changed on ^3s_restart^7.\n", name);
		}
	}

	if (var->flags & CVAR_USER_INFO) {
		cvar_user_info_modified = true;    // transmit at next opportunity
	}

	Mem_Free(var->string);

	var->string = Mem_Link(Mem_CopyString(value), var);
	var->value = strtof(var->string, NULL);
	var->integer = (int32_t) strtol(var->string, NULL, 0);

	var->modified = true;

	return var;
}

/**
 * @brief
 */
cvar_t *Cvar_ForceSet(const char *name, const char *value) {
	return Cvar_Set_(name, value, true);
}

/**
 * @brief
 */
cvar_t *Cvar_Set(const char *name, const char *value) {
	return Cvar_Set_(name, value, false);
}

/**
 * @brief
 */
cvar_t *Cvar_FullSet(const char *name, const char *value, uint32_t flags) {
	cvar_t *var;

	var = Cvar_Get_(name, true);
	if (!var) { // create it
		return Cvar_Add(name, value, flags, NULL);
	}

	if (var->flags & CVAR_USER_INFO) {
		cvar_user_info_modified = true; // transmit at next opportunity
	}

	Mem_Free(var->string);

	var->string = Mem_Link(Mem_CopyString(value), var);
	var->value = strtof(var->string, NULL);
	var->integer = (int32_t) strtol(var->string, NULL, 0);
	var->flags = flags;
	var->modified = true;

	return var;
}

/**
 * @brief
 */
cvar_t *Cvar_SetValue(const char *name, vec_t value) {
	char val[32];

	if (value == (int32_t) value) {
		g_snprintf(val, sizeof(val), "%i", (int32_t) value);
	} else {
		g_snprintf(val, sizeof(val), "%f", value);
	}

	return Cvar_Set(name, val);
}

/**
 * @brief
 */
cvar_t *Cvar_Toggle(const char *name) {

	cvar_t *var = Cvar_Get_(name, true) ? : Cvar_Add(name, "0", 0, NULL);

	if (var->value) {
		return Cvar_SetValue(name, 0.0);
	} else {
		return Cvar_SetValue(name, 1.0);
	}
}

/**
 * @brief Enumeration helper for Car_ResetDeveloper.
 */
static void Car_ResetDeveloper_enumerate(cvar_t *var, void *data) {

	if (var->flags & CVAR_DEVELOPER) {
		if (var->default_value) {
			Cvar_FullSet(var->name, var->default_value, var->flags);
		}
	}
}

/**
 * @brief Reset CVAR_DEVELOPER to their default values.
 */
void Car_ResetDeveloper(void) {

	if (!Com_WasInit(QUETOO_SERVER)) {
		Cvar_Enumerate(Car_ResetDeveloper_enumerate, NULL);
	}
}

/**
 * @brief Enumeration helper for Cvar_PendingLatched.
 */
static void Cvar_PendingLatched_enumerate(cvar_t *var, void *data) {

	if (var->latched_string) {
		*((_Bool *) data) = true;
	}
}

/**
 * @brief Returns true if there are any CVAR_LATCH variables pending.
 */
_Bool Cvar_PendingLatched(void) {
	_Bool pending = false;

	Cvar_Enumerate(Cvar_PendingLatched_enumerate, (void *) &pending);

	return pending;
}

/**
 * @brief Enumeration helper for Cvar_UpdateLatched.
 */
static void Cvar_UpdateLatched_enumerate(cvar_t *var, void *data) {

	if (var->latched_string) {
		Mem_Free(var->string);

		var->string = var->latched_string;
		var->latched_string = NULL;
		var->value = strtof(var->string, NULL);
		var->integer = (int32_t) strtol(var->string, NULL, 0);
		var->modified = true;
	}
}

/**
 * @brief Apply any pending latched changes.
 */
void Cvar_UpdateLatched(void) {
	Cvar_Enumerate(Cvar_UpdateLatched_enumerate, NULL);
}

static _Bool cvar_pending;

/**
 * @brief Enumeration helper for Cvar_Pending.
 */
static void Cvar_Pending_enumerate(cvar_t *var, void *data) {
	uint32_t flags = *((uint32_t *) data);

	if ((var->flags & flags) && var->modified) {
		cvar_pending = true;
	}
}

/**
 * @brief Returns true if any variables whose flags match the specified mask are pending.
 */
_Bool Cvar_Pending(uint32_t flags) {
	cvar_pending = false;

	Cvar_Enumerate(Cvar_Pending_enumerate, (void *) &flags);

	return cvar_pending;
}

/**
 * @brief Enumeration helper for Cvar_ClearAll.
 */
static void Cvar_ClearAll_enumerate(cvar_t *var, void *data) {
	uint32_t flags = *((uint32_t *) data);

	if ((var->flags & flags) && var->modified) {
		var->modified = false;
	}
}

/**
 * @brief Clears the modified flag on all variables matching the specified mask.
 */
void Cvar_ClearAll(uint32_t flags) {
	Cvar_Enumerate(Cvar_ClearAll_enumerate, (void *) &flags);
}

/**
 * @brief Handles variable inspection and changing from the console
 */
_Bool Cvar_Command(void) {
	cvar_t *var;

	// check variables
	var = Cvar_Get(Cmd_Argv(0));
	if (!var) {
		return false;
	}

	// perform a variable print or set
	if (Cmd_Argc() == 1) {
		Com_Print("%s\n", Cvar_Stringify(var));
		return true;
	}

	Cvar_Set(var->name, Cmd_Argv(1));
	return true;
}

/**
 * @brief Set command autocompletion.
 */
static void Cvar_Set_Autocomplete_f(const uint32_t argi, GList **matches) {
	const char *pattern = va("%s*", Cmd_Argv(argi));
	Cvar_CompleteVar(pattern, matches);
}

/**
 * @brief Allows setting and defining of arbitrary cvars from console
 */
static void Cvar_Set_f(void) {

	if (Cmd_Argc() != 3) {
		Com_Print("Usage: %s <variable> <value>\n", Cmd_Argv(0));
		return;
	}

	if (!g_strcmp0("seta", Cmd_Argv(0))) {
		Cvar_FullSet(Cmd_Argv(1), Cmd_Argv(2), CVAR_ARCHIVE);
	} else if (!g_strcmp0("sets", Cmd_Argv(0))) {
		Cvar_FullSet(Cmd_Argv(1), Cmd_Argv(2), CVAR_SERVER_INFO);
	} else if (!g_strcmp0("setu", Cmd_Argv(0))) {
		Cvar_FullSet(Cmd_Argv(1), Cmd_Argv(2), CVAR_USER_INFO);
	} else {
		Cvar_Set(Cmd_Argv(1), Cmd_Argv(2));
	}
}

/**
 * @brief Allows toggling of arbitrary cvars from console.
 */
static void Cvar_Toggle_f(void) {
	if (Cmd_Argc() != 2) {
		Com_Print("Usage: toggle <variable>\n");
		return;
	}
	Cvar_Toggle(Cmd_Argv(1));
}

/**
 * @brief Enumeration helper for Cvar_List_f.
 */
static void Cvar_List_f_enumerate(cvar_t *var, void *data) {
	Com_Print("%s\n", Cvar_Stringify(var));
}

/**
 * @brief Lists all known console variables.
 */
static void Cvar_List_f(void) {
	Cvar_Enumerate(Cvar_List_f_enumerate, NULL);
}

/**
 * @brief Enumeration helper for Cvar_Userinfo.
 */
static void Cvar_UserInfo_enumerate(cvar_t *var, void *data) {

	if (var->flags & CVAR_USER_INFO) {
		SetUserInfo((char *) data, var->name, var->string);
	}
}

/**
 * @brief Returns an info string containing all the CVAR_USER_INFO cvars.
 */
char *Cvar_UserInfo(void) {
	static char info[MAX_USER_INFO_STRING];

	memset(info, 0, sizeof(info));

	Cvar_Enumerate(Cvar_UserInfo_enumerate, (void *) info);

	return info;
}

/**
 * @brief Enumeration helper for Cvar_ServerInfo.
 */
static void Cvar_ServerInfo_enumerate(cvar_t *var, void *data) {

	if (var->flags & CVAR_SERVER_INFO) {
		SetUserInfo((char *) data, var->name, var->string);
	}
}

/**
 * @return An info string containing all the CVAR_SERVER_INFO cvars.
 */
char *Cvar_ServerInfo(void) {
	static char info[MAX_USER_INFO_STRING];

	memset(info, 0, sizeof(info));

	Cvar_Enumerate(Cvar_ServerInfo_enumerate, (void *) info);

	return info;
}

/**
 * @brief Enumeration helper for Cl_WriteVariables.
 */
static void Cvar_WriteVariables_enumerate(cvar_t *var, void *data) {

	if (var->flags & CVAR_ARCHIVE) {
		Fs_Print((file_t *) data, "set %s \"%s\"\n", var->name, var->string);
	}
}

/**
 * @brief Writes all variables to the specified file.
 */
void Cvar_WriteAll(file_t *f) {
	Cvar_Enumerate(Cvar_WriteVariables_enumerate, (void *) f);
}

static GRegex *cvar_emplace_regex = NULL;

/**
 * @brief
 */
static gboolean Cvar_ExpandString_EvalCallback(const GMatchInfo *match_info, GString *result, gpointer data) {
	gchar *name = g_match_info_fetch(match_info, 1);
	const char *value = Cvar_GetString(name);
	g_string_append(result, value);
	g_free(name);
	return false;
}

/**
 * @brief Replaces cvar replacement strings (ie, $cvar_name_here) with their string values.
 * @returns false if no emplacement was performed (or could be performed), otherwise the *output is set
 * to a valid GString.
 */
_Bool Cvar_ExpandString(const char *input, const size_t in_size, GString **output) {
	// sanity checks
	if (!input || !in_size) {
		return false;
	}

	GError *error = NULL;
	gchar *replaced = g_regex_replace_eval(cvar_emplace_regex, input, in_size, 0, 0, Cvar_ExpandString_EvalCallback, NULL,
	                                       &error);

	if (error) {
		Com_Warn("Error preprocessing shader: %s", error->message);
	}

	*output = g_string_new(replaced);
	g_free(replaced);
	return true;
}

/**
 * @brief
 */
static void Cvar_HashTable_Free(gpointer list) {

	g_queue_free_full((GQueue *) list, Mem_Free);
}

/**
 * @brief Initializes the console variable subsystem. Parses the command line
 * arguments looking for "+set" directives. Any variables initialized at the
 * command line will be flagged as "sticky" and retain their values through
 * initialization.
 */
void Cvar_Init(void) {

	cvar_vars = g_hash_table_new_full(g_stri_hash, g_stri_equal, NULL, Cvar_HashTable_Free);

	cmd_t *set_cmd = Cmd_Add("set", Cvar_Set_f, 0, "Set a console variable");
	cmd_t *seta_cmd = Cmd_Add("seta", Cvar_Set_f, 0, "Set an archived console variable");
	cmd_t *sets_cmd = Cmd_Add("sets", Cvar_Set_f, 0, "Set a server-info console variable");
	cmd_t *setu_cmd = Cmd_Add("setu", Cvar_Set_f, 0, "Set a user-info console variable");

	Cmd_SetAutocomplete(set_cmd, Cvar_Set_Autocomplete_f);
	Cmd_SetAutocomplete(seta_cmd, Cvar_Set_Autocomplete_f);
	Cmd_SetAutocomplete(sets_cmd, Cvar_Set_Autocomplete_f);
	Cmd_SetAutocomplete(setu_cmd, Cvar_Set_Autocomplete_f);

	cmd_t *toggle_cmd = Cmd_Add("toggle", Cvar_Toggle_f, 0, "Toggle a cvar between 0 and 1");

	Cmd_SetAutocomplete(toggle_cmd, Cvar_Set_Autocomplete_f);

	Cmd_Add("cvar_list", Cvar_List_f, 0, NULL);

	for (int32_t i = 1; i < Com_Argc(); i++) {
		const char *s = Com_Argv(i);

		if (!strncmp(s, "+set", 4)) {
			Cmd_ExecuteString(va("%s %s %s\n", Com_Argv(i) + 1, Com_Argv(i + 1), Com_Argv(i + 2)));

			cvar_t *var = Cvar_Get_(Com_Argv(i + 1), true);
			if (var) {
				var->flags |= CVAR_CLI;
			} else {
				Com_Warn("Failed to set \"%s\"\n", Com_Argv(i + 1));
			}

			i += 2;
		}
	}

	// this only needs to be done once
	if (!cvar_emplace_regex) {
		GError *error = NULL;
		cvar_emplace_regex = g_regex_new("\\$([a-z0-9_]+)", G_REGEX_CASELESS | G_REGEX_MULTILINE | G_REGEX_DOTALL, 0, &error);

		if (error) {
			Com_Warn("Error compiling regex: %s", error->message);
		}
	}
}

/**
 * @brief Shuts down the console variable subsystem.
 */
void Cvar_Shutdown(void) {

	g_hash_table_destroy(cvar_vars);
	cvar_vars = NULL;

	Cmd_Remove("set");
	Cmd_Remove("seta");
	Cmd_Remove("sets");
	Cmd_Remove("setu");
	Cmd_Remove("toggle");
	Cmd_Remove("cvar_list");
}
