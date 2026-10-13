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
 * @brief The `g_aiProbe` flag that records what each bot does: where it is, its goal and speed,
 * its paths and why they end, its deaths, its pushes by a `trigger_push`, planned or not, its gap
 * jumps, its pickups and the times that it is stuck.
 */
#define AI_PROBE_BOTS 1

/**
 * @brief The `g_aiProbe` flag that records the navigation graph once the game has added its nodes
 * and links, and the bounds of the level's `trigger_push` and `trigger_hurt` entities.
 */
#define AI_PROBE_GRAPH 2

/**
 * @brief Registers `g_aiProbe`, a developer cvar whose flags select what the probe records. With
 * `g_aiProbe 0`, the default, the probe does nothing.
 */
void G_Ai_Probe_Init(void);

/**
 * @brief Opens the probe's log for the level, `ai_probe_<level>.log` in the write directory, if
 * `g_aiProbe` is set. Each line of the log is `PROBE <time> <client> <EVENT> <key>=<value> ...`,
 * where the client is -1 for an event of the level. `src/tools/ai_probe.py` reads it.
 */
void G_Ai_Probe_Load(void);

/**
 * @brief Closes the probe's log.
 */
void G_Ai_Probe_Shutdown(void);

/**
 * @brief Records the navigation graph, with `AI_PROBE_GRAPH`. The game calls it once its nodes are
 * ready.
 */
void G_Ai_Probe_Graph(void);

/**
 * @brief Records what the bot does, with `AI_PROBE_BOTS`. The game calls it once each frame for each
 * bot.
 */
void G_Ai_Probe_Think(const GameClient *cl);

/**
 * @brief Records that the bot's move goal ends, and why, with `AI_PROBE_BOTS`. The game calls it
 * before it clears the goal.
 * @param reason One word: `arrived`, `distress`, `unlinked` or `teleported`.
 */
void G_Ai_Probe_PathEnd(const GameClient *cl, const char *reason);

/**
 * @brief Records that the bot took the item, with `AI_PROBE_BOTS`.
 */
void G_Ai_Probe_ItemTaken(const GameClient *cl, const GameEntity *item);
#endif
