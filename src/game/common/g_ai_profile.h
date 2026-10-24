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
 * @brief The `g_aiProfile` flag that records what each bot does: where it is, its objective and
 * speed, its paths and why they end, its deaths, its pushes by a `trigger_push`, planned or not,
 * its gap jumps, its pickups, the times that it is stuck, its entries into lava or slime
 * (`HAZARD`), its grabs and releases of a ladder (`LADDER`), each node that it passes (`VISIT`),
 * the level's gates (`GATE`) and what it does with them (`GATEUSE`).
 */
#define AI_PROFILE_BOTS 1

/**
 * @brief The `g_aiProfile` flag that records the navigation graph once the game has added its nodes
 * and links, and the bounds of the level's `trigger_push` and `trigger_hurt` entities.
 */
#define AI_PROFILE_GRAPH 2

/**
 * @brief The `g_aiProfile` flag that records what each human player does: each movement command,
 * with the node nearest the player, each node that the player passes, and the player's pickups.
 */
#define AI_PROFILE_PLAYERS 4

/**
 * @brief Registers `g_aiProfile`, a developer cvar whose flags select what the profiler records.
 * With `g_aiProfile 0`, the default, the profiler does nothing.
 */
void G_Ai_Profile_Init(void);

/**
 * @brief Opens the profiler's log for the level, `ai_profile_<level>.log` in the write directory,
 * if `g_aiProfile` is set. Each line of the log is
 * `PROFILE <time> <client> <EVENT> <key>=<value> ...`, where the client is -1 for an event of the
 * level. `src/tools/ai_profile.py` reads it.
 */
void G_Ai_Profile_Load(void);

/**
 * @brief Closes the profiler's log.
 */
void G_Ai_Profile_Shutdown(void);

/**
 * @brief Records the navigation graph, with `AI_PROFILE_GRAPH`. The game calls it once its nodes
 * are ready.
 */
void G_Ai_Profile_Graph(void);

/**
 * @brief Records what the bot does, with `AI_PROFILE_BOTS`, and each node that it passes. The game
 * calls it once each frame for each bot.
 */
void G_Ai_Profile_Think(const GameClient *cl);

/**
 * @brief Records that the bot's move goal ends, and why, with `AI_PROFILE_BOTS`. The game calls it
 * before it clears the goal.
 * @param reason One word: `arrived`, `distress`, `unlinked` or `teleported`.
 */
void G_Ai_Profile_PathEnd(const GameClient *cl, const char *reason);

/**
 * @brief Records that the client took the item: a bot with `AI_PROFILE_BOTS`, a player with
 * `AI_PROFILE_PLAYERS`.
 */
void G_Ai_Profile_ItemTaken(const GameClient *cl, const GameEntity *item);

/**
 * @brief Records a gate that the game found, with `AI_PROFILE_BOTS`: the `targetname` of its doors,
 * the counts of its buttons, of the nodes from which a bot presses them, and of its doors, and the
 * time in milliseconds for which its doors stay activated after a press, or -1 if they do not move
 * back.
 */
void G_Ai_Profile_Gate(const char *name, const int32_t numButtons, const int32_t numSources,
                       const int32_t numDoors, const uint32_t openMillis);

/**
 * @brief Records that a bot changes its weapon, with `AI_PROFILE_BOTS`: `pick` when it picks the
 * best weapon for its target, `spam` when it takes a grenade weapon to spam with.
 */
void G_Ai_Profile_WeaponChange(const GameClient *cl, const GameItem *weapon, const char *reason);

/**
 * @brief Records what a bot did with a gate, with `AI_PROFILE_BOTS`: `choose` when it goes to the
 * button, `open` when it goes through the open doors, and `fail` when it gives up.
 */
void G_Ai_Profile_GateUse(const GameClient *cl, const char *name, const char *result);

/**
 * @brief Records a human player's movement command, with `AI_PROFILE_PLAYERS`, with the node
 * nearest the player after the move, or -1 if none is within reach, and a `VISIT` if the player
 * moved to another node. The game calls it after it moves the player with the command.
 */
void G_Ai_Profile_ClientMove(const GameClient *cl, const PMoveCmd *cmd);

/**
 * @brief Records that the client was killed, by whom, with which weapon and at which range, with
 * `AI_PROFILE_BOTS` or `AI_PROFILE_PLAYERS`, for bots and players alike. The attacker is the victim
 * itself for a suicide, and -1 for the world. The client's next `VISIT` starts a new route. For a
 * bot, with `AI_PROFILE_BOTS`, it also records the bot's `DEATH` and its samples before it. The
 * game calls it as the client dies.
 */
void G_Ai_Profile_Killed(const GameClient *cl, const GameEntity *attacker, const uint32_t mod);

/**
 * @brief Records how the graph links a `trigger_push`, with `AI_PROFILE_GRAPH`: `linked`, with the
 * link's nodes, or why it has no link: `nonode` (no node at the pad), `nofloor`, `noland` (the
 * flight does not land), or `nolanding` (no node in sight where it lands), with the landing.
 */
void G_Ai_Profile_PadLink(const GameEntity *push, const char *result, const int32_t from,
                          const int32_t to, const Vec3 landing);

/**
 * @brief Records what happened to a team's flag, with `AI_PROFILE_BOTS` or `AI_PROFILE_PLAYERS`:
 * `steal` from its base, `pickup` where it lies dropped, `capture`, `return` by a player of its team,
 * `drop`, or `reset` to its base when it lay dropped too long or fell into a `trigger_hurt`.
 * @param cl The client that did it, or NULL for a `reset`.
 * @param team The team whose flag it is.
 */
void G_Ai_Profile_Flag(const GameClient *cl, const char *event, const int32_t team);
#endif
