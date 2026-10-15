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

uint32_t G_Ai_Node_Count(void);
GameAiNodeId G_Ai_Node_Create(const Vec3 position);
bool G_Ai_Node_IsLinked(const GameAiNodeId a, const GameAiNodeId b);

/**
 * @brief A link from one AI node to another, with traversal cost.
 */
typedef struct {
  /**
   * @brief Destination node ID.
   */
  GameAiNodeId id;

  /**
   * @brief Traversal cost to reach the destination node.
   */
  float cost;

  /**
   * @brief The `NavTravelType`.
   */
  int32_t travelType;

  /**
   * @brief The inline model of the door whose doorway the link crosses, as `NavLink::gate`, or 0.
   */
  int32_t gate;

  /**
   * @brief True if `hazard` holds the result of `G_Ai_LinkPassesHazard` for the link, which
   * `G_Ai_Node_LinkTravel` tests once for a link between two nodes that do not move.
   */
  bool hazardTested;

  /**
   * @brief True if the link's line passes lava or slime, once `hazardTested`.
   */
  bool hazard;
} GameAiLink;

const Vector *G_Ai_Node_GetLinks(const GameAiNodeId a);
const GameAiLink *G_Ai_Node_GetLink(const GameAiNodeId a, const GameAiNodeId b);
Vec3 G_Ai_Node_GetPosition(const GameAiNodeId node);

/**
 * @return True if the node stands at the edge of the void, as `NavNode::edge`.
 */
bool G_Ai_Node_IsEdge(const GameAiNodeId node);

/**
 * @return True if the node is in lava or slime.
 */
bool G_Ai_Node_IsHazard(const GameAiNodeId node);

/**
 * @return True if the node is in water, lava or slime.
 */
bool G_Ai_Node_IsLiquid(const GameAiNodeId node);

/**
 * @return The climb of the ladder link from node `a` to node `b`, or NULL.
 */
const NavLadderClimb *G_Ai_Node_GetClimb(const GameAiNodeId a, const GameAiNodeId b);

/**
 * @return The ladder face that the climb climbs.
 */
const NavLadder *G_Ai_Node_GetLadder(const NavLadderClimb *climb);

GameAiNodeId G_Ai_Node_FindClosest(const Vec3 position, const float maxDistance, const bool onlyVisible, const bool preferLevel);
GameAiNodeId G_Ai_Node_FindClosestStatic(const Vec3 position, const float maxDistance, const bool onlyVisible, const bool preferLevel);

/**
 * @brief Finds the entry of an angled `trigger_push`: the player origin behind the pad, on the line
 * of its push through its center, from which a player that walks along the push enters the pad.
 * @param entry Receives the player origin on the floor at the entry. The floor there MAY be up to a
 * step higher than the pad's.
 * @return True if the push is angled, and a player has floor at the entry and can walk from it to
 * the pad.
 */
bool G_Ai_Node_PushEntry(const GameEntity *push, Vec3 *entry);
bool G_Ai_Node_CanPathTo(const Vec3 position);
const GameEntity *G_Ai_Node_RaisedPlatform(const Vec3 position);
bool G_Ai_Path_CanPathTo(const Vector *path, const uint32_t index);
void G_Ai_Node_Link(const GameAiNodeId a, const GameAiNodeId b, const float cost);
void G_Ai_Node_LinkTravel(const GameAiNodeId a, const GameAiNodeId b, const float cost, const int32_t travelType);
void G_Ai_InitNodes(void);
bool G_Ai_Node_IsStale(void);
void G_Ai_NodesReady(void);
void G_Ai_Node_UpdateMovers(void);
bool G_Ai_Node_IsCrushing(const GameAiNodeId id);
void G_Ai_ShutdownNodes(void);

typedef float (*GameAiNodeCostFunc)(const GameAiNodeId a, const GameAiNodeId b);

/**
 * @brief Heuristic cost function for A* pathfinding using Manhattan distance.
 */
static inline float G_Ai_Node_Heuristic(const GameAiNodeId link, const GameAiNodeId end) {
  const Vec3 av = G_Ai_Node_GetPosition(link);
  const Vec3 bv = G_Ai_Node_GetPosition(end);
  return fabsf(av.x - bv.x) + fabsf(av.y - bv.y) + fabsf(av.z - bv.z);
}

