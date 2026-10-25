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
 * @brief Lets the bot use the grapple hook along its path, as a player does. A bot with a distant
 * objective glances at it, and looks for the farthest node of its path that it can be pulled to in
 * a straight line, with a wall or a floor at most `AI_HOOK_BEYOND` units beyond it for the hook to
 * catch. If it finds one, it turns to that point and fires, and keeps to its route until the hook
 * pulls it. It lets go at the node, and always lets go in time. The bot does not consider the hook
 * in a fight, only when it chases or retreats from an enemy.
 * @return True if the hook pulls the bot, which then moves no other way.
 */
bool G_Ai_Hook_Move(GameClient *cl, PMoveCmd *cmd);

/**
 * @brief Sets the view angles with which the bot glances at its objective, or aims the hook.
 * @return True if the hook owns the bot's view.
 */
bool G_Ai_Hook_Turn(const GameClient *cl, Vec3 *angles);

/**
 * @return True if the bot fired the hook and has not let go: it is committed to the hook, as to a
 * climb, and its state machine enters `AI_STATE_HOOK`.
 */
bool G_Ai_Hook_IsCommitted(const GameClient *cl);

/**
 * @brief Forgets what the bot does with the hook, when it respawns.
 */
void G_Ai_Hook_Reset(GameClient *cl);

/**
 * @brief Finds the items of the level that no path reaches from most of the graph, but from which
 * a path leads to most of it, such as an item on a ledge that a player can only drop from. A bot
 * that sees one MAY hook to it. An item on an island that no path leaves, as over lava, is not one.
 */
void G_Ai_Hook_Load(void);

#endif
