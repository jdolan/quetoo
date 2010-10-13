/*
* Copyright(c) 1997-2001 Id Software, Inc.
* Copyright(c) 2002 The Quakeforge Project.
* Copyright(c) 2008 Quake2World.
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

#define CON_TEXTSIZE 32768
#define CON_MAXLINES 1024
#define CON_NUMTIMES 4
#define CON_SCROLL 5
#define CON_CURSORCHAR 0x0b

// common console structures
typedef struct {
	// console data
	char text[CON_TEXTSIZE]; 	// buffer to hold the console data
	char *insert;			// where to add new text
} consoledata_t;

typedef struct {
	qboolean initialized;
	// console dimensions
	int width;			// console printable width in characters
	int height;			// console printable height in characters

	// console index
	int linecolor[CON_MAXLINES];	// text color at the start of the line
	char *linestart[CON_MAXLINES];	// an index to word-wrapped line starts
	int lastline;			// last line of the console

	int scroll;			// scroll position

	float times[CON_NUMTIMES];  	// cls.realtime time the line was generated
					// for transparent notify lines
} console_t;

// common console functions
void Con_Init(void);
void Con_Shutdown(void);
void Con_Print(const char *text);
void Con_Resize(console_t *con, int width, int height);
int Con_CompleteCommand(char *input_text, int *input_position);

#endif /* __CONSOLE_H__ */
