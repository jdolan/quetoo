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

#include <ctype.h>

#include "cl_local.h"

static char **cl_key_names;

/**
 * @brief  Sets the key state destination.
 */
void Cl_SetKeyDest(cl_key_dest_t dest) {

	if (dest == cls.key_state.dest) {
		return;
	}

	// release keys and re-center the mouse when leaving KEY_GAME

	if (cls.key_state.dest == KEY_GAME) {
		SDL_Event e = { .type = SDL_KEYUP };

		for (SDL_Scancode k = SDL_SCANCODE_UNKNOWN; k < SDL_NUM_SCANCODES; k++) {
			if (cls.key_state.down[k]) {
				if (cls.key_state.binds[k] && cls.key_state.binds[k][0] == '+') {
					e.key.keysym.scancode = k;
					Cl_KeyEvent(&e);
				}
			}
		}

		SDL_SetRelativeMouseMode(false);

		const r_pixel_t cx = r_context.window_width * 0.5;
		const r_pixel_t cy = r_context.window_height * 0.5;

		SDL_WarpMouseInWindow(r_context.window, cx, cy);
	}

	switch (dest) {
		case KEY_CONSOLE:
		case KEY_CHAT:
			SDL_StartTextInput();
			break;
		case KEY_UI:
			SDL_StopTextInput();
			Ui_ViewWillAppear();
			break;
		case KEY_GAME:
			SDL_StopTextInput();
			SDL_SetRelativeMouseMode(true);
			break;
	}

	SDL_PumpEvents();

	SDL_FlushEvent(SDL_TEXTINPUT);

	cls.key_state.dest = dest;

	Cvar_ForceSetInteger(active->name, dest == KEY_GAME);
}

/**
 * @brief Interactive line editing and console scrollback.
 */
static void Cl_KeyConsole(const SDL_Event *event) {

	if (event->type == SDL_KEYUP) { // don't care
		return;
	}

	console_input_t *in = &cl_console.input;

	const SDL_Keycode key = event->key.keysym.sym;
	switch (key) {

		case SDLK_RETURN:
		case SDLK_KP_ENTER:
			Con_SubmitInput(&cl_console);
			break;

		case SDLK_TAB:
		case SDLK_KP_TAB:
			Con_CompleteInput(&cl_console);
			break;

		case SDLK_BACKSPACE:
		case SDLK_KP_BACKSPACE:
			if (in->pos > 0) {
				char *c = in->buffer + in->pos - 1;
				while (*c) {
					*c = *(c + 1);
					c++;
				}
				in->pos--;
			}
			break;

		case SDLK_DELETE:
			if (in->pos < strlen(in->buffer)) {
				char *c = in->buffer + in->pos;
				while (*c) {
					*c = *(c + 1);
					c++;
				}
			}
			break;

		case SDLK_UP:
			Con_NavigateHistory(&cl_console, CON_HISTORY_PREV);
			break;

		case SDLK_DOWN:
			Con_NavigateHistory(&cl_console, CON_HISTORY_NEXT);
			break;

		case SDLK_LEFT:
			if (SDL_GetModState() & KMOD_CTRL) { // move one word left
				while (in->pos > 0 && in->buffer[in->pos] == ' ') {
					in->pos--;    // off current word
				}
				while (in->pos > 0 && in->buffer[in->pos] != ' ') {
					in->pos--;    // and behind previous word
				}
			} else if (in->pos > 0) {
				in->pos--;
			}
			break;

		case SDLK_RIGHT:
			if (SDL_GetModState() & KMOD_CTRL) { // move one word right
				const size_t len = strlen(in->buffer);
				while (in->pos < len && in->buffer[in->pos] == ' ') {
					in->pos++;    // off current word
				}
				while (in->pos < len && in->buffer[in->pos] != ' ') {
					in->pos++;    // and in front of next word
				}
				if (in->pos < len) { // all the way in front
					in->pos++;
				}
			} else if (in->pos < strlen(in->buffer)) {
				in->pos++;
			}
			break;

		case SDLK_PAGEUP:
			if (cl_console.scroll + cl_console.height < console_state.strings.length) {
				cl_console.scroll += cl_console.height;
			} else {
				cl_console.scroll = console_state.strings.length;
			}
			break;

		case SDLK_PAGEDOWN:
			if (cl_console.scroll > cl_console.height) {
				cl_console.scroll -= cl_console.height;
			} else {
				cl_console.scroll = 0;
			}
			break;

		case SDLK_HOME:
			cl_console.scroll = console_state.strings.length;
			break;

		case SDLK_END:
			cl_console.scroll = 0;
			break;

		case SDLK_a:
			if (SDL_GetModState() & KMOD_CTRL) {
				in->pos = 0;
			}
			break;
		case SDLK_e:
			if (SDL_GetModState() & KMOD_CTRL) {
				in->pos = strlen(in->buffer);
			}
			break;
		case SDLK_c:
			if (SDL_GetModState() & KMOD_CTRL) {
				in->buffer[0] = '\0';
				in->pos = 0;
			}
			break;

		case SDLK_v:
			if ((SDL_GetModState() & KMOD_CLIPBOARD) && SDL_HasClipboardText()) {
				char *tail = g_strdup(in->buffer + in->pos);
				in->buffer[in->pos] = '\0';

				const char *text = SDL_GetClipboardText();
				g_strlcat(in->buffer, text, sizeof(in->buffer));
				g_strlcat(in->buffer, tail, sizeof(in->buffer));
				g_free(tail);

				in->pos = minf(in->pos + strlen(text), sizeof(in->buffer) - 1);
			}
			break;

		default:
			break;
	}
}

