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

#include "g_local.h"
#include "bg_pmove.h"

#define AI_NODE(vector, index) VectorElement((vector), AiNode, (index))
#define AI_LINK(vector, index) VectorElement((vector), AiLink, (index))
#define AI_NODE_ID(vector, index) (VectorValue((vector), AiNodeId, (index)))
#define AI_PLATFORM(vector, index) (VectorValue((vector), GameEntity *, (index)))

/**
 * @brief Cached spatial acceleration structures for the navigation graph.
 *
 * The kd-tree accelerates `G_Ai_Node_FindClosest` queries (O(log n) average vs.
 * the previous O(n) linear scan), and is rebuilt lazily after any mutation
 * of the global node array.
 */
static struct GridKdTree *g_ai_nodes_kdtree = NULL;
static Vec3 *g_ai_nodes_kdtree_positions = NULL;
static size_t g_ai_nodes_kdtree_count = 0;

/**
 * @brief Invalidates the cached kd-tree. Must be called whenever the
 * underlying `g_ai_nodes` array is mutated (add/remove/move/reload).
 */
static void G_Ai_Node_InvalidateSpatialIndex(void) {
  if (g_ai_nodes_kdtree) {
    gridkdtree_free(&g_ai_nodes_kdtree);
  }
  if (g_ai_nodes_kdtree_positions) {
    free(g_ai_nodes_kdtree_positions);
    g_ai_nodes_kdtree_positions = NULL;
  }
  g_ai_nodes_kdtree_count = 0;
}

/**
 * @brief State used during player roam recording for AI node development.
 */
static struct {
  Vec3 position;

  Vec3 floorPosition;

  AiNodeId lastNodes[2];

  bool awaitLanding, isJumping, isWaterJump, onMover;

  InputButtonState latchedButtons, oldButtons, buttons;

  Vector *testPath;

  uint32_t fileNodes, fileLinks;

  bool doNoding, dropNodes;
} g_ai_player_roam;

/**
 * @brief Returns a test path between the last two recorded nodes for visualization.
 */
Vector *G_Ai_Node_TestPath(void) {

  if (!g_ai_node_dev->integer) {
    return NULL;
  }

  if (g_ai_player_roam.testPath) {
    release(g_ai_player_roam.testPath);
    g_ai_player_roam.testPath = NULL;
  }

  if (g_ai_player_roam.lastNodes[0] == AI_NODE_INVALID ||
    g_ai_player_roam.lastNodes[1] == AI_NODE_INVALID) {
    return NULL;
  }

  return g_ai_player_roam.testPath = G_Ai_Node_FindPath(NULL, g_ai_player_roam.lastNodes[1], g_ai_player_roam.lastNodes[0], G_Ai_Node_Heuristic, NULL);
}

/**
 * @brief An AI navigation node with world position, outgoing links, and pathfinding state.
 */
typedef struct {
  // persisted to disk

  Vec3 position;
  Vector *links;

  // only used for nav purposes

  float cost;
  AiNodeId cameFrom;
} AiNode;

/**
 * @brief The global array of navigation nodes for the current map.
 */
static Vector *g_ai_nodes;

/**
 * @brief The global array of platforms for the current map.
 */
static Vector *g_ai_platforms;

/**
 * @brief Returns the index of a node pointer within the global node array.
 */
static inline AiNodeId G_Ai_Node_Index(const AiNode *node) {
  return node - (AiNode *) g_ai_nodes->elements;
}

static void G_Ai_Node_FreePathPool(void);

/**
 * @brief Returns true if the given node is visible (unobstructed) from the specified position.
 */
static bool G_Ai_Node_Visible(const Vec3 position, const AiNodeId node) {

  return gi.Trace(position, G_Ai_Node_GetPosition(node), Box3_Zero(), NULL, CONTENTS_SOLID | CONTENTS_WINDOW).fraction == 1.0f;
}

/**
 * @brief Returns the total number of navigation nodes currently loaded.
 */
uint32_t G_Ai_Node_Count(void) {
  return g_ai_nodes ? (uint32_t) g_ai_nodes->count : 0;
}

/**
 * @brief Lazily (re)builds the kd-tree spatial index over g_ai_nodes.
 * Returns true on success. The index is invalidated whenever the node
 * array is mutated (see G_Ai_Node_InvalidateSpatialIndex callers).
 */
static bool G_Ai_Node_EnsureSpatialIndex(void) {

  if (!g_ai_nodes || g_ai_nodes->count == 0) {
    return false;
  }

  if (g_ai_nodes_kdtree && g_ai_nodes_kdtree_count == g_ai_nodes->count) {
    return true;
  }

  G_Ai_Node_InvalidateSpatialIndex();

  const size_t n = g_ai_nodes->count;
  g_ai_nodes_kdtree_positions = malloc(sizeof(Vec3) * n);
  if (!g_ai_nodes_kdtree_positions) {
    return false;
  }

  for (size_t i = 0; i < n; i++) {
    g_ai_nodes_kdtree_positions[i] = AI_NODE(g_ai_nodes, i)->position;
  }

  g_ai_nodes_kdtree = gridkdtree_create(g_ai_nodes_kdtree_positions, n);
  if (!g_ai_nodes_kdtree) {
    free(g_ai_nodes_kdtree_positions);
    g_ai_nodes_kdtree_positions = NULL;
    return false;
  }

  g_ai_nodes_kdtree_count = n;
  return true;
}

typedef struct {
  Vec3 position;
  bool onlyVisible;
  bool preferLevel;
} AiNodeQueryFilter;

/**
 * @brief Whether a node qualifies for `G_Ai_Node_FindClosest`, and at what distance.
 */
static bool G_Ai_Node_FindClosestFilter(const size_t nodenum, void *data, float *distance) {
  const AiNodeQueryFilter *filter = data;
  const AiNode *node = AI_NODE(g_ai_nodes, nodenum);

  Vec3 dir = Vec3_Subtract(filter->position, node->position);

  if (filter->preferLevel && !(gi.PointContents(node->position) & CONTENTS_MASK_LIQUID)) {
    dir.z *= 4.0f;
  }

  *distance = Vec3_LengthSquared(dir);

  return !filter->onlyVisible || G_Ai_Node_Visible(filter->position, nodenum);
}

/**
 * @brief Finds the closest navigation node to the given position within the specified distance.
 *
 * Uses the cached kd-tree spatial index (built lazily) to gather candidate
 * nodes within `max_distance` and selects the best one that passes the
 * `only_visible` / `prefer_level` filters. Falls back to a linear scan if
 * the spatial index is unavailable.
 */
AiNodeId G_Ai_Node_FindClosest(const Vec3 position, const float maxDistance, const bool onlyVisible, const bool preferLevel) {

  if (!g_ai_nodes || g_ai_nodes->count == 0) {
    return AI_NODE_INVALID;
  }

  AiNodeId closest = AI_NODE_INVALID;
  float closestDist = 0;
  const float distSquared = maxDistance * maxDistance;

  if (G_Ai_Node_EnsureSpatialIndex()) {
    AiNodeQueryFilter filter = {
      .position = position,
      .onlyVisible = onlyVisible,
      .preferLevel = preferLevel
    };

    const size_t node = gridkdtree_query_filter(g_ai_nodes_kdtree, position, maxDistance, G_Ai_Node_FindClosestFilter, &filter);

    return node == SIZE_MAX ? AI_NODE_INVALID : (AiNodeId) node;
  }

  // Fallback: linear scan (kd-tree build failed).
  for (size_t i = 0; i < g_ai_nodes->count; i++) {
    const AiNode *node = AI_NODE(g_ai_nodes, i);

    Vec3 dir = Vec3_Subtract(position, node->position);
    if (preferLevel && !(gi.PointContents(node->position) & CONTENTS_MASK_LIQUID)) {
      dir.z *= 4.0f;
    }
    const float dist = Vec3_LengthSquared(dir);

    if (dist < distSquared
        && (closest == AI_NODE_INVALID || dist < closestDist)
        && (!onlyVisible || G_Ai_Node_Visible(position, i))) {
      closest = i;
      closestDist = dist;
    }
  }

  return closest;
}

