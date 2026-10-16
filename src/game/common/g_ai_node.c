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

#include "g_local.h"
#include "bg_pmove.h"

/**
 * @brief The `GameAiNode` at `index` of the Vector of nodes.
 */
#define AI_NODE(vector, index) VectorElement((vector), GameAiNode, (index))

/**
 * @brief The `GameAiLink` at `index` of a Vector of links.
 */
#define AI_LINK(vector, index) VectorElement((vector), GameAiLink, (index))

/**
 * @brief The `GameAiNodeId` at `index` of a Vector of node IDs.
 */
#define AI_NODE_ID(vector, index) (VectorValue((vector), GameAiNodeId, (index)))

/**
 * @brief The `func_plat` entity at `index` of the Vector of platforms.
 */
#define AI_PLATFORM(vector, index) (VectorValue((vector), GameEntity *, (index)))

/**
 * @brief The contents that hurt a player who stands in them: lava and slime.
 */
#define AI_CONTENTS_HAZARD (CONTENTS_LAVA | CONTENTS_SLIME)

/**
 * @brief The most rotating models that do damage, whose sweep the bots wait out.
 */
#define AI_MAX_CRUSHERS 8

/**
 * @brief A `func_rotating` that turns about the vertical axis and does damage: its sweep around its
 * axis, and the arc of its floor, in the model's space.
 */
typedef struct {
  /**
   * @brief The entity.
   */
  const GameEntity *ent;

  /**
   * @brief The center of the arc that the model covers, in degrees about its axis.
   */
  float center;

  /**
   * @brief Half the width of that arc, in degrees, with `AI_CRUSHER_MARGIN`.
   */
  float halfWidth;
} GameAiCrusher;

/**
 * @brief An entry of the pathfinding queue: a node that a search reached, at a cost.
 */
typedef struct {
  /**
   * @brief The node.
   */
  GameAiNodeId id;

  /**
   * @brief The cost of the path to the node, by which the queue orders its entries.
   */
  float priority;

  /**
   * @brief For `G_Ai_Node_GateCosts`, true if the path crossed the doorway.
   */
  bool crossed;
} GameAiNodePriority;

/**
 * @brief The navigation graph state.
 */
static struct {
  /**
   * @brief The navigation nodes of the level, as `GameAiNode`.
   */
  Vector *nodes;

  /**
   * @brief The `func_plat` entities of the level, for `G_Ai_Node_RaisedPlatform`.
   */
  Vector *platforms;

  /**
   * @brief The k-d tree over the positions of `nodes`, for `G_Ai_Node_FindClosest` beyond the
   * cells around a position. It is built when a query first needs it, and again when nodes are
   * added. The nodes on models that move do not rebuild it as they move: a query skips them in the
   * tree, and compares them one by one where they stand.
   */
  struct GameAiKdTree *kdTree;

  /**
   * @brief The node positions over which `kdTree` was built, which it references.
   */
  Vec3 *kdTreePositions;

  /**
   * @brief The count of nodes over which `kdTree` was built.
   */
  size_t kdTreeCount;

  /**
   * @brief The priority queue of the path searches.
   */
  struct GameAiHeap *pathQueue;

  /**
   * @brief The entries that `pathQueue` holds, allocated in order from a pool of `pathCapacity`.
   */
  GameAiNodePriority *pathEntries;

  /**
   * @brief The capacity of `pathQueue` and `pathEntries`.
   */
  size_t pathCapacity;

  /**
   * @brief The count of `pathEntries` that the current search has used.
   */
  size_t pathCount;

  /**
   * @brief The graph that `Nav_Build` made for the level.
   */
  NavGraph graph;

  /**
   * @brief The movement for which `Nav_Build` made the graph.
   */
  PMoveParams movement;

  /**
   * @brief The count of nodes and links that the graph had, before the game added its own.
   */
  uint32_t graphNodes, graphLinks;

  /**
   * @brief The nodes on models that move, which `G_Ai_Node_UpdateMovers` moves and links.
   */
  Vector *moverNodes;

  /**
   * @brief The nodes that do not move, which `G_Ai_Node_UpdateMovers` linked to a node that moves.
   */
  Vector *moverLinked;

  /**
   * @brief Level time at which `G_Ai_Node_UpdateMovers` next links the nodes that move.
   */
  uint32_t moverLinkTime;

  /**
   * @brief True if a node that moves has moved since `G_Ai_Node_UpdateMovers` last linked them.
   */
  bool moverMoved;

  /**
   * @brief The rotating models that do damage.
   */
  GameAiCrusher crushers[AI_MAX_CRUSHERS];

  /**
   * @brief The count of `crushers`.
   */
  int32_t numCrushers;

  /**
   * @brief The door entity of each inline model, for the links through its doorway.
   */
  const GameEntity *doors[MAX_MODELS];

  /**
   * @brief For `G_Ai_Node_GateCosts`, the travel cost of each node on a path that has not yet
   * crossed the doors, then of each node on a path that has.
   */
  float *pressCosts;

  /**
   * @brief For `G_Ai_Node_GateCosts`, the cost of the source of the path of each entry of
   * `pressCosts`, from which the window of the path is measured.
   */
  float *pressStarts;

  /**
   * @brief For `G_Ai_Node_GateCosts`, the source of the path of each entry of `pressCosts`.
   */
  GameAiNodeId *pressSources;

  /**
   * @brief The number of nodes that the arrays of `G_Ai_Node_GateCosts` hold.
   */
  uint32_t pressCount;

  /**
   * @brief True while the search of a client in lava, from a node in it, runs, which MAY cross the
   * lava to leave.
   */
  bool escape;
} module;

/**
 * @brief Frees the k-d tree, so that the next query builds it again over the nodes as they are.
 */
static void G_Ai_Node_InvalidateSpatialIndex(void) {

  if (module.kdTree) {
    G_Ai_KdTreeFree(&module.kdTree);
  }

  free(module.kdTreePositions);
  module.kdTreePositions = NULL;
  module.kdTreeCount = 0;
}

/**
 * @brief An AI navigation node with world position, outgoing links, and pathfinding state.
 */
typedef struct {
  /**
   * @brief The player origin at the node, in world space.
   */
  Vec3 position;

  /**
   * @brief The links out of the node, as `GameAiLink`.
   */
  Vector *links;

  /**
   * @brief The cell of the node in the graph that `Nav_Build` made, or -1 for a node that the
   * game made, such as an item's.
   */
  int32_t cell;

  /**
   * @brief The liquid contents at the node.
   */
  int32_t contents;

  /**
   * @brief True if the node stands at the edge of the void, as `NavNode::edge`.
   */
  bool edge;

  /**
   * @brief True if only a crouched player fits at the node, as `NavNode::crouch`.
   */
  bool crouch;

  /**
   * @brief The inline model that the node stands on and that moves it, or 0.
   */
  int32_t model;

  /**
   * @brief The node's position in the space of `model`.
   */
  Vec3 local;

  /**
   * @brief The entity of `model`, which moves the node, once the level's entities spawn.
   */
  const GameEntity *mover;

  /**
   * @brief The links that `G_Ai_Node_LinkMover` last made for the node on a model that moves, to
   * and from the nodes that do not move, as `GameAiMoverLink`.
   */
  Vector *moverLinks;

  /**
   * @brief The position at which `G_Ai_Node_LinkMover` made `moverLinks`.
   */
  Vec3 moverLinksPosition;

  /**
   * @brief The level time at which `G_Ai_Node_LinkMover` made `moverLinks`.
   */
  uint32_t moverLinksTime;

  /**
   * @brief True if the node has stood at `moverLinksPosition` for less than
   * `AI_MOVER_RELINK_MILLIS`, and no model near it has moved, so that its links of the last relink
   * still hold.
   */
  bool moverStill;

  /**
   * @brief The links that `G_Ai_Node_LinkMovers` last made from the node to the nodes on other
   * models that move, after it in `moverNodes`, as `GameAiMoverLink`, each also made back.
   */
  Vector *moverPairs;

  /**
   * @brief The links from the node that exist only while a door that a button moves stands where the
   * press moves it, as `GameAiLink` with the door's inline model as `gate`. `G_Ai_Node_LinkPresses`
   * makes them once, and only `G_Ai_Node_GateCosts` follows them, at the cost that the linking test
   * gave, without `G_Ai_Node_StepCost`.
   */
  Vector *pressLinks;

  /**
   * @brief The index plus one of the crusher whose sweep the node lies in, or 0.
   */
  int32_t crusher;

  /**
   * @brief The node's angle in degrees about the axis of its crusher.
   */
  float crushAngle;

  /**
   * @brief One more than the server frame for which `frameContents` and `raised` hold, or 0.
   */
  uint32_t frame;

  /**
   * @brief The contents at the node in `frame`, as `G_Ai_Node_Contents` returns them.
   */
  int32_t frameContents;

  /**
   * @brief True if a raised platform stands over the node in `frame` (`G_Ai_Node_RaisedPlatform`).
   */
  bool raised;

  /**
   * @brief The cost of the cheapest path to the node that the search of `G_Ai_Node_FindPath` has
   * found.
   */
  float cost;

  /**
   * @brief The node before this one on that path.
   */
  GameAiNodeId cameFrom;
} GameAiNode;

static void G_Ai_Node_FreePathPool(void);
static int32_t G_Ai_Node_Contents(const GameAiNode *node);
static const GameAiNode *G_Ai_Node_Refresh(const GameAiNode *node);
static bool G_Ai_Node_LinkPassesHazard(const Vec3 from, const Vec3 to);

/**
 * @return The player box `bounds` raised by a step, so that a trace of it along a floor that slopes,
 * or that steps up, does not stop on the floor.
 */
static Box3 G_Ai_Node_StepBounds(const Box3 bounds) {

  Box3 raised = bounds;
  raised.mins.z += PM_STEP_HEIGHT;

  return raised;
}

/**
 * @brief Returns true if the given node is visible (unobstructed) from the specified position.
 */
static bool G_Ai_Node_Visible(const Vec3 position, const GameAiNodeId node) {

  return gi.Trace(position, G_Ai_Node_GetPosition(node), Box3_Zero(), NULL, CONTENTS_SOLID | CONTENTS_WINDOW).fraction == 1.0f;
}

/**
 * @return True if the level's movement or gravity changed since `G_Ai_Node_Init` built the graph,
 * so that its jumps and climbs out of water no longer match how players move.
 */
bool G_Ai_Node_IsStale(void) {

  const PMoveParams movement = G_MovementParams();

  return !Nav_SameMovement(&module.movement, &movement);
}

/**
 * @brief Returns the total number of navigation nodes currently loaded.
 */
uint32_t G_Ai_Node_Count(void) {
  return module.nodes ? (uint32_t) module.nodes->count : 0;
}

/**
 * @brief Builds the k-d tree over the positions of the nodes, if it is not built, or if nodes were
 * added since it was.
 * @return True if the tree is built.
 */
static bool G_Ai_Node_EnsureSpatialIndex(void) {

  if (!module.nodes || module.nodes->count == 0) {
    return false;
  }

  if (module.kdTree && module.kdTreeCount == module.nodes->count) {
    return true;
  }

  G_Ai_Node_InvalidateSpatialIndex();

  const size_t n = module.nodes->count;
  module.kdTreePositions = malloc(sizeof(Vec3) * n);
  if (!module.kdTreePositions) {
    return false;
  }

  for (size_t i = 0; i < n; i++) {
    module.kdTreePositions[i] = AI_NODE(module.nodes, i)->position;
  }

  module.kdTree = G_Ai_KdTreeCreate(module.kdTreePositions, n);
  if (!module.kdTree) {
    free(module.kdTreePositions);
    module.kdTreePositions = NULL;
    return false;
  }

  module.kdTreeCount = n;
  return true;
}

/**
 * @brief The query of `G_Ai_Node_FindClosest_`, and which nodes qualify for it.
 */
typedef struct {
  /**
   * @brief The position from which the closest node is found.
   */
  Vec3 position;

  /**
   * @brief True to skip the nodes that `position` does not see.
   */
  bool onlyVisible;

  /**
   * @brief True to weigh height four times, so that a node on the level of `position` is preferred
   * to one above or below it, except a node in liquid.
   */
  bool preferLevel;

  /**
   * @brief True to skip the nodes on models that move, for a link that the game makes once.
   */
  bool staticOnly;

  /**
   * @brief True to skip the nodes on models that move, for the k-d tree, which holds them where
   * they stood when it was built.
   */
  bool skipMovers;

  /**
   * @brief True to skip the nodes in lava or slime, for a position that is not in it, so that a
   * bot on the shore never takes a node in the lava as the start of its path, and walks into it.
   */
  bool skipHazards;

  /**
   * @brief For the k-d tree query, the squared distance of the closest node that it has taken, as
   * the query keeps it, from the squared maximum distance down.
   */
  double treeDistance;
} GameAiNodeQueryFilter;

/**
 * @brief Whether a node qualifies for `G_Ai_Node_FindClosest`, and at what distance.
 */
static bool G_Ai_Node_FindClosestFilter(const size_t nodenum, void *data, float *distance) {
  const GameAiNodeQueryFilter *filter = data;
  const GameAiNode *node = AI_NODE(module.nodes, nodenum);

  if ((filter->staticOnly || filter->skipMovers) && node->model) {
    return false;
  }

  if (filter->skipHazards && (G_Ai_Node_Contents(node) & AI_CONTENTS_HAZARD)) {
    return false;
  }

  Vec3 dir = Vec3_Subtract(filter->position, node->position);

  if (filter->preferLevel && !(gi.PointContents(node->position) & CONTENTS_MASK_LIQUID)) {
    dir.z *= 4.0f;
  }

  *distance = Vec3_LengthSquared(dir);

  return !filter->onlyVisible || G_Ai_Node_Visible(filter->position, nodenum);
}

/**
 * @brief The distance within which `G_Ai_Node_FindClosestCell` finds the closest node.
 */
#define AI_NODE_CELL_REACH Minf(NAV_CELL_SIZE, NAV_CELL_HEIGHT * 2.f)

/**
 * @brief Makes `node` the closest, if it passes the filter nearer than `closestDistance`. The level
 * preference of the filter only makes a distance longer, so a node whose plain distance is too long
 * is skipped before the filter traces to it.
 */
static void G_Ai_Node_FindClosest_compare(GameAiNodeQueryFilter *filter, const GameAiNodeId node,
                                              GameAiNodeId *closest, float *closestDistance) {

  const Vec3 position = AI_NODE(module.nodes, node)->position;

  if (Vec3_DistanceSquared(filter->position, position) > *closestDistance) {
    return;
  }

  float distance;
  if (!G_Ai_Node_FindClosestFilter(node, filter, &distance)) {
    return;
  }

  if (distance <= *closestDistance) {
    *closestDistance = distance;
    *closest = node;
  }
}

