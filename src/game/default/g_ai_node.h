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

#if defined(__GAME_LOCAL_H__)

guint G_Ai_Node_Count(void);
ai_node_id_t G_Ai_Node_Create(const vec3_t position);
bool G_Ai_Node_IsLinked(const ai_node_id_t a, const ai_node_id_t b);

/**
 * @brief A link from one AI node to another, with traversal cost.
 */
typedef struct {

  /**
   * @brief Destination node ID.
   */
  ai_node_id_t id;

  /**
   * @brief Traversal cost to reach the destination node.
   */
  float cost;
} ai_link_t;

const GArray *G_Ai_Node_GetLinks(const ai_node_id_t a);
vec3_t G_Ai_Node_GetPosition(const ai_node_id_t node);
ai_node_id_t G_Ai_Node_FindClosest(const vec3_t position, const float max_distance, const bool only_visible, const bool prefer_level);
bool G_Ai_Node_CanPathTo(const vec3_t position);
bool G_Ai_Path_CanPathTo(const GArray *path, const guint index);
void G_Ai_Node_Link(const ai_node_id_t a, const ai_node_id_t b, const float cost);
void G_Ai_Node_PlayerRoam(g_client_t *cl, const pm_cmd_t *cmd);
void G_Ai_Node_Render(void);
void G_Ai_InitNodes(void);
void G_Ai_NodesReady(void);
void G_Ai_SaveNodes(void);
void G_Ai_Node_Destroy(const ai_node_id_t id);
void G_Ai_DeleteNodes(void);
void G_Ai_ShutdownNodes(void);

typedef float (*G_Ai_NodeCostFunc)(const ai_node_id_t a, const ai_node_id_t b);

/**
 * @brief Heuristic cost function for A* pathfinding using weighted Manhattan distance.
 */
static inline float G_Ai_Node_Heuristic(const ai_node_id_t link, const ai_node_id_t end) {
  const vec3_t av = G_Ai_Node_GetPosition(link);
  const vec3_t bv = G_Ai_Node_GetPosition(end);
  float cost = fabsf(av.x - bv.x) + fabsf(av.y - bv.y) + fabsf(av.z - bv.z);

  if (!G_Ai_Node_CanPathTo(av)) {
    cost *= 8.f;
  }

  return cost;
}

/**
 * @brief Computes the traversal cost between two nodes as their Euclidean distance.
 */
static inline float G_Ai_Node_Cost(const ai_node_id_t a, const ai_node_id_t b) {
  const vec3_t av = G_Ai_Node_GetPosition(a);
  const vec3_t bv = G_Ai_Node_GetPosition(b);

  return Vec3_Distance(av, bv);
}

GArray *G_Ai_Node_FindPath(const ai_node_id_t start, const ai_node_id_t end, const G_Ai_NodeCostFunc heuristic, float *length);
GArray *G_Ai_Node_TestPath(void);
bool G_Ai_DropItemLikeNode(g_entity_t *ent);

#endif
