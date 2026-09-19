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

#pragma once

#include <Objectively/List.h>
#include <SDL3/SDL_mutex.h>

#include "cmd.h"
#include "cvar.h"

#define CON_CURSOR_CHAR 0x0b

/**
 * @brief A console string.
 */
typedef struct {

  /**
   * @brief The print level (e.g. `PRINT_HIGH`).
   */
  uint32_t level;

  /**
   * @brief The null-terminated C string.
   */
  char *chars;

  /**
   * @brief The size of this string in bytes.
   */
  size_t size;

  /**
   * @brief The length of this string in printable characters.
   */
  size_t length;

  /**
   * @brief The timestamp.
   */
  uint32_t timestamp;

} ConsoleString;

/**
 * @brief The maximum number of characters to buffer.
 */
#define CON_MAX_SIZE (1024 * 1024)

/**
 * @brief The console state.
 */
typedef struct {

  /**
   * @brief The console strings.
   */
  List *strings;

  /**
   * @brief The length of the console strings in characters.
   */
  size_t size;

  /**
   * @brief The configured consoles.
   */
  List *consoles;

  /**
   * @brief A lock for coordinating thread access to the console.
   */
  SDL_Mutex *lock;

} ConsoleState;

extern ConsoleState consoleState;

/**
 * @brief The maximum number of lines to buffer for console input history.
 */
#define CON_HISTORY_SIZE 64

typedef enum {
  CON_HISTORY_PREV = 1,
  CON_HISTORY_NEXT
} ConsoleHistoryNav;

/**
 * @brief The console history structure.
 */
typedef struct {

  /**
   * @brief The circular buffer of history strings.
   */
  char strings[CON_HISTORY_SIZE][MAX_PRINT_MSG];

  /**
   * @brief The always-increasing, un-clamped insert index.
   */
  size_t index;

  /**
   * @brief The circularly clamped history position for up-scrolling.
   */
  size_t pos;

} ConsoleHistory;

/**
 * @brief The console input structure.
 */
typedef struct {

  /**
   * @brief The input buffer.
   */
  char buffer[MAX_PRINT_MSG];

  /**
   * @brief The cursor offset into `buffer`.
   */
  size_t pos;

} ConsoleInput;

/**
 * @brief The console structure.
 */
typedef struct {

  /**
   * @brief Console width in characters.
   */
  size_t width;

  /**
   * @brief Console height in characters.
   */
  size_t height;

  /**
   * @brief The scroll offset in lines.
   */
  size_t scroll;

  /**
   * @brief The minimum timestamp for tail operations.
   */
  uint32_t whence;

  /**
   * @brief The level mask for tail operations.
   */
  uint32_t level;

  /**
   * @brief The history structure.
   */
  ConsoleHistory history;

  /**
   * @brief The input structure.
   */
  ConsoleInput input;

  /**
   * @brief If true, input is echoed to the console subsystem.
   */
  bool echo;

  /**
   * @brief An optional print callback.
   */
  void (*Append)(const ConsoleString *str);
} Console;

/**
 * @brief The structure used for autocomplete values.
 */
typedef struct {

  /**
   * @brief The match itself
   */
  char *name;

  /**
   * @brief The value printed to the screen. If null, name isused.
   */
  char *description;
} ConAutocompleteMatch;

void Con_Append(int32_t level, const char *string);
size_t Con_Wrap(const char *chars, size_t lineWidth, char **lines, size_t maxLines);
size_t Con_Tail(const Console *console, char **lines, size_t maxLines);
void Con_NavigateHistory(Console *console, ConsoleHistoryNav nav);
void Con_ReadHistory(Console *console, File *file);
void Con_WriteHistory(const Console *console, File *file);
bool Con_CompleteInput(Console *console);
void Con_SubmitInput(Console *console);
void Con_AddConsole(const Console *console);
void Con_RemoveConsole(const Console *console);
void Con_AutocompleteMatch(List *matches, const char *name, const char *description);
void Con_AutocompleteInput_f(const uint32_t argi, List *matches);

void Con_Init(void);
void Con_Shutdown(void);
