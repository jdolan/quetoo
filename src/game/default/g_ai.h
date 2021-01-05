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
#include "ai/ai.h"

#ifdef __GAME_LOCAL_H__
extern cvar_t *g_ai_max_clients;

void G_Ai_RegisterItems(void);
void G_Ai_ClientConnect(const g_entity_t *ent);
void G_Ai_ClientDisconnect(g_entity_t *ent);
void G_Ai_Init(void);
void G_Ai_Shutdown(void);
void G_Ai_Load(const char *mapname);
void G_Ai_Frame(void);
bool G_Ai_DropItemLikeNode(g_entity_t *ent);
#endif /* __GAME_LOCAL_H__ */
