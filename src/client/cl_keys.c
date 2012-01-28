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

key_name_t key_names[] = {
	{"TAB", K_TAB},
	{"ENTER", K_ENTER},
	{"ESCAPE", K_ESCAPE},
	{"SPACE", K_SPACE},
	{"BACKSPACE", K_BACKSPACE},
	{"UPARROW", K_UPARROW},
	{"DOWNARROW", K_DOWNARROW},
	{"LEFTARROW", K_LEFTARROW},
	{"RIGHTARROW", K_RIGHTARROW},

	{"ALT", K_ALT},
	{"CTRL", K_CTRL},
	{"SHIFT", K_SHIFT},

	{"F1", K_F1},
	{"F2", K_F2},
	{"F3", K_F3},
	{"F4", K_F4},
	{"F5", K_F5},
	{"F6", K_F6},
	{"F7", K_F7},
	{"F8", K_F8},
	{"F9", K_F9},
	{"F10", K_F10},
	{"F11", K_F11},
	{"F12", K_F12},

	{"INS", K_INS},
	{"DEL", K_DEL},
	{"PGDN", K_PGDN},
	{"PGUP", K_PGUP},
	{"HOME", K_HOME},
	{"END", K_END},

	{"MOUSE1", K_MOUSE1},
	{"MOUSE2", K_MOUSE2},
	{"MOUSE3", K_MOUSE3},
	{"MOUSE4", K_MOUSE4},
	{"MOUSE5", K_MOUSE5},

	{"AUX1", K_AUX1},
	{"AUX2", K_AUX2},
	{"AUX3", K_AUX3},
	{"AUX4", K_AUX4},
	{"AUX5", K_AUX5},
	{"AUX6", K_AUX6},
	{"AUX7", K_AUX7},
	{"AUX8", K_AUX8},
	{"AUX9", K_AUX9},
	{"AUX10", K_AUX10},
	{"AUX11", K_AUX11},
	{"AUX12", K_AUX12},
	{"AUX13", K_AUX13},
	{"AUX14", K_AUX14},
	{"AUX15", K_AUX15},
	{"AUX16", K_AUX16},


	{"KP_HOME", K_KP_HOME},
	{"KP_UPARROW", K_KP_UPARROW},
	{"KP_PGUP", K_KP_PGUP},
	{"KP_LEFTARROW", K_KP_LEFTARROW},
	{"KP_5", K_KP_5},
	{"KP_RIGHTARROW", K_KP_RIGHTARROW},
	{"KP_END", K_KP_END},
	{"KP_DOWNARROW", K_KP_DOWNARROW},
	{"KP_PGDN", K_KP_PGDN},
	{"KP_ENTER", K_KP_ENTER},
	{"KP_INS", K_KP_INS},
	{"KP_DEL", K_KP_DEL},
	{"KP_SLASH", K_KP_SLASH},
	{"KP_MINUS", K_KP_MINUS},
	{"KP_PLUS", K_KP_PLUS},

	{"MWHEELUP", K_MWHEELUP},
	{"MWHEELDOWN", K_MWHEELDOWN},

	{"PAUSE", K_PAUSE},

	{"SEMICOLON", ';'},  // because a raw semicolon separates commands

	{NULL, 0}
};

static cl_key_state_t *ks = &cls.key_state;


/*
 * Cl_KeyConsole
 *
 * Interactive line editing and console scrollback.
 */