/**
 * @brief Creates and appends a new navigation node at the specified world position.
 */
AiNodeId G_Ai_Node_Create(const Vec3 position) {

  if (gi.PointContents(position) & CONTENTS_MASK_SOLID) {
    G_Ai_Debug("Rejected node at %s (inside solid)\n", vtos(position));
    return AI_NODE_INVALID;
  }

  if (!g_ai_nodes) {
    g_ai_nodes = $(alloc(Vector), initWithSize, sizeof(AiNode));
  }

  AiNode node = (AiNode) {
    .position = position
  };
  $(g_ai_nodes, add, &node);

  G_Ai_Node_InvalidateSpatialIndex();

  G_Ai_Debug("Dropped new node %zu\n", g_ai_nodes->count - 1);

  return g_ai_nodes->count - 1;
}

/**
 * @brief Returns true if node a has a directed link to node b.
 */
bool G_Ai_Node_IsLinked(const AiNodeId a, const AiNodeId b) {
  const AiNode *nodeA = AI_NODE(g_ai_nodes, a);

  if (nodeA->links) {
    for (size_t i = 0; i < nodeA->links->count; i++) {
      const AiLink *link = AI_LINK(nodeA->links, i);

      if (link->id == b) {
        return true;
      }
    }
  }

  return false;
}

/**
 * @brief Returns the array of outgoing links for the specified node.
 */
const Vector *G_Ai_Node_GetLinks(const AiNodeId a) {
  const AiNode *nodeA = AI_NODE(g_ai_nodes, a);
  return nodeA->links;
}

/**
 * @brief Creates a directed link from node a to node b with the given traversal cost.
 */
void G_Ai_Node_Link(const AiNodeId a, const AiNodeId b, const float cost) {

  if (!g_ai_nodes || a >= g_ai_nodes->count || b >= g_ai_nodes->count) {
    return;
  }

  if (a == b || G_Ai_Node_IsLinked(a, b)) {
    return;
  }

  AiNode *nodeA = AI_NODE(g_ai_nodes, a);

  if (!nodeA->links) {
    nodeA->links = $(alloc(Vector), initWithSize, sizeof(AiLink));
  }

  AiLink link = (AiLink) {
    .id = b,
    .cost = cost
  };
  $(nodeA->links, add, &link);

  G_Ai_Debug("Connected %d -> %d\n", a, b);
}

/**
 * @brief Creates a link between two nodes using their Euclidean distance as the cost.
 */
static inline void G_Ai_Node_LinkDefault(const AiNodeId a, const AiNodeId b, const bool bidirectional) {

  if (a == b) {
    return;
  }

  G_Ai_Node_Link(a, b, G_Ai_Node_Cost(a, b));

  if (bidirectional) {
    G_Ai_Node_LinkDefault(b, a, false);
  }
}

/**
 * @brief Removes the bidirectional link between two nodes.
 */
static void G_Ai_Node_Unlink(const AiNodeId a, const AiNodeId b) {
  {
    AiNode *nodeA = AI_NODE(g_ai_nodes, a);

    if (nodeA->links) {
      for (size_t i = 0; i < nodeA->links->count; i++) {
        const AiLink *link = AI_LINK(nodeA->links, i);

        if (link->id == b) {
          $(nodeA->links, removeAt, i);

          if (!nodeA->links->count) {
            release(nodeA->links);
            nodeA->links = NULL;
          }
          break;
        }
      }
    }
  }

  {
    AiNode *nodeB = AI_NODE(g_ai_nodes, b);
  
    if (nodeB->links) {
      for (size_t i = 0; i < nodeB->links->count; i++) {
        const AiLink *link = AI_LINK(nodeB->links, i);

        if (link->id == a) {
          $(nodeB->links, removeAt, i);

          if (!nodeB->links->count) {
            release(nodeB->links);
            nodeB->links = NULL;
          }
          break;
        }
      }
    }
  }
}

/**
 * @brief Removes all links connected to the specified node.
 */
static void G_Ai_Node_UnlinkAll(const AiNodeId id) {
  const AiNode *node = AI_NODE(g_ai_nodes, id);

  if (!node->links) {
    return;
  }

  for (size_t i = node->links->count - 1; ; i--) {
    G_Ai_Node_Unlink(id, AI_LINK(node->links, i)->id);
    
    if (i == 0 || !node->links) {
      break;
    }
  }
}

/**
 * @brief Node `id` has been removed, so we need to fix up connection IDs
 * so that they don't shift.
*/
static void G_Ai_Node_Adjust(const AiNodeId id) {

  for (size_t i = 0; i < g_ai_nodes->count; i++) {
    const AiNode *node = AI_NODE(g_ai_nodes, i);

    if (!node->links) {
      continue;
    }

    for (size_t l = 0; l < node->links->count; l++) {
      AiLink *link = AI_LINK(node->links, l);

      if (link->id >= id) {
        link->id--;
      }
    }
  }
}

/**
 * @brief Destroys a navigation node, removing all of its links and freeing its slot.
 */
void G_Ai_Node_Destroy(const AiNodeId id) {

  if (!g_ai_nodes || id >= g_ai_nodes->count) {
    G_Warn("Invalid node id %u\n", id);
    return;
  }

  G_Ai_Node_UnlinkAll(id);
  $(g_ai_nodes, removeAt, id);

  G_Ai_Node_InvalidateSpatialIndex();

  if (!g_ai_nodes->count) {
    release(g_ai_nodes);
    g_ai_nodes = NULL;
  } else {
    G_Ai_Node_Adjust(id);
  }
}

/**
 * @brief Returns true if the client entity is currently standing on solid ground.
 */
static bool G_Ai_Node_OnGround(const GameClient *cl) {
  const CmTrace tr = gi.Trace(cl->entity->s.origin,
                                 Vec3_Add(cl->entity->s.origin, MakeVec3(0, 0, -PM_GROUND_DIST)),
                                 cl->entity->s.bounds,
                                 NULL,
                                 CONTENTS_MASK_CLIP_CORPSE);

  return tr.fraction < 1.0f && tr.plane.normal.z > PM_STEP_NORMAL;
}

/**
 * @brief Returns the world position of the specified navigation node.
 */
Vec3 G_Ai_Node_GetPosition(const AiNodeId node) {

  return AI_NODE(g_ai_nodes, node)->position;
}

/**
 * @brief Recalculates the traversal costs for all links incident to the given node.
 */
static void G_Ai_Node_UpdateCosts(const AiNodeId id) {

  for (size_t i = 0; i < g_ai_nodes->count; i++) {
    const AiNode *node = AI_NODE(g_ai_nodes, i);

    if (!node->links || !node->links->count) {
      continue;
    }

    for (size_t l = 0; l < node->links->count; l++) {
      AiLink *link = AI_LINK(node->links, l);

      if (id == i || link->id == id) {
        link->cost = G_Ai_Node_Cost(i, link->id);
      }
    }
  }
}

