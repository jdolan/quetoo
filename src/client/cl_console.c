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

static cvar_t *cl_console_background_alpha;

static cvar_t *cl_draw_chat;
static cvar_t *cl_draw_notify;

static cvar_t *cl_chat_lines;
static cvar_t *cl_chat_time;

static cvar_t *cl_notify_lines;
static cvar_t *cl_notify_time;

/**
 * @brief
 */
static void Cl_DrawConsole_Background(void) {

	const r_image_t *image = R_LoadImage("ui/conback", IT_UI);
	if (image->type != IT_NULL) {

		const vec_t x_scale = r_context.width / (vec_t) image->width;
		const vec_t y_scale = r_context.height / (vec_t) image->height;

		const vec_t scale = MAX(x_scale, y_scale);

		if (cls.state == CL_ACTIVE) {
			R_Color((const vec4_t) { 1.0, 1.0, 1.0, cl_console_background_alpha->value });

			R_DrawImage(0, -r_context.window_height * 0.333, scale, image);

			R_Color(NULL);
		} else {
			R_DrawImage(0, 0, scale, image);
		}
	}
}

/**
 * @brief
 */
static void Cl_DrawConsole_Buffer(void) {
	r_pixel_t cw, ch, height;

	R_BindFont("small", &cw, &ch);

	if (cls.state == CL_ACTIVE) {
		height = r_context.height * 0.666;
	} else {
		height = r_context.height;
	}

	cl_console.width = r_context.width / cw;
	cl_console.height = (height / ch) - 1;

	char *lines[cl_console.height];
	const size_t count = Con_Tail(&cl_console, lines, cl_console.height);

	r_pixel_t y = (cl_console.height - count) * ch;

	int32_t color = CON_COLOR_DEFAULT;
	for (size_t i = 0; i < count; i++) {
		R_DrawString(0, y, lines[i], color);
		color = StrrColor(lines[i]);
		g_free(lines[i]);
		y += ch;
	}
}

/**
 * @brief The input line scrolls horizontally if typing goes beyond the right edge
 */
static void Cl_DrawConsole_Input(void) {
	r_pixel_t cw, ch;

	R_BindFont("small", &cw, &ch);

	r_pixel_t x = 1, y = cl_console.height * ch;

	// draw the prompt
	R_DrawChar(0, y, ']', CON_COLOR_ALT);

	// and the input buffer, scrolling horizontally if appropriate
	const char *s = cl_console.input.buffer;
	if (cl_console.input.pos > (size_t) cl_console.width - 2) {
		s += 2 + cl_console.input.pos - cl_console.width;
	}

	while (*s) {
		R_DrawChar(x * cw, y, *s, CON_COLOR_DEFAULT);

		s++;
		x++;
	}

	// and lastly cursor
	R_DrawChar((cl_console.input.pos + 1) * cw, y, 0x0b, CON_COLOR_DEFAULT);
}

/**
 * @brief
 */
void Cl_DrawConsole(void) {

	Cl_DrawConsole_Background();

	Cl_DrawConsole_Buffer();

	Cl_DrawConsole_Input();

	R_BindFont(NULL, NULL, NULL);
}

/**
 * @brief Draws the last few lines of output transparently over the game top
 */
