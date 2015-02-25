/*
 * Copyright(c) 1997-2001 id Software, Inc.
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
static _Bool Cl_KeySystem(const SDL_Event *event) {

	if (event->type == SDL_KEYUP) { // don't care
		return false;
	}

	if (event->key.keysym.sym == SDLK_ESCAPE) { // escape can cancel a few things

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

	SDL_Scancode key = event->key.keysym.scancode;
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
static void Cl_KeyConsole(const SDL_Event *event) {
	size_t i;

	if (event->type == SDL_KEYUP) // don't care
		return;

	// submit buffer on enter
	if (event->key.keysym.sym == SDLK_RETURN || event->key.keysym.sym == SDLK_KP_ENTER) {

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

	if (event->key.keysym.sym == SDLK_TAB) { // command completion
		// ignore the leading bracket in the input string
		ks->pos--;
		Con_CompleteCommand(ks->lines[ks->edit_line] + 1, &ks->pos, KEY_LINE_SIZE - 1);
		ks->pos++;
		return;
	}

	if (event->key.keysym.sym == SDLK_BACKSPACE) { // delete char before cursor
		if (ks->pos > 1) {
			strcpy(ks->lines[ks->edit_line] + ks->pos - 1, ks->lines[ks->edit_line] + ks->pos);
			ks->pos--;
		}
		return;
	}

	if (event->key.keysym.sym == SDLK_DELETE) { // delete char on cursor
		if (ks->pos < strlen(ks->lines[ks->edit_line]))
			strcpy(ks->lines[ks->edit_line] + ks->pos, ks->lines[ks->edit_line] + ks->pos + 1);
		return;
	}

	if (event->key.keysym.sym == SDLK_INSERT) { // toggle insert mode
		ks->insert ^= 1;
		return;
	}

	if (event->key.keysym.sym == SDLK_LEFT) { // move cursor left
		if (SDL_GetModState() & KMOD_CTRL) { // by a whole word
			while (ks->pos > 1 && ks->lines[ks->edit_line][ks->pos - 1] == ' ')
				ks->pos--; // get off current word
			while (ks->pos > 1 && ks->lines[ks->edit_line][ks->pos - 1] != ' ')
				ks->pos--; // and behind previous word
			return;
		}

		if (ks->pos > 1) // or just a char
			ks->pos--;
		return;
	}

	if (event->key.keysym.sym == SDLK_RIGHT) { // move cursor right
		i = strlen(ks->lines[ks->edit_line]);

		if (i == ks->pos)
			return; // no character to get

		if (SDL_GetModState() & KMOD_CTRL) { // by a whole word
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

	if (event->key.keysym.sym == SDLK_UP) { // iterate history back
		do {
			ks->history_line = (ks->history_line + KEY_HISTORY_SIZE - 1) % KEY_HISTORY_SIZE;
		} while (ks->history_line != ks->edit_line && !ks->lines[ks->history_line][1]);

		if (ks->history_line == ks->edit_line)
			ks->history_line = (ks->edit_line + 1) % KEY_HISTORY_SIZE;

		strcpy(ks->lines[ks->edit_line], ks->lines[ks->history_line]);
		ks->pos = strlen(ks->lines[ks->edit_line]);
		return;
	}

	if (event->key.keysym.sym == SDLK_DOWN) { // iterate history forward
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

	if (event->key.keysym.sym == SDLK_PAGEUP
			|| event->key.keysym.scancode == (SDL_Scancode) SDL_SCANCODE_MOUSE4) {
		cl_console.scroll += CON_SCROLL;
		if (cl_console.scroll > cl_console.last_line)
			cl_console.scroll = cl_console.last_line;
		return;
	}

	if (event->key.keysym.sym == SDLK_PAGEDOWN
			|| event->key.keysym.scancode == (SDL_Scancode) SDL_SCANCODE_MOUSE5) {
		cl_console.scroll -= CON_SCROLL;
		if (cl_console.scroll < 0)
			cl_console.scroll = 0;
		return;
	}

	// Ctrl-A jumps to the beginning of the line
	if (event->key.keysym.sym == SDLK_a && (SDL_GetModState() & KMOD_CTRL)) {
		ks->pos = 1;
		return;
	}

	if (event->key.keysym.sym == SDLK_HOME) { // go to the start of line
		if (SDL_GetModState() & KMOD_CTRL) // go to the start of the console
			cl_console.scroll = cl_console.last_line;
		else
			ks->pos = 1;
		return;
	}

	// Ctrl-E jumps to the end of the line
	if (event->key.keysym.sym == SDLK_e && (SDL_GetModState() & KMOD_CTRL)) {
		ks->pos = strlen(ks->lines[ks->edit_line]);
		return;
	}

	if (event->key.keysym.sym == SDLK_END) { // go to the end of line
		if (SDL_GetModState() & KMOD_CTRL) // go to the end of the console
			cl_console.scroll = 0;
		else
			ks->pos = strlen(ks->lines[ks->edit_line]);
		return;
	}
}

/*
 * @brief
 */