/**
 * @brief Returns false if any `func_plat` covering position is not at its bottom (accessible) state.
 * Bots should not attempt to path to nodes on an elevated platform.
 */
static bool G_Ai_PlatformAccessible(const Vec3 position) {

  G_ForEachEntity(ent, {
    if (!ent->classname || q_strcmp(ent->classname, "func_plat") != 0) {
      continue;
    }

    if (position.x < ent->absBounds.mins.x || position.x > ent->absBounds.maxs.x ||
        position.y < ent->absBounds.mins.y || position.y > ent->absBounds.maxs.y) {
      continue;
    }

    // Platform surface (abs_bounds.maxs.z) must be within reach of the node's Z.
    if (ent->absBounds.maxs.z > position.z + 32.f) {
      return false;
    }
  });

  return true;
}

/**
 * @brief Check if the node we want to move towards is currently pathable.
 */
bool G_Ai_Node_CanPathTo(const Vec3 position) {

  // if we're heading onto a mover node, only allow us to go forth
  // if the mover is there
  const Vec3 end = Vec3_Subtract(position, MakeVec3(0, 0, PM_GROUND_DIST * 3.f));

  // check if the destination has ground
  CmTrace tr = gi.Trace(position, end, Box3_Expand3(G_PlayerBounds(), MakeVec3(1.f, 1.f, 0.f)), NULL, CONTENTS_MASK_CLIP_CORPSE | CONTENTS_MASK_LIQUID);

  // bad ground
  bool stuckInMover = tr.ent
      && (tr.startSolid || tr.allSolid)
      && (((GameEntity *) tr.ent)->s.number != 0
      && !(tr.contents & CONTENTS_MASK_LIQUID));

  if (tr.fraction == 1.0f) {
    // Legitimate drop links may have no immediate floor under the node position.
    // Treat those as pathable and let normal movement/distress logic handle the descent.
    return true;
  }

  if (stuckInMover) {

    // check with a thinner box; it might be a button press or rotating thing
    const Box3 bounds = G_PlayerBounds();
    tr = gi.Trace(position,
               Vec3_Subtract(position, MakeVec3(0, 0, PM_GROUND_DIST * 3.f)),
               MakeBox3(MakeVec3(-4.f, -4.f, bounds.mins.z), MakeVec3(4.f, 4.f, bounds.maxs.z)),
               NULL,
               CONTENTS_MASK_CLIP_CORPSE | CONTENTS_MASK_LIQUID);
    stuckInMover = tr.ent
        && (tr.startSolid || tr.allSolid)
        && (((GameEntity *) tr.ent)->s.number != 0
        && !(tr.contents & CONTENTS_MASK_LIQUID));

    if (!stuckInMover) {
      return true;
    }

    return false;
  }

  return !G_Ai_PlatformAccessible(position) ? false : true;
}

/**
 * @brief Check if the node we want to move towards is currently pathable.
 */
bool G_Ai_Path_CanPathTo(const Vector *path, const uint32_t index) {

  // sanity
  if (index >= path->count) {
    return true;
  }

  // if we're heading onto a mover node, only allow us to go forth
  // if the mover is there
  return G_Ai_Node_CanPathTo(AI_NODE(g_ai_nodes, AI_NODE_ID(path, index))->position);
}

/**
 * @brief The length of space that nodes will drop while walking.
 */
#define WALKING_DISTANCE  128.f

/**
 * @brief The length of space that couldn't be logically travelled by regular means.
 */
#define TELEPORT_DISTANCE  64.f

/**
 * @brief Handles automatic node placement as the player moves through the map during development.
 */
