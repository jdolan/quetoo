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

#include <ctype.h>

#include "cl_local.h"

static cl_key_state_t *ks = &cls.key_state;

static char **cl_key_names;

/*
 * @brief Execute any system-level binds, regardless of key state. This enables e.g.
 * toggling of the console, toggling fullscreen, etc.
 */
static bool Cl_KeySystem(SDLKey key, uint16_t unicode __attribute__((unused)), bool down, uint32_t time __attribute__((unused))) {

	if (!down) { // don't care
		return false;
	}

	if (key == SDLK_ESCAPE) { // escape can cancel a few things

		// connecting to a server
		if (cls.state == CL_CONNECTING || cls.state == CL_CONNECTED) {
			Com_Error(ERR_NONE, "Connection aborted by user\n");
		}

		// message mode
		if (ks->dest == KEY_CHAT) {
			ks->dest = KEY_GAME;
			return true;
		}

		// console
		if (ks->dest == KEY_CONSOLE) {
			Cl_ToggleConsole_f();
			return true;
		}

		// and menus
		if (ks->dest == KEY_UI) {

			// if we're in the game, just hide the menus
			if (cls.state == CL_ACTIVE) {
				ks->dest = KEY_GAME;
			}

			return true;
		}

		ks->dest = KEY_UI;
		return true;
	}

	// for everything other than ESC, check for system-level command binds

	if (ks->binds[key]) {
		cmd_t *cmd;

		if ((cmd = Cmd_Get(ks->binds[key]))) {
			if (cmd->flags & CMD_SYSTEM) {
				Cbuf_AddText(ks->binds[key]);
				return true;
			}
		}
	}

	return false;
}

/*
 * @brief Interactive line editing and console scrollback.
 */