/**
 * @brief The filter of the k-d tree query of `G_Ai_Node_FindClosest_`. The query takes a node that
 * passes the filter nearer than `treeDistance`, the nearest that it has taken, so a node whose plain
 * distance is no nearer is rejected before the filter traces to it, and `treeDistance` follows each
 * node that the query takes.
 */
static bool G_Ai_Node_FindClosest_tree(const size_t node, void *data, float *distance) {

  GameAiNodeQueryFilter *filter = data;

  const Vec3 position = AI_NODE(module.nodes, node)->position;

  if (Vec3_DistanceSquared(filter->position, position) >= filter->treeDistance) {
    return false;
  }

  if (!G_Ai_Node_FindClosestFilter(node, filter, distance) || *distance >= filter->treeDistance) {
    return false;
  }

  filter->treeDistance = *distance;
  return true;
}

/**
 * @brief Finds the closest node of the built graph in the cells around the given position, or of
 * the nodes that the game added to it, and the nodes on models that move, which are in no cell.
 * @details Any node of the built graph outside of the cells searched is at least one cell width, or
 * two cell heights, from the position, so a node found within that distance is the closest of all.
 * The added nodes, for items, teleporters and elevators, and the nodes that move, are few, and each
 * is compared.
 * @param maxDistance The squared distance, at most `AI_NODE_CELL_REACH` squared, to search.
 */
static GameAiNodeId G_Ai_Node_FindClosestCell(GameAiNodeQueryFilter *filter, const float maxDistance) {

  const NavGraph *graph = &module.graph;

  const Vec3 rel = Vec3_Subtract(filter->position, graph->mins);
  const int32_t x = (int32_t) floorf(rel.x / NAV_CELL_SIZE);
  const int32_t y = (int32_t) floorf(rel.y / NAV_CELL_SIZE);
  const int32_t z = (int32_t) floorf(rel.z / NAV_CELL_HEIGHT);

  GameAiNodeId closest = AI_NODE_INVALID;
  float closestDistance = maxDistance;

  for (int32_t dz = -2; dz <= 2; dz++) {
    for (int32_t dy = -1; dy <= 1; dy++) {
      for (int32_t dx = -1; dx <= 1; dx++) {

        const Vec3i xyz = MakeVec3i(x + dx, y + dy, z + dz);

        if (xyz.x < 0 || xyz.y < 0 || xyz.z < 0) {
          continue;
        }

        if (xyz.x >= graph->size.x || xyz.y >= graph->size.y || xyz.z >= graph->size.z) {
          continue;
        }

        const int32_t cell = (xyz.z * graph->size.y + xyz.y) * graph->size.x + xyz.x;
        const int32_t node = graph->cellNodes[cell];

        if (node == -1) {
          continue;
        }

        G_Ai_Node_FindClosest_compare(filter, node, &closest, &closestDistance);
      }
    }
  }

  for (uint32_t node = module.graphNodes; node < module.nodes->count; node++) {
    G_Ai_Node_FindClosest_compare(filter, node, &closest, &closestDistance);
  }

  if (module.moverNodes) {
    for (size_t i = 0; i < module.moverNodes->count; i++) {
      G_Ai_Node_FindClosest_compare(filter, AI_NODE_ID(module.moverNodes, i), &closest, &closestDistance);
    }
  }

  return closest;
}

/**
 * @brief Finds the closest navigation node to the given position within the specified distance.
 * @details The cells around the position are searched first, with `G_Ai_Node_FindClosestCell`.
 * Beyond them, the k-d tree finds the closest node that passes the `onlyVisible` and `preferLevel`
 * filters, and every node is compared if the tree cannot be built. The tree holds the nodes on
 * models that move where they stood when it was built, so they are compared one by one, where they
 * stand now, and the tree is not built again as they move.
 * @param staticOnly True to skip the nodes on models that move.
 */
static GameAiNodeId G_Ai_Node_FindClosest_(const Vec3 position, const float maxDistance, const bool onlyVisible,
                                           const bool preferLevel, const bool staticOnly) {

  if (!module.nodes || module.nodes->count == 0) {
    return AI_NODE_INVALID;
  }

  GameAiNodeId closest = AI_NODE_INVALID;
  const float distSquared = maxDistance * maxDistance;

  GameAiNodeQueryFilter filter = {
    .position = position,
    .onlyVisible = onlyVisible,
    .preferLevel = preferLevel,
    .skipHazards = !(gi.PointContents(position) & AI_CONTENTS_HAZARD),
    .staticOnly = staticOnly
  };

  if (module.graph.cellNodes) {
    closest = G_Ai_Node_FindClosestCell(&filter, Minf(distSquared, AI_NODE_CELL_REACH * AI_NODE_CELL_REACH));
    if (closest != AI_NODE_INVALID) {
      return closest;
    }
  }

  if (G_Ai_Node_EnsureSpatialIndex()) {

    filter.skipMovers = true;
    filter.treeDistance = (double) maxDistance * maxDistance;
    const size_t node = G_Ai_KdTreeQuery(module.kdTree, position, maxDistance, G_Ai_Node_FindClosest_tree,
                                         &filter);
    filter.skipMovers = false;

    float closestDistance = distSquared;

    if (node != SIZE_MAX) {
      closest = (GameAiNodeId) node;
      closestDistance = (float) filter.treeDistance;
    }

    if (module.moverNodes && !staticOnly) {
      for (size_t i = 0; i < module.moverNodes->count; i++) {
        G_Ai_Node_FindClosest_compare(&filter, AI_NODE_ID(module.moverNodes, i), &closest, &closestDistance);
      }
    }

    return closest;
  }

  float closestDistance = distSquared;

  for (size_t i = 0; i < module.nodes->count; i++) {
    G_Ai_Node_FindClosest_compare(&filter, (GameAiNodeId) i, &closest, &closestDistance);
  }

  return closest;
}

/**
 * @brief Finds the closest navigation node to the given position, of all nodes, for a bot that may
 * stand on a model that moves.
 */
GameAiNodeId G_Ai_Node_FindClosest(const Vec3 position, const float maxDistance, const bool onlyVisible, const bool preferLevel) {
  return G_Ai_Node_FindClosest_(position, maxDistance, onlyVisible, preferLevel, false);
}

/**
 * @brief Finds the closest navigation node to the given position that does not move, for a link
 * that the game makes once, such as an item's or a jump pad's, which would break as the node moved.
 */
GameAiNodeId G_Ai_Node_FindClosestStatic(const Vec3 position, const float maxDistance, const bool onlyVisible, const bool preferLevel) {
  return G_Ai_Node_FindClosest_(position, maxDistance, onlyVisible, preferLevel, true);
}

/**
 * @brief Creates and appends a new navigation node at the specified world position. The node is a
 * crouch node if the standing box, raised by a step off a floor that slopes, does not fit there.
 */
GameAiNodeId G_Ai_Node_Create(const Vec3 position) {

  if (gi.PointContents(position) & CONTENTS_MASK_SOLID) {
    G_Ai_Debug("Rejected node at %s (inside solid)\n", vtos(position));
    return AI_NODE_INVALID;
  }

  if (!module.nodes) {
    module.nodes = $(alloc(Vector), initWithSize, sizeof(GameAiNode));
  }

  if (module.nodes->count >= AI_NODE_INVALID) {
    G_Warn("More than %d navigation nodes; rejected node at %s\n", AI_NODE_INVALID - 1, vtos(position));
    return AI_NODE_INVALID;
  }

  const Box3 standing = G_Ai_Node_StepBounds(PM_BOUNDS);

  GameAiNode node = (GameAiNode) {
    .position = position,
    .cell = -1,
    .contents = gi.PointContents(position) & CONTENTS_MASK_LIQUID,
    .crouch = gi.Trace(position, position, standing, NULL, CONTENTS_MASK_SOLID).startSolid
  };
  $(module.nodes, add, &node);

  G_Ai_Node_InvalidateSpatialIndex();

  G_Ai_Debug("Dropped new node %zu\n", module.nodes->count - 1);

  return module.nodes->count - 1;
}

/**
 * @return The link from node `a` to node `b`, or `NULL`.
 */
const GameAiLink *G_Ai_Node_GetLink(const GameAiNodeId a, const GameAiNodeId b) {

  if (!module.nodes || a >= module.nodes->count) {
    return NULL;
  }

  const GameAiNode *node = AI_NODE(module.nodes, a);

  if (node->links) {
    for (size_t i = 0; i < node->links->count; i++) {
      const GameAiLink *link = AI_LINK(node->links, i);
      if (link->id == b) {
        return link;
      }
    }
  }

  return NULL;
}

/**
 * @brief Returns true if node a has a directed link to node b.
 */
bool G_Ai_Node_IsLinked(const GameAiNodeId a, const GameAiNodeId b) {
  return G_Ai_Node_GetLink(a, b) != NULL;
}

/**
 * @brief Returns the array of outgoing links for the specified node.
 */
const Vector *G_Ai_Node_GetLinks(const GameAiNodeId a) {
  const GameAiNode *nodeA = AI_NODE(module.nodes, a);
  return nodeA->links;
}

/**
 * @brief Creates a directed link from node a to node b with the given traversal cost and travel type.
 */
void G_Ai_Node_LinkTravel(const GameAiNodeId a, const GameAiNodeId b, const float cost, const NavTravelType travelType) {

  if (!module.nodes || a >= module.nodes->count || b >= module.nodes->count) {
    return;
  }

  if (a == b || G_Ai_Node_IsLinked(a, b)) {
    return;
  }

  GameAiNode *nodeA = AI_NODE(module.nodes, a);

  if (!nodeA->links) {
    nodeA->links = $(alloc(Vector), initWithSize, sizeof(GameAiLink));
  }

  const GameAiNode *nodeB = AI_NODE(module.nodes, b);
  const bool still = !nodeA->model && !nodeB->model;

  GameAiLink link = (GameAiLink) {
    .id = b,
    .cost = cost,
    .travelType = travelType,
    .hazardTested = still,
    .hazard = still && G_Ai_Node_LinkPassesHazard(nodeA->position, nodeB->position)
  };
  $(nodeA->links, add, &link);

  G_Ai_Debug("Connected %d -> %d\n", a, b);
}

/**
 * @brief Creates a directed link from node a to node b with the given traversal cost.
 */
void G_Ai_Node_Link(const GameAiNodeId a, const GameAiNodeId b, const float cost) {
  G_Ai_Node_LinkTravel(a, b, cost, 0);
}

/**
 * @brief Returns the world position of the specified navigation node.
 */
Vec3 G_Ai_Node_GetPosition(const GameAiNodeId node) {

  return AI_NODE(module.nodes, node)->position;
}

/**
 * @see g_ai_node.h
 */
bool G_Ai_Node_IsEdge(const GameAiNodeId node) {
  return AI_NODE(module.nodes, node)->edge;
}

/**
 * @return The `func_plat` over the given position that is not at its bottom, or `NULL`.
 */
const GameEntity *G_Ai_Node_RaisedPlatform(const Vec3 position) {

  if (!module.platforms) {
    return NULL;
  }

  for (size_t i = 0; i < module.platforms->count; i++) {
    const GameEntity *plat = AI_PLATFORM(module.platforms, i);

    if (position.x < plat->absBounds.mins.x || position.x > plat->absBounds.maxs.x ||
        position.y < plat->absBounds.mins.y || position.y > plat->absBounds.maxs.y) {
      continue;
    }

    if (plat->absBounds.maxs.z > position.z + 32.f) {
      return plat;
    }
  }

  return NULL;
}

/**
 * @return True if a bot can move to the node at `position` now: if a player's box there has ground
 * under it, or none at all, because a drop link MAY lead to a node with no floor right under it,
 * and the movement falls to the floor below. A box that starts in a model that moves, such as a
 * door, a button or a rotating model, is tested again with a thinner box, which only the model's
 * own body blocks. A raised platform over the node blocks it.
 */
static bool G_Ai_Node_CanPathTo(const Vec3 position) {

  const Vec3 end = Vec3_Subtract(position, MakeVec3(0, 0, PM_GROUND_DIST * 3.f));

  CollisionTrace tr = gi.Trace(position, end, Box3_Expand3(G_PlayerBounds(), MakeVec3(1.f, 1.f, 0.f)), NULL,
                               CONTENTS_MASK_CLIP_CORPSE | CONTENTS_MONSTER_CLIP | CONTENTS_MASK_LIQUID);

  bool stuckInMover = tr.ent
      && (tr.startSolid || tr.allSolid)
      && (((GameEntity *) tr.ent)->s.number != 0
      && !(tr.contents & CONTENTS_MASK_LIQUID));

  if (tr.fraction == 1.0f) {
    return true;
  }

  if (stuckInMover) {

    const Box3 bounds = G_PlayerBounds();
    tr = gi.Trace(position,
               Vec3_Subtract(position, MakeVec3(0, 0, PM_GROUND_DIST * 3.f)),
               MakeBox3(MakeVec3(-4.f, -4.f, bounds.mins.z), MakeVec3(4.f, 4.f, bounds.maxs.z)),
               NULL,
               CONTENTS_MASK_CLIP_CORPSE | CONTENTS_MONSTER_CLIP | CONTENTS_MASK_LIQUID);
    stuckInMover = tr.ent
        && (tr.startSolid || tr.allSolid)
        && (((GameEntity *) tr.ent)->s.number != 0
        && !(tr.contents & CONTENTS_MASK_LIQUID));

    if (!stuckInMover) {
      return true;
    }

    return false;
  }

  return G_Ai_Node_RaisedPlatform(position) == NULL;
}

/**
 * @return True if a bot can move to the node at `index` of `path` now, with
 * `G_Ai_Node_CanPathTo`, or if the path has no node at `index`.
 */
bool G_Ai_Node_CanFollowPath(const Vector *path, const uint32_t index) {

  if (index >= path->count) {
    return true;
  }

  return G_Ai_Node_CanPathTo(AI_NODE(module.nodes, AI_NODE_ID(path, index))->position);
}

/**
 * @return A player trace against the world alone, so that players and movers do not block it.
 */
static CollisionTrace G_Ai_Node_WorldTrace(const Vec3 start, const Vec3 end, const Box3 bounds) {
  return gi.Clip(start, end, bounds, ge.entities[0], CONTENTS_MASK_CLIP_CORPSE | CONTENTS_MONSTER_CLIP);
}