void G_Ai_Node_PlayerRoam(GameClient *cl, const PlayerMoveCmd *cmd) {

  if (!g_ai_node_dev->integer) {
    return;
  }

  GameEntity *ent = cl->entity;

  g_ai_player_roam.oldButtons = g_ai_player_roam.buttons;
  g_ai_player_roam.buttons = cmd->buttons;
  g_ai_player_roam.latchedButtons |= g_ai_player_roam.buttons & ~g_ai_player_roam.oldButtons;
  
  const bool allowAdjustments = g_ai_node_dev->integer == 1;
  const bool doNoding = allowAdjustments && g_ai_player_roam.dropNodes && cl->ps.pmState.type == PM_NORMAL;

  // we just switched between noclip/not noclip, clear some stuff
  // so we don't accidentally drop nodes
  if (g_ai_player_roam.doNoding != doNoding) {
    g_ai_player_roam.doNoding = doNoding;

    g_ai_player_roam.position = ent->s.origin;
    g_ai_player_roam.lastNodes[0] = g_ai_player_roam.lastNodes[1] = AI_NODE_INVALID;
    g_ai_player_roam.awaitLanding = true;
  }

  const bool inWater = gi.PointContents(ent->s.origin) & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA);
  const float lastNodeDistanceCompare = g_ai_player_roam.lastNodes[0] == AI_NODE_INVALID ? FLT_MAX : Vec3_Distance(ent->s.origin, G_Ai_Node_GetPosition(g_ai_player_roam.lastNodes[0]));
  const float playerDistanceCompare = Vec3_Distance(ent->s.origin, g_ai_player_roam.position);

  if (G_Ai_Node_OnGround(cl)) {
    g_ai_player_roam.floorPosition = ent->s.origin;
  }

  if (doNoding) {
    // we're waiting to land to drop a node; we jumped, fell, got sent by a jump pad, something like that.
    if (g_ai_player_roam.awaitLanding) {

      if (G_Ai_Node_OnGround(cl) || inWater) {

        // we landed!
        g_ai_player_roam.awaitLanding = false;
        g_ai_player_roam.position = ent->s.origin;

        AiNodeId landedNearNode = G_Ai_Node_FindClosest(ent->s.origin, WALKING_DISTANCE / 2, true, true);

        if (landedNearNode == AI_NODE_INVALID) {

          landedNearNode = G_Ai_Node_Create(ent->s.origin);
        }

        // one-way node from where we were to here
        if (g_ai_player_roam.lastNodes[0] != AI_NODE_INVALID) {
          bool bidirectional = false;

          if (g_ai_player_roam.isWaterJump) {
            bidirectional = true;
            G_Ai_Debug("Most likely waterjump; connecting both ends\n");
          } else if (g_ai_player_roam.isJumping || inWater) {
            const float zDiff = fabsf(G_Ai_Node_GetPosition(g_ai_player_roam.lastNodes[0]).z - G_Ai_Node_GetPosition(landedNearNode).z);

            if (zDiff < PM_STEP_HEIGHT || (inWater && zDiff < PM_STEP_HEIGHT * 3.f)) {
              bidirectional = true;
              G_Ai_Debug("Most likely jump or drop-into-water link; connecting both ends\n");
            }
          }

          G_Ai_Node_LinkDefault(g_ai_player_roam.lastNodes[0], landedNearNode, bidirectional);
        }

        g_ai_player_roam.lastNodes[1] = g_ai_player_roam.lastNodes[0];
        g_ai_player_roam.lastNodes[0] = landedNearNode;
        g_ai_player_roam.isWaterJump = false;
      }

      return;
    }

    // we probably teleported; no node, just start dropping here when we land
    if (playerDistanceCompare > TELEPORT_DISTANCE) {

      g_ai_player_roam.lastNodes[0] = g_ai_player_roam.lastNodes[1] = AI_NODE_INVALID;
      g_ai_player_roam.position = ent->s.origin;
      g_ai_player_roam.awaitLanding = true;

      G_Ai_Debug("Teleport detected; awaiting landing...\n");
      return;
    }

    // we just left the floor (or water); drop a node here
    if (!G_Ai_Node_OnGround(cl) && !inWater) {
      // for water leavings, we want to drop where we are, not where we went into the water from
      const Vec3 where = g_ai_player_roam.isWaterJump ? ent->s.origin : g_ai_player_roam.floorPosition;
      const AiNodeId jumpedNearNode = G_Ai_Node_FindClosest(where, WALKING_DISTANCE / 2, true, false);
      const bool isJump = cl->ps.pmState.velocity.z > 0;

      if (jumpedNearNode == AI_NODE_INVALID) {
        const AiNodeId id = G_Ai_Node_Create(where);

        if (g_ai_player_roam.lastNodes[0] != AI_NODE_INVALID) {

          G_Ai_Node_LinkDefault(g_ai_player_roam.lastNodes[0], id, true);
        }
      
        g_ai_player_roam.lastNodes[1] = g_ai_player_roam.lastNodes[0];
        g_ai_player_roam.lastNodes[0] = id;
      } else {
        
        if (g_ai_player_roam.lastNodes[0] != AI_NODE_INVALID) {

          G_Ai_Node_LinkDefault(g_ai_player_roam.lastNodes[0], jumpedNearNode, true);
        }
      
        g_ai_player_roam.lastNodes[1] = g_ai_player_roam.lastNodes[0];
        g_ai_player_roam.lastNodes[0] = jumpedNearNode;
      }

      g_ai_player_roam.awaitLanding = true;
      g_ai_player_roam.isJumping = isJump;

      // we didn't leave from water jump, so turn this off.
      if (!isJump) {
        g_ai_player_roam.isWaterJump = false;
      }

      G_Ai_Debug("Left ground; jumping? %s\n", isJump ? "yes" : "nop");
      return;
    }
  }

  // we're walkin'

  const AiNodeId closestNode = G_Ai_Node_FindClosest(ent->s.origin, WALKING_DISTANCE / 4, true, false);
  const bool onMover = ent->ground.ent && ((GameEntity *) ent->ground.ent)->s.number != 0;

  // attack button enables/disables placement
  if (allowAdjustments && (g_ai_player_roam.latchedButtons & BUTTON_ATTACK)) {
    g_ai_player_roam.dropNodes = !g_ai_player_roam.dropNodes;

    g_ai_player_roam.latchedButtons &= ~BUTTON_ATTACK;
  // "use" moves node
  } else if (allowAdjustments && ent->moveNode) {
    if (g_ai_player_roam.lastNodes[0] != AI_NODE_INVALID) {
      AiNode *node = AI_NODE(g_ai_nodes, g_ai_player_roam.lastNodes[0]);
      node->position = ent->s.origin;

      if (cmd->up < 0) {
        const CmTrace tr = gi.Trace(node->position, Vec3_Subtract(node->position, MakeVec3(0.f, 0.f, MAX_WORLD_COORD)), Pm_Bounds(&ent->client->ps.pmState.params, false), ent, CONTENTS_MASK_SOLID);
        node->position = tr.end;
      }

      // recalculate links
      G_Ai_Node_UpdateCosts(g_ai_player_roam.lastNodes[0]);
    }

    ent->moveNode = false;
  // hook destroys node
  } else if (allowAdjustments && (g_ai_player_roam.latchedButtons & BUTTON_HOOK)) {
    if (g_ai_player_roam.lastNodes[0] != AI_NODE_INVALID) {
      G_Ai_Node_Destroy(g_ai_player_roam.lastNodes[0]);
      g_ai_player_roam.position = ent->s.origin;
      g_ai_player_roam.lastNodes[0] = g_ai_player_roam.lastNodes[1] = AI_NODE_INVALID;
      g_ai_player_roam.lastNodes[0] = G_Ai_Node_FindClosest(ent->s.origin, WALKING_DISTANCE * 2.5f, true, false);
    }
    g_ai_player_roam.latchedButtons &= ~BUTTON_HOOK;
  // score adjusts link connections
  } else if (allowAdjustments && (g_ai_player_roam.latchedButtons & BUTTON_SCORE)) {

    if (g_ai_player_roam.lastNodes[1] != AI_NODE_INVALID && g_ai_player_roam.lastNodes[0] != AI_NODE_INVALID) {
      uint8_t bits = 0;

      if (G_Ai_Node_IsLinked(g_ai_player_roam.lastNodes[0], g_ai_player_roam.lastNodes[1])) {
        bits |= 1;
      }
      if (G_Ai_Node_IsLinked(g_ai_player_roam.lastNodes[1], g_ai_player_roam.lastNodes[0])) {
        bits |= 2;
      }

      bits = (bits + 1) % 4;

      G_Ai_Node_Unlink(g_ai_player_roam.lastNodes[0], g_ai_player_roam.lastNodes[1]);
      G_Ai_Node_Unlink(g_ai_player_roam.lastNodes[1], g_ai_player_roam.lastNodes[0]);
      
      if (bits & 1) {
        G_Ai_Node_LinkDefault(g_ai_player_roam.lastNodes[0], g_ai_player_roam.lastNodes[1], false);
      }
      if (bits & 2) {
        G_Ai_Node_LinkDefault(g_ai_player_roam.lastNodes[1], g_ai_player_roam.lastNodes[0], false);
      }
    }
    
    g_ai_player_roam.latchedButtons &= ~BUTTON_SCORE;
  // we're stepping on/off a mover; connect us one-way
  } else if (onMover != g_ai_player_roam.onMover) {

    g_ai_player_roam.onMover = onMover;

    if (doNoding) {
      AiNodeId id = G_Ai_Node_FindClosest(ent->s.origin, WALKING_DISTANCE / 8, true, false);
      
      if (id == AI_NODE_INVALID) {
        id = G_Ai_Node_Create(ent->s.origin);
      }

      if (g_ai_player_roam.lastNodes[0] != AI_NODE_INVALID) {

        G_Ai_Node_LinkDefault(g_ai_player_roam.lastNodes[0], id, false);
      }
    
      g_ai_player_roam.lastNodes[1] = g_ai_player_roam.lastNodes[0];
      g_ai_player_roam.lastNodes[0] = id;
    }

  // if we touched another node and had another node lit up; connect us if we aren't already
  } else if (closestNode != AI_NODE_INVALID && closestNode != g_ai_player_roam.lastNodes[0] && G_Ai_Node_Visible(ent->s.origin, closestNode)) {

    if (doNoding && g_ai_player_roam.lastNodes[0] != AI_NODE_INVALID) {

      G_Ai_Node_LinkDefault(g_ai_player_roam.lastNodes[0], closestNode, !g_ai_player_roam.onMover);
    }
    
    g_ai_player_roam.lastNodes[1] = g_ai_player_roam.lastNodes[0];
    g_ai_player_roam.lastNodes[0] = closestNode;
  // we got far enough from the last node, so drop a new node
  } else if (lastNodeDistanceCompare > WALKING_DISTANCE) {

    if (doNoding) {
      AiNodeId id = G_Ai_Node_FindClosest(ent->s.origin, WALKING_DISTANCE / 2, true, !inWater);
      
      if (id == AI_NODE_INVALID) {
        id = G_Ai_Node_Create(ent->s.origin);
      }

      if (g_ai_player_roam.lastNodes[0] != AI_NODE_INVALID) {

        G_Ai_Node_LinkDefault(g_ai_player_roam.lastNodes[0], id, !g_ai_player_roam.onMover);
      }
    
      g_ai_player_roam.lastNodes[1] = g_ai_player_roam.lastNodes[0];
      g_ai_player_roam.lastNodes[0] = id;
    }
  }

  // we're currently in water; the only way we can get out
  // is by waterjumping, having something take us out of the water,
  // a ladder or walking straight out of water. mark waterjump as true,
  // we'll figure it out when we actually leave water what the intentions are.
  if (inWater) {
    g_ai_player_roam.isWaterJump = true;
  }

  g_ai_player_roam.position = ent->s.origin;
}