static void Cl_KeyConsole(SDLKey key, uint16_t unicode, bool down, uint32_t time __attribute__((unused))) {
	size_t i;

	if (!down) // don't care
		return;

	// submit buffer on enter
	if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {

		if (ks->lines[ks->edit_line][1] == '\\' || ks->lines[ks->edit_line][1] == '/')
			Cbuf_AddText(ks->lines[ks->edit_line] + 2); // skip the /
		else
			Cbuf_AddText(ks->lines[ks->edit_line] + 1); // valid command

		Cbuf_AddText("\n");
		Com_Print("%s\n", ks->lines[ks->edit_line]);

		ks->edit_line = (ks->edit_line + 1) % KEY_HISTORY_SIZE;
		ks->history_line = ks->edit_line;

		memset(ks->lines[ks->edit_line], 0, sizeof(ks->lines[0]));

		ks->lines[ks->edit_line][0] = ']';
		ks->pos = 1;

		if (cls.state == CL_DISCONNECTED) // force an update, because the command
			Cl_UpdateScreen(); // may take some time

		return;
	}

	if (key == SDLK_TAB) { // command completion
		// ignore the leading bracket in the input string
		ks->pos--;
		Con_CompleteCommand(ks->lines[ks->edit_line] + 1, &ks->pos, KEY_LINE_SIZE - 1);
		ks->pos++;
		return;
	}

	if (key == SDLK_BACKSPACE) { // delete char before cursor
		if (ks->pos > 1) {
			strcpy(ks->lines[ks->edit_line] + ks->pos - 1, ks->lines[ks->edit_line] + ks->pos);
			ks->pos--;
		}
		return;
	}

	if (key == SDLK_DELETE) { // delete char on cursor
		if (ks->pos < strlen(ks->lines[ks->edit_line]))
			strcpy(ks->lines[ks->edit_line] + ks->pos, ks->lines[ks->edit_line] + ks->pos + 1);
		return;
	}

	if (key == SDLK_INSERT) { // toggle insert mode
		ks->insert ^= 1;
		return;
	}

	if (key == SDLK_LEFT) { // move cursor left
		if (ks->down[SDLK_LCTRL] || ks->down[SDLK_RCTRL]) { // by a whole word
			while (ks->pos > 1 && ks->lines[ks->edit_line][ks->pos - 1] == ' ')
				ks->pos--; // get off current word
			while (ks->pos > 1 && ks->lines[ks->edit_line][ks->pos - 1] != ' ')
				ks->pos--; // and behind previous word
			return;
		}

		if (ks->pos > 1) //or just a char
			ks->pos--;
		return;
	}

	if (key == SDLK_RIGHT) { // move cursor right
		i = strlen(ks->lines[ks->edit_line]);

		if (i == ks->pos)
			return; // no character to get

		if (ks->down[SDLK_LCTRL] || ks->down[SDLK_RCTRL]) { // by a whole word
			while (ks->pos < i && ks->lines[ks->edit_line][ks->pos + 1] == ' ')
				ks->pos++; // get off current word
			while (ks->pos < i && ks->lines[ks->edit_line][ks->pos + 1] != ' ')
				ks->pos++; // and in front of next word
			if (ks->pos < i) // all the way in front
				ks->pos++;
			return;
		}

		ks->pos++; // or just a char
		return;
	}

	if (key == SDLK_UP) { // iterate history back
		do {
			ks->history_line = (ks->history_line + KEY_HISTORY_SIZE - 1) % KEY_HISTORY_SIZE;
		} while (ks->history_line != ks->edit_line && !ks->lines[ks->history_line][1]);

		if (ks->history_line == ks->edit_line)
			ks->history_line = (ks->edit_line + 1) % KEY_HISTORY_SIZE;

		strcpy(ks->lines[ks->edit_line], ks->lines[ks->history_line]);
		ks->pos = strlen(ks->lines[ks->edit_line]);
		return;
	}

	if (key == SDLK_DOWN) { // iterate history forward
		if (ks->history_line == ks->edit_line)
			return;
		do {
			ks->history_line = (ks->history_line + 1) % KEY_HISTORY_SIZE;
		} while (ks->history_line != ks->edit_line && !ks->lines[ks->history_line][1]);

		if (ks->history_line == ks->edit_line) {
			strcpy(ks->lines[ks->edit_line], "]");
			ks->pos = 1;
		} else {
			strcpy(ks->lines[ks->edit_line], ks->lines[ks->history_line]);
			ks->pos = strlen(ks->lines[ks->edit_line]);
		}
		return;
	}

	if (key == SDLK_PAGEUP || key == SDLK_MOUSE4) {
		cl_console.scroll += CON_SCROLL;
		if (cl_console.scroll > cl_console.last_line)
			cl_console.scroll = cl_console.last_line;
		return;
	}

	if (key == SDLK_PAGEDOWN || key == SDLK_MOUSE5) {
		cl_console.scroll -= CON_SCROLL;
		if (cl_console.scroll < 0)
			cl_console.scroll = 0;
		return;
	}

	// Ctrl-A jumps to the beginning of the line
	if (key == SDLK_a && (ks->down[SDLK_LCTRL] || ks->down[SDLK_RCTRL])) {
		ks->pos = 1;
		return;
	}

	if (key == SDLK_HOME) { // go to the start of line
		if (ks->down[SDLK_LCTRL] || ks->down[SDLK_RCTRL]) // go to the start of the console
			cl_console.scroll = cl_console.last_line;
		else
			ks->pos = 1;
		return;
	}

	// Ctrl-E jumps to the end of the line
	if (key == SDLK_e && (ks->down[SDLK_LCTRL] || ks->down[SDLK_RCTRL])) {
		ks->pos = strlen(ks->lines[ks->edit_line]);
		return;
	}

	if (key == SDLK_END) { // go to the end of line
		if (ks->down[SDLK_LCTRL] || ks->down[SDLK_RCTRL]) // go to the end of the console
			cl_console.scroll = 0;
		else
			ks->pos = strlen(ks->lines[ks->edit_line]);
		return;
	}

	if (unicode < 32 || unicode > 127)
		return; // non printable

	if (ks->pos < KEY_LINE_SIZE - 1) {

		if (ks->insert) { // can't do strcpy to move string to right
			i = strlen(ks->lines[ks->edit_line]) - 1;

			if (i == 254)
				i--;

			for (; i >= ks->pos; i--)
				ks->lines[ks->edit_line][i + 1] = ks->lines[ks->edit_line][i];
		}

		i = ks->lines[ks->edit_line][ks->pos];
		ks->lines[ks->edit_line][ks->pos] = unicode;
		ks->pos++;

		if (!i) // only null terminate if at the end
			ks->lines[ks->edit_line][ks->pos] = 0;
	}
}

/*
 * @brief
 */
static void Cl_KeyGame(SDLKey key, uint16_t unicode __attribute__((unused)), bool down, uint32_t time) {
	char cmd[MAX_STRING_CHARS];

	const char *kb = ks->binds[key];
	if (!kb)
		return;

	cmd[0] = '\0';

	if (kb[0] == '+') { // button commands add key and time as a param
		if (down)
			g_snprintf(cmd, sizeof(cmd), "%s %i %i\n", kb, key, time);
		else
			g_snprintf(cmd, sizeof(cmd), "-%s %i %i\n", kb + 1, key, time);
	} else {
		if (down) {
			g_snprintf(cmd, sizeof(cmd), "%s\n", kb);
		}
	}

	if (cmd[0]) { // send the command
		Cbuf_AddText(cmd);
	}
}

/*
 * @brief
 */
