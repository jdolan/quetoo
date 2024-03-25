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

#ifdef __GAME_LOCAL_H__
#include "g_ai_types.h"

void Ai_SetPositionalGoal(const g_entity_t *self, ai_goal_t *goal, float priority, const vec3_t position);
void Ai_SetEntityGoal(const g_entity_t *self, ai_goal_t *goal, float priority, const g_entity_t *entity);
void Ai_SetPathGoal(const g_entity_t *self, ai_goal_t *goal, float priority, GArray *path, const g_entity_t *path_target);
void Ai_CopyGoal(const ai_goal_t *from, ai_goal_t *to);
void Ai_ClearGoal(ai_goal_t *goal);
bool Ai_GoalHasEntity(const ai_goal_t *goal, const g_entity_t *ent);
#endif /* __GAME_LOCAL_H__ */