/**
 * @brief Builds the level's navigation graph, and copies its nodes and links into `module.nodes`.
 */
void G_Ai_Node_Init(void) {

  G_Ai_Node_Shutdown();

  const NavParams params = {
    .bsp = gi.Bsp(),
    .movement = G_MovementParams(),
    .BoxTrace = gi.BoxTrace,
    .PointInsideBrush = gi.PointInsideBrush,
    .EntityValue = gi.EntityValue,
    .Warn = gi.Warn,
    .Error = gi.Error
  };

  module.movement = params.movement;

  if (!Nav_Build(&params, &module.graph)) {
    G_Warn("This map has no floor to navigate; bots will be dumb!\n");
    return;
  }

  const NavGraph *graph = &module.graph;

  if (graph->numNodes >= AI_NODE_INVALID) {
    G_Warn("This map has %d navigation nodes, more than %d; bots will be dumb!\n", graph->numNodes, AI_NODE_INVALID - 1);
    Nav_Free(&module.graph);
    return;
  }

  gi.Print("  Built %d navigation nodes, %d at an offset, %d at the edge of the void and %d in open water, and "
           "found %d floors with no room for a player and %d in a hurt or push trigger\n", graph->numNodes,
           graph->numOffset, graph->numEdge, graph->numSwim, graph->numNoFit, graph->numTrigger);
  gi.Print("  Built %d walk, %d crouch, %d barrier jump, %d walk off ledge, %d swim, %d jump and %d ladder "
           "links, %d of them through a doorway\n",
           graph->numTravel[NAV_TRAVEL_WALK], graph->numTravel[NAV_TRAVEL_CROUCH],
           graph->numTravel[NAV_TRAVEL_BARRIER_JUMP], graph->numTravel[NAV_TRAVEL_WALK_OFF_LEDGE],
           graph->numTravel[NAV_TRAVEL_SWIM], graph->numTravel[NAV_TRAVEL_JUMP],
           graph->numTravel[NAV_TRAVEL_LADDER], graph->numGated);

  module.nodes = $(alloc(Vector), initWithSize, sizeof(GameAiNode));

  for (int32_t i = 0; i < graph->numNodes; i++) {
    const NavNode *in = &graph->nodes[i];

    GameAiNode node = {
      .position = in->position,
      .cell = in->cell,
      .contents = in->contents,
      .edge = in->edge,
      .crouch = in->crouch,
      .model = in->model,
      .local = in->local
    };

    if (node.model) {
      if (!module.moverNodes) {
        module.moverNodes = $(alloc(Vector), initWithSize, sizeof(GameAiNodeId));
        module.moverLinked = $(alloc(Vector), initWithSize, sizeof(GameAiNodeId));
      }

      const GameAiNodeId id = (GameAiNodeId) i;
      $(module.moverNodes, add, (ident) &id);

      module.graph.cellNodes[in->cell] = -1;
    }

    if (in->numLinks) {
      node.links = $(alloc(Vector), initWithSize, sizeof(GameAiLink));

      for (int32_t j = 0; j < in->numLinks; j++) {
        const NavLink *link = &graph->links[in->firstLink + j];
        const float distance = Vec3_Distance(in->position, graph->nodes[link->node].position);

        const GameAiLink out = {
          .id = link->node,
          .cost = Maxf(distance, link->travelMillis * Maxf(1.f, module.movement.speedGround) / 1000.f),
          .travelType = link->travelType,
          .gate = link->gate
        };

        $(node.links, add, (ident) &out);
      }

      $(node.links, resize, node.links->count);
    }

    $(module.nodes, add, &node);
  }

  module.graphNodes = (uint32_t) graph->numNodes;
  module.graphLinks = (uint32_t) graph->numLinks;

  Nav_FreeNodes(&module.graph);

  G_Ai_Node_InvalidateSpatialIndex();
}

/**
 * @brief The distance within which an item is reachable from a node.
 */
#define AI_NODE_ITEM_DISTANCE 320.f

/**
 * @brief Reports each item that no node is near.
 */
static void G_Ai_Node_CheckItems(void) {

  G_ForEachEntity(ent, {
    if (!ent->item) {
      continue;
    }

    const GameAiNodeId node = G_Ai_Node_FindClosest(ent->s.origin, AI_NODE_ITEM_DISTANCE, true, false);

    if (node == AI_NODE_INVALID) {
      G_Ai_Debug("Entity %s @ %s appears to be unreachable by nodes\n", ent->classname, vtos(ent->s.origin));
    }
  });
}

/**
 * @brief The distance within which a ride starts or ends at a node.
 */
#define AI_RIDE_NODE_DISTANCE 64.f

/**
 * @brief The distance within which a jump pad starts or lands at a node that is in sight. It spans
 * several cells, so that a pad whose trigger or landing lies between nodes still links.
 */
#define AI_PUSH_NODE_DISTANCE 128.f

/**
 * @brief A `trigger_push` whose direction has at least this upward component pushes straight up,
 * as `PUSH_VERTICAL` in `g_entity_trigger.c`.
 */
#define AI_RIDE_VERTICAL .99f

/**
 * @brief The horizontal reach of a bot that steers in flight from a vertical push.
 */
#define AI_RIDE_VERTICAL_REACH 128.f

/**
 * @return The cost of a ride that lasts `seconds`.
 */
static float G_Ai_Node_RideCost(const float seconds) {
  return Maxf(1.f, seconds * PM_SPEED_RUN);
}

/**
 * @return The time in seconds in which a player pushed straight up at `speed` rises `rise` units,
 * under `gravity`. The rise MUST NOT exceed the apex of the flight.
 */
static float G_Ai_Node_RiseTime(const float speed, const float gravity, const float rise) {
  return (speed - sqrtf(speed * speed - 2.f * gravity * rise)) / gravity;
}

/**
 * @brief Flies a player from `origin` on a `trigger_push` with the level's movement and no input,
 * as a bot rides one, with `Nav_FlyPush`, which the client game draws the same flight with.
 * @param landing The player origin where the flight lands.
 * @param time The duration of the flight, in seconds.
 * @return True if the flight lands within `NAV_FLIGHT_SECONDS`.
 */
static bool G_Ai_Node_FlyPush(const GameEntity *push, const Vec3 origin, Vec3 *landing, float *time) {

  const NavPush nav = {
    .bounds = push->absBounds,
    .velocity = Vec3_Scale(push->moveDir, push->speed * 10.f)
  };

  PMove pm;
  memset(&pm, 0, sizeof(pm));

  pm.s.type = PM_NORMAL;
  pm.s.origin = origin;
  pm.s.params = G_MovementParams();

  pm.PointContents = gi.PointContents;
  pm.BoxContents = gi.BoxContents;
  pm.Trace = G_Ai_Node_WorldTrace;

  pm.Debug = gi.Debug;
  pm.DebugMask = gi.DebugMask;
  pm.debugMask = DEBUG_PMOVE_SERVER;

  if (!Nav_FlyPush(&nav, &pm, NULL, 0, NULL, time)) {
    return false;
  }

  *landing = pm.s.origin;
  return true;
}

/**
 * @brief The clearance in units between a player at the entry of a jump pad and the pad's trigger.
 */
#define AI_PUSH_ENTRY_CLEARANCE 8.f

/**
 * @return True if a player can walk the straight line from `start` to `end`. The box is raised by
 * a step, so that the floor does not stop it.
 */
static bool G_Ai_Node_CanWalk(const Vec3 start, const Vec3 end) {

  const CollisionTrace trace = G_Ai_Node_WorldTrace(start, end, G_Ai_Node_StepBounds(PM_BOUNDS));
  return !trace.startSolid && trace.fraction == 1.f;
}

/**
 * @brief Finds the floor under a player at `origin`, within four steps below it.
 * @param floor Receives the player origin on the floor.
 * @return True if the player has floor there.
 */
static bool G_Ai_Node_Floor(const Vec3 origin, Vec3 *floor) {

  const Vec3 bottom = Vec3_Subtract(origin, MakeVec3(0.f, 0.f, PM_STEP_HEIGHT * 4.f));
  const CollisionTrace trace = G_Ai_Node_WorldTrace(origin, bottom, PM_BOUNDS);

  if (trace.startSolid || trace.fraction == 1.f) {
    return false;
  }

  *floor = trace.end;
  return true;
}

/**
 * @brief Finds the floor of a `trigger_push`, under a player at the top of its center.
 * @param floor Receives the player origin on the floor.
 * @return True if the pad has floor within four steps below its trigger.
 */
static bool G_Ai_Node_PushFloor(const GameEntity *push, Vec3 *floor) {

  const Vec3 center = Box3_Center(push->absBounds);
  const Vec3 top = MakeVec3(center.x, center.y, push->absBounds.maxs.z - PM_BOUNDS.mins.z);
  const Vec3 bottom = MakeVec3(center.x, center.y, push->absBounds.mins.z - PM_BOUNDS.mins.z - PM_STEP_HEIGHT * 4.f);
  const CollisionTrace trace = G_Ai_Node_WorldTrace(top, bottom, PM_BOUNDS);

  if (trace.startSolid || trace.fraction == 1.f) {
    return false;
  }

  *floor = trace.end;
  return true;
}

/**
 * @see g_ai_node.h
 */
bool G_Ai_Node_PushEntry(const GameEntity *push, Vec3 *entry) {

  const Vec3 dir = MakeVec3(push->moveDir.x, push->moveDir.y, 0.f);
  if (push->moveDir.z >= AI_RIDE_VERTICAL || Vec3_Length(dir) <= .1f) {
    return false;
  }

  Vec3 pad;
  if (!G_Ai_Node_PushFloor(push, &pad)) {
    return false;
  }

  const Vec3 forward = Vec3_Normalize(dir);
  const Vec3 size = Box3_Size(push->absBounds);

  const float extent = (fabsf(forward.x) * size.x + fabsf(forward.y) * size.y) * .5f;
  const float distance = extent + PM_BOUNDS.maxs.x + AI_PUSH_ENTRY_CLEARANCE;

  const Vec3 behind = MakeVec3(pad.x - forward.x * distance, pad.y - forward.y * distance, pad.z);

  if (!G_Ai_Node_CanWalk(behind, pad)) {
    return false;
  }

  return G_Ai_Node_Floor(Vec3_Add(behind, MakeVec3(0.f, 0.f, PM_STEP_HEIGHT)), entry);
}

/**
 * @return The node from which a bot walks into an angled `trigger_push` along its push: the node
 * nearest the pad's entry, within `AI_RIDE_NODE_DISTANCE` and at the level of the floor there, from
 * which a player can walk to the entry. Else `AI_NODE_INVALID`. The distance keeps the pad within
 * the reach in which `G_Ai_PushDestination` looks for it from the node.
 * @remarks A pad can lie in a channel between walls, with a node beside it that a line of sight
 * reaches but a player cannot. A bot that walks from that node into the side wall slides along it,
 * past the pad, and off the edge that the pad throws it over.
 */
static GameAiNodeId G_Ai_Node_PushStart(const GameEntity *push) {

  Vec3 entry;
  if (!G_Ai_Node_PushEntry(push, &entry)) {
    return AI_NODE_INVALID;
  }

  GameAiNodeId start = AI_NODE_INVALID;
  float startDistance = AI_RIDE_NODE_DISTANCE;

  for (size_t i = 0; i < module.nodes->count; i++) {
    const GameAiNode *node = AI_NODE(module.nodes, i);

    if (node->cell == -1 || node->model || fabsf(node->position.z - entry.z) > PM_STEP_HEIGHT) {
      continue;
    }

    const float distance = Vec3_Distance(node->position, entry);
    if (distance >= startDistance) {
      continue;
    }

    if (!G_Ai_Node_CanWalk(node->position, entry)) {
      continue;
    }

    start = (GameAiNodeId) i;
    startDistance = distance;
  }

  return start;
}

/**
 * @brief Links the node `start` beside a vertical `trigger_push` to the ledges beside its column,
 * which a bot steers onto in flight: the built nodes that do not move, above a step and below the
 * `apex` of the flight, within `AI_RIDE_VERTICAL_REACH` across.
 * @param speed The vertical speed of the push.
 */
static void G_Ai_Node_LinkPush_ledges(const GameAiNodeId start, const float speed, const float gravity,
                                      const float apex) {

  const Vec3 origin = G_Ai_Node_GetPosition(start);

  for (size_t i = 0; i < module.nodes->count; i++) {
    const GameAiNode *node = AI_NODE(module.nodes, i);
    const float rise = node->position.z - origin.z;

    if (node->cell == -1 || node->model || rise <= PM_STEP_HEIGHT || rise > apex) {
      continue;
    }

    if (Vec2_Distance(Vec3_XY(node->position), Vec3_XY(origin)) > AI_RIDE_VERTICAL_REACH) {
      continue;
    }

    const Vec3 column = MakeVec3(origin.x, origin.y, node->position.z);

    if (gi.Trace(origin, column, PM_BOUNDS, NULL, CONTENTS_MASK_CLIP_MONSTER).fraction < 1.f) {
      continue;
    }

    if (gi.Trace(column, node->position, PM_BOUNDS, NULL, CONTENTS_MASK_CLIP_MONSTER).fraction < 1.f) {
      continue;
    }

    const float time = G_Ai_Node_RiseTime(speed, gravity, rise);
    G_Ai_Node_LinkTravel(start, (GameAiNodeId) i, G_Ai_Node_RideCost(time), NAV_TRAVEL_PUSH);
  }
}

/**
 * @brief Links the node `start` beside a vertical `trigger_push` to a node that it makes for each
 * hovering item in its column, which a bot takes in flight, as a player does. The column to an
 * item rises from the pad's center, `pad`, as a player flies, not from the node beside the pad.
 * @param speed The vertical speed of the push.
 */
static void G_Ai_Node_LinkPush_items(const GameAiNodeId start, const Vec3 pad, const float speed,
                                     const float gravity, const float apex) {

  const Vec3 origin = G_Ai_Node_GetPosition(start);

  G_ForEachEntity(item, {
    if (!item->item || item->node != AI_NODE_INVALID) {
      continue;
    }

    const float rise = item->s.origin.z - origin.z;
    if (rise <= PM_STEP_HEIGHT || rise > apex) {
      continue;
    }

    if (Vec2_Distance(Vec3_XY(item->s.origin), Vec3_XY(origin)) > AI_RIDE_VERTICAL_REACH) {
      continue;
    }

    const Vec3 column = MakeVec3(pad.x, pad.y, item->s.origin.z);

    if (gi.Trace(pad, column, PM_BOUNDS, NULL, CONTENTS_MASK_CLIP_MONSTER).fraction < 1.f) {
      continue;
    }

    const GameAiNodeId node = G_Ai_Node_Create(item->s.origin);
    if (node == AI_NODE_INVALID) {
      continue;
    }

    const float time = G_Ai_Node_RiseTime(speed, gravity, rise);
    G_Ai_Node_LinkTravel(start, node, G_Ai_Node_RideCost(time), NAV_TRAVEL_PUSH);

    item->node = node;
  });
}

