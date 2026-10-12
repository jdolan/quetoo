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
 * @brief How a bot travels a link. The names follow the Quake III AAS travel types.
 */
typedef enum {
  /**
   * @brief A link that the game made to a node of its own, such as an item's.
   */
  AI_TRAVEL_NONE,

  /**
   * @brief Walk to the target, stepping up or down at most `PM_STEP_HEIGHT`.
   */
  AI_TRAVEL_WALK,

  /**
   * @brief Walk to the target while crouched, beneath a low ceiling.
   */
  AI_TRAVEL_CROUCH,

  /**
   * @brief Walk off a ledge and fall to the target.
   */
  AI_TRAVEL_WALK_OFF_LEDGE,

  /**
   * @brief Jump straight up onto a ledge that is too high to step onto.
   */
  AI_TRAVEL_BARRIER_JUMP,

  /**
   * @brief Touch a teleporter.
   */
  AI_TRAVEL_TELEPORT,

  /**
   * @brief Ride a `trigger_push`.
   */
  AI_TRAVEL_JUMP_PAD,

  /**
   * @brief Ride a `func_plat` up.
   */
  AI_TRAVEL_ELEVATOR,
} GameAiTravelType;

/**
 * @brief The width of a navigation cell, in units. Each cell has one node at most.
 */
#define AI_NAV_CELL_SIZE 64.f

/**
 * @brief The height of a navigation cell, in units, so that floors this far apart have their own
 * nodes.
 */
#define AI_NAV_CELL_HEIGHT 32.f

/**
 * @brief A node of the graph that `G_Ai_Nav_Build` makes.
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
   * @brief The index of the node's first link in `GameAiNavGraph::links`.
   */
  int32_t firstLink;

  /**
   * @brief The count of the node's links.
   */
  int32_t numLinks;
} GameAiNavNode;

/**
 * @brief A link of the graph that `G_Ai_Nav_Build` makes.
 */
typedef struct {
  /**
   * @brief The index of the node that the link leads to.
   */
  int32_t node;

  /**
   * @brief The `GameAiTravelType`.
   */
  GameAiTravelType travelType;

  /**
   * @brief The time to travel the link, in hundredths of a second.
   */
  int32_t travelTime;
} GameAiNavLink;

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

  GameAiNavNode *nodes;
  int32_t numNodes;

  GameAiNavLink *links;
  int32_t numLinks;

  /**
   * @brief The node on the floor of each cell, or -1.
   */
  int32_t *cellNodes;
} GameAiNavGraph;

/**
 * @brief Makes the level's navigation graph from its collision model: a node on each floor that a
 * player fits on in each cell, and walk, crouch, barrier jump and walk-off-ledge links between them.
 * @return True if the graph has at least one node.
 * @remarks The caller MUST free the graph with `G_Ai_Nav_Free`.
 */
bool G_Ai_Nav_Build(GameAiNavGraph *graph);

/**
 * @brief Frees the nodes and links of a graph that `G_Ai_Nav_Build` made, and keeps its cells.
 */
void G_Ai_Nav_FreeNodes(GameAiNavGraph *graph);

/**
 * @brief Frees a graph that `G_Ai_Nav_Build` made.
 */
void G_Ai_Nav_Free(GameAiNavGraph *graph);

#endif /* __G_LOCAL_H__ */