/**
 * @brief A compact representation of a directed link pair used for render deduplication.
 */
typedef struct {
  union {
    struct {
      AiNodeId a, b;
    };
    int32_t v;
  };
} AiUniqueLink;

typedef struct {
  AiUniqueLink link;
  int32_t bits;
} AiRenderLink;

/**
 * @brief Renders a single node link line for developer visualization.
 */
static void G_Ai_Node_RenderLink(const AiUniqueLink ulink, const int32_t bits) {
  
  const AiNode *nodeA = AI_NODE(g_ai_nodes, ulink.a);
  const AiNode *nodeB = AI_NODE(g_ai_nodes, ulink.b);

  GameClient *client = NULL;
  G_ForEachClient(cl, {
    if (!cl->ai) {
      client = cl;
      break;
    }
  });

  assert(client);
  GameEntity *ent = client->entity;

  if (!G_Ai_Node_Visible(Vec3_Add(ent->s.origin, client->ps.pmState.viewOffset), ulink.a)
      && !G_Ai_Node_Visible(Vec3_Add(ent->s.origin,client->ps.pmState.viewOffset), ulink.b)) {
    return;
  }

  gi.WriteByte(SV_CMD_TEMP_ENTITY);
  gi.WriteByte(TE_AI_NODE_LINK);
  gi.WritePosition(nodeA->position);
  gi.WritePosition(nodeB->position);
  gi.WriteByte(bits);
  gi.Multicast(nodeA->position, MULTICAST_PVS);
}

/**
 * @brief Returns true if the specified node ID is present in the given path array.
 */
