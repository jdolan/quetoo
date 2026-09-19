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

#include <curses.h>
#include <signal.h>

#if defined(_WIN32)
  #include <winsock2.h> // for AllocConsole
  #include <io.h>
  #define isatty _isatty
  #define fileno _fileno
#endif

#include "sv_local.h"

static struct {
  WINDOW *window;
  bool dirty;
} module;

static Console svConsole;

/**
 * @brief Console append callback.
 */
static void Sv_Print(const ConsoleString *str) {

  module.dirty = true;
}

/**
 * @brief Handle curses input and redraw if necessary
 */
static void Sv_HandleEvents(void) {

  ConsoleInput *in = &svConsole.input;

  int32_t key;
  while ((key = wgetch(module.window)) != ERR) {

    module.dirty = true;

    switch (key) {

      case '\n':
      case KEY_ENTER:
        Con_SubmitInput(&svConsole);
        break;

      case '\t':
      case KEY_STAB:
        Con_CompleteInput(&svConsole);
        break;

      case '\b':   // Windows backspace
      case 127:    // macOS backspace
      case KEY_BACKSPACE:
        if (in->pos > 0) {
          char *c = in->buffer + in->pos - 1;
          while (*c) {
            *c = *(c + 1);
            c++;
          }
          in->pos--;
        }
        break;

      case KEY_DC:
        if (in->pos < q_strlen(in->buffer)) {
          char *c = in->buffer + in->pos;
          while (*c) {
            *c = *(c + 1);
            c++;
          }
        }
        break;

      case KEY_UP:
        Con_NavigateHistory(&svConsole, CON_HISTORY_PREV);
        break;

      case KEY_DOWN:
        Con_NavigateHistory(&svConsole, CON_HISTORY_NEXT);
        break;

      case KEY_LEFT:
        if (in->pos > 0) {
          in->pos--;
        }
        break;

      case KEY_RIGHT:
        if (in->pos < q_strlen(in->buffer)) {
          in->pos++;
        }
        break;

      case KEY_PPAGE:
        if (svConsole.scroll < consoleState.strings->count) {
          svConsole.scroll++;
        } else {
          svConsole.scroll = consoleState.strings->count;
        }
        break;

      case KEY_NPAGE:
        if (svConsole.scroll > 0) {
          svConsole.scroll--;
        } else {
          svConsole.scroll = 0;
        }
        break;

      case KEY_HOME:
        in->pos = 0;
        break;

      case KEY_END:
        in->pos = q_strlen(in->buffer);
        break;

      default:
        if (isascii(key) && isprint(key)) {
          if (q_strlen(in->buffer) < sizeof(in->buffer) - 1) {
            char tmp[MAX_STRING_CHARS];
            q_strlcpy(tmp, in->buffer + in->pos, sizeof(tmp));
            in->buffer[in->pos++] = key;
            in->buffer[in->pos] = '\0';
            q_strlcat(in->buffer, tmp, sizeof(in->buffer));
          }
        }
        break;
    }
  }
}

/**
 * @brief Set the curses drawing color
 */
static void Sv_DrawConsole_Color(int32_t color) {

  if (!has_colors()) {
    return;
  }

  color_set(color, NULL);

  if (color == 3 || color == 0) {
    attron(A_BOLD);
  } else {
    attroff(A_BOLD);
  }
}

/**
 * @brief Clear and draw background objects
 */
static void Sv_DrawConsole_Background(void) {

  Sv_DrawConsole_Color(ESC_COLOR_WHITE);

  bkgdset(' ');
  clear();

  border(0, 0, 0, ' ', 0, 0, 0, 0);

  Sv_DrawConsole_Color(ESC_COLOR_ALT);
  mvaddstr(0, 2, va("Quetoo Dedicated %s", VERSION));
}

/**
 * @brief Renders the scrollback buffer to the curses window.
 */
static void Sv_DrawConsole_Buffer(void) {

  char *lines[svConsole.height];
  const size_t count = Con_Tail(&svConsole, lines, svConsole.height);

  size_t row = svConsole.height;

  for (size_t i = 0; i < count; i++) {
    const size_t j = count - i - 1;
    char *line = lines[j];
    char *s = line;

    Sv_DrawConsole_Color(j ? q_strrcolor(lines[j - 1]) : ESC_COLOR_DEFAULT);

    size_t col = 1;
    while (*s) {
      if (q_striscolor(s)) {
        Sv_DrawConsole_Color(*(s + 1) - '0');
        s++;
      } else if (isascii(*s)) {
        mvaddch((int32_t) row, (int32_t) col++, *s);
      }
      s++;
    }

    Mem_Free(line);
    row--;
  }

  Sv_DrawConsole_Color(ESC_COLOR_DEFAULT);
}

