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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "cl_local.h"

Console clConsole;

Cvar *cl_consoleHeight;
Cvar *cl_drawConsoleBackgroundAlpha;

/**
 * @brief Outputs a stripped (color-code-free) console string to stdout.
 */
static void Cl_Print(const ConsoleString *str) {
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

  if (cls.keyState.dest == KEY_CONSOLE) {
    if (cls.state == CL_ACTIVE) {
      Cl_SetKeyDest(KEY_GAME);
    } else {
      Cl_SetKeyDest(KEY_UI);
    }
  } else {
    Cl_SetKeyDest(KEY_CONSOLE);
  }

  memset(&clConsole.input, 0, sizeof(clConsole.input));
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
  Err err = ERROR_DROP;

  if (Cmd_Argc() > 1) {
    err = (Err) strtoul(Cmd_Argv(1), NULL, 10);
  }

  Com_Error(err, __func__);
}

/**
 * @brief Initializes the client console.
 */
void Cl_InitConsole(void) {

  memset(&clConsole, 0, sizeof(clConsole));

  clConsole.echo = true;

  clConsole.Append = Cl_Print;

  Con_AddConsole(&clConsole);

  File *file = Fs_OpenRead("history");
  if (file) {
    Con_ReadHistory(&clConsole, file);
    Fs_Close(file);
  } else {
    Com_Debug(DEBUG_CLIENT, "Couldn't read history");
  }

  cl_consoleHeight = Cvar_Add("cl_consoleHeight", "0.4", CVAR_ARCHIVE, "Console height, as a multiplier of the screen height. Default is 0.4.");
  cl_drawConsoleBackgroundAlpha = Cvar_Add("cl_drawConsoleBackgroundAlpha", "0.8", CVAR_ARCHIVE, "The opacity of the console background, from 0 to 1.");

  Cmd_Add("cl_toggleConsole", Cl_ToggleConsole_f, CMD_SYSTEM | CMD_CLIENT, "Toggle the console");

  Cmd_Add("cl_backtrace", Cl_Backtrace_f, CMD_SYSTEM, "Generate a backtrace");
  Cmd_Add("cl_error", Cl_Error_f, CMD_SYSTEM, "Generate an error");

  Com_Print("Client console initialized\n");
}

/**
 * @brief Shuts down the client console.
 */
void Cl_ShutdownConsole(void) {

  Con_RemoveConsole(&clConsole);

  File *file = Fs_OpenWrite("history");
  if (file) {
    Con_WriteHistory(&clConsole, file);
    Fs_Close(file);
  } else {
    Com_Warn("Couldn't write history\n");
  }

  Cmd_Remove("cl_toggleConsole");

  Cmd_Remove("crash");
  Cmd_Remove("fatal");

  Com_Print("Client console shutdown\n");
}