/**
 * @brief Links the node `start` nearest an angled `trigger_push` whose pad is at `pad`, or the node
 * of `G_Ai_Node_PushStart`, if it has one, to the node on which the flight of
 * `G_Ai_Node_FlyPush` from the pad lands.
 */
static void G_Ai_Node_LinkPush_angled(const GameEntity *push, const GameAiNodeId start, const Vec3 pad) {

  Vec3 floor;
  if (!G_Ai_Node_PushFloor(push, &floor)) {
    G_Ai_Debug("%s @ %s has no floor\n", push->classname, vtos(pad));
    return;
  }

  Vec3 landing;
  float time;

  if (!G_Ai_Node_FlyPush(push, floor, &landing, &time)) {
    G_Ai_Debug("%s @ %s does not land within %.0f seconds\n", push->classname, vtos(pad), NAV_FLIGHT_SECONDS);
    return;
  }

  const GameAiNodeId entry = G_Ai_Node_PushStart(push);

  const GameAiNodeId end = G_Ai_Node_FindClosestStatic(landing, AI_PUSH_NODE_DISTANCE, true, false);
  if (end == AI_NODE_INVALID || end == start || end == entry) {
    G_Ai_Debug("%s @ %s lands at %s, with no node in sight\n", push->classname, vtos(pad), vtos(landing));
    return;
  }

  G_Ai_Node_LinkTravel(entry == AI_NODE_INVALID ? start : entry, end, G_Ai_Node_RideCost(time), NAV_TRAVEL_PUSH);
}

/**
 * @brief Links the node on a `trigger_push` to the nodes that its push lands on.
 * @details An angled push is flown from the pad with `G_Ai_Node_FlyPush`, and linked from the node
 * of `G_Ai_Node_PushStart`, if it has one, else from the node nearest the pad. A vertical push links
 * to the ledges beside its column, which a bot steers onto in flight, and to a node that it makes
 * for each hovering item in its column, which a bot takes in flight, as a player does. The column
 * to an item rises from the pad's center, as a player flies, not from the node beside the pad.
 */
static void G_Ai_Node_LinkPush(const GameEntity *push) {

  if (push->solid == SOLID_NOT) {
    return;
  }

  const Vec3 center = Box3_Center(push->absBounds);
  const Vec3 pad = MakeVec3(center.x, center.y, push->absBounds.mins.z - PM_BOUNDS.mins.z);

  const GameAiNodeId start = G_Ai_Node_FindClosestStatic(pad, AI_PUSH_NODE_DISTANCE, true, false);
  if (start == AI_NODE_INVALID) {
    G_Ai_Debug("No node at %s @ %s\n", push->classname, vtos(pad));
    return;
  }

  if (push->moveDir.z < AI_RIDE_VERTICAL) {
    G_Ai_Node_LinkPush_angled(push, start, pad);
    return;
  }

  const float gravity = G_LevelGravity();
  const Vec3 velocity = Vec3_Scale(push->moveDir, push->speed * 10.f);
  const float apex = velocity.z * velocity.z / (2.f * gravity);

  G_Ai_Node_LinkPush_ledges(start, velocity.z, gravity, apex);
  G_Ai_Node_LinkPush_items(start, pad, velocity.z, gravity, apex);
}

/**
 * @brief Links the built nodes at the given height around a plat to the plat's own node there,
 * in both directions. The line between them is swept with the player's box raised by a step,
 * because a plat stops a few units off the level of its landing, and the box would clip its edge.
 */
static void G_Ai_Node_LinkPlatLevel(const GameEntity *plat, const GameAiNodeId platNode) {

  const Vec3 position = G_Ai_Node_GetPosition(platNode);
  const float reach = Maxf(Box3_Size(plat->absBounds).x, Box3_Size(plat->absBounds).y) * .5f + AI_RIDE_NODE_DISTANCE;

  const Box3 bounds = G_Ai_Node_StepBounds(PM_BOUNDS);

  for (size_t i = 0; i < module.nodes->count; i++) {
    const GameAiNode *node = AI_NODE(module.nodes, i);

    if (node->cell == -1 || node->model || fabsf(node->position.z - position.z) > PM_STEP_HEIGHT) {
      continue;
    }

    const float distance = Vec2_Distance(Vec3_XY(node->position), Vec3_XY(position));
    if (distance > reach) {
      continue;
    }

    if (gi.Trace(node->position, position, bounds, NULL, CONTENTS_MASK_SOLID).fraction < 1.f) {
      continue;
    }

    G_Ai_Node_LinkTravel((GameAiNodeId) i, platNode, distance, NAV_TRAVEL_WALK);
    G_Ai_Node_LinkTravel(platNode, (GameAiNodeId) i, distance, NAV_TRAVEL_WALK);
  }
}

/**
 * @brief Gives a `func_plat` a node at the bottom and at the top of its travel, links the bottom
 * to the top as an elevator, and links each to the built nodes beside it.
 */
static void G_Ai_Node_LinkPlat(const GameEntity *plat) {

  const float rise = plat->pos1.z - plat->pos2.z;
  if (rise <= PM_STEP_HEIGHT) {
    return;
  }

  const Vec3 center = Box3_Center(plat->absBounds);
  const float bottom = plat->absBounds.maxs.z - (plat->s.origin.z - plat->pos2.z) - PM_BOUNDS.mins.z;

  const GameAiNodeId lower = G_Ai_Node_Create(MakeVec3(center.x, center.y, bottom));
  const GameAiNodeId upper = G_Ai_Node_Create(MakeVec3(center.x, center.y, bottom + rise));

  if (lower == AI_NODE_INVALID || upper == AI_NODE_INVALID) {
    G_Ai_Debug("No room for nodes on %s @ %s\n", plat->classname, vtos(center));
    return;
  }

  const float speed = Maxf(1.f, plat->moveInfo.speed);
  G_Ai_Node_LinkTravel(lower, upper, G_Ai_Node_RideCost(rise / speed), NAV_TRAVEL_PLAT);

  G_Ai_Node_LinkPlatLevel(plat, lower);
  G_Ai_Node_LinkPlatLevel(plat, upper);
}

/**
 * @brief Links the graph's rides: jump pads and plats. Teleporters link themselves.
 */
static void G_Ai_Node_LinkRides(void) {

  G_ForEachEntity(ent, {
    if (!ent->classname) {
      continue;
    }

    if (!Str_Compare(ent->classname, "trigger_push")) {
      G_Ai_Node_LinkPush(ent);
    } else if (!Str_Compare(ent->classname, "func_plat")) {
      G_Ai_Node_LinkPlat(ent);
    }
  });

  int32_t counts[NAV_TRAVEL_PLAT + 1] = { 0 };

  for (size_t i = 0; i < module.nodes->count; i++) {
    const Vector *links = AI_NODE(module.nodes, i)->links;

    for (size_t j = 0; links && j < links->count; j++) {
      const int32_t type = AI_LINK(links, j)->travelType;
      if (type >= NAV_TRAVEL_TELEPORT && type <= NAV_TRAVEL_PLAT) {
        counts[type]++;
      }
    }
  }

  gi.Print("  Linked %d teleporters, %d jump pads and %d elevators.\n",
           counts[NAV_TRAVEL_TELEPORT], counts[NAV_TRAVEL_PUSH], counts[NAV_TRAVEL_PLAT]);
}

/**
 * @brief The interval in milliseconds at which the nodes on models that move are linked again to
 * the nodes beside them.
 */
#define AI_MOVER_LINK_MILLIS 100

/**
 * @brief The horizontal distance within which a node on a model that moves links to a node that
 * does not: a little more than a cell, so that one is always in reach as the model passes.
 */
#define AI_MOVER_LINK_DISTANCE (NAV_CELL_SIZE * 1.25f)

/**
 * @brief The least damage of a `func_rotating` that crushes. A `func_rotating` with no `dmg` key
 * does 2 damage when it is blocked, which a bot can ride and walk beside.
 */
#define AI_CRUSHER_DAMAGE 100

/**
 * @brief The margin in degrees that widens the arc of a crusher's floor nodes to its whole body.
 */
#define AI_CRUSHER_MARGIN 20.f

/**
 * @brief The time in seconds that a bot needs inside a crusher's sweep: a node is closed if the
 * crusher covers it now, or will within this time.
 */
#define AI_CRUSHER_SAFE_SECONDS 4.f

/**
 * @return The next node on a model that moves that `ent` moves, from the index `*i` of
 * `moverNodes`, which advances past it, or `NULL`.
 */
static const GameAiNode *G_Ai_Node_FindCrushers_next(const GameEntity *ent, size_t *i) {

  while (*i < module.moverNodes->count) {
    const GameAiNode *node = AI_NODE(module.nodes, AI_NODE_ID(module.moverNodes, (*i)++));
    if (node->mover == ent) {
      return node;
    }
  }

  return NULL;
}

/**
 * @brief Finds the arc of the nodes that `ent` moves, about its origin.
 * @param center Receives the center of the arc, in degrees about its axis.
 * @param halfWidth Receives half the width of the arc, in degrees.
 */
static void G_Ai_Node_FindCrushers_arc(const GameEntity *ent, float *center, float *halfWidth) {

  Vec2 sum = Vec2_Zero();
  const GameAiNode *node;

  for (size_t i = 0; (node = G_Ai_Node_FindCrushers_next(ent, &i));) {
    const float length = Vec2_Length(Vec3_XY(node->local));
    if (length > 0.f) {
      sum = Vec2_Add(sum, Vec2_Scale(Vec3_XY(node->local), 1.f / length));
    }
  }

  *center = AngleMod(Degrees(atan2f(sum.y, sum.x)));
  *halfWidth = 0.f;

  for (size_t i = 0; (node = G_Ai_Node_FindCrushers_next(ent, &i));) {
    const float angle = AngleMod(Degrees(atan2f(node->local.y, node->local.x)));
    *halfWidth = Maxf(*halfWidth, SmallestAngleBetween(angle, *center));
  }
}

/**
 * @return The radius of the sweep of `ent`: the farthest of its bounds' sides and its nodes, with
 * the player's box, from its origin.
 */
static float G_Ai_Node_FindCrushers_radius(const GameEntity *ent) {

  const Box3 bounds = ent->bounds;
  float radius = Maxf(Maxf(-bounds.mins.x, bounds.maxs.x), Maxf(-bounds.mins.y, bounds.maxs.y));

  const GameAiNode *node;
  for (size_t i = 0; (node = G_Ai_Node_FindCrushers_next(ent, &i));) {
    radius = Maxf(radius, Vec2_Length(Vec3_XY(node->local)) + PM_BOUNDS.maxs.x);
  }

  return radius;
}

/**
 * @brief Records the crusher `index` of `ent`, and its angle, in each node that does not move and
 * whose box reaches more than a step into the cylinder of `radius` of its sweep.
 */
static void G_Ai_Node_FindCrushers_mark(const GameEntity *ent, const int32_t index, const float radius) {

  const float bottom = ent->s.origin.z + ent->bounds.mins.z;
  const float top = ent->s.origin.z + ent->bounds.maxs.z;

  for (size_t i = 0; i < module.nodes->count; i++) {
    GameAiNode *node = AI_NODE(module.nodes, i);

    if (node->model || node->position.z + PM_BOUNDS.maxs.z < bottom ||
        node->position.z + PM_BOUNDS.mins.z >= top - PM_STEP_HEIGHT) {
      continue;
    }

    const Vec2 delta = Vec2_Subtract(Vec3_XY(node->position), Vec3_XY(ent->s.origin));
    if (Vec2_Length(delta) > radius + PM_BOUNDS.maxs.x) {
      continue;
    }

    node->crusher = index + 1;
    node->crushAngle = AngleMod(Degrees(atan2f(delta.y, delta.x)));
  }
}

/**
 * @brief Finds the crusher of each model that moves, whose entity is a `func_rotating` that does at
 * least `AI_CRUSHER_DAMAGE` and turns about the vertical axis. Its arc is the arc of its nodes about
 * its origin. Its sweep is the cylinder of the farthest of its bounds' sides and its nodes, through
 * its height. Each node that does not move, and whose box reaches more than a step into that
 * cylinder, records the crusher and its angle. Where two sweeps overlap, the later crusher is kept.
 */
static void G_Ai_Node_FindCrushers(void) {

  for (size_t n = 0; n < module.moverNodes->count; n++) {
    const GameAiNode *node = AI_NODE(module.nodes, AI_NODE_ID(module.moverNodes, n));
    const GameEntity *ent = node->mover;

    if (!ent || Str_Compare(ent->classname, "func_rotating") || ent->damage < AI_CRUSHER_DAMAGE || ent->moveDir.y == 0.f) {
      continue;
    }

    int32_t index = -1;
    for (int32_t i = 0; i < module.numCrushers; i++) {
      if (module.crushers[i].ent == ent) {
        index = i;
      }
    }

    if (index != -1) {
      continue;
    }

    if (module.numCrushers == AI_MAX_CRUSHERS) {
      G_Warn("More than %d crushers; bots may walk under %s @ %s\n", AI_MAX_CRUSHERS, ent->classname, vtos(ent->s.origin));
      continue;
    }

    float center, halfWidth;
    G_Ai_Node_FindCrushers_arc(ent, &center, &halfWidth);

    index = module.numCrushers++;
    module.crushers[index] = (GameAiCrusher) {
      .ent = ent,
      .center = center,
      .halfWidth = halfWidth + AI_CRUSHER_MARGIN
    };

    G_Ai_Node_FindCrushers_mark(ent, index, G_Ai_Node_FindCrushers_radius(ent));

    G_Debug("%s @ %s crushes in an arc of %.0f degrees\n", ent->classname, vtos(ent->s.origin), halfWidth * 2.f);
  }
}

/**
 * @return True if the node lies in the sweep of a crusher that covers it now, or that will reach it
 * within `AI_CRUSHER_SAFE_SECONDS`.
 */
