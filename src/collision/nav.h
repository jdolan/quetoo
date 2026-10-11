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

#include "cm_types.h"

/**
 * @file
 * @brief The compiled navigation file format, `maps/<name>.nav`.
 * @details `quemap -nav` writes it from the BSP voxel grid: one floor node at most per voxel,
 * and typed links between nodes. The game reads it to route its bots, and appends the jumps
 * that it learns from players. Every field is little-endian, and the file is the header, then
 * `numNodes` nodes, `numLinks` links, `numJumps` jumps and `numJumpSteps` jump steps.
 */

/**
 * @brief The navigation file identifier. Version 2 files, hand-recorded in game, share it.
 */
#define NAV_MAGIC ('Q' | '2' << 8 | 'N' << 16 | 'S' << 24)

/**
 * @brief The compiled navigation file version.
 */
#define NAV_VERSION 3

/**
 * @brief How a bot travels a link. The names follow the Quake III AAS travel types.
 */
typedef enum {
  /**
   * @brief Walk to the target, stepping up or down at most `PM_STEP_HEIGHT`.
   */
  NAV_TRAVEL_WALK = 1,

  /**
   * @brief Walk to the target while crouched, beneath a low ceiling.
   */
  NAV_TRAVEL_CROUCH,

  /**
   * @brief Walk off a ledge and fall to the target.
   */
  NAV_TRAVEL_WALK_OFF_LEDGE,

  /**
   * @brief Jump straight up onto a ledge that is too high to step onto.
   */
  NAV_TRAVEL_BARRIER_JUMP,

  /**
   * @brief Touch a teleporter. The game links these at load; they are never compiled.
   */
  NAV_TRAVEL_TELEPORT,

  /**
   * @brief Ride a `trigger_push`. The game links these at load; they are never compiled.
   */
  NAV_TRAVEL_JUMP_PAD,

  /**
   * @brief Ride a `func_plat` up. The game links these at load; they are never compiled.
   */
  NAV_TRAVEL_ELEVATOR,

  /**
   * @brief Replay a `NavJump` that the game learned from a player.
   */
  NAV_TRAVEL_JUMP,
} NavTravelType;

/**
 * @brief Node flags.
 */
typedef enum {
  /**
   * @brief Only the crouched bounding box fits at this node.
   */
  NAV_NODE_CROUCH = 1 << 0,
} NavNodeFlags;

/**
 * @brief The navigation file header.
 */
typedef struct {
  /**
   * @brief `NAV_MAGIC`.
   */
  int32_t magic;

  /**
   * @brief `NAV_VERSION`.
   */
  int32_t version;

  /**
   * @brief The voxel grid dimensions of the BSP this file was compiled from.
   */
  Vec3i voxelSize;

  /**
   * @brief The voxel grid origin of the BSP this file was compiled from.
   */
  Vec3 voxelMins;

  /**
   * @brief The node count.
   */
  int32_t numNodes;

  /**
   * @brief The link count.
   */
  int32_t numLinks;

  /**
   * @brief The learned jump count.
   */
  int32_t numJumps;

  /**
   * @brief The jump step count, over all jumps.
   */
  int32_t numJumpSteps;
} NavHeader;

/**
 * @brief A floor position that a player can stand on.
 */
typedef struct {
  /**
   * @brief The player origin when standing here, `-PM_BOUNDS.mins.z` above the floor.
   */
  Vec3 position;

  /**
   * @brief The index of the voxel that sampled this node.
   */
  int32_t voxel;

  /**
   * @brief The `NavNodeFlags`.
   */
  int32_t flags;

  /**
   * @brief The liquid contents at `position`, if any.
   */
  int32_t contents;

  /**
   * @brief The index of this node's first link.
   */
  int32_t firstLink;

  /**
   * @brief The count of this node's links.
   */
  int32_t numLinks;
} NavNode;

/**
 * @brief A directed link from one node to another.
 */
typedef struct {
  /**
   * @brief The target node index.
   */
  int32_t node;

  /**
   * @brief The `NavTravelType`.
   */
  int32_t travelType;

  /**
   * @brief The travel time, in hundredths of a second.
   */
  int32_t travelTime;
} NavLink;

/**
 * @brief A jump that the game learned from a player: the state at its start, and the commands
 * that the player sent from there until landing.
 */
typedef struct {
  /**
   * @brief The node nearest the start of the jump.
   */
  int32_t from;

  /**
   * @brief The node nearest the landing.
   */
  int32_t to;

  /**
   * @brief The `PMovement` that the jump was learned under. It is valid for that movement only.
   */
  int32_t movement;

  /**
   * @brief The travel time, in hundredths of a second.
   */
  int32_t travelTime;

  /**
   * @brief The player origin at the start of the jump.
   */
  Vec3 origin;

  /**
   * @brief The player velocity at the start of the jump.
   */
  Vec3 velocity;

  /**
   * @brief The Pmove flags at the start of the jump.
   */
  int32_t flags;

  /**
   * @brief The Pmove timer at the start of the jump.
   */
  int32_t time;

  /**
   * @brief The index of the jump's first step.
   */
  int32_t firstStep;

  /**
   * @brief The count of the jump's steps.
   */
  int32_t numSteps;
} NavJump;

/**
 * @brief One movement command of a learned jump, and where it left the player.
 */
typedef struct {
  /**
   * @brief The view angles that Pmove used: the command's angles plus the delta angles.
   */
  Vec3 angles;

  /**
   * @brief The player origin after the command.
   */
  Vec3 origin;

  /**
   * @brief The command's directional intentions.
   */
  int16_t forward, right, up;

  /**
   * @brief The command's duration, in milliseconds.
   */
  uint8_t msec;

  /**
   * @brief The command's buttons.
   */
  uint8_t buttons;
} NavJumpStep;

_Static_assert(sizeof(NavHeader) == 48, "NavHeader is on disk; changing it requires a new NAV_VERSION");
_Static_assert(sizeof(NavNode) == 32, "NavNode is on disk; changing it requires a new NAV_VERSION");
_Static_assert(sizeof(NavLink) == 12, "NavLink is on disk; changing it requires a new NAV_VERSION");
_Static_assert(sizeof(NavJump) == 56, "NavJump is on disk; changing it requires a new NAV_VERSION");
_Static_assert(sizeof(NavJumpStep) == 32, "NavJumpStep is on disk; changing it requires a new NAV_VERSION");
