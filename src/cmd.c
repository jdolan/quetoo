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
	GList *keys;

	mem_buf_t buf;
	char buffers[2][CBUF_CHARS];

	cmd_args_t args;

	_Bool wait; // commands may be deferred one frame

	uint16_t alias_loop_count;
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
	void *temp;

	// copy off any commands still remaining in the exec buffer
	const size_t size = cmd_state.buf.size;
	if (size) {
		temp = Mem_Malloc(size);
		memcpy(temp, cmd_state.buf.data, size);
		Mem_ClearBuffer(&cmd_state.buf);
	} else
		temp = NULL; // shut up compiler

	// add the entire text of the file
	Cbuf_AddText(text);

	// add the copied off data
	if (size) {
		Mem_WriteBuffer(&cmd_state.buf, temp, size);
		Mem_Free(temp);
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
	uint32_t i;
	char *text;
	char line[MAX_STRING_CHARS];
	int32_t quotes;

	cmd_state.alias_loop_count = 0; // don't allow infinite alias loops

	while (cmd_state.buf.size) {
		// find a \n or; line break
		text = (char *) cmd_state.buf.data;

		quotes = 0;
		for (i = 0; i < cmd_state.buf.size; i++) {
			if (text[i] == '"')
				quotes++;
			if (!(quotes & 1) && text[i] == ';')
				break; // don't break if inside a quoted string
			if (text[i] == '\n')
				break;
		}

		if (i > MAX_STRING_CHARS) { // length check each command
			Com_Warn("Command exceeded %i chars, discarded\n", MAX_STRING_CHARS);
			return;
		}

		memcpy(line, text, i);
		line[i] = 0;

		// delete the text from the command buffer and move remaining commands down
		// this is necessary because commands (exec, alias) can insert data at the
		// beginning of the text buffer

		if (i == cmd_state.buf.size)
			cmd_state.buf.size = 0;
		else {
			i++;
			cmd_state.buf.size -= i;
			memmove(text, text + i, cmd_state.buf.size);
		}

		// execute the command line
		Cmd_ExecuteString(line);

		if (cmd_state.wait) {
			// skip out while text still remains in buffer, leaving it for next frame
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
	if (arg >= cmd_state.args.argc)
		return "";
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
	char *c;

	// clear the command state from the last string
	memset(&cmd_state.args, 0, sizeof(cmd_state.args));

	if (!text)
		return;

	// prevent overflows
	if (strlen(text) >= MAX_STRING_CHARS) {
		Com_Warn("MAX_STRING_CHARS exceeded\n");
		return;
	}

	while (true) {
		// stop after we've exhausted our token buffer
		if (cmd_state.args.argc == MAX_STRING_TOKENS) {
			Com_Warn("MAX_STRING_TOKENS exceeded\n");
			return;
		}

		// skip whitespace up to a \n
		while (*text <= ' ') {
			if (!*text || *text == '\n')
				return;
			text++;
		}

		// set cmd_state.args to everything after the command name
		if (cmd_state.args.argc == 1) {
			g_strlcpy(cmd_state.args.args, text, MAX_STRING_CHARS);

			// strip off any trailing whitespace
			size_t l = strlen(cmd_state.args.args);
			c = &cmd_state.args.args[l - 1];

			while (*c <= ' ') {
				*c-- = '\0';
			}
		}

		c = ParseToken(&text);

		if (*c == '\0' && !text) { // we're done
			return;
		}

		// expand console variables
		if (*c == '$' && g_strcmp0(cmd_state.args.argv[0], "alias")) {
			c = (char *) Cvar_GetString(c + 1);
		}

		g_strlcpy(cmd_state.args.argv[cmd_state.args.argc], c, MAX_TOKEN_CHARS);
		cmd_state.args.argc++;
	}
}

/**
 * @return The command by the specified name, or `NULL`.
 */
cmd_t *Cmd_Get(const char *name) {

	if (cmd_state.commands) {
		return (cmd_t *) g_hash_table_lookup(cmd_state.commands, name);
	}

	return NULL;
}

/**
 * @brief Enumerates all known commands with the given function.
 */
void Cmd_Enumerate(CmdEnumerateFunc func, void *data) {

	GList *key = cmd_state.keys;
	while (key) {
		cmd_t *cmd = g_hash_table_lookup(cmd_state.commands, key->data);
		if (cmd) {
			func(cmd, data);
		} else {
			Com_Error(ERR_FATAL, "Missing command: %s\n", (char *) key->data);
		}
		key = key->next;
	}
}

/**
 * @brief Adds the specified command, bound to the given function.
 */
cmd_t *Cmd_Add(const char *name, CmdExecuteFunc function, uint32_t flags,
		const char *description) {
	cmd_t *cmd;

	if (Cvar_Add(name, NULL, 0, NULL)) {
		Com_Debug("%s already defined as a var\n", name);
		return NULL;
	}

	if ((cmd = Cmd_Get(name))) {
		Com_Debug("%s already defined\n", name);
		return cmd;
	}

	cmd = Mem_Malloc(sizeof(*cmd));

	cmd->name = Mem_Link(Mem_CopyString(name), cmd);
	cmd->Execute = function;
	cmd->flags = flags;

	if (description) {
		cmd->description = Mem_Link(Mem_CopyString(description), cmd);
	}

	gpointer key = (gpointer) name;

	g_hash_table_insert(cmd_state.commands, key, cmd);
	cmd_state.keys = g_list_insert_sorted(cmd_state.keys, key, (GCompareFunc) strcmp);

	return cmd;
}

/**
 * @brief Adds the specified alias command, bound to the given commands string.
 */
static cmd_t *Cmd_Alias(const char *name, const char *commands) {
	cmd_t *cmd;

	if (Cvar_Add(name, NULL, 0, NULL)) {
		Com_Debug("%s already defined as a var\n", name);
		return NULL;
	}

	if ((cmd = Cmd_Get(name))) {
		Com_Debug("%s already defined\n", name);
		return cmd;
	}

	cmd = Mem_Malloc(sizeof(*cmd));

	cmd->name = Mem_Link(Mem_CopyString(name), cmd);
	cmd->commands = Mem_Link(Mem_CopyString(commands), cmd);

	gpointer *key = (gpointer *) cmd->name;

	g_hash_table_insert(cmd_state.commands, key, cmd);
	cmd_state.keys = g_list_insert_sorted(cmd_state.keys, key, (GCompareFunc) strcmp);

	return cmd;
}

/**
 * @brief Removes the specified command.
 */
void Cmd_Remove(const char *name) {
	g_hash_table_remove(cmd_state.commands, name);
}

/**
 * @brief GHRFunc for Cmd_RemoveAll.
 */
static gboolean Cmd_RemoveAll_(gpointer key, gpointer value, gpointer data) {
	cmd_t *cmd = (cmd_t *) value;
	uint32_t flags = *((uint32_t *) data);

	if (cmd->flags & flags) {
		cmd_state.keys = g_list_remove(cmd_state.keys, key);
		return true;
	}

	return false;
}

/**
 * @brief Removes all commands which match the specified flags mask.
 */
void Cmd_RemoveAll(uint32_t flags) {
	g_hash_table_foreach_remove(cmd_state.commands, Cmd_RemoveAll_, &flags);
}

static const char *cmd_complete_pattern;

/**
 * @brief Enumeration helper for Cmd_CompleteCommand.
 */
static void Cmd_CompleteCommand_enumerate(cmd_t *cmd, void *data) {
	GList **matches = (GList **) data;

	if (GlobMatch(cmd_complete_pattern, cmd->name)) {

		if (cmd->Execute) {
			Com_Print("^1%s^7\n", cmd->name);

			if (cmd->description)
				Com_Print("\t%s\n", cmd->description);
		} else if (cmd->commands) {
			Com_Print("^3%s^7\n", cmd->name);
			Com_Print("\t%s\n", cmd->commands);
		}

		*matches = g_list_prepend(*matches, Mem_CopyString(cmd->name));
	}
}

/**
 * @brief Console completion for commands and aliases.
 */
void Cmd_CompleteCommand(const char *pattern, GList **matches) {
	cmd_complete_pattern = pattern;
	Cmd_Enumerate(Cmd_CompleteCommand_enumerate, (void *) matches);
}

/**
 * @brief A complete command line has been parsed, so try to execute it
 */
void Cmd_ExecuteString(const char *text) {
	cmd_t *cmd;

	Cmd_TokenizeString(text);

	if (!Cmd_Argc())
		return;

	// execute the command line
	if ((cmd = Cmd_Get(Cmd_Argv(0)))) {
		if (cmd->Execute) {
			cmd->Execute();
		} else if (cmd->commands) {
			if (++cmd_state.alias_loop_count == MAX_ALIAS_LOOP_COUNT) {
				Com_Warn("ALIAS_LOOP_COUNT reached\n");
			} else {
				Cbuf_AddText(cmd->commands);
			}
		} else if (!Cvar_GetValue("dedicated") && Cmd_ForwardToServer) {
			Cmd_ForwardToServer();
		}
		return;
	}

	// check cvars
	if (Cvar_Command())
		return;

	// send it as a server command if we are connected
	if (!Cvar_GetValue("dedicated") && Cmd_ForwardToServer)
		Cmd_ForwardToServer();
}

/**
 * @brief Enumeration helper for Cmd_Alias_f.
 */
static void Cmd_Alias_f_enumerate(cmd_t *cmd, void *data __attribute__((unused))) {

	if (cmd->commands) {
		Com_Print("%s: %s\n", cmd->name, cmd->commands);
	}
}

/**
 * @brief Creates a new command that executes a command string (possibly ; seperated)
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

	if (Cvar_Add(Cmd_Argv(1), NULL, 0, NULL)) {
		Com_Print("%s is a variable\n", Cmd_Argv(1));
		return;
	}

	if (Cmd_Get(Cmd_Argv(1))) {
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
static void Cmd_List_f_enumerate(cmd_t *cmd, void *data __attribute__((unused))) {

	if (cmd->Execute) {
		Com_Print("%s\n", cmd->name);

		if (cmd->description) {
			Com_Print("   ^2%s\n", cmd->description);
		}
	}
}

/**
 * @brief Lists all known commands at the console.
 */
static void Cmd_List_f(void) {
	Cmd_Enumerate(Cmd_List_f_enumerate, NULL);
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

	for (i = 1; i < Cmd_Argc(); i++)
		Com_Print("%s ", Cmd_Argv(i));

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
 * @brief Initializes the command subsystem.
 */
void Cmd_Init(void) {

	memset(&cmd_state, 0, sizeof(cmd_state));

	cmd_state.commands = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, Mem_Free);

	Mem_InitBuffer(&cmd_state.buf, (byte *) cmd_state.buffers[0], sizeof(cmd_state.buffers[0]));

	Cmd_Add("cmd_list", Cmd_List_f, 0, NULL);
	Cmd_Add("exec", Cmd_Exec_f, CMD_SYSTEM, NULL);
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
	g_list_free(cmd_state.keys);
}

/*
 * An optional function pointer the client will implement; the server will not.
 */
void (*Cmd_ForwardToServer)(void) = NULL;
