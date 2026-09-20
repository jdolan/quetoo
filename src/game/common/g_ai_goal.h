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

#if defined(__G_LOCAL_H__)
#include "g_ai_types.h"

void G_Ai_SetPositionGoal(const GameClient *cl, AiGoal *goal, float priority, const Vec3 position);
void G_Ai_SetEntityGoal(const GameClient *cl, AiGoal *goal, float priority, const GameEntity *entity);
void G_Ai_SetPathGoal(const GameClient *cl, AiGoal *goal, float priority, Vector *path, const GameEntity *pathTarget);
void G_Ai_CopyGoal(const AiGoal *from, AiGoal *to);
void G_Ai_ClearGoal(AiGoal *goal);
bool G_Ai_GoalHasEntity(const AiGoal *goal, const GameEntity *ent);
#if AI_GOAL_HARDENING
const GameEntity *G_Ai_ResolveGoalEntity(int32_t number);
#endif
#endif
