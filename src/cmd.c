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

#include "cmd.h"

#define MAX_ALIAS_NAME 32

typedef struct cmd_alias_s {
	struct cmd_alias_s *next;
	char name[MAX_ALIAS_NAME];
	char *value;
} cmd_alias_t;

static cmd_alias_t *cmd_alias;

static bool cmd_wait;

#define ALIAS_LOOP_COUNT 16
static int32_t alias_count; // for detecting runaway loops


/*
 * @brief Causes execution of the remainder of the command buffer to be delayed until
 * next frame. This allows commands like:
 * bind g "+attack; wait; -attack;"
 */
static void Cmd_Wait_f(void) {
	cmd_wait = true;
}

/*
 *
 * COMMAND BUFFER
 *
 */

static size_buf_t cmd_text;
static byte cmd_text_buf[8192];

static char defer_text_buf[8192];

/*
 * @brief
 */
void Cbuf_Init(void) {
	Sb_Init(&cmd_text, cmd_text_buf, sizeof(cmd_text_buf));
}

/*
 * @brief Adds command text at the end of the buffer
 */
void Cbuf_AddText(const char *text) {
	int32_t l;

	l = strlen(text);

	if (cmd_text.size + l >= cmd_text.max_size) {
		Com_Print("Cbuf_AddText: overflow\n");
		return;
	}

	Sb_Write(&cmd_text, text, l);
}

/*
 * @brief Adds command text immediately after the current command
 * Adds a \n to the text.
 */
void Cbuf_InsertText(const char *text) {
	char *temp;
	int32_t temp_len;

	// copy off any commands still remaining in the exec buffer
	temp_len = cmd_text.size;
	if (temp_len) {
		temp = Z_Malloc(temp_len);
		memcpy(temp, cmd_text.data, temp_len);
		Sb_Clear(&cmd_text);
	} else
		temp = NULL; // shut up compiler

	// add the entire text of the file
	Cbuf_AddText(text);

	// add the copied off data
	if (temp_len) {
		Sb_Write(&cmd_text, temp, temp_len);
		Z_Free(temp);
	}
}

/*
 * @brief
 */
void Cbuf_CopyToDefer(void) {
	memcpy(defer_text_buf, cmd_text_buf, cmd_text.size);
	defer_text_buf[cmd_text.size] = 0;
	cmd_text.size = 0;
}

/*
 * @brief
 */
void Cbuf_InsertFromDefer(void) {
	Cbuf_InsertText(defer_text_buf);
	defer_text_buf[0] = 0;
}

/*
 * @brief
 */
void Cbuf_Execute(void) {
	uint32_t i;
	char *text;
	char line[MAX_STRING_CHARS];
	int32_t quotes;

	alias_count = 0; // don't allow infinite alias loops

	while (cmd_text.size) {
		// find a \n or; line break
		text = (char *) cmd_text.data;

		quotes = 0;
		for (i = 0; i < cmd_text.size; i++) {
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
		// this is necessary because commands(exec, alias) can insert data at the
		// beginning of the text buffer

		if (i == cmd_text.size)
			cmd_text.size = 0;
		else {
			i++;
			cmd_text.size -= i;
			memmove(text, text + i, cmd_text.size);
		}

		// execute the command line
		Cmd_ExecuteString(line);

		if (cmd_wait) {
			// skip out while text still remains in buffer, leaving it
			// for next frame
			cmd_wait = false;
			break;
		}
	}
}

/*
 * @brief Adds command line parameters as script statements
 * Commands lead with a +, and continue until another +.
 *
 * Set commands are added early, so they are guaranteed to be set before
 * the client and server initialize for the first time.
 *
 * Other commands are added late, after all initialization is complete.
 */
void Cbuf_AddEarlyCommands(bool clear) {
	int32_t i;
	char *s;

	for (i = 0; i < Com_Argc(); i++) {
		s = Com_Argv(i);
		if (strcmp(s, "+set"))
			continue;
		Cbuf_AddText(va("set %s %s\n", Com_Argv(i + 1), Com_Argv(i + 2)));
		if (clear) {
			Com_ClearArgv(i);
			Com_ClearArgv(i + 1);
			Com_ClearArgv(i + 2);
		}
		i += 2;
	}
	Cbuf_Execute();
}

/*
 * @brief Adds remaining command line parameters as script statements
 * Commands lead with a + and continue until another +.
 */
void Cbuf_AddLateCommands(void) {
	uint32_t i, j, k;
	char *c, text[MAX_STRING_CHARS];

	j = 0;
	memset(text, 0, sizeof(text));

	for (i = 1; i < (uint32_t) Com_Argc(); i++) {

		c = Com_Argv(i);
		k = strlen(c);

		if (j + k > sizeof(text) - 1)
			break;

		strcat(text, c);
		strcat(text, " ");
		j += k + 1;
	}

	for (i = 0; i < j; i++) {
		if (text[i] == '+')
			text[i] = '\n';
	}
	strcat(text, "\n");

	Cbuf_AddText(text);
	Cbuf_Execute();
}

/*
 *
 * 						SCRIPT COMMANDS
 *
 */

/*
 * @brief
 */
static void Cmd_Exec_f(void) {
	void *buf;
	int32_t len;
	char cfg[MAX_QPATH];

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <file_name> : execute a script file\n", Cmd_Argv(0));
		return;
	}

	strcpy(cfg, Cmd_Argv(1));
	if (strstr(cfg, ".cfg") == NULL)
		strcat(cfg, ".cfg");

	if ((len = Fs_LoadFile(cfg, &buf)) == -1) {
		Com_Print("couldn't exec %s\n", cfg);
		return;
	}

	Cbuf_InsertText((const char *) buf);
	Fs_FreeFile(buf);
}

