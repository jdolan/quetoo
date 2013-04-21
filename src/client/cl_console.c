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

console_t cl_console;

static cvar_t *con_notify_time;
static cvar_t *con_alpha;

/*
 * @brief
 */
void Cl_ToggleConsole_f(void) {

	if (cls.state == CL_CONNECTING || cls.state == CL_CONNECTED) {
		// wait until we've loaded
		return;
	}

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
 * @brief Update client message notification times.
 */
void Cl_UpdateNotify(int32_t last_line) {
	int32_t i;

	for (i = last_line; i < cl_console.last_line; i++)
		cl_console.notify_times[i % CON_NUM_NOTIFY] = cls.real_time;
}

/*
 * @brief Clear client message notification times.
 */
void Cl_ClearNotify(void) {
	int32_t i;

	for (i = 0; i < CON_NUM_NOTIFY; i++)
		cl_console.notify_times[i] = 0;
}

/*
 * @brief
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
 * @brief
 */
void Cl_InitConsole(void) {

	memset(&cl_console, 0, sizeof(console_t));

	// the last line of the console is reserved for input
	Con_Resize(&cl_console, r_context.width >> 4, (r_context.height >> 5) - 1);

	Cl_ClearNotify();

	con_notify_time = Cvar_Get("con_notify_time", "3", CVAR_ARCHIVE,
			"Seconds to draw the last messages on the game top");
	con_alpha = Cvar_Get("con_alpha", "0.3", CVAR_ARCHIVE, "Console alpha background [0.0-1.0]");

	Cmd_AddCommand("toggle_console", Cl_ToggleConsole_f, CMD_SYSTEM, "Toggle the console");

	Cmd_AddCommand("message_mode", Cl_MessageMode_f, 0, "Activate chat");
	Cmd_AddCommand("message_mode_2", Cl_MessageMode2_f, 0, "Activate team chat");

	Com_Print("Console initialized.\n");
}

/*
 * @brief The input line scrolls horizontally if typing goes beyond the right edge
 */
static void Cl_DrawInput(void) {
	char edit_line_copy[KEY_LINE_SIZE], *text;
	r_pixel_t ch;
	size_t i, y;

	R_BindFont("small", NULL, &ch);

	text = strcpy(edit_line_copy, Cl_EditLine());
	y = strlen(text);

	// add the cursor frame
	if ((uint32_t) (cls.real_time >> 8) & 1) {
		text[cls.key_state.pos] = CON_CURSOR_CHAR;
		if (y == cls.key_state.pos)
			y++;
	}

	// fill out remainder with spaces
	for (i = y; i < KEY_LINE_SIZE; i++)
		text[i] = ' ';

	// prestep if horizontally scrolling
	if (cls.key_state.pos >= cl_console.width)
		text += 1 + cls.key_state.pos - cl_console.width;

	// draw it
	R_DrawBytes(0, cl_console.height * ch, text, cl_console.width, CON_COLOR_DEFAULT);
}

/*
 * @brief Draws the last few lines of output transparently over the game top
 */
void Cl_DrawNotify(void) {
	r_pixel_t y, cw, ch;
	int32_t i, color;

	if (cls.state != CL_ACTIVE)
		return;

	R_BindFont("small", &cw, &ch);

	y = 0;

	for (i = cl_console.last_line - CON_NUM_NOTIFY; i < cl_console.last_line; i++) {
		if (i < 0)
			continue;
		if (cl_console.notify_times[i % CON_NUM_NOTIFY] + con_notify_time->value * 1000
				> cls.real_time) {
			R_DrawBytes(0, y, cl_console.line_start[i],
					cl_console.line_start[i + 1] - cl_console.line_start[i],
					cl_console.line_color[i]);
			y += ch;
		}
	}

	if (cls.key_state.dest == KEY_CHAT) {
		uint16_t skip;
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

		if ((uint32_t) (cls.real_time >> 8) & 1) // draw the cursor
			R_DrawChar((len + skip) * cw, y, CON_CURSOR_CHAR, color);
	}

	R_BindFont(NULL, NULL, NULL);
}

/*
 * @brief
 */
void Cl_DrawConsole(void) {
	int32_t line;
	int32_t lines;
	int32_t kb;
	r_pixel_t y, cw, ch;
	char dl[MAX_STRING_CHARS];

	if (cls.key_state.dest != KEY_CONSOLE)
		return;

	R_BindFont("small", &cw, &ch);

	Con_Resize(&cl_console, r_context.width / cw, (r_context.height / ch) - 1);

	// draw a background
	if (cls.state == CL_ACTIVE)
		R_DrawFill(0, 0, r_context.width, r_context.height, 5, con_alpha->value);
	else
		R_DrawFill(0, 0, r_context.width, r_context.height, 0, 1.0);

	// draw the text
	lines = cl_console.height;
	y = 0;
	for (line = cl_console.last_line - cl_console.scroll - lines; line < cl_console.last_line
			- cl_console.scroll; line++) {

		if (line >= 0 && cl_console.line_start[line][0] != '\0') {
			R_DrawBytes(0, y, cl_console.line_start[line],
					cl_console.line_start[line + 1] - cl_console.line_start[line],
					cl_console.line_color[line]);
		}
		y += ch;
	}

	// draw the loading string or the input prompt

	if (cls.state >= CL_CONNECTED && cls.loading) { // draw loading progress
		g_snprintf(dl, sizeof(dl), "Loading... %2d%%", cls.loading);

		R_DrawString(0, cl_console.height * ch, dl, CON_COLOR_INFO);
	} else if (cls.download.file) { // draw download progress

		kb = (int) Fs_Tell(cls.download.file) / 1024;

		g_snprintf(dl, sizeof(dl), "%s [%s] %dKB ", cls.download.name,
				(cls.download.http ? "HTTP" : "UDP"), kb);

		R_DrawString(0, cl_console.height * ch, dl, CON_COLOR_INFO);
	} else { // draw the input prompt, user text, and cursor if desired
		Cl_DrawInput();
	}

	R_BindFont(NULL, NULL, NULL);
}
