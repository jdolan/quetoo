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

console_t cl_notify_console;

cvar_t *cl_console_height;
cvar_t *cl_draw_console_background_alpha;

cvar_t *cl_draw_chat;
cvar_t *cl_draw_notify;

cvar_t *cl_chat_lines;
cvar_t *cl_chat_time;

cvar_t *cl_notify_lines;
cvar_t *cl_notify_time;

/**
 * @brief Outputs a stripped (color-code-free) console string to stdout.
 */
static void Cl_Print(const console_string_t *str) {
  char stripped[q_strlen(str->chars) + 1];

  q_strcolorstrip(str->chars, stripped);
  fputs(stripped, stdout);
}

/**
 * @brief Handles the `toggleconsole` command, toggling the console open or closed.
 */
void Cl_ToggleConsole_f(void) {

  if (cls.state == CL_LOADING) {
    return;
  }

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

/**
 * @brief Switches key destination to chat mode, optionally for team chat.
 */
static void Cl_MessageMode(bool team_chat) {

  console_input_t *in = &cl_chat_console.input;
  memset(in, 0, sizeof(*in));

  cls.chat_state.team_chat = team_chat;

  Cl_SetKeyDest(KEY_CHAT);
}

/**
 * @brief Handles the `messagemode` console command, opening the global chat input.
 */
static void Cl_MessageMode_f(void) {

  Cl_MessageMode(false);
}

/**
 * @brief Handles the `messagemode2` console command, opening the team chat input.
 */
static void Cl_MessageMode2_f(void) {

  Cl_MessageMode(true);
}

/**
 * @brief Generate a backtrace.
 */
static void Cl_Backtrace_f(void) {
  char *backtrace = Sys_Backtrace(0, UINT32_MAX);
  Com_Print("%s\n", backtrace);
  free(backtrace);
}

/**
 * @brief Handles the `error` console command, triggering a `Com_Error` with the given type.
 */
__attribute__((noreturn))
static void Cl_Error_f(void) {
  err_t err = ERROR_DROP;

  if (Cmd_Argc() > 1) {
    err = (err_t) strtoul(Cmd_Argv(1), NULL, 10);
  }

  Com_Error(err, __func__);
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
    Com_Debug(DEBUG_CLIENT, "Couldn't read history");
  }

  memset(&cl_chat_console, 0, sizeof(cl_chat_console));
  cl_chat_console.level = PRINT_CHAT | PRINT_TEAM_CHAT;

  cl_console_height = Cvar_Add("cl_console_height", "0.4", CVAR_ARCHIVE, "Console height, as a multiplier of the screen height. Default is 0.4.");
  cl_draw_console_background_alpha = Cvar_Add("cl_draw_console_background_alpha", "0.8", CVAR_ARCHIVE, NULL);

  cl_draw_chat = Cvar_Add("cl_draw_chat", "1", 0, "Draw recent chat messages");
  cl_draw_notify = Cvar_Add("cl_draw_notify", "1", 0, "Draw recent console activity");

  cl_notify_lines = Cvar_Add("cl_console_notify_lines", "3", CVAR_ARCHIVE, "How many lines to show in the notify console.");
  cl_notify_time = Cvar_Add("cl_notify_time", "3.0", CVAR_ARCHIVE, "How long notify messages stay on-screen.");

  cl_chat_lines = Cvar_Add("cl_chat_lines", "4", CVAR_ARCHIVE, "How many chat lines to show");
  cl_chat_time = Cvar_Add("cl_chat_time", "10.0", CVAR_ARCHIVE, "How long chat messages last");

  Cmd_Add("cl_toggle_console", Cl_ToggleConsole_f, CMD_SYSTEM | CMD_CLIENT, "Toggle the console");
  Cmd_Add("cl_message_mode", Cl_MessageMode_f, CMD_CLIENT, "Activate chat");
  Cmd_Add("cl_message_mode_2", Cl_MessageMode2_f, CMD_CLIENT, "Activate team chat");

  Cmd_Add("cl_backtrace", Cl_Backtrace_f, CMD_SYSTEM, "Generate a backtrace");
  Cmd_Add("cl_error", Cl_Error_f, CMD_SYSTEM, "Generate an error");

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

  Cmd_Remove("crash");
  Cmd_Remove("fatal");

  Com_Print("Client console shutdown\n");
}