void Cl_DrawNotify(void) {
	r_pixel_t cw, ch;

	if (!cl_draw_notify->value) {
		return;
	}

	R_BindFont("small", &cw, &ch);

	console_t con = {
		.width = r_context.width / cw,
		.height = Clamp(cl_notify_lines->integer, 1, 12),
		.level = (PRINT_MEDIUM | PRINT_HIGH),
	};

	if (cl.systime > cl_notify_time->value * 1000) {
		con.whence = cl.systime - cl_notify_time->value * 1000;
	}

	char *lines[con.height];
	const size_t count = Con_Tail(&con, lines, con.height);

	r_pixel_t y = 0;

	int32_t color = CON_COLOR_DEFAULT;
	for (size_t i = 0; i < count; i++) {
		R_DrawString(0, y, lines[i], color);
		color = StrrColor(lines[i]);
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

	r_pixel_t x = 1, y = r_view.viewport.y + r_view.viewport.h * 0.66;

	cl_chat_console.width = r_context.width / cw / 3;
	cl_chat_console.height = Clamp(cl_chat_lines->integer, 0, 8);

	if (cl_draw_chat->value && cl_chat_console.height) {

		if (cl.systime > cl_chat_time->value * 1000) {
			cl_chat_console.whence = cl.systime - cl_chat_time->value * 1000;
		}

		char *lines[cl_chat_console.height];
		const size_t count = Con_Tail(&cl_chat_console, lines, cl_chat_console.height);

		int32_t color = CON_COLOR_DEFAULT;
		for (size_t i = 0; i < count; i++) {
			R_DrawString(0, y, lines[i], color);
			color = StrrColor(lines[i]);
			g_free(lines[i]);
			y += ch;
		}
	}

	if (cls.key_state.dest == KEY_CHAT) {

		const int32_t color = cls.chat_state.team_chat ? CON_COLOR_TEAMCHAT : CON_COLOR_CHAT;

		// draw the prompt
		R_DrawChar(0, y, ']', color);

		// and the input, scrolling horizontally if appropriate
		const char *s = cl_chat_console.input.buffer;
		if (cl_chat_console.input.pos > (size_t) cl_chat_console.width - 2) {
			s += 2 + cl_chat_console.input.pos - cl_chat_console.width;
		}

		while (*s) {
			R_DrawChar(x * cw, y, *s, CON_COLOR_DEFAULT);

			s++;
			x++;
		}

		// and lastly cursor
		R_DrawChar(x * cw, y, 0x0b, CON_COLOR_DEFAULT);
	}
}

/**
 * @brief
 */
static void Cl_Print(const console_string_t *str) {
	char stripped[strlen(str->chars) + 1];

	StripColors(str->chars, stripped);
	fputs(stripped, stdout);
}

/**
 * @brief
 */
void Cl_ToggleConsole_f(void) {

	if (cls.state == CL_LOADING)
		return;

	if (cls.key_state.dest == KEY_CONSOLE) {
		if (cls.state == CL_ACTIVE) {
			Cl_SetKeyDest(KEY_GAME);
		} else {
			Cl_SetKeyDest(KEY_UI);
		}
	} else {
		Cl_SetKeyDest(KEY_CONSOLE);
	}

	memset(&cl_console.input, 0, sizeof(cl_console.input));
}

static void Cl_MessageMode(_Bool team) {

	console_input_t *in = &cl_chat_console.input;
	memset(in, 0, sizeof(*in));

	cls.chat_state.team_chat = team;

	Cl_SetKeyDest(KEY_CHAT);
}

/**
 * @brief
 */
static void Cl_MessageMode_f(void) {

	Cl_MessageMode(false);
}

/**
 * @brief
 */
static void Cl_MessageMode2_f(void) {

	Cl_MessageMode(true);
}

/**
 * @brief Initializes the client console.
 */
void Cl_InitConsole(void) {

	memset(&cl_console, 0, sizeof(cl_console));

	cl_console.echo = true;

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

	cl_console_background_alpha = Cvar_Get("cl_console_background_alpha", "0.9", CVAR_ARCHIVE, NULL);

	cl_draw_chat = Cvar_Get("cl_draw_chat", "1", 0, "Draw recent chat messages");
	cl_draw_notify = Cvar_Get("cl_draw_notify", "1", 0, "Draw recent console activity");

	cl_notify_lines = Cvar_Get("cl_console_notify_lines", "3", CVAR_ARCHIVE, NULL);
	cl_notify_time = Cvar_Get("cl_notify_time", "3.0", CVAR_ARCHIVE, NULL);

	cl_chat_lines = Cvar_Get("cl_chat_lines", "3", CVAR_ARCHIVE, NULL);
	cl_chat_time = Cvar_Get("cl_chat_time", "10.0", CVAR_ARCHIVE, NULL);

	Cmd_Add("cl_toggle_console", Cl_ToggleConsole_f, CMD_SYSTEM | CMD_CLIENT, "Toggle the console");
	Cmd_Add("cl_message_mode", Cl_MessageMode_f, CMD_CLIENT, "Activate chat");
	Cmd_Add("cl_message_mode_2", Cl_MessageMode2_f, CMD_CLIENT, "Activate team chat");

	Com_Print("Client console initialized\n");
}

/**
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

	Cmd_Remove("cl_toggle_console");
	Cmd_Remove("cl_message_mode");
	Cmd_Remove("cl_message_mode_2");

	Com_Print("Client console shutdown\n");
}
