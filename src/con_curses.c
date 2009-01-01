/* Copyright(c) 2007 Quake2World.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or(at your option) any later version.
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
 * curses.c
 *
 * Functions for the curses console
 */

#include "config.h"

#ifdef HAVE_CURSES

#include <curses.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "console.h"
#include "con_curses.h"

#define CURSES_HISTORYSIZE 64
#define CURSES_LINESIZE 1024
#define CURSES_TIMEOUT 250	// 500 msec redraw timeout

static WINDOW *stdwin;	// ncurses standard window

static char input[CURSES_HISTORYSIZE][CURSES_LINESIZE];
static int historyline;
static int inputline;
static int inputpos;

console_t sv_con;

static char versionstring[32];

static int curses_redraw;		// indactes what part needs to be drawn
static int curses_lastupdate;		// number of msecs since last redraw


/*
 * Curses_SetColor
 *
 * Set the curses drawing color
 */
static void Curses_SetColor(int color){
	if (!has_colors())
		return;

	color_set(color, NULL);
	if (color == 3 || color == 0)
		attron(A_BOLD);
	else
		attroff(A_BOLD);
}


/*
 * Curses_DrawBackground
 *
 * Clear and draw background objects
 */
static void Curses_DrawBackground(void){
	Curses_SetColor(CON_COLOR_DEFAULT);
	bkgdset(' ');
	clear();

	// Draw the frame
	box(stdwin, ACS_VLINE , ACS_HLINE);

	// Draw the header
	Curses_SetColor(CON_COLOR_ALT);
	mvaddstr(0, 2, versionstring);
}


/*
 * Curses_DrawInput
 *
 * Draw the inputbox
 */
static void Curses_DrawInput(void){
	int x;
	const int xPos = COLS - 5 < CURSES_LINESIZE ? COLS - 5 : CURSES_LINESIZE - 1;

	Curses_SetColor(CON_COLOR_ALT);
	for(x = 2; x < COLS - 1; x++)
		mvaddstr(LINES-1, x," ");

	mvaddnstr(LINES-1, 3, input[historyline], xPos);

	// move the cursor to input position
	wmove(stdwin, LINES-1, 3 + inputpos);
}


/*
 * Curses_DrawConsole
 *
 * Draw the content of the console,
 * parse color codes and line breaks.
 *
 */
static void Curses_DrawConsole(void){
	int w,h;
	int x, y;
	int lines;
	int line;
	const char *pos;

	if(!sv_con.initialized)
		return;

	w = COLS-1;
	h = LINES-1;

	if(w < 3 && h < 3)
		return;

	Con_Resize(&sv_con, w - 1, h - 2);
	Curses_SetColor(CON_COLOR_DEFAULT);

	lines = sv_con.height + 1;

	y = 1;
	for(line = sv_con.lastline - sv_con.scroll - lines ; line < sv_con.lastline - sv_con.scroll; line++){
		if(line >= 0 && *sv_con.linestart[line]) {
			x = 1;
			// color of the first character of the line
			Curses_SetColor(sv_con.linecolor[line]);

			pos = sv_con.linestart[line];
			while(pos < sv_con.linestart[line + 1]){
				if(IS_LEGACY_COLOR(pos)){
					Curses_SetColor(CON_COLOR_ALT);
				} else if(IS_COLOR(pos)){
					Curses_SetColor(*(pos + 1) - '0');
					pos++;
				} else if(pos[0] == '\n' || pos[0] == '\r'){
					// skip \r and \n
					x++;
				} else if(x < w){
					mvaddnstr(y, x, pos, 1);
					x++;
				}
				pos++;
			}
		}
		y++;
	}

	// draw a scroll indicator
	if(sv_con.lastline > 0){
		Curses_SetColor(CON_COLOR_ALT);
		mvaddnstr(1 + ((sv_con.lastline-sv_con.scroll) * sv_con.height / sv_con.lastline) , w, "O", 1);
	}

	// reset drawing colors
	Curses_SetColor(CON_COLOR_DEFAULT);
}


/*
 * Curses_Refresh
 *
 * Mark the buffer for redraw
 */
void Curses_Refresh(void){
	curses_redraw |= 2;
}

/*
 * Curses_Draw
 *
 * Draw everything
 */
static void Curses_Draw(void){
	int timeout;

	if(!sv_con.initialized)
		return;

	if(con_timeout && con_timeout->value)
		timeout = con_timeout->value;
	else
		timeout = 250;

	if(curses_lastupdate > timeout && curses_redraw){
		if((curses_redraw & 2) == 2){
			// Refresh screen
			Curses_DrawBackground();
			Curses_DrawConsole();
			Curses_DrawInput();
		} else if((curses_redraw & 1) == 1){
			// Refresh input only
			Curses_DrawInput();
		}

		wrefresh(stdwin);

		curses_redraw = 0;
		curses_lastupdate = 0;
	}
}


/*
 * Curses_Resize
 *
 * Window resize signal handler
 */
void Curses_Resize(void){
	if(!sv_con.initialized)
		return;

	endwin();
	refresh();

	curses_redraw = 2;
	curses_lastupdate = con_timeout->value * 2;
	Curses_Draw();
}


