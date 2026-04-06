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
#define AI_NAME "default"

#define G_Ai_Debug(...) ({ if (gi.DebugMask() & DEBUG_AI) { gi.Debug_(DEBUG_AI, __func__, __VA_ARGS__); } })

#include "g_ai_goal.h"
#include "g_ai_info.h"
#include "g_ai_item.h"
#include "g_ai_node.h"
#include "g_ai_types.h"

extern cvar_t *g_ai_clients;
extern cvar_t *g_ai_no_target;
extern cvar_t *g_ai_node_dev;

void G_Ai_Disconnect(g_client_t *cl);
void G_Ai_Think(g_client_t *cl, pm_cmd_t *cmd);
void G_Ai_Respawn(g_client_t *cl);
void G_Ai_Begin(g_client_t *cl);
void G_Ai_Init(void);
void G_Ai_Frame(void);
void G_Ai_Load(void);
void G_Ai_Shutdown(void);
bool G_Ai_InDeveloperMode(void);
bool G_Ai_ShouldSlowDrop(const ai_node_id_t from_node, const ai_node_id_t to_node);

#endif /* __GAME_LOCAL_H__ */