static void Cl_KeyMessage(SDLKey key, uint16_t unicode, bool down, uint32_t time __attribute__((unused))) {

	if (!down) // don't care
		return;

	if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
		if (*cls.chat_state.buffer) {
			if (cls.chat_state.team)
				Cbuf_AddText("say_team \"");
			else
				Cbuf_AddText("say \"");
			Cbuf_AddText(cls.chat_state.buffer);
			Cbuf_AddText("\"");
		}

		ks->dest = KEY_GAME;
		cls.chat_state.buffer[0] = 0;
		cls.chat_state.len = 0;
		return;
	}

	if (key == SDLK_BACKSPACE) {
		if (cls.chat_state.len) {
			cls.chat_state.len--;
			cls.chat_state.buffer[cls.chat_state.len] = 0;
		}
		return;
	}

	if (unicode < 32 || unicode > 127)
		return; // non printable

	if (cls.chat_state.len == sizeof(cls.chat_state.buffer) - 1)
		return; // full

	cls.chat_state.buffer[cls.chat_state.len++] = unicode;
	cls.chat_state.buffer[cls.chat_state.len] = 0;
}

/*
 * @brief Returns the name of the specified key.
 */
const char *Cl_KeyName(SDLKey key) {

	if (key == SDLK_UNKNOWN || key >= SDLK_MLAST) {
		return va("<unknown %d>", key);
	}

	return cl_key_names[key];
}

/*
 * @brief Returns the number for the specified key name.
 */
SDLKey Cl_Key(const char *name) {
	SDLKey i;

	if (!name || !name[0])
		return SDLK_MLAST;

	for (i = SDLK_FIRST; i < SDLK_MLAST; i++) {
		if (!strcasecmp(name, cl_key_names[i]))
			return i;
	}

	return SDLK_MLAST;
}

/*
 * @brief Binds the specified key to the given command.
 */
void Cl_Bind(SDLKey key, const char *binding) {

	if (key == SDLK_UNKNOWN || key >= SDLK_MLAST)
		return;

	// free the old binding
	if (ks->binds[key]) {
		Z_Free(ks->binds[key]);
		ks->binds[key] = NULL;
	}

	// allocate for new binding and copy it in
	ks->binds[key] = Z_TagMalloc(strlen(binding) + 1, Z_TAG_CLIENT);
	strcpy(ks->binds[key], binding);
}

/*
 * @brief
 */
