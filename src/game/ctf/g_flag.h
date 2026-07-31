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

#if defined(__GAME_LOCAL_H__)

const g_item_t *G_GetFlag(const g_client_t *cl);
g_team_t *G_TeamForFlag(const g_entity_t *ent);
g_entity_t *G_FlagForTeam(const g_team_t *team);
int32_t G_EffectForTeam(const g_team_t *team);

#endif /* __GAME_LOCAL_H__ */
