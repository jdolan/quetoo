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

void G_Ai_Disconnect(g_entity_t *self);
void G_Ai_Think(g_entity_t *self, pm_cmd_t *cmd);
void G_Ai_Respawn(g_entity_t *self);
void G_Ai_Begin(g_entity_t *self);
void G_Ai_InitLocals(void);
void G_Ai_Load(void);
void G_Ai_Shutdown(void);
bool G_Ai_InDeveloperMode(void);

#ifdef __GAME_LOCAL_H__
extern ai_level_t ai_level;

extern cvar_t *ai_no_target;
extern cvar_t *ai_node_dev;

/**
 * @brief Resolve the entity at the given index.
 */
#define ENTITY_FOR_NUM(n) \
	&g_game.entities[n]

ai_locals_t *Ai_GetLocals(const g_entity_t *ent);

bool Ai_ShouldSlowDrop(const ai_node_id_t from_node, const ai_node_id_t to_node);

#endif /* __GAME_LOCAL_H__ */
