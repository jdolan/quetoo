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

#include "cl_local.h"

console_t cl_con;

static cvar_t *con_notify_time;
static cvar_t *con_alpha;

/*
 * Cl_ToggleConsole_f
 */
void Cl_ToggleConsole_f(void) {

	Cl_ClearTyping();

	Cl_ClearNotify();

	if (cls.key_state.dest == KEY_CONSOLE) {

		if (cls.state == CL_ACTIVE)
			cls.key_state.dest = KEY_GAME;
		else
			cls.key_state.dest = KEY_UI;
	} else
		cls.key_state.dest = KEY_CONSOLE;
}

/*
 * Cl_UpdateNotify
 *
 * Update client message notification times
 */
void Cl_UpdateNotify(int last_line) {
	int i;

	for (i = last_line; i < cl_con.last_line; i++)
		cl_con.notify_times[i % CON_NUM_NOTIFY] = cls.real_time;
}

/*
 * Cl_ClearNotify
 *
 * Clear client message notification times
 */
void Cl_ClearNotify(void) {
	int i;

	for (i = 0; i < CON_NUM_NOTIFY; i++)
		cl_con.notify_times[i] = 0;
}

/*
 * Cl_MessageMode_f
 */
static void Cl_MessageMode_f(void) {

	memset(&cls.chat_state, 0, sizeof(cl_chat_state_t));

	cls.chat_state.team = false;

	cls.key_state.dest = KEY_CHAT;
}

/*
 * Cl_MessageMode2_f
 */
static void Cl_MessageMode2_f(void) {

	memset(&cls.chat_state, 0, sizeof(cl_chat_state_t));

	cls.chat_state.team = true;

	cls.key_state.dest = KEY_CHAT;
}

/*
 * Cl_InitConsole
 */
void Cl_InitConsole(void) {

	memset(&cl_con, 0, sizeof(console_t));

	cl_con.width = -1;
	// the last line of the console is reserved for input
	Con_Resize(&cl_con, r_context.width >> 4, (r_context.height >> 5) - 1);

	Cl_ClearNotify();

	con_notify_time = Cvar_Get("con_notify_time", "3", CVAR_ARCHIVE,
			"Seconds to draw the last messages on the game top");
	con_alpha = Cvar_Get("con_alpha", "0.3", CVAR_ARCHIVE,
			"Console alpha background [0.0-1.0]");

	Cmd_AddCommand("toggle_console", Cl_ToggleConsole_f, "Toggle the console");

	Cmd_AddCommand("message_mode", Cl_MessageMode_f, "Activate chat");
	Cmd_AddCommand("message_mode_2", Cl_MessageMode2_f, "Activate team chat");

	Com_Print("Console initialized.\n");
}

/*
 * Cl_DrawInput
 *
 * The input line scrolls horizontally if typing goes beyond the right edge
 */
static void Cl_DrawInput(void) {
	char edit_line_copy[KEY_LINESIZE], *text;
	r_pixel_t ch;
	size_t i, y;

	R_BindFont("small", NULL, &ch);

	text = strcpy(edit_line_copy, Cl_EditLine());
	y = strlen(text);

	// add the cursor frame
	if ((unsigned int) (cls.real_time >> 8) & 1) {
		text[cls.key_state.pos] = CON_CURSOR_CHAR;
		if (y == cls.key_state.pos)
			y++;
	}

	// fill out remainder with spaces
	for (i = y; i < KEY_LINESIZE; i++)
		text[i] = ' ';

	// prestep if horizontally scrolling
	if (cls.key_state.pos >= cl_con.width)
		text += 1 + cls.key_state.pos - cl_con.width;

	// draw it
	R_DrawBytes(0, cl_con.height * ch, text, cl_con.width, CON_COLOR_DEFAULT);
}

/*
 * Cl_DrawNotify
 *
 * Draws the last few lines of output transparently over the game top
 */
void Cl_DrawNotify(void) {
	r_pixel_t y, cw, ch;
	int i, color;

	if (cls.state != CL_ACTIVE)
		return;

	R_BindFont("small", &cw, &ch);

	y = 0;

	for(i = cl_con.last_line - CON_NUM_NOTIFY; i < cl_con.last_line; i++) {
		if (i < 0)
			continue;
		if (cl_con.notify_times[i % CON_NUM_NOTIFY] + con_notify_time->value
				* 1000 > cls.real_time) {
			R_DrawBytes(0, y, cl_con.line_start[i],
					cl_con.line_start[i + 1] - cl_con.line_start[i],
					cl_con.line_color[i]);
			y += ch;
		}
	}

	if (cls.key_state.dest == KEY_CHAT) {
		unsigned short skip;
		size_t len;
		char *s;

		if (cls.chat_state.team) {
			color = CON_COLOR_TEAMCHAT;
			R_DrawString(0, y, "say_team", CON_COLOR_DEFAULT);
			skip = 10;
		} else {
			color = CON_COLOR_CHAT;
			R_DrawString(0, y, "say", CON_COLOR_DEFAULT);
			skip = 5;
		}

		R_DrawChar((skip - 2) * cw, y, ':', color);

		s = cls.chat_state.buffer;
		len = R_DrawString(skip * cw, y, s, color);

		if ((unsigned int) (cls.real_time >> 8) & 1) // draw the cursor
			R_DrawChar((len + skip) * cw, y, CON_CURSOR_CHAR, color);
	}

	R_BindFont(NULL, NULL, NULL);
}

/*
 * Cl_DrawConsole
 */
void Cl_DrawConsole(void) {
	int line;
	int lines;
	int kb;
	r_pixel_t y, cw, ch;
	char dl[MAX_STRING_CHARS];

	if (cls.key_state.dest != KEY_CONSOLE)
		return;

	R_BindFont("small", &cw, &ch);

	Con_Resize(&cl_con, r_context.width / cw, (r_context.height / ch) - 1);

	// draw a background
	if (cls.state == CL_ACTIVE)
		R_DrawFill(0, 0, r_context.width, r_context.height, 5, con_alpha->value);
	else
		R_DrawFill(0, 0, r_context.width, r_context.height, 0, 1.0);

	// draw the text
	lines = cl_con.height;
	y = 0;
	for (line = cl_con.last_line - cl_con.scroll - lines; line
			< cl_con.last_line - cl_con.scroll; line++) {

		if (line >= 0 && cl_con.line_start[line][0] != '\0') {
			R_DrawBytes(0, y, cl_con.line_start[line],
					cl_con.line_start[line + 1] - cl_con.line_start[line],
					cl_con.line_color[line]);
		}
		y += ch;
	}

	// draw the loading string or the input prompt

	if (cls.state >= CL_CONNECTED && cls.loading) { // draw loading progress
		snprintf(dl, sizeof(dl), "Loading... %2d%%", cls.loading);

		R_DrawString(0, cl_con.height * ch, dl, CON_COLOR_INFO);
	} else if (cls.download.file) { // draw download progress

		kb = (int) ftell(cls.download.file) / 1024;

		snprintf(dl, sizeof(dl), "%s [%s] %dKB ", cls.download.name,
				(cls.download.http ? "HTTP" : "UDP"), kb);

		R_DrawString(0, cl_con.height * ch, dl, CON_COLOR_INFO);
	} else { // draw the input prompt, user text, and cursor if desired
		Cl_DrawInput();
	}

	R_BindFont(NULL, NULL, NULL);
}
