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
#include "bg_pmove.h"

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
  NAV_TRAVEL_PUSH,

  /**
   * @brief Ride a `func_plat` up.
   */
  NAV_TRAVEL_PLAT,

  /**
   * @brief Swim to the target: through water, or up to the surface and out onto a ledge.
   */
  NAV_TRAVEL_SWIM,

  /**
   * @brief Run and jump across a gap, from the edge of the floor.
   */
  NAV_TRAVEL_JUMP,

  /**
   * @brief Climb a ladder up to the floor over its top.
   */
  NAV_TRAVEL_LADDER,
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
   * @brief The player origin, standing or crouched, on the floor of the node's cell, or at its
   * center for a swim node.
   */
  Vec3 position;

  /**
   * @brief The index of the cell whose floor the node stands on, or that a swim node floats in.
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
   * @brief True if the node floats in water at the center of its cell, which has no floor. A swim
   * node links only by swimming.
   */
  bool swim;

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

  /**
   * @brief The inline model of the door whose doorway the walk, drop or jump crosses, or 0. The game lets
   * a bot use the link only while the door is away from where the map places it.
   */
  int32_t gate;
} NavLink;

/**
 * @brief A `trigger_push` of the level, as `Nav_Build` reads it from the entity, so that the game
 * and the client game fly the same pads.
 */
typedef struct {
  /**
   * @brief The bounds of the trigger.
   */
  Box3 bounds;

  /**
   * @brief The velocity that the trigger gives a player in it: `speed`, 100 by default, times 10,
   * along `angles` or `angle`, where an `angle` of -1 is up and -2 is down, as `G_SetMoveDir` reads
   * them.
   */
  Vec3 velocity;

  /**
   * @brief True if the trigger starts off, and pushes only once something uses it.
   */
  bool startOff;
} NavPush;

/**
 * @brief A climb of a ladder, which a ladder link makes.
 */
typedef struct {
  /**
   * @brief The node at the ladder's foot.
   */
  int32_t bottom;

  /**
   * @brief The node on the floor over the ladder's top.
   */
  int32_t top;

  /**
   * @brief The time of the climb in hundredths of a second.
   */
  int32_t time;

  /**
   * @brief The player origin at the foot of the column beside the ladder that the build tested,
   * which the nodes of the climb MAY stand beside.
   */
  Vec3 column;

  /**
   * @brief The player origin on the floor over the ladder's top, where the climb ends.
   */
  Vec3 landing;
} NavClimb;

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
   * @brief The node on the floor of each cell, or floating in it, or -1.
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
   * @brief The count of swim nodes.
   */
  int32_t numSwim;

  /**
   * @brief The level's `trigger_push` entities.
   */
  NavPush *pushes;
  int32_t numPushes;

  /**
   * @brief The count of links of each `NavTravelType`.
   */
  int32_t numTravel[NAV_TRAVEL_LADDER + 1];

  /**
   * @brief The count of walks and jumps through the doorway of a door.
   */
  int32_t numGated;

  /**
   * @brief The climbs of the level's ladders, one for each ladder link.
   */
  NavClimb *climbs;

  /**
   * @brief The count of `climbs`.
   */
  int32_t numClimbs;
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
 * @brief The longest flight in milliseconds that `Nav_FlyPush` follows. It is an integer, so that
 * an array sized by it is a constant array.
 */
#define NAV_FLIGHT_MILLIS 4000

/**
 * @brief The longest flight in seconds that `Nav_FlyPush` follows.
 */
#define NAV_FLIGHT_SECONDS (NAV_FLIGHT_MILLIS / 1000.f)

/**
 * @brief Flies a player from where `pm` stands on the push, with no input, as a bot rides one. The
 * push sets the velocity again in each move that the player's box touches it, as
 * `G_trigger_push_Push` does. One move is one server frame.
 * @param push The push to fly.
 * @param pm The move, with the level's movement parameters, the origin, and the trace and contents
 * functions that the caller chooses. The flight moves it.
 * @param points If not NULL, the player origin at the start of each move, up to `maxPoints`. The
 * last is where the flight lands, if it lands.
 * @param maxPoints The capacity of `points`.
 * @param numPoints The count of points written, if `points` is not NULL.
 * @param seconds The duration of the flight, written only if it lands.
 * @return True if the flight lands within `NAV_FLIGHT_SECONDS`, at `pm->s.origin`.
 */
bool Nav_FlyPush(const NavPush *push, PMove *pm, Vec3 *points, int32_t maxPoints, int32_t *numPoints, float *seconds);

/**
 * @brief Frees the nodes and links of a graph that `Nav_Build` made, and keeps its cells.
 */
void Nav_FreeNodes(NavGraph *graph);

/**
 * @brief Frees a graph that `Nav_Build` made.
 */
void Nav_Free(NavGraph *graph);

