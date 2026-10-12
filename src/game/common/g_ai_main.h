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

#include "g_types.h"

#if defined(__G_LOCAL_H__)
#define AI_NAME "default"

#define G_Ai_Debug(...) gi.Debug(DEBUG_AI, __func__, __VA_ARGS__)

#include "g_ai_combat.h"
#include "g_ai_goal.h"
#include "g_ai_grid.h"
#include "g_ai_info.h"
#include "g_ai_item.h"
#include "g_ai_node.h"
#include "g_ai_state.h"
#include "g_ai_types.h"

extern Cvar *g_aiNoTarget;
extern Cvar *g_aiNodeDev;

void G_Ai_Disconnect(GameClient *cl);
void G_Ai_InvalidateReferences(GameAi *ai, const GameEntity *ent);
void G_Ai_Think(GameClient *cl, PMoveCmd *cmd);
void G_Ai_Respawn(GameClient *cl);
void G_Ai_Begin(GameClient *cl);
void G_Ai_Init(void);
void G_Ai_Frame(void);
void G_Ai_Load(void);
void G_Ai_Shutdown(void);
bool G_Ai_InDeveloperMode(void);
bool G_Ai_ShouldSlowDrop(const GameAiNodeId fromNode, const GameAiNodeId toNode);
bool G_Ai_IsArmed(const GameClient *cl);
bool G_Ai_IsOutgunned(const GameClient *cl, const GameEntity *enemy);
bool G_Ai_ShouldRetreat(const GameClient *cl);

/**
 * @return The health below which the bot retreats. Aggressive bots retreat at lower health.
 */
int32_t G_Ai_RetreatHealth(const GameClient *cl);

/**
 * @return True if nothing solid lies between the AI's eyes and the entity.
 */
bool G_Ai_HasLineOfSight(const GameClient *cl, const GameEntity *other);

/**
 * @return True if the AI is unarmed, and the item is a weapon that it lacks and that would arm it.
 */
bool G_Ai_WouldArm(const GameClient *cl, const GameItem *item);

/**
 * @return True if the entity is a map control item and the AI is armed, or a dropped weapon that
 * the AI lacks. Bots go after these above all else, even in combat. An unarmed bot arms itself
 * first, so that it can defend the map control item it then takes.
 */
bool G_Ai_IsCovetedItem(const GameClient *cl, const GameEntity *ent);

/**
 * @return True if the bot remembers that the taken item returns by the time it gets there.
 * @param eta The time in milliseconds that the bot needs to get to the item.
 */
bool G_Ai_IsTimedItem(const GameClient *cl, const GameEntity *item, const uint32_t eta);

/**
 * @return True if the AI knows about the item. Bots learn where the map's items spawn, so they need
 * no sight of those. A dropped item must be in sight, and in view unless it is near or coveted.
 */
bool G_Ai_IsItemKnown(const GameClient *cl, const GameEntity *ent, const bool coveted);

/**
 * @return The distance at which the bot prefers to fight with its current weapon.
 */
float G_Ai_PreferredRange(const GameClient *cl);

/**
 * @brief Ends the bot's path, because it reached the end or cannot follow it, and lets the state
 * machine choose the next one at once.
 */
void G_Ai_EndPath(GameClient *cl);
bool G_Ai_CanSee(const GameClient *cl, const GameEntity *other);
bool G_Ai_FallsToVoid(const GameClient *cl, Vec3 origin, Vec3 velocity);
Vec3 G_Ai_JumpVelocity(const GameClient *cl);

/**
 * @brief The distance at which bots hear a weapon fire.
 */
#define AI_HEAR_WEAPON_DISTANCE 1024.f

/**
 * @brief The distance at which bots hear an item picked up.
 */
#define AI_HEAR_PICKUP_DISTANCE 512.f

/**
 * @brief Alerts the bots within `distance` of `source`, other than its teammates, to its position.
 */
void G_Ai_Hear(const GameEntity *source, const float distance);

/**
 * @brief Alerts a bot that `attacker` hurt it, so that it turns toward the attacker and prefers
 * it as a target.
 */
void G_Ai_Damaged(const GameEntity *target, const GameEntity *attacker);

/**
 * @return True if the bot heard an enemy, or was shot, within `AI_ALERT_MILLIS`.
 */
bool G_Ai_IsAlerted(const GameClient *cl);

/**
 * @return True if `ent` hurt the bot recently.
 */
bool G_Ai_IsAttacker(const GameClient *cl, const GameEntity *ent);

/**
 * @brief Lets the bots that see or hear a map control item taken remember when it respawns, and
 * lets a bot that took the item avoid it for a time.
 */
void G_Ai_ItemTaken(const GameEntity *item, const GameEntity *taker);

/**
 * @brief Candidate item considered for AI pickup, weighted by desirability.
 */
typedef struct {
  const GameEntity *entity;
  const GameItem *item;
  float weight;
} GameAiItemPick;

int32_t G_Ai_CompareItems(const void *a, const void *b);

/**
 * @return The shortest signed rotation in degrees from `from` to `to`.
 */
static inline float G_Ai_AngleDelta(const float from, const float to) {
  return remainderf(to - from, 360.f);
}

#endif