bool G_Ai_Node_IsCrushing(const GameAiNodeId id) {

  const GameAiNode *node = AI_NODE(module.nodes, id);

  if (!node->crusher) {
    return false;
  }

  const GameAiCrusher *crusher = &module.crushers[node->crusher - 1];
  const float speed = crusher->ent->avelocity.y;

  if (speed == 0.f) {
    return false;
  }

  const float center = AngleMod(crusher->center + crusher->ent->s.angles.y);

  if (SmallestAngleBetween(node->crushAngle, center) <= crusher->halfWidth) {
    return true;
  }

  const float edge = speed > 0.f ? center + crusher->halfWidth : center - crusher->halfWidth;
  const float travel = speed > 0.f ? AngleMod(node->crushAngle - edge) : AngleMod(edge - node->crushAngle);

  return travel / fabsf(speed) < AI_CRUSHER_SAFE_SECONDS;
}

/**
 * @return The inline model of the entity, as its `model` names it (`*1`, `*2`...), or 0 if it has
 * no inline model, or one out of range.
 */
static int32_t G_Ai_Node_InlineModel(const GameEntity *ent) {

  if (!ent->model || ent->model[0] != '*') {
    return 0;
  }

  const long model = strtol(ent->model + 1, NULL, 10);

  return model > 0 && model < MAX_MODELS ? (int32_t) model : 0;
}

/**
 * @brief Finds the door entity of each inline model, and the entity of each node on a model that
 * moves, by the entity's inline model. Where several entities share an inline model, the last door
 * is the model's door, and the first entity moves its nodes.
 */
static void G_Ai_Node_FindMovers(void) {

  const GameEntity *movers[MAX_MODELS] = { NULL };

  G_ForEachEntity(ent, {
    const int32_t model = G_Ai_Node_InlineModel(ent);
    if (!model) {
      continue;
    }

    if (!movers[model]) {
      movers[model] = ent;
    }

    if (ent->classname &&
        (!Str_Compare(ent->classname, "func_door") || !Str_Compare(ent->classname, "func_door_rotating"))) {
      module.doors[model] = ent;
    }
  });

  if (!module.moverNodes) {
    return;
  }

  for (size_t n = 0; n < module.moverNodes->count; n++) {
    GameAiNode *node = AI_NODE(module.nodes, AI_NODE_ID(module.moverNodes, n));

    if (node->model > 0 && node->model < MAX_MODELS) {
      node->mover = movers[node->model];
    }

    if (!node->mover) {
      G_Debug("No entity moves navigation node %u on model *%d\n", AI_NODE_ID(module.moverNodes, n), node->model);
    }
  }

  G_Ai_Node_FindCrushers();
}

/**
 * @return The world position of the node on a model that moves, where its `mover` stands now.
 */
static Vec3 G_Ai_Node_MoverPosition(const GameAiNode *node) {

  const Mat4 matrix = Mat4_FromRotationTranslationScale(node->mover->s.angles, node->mover->s.origin, 1.f);

  return Mat4_Transform(matrix, node->local);
}

/**
 * @brief Removes the links of `node` to the nodes that are not on `model`.
 */
static void G_Ai_Node_UnlinkModel(GameAiNode *node, const int32_t model) {

  if (!node->links) {
    return;
  }

  for (size_t i = node->links->count; i > 0; i--) {
    if (AI_NODE(module.nodes, AI_LINK(node->links, i - 1)->id)->model != model) {
      $(node->links, removeAt, i - 1);
    }
  }
}

/**
 * @brief The spacing in units of the floor samples under a dynamic walk link.
 */
#define AI_MOVER_FLOOR_SPACING 16.f

/**
 * @brief The highest drop onto or off a model that moves, which a dynamic link spans.
 */
#define AI_MOVER_DROP_HEIGHT 128.f

/**
 * @brief Adds a link from node `a` to node `b`, without the debug output of
 * `G_Ai_Node_LinkTravel`, because the nodes that move are linked again many times a second.
 */
static void G_Ai_Node_LinkDynamic(const GameAiNodeId a, const GameAiNodeId b, const float cost, const NavTravelType type) {

  GameAiNode *node = AI_NODE(module.nodes, a);

  if (!node->links) {
    node->links = $(alloc(Vector), initWithSize, sizeof(GameAiLink));
  }

  const GameAiLink link = {
    .id = b,
    .cost = cost,
    .travelType = type
  };

  $(node->links, add, (ident) &link);
}

/**
 * @brief A link that `G_Ai_Node_LinkMover` or `G_Ai_Node_LinkMovers` made, which it adds again
 * while its node stays where it was.
 */
typedef struct {
  /**
   * @brief The node that the link leaves.
   */
  GameAiNodeId from;

  /**
   * @brief The node that the link reaches.
   */
  GameAiNodeId to;

  /**
   * @brief The cost of the link.
   */
  float cost;

  /**
   * @brief The `NavTravelType` of the link.
   */
  NavTravelType travelType;
} GameAiMoverLink;

/**
 * @brief The longest time in milliseconds for which a node on a model at rest keeps the links that
 * `G_Ai_Node_LinkMover` made, so that a link that a player or another model blocked comes back.
 */
#define AI_MOVER_RELINK_MILLIS 1000

/**
 * @brief The most models that move between two relinks, near which a node at rest is linked again,
 * because the floor under its links, or a wall across them, MAY have moved. With more, every node is
 * linked again.
 */
#define AI_MOVER_MAX_MOVED 32

/**
 * @brief Adds a press link from node `a` to node `b`, through the door with the inline model `gate`.
 */
static void G_Ai_Node_LinkPress(const GameAiNodeId a, const GameAiNodeId b, const float cost, const NavTravelType type,
                                const int32_t gate) {

  GameAiNode *node = AI_NODE(module.nodes, a);

  if (!node->pressLinks) {
    node->pressLinks = $(alloc(Vector), initWithSize, sizeof(GameAiLink));
  }

  const GameAiLink link = {
    .id = b,
    .cost = cost,
    .travelType = type,
    .gate = gate
  };

  $(node->pressLinks, add, (ident) &link);
}

/**
 * @brief Adds a link that `G_Ai_Node_LinkMover` made for `node`, and keeps it in `moverLinks`, or,
 * if `gate` is not 0, adds it as a press link through the door with that inline model.
 */
static void G_Ai_Node_LinkMoverTo_add(GameAiNode *node, const GameAiNodeId a, const GameAiNodeId b,
                                    const float cost, const NavTravelType type, const int32_t gate) {

  if (gate) {
    G_Ai_Node_LinkPress(a, b, cost, type, gate);
    return;
  }

  G_Ai_Node_LinkDynamic(a, b, cost, type);

  const GameAiMoverLink link = {
    .from = a,
    .to = b,
    .cost = cost,
    .travelType = type
  };

  $(node->moverLinks, add, (ident) &link);
}

/**
 * @return True if there is floor, and no liquid, under the player's box at each point
 * `AI_MOVER_FLOOR_SPACING` apart on the line from `a` to `b`, which are player origins on the same
 * level. The box is as wide as the player, because a player stands where any part of it has floor,
 * and does not fall through a seam between two models side by side. Where the box starts in a wall,
 * a point is traced instead.
 */
static bool G_Ai_Node_HasFloor(const Vec3 a, const Vec3 b) {

  const float length = Vec3_Distance(a, b);
  const float depth = -PM_BOUNDS.mins.z + PM_STEP_HEIGHT * 2.f;
  const Box3 bounds = MakeBox3(MakeVec3(PM_BOUNDS.mins.x, PM_BOUNDS.mins.y, 0.f), MakeVec3(PM_BOUNDS.maxs.x, PM_BOUNDS.maxs.y, 0.f));

  for (float d = AI_MOVER_FLOOR_SPACING; d < length; d += AI_MOVER_FLOOR_SPACING) {

    const Vec3 point = Vec3_Mix(a, b, d / length);
    const Vec3 start = Vec3_Add(point, MakeVec3(0.f, 0.f, PM_STEP_HEIGHT));
    const Vec3 end = Vec3_Subtract(point, MakeVec3(0.f, 0.f, depth));

    CollisionTrace tr = gi.Trace(start, end, bounds, NULL, CONTENTS_MASK_SOLID | CONTENTS_MASK_LIQUID);

    if (tr.startSolid) {
      tr = gi.Trace(start, end, Box3_Zero(), NULL, CONTENTS_MASK_SOLID | CONTENTS_MASK_LIQUID);
    }

    if (tr.fraction == 1.f || (tr.contents & CONTENTS_MASK_LIQUID)) {
      return false;
    }
  }

  return true;
}

/**
 * @return True if `b` is within a step of height and `AI_MOVER_LINK_DISTANCE` across of `a`.
 * @param distance Receives the distance across.
 */
static bool G_Ai_Node_InReach(const Vec3 a, const Vec3 b, float *distance) {

  if (fabsf(b.z - a.z) > PM_STEP_HEIGHT) {
    return false;
  }

  *distance = Vec2_Distance(Vec3_XY(b), Vec3_XY(a));

  return *distance <= AI_MOVER_LINK_DISTANCE;
}

/**
 * @return True if a player walks from `a` to `b`, player origins of which one at least is on a
 * model that moves: within a step of height and `AI_MOVER_LINK_DISTANCE` across of each other, with
 * a clear line for the player's box raised by a step, and floor under the line.
 * @param bounds The player's box, raised by a step.
 * @param distance Receives the distance across.
 */
static bool G_Ai_Node_CanWalkMover(const Vec3 a, const Vec3 b, const Box3 bounds, float *distance) {

  if (!G_Ai_Node_InReach(a, b, distance)) {
    return false;
  }

  const CollisionTrace tr = gi.Trace(a, b, bounds, NULL, CONTENTS_MASK_CLIP_MONSTER);

  return !tr.startSolid && tr.fraction == 1.f && G_Ai_Node_HasFloor(a, b);
}

/**
 * @brief Links the node `id` on a model that moves to the node `other` that does not move: both
 * ways with a walk, if `other` is on its level, with a clear line for the player's box, raised by a
 * step, and floor under the line, or one way with a drop, from the higher of the two to the lower,
 * within `AI_MOVER_DROP_HEIGHT`, with a clear line across at the higher level and down onto the
 * lower. The nodes MUST be within `AI_MOVER_LINK_DISTANCE` of each other across.
 * @param gate 0 for a link of the graph, or the inline model of the door for a press link.
 * @return True if the nodes are linked.
 */
static bool G_Ai_Node_LinkMoverTo(GameAiNode *node, const GameAiNodeId id, const GameAiNodeId other, const Box3 bounds,
                                  const int32_t gate) {

  const Vec3 position = AI_NODE(module.nodes, other)->position;

  float distance = Vec2_Distance(Vec3_XY(position), Vec3_XY(node->position));
  if (distance > AI_MOVER_LINK_DISTANCE) {
    return false;
  }

  const float height = position.z - node->position.z;

  if (fabsf(height) <= PM_STEP_HEIGHT) {
    if (!G_Ai_Node_CanWalkMover(node->position, position, bounds, &distance)) {
      return false;
    }

    G_Ai_Node_LinkMoverTo_add(node, id, other, distance, NAV_TRAVEL_WALK, gate);
    G_Ai_Node_LinkMoverTo_add(node, other, id, distance, NAV_TRAVEL_WALK, gate);
    return true;
  }

  if (fabsf(height) > AI_MOVER_DROP_HEIGHT) {
    return false;
  }

  const Vec3 high = height > 0.f ? position : node->position;
  const Vec3 low = height > 0.f ? node->position : position;
  const Vec3 over = MakeVec3(low.x, low.y, high.z);

  const CollisionTrace across = gi.Trace(high, over, bounds, NULL, CONTENTS_MASK_CLIP_MONSTER);
  if (across.startSolid || across.fraction < 1.f) {
    return false;
  }

  const CollisionTrace down = gi.Trace(over, low, G_PlayerBounds(), NULL, CONTENTS_MASK_CLIP_MONSTER);
  if (down.startSolid || down.fraction < 1.f) {
    return false;
  }

  const float cost = distance + fabsf(height);
  if (height > 0.f) {
    G_Ai_Node_LinkMoverTo_add(node, other, id, cost, NAV_TRAVEL_WALK_OFF_LEDGE, gate);
  } else {
    G_Ai_Node_LinkMoverTo_add(node, id, other, cost, NAV_TRAVEL_WALK_OFF_LEDGE, gate);
  }

  return true;
}

/**
 * @brief Links the node on a model that moves to each node that does not move in the cells around
 * it, and to each node that the game added, such as an item's, with `G_Ai_Node_LinkMoverTo`.
 * @param gate 0 for links of the graph, which `moverLinked` records, or the inline model of the
 * door for press links.
 */
static void G_Ai_Node_LinkMoverAround(GameAiNode *node, const GameAiNodeId id, const int32_t gate) {

  const NavGraph *graph = &module.graph;

  const Box3 bounds = G_Ai_Node_StepBounds(G_PlayerBounds());

  const Vec3 rel = Vec3_Subtract(node->position, graph->mins);
  const int32_t x = (int32_t) floorf(rel.x / NAV_CELL_SIZE);
  const int32_t y = (int32_t) floorf(rel.y / NAV_CELL_SIZE);
  const int32_t z = (int32_t) floorf(rel.z / NAV_CELL_HEIGHT);

  const int32_t levels = (int32_t) ceilf(AI_MOVER_DROP_HEIGHT / NAV_CELL_HEIGHT);

  for (int32_t dz = -levels; dz <= levels; dz++) {
    for (int32_t dy = -1; dy <= 1; dy++) {
      for (int32_t dx = -1; dx <= 1; dx++) {

        const Vec3i xyz = MakeVec3i(x + dx, y + dy, z + dz);

        if (xyz.x < 0 || xyz.y < 0 || xyz.z < 0 || xyz.x >= graph->size.x || xyz.y >= graph->size.y || xyz.z >= graph->size.z) {
          continue;
        }

        const int32_t other = graph->cellNodes[(xyz.z * graph->size.y + xyz.y) * graph->size.x + xyz.x];
        if (other == -1) {
          continue;
        }

        if (!G_Ai_Node_LinkMoverTo(node, id, (GameAiNodeId) other, bounds, gate) || gate) {
          continue;
        }

        const GameAiNodeId linked = (GameAiNodeId) other;
        $(module.moverLinked, add, (ident) &linked);
      }
    }
  }

  for (GameAiNodeId other = module.graphNodes; other < module.nodes->count; other++) {
    const bool moves = AI_NODE(module.nodes, other)->model != 0;

    if (!moves && G_Ai_Node_LinkMoverTo(node, id, other, bounds, gate) && !gate) {
      $(module.moverLinked, add, (ident) &other);
    }
  }
}

