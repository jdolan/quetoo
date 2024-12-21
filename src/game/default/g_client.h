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

#pragma once

#include "g_types.h"

#ifdef __GAME_LOCAL_H__
void G_ClientBegin(g_entity_t *ent);
void G_ClientBeginFrame(g_entity_t *ent);
bool G_ClientConnect(g_entity_t *ent, char *user_info);
void G_ClientDisconnect(g_entity_t *ent);
void G_ClientRespawn(g_entity_t *ent, bool voluntary);
void G_ClientThink(g_entity_t *ent, pm_cmd_t *ucmd);
void G_ClientUserInfoChanged(g_entity_t *ent, const char *user_info);
void G_SetClientHookStyle(g_entity_t *ent);
#endif /* __GAME_LOCAL_H__ */