/**
 * @brief Computes the traversal cost between two nodes as their Euclidean distance.
 */
static inline float G_Ai_Node_Cost(const GameAiNodeId a, const GameAiNodeId b) {
  const Vec3 av = G_Ai_Node_GetPosition(a);
  const Vec3 bv = G_Ai_Node_GetPosition(b);

  return Vec3_Distance(av, bv);
}

/**
 * @brief Finds a path between two nodes with A*.
 * @param maxCost The search MUST NOT extend a path whose cost, including drop penalties, exceeds
 * this. Pass 0 for no limit.
 * @param length If not NULL, receives the cost of the path found, including drop penalties, as
 * `G_Ai_Node_TravelCosts` measures it.
 * @return The path as a Vector of `GameAiNodeId`, or NULL if none was found.
 */
Vector *G_Ai_Node_FindPath(const GameClient *cl, const GameAiNodeId start, const GameAiNodeId end, const GameAiNodeCostFunc heuristic, const float maxCost, float *length);

/**
 * @brief Finds the least travel cost from `start` to every node: the link costs plus the drop
 * penalties, which `G_Ai_Node_FindPath` minimizes.
 * @param costs An array of `G_Ai_Node_Count()` costs, which receives `FLT_MAX` for each node that
 * the client cannot reach.
 * @return True if the search ran to completion. A search that runs out of queue entries returns
 * false, because the nodes that it did not reach would read as unreachable.
 */
bool G_Ai_Node_TravelCosts(const GameClient *cl, const GameAiNodeId start, float *costs);
bool G_Ai_DropItemLikeNode(GameEntity *ent);

/**
 * @return True if the link crosses no doorway, or the door whose doorway it crosses is away from
 * where the map places it, or moves away from it. Until the level's doors are found, a link through
 * a doorway is closed.
 */
bool G_Ai_Node_IsLinkOpen(const GameAiLink *link);

/**
 * @return True if the link crosses the doorway of a door with the `targetName`.
 */
bool G_Ai_Node_IsLinkGatedBy(const GameAiLink *link, const char *targetName);

/**
 * @return True if a link crosses the doorway of a door with the `targetName`.
 */
bool G_Ai_Node_HasGate(const char *targetName);

/**
 * @brief The travel cost of a node through closed doors that a button opens, from
 * `G_Ai_Node_GateCosts`.
 */
typedef struct {
  /**
   * @brief The travel cost, or `FLT_MAX` if no path through the doors is cheaper than the path
   * without them.
   */
  float cost;

  /**
   * @brief The caller's key for the doors.
   */
  int32_t key;

  /**
   * @brief The node from which the path presses the button.
   */
  GameAiNodeId source;
} GameAiGateCost;

/**
 * @brief Finds the nodes that the client reaches cheaper if it goes first to one of the `sources`,
 * presses there the button that moves the doors with the `targetname` `doors`, and then crosses
 * the doors: a link through their doorway, whose state the press changes, or a press link. A path
 * starts at a source at the source's cost in `costs`, and MUST take each link through the doors
 * within `window` of travel cost after it. A path that has crossed the doors goes on only through
 * nodes that it reaches cheaper than `costs` does, except the nodes on the doors, whose costs in
 * `costs` are those of where the doors are now. Each node keeps one path for each phase, the
 * cheapest, so that a costlier path with more of the window left is lost. A node on the doors
 * follows only its links on the same doors and its press links, because its other links are those
 * of where the doors are now. Those links, as press links, cost what the build or the linking test
 * gave, because `G_Ai_Node_StepCost` would test them where the doors are now. Each node whose path
 * through the doors is cheaper than its cost in `gateCosts`, and, except a node on the doors, than
 * its cost in `costs`, receives that path's cost, the `key` and its source. The caller sets each
 * cost in `gateCosts` to `FLT_MAX` before the first call, so that the calls for several doors leave
 * the cheapest path of each node.
 * @param costs The travel costs from the client, from `G_Ai_Node_TravelCosts`.
 * @return True if the search ran to completion.
 */
bool G_Ai_Node_GateCosts(const GameClient *cl, const float *costs, const GameAiNodeId *sources, const int32_t numSources,
                         const char *doors, const float window, const int32_t key, GameAiGateCost *gateCosts);
#endif
