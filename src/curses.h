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

#ifndef __CURSES_H__
#define __CURSES_H__

#include "console.h"

#ifdef HAVE_CURSES

#include <signal.h>
#include <ncurses/ncurses.h>

#define CURSES_HISTORYSIZE 64
#define CURSES_LINESIZE 1024
#define CURSES_TIMEOUT 250	// 250 msec redraw timeout

// structures for the server console
extern console_t sv_con;
extern cvar_t *con_curses;
extern cvar_t *con_timeout;

void Curses_Init(void);
void Curses_Shutdown(void);
void Curses_Frame(int msec);
void Curses_Refresh(void);

#endif /* HAVE_CURSES */

#endif /* __CURSES_H__ */