static bool G_Ai_NodeInPath(Vector *path, AiNodeId node) {

  if (!path) {
    return false;
  }

  for (size_t i = 0; i < path->count; i++) {
    if (AI_NODE_ID(path, i) == node) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Renders all navigation nodes and their links for developer visualization.
 */
void G_Ai_Node_Render(void) {

  if (!g_ai_node_dev->integer) {
    return;
  }

  if (!g_ai_nodes) {
    return;
  }

  GameClient *client = NULL;
  G_ForEachClient(cl, {
    if (cl->entity && !cl->ai) {
      client = cl;
      break;
    }
  });

  if (!client) { // probably just hasn't spawned yet
    return;
  }

  GameEntity *ent = client->entity;

  Vector *uniqueLinks = $(alloc(Vector), initWithSize, sizeof(AiRenderLink));

  for (uint32_t i = 0; i < g_ai_nodes->count; i++) {
    const AiNode *node = AI_NODE(g_ai_nodes, i);
    const bool inPath = G_Ai_NodeInPath(g_ai_player_roam.testPath, i);

    if (G_Ai_Node_Visible(Vec3_Add(ent->s.origin, client->ps.pmState.viewOffset), i)) {

      gi.WriteByte(SV_CMD_TEMP_ENTITY);
      gi.WriteByte(TE_AI_NODE);
      gi.WritePosition(node->position);
      gi.WriteShort(i);

      byte bits = 0;

      if (inPath) {
        bits = 3;
      } else if (g_ai_player_roam.lastNodes[0] == i) {
        bits = 1;
      } else if (g_ai_player_roam.lastNodes[1] == i) {
        bits = 2;
      }

      if (!G_Ai_Node_CanPathTo(node->position)) {
        bits |= 16;
      }

      gi.WriteByte(bits);
      gi.Multicast(node->position, MULTICAST_PVS);
    }

    if (node->links) {

      for (size_t l = 0; l < node->links->count; l++) {
        const AiLink *link = AI_LINK(node->links, l);
        AiUniqueLink ulink;
        int32_t bit;

        if (link->id > i) {
          ulink.a = link->id;
          ulink.b = i;
          bit = 1;
        } else {
          ulink.a = i;
          ulink.b = link->id;
          bit = 2;
        }

        if (!inPath) {
          bit |= 4;
        }

        if (G_Ai_ShouldSlowDrop(i, link->id)) {
          bit |= 16;
        }

        bool found = false;

        for (uint32_t u = 0; u < uniqueLinks->count; u++) {
          AiRenderLink *renderLink = VectorElement(uniqueLinks, AiRenderLink, u);
          if (renderLink->link.v == ulink.v) {
            renderLink->bits |= bit;
            found = true;
            break;
          }
        }

        if (!found) {
          AiRenderLink renderLink = {
            .link = ulink,
            .bits = bit
          };
          $(uniqueLinks, add, &renderLink);
        }
      }
    }
  }

  for (size_t i = 0; i < uniqueLinks->count; i++) {
    const AiRenderLink *renderLink = VectorElement(uniqueLinks, AiRenderLink, i);
    G_Ai_Node_RenderLink(renderLink->link, renderLink->bits);
  }

  release(uniqueLinks);

  // draw the bots' targets
  G_ForEachClient(cl, {

    if (!cl->ai) {
      continue;
    }


    if (cl->ai->moveTarget.type != AI_GOAL_PATH) {
      continue;
    }

    gi.WriteByte(SV_CMD_TEMP_ENTITY);
    gi.WriteByte(TE_AI_NODE_LINK);
    gi.WritePosition(cl->entity->s.origin);
    gi.WritePosition(cl->ai->moveTarget.path.pathPosition);
    gi.WriteByte(8);
    gi.Multicast(cl->entity->s.origin, MULTICAST_PVS);
  });
}

#define AI_NODE_MAGIC ('Q' | '2' << 8 | 'N' << 16 | 'S' << 24)
#define AI_NODE_VERSION 2

_Static_assert(sizeof(AiLink) == 8, "AiLink is the on-disk link record; changing it requires a new AI_NODE_VERSION");

/**
 * @brief Reads the nodes and links from an open .nav file into `g_ai_nodes`.
 * @return False if the file is malformed, in which case the nodes read so far
 * must be discarded.
 */
static bool G_Ai_ReadNodes(File *file) {
  int32_t magic, version;

  if (gi.ReadFile(file, &magic, sizeof(magic), 1) != 1 || magic != AI_NODE_MAGIC) {
    G_Warn("Nav file invalid format!\n");
    return false;
  }

  if (gi.ReadFile(file, &version, sizeof(version), 1) != 1 || version != AI_NODE_VERSION) {
    G_Warn("Nav file out of date!\n");
    return false;
  }

  uint32_t numNodes;
  if (gi.ReadFile(file, &numNodes, sizeof(numNodes), 1) != 1 || numNodes >= AI_NODE_INVALID) {
    G_Warn("Nav file has an invalid node count\n");
    return false;
  }

  g_ai_nodes = $(alloc(Vector), initWithSize, sizeof(AiNode));

  for (size_t i = 0; i < numNodes; i++) {
    AiNode node = { 0 };
    $(g_ai_nodes, add, &node);
  }

  G_Ai_Node_InvalidateSpatialIndex();

  size_t totalLinks = 0;

  for (size_t i = 0; i < g_ai_nodes->count; i++) {
    AiNode *node = AI_NODE(g_ai_nodes, i);

    if (gi.ReadFile(file, &node->position, sizeof(node->position), 1) != 1) {
      G_Warn("Nav file is truncated at node %zu\n", i);
      return false;
    }

    uint32_t numLinks;
    if (gi.ReadFile(file, &numLinks, sizeof(numLinks), 1) != 1 || numLinks > numNodes) {
      G_Warn("Nav file has an invalid link count at node %zu\n", i);
      return false;
    }

    if (numLinks) {
      node->links = $(alloc(Vector), initWithSize, sizeof(AiLink));

      for (size_t l = 0; l < numLinks; l++) {
        AiLink link;

        if (gi.ReadFile(file, &link, sizeof(link), 1) != 1) {
          G_Warn("Nav file is truncated at node %zu\n", i);
          return false;
        }

        if (link.id >= numNodes) {
          G_Warn("Nav file links node %zu to nonexistent node %u\n", i, link.id);
          return false;
        }

        $(node->links, add, &link);
      }

      totalLinks += numLinks;

      G_Ai_Node_Unlink(i, i);
    } else {
      node->links = NULL;
    }
  }

  gi.Print("  Loaded %u nodes with %zu total links.\n", numNodes, totalLinks);

  return true;
}

/**
 * @brief Initializes the navigation node system and loads the .nav file for the current map.
 */
void G_Ai_InitNodes(void) {

  G_Ai_ShutdownNodes();

  g_ai_player_roam.position = MakeVec3(MAX_WORLD_DIST, MAX_WORLD_DIST, MAX_WORLD_DIST);
  g_ai_player_roam.lastNodes[0] = g_ai_player_roam.lastNodes[1] = AI_NODE_INVALID;
  g_ai_player_roam.awaitLanding = true;

  char filename[MAX_OS_PATH];

  q_snprintf(filename, sizeof(filename), "maps/%s.nav", g_level.name);

  if (!gi.FileExists(filename)) {
    G_Warn("No navigation file exists for this map; bots will be dumb!\nUse `g_ai_node_dev` to set up nodes.\n");
    return;
  }

  File *file = gi.OpenFile(filename);
  if (!file) {
    G_Warn("Failed to open %s\n", filename);
    return;
  }

  const bool loaded = G_Ai_ReadNodes(file);

  gi.CloseFile(file);

  if (!loaded) {
    G_Ai_ShutdownNodes();
    return;
  }

  g_ai_player_roam.fileNodes = (uint32_t) g_ai_nodes->count;
  g_ai_player_roam.fileLinks = 0;

  for (size_t i = 0; i < g_ai_nodes->count; i++) {
    const AiNode *node = AI_NODE(g_ai_nodes, i);

    if (node->links) {
      g_ai_player_roam.fileLinks += (uint32_t) node->links->count;
    }
  }
}

/**
 * @brief Validates node integrity: warns about item entities with no nearby node and nodes inside solid.
 */
static void G_Ai_CheckNodes(void) {

  if (g_ai_node_dev->integer) {
    G_ForEachEntity(ent, {

      // only warn for item nodes
      if (!ent->item) {
        continue;
      }

      AiNodeId node = G_Ai_Node_FindClosest(ent->s.origin, WALKING_DISTANCE * 2.5f, true, false);

      if (node == AI_NODE_INVALID) {
        G_Warn("Entity %s @ %s appears to be unreachable by nodes\n", ent->classname, vtos(ent->s.origin));
      }
    });
  }

  for (size_t i = 0; i < g_ai_nodes->count; i++) {
    const AiNode *node = AI_NODE(g_ai_nodes, i);
    
    if (gi.PointContents(node->position) & CONTENTS_MASK_SOLID) {
      G_Warn("Node %zu @ %s is inside of solid\n", i, vtos(node->position));
    }
  }
}

/**
 * @brief Called after the first second of gameplay to allow the AI to pick up nodes
 * from late-spawning entities.
 */
void G_Ai_NodesReady(void) {

  if (!g_ai_nodes) {
    return;
  }

  const size_t addedNodes = g_ai_nodes->count - g_ai_player_roam.fileNodes;
  size_t addedLinks = 0;

  for (size_t i = 0; i < g_ai_nodes->count; i++) {
    const AiNode *node = AI_NODE(g_ai_nodes, i);

    if (node->links) {
      addedLinks += node->links->count;
    }
  }

  addedLinks -= g_ai_player_roam.fileLinks;
  gi.Print("  Game loaded %zu additional nodes with %zu new links.\n", addedNodes, addedLinks);

  G_ForEachEntity(ent, {
    if (ent->classname && q_strcmp(ent->classname, "func_plat") == 0) {
      if (!g_ai_platforms) {
        g_ai_platforms = $(alloc(Vector), initWithSize, sizeof(GameEntity *));
      }
      $(g_ai_platforms, add, &ent);
    }
  });

  /*if (g_ai_node_dev->integer != 1) {
    const uint32_t optimized = Ai_OptimizeNodes();
    gi.Print("  %u nodes optimized\n", optimized);
  }*/

  G_Ai_CheckNodes();
}

/**
 * @brief Serializes all navigation nodes and their links to the current map's .nav file.
 */
void G_Ai_SaveNodes(void) {

  if (g_ai_node_dev->integer != 1) {
    G_Warn("This command only works with `g_ai_node_dev` set to 1.\n");
    return;
  }

  char filename[MAX_OS_PATH];

  q_snprintf(filename, sizeof(filename), "maps/%s.nav", g_level.name);

  if (!g_ai_nodes) {
    G_Warn("No nodes to write.\n");
    return;
  }

  File *file = gi.OpenFileWrite(filename);
  int32_t magic = AI_NODE_MAGIC;
  int32_t version = AI_NODE_VERSION;
  
  gi.WriteFile(file, &magic, sizeof(magic), 1);
  gi.WriteFile(file, &version, sizeof(version), 1);

  const uint32_t numNodes = (uint32_t) g_ai_nodes->count;
  gi.WriteFile(file, &numNodes, sizeof(numNodes), 1);

  for (size_t i = 0; i < g_ai_nodes->count; i++) {
    const AiNode *node = AI_NODE(g_ai_nodes, i);

    gi.WriteFile(file, &node->position, sizeof(node->position), 1);

    if (node->links) {
      const uint32_t numLinks = (uint32_t) node->links->count;
      gi.WriteFile(file, &numLinks, sizeof(numLinks), 1);
      gi.WriteFile(file, node->links->elements, sizeof(AiLink), node->links->count);
    } else {
      uint32_t len = 0;
      gi.WriteFile(file, &len, sizeof(len), 1);
    }
  }

  gi.CloseFile(file);

  gi.Print("Wrote nodes to %s.\n", gi.RealPath(filename));

  G_Ai_CheckNodes();
}

/**
 * @brief Clears all navigation nodes, freeing their link arrays but retaining the backing array.
 */
void G_Ai_DeleteNodes(void) {

  if (g_ai_nodes) {
    for (uint32_t i = 0; i < g_ai_nodes->count; i++) {
      AiNode *node = AI_NODE(g_ai_nodes, i);

      if (node->links) {
        release(node->links);
      }
    }

    $(g_ai_nodes, removeAll);
  }

  g_ai_platforms = release(g_ai_platforms);

  G_Ai_Node_InvalidateSpatialIndex();
  G_Ai_Node_FreePathPool();
}

/**
 * @brief Frees all navigation node data, including the backing node array.
 */
void G_Ai_ShutdownNodes(void) {

  if (g_ai_nodes) {
    for (size_t i = 0; i < g_ai_nodes->count; i++) {
      AiNode *node = AI_NODE(g_ai_nodes, i);

      if (node->links) {
        release(node->links);
      }
    }

    g_ai_nodes = release(g_ai_nodes);
  }

  g_ai_platforms = release(g_ai_platforms);

  G_Ai_Node_InvalidateSpatialIndex();
  G_Ai_Node_FreePathPool();
}

typedef struct {
  AiNodeId id;
  float priority;
} AiNodePriority;

static struct GHeap *g_ai_node_path_queue;
static AiNodePriority *g_ai_node_path_entries;
static size_t g_ai_node_path_capacity;
static size_t g_ai_node_path_count;

/**
 * @brief Releases the pathfinding queue and its entries.
 */
static void G_Ai_Node_FreePathPool(void) {

  gheap_free(&g_ai_node_path_queue);
  free(g_ai_node_path_entries);
  g_ai_node_path_entries = NULL;
  g_ai_node_path_capacity = 0;
  g_ai_node_path_count = 0;
}

/**
 * @brief Readies the pathfinding queue and entry pool for `capacity` nodes, reusing them when they suffice.
 */
static bool G_Ai_Node_EnsurePathPool(const size_t capacity) {

  if (g_ai_node_path_queue && g_ai_node_path_capacity >= capacity) {
    gheap_reset(g_ai_node_path_queue);
    g_ai_node_path_count = 0;
    return true;
  }

  G_Ai_Node_FreePathPool();

  g_ai_node_path_queue = gheap_create(capacity);
  g_ai_node_path_entries = malloc(sizeof(AiNodePriority) * capacity);

  if (!g_ai_node_path_queue || !g_ai_node_path_entries) {
    G_Ai_Node_FreePathPool();
    return false;
  }

  g_ai_node_path_capacity = capacity;
  return true;
}

/**
 * @brief The next free pathfinding entry, or `NULL` when the pool is spent.
 */
static AiNodePriority *G_Ai_Node_AllocPathEntry(void) {

  if (g_ai_node_path_count == g_ai_node_path_capacity) {
    return NULL;
  }

  return &g_ai_node_path_entries[g_ai_node_path_count++];
}

#define AI_MAX_DROP_HEIGHT 512.f
#define AI_DROP_PENALTY_START 128.f
#define AI_DROP_PENALTY_SCALE 0.5f
#define AI_DROP_HEALTH_MARGIN 8.f
#define AI_DROP_DAMAGE_PENALTY_SCALE 6.f

/**
 * @brief The stored cost of the link from `a` to `b`.
 */
static inline float G_Ai_LinkCost(const AiNodeId a, const AiNodeId b) {
  const AiNode *node = AI_NODE(g_ai_nodes, a);

  assert(node->links);

  for (uint32_t i = 0; i < node->links->count; i++) {
    const AiLink *link = AI_LINK(node->links, i);

    if (link->id == b) {
      return link->cost;
    }
  }

  assert(false);
  return -1;
}

/**
 * @brief Returns true if the segment between two nodes intersects lava or slime.
 */
static bool G_Ai_LinkPassesHazard(const Vec3 from, const Vec3 to) {
  const Vec3 delta = Vec3_Subtract(to, from);
  const float length = Vec3_Length(delta);

  if (length <= 0.f) {
    return (gi.PointContents(from) & (CONTENTS_LAVA | CONTENTS_SLIME)) != 0;
  }

  const Vec3 dir = Vec3_Scale(delta, 1.f / length);
  const float step = 24.f;
  const int32_t samples = Maxi(1, (int32_t) ceilf(length / step));

  for (int32_t i = 0; i <= samples; i++) {
    const float dist = Minf(length, i * step);
    const Vec3 point = Vec3_Fmaf(from, dist, dir);

    if (gi.PointContents(point) & (CONTENTS_LAVA | CONTENTS_SLIME)) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Finds the shortest path between two nodes using the A* algorithm.
 */
static float G_Ai_EstimatedFallDamage(const float drop, const int32_t gravity, const int32_t waterLevel) {

  if (drop <= 0.f || gravity <= 0) {
    return 0.f;
  }

  const float impactVelocity = -sqrtf(2.f * gravity * drop);
  if (impactVelocity > PM_SPEED_FALL) {
    return 0.f;
  }

  float damage = -((impactVelocity - PM_SPEED_FALL) * 0.05f);

  if (waterLevel > 0) {
    damage /= (float) (1u << Mini(waterLevel, 3));
  }

  return Maxf(0.f, damage);
}

/**
 * @see g_ai_node.h
 */
Vector *G_Ai_Node_FindPath(const GameClient *cl, const AiNodeId start, const AiNodeId end, const G_Ai_NodeCostFunc heuristic, float *length) {
  
  if (length) {
    *length = 0;
  }

  // sanity
  if (start == AI_NODE_INVALID || end == AI_NODE_INVALID) {
    return NULL;
  }
  
  // Pre-collect func_plat entities once so G_Ai_PlatformAccessible doesn't
  // call G_ForEachEntity for every link expansion inside the A* loop.

  // size on 64k nodes is, say, 8kb.
  const size_t costsStartedWords = ((size_t) g_ai_nodes->count + 31u) / 32u;
  uint32_t *costsStarted = calloc(costsStartedWords, sizeof(*costsStarted));
  if (!costsStarted) {
    return NULL;
  }
  uint32_t visited = 0;
  // Min-heap open set (priority = f-cost). Replaces the previous sorted-array
  // queue (O(n) insertion) with an O(log n) binary heap from g_ai_grid.c.
  // Capacity is bounded by the node count plus headroom for re-insertions
  // (A* may push a better-cost duplicate before popping the stale one).
  const size_t heapCapacity = (size_t) g_ai_nodes->count * 4 + 16;
  if (!G_Ai_Node_EnsurePathPool(heapCapacity)) {
    free(costsStarted);
    return NULL;
  }

  struct GHeap *queue = g_ai_node_path_queue;
  bool finished = false;

  {
    AiNodePriority *e = G_Ai_Node_AllocPathEntry();
    e->id = start;
    e->priority = 0;
    gheap_push(queue, e->priority, e);
  }

  AiNode *startNode = AI_NODE(g_ai_nodes, start);
  startNode->cost = 0;
  costsStarted[start / 32] |= (uint32_t)1 << (start % 32);
  visited++;

  for (;;) {
    AiNodePriority *current = (AiNodePriority *) gheap_pop(queue);
    if (!current) {
      break;
    }

    if (current->id == end) {
      finished = true;
      break;
    }

    AiNode *node = AI_NODE(g_ai_nodes, current->id);

    // Stale entry guard: if this entry's priority is worse than the node's
    // best-known f-cost approximation (cost + 0 heuristic lower bound), the
    // node has already been processed via a better path.
    if (current->priority > node->cost + heuristic(current->id, end) + 0.0001f) {
      continue;
    }

    if (!node->links || !node->links->count) {
      continue;
    }

    // Compute once per expanded node rather than once per link.
    const int32_t nodeContents = gi.PointContents(node->position);
    const bool fromHazard = (nodeContents & (CONTENTS_LAVA | CONTENTS_SLIME)) != 0;

    for (size_t i = 0; i < node->links->count; i++) {
      const AiLink *link = AI_LINK(node->links, i);
      AiNode *linkNode = AI_NODE(g_ai_nodes, link->id);
      const float drop = node->position.z - linkNode->position.z;
      const int32_t linkContents = gi.PointContents(linkNode->position);
      const bool toHazard = (linkContents & (CONTENTS_LAVA | CONTENTS_SLIME)) != 0;

      // Check platform accessibility using the pre-collected list.
      if (g_ai_platforms) {
        bool blocked = false;
        for (size_t p = 0; p < g_ai_platforms->count; p++) {
          const GameEntity *plat = AI_PLATFORM(g_ai_platforms, p);
          if (linkNode->position.x < plat->absBounds.mins.x || linkNode->position.x > plat->absBounds.maxs.x ||
              linkNode->position.y < plat->absBounds.mins.y || linkNode->position.y > plat->absBounds.maxs.y) {
            continue;
          }
          if (plat->absBounds.maxs.z > linkNode->position.z + 32.f) {
            blocked = true;
            break;
          }
        }
        if (blocked) {
          continue;
        }
      }

      if (fromHazard && toHazard) {
        continue;
      }

      if (!fromHazard && G_Ai_LinkPassesHazard(node->position, linkNode->position)) {
        continue;
      }

      if (drop > AI_MAX_DROP_HEIGHT) {
        continue;
      }

      if (drop > 0.f && (linkContents & (CONTENTS_LAVA | CONTENTS_SLIME))) {
        continue;
      }

      float dropPenalty = 0.f;
      if (drop > AI_DROP_PENALTY_START) {
        dropPenalty = (drop - AI_DROP_PENALTY_START) * AI_DROP_PENALTY_SCALE;
      }

      if (cl && cl->entity) {
        const int32_t waterLevel = (linkContents & CONTENTS_WATER) ? 1 : 0;
        const float estimatedDamage = G_Ai_EstimatedFallDamage(drop, G_LevelGravity(), waterLevel);

        if (estimatedDamage + AI_DROP_HEALTH_MARGIN >= cl->entity->health) {
          continue;
        }

        dropPenalty += estimatedDamage * AI_DROP_DAMAGE_PENALTY_SCALE;
      }

      const float newCost = node->cost + link->cost + dropPenalty;

      AiNodeId linkIndex = G_Ai_Node_Index(linkNode);
      const bool found = (costsStarted[linkIndex / 32] & ((uint32_t) 1u << (linkIndex % 32))) != 0;
      if (!found) {
        costsStarted[linkIndex / 32] |= (uint32_t) 1u << (linkIndex % 32);
        visited++;
      }
      if (!found || newCost < linkNode->cost) {

        linkNode->cost = newCost;
        const float priority = newCost + heuristic(link->id, end);

        AiNodePriority *e = G_Ai_Node_AllocPathEntry();
        if (!e) {
          G_Warn("A* open-set entry pool exhausted (capacity %zu)\n", heapCapacity);
          break;
        }

        e->id = link->id;
        e->priority = priority;
        // If the heap is somehow full (large maps with many revisits),
        // fall back to allocating a larger one is overkill; we simply
        // log and stop expanding from this node. The capacity heuristic
        // above (4 * N + 16) is generous for typical Quetoo maps.
        if (!gheap_push(queue, priority, e)) {
          G_Warn("A* open-set heap exhausted (capacity %zu)\n", heapCapacity);
          break;
        }

        linkNode->cameFrom = G_Ai_Node_Index(node);
      }
    }
  }

  Vector *returnPath = NULL;

  if (finished) {
    G_Ai_Debug("Found path from %u -> %u with %u nodes visited\n", start, end, visited);

    returnPath = $(alloc(Vector), initWithSize, sizeof(AiNodeId));
    $(returnPath, insert, (void *) &end, 0);

    if (start != end) {
      AiNodeId from = end;

      for (;;) {
        const AiNode *fromNode = AI_NODE(g_ai_nodes, from);
        from = fromNode->cameFrom;
        $(returnPath, insert, &from, 0);

        if (from == start) {
          break;
        }
      }

      if (length) {
        for (size_t i = 0; i < returnPath->count - 1; i++) {
          const AiNodeId a = AI_NODE_ID(returnPath, i);
          const AiNodeId b = AI_NODE_ID(returnPath, i + 1);

          *length += G_Ai_LinkCost(a, b);
        }
      }
    }
  } else {
    G_Ai_Debug("Couldn't find path from %u -> %u\n", start, end);
  }

  free(costsStarted);
  return returnPath;
}

/**
 * @brief Translates every node by the given offset, for repairing a graph after a map shifts.
 */
void G_Ai_OffsetNodes_f(void) {

  Vec3 translate;

  if (gi.Argc() <= 1) {
    if (g_ai_player_roam.lastNodes[0] == AI_NODE_INVALID) {
      return;
    }

    const Vec3 node = G_Ai_Node_GetPosition(g_ai_player_roam.lastNodes[0]);
    const Vec3 playerPosition = g_ai_player_roam.position;
    translate = Vec3_Subtract(playerPosition, node);
  } else {
    const char *offset = gi.Argv(1);

    if (Parse_QuickPrimitive(offset, PARSER_DEFAULT, PARSE_DEFAULT, PARSE_FLOAT, &translate, 3) != 3) {
      return;
    }
  }

  for (uint32_t i = 0; i < g_ai_nodes->count; i++) {
    AiNode *node = AI_NODE(g_ai_nodes, i);
    node->position = Vec3_Add(node->position, translate);
  }

  G_Ai_Node_InvalidateSpatialIndex();
}

/**
 * @brief Drops a node on top of this object and connects it to any nearby
 * nodes.
 */
bool G_Ai_DropItemLikeNode(GameEntity *ent) {

  if (G_Ai_InDeveloperMode()) {
    return false;
  }

  // find node closest to us
  const AiNodeId srcNode = G_Ai_Node_FindClosest(ent->s.origin, 512.f, true, true);

  if (srcNode == AI_NODE_INVALID) {
    return false;
  }

  // make a new node on the item
  CmTrace down = gi.Trace(ent->s.origin, Vec3_Subtract(ent->s.origin, MakeVec3(0, 0, MAX_WORLD_COORD)), Box3_Zero(), NULL, CONTENTS_MASK_SOLID);
  Vec3 pos;

  if (down.fraction == 1.0) {
    pos = ent->s.origin;
  } else {
    pos = Vec3_Subtract(down.end, MakeVec3(0.f, 0.f, G_PlayerBounds().mins.z));
  }

  // grab all the links of the node that brought us here
  const Vector *srcLinks = G_Ai_Node_GetLinks(srcNode);

  const AiNodeId newNode = G_Ai_Node_Create(pos);
  const float dist = Vec3_Distance(G_Ai_Node_GetPosition(srcNode), ent->s.origin);

  // bidirectionally connect us to source
  G_Ai_Node_Link(srcNode, newNode, dist);
  G_Ai_Node_Link(newNode, srcNode, dist);

  // if we had source links, link any connected bi-directional nodes
  // to the item as well
  if (srcLinks) {

    for (size_t i = 0; i < srcLinks->count; i++) {
      const AiLink *link = AI_LINK(srcLinks, i);

      // not bidirectional
      if (!G_Ai_Node_IsLinked(link->id, srcNode)) {
        continue;
      }

      const Vec3 linkPos = G_Ai_Node_GetPosition(link->id);

      // can't see
      if (gi.Trace(ent->s.origin, linkPos, Box3_Zero(), NULL, CONTENTS_MASK_SOLID).fraction < 1.0) {
        continue;
      }

      const float dist = Vec3_Distance(linkPos, ent->s.origin);

      // bidirectionally connect us to source
      G_Ai_Node_Link(link->id, newNode, dist);
      G_Ai_Node_Link(newNode, link->id, dist);
    }
  }

  ent->node = newNode;
  return true;
}
