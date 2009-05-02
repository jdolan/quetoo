/*
* Copyright(c) 2007 Quake2World.
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

#ifndef __INCLUDED_CURSES_H__
#define __INCLUDED_CURSES_H__

#include "shared.h"

#ifdef HAVE_CURSES

void Curses_Init(void);
void Curses_Shutdown(void);
void Curses_Frame(int msec);
void Curses_Refresh(void);

#endif // HAVE_CURSES

#endif // __INCLUDED_CURSES_H__
