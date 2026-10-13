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
} GameAiLink;

const Vector *G_Ai_Node_GetLinks(const GameAiNodeId a);
const GameAiLink *G_Ai_Node_GetLink(const GameAiNodeId a, const GameAiNodeId b);
Vec3 G_Ai_Node_GetPosition(const GameAiNodeId node);

/**
 * @return True if the node stands at the edge of the void, as `NavNode::edge`.
 */
bool G_Ai_Node_IsEdge(const GameAiNodeId node);
GameAiNodeId G_Ai_Node_FindClosest(const Vec3 position, const float maxDistance, const bool onlyVisible, const bool preferLevel);
GameAiNodeId G_Ai_Node_FindClosestStatic(const Vec3 position, const float maxDistance, const bool onlyVisible, const bool preferLevel);
bool G_Ai_Node_CanPathTo(const Vec3 position);
const GameEntity *G_Ai_Node_RaisedPlatform(const Vec3 position);
bool G_Ai_Path_CanPathTo(const Vector *path, const uint32_t index);
void G_Ai_Node_Link(const GameAiNodeId a, const GameAiNodeId b, const float cost);
void G_Ai_Node_LinkTravel(const GameAiNodeId a, const GameAiNodeId b, const float cost, const int32_t travelType);
void G_Ai_InitNodes(void);
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

#endif