static void Cl_Unbind_f(void) {
	int32_t b;

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <key> : remove commands from a key\n", Cmd_Argv(0));
		return;
	}

	b = Cl_Key(Cmd_Argv(1));
	if (b == SDLK_MLAST) {
		Com_Print("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	Cl_Bind(b, "");
}

/*
 * @brief
 */
static void Cl_UnbindAll_f(void) {
	SDLKey i;

	for (i = SDLK_FIRST; i < SDLK_MLAST; i++)
		if (ks->binds[i])
			Cl_Bind(i, "");
}

/*
 * @brief
 */
static void Cl_Bind_f(void) {
	char cmd[MAX_STRING_CHARS];

	int32_t i, c = Cmd_Argc();
	if (c < 2) {
		Com_Print("Usage: %s <key> [command] : bind a command to a key\n", Cmd_Argv(0));
		return;
	}

	const SDLKey k = Cl_Key(Cmd_Argv(1));

	if (k == SDLK_MLAST) {
		Com_Print("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	if (c == 2) {
		if (ks->binds[k])
			Com_Print("\"%s\" = \"%s\"\n", Cmd_Argv(1), ks->binds[k]);
		else
			Com_Print("\"%s\" is not bound\n", Cmd_Argv(1));
		return;
	}

	// copy the rest of the command line
	cmd[0] = 0; // start out with a null string
	for (i = 2; i < c; i++) {
		strcat(cmd, Cmd_Argv(i));
		if (i != (c - 1))
			strcat(cmd, " ");
	}

	// check for compound bindings
	if (strchr(cmd, ';')) {
		Com_Print("Complex bind \"%s\" ignored; use 'alias' instead\n", cmd);
		return;
	}

	Cl_Bind(k, cmd);
}

/*
 * @brief Writes lines containing "bind key value"
 */
void Cl_WriteBindings(file_t *f) {
	SDLKey i;

	for (i = SDLK_FIRST; i < SDLK_MLAST; i++) {
		if (ks->binds[i] && ks->binds[i][0])
			Fs_Print(f, "bind \"%s\" \"%s\"\n", Cl_KeyName(i), ks->binds[i]);
	}
}

/*
 * @brief
 */
static void Cl_BindList_f(void) {
	SDLKey i;

	for (i = SDLK_FIRST; i < SDLK_MLAST; i++)
		if (ks->binds[i] && ks->binds[i][0])
			Com_Print("\"%s\" \"%s\"\n", Cl_KeyName(i), ks->binds[i]);
}

/*
 * @brief
 */
static void Cl_WriteHistory(void) {
	file_t *f;
	uint32_t i;

	if (!(f = Fs_OpenWrite("history"))) {
		Com_Warn("Couldn't write history.\n");
		return;
	}

	for (i = (ks->edit_line + 1) % KEY_HISTORY_SIZE; i != ks->edit_line; i = (i + 1)
			% KEY_HISTORY_SIZE) {
		if (ks->lines[i][1]) {
			Fs_Print(f, "%s\n", ks->lines[i] + 1);
		}
	}

	Fs_Close(f);
}

/*
 * @brief
 */
static void Cl_ReadHistory(void) {
	file_t *f;
	char line[KEY_LINE_SIZE];

	if (!(f = Fs_OpenRead("history")))
		return;

	while (Fs_ReadLine(f, line, KEY_LINE_SIZE - 2)) {
		if (line[strlen(line) - 1] == '\n')
			line[strlen(line) - 1] = '\0';
		strncpy(&ks->lines[ks->edit_line][1], line, KEY_LINE_SIZE - 2);
		ks->edit_line = (ks->edit_line + 1) % KEY_HISTORY_SIZE;
		ks->history_line = ks->edit_line;
		ks->lines[ks->edit_line][1] = '\0';
	}

	Fs_Close(f);
}

/*
 * @brief
 */
void Cl_InitKeys(void) {
	uint16_t i;
	SDLKey k;

	cl_key_names = Z_TagMalloc(SDLK_MLAST * sizeof (char *), Z_TAG_CLIENT);

	for (k = SDLK_FIRST; k < SDLK_LAST; k++) {
		cl_key_names[k] = Z_Link(cl_key_names, Z_CopyString(SDL_GetKeyName(k)));
	}
	for (k = SDLK_MOUSE1; k < SDLK_MLAST; k++) {
		cl_key_names[k] = Z_Link(cl_key_names, Z_CopyString(va("mouse %d", k - SDLK_MOUSE1 + 1)));
	}

	memset(ks, 0, sizeof(cl_key_state_t));

	ks->insert = 1;

	for (i = 0; i < KEY_HISTORY_SIZE; i++) {
		ks->lines[i][0] = ']';
		ks->lines[i][1] = '\0';
	}

	ks->pos = 1;
	ks->edit_line = ks->history_line = 0;

	Cl_ReadHistory();

	// register our functions
	Cmd_AddCommand("bind", Cl_Bind_f, 0, NULL);
	Cmd_AddCommand("unbind", Cl_Unbind_f, 0, NULL);
	Cmd_AddCommand("unbind_all", Cl_UnbindAll_f, 0, NULL);
	Cmd_AddCommand("bind_list", Cl_BindList_f, 0, NULL);
}

/*
 * @brief
 */
void Cl_ShutdownKeys(void) {

	Cl_WriteHistory();

	Z_Free(cl_key_names);

	Cmd_RemoveCommand("bind");
	Cmd_RemoveCommand("unbind");
	Cmd_RemoveCommand("unbind_all");
	Cmd_RemoveCommand("bind_list");
}

/*
 * @brief
 */
void Cl_KeyEvent(SDLKey key, uint16_t unicode, bool down, uint32_t time) {

	// check for system commands first, swallowing such events
	if (Cl_KeySystem(key, unicode, down, time)) {
		return;
	}

	ks->down[key] = down;

	switch (ks->dest) {
		case KEY_GAME:
			Cl_KeyGame(key, unicode, down, time);
			break;
		case KEY_UI:
			// handled by Ui_Event, called from Cl_HandleEvent
			break;
		case KEY_CHAT:
			Cl_KeyMessage(key, unicode, down, time);
			break;
		case KEY_CONSOLE:
			Cl_KeyConsole(key, unicode, down, time);
			break;

		default:
			Com_Debug("Cl_KeyEvent: Bad cl_key_dest: %d.\n", ks->dest);
			break;
	}

	if (ks->dest == KEY_GAME && !(active->integer)) {
		Cvar_FullSet("active", "1", CVAR_USER_INFO | CVAR_NO_SET);
	} else if (ks->dest != KEY_GAME && active->integer) {
		Cvar_FullSet("active", "0", CVAR_USER_INFO | CVAR_NO_SET);
	}
}

/*
 * @brief Clears the current input line.
 */
void Cl_ClearTyping(void) {
	ks->lines[ks->edit_line][1] = 0;
	ks->pos = 1;
}

/*
 * @brief Returns the current input line.
 */
char *Cl_EditLine(void) {
	return ks->lines[ks->edit_line];
}