static void Cl_KeyConsole(unsigned key, unsigned short unicode, boolean_t down,
		unsigned time) {
	boolean_t numlock = ks->down[K_NUMLOCK];
	size_t i;

	if (!down) // don't care
		return;

	// submit buffer on enter
	if (key == K_ENTER || key == K_KP_ENTER) {

		if (ks->lines[ks->edit_line][1] == '\\' || ks->lines[ks->edit_line][1]
				== '/')
			Cbuf_AddText(ks->lines[ks->edit_line] + 2); // skip the /
		else
			Cbuf_AddText(ks->lines[ks->edit_line] + 1); // valid command

		Cbuf_AddText("\n");
		Com_Print("%s\n", ks->lines[ks->edit_line]);

		ks->edit_line = (ks->edit_line + 1) % KEY_HISTORYSIZE;
		ks->history_line = ks->edit_line;

		memset(ks->lines[ks->edit_line], 0, sizeof(ks->lines[0]));

		ks->lines[ks->edit_line][0] = ']';
		ks->pos = 1;

		if (cls.state == CL_DISCONNECTED) // force an update, because the command
			Cl_UpdateScreen(); // may take some time

		return;
	}

	if (key == K_TAB) { // command completion
		// ignore the leading bracket in the input string
		ks->pos--;
		Con_CompleteCommand(ks->lines[ks->edit_line] + 1, &ks->pos);
		ks->pos++;
		return;
	}

	if (key == K_BACKSPACE) { // delete char before cursor
		if (ks->pos > 1) {
			strcpy(ks->lines[ks->edit_line] + ks->pos - 1, ks->lines[ks->edit_line] + ks->pos);
			ks->pos--;
		}
		return;
	}

	if (key == K_DEL || (key == K_KP_DEL && !numlock)) { // delete char on cursor
		if (ks->pos < strlen(ks->lines[ks->edit_line]))
			strcpy(ks->lines[ks->edit_line] + ks->pos, ks->lines[ks->edit_line] + ks->pos + 1);
		return;
	}

	if (key == K_INS || (key == K_KP_INS && !numlock)) { // toggle insert mode
		ks->insert ^= 1;
		return;
	}

	if (key == K_LEFTARROW || (key == K_KP_LEFTARROW && !numlock)) { // move cursor left
		if (ks->down[K_CTRL]) { // by a whole word
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

	if (key == K_RIGHTARROW || (key == K_KP_RIGHTARROW && !numlock)) { // move cursor right
		i = strlen(ks->lines[ks->edit_line]);

		if (i == ks->pos)
			return; // no character to get

		if (ks->down[K_CTRL]) { //by a whole word
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

	if (key == K_UPARROW || (key == K_KP_UPARROW && !numlock)) { // iterate history back
		do {
			ks->history_line = (ks->history_line + KEY_HISTORYSIZE - 1)
					% KEY_HISTORYSIZE;
		} while (ks->history_line != ks->edit_line
				&& !ks->lines[ks->history_line][1]);

		if (ks->history_line == ks->edit_line)
			ks->history_line = (ks->edit_line + 1) % KEY_HISTORYSIZE;

		strcpy(ks->lines[ks->edit_line], ks->lines[ks->history_line]);
		ks->pos = strlen(ks->lines[ks->edit_line]);
		return;
	}

	if (key == K_DOWNARROW || (key == K_KP_DOWNARROW && !numlock)) { // iterate history forward
		if (ks->history_line == ks->edit_line)
			return;
		do {
			ks->history_line = (ks->history_line + 1) % KEY_HISTORYSIZE;
		} while (ks->history_line != ks->edit_line
				&& !ks->lines[ks->history_line][1]);

		if (ks->history_line == ks->edit_line) {
			strcpy(ks->lines[ks->edit_line], "]");
			ks->pos = 1;
		} else {
			strcpy(ks->lines[ks->edit_line], ks->lines[ks->history_line]);
			ks->pos = strlen(ks->lines[ks->edit_line]);
		}
		return;
	}

	if (key == K_PGUP || (key == K_KP_PGUP && !numlock) || key == K_MWHEELUP) {
		cl_con.scroll += CON_SCROLL;
		if (cl_con.scroll > cl_con.last_line)
			cl_con.scroll = cl_con.last_line;
		return;
	}

	if (key == K_PGDN || (key == K_KP_PGDN && !numlock) || key == K_MWHEELDOWN) {
		cl_con.scroll -= CON_SCROLL;
		if (cl_con.scroll < 0)
			cl_con.scroll = 0;
		return;
	}

	if (key == K_CTRL_A) { // start of line
		ks->pos = 1;
		return;
	}

	if (key == K_HOME || (key == K_KP_HOME && !numlock)) { // go to the start of line
		if (ks->down[K_CTRL]) // go to the start of the console
			cl_con.scroll = cl_con.last_line;
		else
			ks->pos = 1;
		return;
	}

	if (key == K_CTRL_E) { // Ctrl+E shortcut to go to the end of line
		ks->pos = strlen(ks->lines[ks->edit_line]);
		return;
	}

	if (key == K_END || (key == K_KP_END && !numlock)) { // go to the end of line
		if (ks->down[K_CTRL]) // go to the end of the console
			cl_con.scroll = 0;
		else
			ks->pos = strlen(ks->lines[ks->edit_line]);
		return;
	}

	if (unicode < 32 || unicode > 127)
		return; // non printable

	if (ks->pos < KEY_LINESIZE - 1) {

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
 * Cl_KeyGame
 */
static void Cl_KeyGame(unsigned key, unsigned short unicode, boolean_t down,
		unsigned time) {
	char cmd[MAX_STRING_CHARS];
	char *kb;

	if (cls.state != CL_ACTIVE) // not in game
		return;

	if (!ks->binds[key])
		return;

	memset(cmd, 0, sizeof(cmd));
	kb = ks->binds[key];

	if (kb[0] == '+') { // button commands add key and time as a param
		if (down)
			snprintf(cmd, sizeof(cmd), "%s %i %i\n", kb, key, time);
		else
			snprintf(cmd, sizeof(cmd), "-%s %i %i\n", kb + 1, key, time);
	} else {
		if (down) {
			snprintf(cmd, sizeof(cmd), "%s\n", kb);
		}
	}

	if (cmd[0]) // send the command
		Cbuf_AddText(cmd);
}

/*
 * Cl_KeyMessage
 */
static void Cl_KeyMessage(unsigned key, unsigned short unicode, boolean_t down,
		unsigned time) {

	if (!down) // don't care
		return;

	if (key == K_ENTER || key == K_KP_ENTER) {
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

	if (key == K_BACKSPACE) {
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
 * Cl_StringToKeyNum
 *
 * Returns a key number to be used to index ks->binds[] by looking at
 * the given string.  Single ascii characters return themselves, while
 * the K_* names are matched up.
 */
static int Cl_StringToKeyNum(const char *str) {
	key_name_t *kn;

	if (!str || !str[0])
		return -1;

	if (!str[1])
		return str[0];

	for (kn = key_names; kn->name; kn++) {
		if (!strcasecmp(str, kn->name))
			return kn->key_num;
	}

	return -1;
}

/*
 * Cl_KeyNumToString
 *
 * Returns a string (either a single ASCII char, or a K_* name) for the
 * given key_num.
 * FIXME: handle quote special (general escape sequence?)
 */
const char *Cl_KeyNumToString(unsigned short key_num) {
	key_name_t *kn;
	static char s[2];

	if (key_num > 32 && key_num < 127) { // printable ASCII
		s[0] = key_num;
		s[1] = 0;
		return s;
	}

	for (kn = key_names; kn->name; kn++)
		if (key_num == kn->key_num)
			return kn->name;

	return "<UNKNOWN KEYNUM>";
}

/*
 * Cl_Bind
 */
static void Cl_Bind(int key_num, char *binding) {

	if (key_num == -1)
		return;

	// free old binding
	if (ks->binds[key_num]) {
		Z_Free(ks->binds[key_num]);
		ks->binds[key_num] = NULL;
	}

	// allocate for new binding and copy it in
	ks->binds[key_num] = Z_Malloc(strlen(binding) + 1);
	strcpy(ks->binds[key_num], binding);
}

/*
 * Cl_Unbind_f
 */
static void Cl_Unbind_f(void) {
	int b;

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <key> : remove commands from a key\n", Cmd_Argv(0));
		return;
	}

	b = Cl_StringToKeyNum(Cmd_Argv(1));
	if (b == -1) {
		Com_Print("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	Cl_Bind(b, "");
}

/*
 * Cl_UnbindAll_f
 */
static void Cl_UnbindAll_f(void) {
	int i;

	for (i = K_FIRST; i < K_LAST; i++)
		if (ks->binds[i])
			Cl_Bind(i, "");
}

/*
 * Cl_Bind_f
 */
static void Cl_Bind_f(void) {
	int i, c, b;
	char cmd[1024];

	c = Cmd_Argc();

	if (c < 2) {
		Com_Print("Usage: %s <key> [command] : attach a command to a key\n",
				Cmd_Argv(0));
		return;
	}
	b = Cl_StringToKeyNum(Cmd_Argv(1));
	if (b == -1) {
		Com_Print("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	if (c == 2) {
		if (ks->binds[b])
			Com_Print("\"%s\" = \"%s\"\n", Cmd_Argv(1), ks->binds[b]);
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

	Cl_Bind(b, cmd);
}

/*
 * Cl_WriteBindings
 *
 * Writes lines containing "bind key value"
 */
void Cl_WriteBindings(FILE *f) {
	unsigned short i;

	for (i = K_FIRST; i < K_LAST; i++)
		if (ks->binds[i] && ks->binds[i][0])
			fprintf(f, "bind %s \"%s\"\n", Cl_KeyNumToString(i), ks->binds[i]);
}

/*
 * Cl_Bindlist_f
 */
static void Cl_BindList_f(void) {
	unsigned short i;

	for (i = K_FIRST; i < K_LAST; i++)
		if (ks->binds[i] && ks->binds[i][0])
			Com_Print("%s \"%s\"\n", Cl_KeyNumToString(i), ks->binds[i]);
}

/*
 * Cl_WriteHistory
 */
static void Cl_WriteHistory(void) {
	FILE *f;
	char path[MAX_OSPATH];
	unsigned int i;

	snprintf(path, sizeof(path), "%s/history", Fs_Gamedir());
	f = fopen(path, "w");
	if (!f) {
		Com_Warn("Couldn't write %s.\n", path);
		return;
	}

	for (i = (ks->edit_line + 1) % KEY_HISTORYSIZE; i != ks->edit_line; i = (i
			+ 1) % KEY_HISTORYSIZE) {
		if (ks->lines[i][1]) {
			fprintf(f, "%s\n", ks->lines[i] + 1);
		}
	}

	Fs_CloseFile(f);
}

/*
 * Cl_ReadHistory
 */
static void Cl_ReadHistory(void) {
	char path[MAX_OSPATH];
	FILE *f;
	char line[KEY_LINESIZE];

	snprintf(path, sizeof(path), "%s/history", Fs_Gamedir());

	f = fopen(path, "r");
	if (!f)
		return;

	while (fgets(line, KEY_LINESIZE - 2, f)) {
		if (line[strlen(line) - 1] == '\n')
			line[strlen(line) - 1] = 0;
		strncpy(&ks->lines[ks->edit_line][1], line, KEY_LINESIZE - 2);
		ks->edit_line = (ks->edit_line + 1) % KEY_HISTORYSIZE;
		ks->history_line = ks->edit_line;
		ks->lines[ks->edit_line][1] = 0;
	}

	Fs_CloseFile(f);
}

/*
 * Cl_InitKeys
 */
void Cl_InitKeys(void) {
	int i;

	memset(ks, 0, sizeof(cl_key_state_t));

	ks->insert = 1;

	for (i = K_FIRST; i < K_LAST; i++) {
		ks->down[i] = false;
	}

	for (i = 0; i < KEY_HISTORYSIZE; i++) {
		ks->lines[i][0] = ']';
		ks->lines[i][1] = 0;
	}

	ks->pos = 1;
	ks->edit_line = ks->history_line = 0;

	Cl_ReadHistory();

	// register our functions
	Cmd_AddCommand("bind", Cl_Bind_f, NULL);
	Cmd_AddCommand("unbind", Cl_Unbind_f, NULL);
	Cmd_AddCommand("unbind_all", Cl_UnbindAll_f, NULL);
	Cmd_AddCommand("bind_list", Cl_BindList_f, NULL);
}

/*
 * Cl_ShutdownKeys
 */
void Cl_ShutdownKeys(void) {

	Cl_WriteHistory();

	Cmd_RemoveCommand("bind");
	Cmd_RemoveCommand("unbind");
	Cmd_RemoveCommand("unbind_all");
	Cmd_RemoveCommand("bind_list");
}

/*
 * Cl_KeyEvent
 */
void Cl_KeyEvent(unsigned key, unsigned short unicode, boolean_t down,
		unsigned time) {

	if (key == K_ESCAPE && down) { // escape can cancel a few things

		// message mode
		if (ks->dest == KEY_CHAT) {

			// we should always be in game here, but check to be safe
			if (cls.state == CL_ACTIVE)
				ks->dest = KEY_GAME;
			else
				ks->dest = KEY_UI;
			return;
		}

		// score
		if (cl.frame.ps.stats[STAT_SCORES]) {
			Cbuf_AddText("score\n");
			return;
		}

		// console
		if (ks->dest == KEY_CONSOLE) {
			Cl_ToggleConsole_f();
			return;
		}

		// and menus
		if (ks->dest == KEY_UI) {

			// if we're in the game, just hide the menus
			if (cls.state == CL_ACTIVE)
				ks->dest = KEY_GAME;
			// otherwise, pop back from a child menu
			else
				Cbuf_AddText("mn_pop\n");

			return;
		}

		ks->dest = KEY_UI;
		return;
	}

	// tilde always toggles the console
	if ((unicode == '`' || unicode == '~') && down) {
		Cl_ToggleConsole_f();
		return;
	}

	ks->down[key] = down;

	if (!down) // always send up events to release button binds
		Cl_KeyGame(key, unicode, down, time);

	switch (ks->dest) {
	case KEY_GAME:
		Cl_KeyGame(key, unicode, down, time);
		break;
	case KEY_UI:
		// the UI optionally handle events through Cl_HandleEvent
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
}

/*
 * Cl_ClearTyping
 *
 * Clears the current input line.
 */
void Cl_ClearTyping(void) {
	ks->lines[ks->edit_line][1] = 0;
	ks->pos = 1;
}

/*
 * Cl_EditLine
 *
 * Returns the current input line.
 */
char *Cl_EditLine(void) {
	return ks->lines[ks->edit_line];
}
