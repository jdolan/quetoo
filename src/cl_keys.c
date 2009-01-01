/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
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

#include "client.h"

static char key_lines[KEY_HISTORYSIZE][KEY_LINESIZE];
int key_linepos;
static int key_insert;

static int edit_line = 0;
static int history_line = 0;

static int key_waiting;
static char *key_bindings[K_LAST];
static qboolean key_forconsole[K_LAST];  // if true, can't be rebound while in console
static int key_repeats[K_LAST];  // if > 1, it is autorepeating
static qboolean key_down[K_LAST];
int key_numdown;

typedef struct {
	const char *name;
	int keynum;
} keyname_t;

keyname_t keynames[] = {
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

	{"SEMICOLON", ';'},  // because a raw semicolon seperates commands

	{NULL, 0}
};


/*
 * Cl_KeyConsole
 *
 * Interactive line editing and console scrollback.
 */
static void Cl_KeyConsole(int key){
	int i;

	// submit buffer on enter key with valid input
	if((key == K_ENTER || key == K_KP_ENTER) && strlen(key_lines[edit_line]) > 1){
		if(key_lines[edit_line][1] == '\\' || key_lines[edit_line][1] == '/')
			Cbuf_AddText(key_lines[edit_line] + 2);  // skip the /
		else
			Cbuf_AddText(key_lines[edit_line] + 1);  // valid command

		Cbuf_AddText("\n");
		Com_Printf("%s\n", key_lines[edit_line]);
		edit_line = (edit_line + 1) % KEY_HISTORYSIZE;
		history_line = edit_line;
		memset(key_lines[edit_line], 0, sizeof(key_lines[0]));
		key_lines[edit_line][0] = ']';
		key_linepos = 1;

		if(cls.state == ca_disconnected)  // force an update, because the command
			Cl_UpdateScreen();  // may take some time

		return;
	}

	if(key == K_TAB){  // command completion
		// ignore the leading bracket in the input string
		key_linepos--;
		Con_CompleteCommand(key_lines[edit_line] + 1, &key_linepos);
		key_linepos++;
		return;
	}

	if(key == K_BACKSPACE){  // delete char before cursor
		if(key_linepos > 1){
			strcpy(key_lines[edit_line] + key_linepos - 1, key_lines[edit_line] + key_linepos);
			key_linepos--;
		}
		return;
	}

	if(key == K_DEL || key == K_KP_DEL){  // delete char on cursor
		if(key_linepos < strlen(key_lines[edit_line]))
			strcpy(key_lines[edit_line] + key_linepos, key_lines[edit_line] + key_linepos + 1);
		return;
	}

	if(key == K_INS || key == K_KP_INS){  // toggle insert mode
		key_insert ^= 1;
		return;
	}

	if(key == K_LEFTARROW || key == K_KP_LEFTARROW){  // move cursor left
		if(key_down[K_CTRL]){ // by a whole word
			while(key_linepos > 1 && key_lines[edit_line][key_linepos - 1] == ' ')
				key_linepos--;  // get off current word
			while(key_linepos > 1 && key_lines[edit_line][key_linepos - 1] != ' ')
				key_linepos--;  // and behind previous word
			return;
		}

		if(key_linepos > 1)  //or just a char
			key_linepos--;
		return;
	}

	if(key == K_RIGHTARROW || key == K_KP_RIGHTARROW){  // move cursor right
		if((i = strlen(key_lines[edit_line])) == key_linepos)
			return;	// no character to get

		if(key_down[K_CTRL]){  //by a whole word
			while(key_linepos < i && key_lines[edit_line][key_linepos + 1] == ' ')
				key_linepos++;  // get off current word
			while(key_linepos < i && key_lines[edit_line][key_linepos + 1] != ' ')
				key_linepos++;  // and in front of next word
			if(key_linepos < i)  // all the way in front
				key_linepos++;
			return;
		}

		key_linepos++;  // or just a char
		return;
	}

	if(key == K_UPARROW || key == K_KP_UPARROW){  // iterate history back
		do {
			history_line = (history_line + KEY_HISTORYSIZE - 1) % KEY_HISTORYSIZE;
		} while(history_line != edit_line && !key_lines[history_line][1]);

		if(history_line == edit_line)
			history_line = (edit_line + 1) % KEY_HISTORYSIZE;

		strcpy(key_lines[edit_line], key_lines[history_line]);
		key_linepos = strlen(key_lines[edit_line]);
		return;
	}

	if(key == K_DOWNARROW || key == K_KP_DOWNARROW){  // iterate history forward
		if(history_line == edit_line)
			return;
		do {
			history_line = (history_line + 1) % KEY_HISTORYSIZE;
		} while(history_line != edit_line && !key_lines[history_line][1]);

		if(history_line == edit_line){
			strcpy(key_lines[edit_line], "]");
			key_linepos = 1;
		} else {
			strcpy(key_lines[edit_line], key_lines[history_line]);
			key_linepos = strlen(key_lines[edit_line]);
		}
		return;
	}

	if(key == K_PGUP || key == K_KP_PGUP || key == K_MWHEELUP){
		cl_con.scroll += CON_SCROLL;
		if(cl_con.scroll > cl_con.lastline)
			cl_con.scroll = cl_con.lastline;
		return;
	}

	if(key == K_PGDN || key == K_KP_PGDN || key == K_MWHEELDOWN){
		cl_con.scroll -= CON_SCROLL;
		if(cl_con.scroll < 0)
			cl_con.scroll = 0;
		return;
	}

	if(key == K_CTRL_A){  // start of line
		key_linepos = 1;
		return;
	}

	if(key == K_HOME || key == K_KP_HOME){	// go to the start of line
		if(key_down[K_CTRL])	// go to the start of the console
			cl_con.scroll = cl_con.lastline;
		else
			key_linepos = 1;
		return;
	}

	if(key == K_CTRL_E) {	// Ctrl+E shortcut to go to the end of line
		key_linepos = strlen(key_lines[edit_line]);
		return;
	}

	if(key == K_END || key == K_KP_END){	// go to the end of line
		if(key_down[K_CTRL])	// go to the end of the console
			cl_con.scroll = 0;
		else
			key_linepos = strlen(key_lines[edit_line]);
		return;
	}

	if(key < 32 || key > 127)
		return;  // non printable

	if(key_linepos < KEY_LINESIZE - 1){
		int i;

		if(key_insert){  // can't do strcpy to move string to right
			i = strlen(key_lines[edit_line]) - 1;

			if(i == 254)
				i--;

			for(; i >= key_linepos; i--)
				key_lines[edit_line][i + 1] = key_lines[edit_line][i];
		}

		i = key_lines[edit_line][key_linepos];
		key_lines[edit_line][key_linepos] = key;
		key_linepos++;

		if(!i)  // only null terminate if at the end
			key_lines[edit_line][key_linepos] = 0;
	}
}


