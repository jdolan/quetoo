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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#pragma once

#if defined(__G_LOCAL_H__)

/**
 * @brief Funcgoal that runs the bot's decision state machine. The active state alone sets the
 * bot's move target: the long-term goal, a nearby item, the fight, the chase, the retreat or the
 * investigation of an alert. A bot that follows a path in the air keeps it until it lands.
 */
uint32_t G_Ai_State(GameClient *cl, PMoveCmd *cmd);

/**
 * @return True if the bot collects an item that is taken, and stands where it returns, so that the
 * bot reviews its choices only at intervals while it waits, and does not wander.
 */
bool G_Ai_IsWaiting(const GameClient *cl);

/**
 * @brief Returns the bot to `AI_STATE_ROAM` with no goal, as when it spawns.
 */
void G_Ai_State_Reset(GameClient *cl);

/**
 * @brief Lets the bot avoid the item for a time: because it took the item, failed to reach it, or
 * began a detour to it, so that a failed detour is not tried again at once.
 */
void G_Ai_Avoid(GameClient *cl, const GameEntity *item);

/**
 * @return The name of the bot's state, such as `collect` or `fight`.
 */
const char *G_Ai_StateName(const GameClient *cl);

/**
 * @return The item that the bot collects, or NULL if it collects none, or the item is gone.
 */
const GameEntity *G_Ai_GoalEntity(const GameClient *cl);

/**
 * @brief Frees the state machine's travel cost buffer.
 */
void G_Ai_State_Shutdown(void);
#endif
