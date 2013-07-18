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

#include "console.h"

#ifdef HAVE_CURSES

#include <signal.h>
#include <curses.h>

#define CURSES_HISTORYSIZE 64
#define CURSES_LINESIZE 1024
#define CURSES_TIMEOUT 250	// 250 msec redraw timeout

static WINDOW *stdwin; // ncurses standard window

static char input[CURSES_HISTORYSIZE][CURSES_LINESIZE];
static uint16_t history_line;
static uint16_t input_line;
static uint16_t input_pos;

console_t sv_console;

cvar_t *con_curses;
cvar_t *con_timeout;

static char version_string[32];

static uint32_t curses_redraw; // indicates what part needs to be drawn
static uint32_t curses_last_update; // number of milliseconds since last redraw

/*
 * @brief Set the curses drawing color
 */
static void Curses_SetColor(int32_t color) {

	if (!has_colors())
		return;

	color_set(color, NULL);
	if (color == 3 || color == 0)
		attron(A_BOLD);
	else
		attroff(A_BOLD);
}

/*
 * @brief Clear and draw background objects
 */
static void Curses_DrawBackground(void) {
	Curses_SetColor(CON_COLOR_DEFAULT);
	bkgdset(' ');
	clear();

	// Draw the frame
	box(stdwin, ACS_VLINE , ACS_HLINE);

	// Draw the header
	Curses_SetColor(CON_COLOR_ALT);
	mvaddstr(0, 2, version_string);
}

/*
 * @brief Draw the inputbox
 */
static void Curses_DrawInput(void) {
	int32_t x;
	const int32_t xPos = COLS - 5 < CURSES_LINESIZE ? COLS - 5 : CURSES_LINESIZE - 1;

	Curses_SetColor(CON_COLOR_ALT);
	for (x = 2; x < COLS - 1; x++)
		mvaddstr(LINES-1, x," ");

	mvaddnstr(LINES-1, 3, input[history_line], xPos);

	// move the cursor to input position
	wmove(stdwin, LINES - 1, 3 + input_pos);
}

/*
 * @brief Draw the content of the console, parse color codes and line breaks.
 */
static void Curses_DrawConsole(void) {
	int32_t w, h;
	int32_t x, y;
	int32_t lines;
	int32_t line;
	const char *pos;

	if (!sv_console.initialized)
		return;

	w = COLS - 1;
	h = LINES - 1;

	if (w < 3 && h < 3)
		return;

	Con_Resize(&sv_console, w - 1, h - 2);
	Curses_SetColor(CON_COLOR_DEFAULT);

	lines = sv_console.height + 1;

	y = 1;
	for (line = sv_console.last_line - sv_console.scroll - lines; line < sv_console.last_line
			- sv_console.scroll; line++) {
		if (line >= 0 && *sv_console.line_start[line]) {
			x = 1;
			// color of the first character of the line
			Curses_SetColor(sv_console.line_color[line]);

			pos = sv_console.line_start[line];
			while (pos < sv_console.line_start[line + 1]) {
				if (IS_LEGACY_COLOR(pos)) {
					Curses_SetColor(CON_COLOR_ALT);
				} else if (IS_COLOR(pos)) {
					Curses_SetColor(*(pos + 1) - '0');
					pos++;
				} else if (pos[0] == '\n' || pos[0] == '\r') {
					// skip \r and \n
					x++;
				} else if (x < w) {
					mvaddnstr(y, x, pos, 1);
					x++;
				}
				pos++;
			}
		}
		y++;
	}

	// draw a scroll indicator
	if (sv_console.last_line > 0) {
		Curses_SetColor(CON_COLOR_ALT);
		mvaddnstr(1 + ((sv_console.last_line-sv_console.scroll) * sv_console.height / sv_console.last_line) , w, "O", 1);
	}

	// reset drawing colors
	Curses_SetColor(CON_COLOR_DEFAULT);
}

/*
 * @brief Mark the buffer for redraw
 */
void Curses_Refresh(void) {
	curses_redraw |= 2;
}

/*
 * @brief Draw everything
 */
static void Curses_Draw(void) {

	if (!sv_console.initialized)
		return;

	const uint32_t timeout = Clamp(con_timeout->integer, 20, 1000);

	if (curses_last_update > timeout && curses_redraw) {
		if ((curses_redraw & 2) == 2) {
			// Refresh screen
			Curses_DrawBackground();
			Curses_DrawConsole();
			Curses_DrawInput();
		} else if ((curses_redraw & 1) == 1) {
			// Refresh input only
			Curses_DrawInput();
		}

		wrefresh(stdwin);

		curses_redraw = 0;
		curses_last_update = 0;
	}
}

#ifdef SIGWINCH
/*
 * @brief Window resize signal handler
 */
static void Curses_Resize(int32_t sig __attribute__((unused))) {

	if (!sv_console.initialized) {
		return;
	}

	endwin();
	refresh();

	curses_redraw = 2;
	curses_last_update = con_timeout->value * 2;

	Curses_Draw();
}
#endif

/*
 * @brief Handle curses input and redraw if necessary
 */
