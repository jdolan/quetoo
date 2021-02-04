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

typedef struct cmd_args_s {
	int32_t argc;
	char argv[MAX_STRING_TOKENS][MAX_TOKEN_CHARS];
	char args[MAX_STRING_CHARS];
} cmd_args_t;

#define CBUF_CHARS 8192

typedef struct cmd_state_s {
	GHashTable *commands;

	mem_buf_t buf;
	char buffers[2][CBUF_CHARS];

	cmd_args_t args;

	_Bool wait; // commands may be deferred one frame

	int32_t alias_loop_count;
} cmd_state_t;

static cmd_state_t cmd_state;

#define MAX_ALIAS_LOOP_COUNT 8

/**
 * @brief Adds command text at the end of the buffer
 */
void Cbuf_AddText(const char *text) {

	const size_t l = strlen(text);

	if (cmd_state.buf.size + l >= cmd_state.buf.max_size) {
		Com_Warn("Overflow\n");
		return;
	}

	Mem_WriteBuffer(&cmd_state.buf, text, l);
}

/**
 * @brief Inserts command text at the beginning of the buffer.
 */
void Cbuf_InsertText(const char *text) {

	if (text  && strlen(text)) {
		void *temp;

		// copy off any commands still remaining in the exec buffer
		const size_t size = cmd_state.buf.size;
		if (size) {
			temp = Mem_TagMalloc(size, MEM_TAG_CMD);
			memcpy(temp, cmd_state.buf.data, size);
			Mem_ClearBuffer(&cmd_state.buf);
		} else {
			temp = NULL; // shut up compiler
		}

		// add the entire text of the file
		Cbuf_AddText(text);

		// add the copied off data
		if (size) {
			Mem_WriteBuffer(&cmd_state.buf, temp, size);
			Mem_Free(temp);
		}
	}
}

/**
 * @brief
 */
void Cbuf_CopyToDefer(void) {

	memcpy(cmd_state.buffers[1], cmd_state.buffers[0], cmd_state.buf.size);

	memset(cmd_state.buffers[0], 0, sizeof(cmd_state.buffers[0]));

	cmd_state.buf.size = 0;
}

/**
 * @brief
 */
void Cbuf_InsertFromDefer(void) {

	Cbuf_InsertText(cmd_state.buffers[1]);

	memset(cmd_state.buffers[1], 0, sizeof(cmd_state.buffers[1]));
}

/**
 * @brief Executes the pending command buffer.
 */
void Cbuf_Execute(void) {

	cmd_state.alias_loop_count = 0; // don't allow infinite alias loops

	while (cmd_state.buf.size) {

		// read a single command line from the buffer
		char line[sizeof(cmd_state.args.args)] = "";

		// find a \n or; line break
		char *text = (char *) cmd_state.buf.data;

		uint32_t i, quotes = 0;
		for (i = 0; i < cmd_state.buf.size; i++) {
			if (text[i] == '"') {
				quotes++;
			}
			if (!(quotes & 1) && text[i] == ';') {
				break; // don't break if inside a quoted string
			}
			if (text[i] == '\n') {
				break;
			}
		}

		if (i == sizeof(line)) {
			Com_Warn("Command exceeded %" PRIuPTR " chars, discarded\n", sizeof(line));
		} else {
			g_strlcpy(line, text, i + 1);
		}

		// delete the text from the command buffer and move remaining commands down
		// this is necessary because commands (exec, alias) can insert data at the
		// beginning of the text buffer

		if (i == cmd_state.buf.size) {
			cmd_state.buf.size = 0;
		} else {
			i++;

			cmd_state.buf.size = cmd_state.buf.size - i;
			memmove(text, text + i, cmd_state.buf.size);
		}

		// execute the command linequit

		Cmd_ExecuteString(line);

		// skip out while text still remains in buffer, leaving it for next frame
		if (cmd_state.wait) {
			cmd_state.wait = false;
			break;
		}
	}
}

/**
 * @return The command argument count.
 */
int32_t Cmd_Argc(void) {
	return cmd_state.args.argc;
}

/**
 * @return The command argument at the specified index.
 */
const char *Cmd_Argv(int32_t arg) {
	if (arg >= cmd_state.args.argc) {
		return "";
	}
	return cmd_state.args.argv[arg];
}

