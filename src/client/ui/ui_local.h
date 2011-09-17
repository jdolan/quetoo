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

#ifndef __UI_LOCAL_H__
#define __UI_LOCAL_H__

#include "client.h"

#include <AntTweakBar.h>

typedef struct ui_s {
	TwBar *root;
	TwBar *servers;
	TwBar *controls;
	TwBar *player;
	TwBar *system;
} ui_t;

extern ui_t ui;

// ui_controls.c
TwBar *Ui_Controls(void);

// ui_data.c
void Ui_CvarText(TwBar *bar, const char *name, cvar_t *var, const char *def);
void Ui_CvarInteger(TwBar *bar, const char *name, cvar_t *var, const char *def);
void Ui_CvarDecimal(TwBar *bar, const char *name, cvar_t *var, const char *def);
void TW_CALL Ui_Command(void *data);
void Ui_Bind(TwBar *bar, const char *name, const char *bind, const char *def);

// ui_misc.c
void TW_CALL Ui_ToggleBar(void *data);

// ui_player.c
TwBar *Ui_Player(void);

// ui_servers.c
void Ui_NewServer(void);
TwBar *Ui_Servers(void);

TwBar *Ui_System(void);

#endif /* __UI_LOCAL_H__ */
