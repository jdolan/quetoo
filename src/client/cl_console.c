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

/*
 * cl_console.c
 * Drawing functions for the client console
 */

#include "client.h"

console_t cl_con;

static cvar_t *con_notifytime;
static cvar_t *con_alpha;

extern int key_linepos;

/*
 * Cl_ToggleConsole_f
 */
void Cl_ToggleConsole_f(void){

	Cl_ClearTyping();
	Cl_ClearNotify();

	if(cls.key_dest == key_console){

		if(cls.state == ca_active)
			cls.key_dest = key_game;
		else
			cls.key_dest = key_menu;
	}
	else
		cls.key_dest = key_console;
}


/*
 * Cl_UpdateNotify
 *
 * Update client message notification times
 */
void Cl_UpdateNotify(int lastline){
	int i;

	for(i = lastline; i < cl_con.lastline; i++)
		cl_con.times[i % CON_NUMTIMES] = cls.realtime;
}


/*
 * Cl_ClearNotify
 *
 * Clear client message notification times
 */
void Cl_ClearNotify(void){
	int i;

	for(i = 0; i < CON_NUMTIMES; i++)
		cl_con.times[i] = 0;
}


/*
 * Cl_MessageMode_f
 */
static void Cl_MessageMode_f(void){
	chat_team = false;
	cls.key_dest = key_message;
}


/*
 * Cl_MessageMode2_f
 */
static void Cl_MessageMode2_f(void){
	chat_team = true;
	cls.key_dest = key_message;
}


/*
 * Cl_InitConsole
 */
void Cl_InitConsole(void){

	memset(&cl_con, 0, sizeof(console_t));

	cl_con.width = -1;
	// the last line of the console is reserved for input
	Con_Resize(&cl_con, r_state.width >> 4, (r_state.height >> 5) - 1);

	Cl_ClearNotify();

	con_notifytime = Cvar_Get("con_notifytime", "3", CVAR_ARCHIVE, "Seconds to draw the last messages on the game top");
	con_alpha = Cvar_Get("con_alpha", "0.3", CVAR_ARCHIVE, "Console alpha background [0.0-1.0]");

	Cmd_AddCommand("toggleconsole", Cl_ToggleConsole_f, "Toggle the console");
	Cmd_AddCommand("messagemode", Cl_MessageMode_f, "Activate chat");
	Cmd_AddCommand("messagemode2", Cl_MessageMode2_f, "Activate team chat");

	Com_Printf("Console initialized.\n");
}


/*
 * Cl_DrawInput
 *
 * The input line scrolls horizontally if typing goes beyond the right edge
 */
static void Cl_DrawInput(void){
	int i, y;
	char editlinecopy[KEY_LINESIZE], *text;

	text = strcpy(editlinecopy, Cl_EditLine());
	y = strlen(text);

	// add the cursor frame
	if((int)(cls.realtime >> 8) & 1){
		text[key_linepos] = CON_CURSORCHAR;
		if(key_linepos == y)
			y++;
	}

	// fill out remainder with spaces
	for(i = y; i < KEY_LINESIZE; i++)
		text[i] = ' ';

	// prestep if horizontally scrolling
	if(key_linepos >= cl_con.width)
		text += 1 + key_linepos - cl_con.width;

	// draw it
	R_DrawBytes(0, cl_con.height << 5, text, cl_con.width, CON_COLOR_DEFAULT);
}


/*
 * Cl_DrawNotify
 *
 * Draws the last few lines of output transparently over the game top
 */
void Cl_DrawNotify(void){
	int i, y = 0;
	char *s;
	int skip;
	int len;
	int color;

	if(cls.state != ca_active)
		return;

	for(i = cl_con.lastline - CON_NUMTIMES; i < cl_con.lastline; i++ ){
		if(i < 0)
			continue;

		if(cl_con.times[i % CON_NUMTIMES] + con_notifytime->value * 1000 > cls.realtime){
			R_DrawBytes(0, y, cl_con.linestart[i], cl_con.linestart[i + 1] - cl_con.linestart[i], cl_con.linecolor[i]);
			y += 32;
		}
	}

	if(cls.key_dest == key_message){
		if(chat_team){
			color = CON_COLOR_TEAMCHAT;
			R_DrawString(0, y, "say_team", CON_COLOR_DEFAULT);
			skip = 10;
		} else {
			color = CON_COLOR_CHAT;
			R_DrawString(0, y, "say", CON_COLOR_DEFAULT);
			skip = 5;
		}
		R_DrawChar((skip - 2) << 4 , y, ':', color);

		s = chat_buffer;
		// FIXME check the skipped part for color codes
		if(chat_bufferlen > (r_state.width >> 4) - (skip + 1))
			s += chat_bufferlen - ((r_state.width >> 4) - (skip + 1));

		len = R_DrawString(skip << 4, y, s, color);

		if((int)(cls.realtime >> 8) & 1)  // draw the cursor
			R_DrawChar((len + skip) << 4, y, CON_CURSORCHAR, color);
	}
}


/*
 * Cl_DrawConsole
 */
void Cl_DrawConsole(void){
	int line;
	int lines;
	int kb;
	int y;
	char dl[MAX_STRING_CHARS];

	if(cls.key_dest != key_console)
		return;

	Con_Resize(&cl_con, r_state.width >> 4, (r_state.height >> 5) - 1);

	// draw a background
	if(cls.state == ca_active)
		R_DrawFill(0, 0, r_state.width, r_state.height, 5, con_alpha->value);
	else
		R_DrawFill(0, 0, r_state.width, r_state.height, 0, 1.0);

	// draw the text
	lines = cl_con.height;
	y = 0;
	for(line = cl_con.lastline - cl_con.scroll - lines;
			line < cl_con.lastline - cl_con.scroll; line++){

		if(line >= 0 && cl_con.linestart[line][0] != '\0'){
			R_DrawBytes(0, y, cl_con.linestart[line],
					cl_con.linestart[line + 1] - cl_con.linestart[line], cl_con.linecolor[line]);
		}
		y += 32;
	}

	if(cls.loading){  // draw loading progress
		snprintf(dl, sizeof(dl), "Loading... %2d%%", cls.loading);
		R_DrawString(0, cl_con.height << 5, dl, CON_COLOR_INFO);
	}
	else if(cls.download.file){  // draw download progress

		kb = (int)ftell(cls.download.file) / 1024;

		snprintf(dl, sizeof(dl), "%s [%s] %dKB ", cls.download.name,
				(cls.download.http ? "HTTP" : "UDP"), kb);

		R_DrawString(0, cl_con.height << 5, dl, CON_COLOR_INFO);
	}
	else {  // draw the input prompt, user text, and cursor if desired
		Cl_DrawInput();
	}
}