/**
 * @return A single string containing all command arguments.
 */
const char *Cmd_Args(void) {
	return cmd_state.args.args;
}

/**
 * @brief Parses the given string into command line tokens.
 */
void Cmd_TokenizeString(const char *text) {

	// clear the command state from the last string
	memset(&cmd_state.args, 0, sizeof(cmd_state.args));

	if (!text) {
		return;
	}

	// prevent overflows
	if (strlen(text) >= MAX_STRING_CHARS) {
		Com_Warn("MAX_STRING_CHARS exceeded\n");
		return;
	}

	parser_t parser;
	Parse_Init(&parser, text, PARSER_DEFAULT);

	while (true) {
		// stop after we've exhausted our token buffer
		if (cmd_state.args.argc == MAX_STRING_TOKENS) {
			Com_Warn("MAX_STRING_TOKENS exceeded\n");
			return;
		}

		// set cmd_state.args to everything after the command name
		if (cmd_state.args.argc == 1) {
			g_strlcpy(cmd_state.args.args, parser.position.ptr + 1, MAX_STRING_CHARS);

			// strip off any trailing whitespace
			size_t l = strlen(cmd_state.args.args);
			char *c = &cmd_state.args.args[l - 1];

			while (*c <= ' ') {
				*c-- = '\0';
			}
		}

		if (!Parse_Token(&parser, PARSE_NO_WRAP | PARSE_COPY_QUOTED_LITERALS, cmd_state.args.argv[cmd_state.args.argc], MAX_TOKEN_CHARS)) { // we're done
			return;
		}

		// expand console variables
		if (*cmd_state.args.argv[cmd_state.args.argc] == '$' && g_strcmp0(cmd_state.args.argv[0], "alias")) {
			const char *c = Cvar_GetString(cmd_state.args.argv[cmd_state.args.argc] + 1);
			g_strlcpy(cmd_state.args.argv[cmd_state.args.argc], c, MAX_TOKEN_CHARS);
		}

		cmd_state.args.argc++;
	}
}

/**
 * @return The variable by the specified name, or `NULL`.
 */
