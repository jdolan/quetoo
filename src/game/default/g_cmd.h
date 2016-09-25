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

#ifndef __GAME_COMMANDS_H__
#define __GAME_COMMANDS_H__

#include "g_types.h"

#ifdef __GAME_LOCAL_H__
void G_InitVote();
void G_ShutdownVote();
_Bool G_AddClientToTeam(g_entity_t *ent, const char *team_name);
void G_ClientCommand(g_entity_t *ent);
void G_Score_f(g_entity_t *ent);
void G_Timeout_f(g_entity_t *ent);
void G_Stuffall_Sv_f(void);
void G_Mute_Sv_f(void);
void G_Stuff_Sv_f(void);

#endif /* __GAME_LOCAL_H__ */

#endif /* __GAME_COMMANDS_H__ */
