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

#if defined(__G_LOCAL_H__)
#define AI_NAME "default"

#define G_Ai_Debug(...) gi.Debug(DEBUG_AI, __func__, __VA_ARGS__)

#include "g_ai_goal.h"
#include "g_ai_grid.h"
#include "g_ai_info.h"
#include "g_ai_item.h"
#include "g_ai_node.h"
#include "g_ai_types.h"

extern Cvar *g_ai_no_target;
extern Cvar *g_ai_node_dev;

void G_Ai_Disconnect(GameClient *cl);
void G_Ai_InvalidateReferences(Ai *ai, const GameEntity *ent);
void G_Ai_Think(GameClient *cl, PlayerMoveCmd *cmd);
void G_Ai_Respawn(GameClient *cl);
void G_Ai_Begin(GameClient *cl);
void G_Ai_Init(void);
void G_Ai_Frame(void);
void G_Ai_Load(void);
void G_Ai_Shutdown(void);
bool G_Ai_InDeveloperMode(void);
bool G_Ai_ShouldSlowDrop(const AiNodeId fromNode, const AiNodeId toNode);

#endif