/*
 * Curses_Frame
 *
 * Handle ncurses input and redraw if necessary
 */
void Curses_Frame(int msec){
	int key;
	char buf[CURSES_LINESIZE];

	if(!sv_con.initialized)
		return;

	key = wgetch(stdwin);
	while(key != ERR) {
		if(key == KEY_BACKSPACE || key == 8 || key == 127){
			if(input[historyline][0] != '\0' && inputpos > 0){
				inputpos--;
				key = inputpos;
				while(input[historyline][key]){
					input[historyline][key] = input[historyline][key + 1];
					key++;
				}
				curses_redraw |= 1;
			}
		} else if(key == KEY_STAB || key == 9){
			if(Con_CompleteCommand(input[historyline], &inputpos)){
				curses_redraw |= 2;
			}
		} else if(key == KEY_LEFT){
			if(input[historyline][0] != '\0' &&  inputpos > 0){
				inputpos--;
				curses_redraw |= 1;
			}
		} else if(key == KEY_HOME){
			if(inputpos > 0){
				inputpos = 0;
				curses_redraw |= 1;
			}
		} else if(key == KEY_RIGHT){
			if(input[historyline][0] != '\0' && inputpos < CURSES_LINESIZE - 1 && input[historyline][inputpos]){
				inputpos++;
				curses_redraw |= 1;
			}
		} else if(key == KEY_END){
			while(input[historyline][inputpos]){
				inputpos++;
			}
			curses_redraw |= 1;
		} else if(key == KEY_UP){
			if(input[(historyline + CURSES_HISTORYSIZE - 1) % CURSES_HISTORYSIZE][0] != '\0') {
				historyline = (historyline + CURSES_HISTORYSIZE - 1) % CURSES_HISTORYSIZE;
				inputpos = 0;
				while (input[historyline][inputpos])
					inputpos++;
				curses_redraw |= 1;
			}

		} else if (key == KEY_DOWN) {
			if (historyline != inputline) {
				historyline = (historyline + 1) % CURSES_HISTORYSIZE;
				inputpos = 0;
				while (input[historyline][inputpos])
					inputpos++;
				curses_redraw |= 1;
			}
		} else if(key == KEY_ENTER || key == '\n') {
			if(input[historyline][0] != '\0'){
				if (input[historyline][0] == '\\' || input[historyline][0] == '/')
					snprintf(buf, CURSES_LINESIZE - 2,"%s\n", input[historyline] + 1);
				else
					snprintf(buf, CURSES_LINESIZE - 1,"%s\n", input[historyline]);
				Com_Printf("]%s\n", input[historyline]);
				Cmd_ExecuteString(buf);

				if (historyline == inputline)
					inputline = (inputline + 1) % CURSES_HISTORYSIZE;

				memset(input[inputline], 0, sizeof(input[inputline]));
				historyline = inputline;
				inputpos = 0;

				curses_redraw |= 2;
			}
		} else if(key == KEY_PPAGE){
			if(sv_con.scroll < sv_con.lastline){
				// scroll up
				sv_con.scroll += CON_SCROLL;
				if(sv_con.scroll > sv_con.lastline)
					sv_con.scroll = sv_con.lastline;
				curses_redraw |= 2;
			}
		} else if(key == KEY_NPAGE){
			if (sv_con.scroll > 0){
				// scroll down
				sv_con.scroll -= CON_SCROLL;
				if(sv_con.scroll < 0)
					sv_con.scroll = 0;
				curses_redraw |= 2;
			}
		} else if(key >= 32 && key < 127 && inputpos < CURSES_LINESIZE - 1){
			const char c = (const char) key;
			// find the end of the line
			key = inputpos;
			while(input[historyline][key]){
				key++;
			}
			if(key < CURSES_LINESIZE - 1){
				while(key >= inputpos) {
					input[historyline][key + 1] = input[historyline][key];
					key--;
				}

				input[historyline][inputpos++] = c;
				curses_redraw |= 1;
			}
		}
		key = wgetch(stdwin);
	}

	curses_lastupdate += msec;
	Curses_Draw();
}


/*
 * Curses_Init
 *
 * Initialize the curses console
 */
void Curses_Init(void){
	sv_con.initialized = false;

	if(con_curses && !con_curses->value)
		return;

	stdwin = initscr();		// initialize the ncurses window
	cbreak();			// disable input line buffering
	noecho();			// don't show type characters
	keypad(stdwin, TRUE);		// enable special keys
	nodelay(stdwin, TRUE);		// non-blocking input
	curs_set(1);			// enable the cursor

	sv_con.scroll = 0;

	if(has_colors() == TRUE){
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
	snprintf(versionstring, sizeof(versionstring), " Quake2World %s ", VERSION);

	// clear the input box
	inputpos = 0;
	inputline = 0;
	historyline = 0;
	memset(input, 0, sizeof(input));

	refresh();

	curses_lastupdate = con_timeout->value * 2;

	sv_con.initialized = true;

	Curses_Draw();
}


/*
 * Curses_Shutdown
 *
 * Shutdown the curses console
 */
void Curses_Shutdown(void){
	// shutdown ncurses
	endwin();
}

#endif /* HAVE_CURSES */