/**
 * @brief
 */
static void Cl_KeyGame(const SDL_Event *event) {
	char cmd[MAX_STRING_CHARS];

	const SDL_Scancode key = event->key.keysym.scancode;
	const char *bind = cls.key_state.binds[key];

	if (!bind) {
		return;
	}

	cmd[0] = '\0';

	if (bind[0] == '+') { // button commands add key and time as a param
		if (event->type == SDL_KEYDOWN) {
			if (cls.key_state.down[key] == false) {
				g_snprintf(cmd, sizeof(cmd), "%s %i %i\n", bind, key, cl.unclamped_time);
				cls.key_state.latched[key] = true;
			}
		} else {
			if (cls.key_state.down[key] == true && cls.key_state.latched[key] == true) {
				g_snprintf(cmd, sizeof(cmd), "-%s %i %i\n", bind + 1, key, cl.unclamped_time);
				cls.key_state.latched[key] = false;
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

/**
 * @brief
 */
static void Cl_KeyChat(const SDL_Event *event) {

	if (event->type == SDL_KEYUP) { // don't care
		return;
	}

	console_input_t *in = &cl_chat_console.input;

	switch (event->key.keysym.sym) {

		case SDLK_RETURN:
		case SDLK_KP_ENTER: {
				const char *out;
				if (cls.chat_state.team_chat) {
					out = va("say_team %s^7", in->buffer);
				} else {
					out = va("say %s^7", in->buffer);
				}
				strncpy(in->buffer, out, sizeof(in->buffer));
				Con_SubmitInput(&cl_chat_console);

				Cl_SetKeyDest(KEY_GAME);
			}
			break;

		case SDLK_BACKSPACE:
		case SDLK_KP_BACKSPACE:
			if (in->pos > 0) {
				char *c = in->buffer + in->pos - 1;
				while (*c) {
					*c = *(c + 1);
					c++;
				}
				in->pos--;
			}
			break;

		case SDLK_DELETE:
			if (in->pos < strlen(in->buffer)) {
				char *c = in->buffer + in->pos;
				while (*c) {
					*c = *(c + 1);
					c++;
				}
			}
			break;
		default:
			break;
	}
}

/**
 * @brief Returns the name of the specified key.
 */
const char *Cl_KeyName(SDL_Scancode key) {

	if (key == SDL_SCANCODE_UNKNOWN || key >= SDL_NUM_SCANCODES) {
		return va("<unknown %d>", key);
	}

	return cl_key_names[key];
}

/**
 * @brief Returns the number for the specified key name.
 */
SDL_Scancode Cl_KeyForName(const char *name) {

	if (!name || !name[0]) {
		return SDL_NUM_SCANCODES;
	}

	for (SDL_Scancode k = SDL_SCANCODE_UNKNOWN; k < SDL_NUM_SCANCODES; k++) {
		if (cl_key_names[k]) {
			if (!g_ascii_strcasecmp(name, cl_key_names[k])) {
				return k;
			}
		}
	}

	return SDL_NUM_SCANCODES;
}

/**
 * @brief Returns the key bound to the given command.
 */
SDL_Scancode Cl_KeyForBind(SDL_Scancode from, const char *binding) {

	for (SDL_Scancode k = from + 1; k < SDL_NUM_SCANCODES; k++) {
		if (g_strcmp0(binding, cls.key_state.binds[k]) == 0) {
			return k;
		}
	}

	return SDL_SCANCODE_UNKNOWN;
}

/**
 * @brief Binds the specified key to the given command.
 */
void Cl_Bind(SDL_Scancode key, const char *bind) {

	if (key == SDL_SCANCODE_UNKNOWN || key >= SDL_NUM_SCANCODES) {
		return;
	}

	// free the old binding
	if (cls.key_state.binds[key]) {
		Mem_Free(cls.key_state.binds[key]);
		cls.key_state.binds[key] = NULL;
	}

	if (!bind) {
		return;
	}

	// allocate for new binding and copy it in
	cls.key_state.binds[key] = Mem_TagMalloc(strlen(bind) + 1, MEM_TAG_CLIENT);
	strcpy(cls.key_state.binds[key], bind);
}


/**
 * @brief
 */
static void Cl_Unbind_f(void) {

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <key> : remove commands from a key\n", Cmd_Argv(0));
		return;
	}

	const SDL_Scancode k = Cl_KeyForName(Cmd_Argv(1));

	if (k == SDL_NUM_SCANCODES) {
		Com_Print("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	Cl_Bind(k, NULL);
}

/**
 * @brief
 */
static void Cl_UnbindAll_f(void) {

	for (SDL_Scancode k = SDL_SCANCODE_UNKNOWN; k < SDL_NUM_SCANCODES; k++) {
		if (cls.key_state.binds[k]) {
			Cl_Bind(k, NULL);
		}
	}
}

/**
 * @brief Bind command autocomplete
 */
static void Cl_Bind_Autocomplete_f(const uint32_t argi, GList **matches) {

	if (argi != 1) {
		return;
	}

	const char *pattern = va("%s*", Cmd_Argv(argi));

	for (SDL_Scancode k = SDL_SCANCODE_UNKNOWN; k < SDL_NUM_SCANCODES; k++) {
		if (cl_key_names[k]) {
			const char *keyName = cl_key_names[k];

			if (GlobMatch(pattern, keyName, GLOB_CASE_INSENSITIVE)) {
				*matches = g_list_insert_sorted(*matches, Com_AllocMatch(keyName, NULL), Com_MatchCompare);
			}
		}
	}
}

/**
 * @brief
 */
static void Cl_Bind_f(void) {
	char cmd[MAX_STRING_CHARS];

	const int32_t c = Cmd_Argc();
	if (c < 2) {
		Com_Print("Usage: %s <key> [command] : bind a command to a key\n", Cmd_Argv(0));
		return;
	}

	const SDL_Scancode k = Cl_KeyForName(Cmd_Argv(1));

	if (k == SDL_NUM_SCANCODES) {
		Com_Print("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	if (c == 2) {
		if (cls.key_state.binds[k]) {
			Com_Print("\"%s\" = \"%s\"\n", Cmd_Argv(1), cls.key_state.binds[k]);
		} else {
			Com_Print("\"%s\" is not bound\n", Cmd_Argv(1));
		}
		return;
	}

	// copy the rest of the command line
	cmd[0] = 0; // start out with a null string
	for (int32_t i = 2; i < c; i++) {
		strcat(cmd, Cmd_Argv(i));
		if (i != (c - 1)) {
			strcat(cmd, " ");
		}
	}

	// check for compound bindings
	if (strchr(cmd, ';')) {
		Com_Print("Complex bind \"%s\" ignored; use 'alias' instead\n", cmd);
		return;
	}

	Cl_Bind(k, cmd);
}

/**
 * @brief Writes lines containing "bind key value"
 */
void Cl_WriteBindings(file_t *f) {

	for (SDL_Scancode k = SDL_SCANCODE_UNKNOWN; k < SDL_NUM_SCANCODES; k++) {
		if (cls.key_state.binds[k] && cls.key_state.binds[k][0]) {
			Fs_Print(f, "bind \"%s\" \"%s\"\n", Cl_KeyName(k), cls.key_state.binds[k]);
		}
	}
}

/**
 * @brief
 */
static void Cl_BindList_f(void) {

	for (SDL_Scancode k = SDL_SCANCODE_UNKNOWN; k < SDL_NUM_SCANCODES; k++) {
		if (cls.key_state.binds[k] && cls.key_state.binds[k][0]) {
			Com_Print("\"%s\" \"%s\"\n", Cl_KeyName(k), cls.key_state.binds[k]);
		}
	}
}

#include "cl_binds.h"

/**
 * @brief
 */
void Cl_InitKeys(void) {

	cl_key_names = Mem_TagMalloc(SDL_NUM_SCANCODES * sizeof(char *), MEM_TAG_CLIENT);

	for (SDL_Scancode k = SDLK_UNKNOWN; k <= SDL_SCANCODE_APP2; k++) {
		const char *name = SDL_GetScancodeName(k);
		if (strlen(name)) {
			cl_key_names[k] = Mem_Link(Mem_TagCopyString(name, MEM_TAG_CLIENT), cl_key_names);
		}
	}

	for (SDL_Buttoncode b = SDL_SCANCODE_MOUSE1; b <= SDL_SCANCODE_MOUSE15; b++) {

		const char *name = va("Mouse %d", b - SDL_SCANCODE_MOUSE1 + 1);
		cl_key_names[b] = Mem_Link(Mem_TagCopyString(name, MEM_TAG_CLIENT), cl_key_names);
	}

	cl_key_names[SDL_SCANCODE_MWHEELUP] = Mem_Link(Mem_TagCopyString("Mouse Wheel Up", MEM_TAG_CLIENT), cl_key_names);
	cl_key_names[SDL_SCANCODE_MWHEELDOWN] = Mem_Link(Mem_TagCopyString("Mouse Wheel Down", MEM_TAG_CLIENT), cl_key_names);

	memset(&cls.key_state, 0, sizeof(cl_key_state_t));

	// register our functions
	cmd_t *bind_cmd = Cmd_Add("bind", Cl_Bind_f, CMD_CLIENT, NULL);
	cmd_t *unbind_cmd = Cmd_Add("unbind", Cl_Unbind_f, CMD_CLIENT, NULL);

	Cmd_SetAutocomplete(bind_cmd, Cl_Bind_Autocomplete_f);
	Cmd_SetAutocomplete(unbind_cmd, Cl_Bind_Autocomplete_f);

	Cmd_Add("unbind_all", Cl_UnbindAll_f, CMD_CLIENT, NULL);
	Cmd_Add("bind_list", Cl_BindList_f, CMD_CLIENT, NULL);

	Cbuf_AddText(DEFAULT_BINDS);
	Cbuf_Execute();
}

/**
 * @brief
 */
void Cl_ShutdownKeys(void) {

	Mem_Free(cl_key_names);
}

/**
 * @brief
 */
void Cl_KeyEvent(const SDL_Event *event) {

	switch (cls.key_state.dest) {
		case KEY_UI:
			break;
		case KEY_GAME:
			Cl_KeyGame(event);
			break;
		case KEY_CHAT:
			Cl_KeyChat(event);
			break;
		case KEY_CONSOLE:
			Cl_KeyConsole(event);
			break;

		default:
			Com_Debug(DEBUG_CLIENT, "Bad cl_key_dest: %d\n", cls.key_state.dest);
			break;
	}

	cls.key_state.down[event->key.keysym.scancode] = event->type == SDL_KEYDOWN;
}