/**
 * @brief Links the node on a model that moves to the nodes that do not move around it, with
 * `G_Ai_Node_LinkMoverAround`. A node that the model has carried into lava or slime is not linked.
 */
static void G_Ai_Node_LinkMover(const GameAiNodeId id) {

  GameAiNode *node = AI_NODE(module.nodes, id);

  if (node->moverStill) {

    for (size_t i = 0; i < node->moverLinks->count; i++) {
      const GameAiMoverLink *link = &VectorValue(node->moverLinks, GameAiMoverLink, i);
      G_Ai_Node_LinkDynamic(link->from, link->to, link->cost, link->travelType);

      const GameAiNodeId linked = link->from == id ? link->to : link->from;
      $(module.moverLinked, add, (ident) &linked);
    }
    return;
  }

  if (!node->moverLinks) {
    node->moverLinks = $(alloc(Vector), initWithSize, sizeof(GameAiMoverLink));
  } else {
    $(node->moverLinks, removeAll);
  }

  node->moverLinksPosition = node->position;
  node->moverLinksTime = gameLevel.time;

  if (gi.PointContents(node->position) & AI_CONTENTS_HAZARD) {
    return;
  }

  G_Ai_Node_LinkMoverAround(node, id, 0);
}

/**
 * @brief Links the nodes on different models that move to each other, both ways with walks, within
 * `AI_MOVER_LINK_DISTANCE` and a step of height, with a clear line for the player's box, raised by
 * a step, and floor under the line, so that a floor of several doors side by side is one floor. A
 * node that its model has carried into lava or slime is not linked. Two nodes that are both still
 * get the links of the last relink back, with no traces.
 */
static void G_Ai_Node_LinkMovers(void) {

  const Box3 bounds = G_Ai_Node_StepBounds(G_PlayerBounds());

  for (size_t i = 0; i < module.moverNodes->count; i++) {
    const GameAiNodeId a = AI_NODE_ID(module.moverNodes, i);
    GameAiNode *node = AI_NODE(module.nodes, a);

    if (!node->moverPairs) {
      node->moverPairs = $(alloc(Vector), initWithSize, sizeof(GameAiMoverLink));
    }

    if (node->moverStill) {
      for (size_t k = node->moverPairs->count; k > 0; k--) {
        const GameAiMoverLink *link = &VectorValue(node->moverPairs, GameAiMoverLink, k - 1);

        if (AI_NODE(module.nodes, link->to)->moverStill) {
          G_Ai_Node_LinkDynamic(link->from, link->to, link->cost, link->travelType);
          G_Ai_Node_LinkDynamic(link->to, link->from, link->cost, link->travelType);
        } else {
          $(node->moverPairs, removeAt, k - 1);
        }
      }
    } else {
      $(node->moverPairs, removeAll);
    }

    int32_t hazard = -1;

    for (size_t j = i + 1; j < module.moverNodes->count; j++) {
      const GameAiNodeId b = AI_NODE_ID(module.moverNodes, j);
      const GameAiNode *other = AI_NODE(module.nodes, b);

      if (node->moverStill && other->moverStill) {
        continue;
      }

      float distance;
      if (other->model == node->model || !G_Ai_Node_InReach(node->position, other->position, &distance)) {
        continue;
      }

      if (hazard == -1) {
        hazard = (gi.PointContents(node->position) & AI_CONTENTS_HAZARD) != 0;
      }

      if (hazard || (gi.PointContents(other->position) & AI_CONTENTS_HAZARD)) {
        continue;
      }

      if (!G_Ai_Node_CanWalkMover(node->position, other->position, bounds, &distance)) {
        continue;
      }

      G_Ai_Node_LinkDynamic(a, b, distance, NAV_TRAVEL_WALK);
      G_Ai_Node_LinkDynamic(b, a, distance, NAV_TRAVEL_WALK);

      const GameAiMoverLink link = {
        .from = a,
        .to = b,
        .cost = distance,
        .travelType = NAV_TRAVEL_WALK
      };

      $(node->moverPairs, add, (ident) &link);
    }
  }
}

/**
 * @brief Collects the doors with the `targetname` of the door of the inline model `m`, from `m` up.
 * @param doors Receives the doors, at most `MAX_MODELS`.
 * @return The count of `doors`, or 0 if the door of a lower inline model has the `targetname`, so
 * that the doors were collected for it.
 */
static int32_t G_Ai_Node_LinkPresses_doors(const int32_t m, GameEntity **doors) {

  const char *name = module.doors[m]->targetName;

  for (int32_t k = 1; k < m; k++) {
    if (module.doors[k] && module.doors[k]->targetName && !Str_Compare(module.doors[k]->targetName, name)) {
      return 0;
    }
  }

  int32_t numDoors = 0;

  for (int32_t k = m; k < MAX_MODELS; k++) {
    if (module.doors[k] && module.doors[k]->targetName && !Str_Compare(module.doors[k]->targetName, name)) {
      doors[numDoors++] = (GameEntity *) module.doors[k];
    }
  }

  return numDoors;
}

/**
 * @brief Collects the nodes on the `doors`, and their positions where the doors stand now.
 */
static void G_Ai_Node_LinkPresses_nodes(GameEntity **doors, const int32_t numDoors, Vector *nodes, Vector *positions) {

  for (size_t i = 0; i < module.moverNodes->count; i++) {
    const GameAiNodeId id = AI_NODE_ID(module.moverNodes, i);
    const GameAiNode *node = AI_NODE(module.nodes, id);

    for (int32_t k = 0; k < numDoors; k++) {
      if (node->mover == doors[k]) {
        $(nodes, add, (ident) &id);
        $(positions, add, (ident) &node->position);
        break;
      }
    }
  }
}

/**
 * @brief Puts the `doors` where a press moves them, and keeps where they stood in `origins` and
 * `angles`.
 */
static void G_Ai_Node_LinkPresses_pose(GameEntity **doors, const int32_t numDoors, Vec3 *origins, Vec3 *angles) {

  for (int32_t k = 0; k < numDoors; k++) {
    origins[k] = doors[k]->s.origin;
    angles[k] = doors[k]->s.angles;
    doors[k]->s.origin = doors[k]->moveInfo.endOrigin;
    doors[k]->s.angles = doors[k]->moveInfo.endAngles;
    gi.LinkEntity(doors[k]);
  }
}

/**
 * @brief Puts the `doors` back where `G_Ai_Node_LinkPresses_pose` found them.
 */
static void G_Ai_Node_LinkPresses_restore(GameEntity **doors, const int32_t numDoors, const Vec3 *origins,
                                          const Vec3 *angles) {

  for (int32_t k = 0; k < numDoors; k++) {
    doors[k]->s.origin = origins[k];
    doors[k]->s.angles = angles[k];
    gi.LinkEntity(doors[k]);
  }
}

/**
 * @brief Makes the press links of the `nodes` on doors that stand where a press moves them: to the
 * nodes that do not move, and to each other, both ways with walks. The nodes are moved with their
 * doors for the traces, and back to their `positions`. A node that the press moves into lava or
 * slime is not linked.
 * @param bounds The player's box, raised by a step.
 */
static void G_Ai_Node_LinkPresses_link(const Vector *nodes, const Vector *positions, const Box3 bounds) {

  for (size_t i = 0; i < nodes->count; i++) {
    GameAiNode *node = AI_NODE(module.nodes, AI_NODE_ID(nodes, i));
    node->position = G_Ai_Node_MoverPosition(node);
  }

  for (size_t i = 0; i < nodes->count; i++) {
    const GameAiNodeId a = AI_NODE_ID(nodes, i);
    GameAiNode *node = AI_NODE(module.nodes, a);

    if (gi.PointContents(node->position) & AI_CONTENTS_HAZARD) {
      continue;
    }

    G_Ai_Node_LinkMoverAround(node, a, node->model);

    for (size_t j = i + 1; j < nodes->count; j++) {
      const GameAiNodeId b = AI_NODE_ID(nodes, j);
      const GameAiNode *other = AI_NODE(module.nodes, b);

      float distance;
      if (other->model == node->model ||
          !G_Ai_Node_CanWalkMover(node->position, other->position, bounds, &distance)) {
        continue;
      }

      G_Ai_Node_LinkPress(a, b, distance, NAV_TRAVEL_WALK, node->model);
      G_Ai_Node_LinkPress(b, a, distance, NAV_TRAVEL_WALK, node->model);
    }
  }

  for (size_t i = 0; i < nodes->count; i++) {
    AI_NODE(module.nodes, AI_NODE_ID(nodes, i))->position = VectorValue(positions, Vec3, i);
  }
}

/**
 * @brief Makes the press links of the doors with a `targetname` that carry nodes: the links that the
 * nodes on the doors with that `targetname` make, to the nodes that do not move and to each other,
 * while the doors stand where a press moves them (`moveInfo.endOrigin` and `endAngles`). The doors
 * and their nodes are put there for the traces, and back.
 */
static void G_Ai_Node_LinkPresses(void) {

  if (!module.moverNodes) {
    return;
  }

  const Box3 bounds = G_Ai_Node_StepBounds(G_PlayerBounds());

  for (int32_t m = 1; m < MAX_MODELS; m++) {

    if (!module.doors[m] || !module.doors[m]->targetName) {
      continue;
    }

    GameEntity *doors[MAX_MODELS];

    const int32_t numDoors = G_Ai_Node_LinkPresses_doors(m, doors);
    if (!numDoors) {
      continue;
    }

    Vector *nodes = $(alloc(Vector), initWithSize, sizeof(GameAiNodeId));
    Vector *positions = $(alloc(Vector), initWithSize, sizeof(Vec3));

    G_Ai_Node_LinkPresses_nodes(doors, numDoors, nodes, positions);

    if (nodes->count) {
      Vec3 origins[MAX_MODELS], angles[MAX_MODELS];

      G_Ai_Node_LinkPresses_pose(doors, numDoors, origins, angles);
      G_Ai_Node_LinkPresses_link(nodes, positions, bounds);
      G_Ai_Node_LinkPresses_restore(doors, numDoors, origins, angles);
    }

    release(nodes);
    release(positions);
  }
}

/**
 * @brief Moves the nodes on models that move with their entities, and notes in `moverMoved` if any
 * has moved. A node that moves is tested again for its contents in this frame.
 */
static void G_Ai_Node_UpdateMovers_move(void) {

  for (size_t i = 0; i < module.moverNodes->count; i++) {
    GameAiNode *node = AI_NODE(module.nodes, AI_NODE_ID(module.moverNodes, i));

    if (node->mover) {
      const Vec3 position = G_Ai_Node_MoverPosition(node);

      if (!Vec3_Equal(position, node->position)) {
        node->position = position;
        node->frame = 0;
        module.moverMoved = true;
      }
    }
  }
}

/**
 * @brief Removes the links that the last relink made: those of the nodes that do not move to the
 * nodes that do, and those of the nodes that move to the nodes off their own model.
 */
static void G_Ai_Node_UpdateMovers_unlink(void) {

  for (size_t i = 0; i < module.moverLinked->count; i++) {
    G_Ai_Node_UnlinkModel(AI_NODE(module.nodes, AI_NODE_ID(module.moverLinked, i)), 0);
  }

  $(module.moverLinked, removeAll);

  for (size_t i = 0; i < module.moverNodes->count; i++) {
    GameAiNode *node = AI_NODE(module.nodes, AI_NODE_ID(module.moverNodes, i));
    G_Ai_Node_UnlinkModel(node, node->model);
  }
}

/**
 * @brief Marks each node on a model that moves as `moverStill` if it stands where it was last
 * linked, for less than `AI_MOVER_RELINK_MILLIS`, and outside of the reach of each model that has
 * moved since. If more than `AI_MOVER_MAX_MOVED` models have moved, or a node that has moved has no
 * entity, no node is still.
 */
static void G_Ai_Node_UpdateMovers_still(void) {

  const GameEntity *moved[AI_MOVER_MAX_MOVED];
  Box3 movedBounds[AI_MOVER_MAX_MOVED];
  size_t numMoved = 0;
  bool allMoved = false;

  for (size_t i = 0; i < module.moverNodes->count; i++) {
    const GameAiNode *node = AI_NODE(module.nodes, AI_NODE_ID(module.moverNodes, i));

    if (node->moverLinks && Vec3_Equal(node->position, node->moverLinksPosition)) {
      continue;
    }

    size_t j = 0;
    while (j < numMoved && moved[j] != node->mover) {
      j++;
    }

    if (j < numMoved) {
      continue;
    }

    if (!node->mover || numMoved == AI_MOVER_MAX_MOVED) {
      allMoved = true;
      break;
    }

    moved[numMoved] = node->mover;
    movedBounds[numMoved++] = Box3_Expand(node->mover->absBounds, AI_MOVER_LINK_DISTANCE + Box3_Size(PM_BOUNDS).x);
  }

  for (size_t i = 0; i < module.moverNodes->count; i++) {
    GameAiNode *node = AI_NODE(module.nodes, AI_NODE_ID(module.moverNodes, i));
    node->moverStill = !allMoved && node->moverLinks && Vec3_Equal(node->position, node->moverLinksPosition) &&
                       gameLevel.time - node->moverLinksTime < AI_MOVER_RELINK_MILLIS;

    for (size_t j = 0; j < numMoved && node->moverStill; j++) {
      node->moverStill = !Box3_ContainsPoint(movedBounds[j], node->position);
    }
  }
}

/**
 * @brief Moves the nodes on models that move with their entities, each frame, and links them
 * again to the nodes beside them every `AI_MOVER_LINK_MILLIS`, if any has moved since they were
 * last linked, and every `AI_MOVER_RELINK_MILLIS` if none has, so that a link that a player or
 * another model blocked when they came to rest comes back. A bot whose path loses a link plans a
 * new one.
 */
void G_Ai_Node_UpdateMovers(void) {

  if (!module.moverNodes || !module.moverNodes->count) {
    return;
  }

  G_Ai_Node_UpdateMovers_move();

  const uint32_t relinkTime = module.moverLinkTime - AI_MOVER_LINK_MILLIS + AI_MOVER_RELINK_MILLIS;

  if (module.moverLinkTime > gameLevel.time ||
      (module.moverLinkTime && !module.moverMoved && relinkTime > gameLevel.time)) {
    return;
  }

  module.moverLinkTime = gameLevel.time + AI_MOVER_LINK_MILLIS;
  module.moverMoved = false;

  G_Ai_Node_UpdateMovers_unlink();
  G_Ai_Node_UpdateMovers_still();

  for (size_t i = 0; i < module.moverNodes->count; i++) {
    G_Ai_Node_LinkMover(AI_NODE_ID(module.moverNodes, i));
  }

  G_Ai_Node_LinkMovers();
}

