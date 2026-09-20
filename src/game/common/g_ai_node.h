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

#if defined(__G_LOCAL_H__)

uint32_t G_Ai_Node_Count(void);
AiNodeId G_Ai_Node_Create(const Vec3 position);
bool G_Ai_Node_IsLinked(const AiNodeId a, const AiNodeId b);

/**
 * @brief A link from one AI node to another, with traversal cost.
 */
typedef struct {

  /**
   * @brief Destination node ID.
   */
  AiNodeId id;

  /**
   * @brief Traversal cost to reach the destination node.
   */
  float cost;
} AiLink;

const Vector *G_Ai_Node_GetLinks(const AiNodeId a);
Vec3 G_Ai_Node_GetPosition(const AiNodeId node);
AiNodeId G_Ai_Node_FindClosest(const Vec3 position, const float maxDistance, const bool onlyVisible, const bool preferLevel);
bool G_Ai_Node_CanPathTo(const Vec3 position);
bool G_Ai_Path_CanPathTo(const Vector *path, const uint32_t index);
void G_Ai_Node_Link(const AiNodeId a, const AiNodeId b, const float cost);
void G_Ai_Node_PlayerRoam(GameClient *cl, const PMoveCmd *cmd);
void G_Ai_Node_Render(void);
void G_Ai_InitNodes(void);
void G_Ai_NodesReady(void);
void G_Ai_SaveNodes(void);
void G_Ai_Node_Destroy(const AiNodeId id);
void G_Ai_DeleteNodes(void);
void G_Ai_ShutdownNodes(void);

typedef float (*G_Ai_NodeCostFunc)(const AiNodeId a, const AiNodeId b);

/**
 * @brief Heuristic cost function for A* pathfinding using Manhattan distance.
 */
static inline float G_Ai_Node_Heuristic(const AiNodeId link, const AiNodeId end) {
  const Vec3 av = G_Ai_Node_GetPosition(link);
  const Vec3 bv = G_Ai_Node_GetPosition(end);
  return fabsf(av.x - bv.x) + fabsf(av.y - bv.y) + fabsf(av.z - bv.z);
}

/**
 * @brief Computes the traversal cost between two nodes as their Euclidean distance.
 */
static inline float G_Ai_Node_Cost(const AiNodeId a, const AiNodeId b) {
  const Vec3 av = G_Ai_Node_GetPosition(a);
  const Vec3 bv = G_Ai_Node_GetPosition(b);

  return Vec3_Distance(av, bv);
}

Vector *G_Ai_Node_FindPath(const GameClient *cl, const AiNodeId start, const AiNodeId end, const G_Ai_NodeCostFunc heuristic, float *length);
Vector *G_Ai_Node_TestPath(void);
bool G_Ai_DropItemLikeNode(GameEntity *ent);

#endif
