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

/*
 * cl_console.c
 * Drawing functions for the client console
 */

#include "cl_local.h"

console_t cl_console;

static cvar_t *cl_console_alpha;
static cvar_t *cl_console_notify_lines;
static cvar_t *cl_console_notify_time;

/**
 * @brief Draws the last few lines of output transparently over the game top
 */
static void Cl_DrawConsole_Notify(void) {
	r_pixel_t cw, ch;

	R_BindFont("small", &cw, &ch);

	cl_console.scroll = 0;

	cl_console.width = r_context.width / cw;

	const size_t max_lines = Clamp(cl_console_notify_lines->integer, 1, 12);
	const char *lines[max_lines];

	const size_t count = Con_Tail(&cl_console, lines, max_lines);

	r_pixel_t y = 0;

	for (size_t i = count; i > 0; i--) {
		R_DrawString(0, y, lines[i - 1], CON_COLOR_DEFAULT);
		y += ch;
	}

	if (cls.key_state.dest == KEY_CHAT) {
		uint16_t skip;

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

		const size_t len = R_DrawString(skip * cw, y, cls.chat_state.buffer, color);

		if ((uint32_t) (cls.real_time >> 8) & 1) // draw the cursor
			R_DrawChar((len + skip) * cw, y, CON_CURSOR_CHAR, color);
	}

	R_BindFont(NULL, NULL, NULL);
}

/*
 * @brief The input line scrolls horizontally if typing goes beyond the right edge
 */
static void Cl_DrawConsole_Input(void) {
	char edit_line_copy[KEY_LINE_SIZE], *text;
	r_pixel_t ch;

	R_BindFont("small", NULL, &ch);

	text = strcpy(edit_line_copy, Cl_EditLine());
	size_t j = strlen(text);

	// add the cursor frame
	if ((uint32_t) (cls.real_time >> 8) & 1) {
		text[cls.key_state.pos] = CON_CURSOR_CHAR;
		if (j == cls.key_state.pos)
			j++;
	}

	// fill out remainder with spaces
	for (size_t i = j; i < KEY_LINE_SIZE; i++)
		text[i] = ' ';

	// horizontal scrolling
	if (cls.key_state.pos >= cl_console.width)
		text += 1 + cls.key_state.pos - cl_console.width;

	// draw it
	R_DrawBytes(0, cl_console.height * ch, text, cl_console.width, CON_COLOR_DEFAULT);
}

/**
 * @brief Draws the console background.
 */
static void Cl_DrawConsole_Background(void) {

	const int32_t black = 0;
	const int32_t white = 5;

	if (cls.key_state.dest == KEY_CONSOLE) {
		if (cls.state == CL_ACTIVE) {
			R_DrawFill(0, 0, r_context.width, r_context.height * 0.5, white, cl_console_alpha->value);
		} else {
			R_DrawFill(0, 0, r_context.width, r_context.height, black, 1.0);
		}
	}
}

/**
 * @brief
 */
static void Cl_DrawConsole_Buffer(void) {
	r_pixel_t cw, ch;

	R_BindFont("small", &cw, &ch);

	cl_console.width = r_context.width / cw;

	if (cls.state == CL_ACTIVE) {
		cl_console.height = (r_context.height / 2 / ch) - 1;
	} else {
		cl_console.height = (r_context.height / ch) - 1;
	}

	char *lines[cl_console.height];
	const size_t count = Con_Tail(&cl_console, lines, cl_console.height);

	r_pixel_t y = 0;

	for (size_t i = 0; i < count; i++) {
		R_DrawString(0, y, lines[i], CON_COLOR_DEFAULT);
		y += ch;
	}
}

/*
 * @brief
 */
void Cl_DrawConsole(void) {

	Cl_DrawConsole_Background();

	if (cls.key_state.dest == KEY_CONSOLE) {
		Cl_DrawConsole_Buffer();
	} else if (cls.state == CL_ACTIVE) {
		Cl_DrawConsole_Notify();
	}


	// draw the loading string or the input prompt

	else if (cls.download.file) { // draw download progress
		char dl[MAX_PRINT_MSG];

		const int32_t kb = (int32_t) Fs_Tell(cls.download.file) / 1024;

		g_snprintf(dl, sizeof(dl), "%s [%s] %dKB ", cls.download.name,
				(cls.download.http ? "HTTP" : "UDP"), kb);

		R_DrawString(0, cl_console.height * ch, dl, CON_COLOR_INFO);
	} else { // draw the input prompt, user text, and cursor if desired
		Cl_DrawConsole_Input();
	}

	R_BindFont(NULL, NULL, NULL);
}

/*
 * @brief
 */
static void Cl_Print(const console_string_t *str) {

	puts(str->chars);
}

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
 * @brief Initializes the client console.
 */
void Cl_InitConsole(void) {

	memset(&cl_console, 0, sizeof(console_t));

	cl_console.Print = Cl_Print;

	Con_AddConsole(&cl_console);

	cl_console_alpha = Cvar_Get("cl_console_alpha", "0.3", CVAR_ARCHIVE, NULL);
	cl_console_notify_lines = Cvar_Get("cl_console_notify_lines", "3", CVAR_ARCHIVE, NULL);
	cl_console_notify_time = Cvar_Get("cl_console_notify_time", "3.0", CVAR_ARCHIVE, NULL);

	Cmd_Add("toggle_console", Cl_ToggleConsole_f, CMD_SYSTEM | CMD_CLIENT, "Toggle the console");
	Cmd_Add("message_mode", Cl_MessageMode_f, CMD_CLIENT, "Activate chat");
	Cmd_Add("message_mode_2", Cl_MessageMode2_f, CMD_CLIENT, "Activate team chat");

	Com_Print("Client console initialized\n");
}

/*
 * @brief Shuts down the client console.
 */
void Cl_ShutdownConsole(void) {

	Con_RemoveConsole(&cl_console);

	Cmd_Remove("toggle_console");
	Cmd_Remove("message_mode");
	Cmd_Remove("message_mode_2");

	Com_Print("Client console shutdown\n");
}