/**
 * @brief Readies the graph once the level's entities have spawned: after the first second of the
 * level, and again when `G_Ai_ReloadNodes` builds the graph anew after that. It reports the nodes
 * and links that the entities added, links the rides, finds the entities of the models that move,
 * their crushers and their press links, collects the platforms, and reports the items that no node
 * is near.
 */
void G_Ai_Node_Ready(void) {

  if (!module.nodes) {
    return;
  }

  const size_t addedNodes = module.nodes->count - module.graphNodes;
  size_t addedLinks = 0;

  for (size_t i = 0; i < module.nodes->count; i++) {
    const GameAiNode *node = AI_NODE(module.nodes, i);

    if (node->links) {
      addedLinks += node->links->count;
    }
  }

  addedLinks -= module.graphLinks;
  gi.Print("  Game loaded %zu additional nodes with %zu new links.\n", addedNodes, addedLinks);

  G_Ai_Node_LinkRides();

  G_Ai_Node_FindMovers();
  G_Ai_Node_LinkPresses();

  G_ForEachEntity(ent, {
    if (ent->classname && Str_Compare(ent->classname, "func_plat") == 0) {
      if (!module.platforms) {
        module.platforms = $(alloc(Vector), initWithSize, sizeof(GameEntity *));
      }
      $(module.platforms, add, &ent);
    }
  });

  G_Ai_Node_CheckItems();

  G_Ai_Probe_Graph();
}

/**
 * @brief Frees all navigation node data, including the backing node array.
 */
void G_Ai_Node_Shutdown(void) {

  if (module.nodes) {
    for (size_t i = 0; i < module.nodes->count; i++) {
      GameAiNode *node = AI_NODE(module.nodes, i);

      if (node->links) {
        release(node->links);
      }

      if (node->moverLinks) {
        release(node->moverLinks);
      }

      if (node->moverPairs) {
        release(node->moverPairs);
      }

      if (node->pressLinks) {
        release(node->pressLinks);
      }
    }

    module.nodes = release(module.nodes);
  }

  module.platforms = release(module.platforms);

  memset(module.doors, 0, sizeof(module.doors));

  module.moverNodes = release(module.moverNodes);
  module.moverLinked = release(module.moverLinked);
  module.moverLinkTime = 0;
  module.moverMoved = false;
  module.numCrushers = 0;

  Nav_Free(&module.graph);
  module.graphNodes = module.graphLinks = 0;

  free(module.pressCosts);
  free(module.pressStarts);
  free(module.pressSources);
  module.pressCosts = module.pressStarts = NULL;
  module.pressSources = NULL;
  module.pressCount = 0;

  G_Ai_Node_InvalidateSpatialIndex();
  G_Ai_Node_FreePathPool();
}

/**
 * @brief Releases the pathfinding queue and its entries.
 */
static void G_Ai_Node_FreePathPool(void) {

  G_Ai_HeapFree(&module.pathQueue);
  free(module.pathEntries);
  module.pathEntries = NULL;
  module.pathCapacity = 0;
  module.pathCount = 0;
}

/**
 * @brief Readies the pathfinding queue and entry pool for `capacity` nodes, reusing them when they suffice.
 */
static bool G_Ai_Node_EnsurePathPool(const size_t capacity) {

  if (module.pathQueue && module.pathCapacity >= capacity) {
    G_Ai_HeapReset(module.pathQueue);
    module.pathCount = 0;
    return true;
  }

  G_Ai_Node_FreePathPool();

  module.pathQueue = G_Ai_HeapCreate(capacity);
  module.pathEntries = malloc(sizeof(GameAiNodePriority) * capacity);

  if (!module.pathQueue || !module.pathEntries) {
    G_Ai_Node_FreePathPool();
    return false;
  }

  module.pathCapacity = capacity;
  return true;
}

/**
 * @brief Pushes the node `id` onto the pathfinding queue, at `priority`, with an entry of the pool.
 * @param search The name of the search, for the warning when the pool or the queue is full.
 * @param crossed For `G_Ai_Node_GateCosts`, true if the path crossed the doorway.
 * @return True if the node was pushed, false if the pool or the queue is full.
 */
static bool G_Ai_Node_PushPath(const char *search, const GameAiNodeId id, const float priority, const bool crossed) {

  if (module.pathCount == module.pathCapacity) {
    G_Warn("%s entry pool exhausted (capacity %zu)\n", search, module.pathCapacity);
    return false;
  }

  GameAiNodePriority *entry = &module.pathEntries[module.pathCount++];
  *entry = (GameAiNodePriority) { .id = id, .priority = priority, .crossed = crossed };

  if (!G_Ai_HeapPush(module.pathQueue, priority, entry)) {
    G_Warn("%s heap exhausted (capacity %zu)\n", search, module.pathCapacity);
    return false;
  }

  return true;
}

/**
 * @brief The highest drop that a search takes.
 */
#define AI_MAX_DROP_HEIGHT 512.f

/**
 * @brief The height of a drop above which the link costs more, by `AI_DROP_PENALTY_SCALE`.
 */
#define AI_DROP_PENALTY_START 128.f

/**
 * @brief The cost added for each unit of a drop above `AI_DROP_PENALTY_START`.
 */
#define AI_DROP_PENALTY_SCALE 0.5f

/**
 * @brief The health above which a client MUST stay after the damage of a drop, to take the drop.
 */
#define AI_DROP_HEALTH_MARGIN 8.f

/**
 * @brief The cost added for each point of damage of a drop.
 */
#define AI_DROP_DAMAGE_PENALTY_SCALE 6.f

/**
 * @brief The multiple of its time that a link into water costs. Water slows a player, who then
 * cannot dodge, at the lowest point of the map, under the fire and the splash of anyone above, so
 * a bot crosses water only where it saves much time, or where its goal is.
 */
#define AI_WATER_EXPOSURE 1.5f

/**
 * @brief The cost added to a link into a node at the edge of the void: half a cell, so that a path
 * keeps to the middle of a walkway where it can, and still crosses a narrow one where it must.
 */
#define AI_EDGE_PENALTY (NAV_CELL_SIZE * .5f)

/**
 * @brief Returns true if the segment between two nodes intersects lava or slime.
 */
static bool G_Ai_Node_LinkPassesHazard(const Vec3 from, const Vec3 to) {
  const Vec3 delta = Vec3_Subtract(to, from);
  const float length = Vec3_Length(delta);

  if (length <= 0.f) {
    return (gi.PointContents(from) & AI_CONTENTS_HAZARD) != 0;
  }

  const Vec3 dir = Vec3_Scale(delta, 1.f / length);
  const float step = 24.f;
  const int32_t samples = Maxi(1, (int32_t) ceilf(length / step));

  for (int32_t i = 0; i <= samples; i++) {
    const float dist = Minf(length, i * step);
    const Vec3 point = Vec3_Fmaf(from, dist, dir);

    if (gi.PointContents(point) & AI_CONTENTS_HAZARD) {
      return true;
    }
  }

  return false;
}

/**
 * @return The node, with its contents and whether a raised platform stands over it tested for the
 * current server frame, once a frame. A search tests them for each link that it tries, and the
 * tests trace the world and its entities. The cache is not a part of the node's value, so the node
 * MAY be const.
 */
static const GameAiNode *G_Ai_Node_Refresh(const GameAiNode *node) {

  GameAiNode *n = (GameAiNode *) node;

  if (n->frame != gameLevel.frameNum + 1) {
    n->frame = gameLevel.frameNum + 1;
    n->frameContents = n->cell != -1 && !n->model ? n->contents : gi.PointContents(n->position);
    n->raised = G_Ai_Node_RaisedPlatform(n->position) != NULL;
  }

  return n;
}

/**
 * @return The contents at the node: those that the build sampled, for a node of the built graph
 * that does not move, and those at its position now, for any other node, once a server frame.
 */
static int32_t G_Ai_Node_Contents(const GameAiNode *node) {
  return G_Ai_Node_Refresh(node)->frameContents;
}

/**
 * @see g_ai_node.h
 */
const NavLadderClimb *G_Ai_Node_GetClimb(const GameAiNodeId a, const GameAiNodeId b) {

  for (int32_t i = 0; i < module.graph.numClimbs; i++) {
    const NavLadderClimb *climb = &module.graph.climbs[i];

    if (climb->bottom == (int32_t) a && climb->top == (int32_t) b) {
      return climb;
    }
  }

  return NULL;
}

/**
 * @see g_ai_node.h
 */
const NavLadder *G_Ai_Node_GetLadder(const NavLadderClimb *climb) {
  return &module.graph.ladders[climb->ladder];
}

/**
 * @see g_ai_node.h
 */
bool G_Ai_Node_IsHazard(const GameAiNodeId node) {
  return (G_Ai_Node_Contents(AI_NODE(module.nodes, node)) & AI_CONTENTS_HAZARD) != 0;
}

/**
 * @see g_ai_node.h
 */
bool G_Ai_Node_IsLiquid(const GameAiNodeId node) {
  return (G_Ai_Node_Contents(AI_NODE(module.nodes, node)) & CONTENTS_MASK_LIQUID) != 0;
}

/**
 * @return True if the client and the node `start` of its search are both in lava, so that the
 * search MAY cross it to leave. A client in slime, which hurts slowly, searches as any other.
 */
static bool G_Ai_Node_EscapesLava(const GameClient *cl, const GameAiNodeId start) {
  return cl && cl->entity && cl->entity->waterLevel > WATER_NONE && (cl->entity->waterType & CONTENTS_LAVA) &&
         (G_Ai_Node_Contents(AI_NODE(module.nodes, start)) & CONTENTS_LAVA);
}

/**
 * @return The water level of the client at the node, for a node whose contents are `contents`:
 * `WATER_NONE` out of water, and `WATER_UNDER` if the water covers the client's eyes.
 */
static int32_t G_Ai_Node_WaterLevel(const GameClient *cl, const GameAiNode *node,
                                    const int32_t contents) {

  if (!(contents & CONTENTS_WATER)) {
    return WATER_NONE;
  }

  const float height = cl->ps.pmState.viewOffset.z + 1.f;
  const Vec3 eyes = Vec3_Add(node->position, MakeVec3(0.f, 0.f, height));

  return (gi.PointContents(eyes) & CONTENTS_WATER) ? WATER_UNDER : WATER_WAIST;
}

/**
 * @return The damage of a fall of `drop` units onto a node at `waterLevel`, as `G_ClientMove`
 * deals it: halved for each level of water, so an eighth under water.
 */
