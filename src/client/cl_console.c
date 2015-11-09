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

#include "cl_local.h"

console_t cl_console;
console_t cl_chat_console;

static cvar_t *cl_notify_lines;
static cvar_t *cl_notify_time;

static cvar_t *cl_chat_lines;
static cvar_t *cl_chat_time;

/**
 * @brief
 */
static void Cl_DrawConsole_Buffer(void) {
	r_pixel_t cw, ch, height;

	R_BindFont("small", &cw, &ch);

	if (cls.state == CL_ACTIVE) {
		height = r_context.height * 0.5;
	} else {
		height = r_context.height;
	}

	R_DrawFill(0, 0, r_context.width, height, 0, 1.0);

	cl_console.width = r_context.width / cw;
	cl_console.height = (height / ch) - 1;

	char *lines[cl_console.height];
	const size_t count = Con_Tail(&cl_console, lines, cl_console.height);

	r_pixel_t y = 0;

	for (size_t i = 0; i < count; i++) {
		R_DrawString(0, y, lines[i], CON_COLOR_DEFAULT);
		g_free(lines[i]);
		y += ch;
	}
}

/*
 * @brief The input line scrolls horizontally if typing goes beyond the right edge
 */
static void Cl_DrawConsole_Input(void) {
	char input[MAX_PRINT_MSG];
	r_pixel_t ch;

	R_BindFont("small", NULL, &ch);

	g_snprintf(input, sizeof(input), "] %s", cl_console.input.buffer);
	size_t j = strlen(input);

	// add the cursor
	if ((uint32_t) (cls.real_time >> 8) & 1) {
		input[cl_console.input.pos + 2] = CON_CURSOR_CHAR;
		if (j == cl_console.input.pos + 2)
			j++;
	}

	// fill out remainder with spaces
	for (size_t i = j; i < sizeof(input); i++)
		input[i] = ' ';

	input[sizeof(input) - 1] = '\0';

	// horizontal scrolling
	const char *text = input;

	if (cl_console.input.pos >= cl_console.width)
		text += 1 + cl_console.input.pos - cl_console.width;

	// draw it
	R_DrawBytes(0, cl_console.height * ch, text, cl_console.width, CON_COLOR_DEFAULT);
}

/*
 * @brief
 */
void Cl_DrawConsole(void) {

	Cl_DrawConsole_Buffer();

	Cl_DrawConsole_Input();

	R_BindFont(NULL, NULL, NULL);
}

/**
 * @brief Draws the last few lines of output transparently over the game top
 */
void Cl_DrawNotify(void) {
	r_pixel_t cw, ch;

	R_BindFont("small", &cw, &ch);

	console_t con = {
		.width = r_context.width / cw,
		.height = Clamp(cl_notify_lines->integer, 1, 12),
		.whence = cls.real_time - cl_notify_time->value * 1000
	};

	char *lines[con.height];
	const size_t count = Con_Tail(&con, lines, con.height);

	r_pixel_t y = 0;

	for (size_t i = 0; i < count; i++) {
		R_DrawString(0, y, lines[i], CON_COLOR_DEFAULT);
		g_free(lines[i]);
		y += ch;
	}

	R_BindFont(NULL, NULL, NULL);
}

/**
 * @brief Draws the chat history and, optionally, the chat input string.
 */
void Cl_DrawChat(void) {
	r_pixel_t cw, ch;

	R_BindFont("small", &cw, &ch);

	r_pixel_t y = r_view.y + r_view.height * 0.66;

	cl_chat_console.width = r_context.width / cw / 3;
	cl_chat_console.height = Clamp(cl_chat_lines->integer, 0, 8);

	if (cl_chat_console.height) {

		char *lines[cl_chat_console.height];
		const size_t count = Con_Tail(&cl_chat_console, lines, cl_chat_console.height);

		for (size_t i = 0; i < count; i++) {
			R_DrawString(0, y, lines[i], CON_COLOR_DEFAULT);
			g_free(lines[i]);
			y += ch;
		}
	}

	if (cls.key_state.dest == KEY_CHAT) {
		const console_input_t *in = &cl_chat_console.input;

		const int32_t color = cls.chat_state.team_chat ? CON_COLOR_TEAMCHAT : CON_COLOR_CHAT;

		R_DrawChar(0, y, ']', color);

		const size_t len = R_DrawString(2 * cw, y, in->buffer, color);

		if ((uint32_t) (cls.real_time >> 8) & 1) // draw the cursor
			R_DrawChar((2 + in->pos) * cw, y, CON_CURSOR_CHAR, color);
	}
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

	if (cls.state != CL_ACTIVE)
		return;

	if (cls.key_state.dest == KEY_CONSOLE) {

		if (cls.state == CL_ACTIVE)
			cls.key_state.dest = KEY_GAME;
		else
			cls.key_state.dest = KEY_UI;
	} else
		cls.key_state.dest = KEY_CONSOLE;
}

static void Cl_MessageMode(_Bool team) {

	console_input_t *in = &cl_chat_console.input;

	memset(in, 0, sizeof(*in));

	g_strlcpy(in->buffer, team ? "say_team " : "say ", sizeof(in->buffer));

	cls.chat_state.team_chat = team;

	cls.key_state.dest = KEY_CHAT;
}

/*
 * @brief
 */
static void Cl_MessageMode_f(void) {

	Cl_MessageMode(false);
}

/*
 * Cl_MessageMode2_f
 */
static void Cl_MessageMode2_f(void) {

	Cl_MessageMode(true);
}

/*
 * @brief Initializes the client console.
 */
void Cl_InitConsole(void) {

	memset(&cl_console, 0, sizeof(cl_console));

	cl_console.Append = Cl_Print;

	Con_AddConsole(&cl_console);

	file_t *file = Fs_OpenRead("history");
	if (file) {
		Con_ReadHistory(&cl_console, file);
		Fs_Close(file);
	} else {
		Com_Debug("Couldn't read history");
	}

	memset(&cl_chat_console, 0, sizeof(cl_chat_console));
	cl_chat_console.level = PRINT_CHAT | PRINT_TEAM_CHAT;

	cl_notify_lines = Cvar_Get("cl_console_notify_lines", "3", CVAR_ARCHIVE, NULL);
	cl_notify_time = Cvar_Get("cl_notify_time", "3.0", CVAR_ARCHIVE, NULL);

	cl_notify_lines = Cvar_Get("cl_console_notify_lines", "3", CVAR_ARCHIVE, NULL);
	cl_notify_time = Cvar_Get("cl_notify_time", "3.0", CVAR_ARCHIVE, NULL);

	cl_chat_lines = Cvar_Get("cl_chat_lines", "3", CVAR_ARCHIVE, NULL);
	cl_chat_time = Cvar_Get("cl_chat_time", "10.0", CVAR_ARCHIVE, NULL);

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

	file_t *file = Fs_OpenWrite("history");
	if (file) {
		Con_WriteHistory(&cl_console, file);
		Fs_Close(file);
	} else {
		Com_Warn("Couldn't write history\n");
	}

	Cmd_Remove("toggle_console");
	Cmd_Remove("message_mode");
	Cmd_Remove("message_mode_2");

	Com_Print("Client console shutdown\n");
}