/*
 * @brief Just prints the rest of the line to the console
 */
static void Cmd_Echo_f(void) {
	int32_t i;

	for (i = 1; i < Cmd_Argc(); i++)
		Com_Print("%s ", Cmd_Argv(i));
	Com_Print("\n");
}

/*
 * @brief Creates a new command that executes a command string (possibly ; seperated)
 */
static void Cmd_Alias_f(void) {
	cmd_alias_t *a;
	char cmd[MAX_STRING_CHARS];
	int32_t i, c;

	if (Cmd_Argc() == 1) {
		Com_Print("Current alias commands:\n");
		for (a = cmd_alias; a; a = a->next)
			Com_Print("%s : %s\n", a->name, a->value);
		return;
	}

	const char *s = Cmd_Argv(1);
	if (strlen(s) >= MAX_ALIAS_NAME) {
		Com_Print("Alias name is too long\n");
		return;
	}

	// if the alias already exists, reuse it
	for (a = cmd_alias; a; a = a->next) {
		if (!strcmp(s, a->name)) {
			Z_Free(a->value);
			break;
		}
	}

	if (!a) {
		a = Z_Malloc(sizeof(*a));
		a->next = cmd_alias;
		cmd_alias = a;
	}
	strcpy(a->name, s);

	// copy the rest of the command line
	cmd[0] = 0; // start out with a null string
	c = Cmd_Argc();
	for (i = 2; i < c; i++) {
		strcat(cmd, Cmd_Argv(i));
		if (i != (c - 1))
			strcat(cmd, " ");
	}
	strcat(cmd, "\n");

	a->value = Z_CopyString(cmd);
}

/*
 *
 * COMMAND EXECUTION
 *
 */

typedef struct cmd_state_s {
	int32_t argc;
	char argv[MAX_STRING_TOKENS][MAX_TOKEN_CHARS];
	char args[MAX_STRING_CHARS];
} cmd_state_t;

static cmd_state_t cmd_state;
static cmd_t *cmd_commands; // possible commands to execute

static GHashTable * cmd_hash; // hashed for fast lookups

/*
 * @brief
 */
int32_t Cmd_Argc(void) {
	return cmd_state.argc;
}

/*
 * @brief
 */
const char *Cmd_Argv(int32_t arg) {
	if (arg >= cmd_state.argc)
		return "";
	return cmd_state.argv[arg];
}

/*
 * @brief Returns a single string containing all command arguments.
 */
const char *Cmd_Args(void) {
	return cmd_state.args;
}

/*
 * @brief Parses the given string into command line tokens.
 */
void Cmd_TokenizeString(const char *text) {
	char *c;

	// clear the command state from the last string
	memset(&cmd_state, 0, sizeof(cmd_state));

	if (!text)
		return;

	// prevent overflows
	if (strlen(text) >= MAX_STRING_CHARS) {
		Com_Warn("Cmd_TokenizeString: MAX_STRING_CHARS exceeded\n");
		return;
	}

	while (true) {
		// stop after we've exhausted our token buffer
		if (cmd_state.argc == MAX_STRING_TOKENS) {
			Com_Warn("Cmd_TokenizeString: MAX_STRING_TOKENS exceeded\n");
			return;
		}

		// skip whitespace up to a \n
		while (*text <= ' ') {
			if (!*text || *text == '\n')
				return;
			text++;
		}

		// set cmd_state.args to everything after the command name
		if (cmd_state.argc == 1) {
			g_strlcpy(cmd_state.args, text, MAX_STRING_CHARS);

			// strip off any trailing whitespace
			size_t l = strlen(cmd_state.args);
			c = &cmd_state.args[l - 1];

			while (*c <= ' ') {
				*c-- = '\0';
			}
		}

		c = ParseToken(&text);

		if (*c == '\0') { // we're done
			return;
		}

		// expand console variables
		if (*c == '$' && strcmp(cmd_state.argv[0], "alias")) {
			c = Cvar_GetString(c + 1);
		}

		g_strlcpy(cmd_state.argv[cmd_state.argc], c, MAX_TOKEN_CHARS);
		cmd_state.argc++;
	}
}

/*
 * @brief
 */
cmd_t *Cmd_Get(const char *name) {
	cmd_t *cmd;

	for (cmd = cmd_commands; cmd; cmd = cmd->next) {
		if (!strcmp(name, cmd->name)) {
			return cmd;
		}
	}
	return NULL;
}