static float G_Ai_Node_FallDamage(const float drop, const int32_t gravity, const int32_t waterLevel) {

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
bool G_Ai_Node_IsLinkOpen(const GameAiLink *link) {

  if (!link->gate) {
    return true;
  }

  if (link->gate >= MAX_MODELS || !module.doors[link->gate]) {
    return false;
  }

  return G_func_door_IsAway(module.doors[link->gate]);
}

/**
 * @return True if the link crosses the doorway of a door with the `targetname` `doorName`.
 */
static bool G_Ai_Node_IsLinkGatedBy(const GameAiLink *link, const char *doorName) {

  if (!link->gate || link->gate >= MAX_MODELS || !module.doors[link->gate] || !doorName) {
    return false;
  }

  const char *name = module.doors[link->gate]->targetName;

  return name && !Str_Compare(name, doorName);
}

/**
 * @return True if the node stands on a door with the `targetname` `doorName`, which moves it.
 */
static bool G_Ai_Node_IsMovedBy(const GameAiNode *node, const char *doorName) {
  return node->mover && node->mover->targetName && doorName && !Str_Compare(node->mover->targetName, doorName);
}

/**
 * @see g_ai_node.h
 */
bool G_Ai_Node_HasGate(const char *doorName) {

  for (uint32_t i = 0; i < G_Ai_Node_Count(); i++) {
    const GameAiNode *node = AI_NODE(module.nodes, i);

    for (size_t j = 0; node->links && j < node->links->count; j++) {
      if (G_Ai_Node_IsLinkGatedBy(AI_LINK(node->links, j), doorName)) {
        return true;
      }
    }

    for (size_t j = 0; node->pressLinks && j < node->pressLinks->count; j++) {
      if (G_Ai_Node_IsLinkGatedBy(AI_LINK(node->pressLinks, j), doorName)) {
        return true;
      }
    }
  }

  return false;
}

/**
 * @return The cost for the client to travel the link out of `node`, with penalties for drops that
 * hurt and for a node at the edge of the void, or -1 if the client must not use the link. A link
 * that the game added is tested for lava and slime along its line, except a walk between a node on
 * a model that moves and a node off that model, which `G_Ai_Node_HasFloor` tested when it was
 * made. A crouch link, and any link into or out of a crouch node, which the build made for
 * Quetoo's crouched box, is refused to a client whose movement crouches to a taller box, or does not
 * crouch, as Quake's. A link within lava or slime, and a drop into it, are refused, except in a
 * search of a client in lava, from a node in it (`escape`), so that such a search crosses the lava
 * to leave.
 * @param fromHazard True if `node` is in lava or slime.
 * @param doorName The `targetname` of the doors that the client is taken to press, whose links are
 * open if they are closed now, and closed if they are open now, or NULL.
 */
static float G_Ai_Node_StepCost(const GameClient *cl, const GameAiNode *node, const bool fromHazard,
                                const GameAiLink *link, const char *doorName) {

  const GameAiNode *linkNode = AI_NODE(module.nodes, link->id);
  const float drop = node->position.z - linkNode->position.z;
  const int32_t linkContents = G_Ai_Node_Contents(linkNode);
  const bool toHazard = (linkContents & AI_CONTENTS_HAZARD) != 0;

  if (G_Ai_Node_Refresh(linkNode)->raised) {
    return -1.f;
  }

  if (G_Ai_Node_IsCrushing(link->id)) {
    return -1.f;
  }

  if (G_Ai_Node_IsLinkOpen(link) == G_Ai_Node_IsLinkGatedBy(link, doorName)) {
    return -1.f;
  }

  if (cl && (link->travelType == NAV_TRAVEL_CROUCH || node->crouch || linkNode->crouch) &&
      Box3_Size(cl->ps.pmState.params.boundsDucked).z > Box3_Size(PM_CROUCHED_BOUNDS).z) {
    return -1.f;
  }

  if (fromHazard && toHazard && !module.escape) {
    return -1.f;
  }

  const bool builtLink = node->cell != -1 && linkNode->cell != -1 && !node->model && !linkNode->model;
  const bool moverWalk = node->model != linkNode->model && link->travelType == NAV_TRAVEL_WALK;
  if (!fromHazard && !builtLink && !moverWalk &&
      (link->hazardTested ? link->hazard : G_Ai_Node_LinkPassesHazard(node->position, linkNode->position))) {
    return -1.f;
  }

  if (drop > AI_MAX_DROP_HEIGHT) {
    return -1.f;
  }

  if (drop > 0.f && toHazard && !module.escape) {
    return -1.f;
  }

  float dropPenalty = 0.f;
  if (drop > AI_DROP_PENALTY_START) {
    dropPenalty = (drop - AI_DROP_PENALTY_START) * AI_DROP_PENALTY_SCALE;
  }

  if (cl && cl->entity) {
    float estimatedDamage = G_Ai_Node_FallDamage(drop, G_LevelGravity(), WATER_NONE);
    if (estimatedDamage > 0.f) {
      const int32_t waterLevel = G_Ai_Node_WaterLevel(cl, linkNode, linkContents);
      estimatedDamage = G_Ai_Node_FallDamage(drop, G_LevelGravity(), waterLevel);
    }

    if (estimatedDamage + AI_DROP_HEALTH_MARGIN >= cl->entity->health) {
      return -1.f;
    }

    dropPenalty += estimatedDamage * AI_DROP_DAMAGE_PENALTY_SCALE;
  }

  const float exposure = (linkContents & CONTENTS_WATER) ? AI_WATER_EXPOSURE : 1.f;

  return link->cost * exposure + dropPenalty + (linkNode->edge ? AI_EDGE_PENALTY : 0.f);
}

/**
 * @return The path that `G_Ai_Node_FindPath` found from `start` to `end`, as a Vector of
 * `GameAiNodeId`, followed back from `end` along each node's `cameFrom`.
 */
static Vector *G_Ai_Node_FindPath_path(const GameAiNodeId start, const GameAiNodeId end) {

  Vector *path = $(alloc(Vector), initWithSize, sizeof(GameAiNodeId));
  $(path, insert, (void *) &end, 0);

  for (GameAiNodeId from = end; from != start;) {
    from = AI_NODE(module.nodes, from)->cameFrom;
    $(path, insert, &from, 0);
  }

  return path;
}

/**
 * @see g_ai_node.h
 */
Vector *G_Ai_Node_FindPath(const GameClient *cl, const GameAiNodeId start, const GameAiNodeId end, const float maxCost,
                           float *length) {

  if (length) {
    *length = 0;
  }

  if (start >= G_Ai_Node_Count() || end >= G_Ai_Node_Count()) {
    return NULL;
  }

  const size_t costsStartedWords = ((size_t) module.nodes->count + 31u) / 32u;
  uint32_t *costsStarted = calloc(costsStartedWords, sizeof(*costsStarted));
  if (!costsStarted) {
    return NULL;
  }

  const size_t heapCapacity = (size_t) module.nodes->count * 4 + 16;
  if (!G_Ai_Node_EnsurePathPool(heapCapacity)) {
    free(costsStarted);
    return NULL;
  }

  bool finished = false;
  bool full = !G_Ai_Node_PushPath("Path search", start, 0.f, false);

  AI_NODE(module.nodes, start)->cost = 0;
  module.escape = G_Ai_Node_EscapesLava(cl, start);
  costsStarted[start / 32] |= (uint32_t) 1 << (start % 32);

  while (!full) {
    const GameAiNodePriority *current = (GameAiNodePriority *) G_Ai_HeapPop(module.pathQueue);
    if (!current) {
      break;
    }

    if (current->id == end) {
      finished = true;
      break;
    }

    GameAiNode *node = AI_NODE(module.nodes, current->id);

    if (current->priority > node->cost + 0.0001f || !node->links || !node->links->count) {
      continue;
    }

    const bool fromHazard = (G_Ai_Node_Contents(node) & AI_CONTENTS_HAZARD) != 0;

    for (size_t i = 0; i < node->links->count; i++) {
      const GameAiLink *link = AI_LINK(node->links, i);
      const float stepCost = G_Ai_Node_StepCost(cl, node, fromHazard, link, NULL);

      if (stepCost < 0.f) {
        continue;
      }

      GameAiNode *linkNode = AI_NODE(module.nodes, link->id);
      const float newCost = node->cost + stepCost;

      if (maxCost > 0.f && newCost > maxCost) {
        continue;
      }

      const GameAiNodeId linkIndex = link->id;
      const bool found = (costsStarted[linkIndex / 32] & ((uint32_t) 1u << (linkIndex % 32))) != 0;

      if (found && newCost >= linkNode->cost) {
        continue;
      }

      if (!G_Ai_Node_PushPath("Path search", link->id, newCost, false)) {
        full = true;
        break;
      }

      costsStarted[linkIndex / 32] |= (uint32_t) 1u << (linkIndex % 32);
      linkNode->cost = newCost;
      linkNode->cameFrom = current->id;
    }
  }

  Vector *path = NULL;

  if (finished) {
    path = G_Ai_Node_FindPath_path(start, end);

    if (length) {
      *length = AI_NODE(module.nodes, end)->cost;
    }
  }

  module.escape = false;

  free(costsStarted);
  return path;
}

/**
 * @see g_ai_node.h
 */
bool G_Ai_Node_TravelCosts(const GameClient *cl, const GameAiNodeId start, float *costs) {

  if (start >= G_Ai_Node_Count()) {
    return false;
  }

  for (uint32_t i = 0; i < module.nodes->count; i++) {
    costs[i] = FLT_MAX;
  }

  const size_t heapCapacity = (size_t) module.nodes->count * 4 + 16;
  if (!G_Ai_Node_EnsurePathPool(heapCapacity)) {
    return false;
  }

  if (!G_Ai_Node_PushPath("Travel cost", start, 0.f, false)) {
    return false;
  }

  costs[start] = 0.f;
  module.escape = G_Ai_Node_EscapesLava(cl, start);

  for (;;) {
    const GameAiNodePriority *current = (GameAiNodePriority *) G_Ai_HeapPop(module.pathQueue);
    if (!current) {
      break;
    }

    if (current->priority > costs[current->id]) {
      continue;
    }

    const GameAiNode *node = AI_NODE(module.nodes, current->id);

    if (!node->links || !node->links->count) {
      continue;
    }

    const bool fromHazard = (G_Ai_Node_Contents(node) & AI_CONTENTS_HAZARD) != 0;

    for (size_t i = 0; i < node->links->count; i++) {
      const GameAiLink *link = AI_LINK(node->links, i);
      const float stepCost = G_Ai_Node_StepCost(cl, node, fromHazard, link, NULL);

      if (stepCost < 0.f) {
        continue;
      }

      const float newCost = costs[current->id] + stepCost;

      if (newCost >= costs[link->id]) {
        continue;
      }

      costs[link->id] = newCost;

      if (!G_Ai_Node_PushPath("Travel cost", link->id, newCost, false)) {
        module.escape = false;
        return false;
      }
    }
  }

  module.escape = false;

  return true;
}

/**
 * @brief Sizes the arrays of `G_Ai_Node_GateCosts` for `count` nodes in each of its two phases,
 * before and after the doors, and marks each node of both phases as not reached.
 */
static void G_Ai_Node_GateCosts_alloc(const uint32_t count) {

  if (count > module.pressCount) {
    module.pressCosts = realloc(module.pressCosts, 2 * count * sizeof(float));
    module.pressStarts = realloc(module.pressStarts, 2 * count * sizeof(float));
    module.pressSources = realloc(module.pressSources, 2 * count * sizeof(GameAiNodeId));

    if (!module.pressCosts || !module.pressStarts || !module.pressSources) {
      G_Error("Failed to allocate gate costs for %u nodes\n", count);
    }

    module.pressCount = count;
  }

  for (uint32_t i = 0; i < 2 * count; i++) {
    module.pressCosts[i] = FLT_MAX;
  }
}

/**
 * @brief Starts a path of `G_Ai_Node_GateCosts` at each of the `sources` that the client reaches,
 * at the source's cost in `costs`, before the doors. A source listed twice starts once.
 * @return False if the pathfinding pool or queue is full.
 */
static bool G_Ai_Node_GateCosts_seed(const float *costs, const GameAiNodeId *sources, const int32_t numSources,
                                     const uint32_t count) {

  for (int32_t i = 0; i < numSources; i++) {
    const GameAiNodeId source = sources[i];

    if (source >= count || costs[source] == FLT_MAX || costs[source] >= module.pressCosts[source]) {
      continue;
    }

    module.pressCosts[source] = module.pressStarts[source] = costs[source];
    module.pressSources[source] = source;

    if (!G_Ai_Node_PushPath("Gate cost", source, costs[source], false)) {
      return false;
    }
  }

  return true;
}

/**
 * @see g_ai_node.h
 */
bool G_Ai_Node_GateCosts(const GameClient *cl, const float *costs, const GameAiNodeId *sources, const int32_t numSources,
                         const char *doorName, const float window, const int32_t key, GameAiGateCost *gateCosts) {

  const uint32_t count = G_Ai_Node_Count();

  if (count == 0) {
    return false;
  }

  G_Ai_Node_GateCosts_alloc(count);

  const size_t heapCapacity = (size_t) count * 8 + 16;
  if (!G_Ai_Node_EnsurePathPool(heapCapacity)) {
    return false;
  }

  if (!G_Ai_Node_GateCosts_seed(costs, sources, numSources, count)) {
    return false;
  }

  for (;;) {
    const GameAiNodePriority *current = (GameAiNodePriority *) G_Ai_HeapPop(module.pathQueue);
    if (!current) {
      break;
    }

    const bool crossed = current->crossed;
    const size_t slot = (crossed ? count : 0) + current->id;
    const float cost = module.pressCosts[slot];

    if (current->priority > cost) {
      continue;
    }

    const GameAiNode *node = AI_NODE(module.nodes, current->id);
    const size_t numLinks = node->links ? node->links->count : 0;
    const size_t numPress = node->pressLinks ? node->pressLinks->count : 0;

    const float start = module.pressStarts[slot];
    const GameAiNodeId source = module.pressSources[slot];
    const bool fromHazard = (G_Ai_Node_Contents(node) & AI_CONTENTS_HAZARD) != 0;
    const bool fromMoved = G_Ai_Node_IsMovedBy(node, doorName);

    for (size_t i = 0; i < numLinks + numPress; i++) {
      const bool press = i >= numLinks;
      const GameAiLink *link = press ? AI_LINK(node->pressLinks, i - numLinks) : AI_LINK(node->links, i);

      if (press && !G_Ai_Node_IsLinkGatedBy(link, doorName)) {
        continue;
      }

      const GameAiNode *linkNode = AI_NODE(module.nodes, link->id);
      const bool toMoved = G_Ai_Node_IsMovedBy(linkNode, doorName);
      const bool onDoors = !press && fromMoved && linkNode->model == node->model;

      if (!press && !onDoors && (fromMoved || toMoved)) {
        continue;
      }

      const float stepCost = press || onDoors ? link->cost : G_Ai_Node_StepCost(cl, node, fromHazard, link, doorName);

      if (stepCost < 0.f) {
        continue;
      }

      const float newCost = cost + stepCost;
      const bool gated = press || G_Ai_Node_IsLinkGatedBy(link, doorName);
      const bool crosses = crossed || gated;

      if ((!crossed || gated) && newCost - start > window) {
        continue;
      }

      const size_t next = (crosses ? count : 0) + link->id;

      if (newCost >= module.pressCosts[next] || (crosses && !toMoved && newCost >= costs[link->id])) {
        continue;
      }

      module.pressCosts[next] = newCost;
      module.pressStarts[next] = start;
      module.pressSources[next] = source;

      if (crosses && newCost < gateCosts[link->id].cost) {
        gateCosts[link->id] = (GameAiGateCost) { .cost = newCost, .key = key, .source = source };
      }

      if (!G_Ai_Node_PushPath("Gate cost", link->id, newCost, crosses)) {
        return false;
      }
    }
  }

  return true;
}

/**
 * @brief Drops a node under the entity, onto the floor below it, and links it both ways to the
 * nearest node that does not move, and to each neighbor of that node which links back to it, does
 * not move, is reached by a walk or a crouch, is in sight of the entity, and has floor under the
 * line to the new node.
 * @return True if the node was made and linked.
 */
bool G_Ai_Node_AddEntity(GameEntity *ent) {

  CollisionTrace down = gi.Trace(ent->s.origin, Vec3_Subtract(ent->s.origin, MakeVec3(0, 0, MAX_WORLD_COORD)), Box3_Zero(), NULL, CONTENTS_MASK_SOLID);
  Vec3 pos;

  if (down.fraction == 1.0) {
    pos = ent->s.origin;
  } else {
    pos = Vec3_Subtract(down.end, MakeVec3(0.f, 0.f, G_PlayerBounds().mins.z));
  }

  const GameAiNodeId srcNode = G_Ai_Node_FindClosestStatic(pos, 512.f, true, true);

  if (srcNode == AI_NODE_INVALID) {
    return false;
  }

  const Vector *srcLinks = G_Ai_Node_GetLinks(srcNode);

  const GameAiNodeId newNode = G_Ai_Node_Create(pos);

  if (newNode == AI_NODE_INVALID) {
    return false;
  }

  const float dist = Vec3_Distance(G_Ai_Node_GetPosition(srcNode), ent->s.origin);

  G_Ai_Node_Link(srcNode, newNode, dist);
  G_Ai_Node_Link(newNode, srcNode, dist);

  if (srcLinks) {

    for (size_t i = 0; i < srcLinks->count; i++) {
      const GameAiLink *link = AI_LINK(srcLinks, i);

      if (!G_Ai_Node_IsLinked(link->id, srcNode) || AI_NODE(module.nodes, link->id)->model) {
        continue;
      }

      if (link->travelType > NAV_TRAVEL_CROUCH) {
        continue;
      }

      const Vec3 linkPos = G_Ai_Node_GetPosition(link->id);

      if (gi.Trace(ent->s.origin, linkPos, Box3_Zero(), NULL, CONTENTS_MASK_SOLID).fraction < 1.0 ||
          !G_Ai_Node_HasFloor(linkPos, pos)) {
        continue;
      }

      const float dist = Vec3_Distance(linkPos, ent->s.origin);

      G_Ai_Node_Link(link->id, newNode, dist);
      G_Ai_Node_Link(newNode, link->id, dist);
    }
  }

  ent->node = newNode;
  return true;
}