void Curses_Frame(uint32_t msec) {
	int32_t key;
	char buf[CURSES_LINESIZE];

	if (!sv_console.initialized)
		return;

	key = wgetch(stdwin);

	while (key != ERR) {
		if (key == KEY_BACKSPACE || key == 8 || key == 127) {
			if (input[history_line][0] != '\0' && input_pos > 0) {
				input_pos--;
				key = input_pos;
				while (input[history_line][key]) {
					input[history_line][key] = input[history_line][key + 1];
					key++;
				}
				curses_redraw |= 1;
			}
		} else if (key == KEY_STAB || key == 9) {
			if (Con_CompleteCommand(input[history_line], &input_pos, CURSES_LINESIZE - 1)) {
				curses_redraw |= 2;
			}
		} else if (key == KEY_LEFT) {
			if (input[history_line][0] != '\0' && input_pos > 0) {
				input_pos--;
				curses_redraw |= 1;
			}
		} else if (key == KEY_HOME) {
			if (input_pos > 0) {
				input_pos = 0;
				curses_redraw |= 1;
			}
		} else if (key == KEY_RIGHT) {
			if (input[history_line][0] != '\0' && input_pos < CURSES_LINESIZE - 1
					&& input[history_line][input_pos]) {
				input_pos++;
				curses_redraw |= 1;
			}
		} else if (key == KEY_END) {
			while (input[history_line][input_pos]) {
				input_pos++;
			}
			curses_redraw |= 1;
		} else if (key == KEY_UP) {
			if (input[(history_line + CURSES_HISTORYSIZE - 1) % CURSES_HISTORYSIZE][0] != '\0') {
				history_line = (history_line + CURSES_HISTORYSIZE - 1) % CURSES_HISTORYSIZE;
				input_pos = 0;
				while (input[history_line][input_pos])
					input_pos++;
				curses_redraw |= 1;
			}

		} else if (key == KEY_DOWN) {
			if (history_line != input_line) {
				history_line = (history_line + 1) % CURSES_HISTORYSIZE;
				input_pos = 0;
				while (input[history_line][input_pos])
					input_pos++;
				curses_redraw |= 1;
			}
		} else if (key == KEY_ENTER || key == '\n') {
			if (input[history_line][0] != '\0') {
				if (input[history_line][0] == '\\' || input[history_line][0] == '/')
					g_snprintf(buf, CURSES_LINESIZE - 2, "%s\n", input[history_line] + 1);
				else
					g_snprintf(buf, CURSES_LINESIZE - 1, "%s\n", input[history_line]);
				Com_Print("]%s\n", input[history_line]);
				Cmd_ExecuteString(buf);

				if (history_line == input_line)
					input_line = (input_line + 1) % CURSES_HISTORYSIZE;

				memset(input[input_line], 0, sizeof(input[input_line]));
				history_line = input_line;
				input_pos = 0;

				curses_redraw |= 2;
			}
		} else if (key == KEY_PPAGE) {
			if (sv_console.scroll < sv_console.last_line) {
				// scroll up
				sv_console.scroll += CON_SCROLL;
				if (sv_console.scroll > sv_console.last_line)
					sv_console.scroll = sv_console.last_line;
				curses_redraw |= 2;
			}
		} else if (key == KEY_NPAGE) {
			if (sv_console.scroll > 0) {
				// scroll down
				sv_console.scroll -= CON_SCROLL;
				if (sv_console.scroll < 0)
					sv_console.scroll = 0;
				curses_redraw |= 2;
			}
		} else if (key >= 32 && key < 127 && input_pos < CURSES_LINESIZE - 1) {
			const char c = (const char) key;
			// find the end of the line
			key = input_pos;
			while (input[history_line][key]) {
				key++;
			}
			if (key < CURSES_LINESIZE - 1) {
				while (key >= input_pos) {
					input[history_line][key + 1] = input[history_line][key];
					key--;
				}

				input[history_line][input_pos++] = c;
				curses_redraw |= 1;
			}
		}
		key = wgetch(stdwin);
	}

	curses_last_update += msec;
	Curses_Draw();
}

/*
 * @brief Initialize the curses console
 */
void Curses_Init(void) {

	memset(&sv_console, 0, sizeof(sv_console));

	if (dedicated->value) {
		con_curses = Cvar_Get("con_curses", "1", CVAR_NO_SET, NULL);
	} else {
		con_curses = Cvar_Get("con_curses", "0", CVAR_NO_SET, NULL);
	}

	con_timeout = Cvar_Get("con_timeout", "20", CVAR_ARCHIVE, NULL);

	if (!con_curses->value)
		return;

	stdwin = initscr(); // initialize the ncurses window
	cbreak(); // disable input line buffering
	noecho(); // don't show type characters
	keypad(stdwin, TRUE); // enable special keys
	nodelay(stdwin, TRUE); // non-blocking input
	curs_set(1); // enable the cursor

	sv_console.scroll = 0;

	if (has_colors() == TRUE) {
		start_color();
		// this is ncurses-specific
		use_default_colors();
		// COLOR_PAIR(0) is terminal default
		init_pair(1, COLOR_RED, -1);
		init_pair(2, COLOR_GREEN, -1);
		init_pair(3, COLOR_YELLOW, -1);
		init_pair(4, COLOR_BLUE, -1);
		init_pair(5, COLOR_CYAN, -1);
		init_pair(6, COLOR_MAGENTA, -1);
		init_pair(7, -1, -1);
	}

	// fill up the version string
	g_snprintf(version_string, sizeof(version_string), " Quake2World %s ", VERSION);

	// clear the input box
	input_pos = 0;
	input_line = 0;
	history_line = 0;
	memset(input, 0, sizeof(input));

#ifdef SIGWINCH
	signal(SIGWINCH, Curses_Resize);
#endif

	refresh();

	curses_last_update = con_timeout->value * 2;

	sv_console.initialized = true;

	Curses_Draw();
}

/*
 * @brief Shutdown the curses console
 */
void Curses_Shutdown(void) {

	if (sv_console.initialized) {
		endwin();
	}
}

#endif /* HAVE_CURSES */