static void Cl_KeyGame(const SDL_Event *event) {
	char cmd[MAX_STRING_CHARS];

	const SDL_Scancode key = event->key.keysym.scancode;
	const char *bind = ks->binds[key];

	if (!bind)
		return;

	cmd[0] = '\0';

	if (bind[0] == '+') { // button commands add key and time as a param
		if (event->type == SDL_KEYDOWN) {
			if (ks->down[key] == false) {
				g_snprintf(cmd, sizeof(cmd), "%s %i %i\n", bind, key, cls.real_time);
			}
		} else {
			if (ks->down[key] == true) {
				g_snprintf(cmd, sizeof(cmd), "-%s %i %i\n", bind + 1, key, cls.real_time);
			}
		}
	} else {
		if (event->type == SDL_KEYDOWN) {
			g_snprintf(cmd, sizeof(cmd), "%s\n", bind);
		}
	}

	if (cmd[0]) { // send the command
		Cbuf_AddText(cmd);
	}
}

/*
 * @brief
 */
static void Cl_KeyChat(const SDL_Event *event) {

	if (event->type == SDL_KEYUP) // don't care
		return;

	if (event->key.keysym.sym == SDLK_RETURN || event->key.keysym.sym == SDLK_KP_ENTER) {
		if (strlen(cls.chat_state.buffer)) {
			if (cls.chat_state.team)
				Cbuf_AddText("say_team \"");
			else
				Cbuf_AddText("say \"");
			Cbuf_AddText(cls.chat_state.buffer);
			Cbuf_AddText("\"");
		}

		ks->dest = KEY_GAME;
		cls.chat_state.buffer[0] = '\0';
		cls.chat_state.len = 0;
		return;
	}

	if (event->key.keysym.sym == SDLK_BACKSPACE) {
		if (cls.chat_state.len) {
			cls.chat_state.len--;
			cls.chat_state.buffer[cls.chat_state.len] = '\0';
		}
		return;
	}
}

/*
 * @brief Returns the name of the specified key.
 */
const char *Cl_KeyName(SDL_Scancode key) {

	if (key == SDL_SCANCODE_UNKNOWN || key >= SDL_NUM_SCANCODES) {
		return va("<unknown %d>", key);
	}

	return cl_key_names[key];
}

/*
 * @brief Returns the number for the specified key name.
 */
SDL_Scancode Cl_Key(const char *name) {

	if (!name || !name[0]) {
		return SDL_NUM_SCANCODES;
	}

	for (SDL_Scancode k = SDL_SCANCODE_UNKNOWN; k < SDL_NUM_SCANCODES; k++) {
		if (cl_key_names[k]) {
			if (!g_ascii_strcasecmp(name, cl_key_names[k]))
				return k;
		}
	}

	return SDL_NUM_SCANCODES;
}

/*
 * @brief Binds the specified key to the given command.
 */
void Cl_Bind(SDL_Scancode key, const char *binding) {

	if (key == SDL_SCANCODE_UNKNOWN || key >= SDL_NUM_SCANCODES)
		return;

	// free the old binding
	if (ks->binds[key]) {
		Mem_Free(ks->binds[key]);
		ks->binds[key] = NULL;
	}

	if (!binding)
		return;

	// allocate for new binding and copy it in
	ks->binds[key] = Mem_TagMalloc(strlen(binding) + 1, MEM_TAG_CLIENT);
	strcpy(ks->binds[key], binding);
}

/*
 * @brief
 */
