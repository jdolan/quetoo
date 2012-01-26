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

#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include "cmd.h"

#define CON_TEXT_SIZE 32768
#define CON_MAX_LINES 1024
#define CON_NUM_NOTIFY 4
#define CON_SCROLL 5
#define CON_CURSOR_CHAR 0x0b

// common console structures
typedef struct {
	// console data
	char text[CON_TEXT_SIZE]; // buffer to hold the console data
	char *insert; // where to add new text
} console_data_t;

typedef struct {
	boolean_t initialized;
	// console dimensions
	unsigned short width; // console printable width in characters
	unsigned short height; // console printable height in characters

	// console index
	int line_color[CON_MAX_LINES]; // text color at the start of the line
	char *line_start[CON_MAX_LINES]; // an index to word-wrapped line starts
	int last_line; // last line of the console

	int scroll; // scroll position

	float notify_times[CON_NUM_NOTIFY]; // cls.real_time time the line was generated
// for transparent notify lines
} console_t;

// common console functions
void Con_Init(void);
void Con_Shutdown(void);
void Con_Print(const char *text);
void Con_Resize(console_t *con, unsigned short width, unsigned short height);
int Con_CompleteCommand(char *input_text, unsigned short *input_position);

#ifdef HAVE_CURSES

#include <signal.h>
#include <curses.h>

#define CURSES_HISTORYSIZE 64
#define CURSES_LINESIZE 1024
#define CURSES_TIMEOUT 250	// 250 msec redraw timeout
// structures for the server console
extern console_t sv_con;
extern cvar_t *con_curses;
extern cvar_t *con_timeout;

void Curses_Init(void);
void Curses_Shutdown(void);
void Curses_Frame(unsigned int msec);
void Curses_Refresh(void);

#endif /* HAVE_CURSES */

#endif /* __CONSOLE_H__ */
