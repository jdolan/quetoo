/**
 * @file m_main.h
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef CLIENT_MENU_M_MAIN_H
#define CLIENT_MENU_M_MAIN_H

#define MAX_MENUS			128
#define MAX_MENUNODES		8192
#define MAX_MENUSTACK		32
#define MAX_MENUACTIONS		8192

#include "client.h"

#include "ui_actions.h"
#include "ui_data.h"
#include "ui_windows.h"

/* initialization */
void MN_Init(void);
void MN_Shutdown(void);

/* misc */
void MN_SetCvar(const char *name, const char *str, float value);

#endif