static void Cl_Unbind_f(void) {

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <key> : remove commands from a key\n", Cmd_Argv(0));
		return;
	}

	const SDL_Scancode k = Cl_Key(Cmd_Argv(1));

	if (k == SDL_NUM_SCANCODES) {
		Com_Print("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	Cl_Bind(k, NULL);
}

/*
 * @brief
 */
static void Cl_UnbindAll_f(void) {

	for (SDL_Scancode k = SDL_SCANCODE_UNKNOWN; k < SDL_NUM_SCANCODES; k++) {
		if (ks->binds[k]) {
			Cl_Bind(k, NULL);
		}
	}
}

/*
 * @brief
 */
static void Cl_Bind_f(void) {
	char cmd[MAX_STRING_CHARS];

	const int32_t c = Cmd_Argc();
	if (c < 2) {
		Com_Print("Usage: %s <key> [command] : bind a command to a key\n", Cmd_Argv(0));
		return;
	}

	const SDL_Scancode k = Cl_Key(Cmd_Argv(1));

	if (k == SDL_NUM_SCANCODES) {
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
	for (int32_t i = 2; i < c; i++) {
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

	for (SDL_Scancode k = SDL_SCANCODE_UNKNOWN; k < SDL_NUM_SCANCODES; k++) {
		if (ks->binds[k] && ks->binds[k][0]) {
			Fs_Print(f, "bind \"%s\" \"%s\"\n", Cl_KeyName(k), ks->binds[k]);
		}
	}
}

/*
 * @brief
 */
static void Cl_BindList_f(void) {

	for (SDL_Scancode k = SDL_SCANCODE_UNKNOWN; k < SDL_NUM_SCANCODES; k++) {
		if (ks->binds[k] && ks->binds[k][0]) {
			Com_Print("\"%s\" \"%s\"\n", Cl_KeyName(k), ks->binds[k]);
		}
	}
}

/*
 * @brief
 */
static void Cl_WriteHistory(void) {
	file_t *f;

	if (!(f = Fs_OpenWrite("history"))) {
		Com_Warn("Couldn't write history\n");
		return;
	}

	for (uint32_t i = (ks->edit_line + 1) % KEY_HISTORY_SIZE; i != ks->edit_line;
			i = (i + 1) % KEY_HISTORY_SIZE) {
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
	char line[KEY_LINE_SIZE];

	file_t *f;
	if (!(f = Fs_OpenRead("history")))
		return;

	while (Fs_ReadLine(f, line, sizeof(line))) {
		g_strlcpy(&ks->lines[ks->edit_line][1], line, KEY_LINE_SIZE - 1);
		ks->edit_line = (ks->edit_line + 1) % KEY_HISTORY_SIZE;
		ks->history_line = ks->edit_line;
		ks->lines[ks->edit_line][1] = '\0';
	}

	Fs_Close(f);
}

#include "cl_binds.h"

/*
 * @brief
 */
void Cl_InitKeys(void) {

	cl_key_names = Mem_TagMalloc(SDL_NUM_SCANCODES * sizeof(char *), MEM_TAG_CLIENT);

	for (SDL_Scancode k = SDLK_UNKNOWN; k <= SDL_SCANCODE_APP2; k++) {
		const char *name = SDL_GetScancodeName(k);
		if (strlen(name)) {
			cl_key_names[k] = Mem_Link(Mem_CopyString(name), cl_key_names);
		}
	}

	for (SDL_Buttoncode b = SDL_SCANCODE_MOUSE1; b <= SDL_SCANCODE_MOUSE8; b++) {
		const char *name = va("Mouse %d", b - SDL_SCANCODE_MOUSE1 + 1);
		cl_key_names[b] = Mem_Link(Mem_CopyString(name), cl_key_names);
	}

	memset(ks, 0, sizeof(cl_key_state_t));

	ks->insert = true;

	for (uint16_t i = 0; i < KEY_HISTORY_SIZE; i++) {
		ks->lines[i][0] = ']';
		ks->lines[i][1] = '\0';
	}

	ks->pos = 1;
	ks->edit_line = ks->history_line = 0;

	Cl_ReadHistory();

	// register our functions
	Cmd_Add("bind", Cl_Bind_f, CMD_CLIENT, NULL);
	Cmd_Add("unbind", Cl_Unbind_f, CMD_CLIENT, NULL);
	Cmd_Add("unbind_all", Cl_UnbindAll_f, CMD_CLIENT, NULL);
	Cmd_Add("bind_list", Cl_BindList_f, CMD_CLIENT, NULL);

	Cbuf_AddText(DEFAULT_BINDS);
	Cbuf_Execute();
}

/*
 * @brief
 */
void Cl_ShutdownKeys(void) {

	Cl_WriteHistory();

	Mem_Free(cl_key_names);
}

/*
 * @brief
 */
void Cl_KeyEvent(const SDL_Event *event) {

	// check for system commands first, swallowing such events
	if (Cl_KeySystem(event)) {
		return;
	}

	switch (ks->dest) {
		case KEY_GAME:
			Cl_KeyGame(event);
			break;
		case KEY_UI:
			break;
		case KEY_CHAT:
			Cl_KeyChat(event);
			break;
		case KEY_CONSOLE:
			Cl_KeyConsole(event);
			break;

		default:
			Com_Debug("Bad cl_key_dest: %d\n", ks->dest);
			break;
	}

	ks->down[event->key.keysym.scancode] = event->type == SDL_KEYDOWN;

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
	ks->lines[ks->edit_line][1] = '\0';
	ks->pos = 1;
}

/*
 * @brief Returns the current input line.
 */
char *Cl_EditLine(void) {
	return ks->lines[ks->edit_line];
}