static cmd_t *Cmd_Get_(const char *name, const _Bool case_sensitive) {

	if (cmd_state.commands) {
		const GQueue *queue = (GQueue *) g_hash_table_lookup(cmd_state.commands, name);

		if (queue) {
			if (queue->length == 1) { // only 1 entry, return it
				cmd_t *cmd = (cmd_t *) queue->head->data;

				if (!case_sensitive || g_strcmp0(cmd->name, name) == 0) {
					return cmd;
				}
			} else {
				// only return the exact match
				for (const GList *list = queue->head; list; list = list->next) {
					cmd_t *cmd = (cmd_t *) list->data;

					if (!g_strcmp0(cmd->name, name)) {
						return cmd;
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
cmd_t *Cmd_Get(const char *name) {
	return Cmd_Get_(name, false);
}

/**
 * @brief GCompareFunc for Cmd_Enumerate.
 */
static gint Cmd_Enumerate_comparator(gconstpointer a, const gconstpointer b) {
	return g_ascii_strcasecmp(((const cmd_t *) a)->name, ((const cmd_t *) b)->name);
}

/**
 * @brief Enumerates all known commands with the given function.
 */
void Cmd_Enumerate(Cmd_Enumerator func, void *data) {
	GList *sorted = NULL;

	GHashTableIter iter;
	gpointer key, value;
	g_hash_table_iter_init(&iter, cmd_state.commands);

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		const GQueue *queue = (GQueue *) value;

		for (GList *list = queue->head; list; list = list->next) {
			sorted = g_list_concat(sorted, g_list_copy(list));
		}
	}

	sorted = g_list_sort(sorted, Cmd_Enumerate_comparator);

	g_list_foreach(sorted, (GFunc) func, data);

	g_list_free(sorted);
}

/**
 * @brief Adds the specified command, bound to the given function.
 */
cmd_t *Cmd_Add(const char *name, CmdExecuteFunc function, uint32_t flags,
               const char *description) {
	cmd_t *cmd;

	if (Cvar_Get(name)) {
		Com_Debug(DEBUG_CONSOLE, "%s already defined as a var\n", name);
		return NULL;
	}

	if ((cmd = Cmd_Get_(name, true))) {
		Com_Debug(DEBUG_CONSOLE, "%s already defined\n", name);
		return cmd;
	}

	cmd = Mem_TagMalloc(sizeof(*cmd), MEM_TAG_CMD);

	cmd->name = Mem_Link(Mem_TagCopyString(name, MEM_TAG_CMD), cmd);
	cmd->Execute = function;
	cmd->flags = flags;

	if (description) {
		cmd->description = Mem_Link(Mem_TagCopyString(description, MEM_TAG_CMD), cmd);
	}

	gpointer key = (gpointer) name;
	GQueue *queue = (GQueue *) g_hash_table_lookup(cmd_state.commands, key);

	if (!queue) {
		queue = g_queue_new();
		g_hash_table_insert(cmd_state.commands, key, queue);
	}

	g_queue_push_head(queue, cmd);
	return cmd;
}

/**
 * @brief Assign the specified autocomplete function to the given command.
 */
void Cmd_SetAutocomplete(cmd_t *cmd, AutocompleteFunc autocomplete) {
	cmd->Autocomplete = autocomplete;
}

/**
 * @brief Adds the specified alias command, bound to the given commands string.
 */
static cmd_t *Cmd_Alias(const char *name, const char *commands) {
	cmd_t *cmd;

	if (Cvar_Get(name)) {
		Com_Debug(DEBUG_CONSOLE, "%s already defined as a var\n", name);
		return NULL;
	}

	if ((cmd = Cmd_Get_(name, true))) {
		Com_Debug(DEBUG_CONSOLE, "%s already defined\n", name);
		return cmd;
	}

	cmd = Mem_TagMalloc(sizeof(*cmd), MEM_TAG_CMD);

	cmd->name = Mem_Link(Mem_TagCopyString(name, MEM_TAG_CMD), cmd);
	cmd->commands = Mem_Link(Mem_TagCopyString(commands, MEM_TAG_CMD), cmd);

	gpointer key = (gpointer) cmd->name;

	GQueue *queue = (GQueue *) g_hash_table_lookup(cmd_state.commands, key);

	if (!queue) {
		queue = g_queue_new();
		g_hash_table_insert(cmd_state.commands, key, queue);
	}

	g_queue_push_head(queue, cmd);
	return cmd;
}

/**
 * @brief Removes the specified command. Returns the backing queue.
 */
static GQueue *Cmd_Remove_(cmd_t *cmd) {
	GQueue *queue = (GQueue *) g_hash_table_lookup(cmd_state.commands, cmd->name);

	if (!g_queue_remove(queue, cmd)) {
		Com_Error(ERROR_FATAL, "Missing command: %s\n", cmd->name);
	}

	Mem_Free(cmd);
	return queue;
}

/**
 * @brief Removes the specified command.
 */
void Cmd_Remove(const char *name) {
	cmd_t *cmd = Cmd_Get_(name, true);

	if (cmd) {
		const GQueue *queue = Cmd_Remove_(cmd);

		if (!queue->length) {
			g_hash_table_remove(cmd_state.commands, name);
		}
	}
}

/**
 * @brief Removes all commands which match the specified flags mask.
 */
void Cmd_RemoveAll(uint32_t flags) {
	GHashTableIter iter;
	gpointer key, value;
	g_hash_table_iter_init(&iter, cmd_state.commands);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		const GQueue *queue = (GQueue *) value;
		
		for (const GList *list = queue->head; list; list = list->next) {
			cmd_t *cmd = (cmd_t *) list->data;

			if (cmd->flags & flags) {
				Cmd_Remove_(cmd);
				
				if (!queue->length) {
					g_hash_table_iter_remove(&iter);
					break;
				}
			}
		}
	}
}

/**
 * @brief Stringify a command. This memory is temporary.
 */
static const char *Cmd_Stringify(const cmd_t *cmd) {
	static char buffer[MAX_STRING_CHARS];
	buffer[0] = '\0';

	if (cmd->Execute) {
		g_strlcat(buffer, va("^1%s^7", cmd->name), sizeof(buffer));

		if (cmd->description) {
			g_strlcat(buffer, va("\n\t%s", cmd->description), sizeof(buffer));
		}
	} else if (cmd->commands) {
		g_strlcat(buffer, va("^3%s^7\n\t%s", cmd->name, cmd->commands), sizeof(buffer));
	} else {
		g_strlcat(buffer, va("^3%s^7", cmd->name), sizeof(buffer));
	}

	return buffer;
}

static char cmd_complete_pattern[MAX_STRING_CHARS];

/**
 * @brief Enumeration helper for Cmd_CompleteCommand.
 */
static void Cmd_CompleteCommand_enumerate(cmd_t *cmd, void *data) {
	GList **matches = (GList **) data;

	if (GlobMatch(cmd_complete_pattern, cmd->name, GLOB_CASE_INSENSITIVE)) {
		*matches = g_list_insert_sorted(*matches, Com_AllocMatch(cmd->name, Cmd_Stringify(cmd)), Com_MatchCompare);
	}
}

/**
 * @brief Console completion for commands and aliases.
 */
void Cmd_CompleteCommand(const char *pattern, GList **matches) {
	g_strlcpy(cmd_complete_pattern, pattern, sizeof(cmd_complete_pattern));
	Cmd_Enumerate(Cmd_CompleteCommand_enumerate, (void *) matches);
}

/**
 * @brief A complete command line has been parsed, so try to execute it
 */
void Cmd_ExecuteString(const char *text) {
	cmd_t *cmd;

	Cmd_TokenizeString(text);

	if (!Cmd_Argc()) {
		return;
	}

	// execute the command line
	if ((cmd = Cmd_Get(Cmd_Argv(0)))) {
		if (cmd->Execute) {
			cmd->Execute();
		} else if (cmd->commands) {
			if (++cmd_state.alias_loop_count == MAX_ALIAS_LOOP_COUNT) {
				Com_Warn("ALIAS_LOOP_COUNT\n");
			} else {
				Cbuf_AddText(cmd->commands);
			}
		} else if (!Cvar_GetValue("dedicated") && Cmd_ForwardToServer) {
			Cmd_ForwardToServer();
		}
		return;
	}

	// check cvars
	if (Cvar_Command()) {
		return;
	}

	// send it as a server command if we are connected
	if (!Cvar_GetValue("dedicated") && Cmd_ForwardToServer) {
		Cmd_ForwardToServer();
	}
}

/**
 * @brief Enumeration helper for Cmd_Alias_f.
 */
static void Cmd_Alias_f_enumerate(cmd_t *cmd, void *data) {

	if (cmd->commands) {
		Com_Print("%s: %s\n", cmd->name, cmd->commands);
	}
}

/**
 * @brief Creates a new command that executes a command string (possibly ; separated)
 */
static void Cmd_Alias_f(void) {
	char cmd[MAX_STRING_CHARS];

	if (Cmd_Argc() == 1) {
		Cmd_Enumerate(Cmd_Alias_f_enumerate, NULL);
		return;
	}

	if (Cmd_Argc() < 3) {
		Com_Print("Usage: %s <commands>", Cmd_Argv(0));
		return;
	}

	if (Cvar_Get(Cmd_Argv(1))) {
		Com_Print("%s is a variable\n", Cmd_Argv(1));
		return;
	}

	if (Cmd_Get_(Cmd_Argv(1), true)) {
		Com_Print("%s is a command\n", Cmd_Argv(1));
		return;
	}

	cmd[0] = '\0';
	for (int32_t i = 2; i < Cmd_Argc(); i++) {
		g_strlcat(cmd, Cmd_Argv(i), sizeof(cmd));
		if (i != (Cmd_Argc() - 1)) {
			g_strlcat(cmd, " ", sizeof(cmd));
		}
	}
	g_strlcat(cmd, "\n", sizeof(cmd));

	Cmd_Alias(Cmd_Argv(1), cmd);
}

/**
 * @brief Enumeration helper for Cmd_List_f.
 */
static void Cmd_List_f_enumerate(cmd_t *cmd, void *data) {
	GSList **list = (GSList **) data;
	gchar *str = g_strdup(Cmd_Stringify(cmd));

	*list = g_slist_insert_sorted(*list, (gpointer) str, (GCompareFunc) StrStripCmp);
}

/**
 * @brief Lists all known commands at the console.
 */
static void Cmd_List_f(void) {
	GSList *list = NULL;

	Cmd_Enumerate(Cmd_List_f_enumerate, &list);
	
	for (GSList *entry = list; entry; entry = entry->next) {
		Com_Print("%s\n", (const gchar *) entry->data);
	}

	g_slist_free_full(list, g_free);
}

/**
 * @brief Demo command autocompletion.
 */
static void Cmd_Exec_Autocomplete_f(const uint32_t argi, GList **matches) {
	const char *pattern = va("%s*.cfg", Cmd_Argv(argi));
	Fs_CompleteFile(pattern, matches);
}

/**
 * @brief Executes the specified script file (e.g autoexec.cfg).
 */
static void Cmd_Exec_f(void) {
	char path[MAX_QPATH];
	void *buffer;

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <filename> : execute a script file\n", Cmd_Argv(0));
		return;
	}

	g_strlcpy(path, Cmd_Argv(1), sizeof(path));
	if (!g_str_has_suffix(path, ".cfg")) {
		g_strlcat(path, ".cfg", sizeof(path));
	}

	if (Fs_Load(path, &buffer) == -1) {
		Com_Print("Couldn't exec %s\n", Cmd_Argv(1));
		return;
	}

	Cbuf_InsertText((const char *) buffer);
	Fs_Free(buffer);
}

/**
 * @brief Prints the remaining command arguments to the console.
 */
static void Cmd_Echo_f(void) {
	int32_t i;

	for (i = 1; i < Cmd_Argc(); i++) {
		Com_Print("%s ", Cmd_Argv(i));
	}

	Com_Print("\n");
}

/**
 * @brief Causes execution of the remainder of the command buffer to be delayed until
 * next frame. This allows commands like: bind g "+attack; wait; -attack;"
 */
static void Cmd_Wait_f(void) {
	cmd_state.wait = true;
}

/**
 * @brief
 */
static void Cmd_HashTable_Free(gpointer list) {

	g_queue_free_full((GQueue *) list, Mem_Free);
}

/**
 * @brief Initializes the command subsystem.
 */
void Cmd_Init(void) {

	memset(&cmd_state, 0, sizeof(cmd_state));

	cmd_state.commands = g_hash_table_new_full(g_stri_hash, g_stri_equal, NULL, Cmd_HashTable_Free);

	Mem_InitBuffer(&cmd_state.buf, (byte *) cmd_state.buffers[0], sizeof(cmd_state.buffers[0]));

	Cmd_Add("cmd_list", Cmd_List_f, 0, NULL);
	cmd_t *exec_cmd = Cmd_Add("exec", Cmd_Exec_f, CMD_SYSTEM, NULL);
	Cmd_SetAutocomplete(exec_cmd, Cmd_Exec_Autocomplete_f);
	Cmd_Add("echo", Cmd_Echo_f, 0, NULL);
	Cmd_Add("alias", Cmd_Alias_f, CMD_SYSTEM, NULL);
	Cmd_Add("wait", Cmd_Wait_f, 0, NULL);

	for (int32_t i = 1; i < Com_Argc(); i++) {
		const char *c = Com_Argv(i);

		// if we encounter a non-set command, consume until the next + or EOL
		if (*c == '+' && strncmp(c, "+set", 4)) {
			Cbuf_AddText(c + 1);
			i++;

			while (i < Com_Argc()) {
				c = Com_Argv(i);
				if (*c == '+') {
					Cbuf_AddText("\n");
					i--;
					break;
				}
				Cbuf_AddText(va(" %s", c));
				i++;
			}
		}
	}
	Cbuf_AddText("\n");
	Cbuf_CopyToDefer();

	// Com_Debug("Deferred buffer: %s", cmd_state.buffers[1]);
}

/**
 * @brief Shuts down the command subsystem.
 */
void Cmd_Shutdown(void) {

	g_hash_table_destroy(cmd_state.commands);
}

/*
 * An optional function pointer the client will implement; the server will not.
 */
void (*Cmd_ForwardToServer)(void) = NULL;
