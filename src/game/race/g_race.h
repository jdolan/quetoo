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

#include "g_local.h"

/**
 * @file
 * @brief Racing: the course, the run, and the rules around them.
 *
 * `g_race.c` owns the run - starting, checkpoints, finishing, the modes and
 * the commands - and installs the hooks that make the module a race rather
 * than a deathmatch. `g_race_entity.c` owns the triggers that describe a
 * course, and calls in here when a client touches one.
 */

/**
 * @brief Installs racing over the hooks it needs, once per module image.
 */
void G_Race_Init(void);

/**
 * @brief How `cl` is taking part right now.
 */
g_race_mode_t G_Race_Mode(const g_client_t *cl);

/**
 * @brief The course, as the triggers spawn. Each records itself here, and
 * `G_ConfigureLevel` validates the result once they all have.
 */
void G_Race_AddStart(void);
bool G_Race_AddCheckpoint(int32_t checkpoint);
void G_Race_AddFinish(void);

/**
 * @brief The run, as the client crosses the course. Each returns true if the
 * touch counted, which is when a trigger fires its targets.
 */
bool G_Race_Start(g_client_t *cl);
bool G_Race_Checkpoint(g_client_t *cl, uint16_t checkpoint);
bool G_Race_Finish(g_client_t *cl);

/**
 * @brief Arms a start zone that begins the run on the way out, or on a jump
 * from inside it, rather than on the way in.
 */
void G_Race_ArmStart(g_client_t *cl, const g_entity_t *start);

/**
 * @brief Whether `cl` touched `ent` within `wait` seconds of last touching it.
 * A trigger stood in reports a touch every frame, and this is what stops each
 * of them counting.
 */
bool G_Race_Debounced(g_client_t *cl, const g_entity_t *ent, float wait);

/**
 * @brief Spawns the race triggers by class name, or returns false for a class
 * that is not one. Chained under `SpawnEntity` by `G_Race_Init`.
 */
bool G_Race_SpawnEntity(g_entity_t *ent);