/**
 * @brief Draws the input buffer.
 */
static void Sv_DrawConsole_Input(void) {

  Sv_DrawConsole_Color(ESC_COLOR_ALT);

  const ConsoleInput *in = &svConsole.input;

  const char *s = &in->buffer[(in->pos / svConsole.width) * svConsole.width];

  const size_t len = q_strlen(s);
  const size_t pos = in->pos - (s - in->buffer);

  int32_t col = 2;
  for (size_t i = 0; i < len; i++) {
    mvaddch(LINES - 1, col++, *s++);
  }

  wmove(module.window, LINES - 1, (int32_t) pos + 2);
}

/**
 * @brief Draw everything
 */
void Sv_DrawConsole(void) {

  if (!module.window) {
    return;
  }

  Sv_HandleEvents();

  if (module.dirty) {

    svConsole.width = COLS - 2;
    svConsole.height = LINES - 2;

    Sv_DrawConsole_Background();
    Sv_DrawConsole_Buffer();
    Sv_DrawConsole_Input();

    refresh();

    module.dirty = false;
  }
}

#if defined(SIGWINCH)

/**
 * @brief Window resize signal handler
 */
static void Sv_ResizeConsole(int32_t sig) {

  endwin();

  module.dirty = true;

  Sv_DrawConsole();
}
#endif

/**
 * @brief Initializes the curses-based server console for dedicated servers.
 */
void Sv_InitConsole(void) {

  if (!dedicated->value) {
    return;
  }

  if (!isatty(fileno(stdin))) {
    Com_Warn("stdin is not a TTY, server console disabled\n");
    return;
  }

#if defined(_WIN32)
  if (AllocConsole()) {
    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONERR$", "w", stderr);
  } else {
    Com_Warn("Failed to allocate console: %u\n", (uint32_t) GetLastError());
  }
#endif

  memset(&module, 0, sizeof(module));

  module.window = initscr();
  module.dirty = true;

  atexit((void (*)(void)) endwin);

  cbreak();
  noecho();
  keypad(module.window, TRUE);
  nodelay(module.window, TRUE);
  curs_set(1);

  if (has_colors() == TRUE) {
    start_color();
    if (use_default_colors() == OK) {
      init_pair(ESC_COLOR_RED, COLOR_RED, -1);
      init_pair(ESC_COLOR_GREEN, COLOR_GREEN, -1);
      init_pair(ESC_COLOR_YELLOW, COLOR_YELLOW, -1);
      init_pair(ESC_COLOR_BLUE, COLOR_BLUE, -1);
      init_pair(ESC_COLOR_CYAN, COLOR_CYAN, -1);
      init_pair(ESC_COLOR_MAGENTA, COLOR_MAGENTA, -1);
      init_pair(ESC_COLOR_WHITE, COLOR_WHITE, -1);
    } else {
      init_pair(ESC_COLOR_RED, COLOR_RED, COLOR_BLACK);
      init_pair(ESC_COLOR_GREEN, COLOR_GREEN, COLOR_BLACK);
      init_pair(ESC_COLOR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
      init_pair(ESC_COLOR_BLUE, COLOR_BLUE, COLOR_BLACK);
      init_pair(ESC_COLOR_CYAN, COLOR_CYAN, COLOR_BLACK);
      init_pair(ESC_COLOR_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
      init_pair(ESC_COLOR_WHITE, COLOR_WHITE, COLOR_BLACK);
    }
  }

#if defined(SIGWINCH)
  signal(SIGWINCH, Sv_ResizeConsole);
#endif

  memset(&svConsole, 0, sizeof(svConsole));

  svConsole.Append = Sv_Print;

  Con_AddConsole(&svConsole);

  File *file = Fs_OpenRead("history");
  if (file) {
    Con_ReadHistory(&svConsole, file);
    Fs_Close(file);
  } else {
    Com_Debug(DEBUG_SERVER, "Couldn't read history");
  }

  Com_Print("Server console initialized\n");
}

/**
 * @brief Shuts down the curses-based server console.
 */
void Sv_ShutdownConsole(void) {

  if (!dedicated->value) {
    return;
  }

  if (!module.window) {
    return;
  }

  endwin();

  Con_RemoveConsole(&svConsole);

  File *file = Fs_OpenWrite("history");
  if (file) {
    Con_WriteHistory(&svConsole, file);
    Fs_Close(file);
  } else {
    Com_Warn("Couldn't write history\n");
  }

#if defined(_WIN32)
  FreeConsole();
#endif
}