/*
 * @brief
 */
void Cmd_AddCommand(const char *name, cmd_function_t function, uint32_t flags,
		const char *description) {
	cmd_t *c, *cmd;

	// fail if the command is a variable name
	if (Cvar_GetString(name)[0]) {
		Com_Debug("Cmd_AddCommand: %s already defined as a var\n", name);
		return;
	}

	if (Cmd_Get(name)) {
		Com_Debug("Cmd_AddCommand: %s already defined\n", name);
		return;
	}

	cmd = Z_Malloc(sizeof(*cmd));
	cmd->name = name;
	cmd->function = function;
	cmd->flags = flags;
	cmd->description = description;

	// hash the command
	g_hash_table_insert(cmd_hash, (gpointer)name, cmd);

	// and add it to the chain
	if (!cmd_commands) {
		cmd_commands = cmd;
		return;
	}

	c = cmd_commands;
	while (c->next) {
		if (strcmp(cmd->name, c->next->name) < 0) { // insert it
			cmd->next = c->next;
			c->next = cmd;
			return;
		}
		c = c->next;
	}

	c->next = cmd;
}

/*
 * @brief
 */
void Cmd_RemoveCommand(const char *name) {
	cmd_t *cmd, **back;

	g_hash_table_remove(cmd_hash, name);

	back = &cmd_commands;
	while (true) {
		cmd = *back;
		if (!cmd) {
			Com_Debug("Cmd_RemoveCommand: %s not added\n", name);
			return;
		}
		if (!strcmp(name, cmd->name)) {
			*back = cmd->next;
			Z_Free(cmd);
			return;
		}
		back = &cmd->next;
	}
}

/*
 * @brief
 */
int32_t Cmd_CompleteCommand(const char *partial, const char *matches[]) {
	cmd_t *cmd;
	cmd_alias_t *a;
	int32_t len;
	int32_t m;

	len = strlen(partial);
	m = 0;

	// check for partial matches in commands
	for (cmd = cmd_commands; cmd; cmd = cmd->next) {
		if (!strncmp(partial, cmd->name, len)) {
			Com_Print("^1%s^7\n", cmd->name);
			if (cmd->description)
				Com_Print("\t%s\n", cmd->description);
			matches[m] = cmd->name;
			m++;
		}
	}

	// and then aliases
	for (a = cmd_alias; a; a = a->next) {
		if (!strncmp(partial, a->name, len)) {
			Com_Print("^3%s^7\n", a->name);
			matches[m] = a->name;
			m++;
		}
	}

	return m;
}

/*
 * @brief A complete command line has been parsed, so try to execute it
 */
void Cmd_ExecuteString(const char *text) {
	cmd_t *cmd;
	cmd_alias_t *a;

	Cmd_TokenizeString(text);

	// execute the command line
	if (!Cmd_Argc())
		return; // no tokens

	if ((cmd = g_hash_table_lookup(cmd_hash, cmd_state.argv[0]))) {
		if (cmd->function) {
			cmd->function();
		} else if (!Cvar_GetValue("dedicated") && Cmd_ForwardToServer)
			Cmd_ForwardToServer();
		return;
	}

	// check alias
	for (a = cmd_alias; a; a = a->next) {
		if (!strcasecmp(cmd_state.argv[0], a->name)) {
			if (++alias_count == ALIAS_LOOP_COUNT) {
				Com_Warn("ALIAS_LOOP_COUNT reached.\n");
				return;
			}
			Cbuf_InsertText(a->value);
			return;
		}
	}

	// check cvars
	if (Cvar_Command())
		return;

	// send it as a server command if we are connected
	if (!Cvar_GetValue("dedicated") && Cmd_ForwardToServer)
		Cmd_ForwardToServer();
}

/*
 * @brief
 */
static void Cmd_List_f(void) {
	cmd_t *cmd;
	int32_t i;

	i = 0;
	for (cmd = cmd_commands; cmd; cmd = cmd->next, i++) {
		Com_Print("%s\n", cmd->name);
		if (cmd->description)
			Com_Print("   ^2%s\n", cmd->description);
	}
	Com_Print("%i commands\n", i);
}

/*
 * @brief
 */
void Cmd_Init(void) {

	cmd_hash = g_hash_table_new(g_str_hash, g_str_equal);

	Cmd_AddCommand("cmd_list", Cmd_List_f, 0, NULL);
	Cmd_AddCommand("exec", Cmd_Exec_f, 0, NULL);
	Cmd_AddCommand("echo", Cmd_Echo_f, 0, NULL);
	Cmd_AddCommand("alias", Cmd_Alias_f, 0, NULL);
	Cmd_AddCommand("wait", Cmd_Wait_f, 0, NULL);

	Cmd_AddCommand("z_size", Z_Size_f, 0, "Prints current size (in MB) of the zone allocation pool.");
}

/*
 * An optional function pointer the client will implement; the server will not.
 */
void (*Cmd_ForwardToServer)(void) = NULL;
