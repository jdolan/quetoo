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

#include "shared/shared.h"
#include "collision/cm_types.h"

/**
 * @brief How a bot travels a link. The names follow the Quake III AAS travel types.
 */
typedef enum {
  /**
   * @brief A link that the game made to a node of its own, such as an item's.
   */
  NAV_TRAVEL_NONE,

  /**
   * @brief Walk to the target, stepping up or down at most `PM_STEP_HEIGHT`.
   */
  NAV_TRAVEL_WALK,

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
   * @brief Touch a teleporter.
   */
  NAV_TRAVEL_TELEPORT,

  /**
   * @brief Ride a `trigger_push`.
   */
  NAV_TRAVEL_JUMP_PAD,

  /**
   * @brief Ride a `func_plat` up.
   */
  NAV_TRAVEL_ELEVATOR,

  /**
   * @brief Swim to the target: through water, or up to the surface and out onto a ledge.
   */
  NAV_TRAVEL_SWIM,

  /**
   * @brief Run and jump across a gap, from the edge of the floor.
   */
  NAV_TRAVEL_JUMP,
} NavTravelType;

/**
 * @brief The width of a navigation cell, in units. Each cell has one node at most.
 */
#define NAV_CELL_SIZE 64.f

/**
 * @brief The height of a navigation cell, in units, so that floors this far apart have their own
 * nodes.
 */
#define NAV_CELL_HEIGHT 32.f

/**
 * @brief A node of the graph that `Nav_Build` makes.
 */
typedef struct {
  /**
   * @brief The player origin, standing or crouched, on the floor of the node's cell.
   */
  Vec3 position;

  /**
   * @brief The index of the cell whose floor the node stands on.
   */
  int32_t cell;

  /**
   * @brief The liquid contents at the node.
   */
  int32_t contents;

  /**
   * @brief True if only the crouched bounding box fits at the node.
   */
  bool crouch;

  /**
   * @brief True if the node stands near an edge with no floor below it within `NAV_MAX_DROP`, such
   * as the edge of the void: a walk of `NAV_EDGE_DISTANCE` from it leaves the floor there.
   */
  bool edge;

  /**
   * @brief The inline model that the node stands on and that moves it, or 0 for a node that does
   * not move.
   */
  int32_t model;

  /**
   * @brief The node's position in the space of `model`, by which the game and the client game move
   * it with the model's entity.
   */
  Vec3 local;

  /**
   * @brief The index of the node's first link in `NavGraph::links`.
   */
  int32_t firstLink;

  /**
   * @brief The count of the node's links.
   */
  int32_t numLinks;
} NavNode;

/**
 * @brief A link of the graph that `Nav_Build` makes.
 */
typedef struct {
  /**
   * @brief The index of the node that the link leads to.
   */
  int32_t node;

  /**
   * @brief The `NavTravelType`.
   */
  NavTravelType travelType;

  /**
   * @brief The time to travel the link, in hundredths of a second.
   */
  int32_t travelTime;
} NavLink;

/**
 * @brief The navigation graph of the level.
 */
typedef struct {
  /**
   * @brief The origin of the cell grid, which covers the world model's bounds.
   */
  Vec3 mins;

  /**
   * @brief The cell grid's dimensions, in cells.
   */
  Vec3i size;

  /**
   * @brief The count of cells, `size.x * size.y * size.z`.
   */
  int32_t numCells;

  NavNode *nodes;
  int32_t numNodes;

  NavLink *links;
  int32_t numLinks;

  /**
   * @brief The node on the floor of each cell, or -1.
   */
  int32_t *cellNodes;

  /**
   * @brief The count of nodes found at an offset from their cell's center.
   */
  int32_t numOffset;

  /**
   * @brief The count of floors with no room for a player.
   */
  int32_t numNoFit;

  /**
   * @brief The count of floors in a `trigger_hurt` or a `trigger_push`.
   */
  int32_t numTrigger;

  /**
   * @brief The count of nodes at the edge of the void.
   */
  int32_t numEdge;

  /**
   * @brief The count of links of each `NavTravelType`.
   */
  int32_t numTravel[NAV_TRAVEL_JUMP + 1];
} NavGraph;

/**
 * @brief What `Nav_Build` needs from the game or the client game. The graph depends on the BSP and
 * its entities alone, so that both build the same graph.
 */
typedef struct {
  /**
   * @brief The level's collision model.
   */
  const CollisionBsp *bsp;

  /**
   * @brief The level's gravity.
   */
  float gravity;

  /**
   * @brief Traces a box against the given BSP head node.
   */
  CollisionTrace (*BoxTrace)(const Vec3 start, const Vec3 end, const Box3 bounds, int32_t headNode, int32_t contents);

  /**
   * @return True if `point` is inside `brush`.
   */
  bool (*PointInsideBrush)(const Vec3 point, const CollisionBrush *brush);

  /**
   * @return The value of the entity's key.
   */
  const Entity *(*EntityValue)(const Entity *entity, const char *key);

  /**
   * @brief Prints a warning.
   */
  void (*Warn)(const char *func, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

  /**
   * @brief Aborts with an error.
   */
  void (*Error)(const char *func, const char *fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));
} NavParams;

/**
 * @brief Makes the level's navigation graph from its collision model and entities: a node on each floor that a
 * player fits on in each cell, and walk, crouch, barrier jump and walk-off-ledge links between them.
 * @return True if the graph has at least one node.
 * @remarks The caller MUST free the graph with `Nav_Free`.
 */
bool Nav_Build(const NavParams *params, NavGraph *graph);

/**
 * @brief Frees the nodes and links of a graph that `Nav_Build` made, and keeps its cells.
 */
void Nav_FreeNodes(NavGraph *graph);

/**
 * @brief Frees a graph that `Nav_Build` made.
 */
void Nav_Free(NavGraph *graph);