qboolean chat_team;
char chat_buffer[KEY_LINESIZE];
int chat_bufferlen = 0;

/*
 * Cl_KeyMessage
 */
static void Cl_KeyMessage(int key){
	if(key == K_ENTER || key == K_KP_ENTER){
		if(*chat_buffer){
			if(chat_team)
				Cbuf_AddText("say_team \"");
			else
				Cbuf_AddText("say \"");
			Cbuf_AddText(chat_buffer);
			Cbuf_AddText("\"");
		}

		cls.key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if(key == K_ESCAPE){
		cls.key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if(key == K_BACKSPACE){
		if(chat_bufferlen){
			chat_bufferlen--;
			chat_buffer[chat_bufferlen] = 0;
		}
		return;
	}

	if(key < 32 || key > 127)
		return;  // non printable

	if(chat_bufferlen == sizeof(chat_buffer) - 1)
		return; // all full

	chat_buffer[chat_bufferlen++] = key;
	chat_buffer[chat_bufferlen] = 0;
}


/*
 * Cl_StringToKeynum
 *
 * Returns a key number to be used to index key_bindings[] by looking at
 * the given string.  Single ascii characters return themselves, while
 * the K_* names are matched up.
 */
static int Cl_StringToKeynum(const char *str){
	keyname_t *kn;

	if(!str || !str[0])
		return -1;
	if(!str[1])
		return str[0];

	for(kn = keynames; kn->name; kn++){
		if(!strcasecmp(str, kn->name))
			return kn->keynum;
	}
	return -1;
}


/*
 * Cl_KeynumToString
 *
 * Returns a string (either a single ascii char, or a K_* name) for the
 * given keynum.
 * FIXME: handle quote special (general escape sequence?)
 */
static const char *Cl_KeynumToString(int keynum){
	keyname_t *kn;
	static char s[2];

	if(keynum == -1)
		return "<KEY NOT FOUND>";
	if(keynum > 32 && keynum < 127){  // printable ascii
		s[0] = keynum;
		s[1] = 0;
		return s;
	}

	for(kn = keynames; kn->name; kn++)
		if(keynum == kn->keynum)
			return kn->name;

	return "<UNKNOWN KEYNUM>";
}


/*
 * Cl_Bind
 */
static void Cl_Bind(int keynum, char *binding){

	if(keynum == -1)
		return;

	// free old binding
	if(key_bindings[keynum]){
		Z_Free(key_bindings[keynum]);
		key_bindings[keynum] = NULL;
	}

	// allocate for new binding and copy it in
	key_bindings[keynum] = Z_Malloc(strlen(binding) + 1);
	strcpy(key_bindings[keynum], binding);
}


/*
 * Cl_Unbind_f
 */
static void Cl_Unbind_f(void){
	int b;

	if(Cmd_Argc() != 2){
		Com_Printf("Usage: %s <key> : remove commands from a key\n", Cmd_Argv(0));
		return;
	}

	b = Cl_StringToKeynum(Cmd_Argv(1));
	if(b == -1){
		Com_Printf("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	Cl_Bind(b, "");
}


/*
 * Cl_UnbindAll_f
 */
static void Cl_UnbindAll_f(void){
	int i;

	for(i = K_FIRST; i < K_LAST; i++)
		if(key_bindings[i])
			Cl_Bind(i, "");
}


/*
 * Cl_Bind_f
 */
static void Cl_Bind_f(void){
	int i, c, b;
	char cmd[1024];

	c = Cmd_Argc();

	if(c < 2){
		Com_Printf("Usage: %s <key> [command] : attach a command to a key\n", Cmd_Argv(0));
		return;
	}
	b = Cl_StringToKeynum(Cmd_Argv(1));
	if(b == -1){
		Com_Printf("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	if(c == 2){
		if(key_bindings[b])
			Com_Printf("\"%s\" = \"%s\"\n", Cmd_Argv(1), key_bindings[b]);
		else
			Com_Printf("\"%s\" is not bound\n", Cmd_Argv(1));
		return;
	}

	// copy the rest of the command line
	cmd[0] = 0;  // start out with a null string
	for(i = 2; i < c; i++){
		strcat(cmd, Cmd_Argv(i));
		if(i != (c - 1))
			strcat(cmd, " ");
	}

	Cl_Bind(b, cmd);
}


/*
 * Cl_WriteBindings
 *
 * Writes lines containing "bind key value"
 */
void Cl_WriteBindings(FILE *f){
	int i;

	for(i = K_FIRST; i < K_LAST; i++)
		if(key_bindings[i] && key_bindings[i][0])
			fprintf(f, "bind %s \"%s\"\n", Cl_KeynumToString(i), key_bindings[i]);
}


/*
 * Cl_Bindlist_f
 */
static void Cl_Bindlist_f(void){
	int i;

	for(i = K_FIRST; i < K_LAST; i++)
		if(key_bindings[i] && key_bindings[i][0])
			Com_Printf("%s \"%s\"\n", Cl_KeynumToString(i), key_bindings[i]);
}


/*
 * Cl_WriteHistory
 */
static void Cl_WriteHistory(void){
	FILE *f;
	char path[MAX_OSPATH];
	int i;

	snprintf(path, sizeof(path), "%s/history", Fs_Gamedir());
	f = fopen(path, "w");
	if(!f){
		Com_Warn("Couldn't write %s.\n", path);
		return;
	}

	for(i = (edit_line + 1) % KEY_HISTORYSIZE; i != edit_line; i = (i+1) % KEY_HISTORYSIZE) {
		if(key_lines[i][1]){
			fprintf(f, "%s\n", key_lines[i] + 1);
		}
	}

	Fs_CloseFile(f);
}


/*
 * Cl_ReadHistory
 */
static void Cl_ReadHistory(void){
	char path[MAX_OSPATH];
	FILE *f;
	char line[KEY_LINESIZE];

	snprintf(path, sizeof(path), "%s/history", Fs_Gamedir());

	f = fopen(path, "r");
	if(!f)
		return;

	while(fgets(line, KEY_LINESIZE - 2, f)) {
		if(line[strlen(line) - 1] == '\n')
			line[strlen(line) - 1] = 0;
		strncpy(&key_lines[edit_line][1], line, KEY_LINESIZE - 2);
		edit_line = (edit_line + 1) % KEY_HISTORYSIZE;
		history_line = edit_line;
		key_lines[edit_line][1] = 0;
	}

	Fs_CloseFile(f);
}


/*
 * Cl_InitKeys
 */
void Cl_InitKeys(void){
	int i;

	key_insert = 1;
	key_numdown = 0;

	for(i = K_FIRST; i < K_LAST; i++){

		key_down[i] = false;
		key_repeats[i] = 0;

		key_forconsole[i] = true;
	}

	key_forconsole['`'] = false;
	key_forconsole['~'] = false;

	for(i = 0; i < KEY_HISTORYSIZE; i++){
		key_lines[i][0] = ']';
		key_lines[i][1] = 0;
	}

	key_linepos = 1;
	edit_line = history_line = 0;

	Cl_ReadHistory();

	// register our functions
	Cmd_AddCommand("bind", Cl_Bind_f, NULL);
	Cmd_AddCommand("unbind", Cl_Unbind_f, NULL);
	Cmd_AddCommand("unbindall", Cl_UnbindAll_f, NULL);
	Cmd_AddCommand("bindlist", Cl_Bindlist_f, NULL);
}


/*
 * Cl_ShutdownKeys
 */
void Cl_ShutdownKeys(void){

	Cl_WriteHistory();

	Cmd_RemoveCommand("bind");
	Cmd_RemoveCommand("unbind");
	Cmd_RemoveCommand("unbindall");
	Cmd_RemoveCommand("bindlist");
}


/*
 * Cl_KeyEvent
 */
void Cl_KeyEvent(unsigned key, unsigned short unicode, qboolean down, unsigned time){
	char *kb;
	char cmd[MAX_STRING_CHARS];

	// hack for modal presses
	if(key_waiting == -1){
		if(down)
			key_waiting = key;
		return;
	}

	// update auto-repeat status
	if(down)
		key_repeats[key]++;
	else
		key_repeats[key] = 0;

	if(key == K_ESCAPE && down){  // escape can cancel messagemode or score

		if(cls.key_dest == key_message){
			Cl_KeyMessage(unicode);
			return;
		}

		if(cl.frame.playerstate.stats[STAT_LAYOUTS]){
			Cbuf_AddText("score\n");
			return;
		}
	}

	// but usually escape and tilde are for toggling the console
	if((unicode == '`' || unicode == '~' || unicode == K_ESCAPE) && down){
		Con_ToggleConsole_f();
		return;
	}

	// little hack for slow motion or fast forward demo playback
	if(down && cl.demoserver && Com_ServerState() &&
			(key == K_LEFTARROW || key == K_RIGHTARROW)){
		extern cvar_t *timescale;

		float ts = timescale->value + (key == K_LEFTARROW ? -0.25 : 0.25);

		ts = ts < 0.5 ? 0.5 : ts;  // enforce reasonable bounds
		ts = ts > 4.0 ? 4.0 : ts;

		Cvar_Set("timescale", va("%f", ts));

		Com_Printf("Demo playback rate %d%%\n", (int)((ts / 1.0) * 100));
	}

	// track if any key is down for BUTTON_ANY
	key_down[key] = down;
	if(down){
		if(key_repeats[key] == 1)
			key_numdown++;
	} else {
		key_numdown--;
		if(key_numdown < 0)
			key_numdown = 0;
	}

	// key up events only generate commands if the game key binding is
	// a button command (leading + sign).  These will occur even in console mode,
	// to keep the character from continuing an action started before a console
	// switch.  Button commands include the kenum as a parameter, so multiple
	// downs can be matched with ups
	if(!down){
		kb = key_bindings[key];
		if(kb && kb[0] == '+'){
			snprintf(cmd, sizeof(cmd), "-%s %i %i\n", kb + 1, key, time);
			Cbuf_AddText(cmd);
		}
		return;
	}

	// if not a consolekey, send to the interpreter no matter what mode is
	if((cls.key_dest == key_console && !key_forconsole[key]) ||
			(cls.key_dest == key_game && (cls.state == ca_active || !key_forconsole[key]))){
		kb = key_bindings[key];
		if(kb){
			if(kb[0] == '+'){  // button commands add keynum and time as a parm
				snprintf(cmd, sizeof(cmd), "%s %i %i\n", kb, key, time);
				Cbuf_AddText(cmd);
			} else {
				Cbuf_AddText(kb);
				Cbuf_AddText("\n");
			}
		}
		return;
	}

	if(!down)
		return;  // other systems only care about key down events

	switch(cls.key_dest){
		case key_message:
			Cl_KeyMessage(unicode);
			break;
		case key_game:
		case key_console:
			Cl_KeyConsole(unicode);
			break;
		default:
			Com_Warn("Cl_KeyEvent: Bad cls.key_dest.\n");
			break;
	}
}


/*
 * Cl_ClearTyping
 *
 * Clears the current input line.
 */
void Cl_ClearTyping(void) {
	key_lines[edit_line][1] = 0;
	key_linepos = 1;
}


/*
 * Cl_EditLine
 *
 * Returns the current input line.
 */
char *Cl_EditLine(void) {
	return key_lines[edit_line];
}
