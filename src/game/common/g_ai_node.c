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
#include "collision/cm_bsp.h"
#include "collision/nav.h"

#define AI_NODE(vector, index) VectorElement((vector), GameAiNode, (index))
#define AI_LINK(vector, index) VectorElement((vector), GameAiLink, (index))
#define AI_NODE_ID(vector, index) (VectorValue((vector), GameAiNodeId, (index)))
#define AI_PLATFORM(vector, index) (VectorValue((vector), GameEntity *, (index)))

/**
 * @brief Cached spatial acceleration structures for the navigation graph.
 *
 * The kd-tree accelerates `G_Ai_Node_FindClosest` queries (O(log n) average vs.
 * the previous O(n) linear scan), and is rebuilt lazily after any mutation
 * of the global node array.
 */
static struct GameAiKdTree *aiNodesKdtree = NULL;
static Vec3 *aiNodesKdtreePositions = NULL;
static size_t aiNodesKdtreeCount = 0;

/**
 * @brief Invalidates the cached kd-tree. Must be called whenever the
 * underlying `aiNodes` array is mutated (add/remove/move/reload).
 */
static void G_Ai_Node_InvalidateSpatialIndex(void) {
  if (aiNodesKdtree) {
    G_Ai_KdTreeFree(&aiNodesKdtree);
  }
  if (aiNodesKdtreePositions) {
    free(aiNodesKdtreePositions);
    aiNodesKdtreePositions = NULL;
  }
  aiNodesKdtreeCount = 0;
}

/**
 * @brief The most steps that a learned jump can have, its run-up included.
 */
#define AI_JUMP_MAX_STEPS 512

/**
 * @brief The run-up, in milliseconds, that a learned jump keeps before its takeoff.
 */
#define AI_JUMP_RUN_UP_MILLIS 250

/**
 * @brief The time on the ground, in milliseconds, that ends a learned jump. A jump within it
 * continues the same jump, as a trick jump does.
 */
#define AI_JUMP_LAND_MILLIS 100

/**
 * @brief The longest jump that the recorder learns, in milliseconds.
 */
#define AI_JUMP_MAX_MILLIS 4000

/**
 * @brief The distance from a node within which a learned jump starts or lands at it.
 */
#define AI_JUMP_NODE_DISTANCE 48.f

/**
 * @brief The most that a replayed jump may stray from the recorded origin of any step.
 */
#define AI_JUMP_REPLAY_TOLERANCE 16.f

/**
 * @brief A jump is learned only if the graph's best path is this much more costly, or absent.
 */
#define AI_JUMP_SHORTCUT 1.5f

/**
 * @brief One recorded command, and the player state before it.
 */
typedef struct {
  NavJumpStep step;
  Vec3 origin, velocity;
  uint16_t flags, time;
  bool grounded;
} GameAiJumpSample;

/**
 * @brief The recorder's progress through a jump.
 */
typedef enum {
  AI_JUMP_IDLE,
  AI_JUMP_AIRBORNE,
  AI_JUMP_LANDED,
} GameAiJumpState;

/**
 * @brief Records the jumps of the developer's player, to learn the ones that the graph lacks.
 */
typedef struct {
  bool primed;
  Vec3 origin, velocity;
  uint16_t flags, time;
  bool grounded;

  GameAiJumpSample samples[AI_JUMP_MAX_STEPS];
  int32_t count;

  GameAiJumpState state;
  int32_t landing;
  uint32_t millis, landedMillis;
} GameAiJumpRecorder;

/**
 * @brief State used during player roam recording for AI node development.
 */
static struct {
  Vec3 position;

  Vec3 floorPosition;

  GameAiNodeId lastNodes[2];

  bool awaitLanding, isJumping, isWaterJump, onMover;

  InputButtonState latchedButtons, oldButtons, buttons;

  Vector *testPath;

  uint32_t fileNodes, fileLinks;

  bool doNoding, dropNodes;

  bool compiled;

  int32_t *voxelNodes;

  NavHeader navFile;
  NavNode *navNodes;
  NavLink *navLinks;

  Vector *jumps;
  Vector *jumpSteps;

  GameAiJumpRecorder recorder;
} module;

/**
 * @brief Returns a test path between the last two recorded nodes for visualization.
 */
Vector *G_Ai_Node_TestPath(void) {

  if (!g_aiNodeDev->integer) {
    return NULL;
  }

  if (module.testPath) {
    release(module.testPath);
    module.testPath = NULL;
  }

  if (module.lastNodes[0] == AI_NODE_INVALID ||
    module.lastNodes[1] == AI_NODE_INVALID) {
    return NULL;
  }

  return module.testPath = G_Ai_Node_FindPath(NULL, module.lastNodes[1], module.lastNodes[0], G_Ai_Node_Heuristic, 0.f, NULL);
}

/**
 * @brief An AI navigation node with world position, outgoing links, and pathfinding state.
 */
typedef struct {
  // persisted to disk

  Vec3 position;
  Vector *links;

  // only set by compiled graphs

  int32_t voxel;
  int32_t contents;

  // only used for nav purposes

  float cost;
  GameAiNodeId cameFrom;
} GameAiNode;

/**
 * @brief The global array of navigation nodes for the current map.
 */
static Vector *aiNodes;

/**
 * @brief The global array of platforms for the current map.
 */
static Vector *aiPlatforms;

/**
 * @brief Returns the index of a node pointer within the global node array.
 */
static inline GameAiNodeId G_Ai_Node_Index(const GameAiNode *node) {
  return node - (GameAiNode *) aiNodes->elements;
}

static void G_Ai_Node_FreePathPool(void);

/**
 * @brief Returns true if the given node is visible (unobstructed) from the specified position.
 */
static bool G_Ai_Node_Visible(const Vec3 position, const GameAiNodeId node) {

  return gi.Trace(position, G_Ai_Node_GetPosition(node), Box3_Zero(), NULL, CONTENTS_SOLID | CONTENTS_WINDOW).fraction == 1.0f;
}

/**
 * @brief Returns the total number of navigation nodes currently loaded.
 */
uint32_t G_Ai_Node_Count(void) {
  return aiNodes ? (uint32_t) aiNodes->count : 0;
}

/**
 * @brief Lazily (re)builds the kd-tree spatial index over aiNodes.
 * Returns true on success. The index is invalidated whenever the node
 * array is mutated (see G_Ai_Node_InvalidateSpatialIndex callers).
 */
static bool G_Ai_Node_EnsureSpatialIndex(void) {

  if (!aiNodes || aiNodes->count == 0) {
    return false;
  }

  if (aiNodesKdtree && aiNodesKdtreeCount == aiNodes->count) {
    return true;
  }

  G_Ai_Node_InvalidateSpatialIndex();

  const size_t n = aiNodes->count;
  aiNodesKdtreePositions = malloc(sizeof(Vec3) * n);
  if (!aiNodesKdtreePositions) {
    return false;
  }

  for (size_t i = 0; i < n; i++) {
    aiNodesKdtreePositions[i] = AI_NODE(aiNodes, i)->position;
  }

  aiNodesKdtree = G_Ai_KdTreeCreate(aiNodesKdtreePositions, n);
  if (!aiNodesKdtree) {
    free(aiNodesKdtreePositions);
    aiNodesKdtreePositions = NULL;
    return false;
  }

  aiNodesKdtreeCount = n;
  return true;
}

typedef struct {
  Vec3 position;
  bool onlyVisible;
  bool preferLevel;
} GameAiNodeQueryFilter;

/**
 * @brief Whether a node qualifies for `G_Ai_Node_FindClosest`, and at what distance.
 */
static bool G_Ai_Node_FindClosestFilter(const size_t nodenum, void *data, float *distance) {
  const GameAiNodeQueryFilter *filter = data;
  const GameAiNode *node = AI_NODE(aiNodes, nodenum);

  Vec3 dir = Vec3_Subtract(filter->position, node->position);

  if (filter->preferLevel && !(gi.PointContents(node->position) & CONTENTS_MASK_LIQUID)) {
    dir.z *= 4.0f;
  }

  *distance = Vec3_LengthSquared(dir);

  return !filter->onlyVisible || G_Ai_Node_Visible(filter->position, nodenum);
}

/**
 * @brief Finds the closest compiled node in the voxels around the given position.
 * @details Any node outside of the voxels searched is at least `BSP_VOXEL_SIZE` from the
 * position, so a node found within that distance is the closest of all.
 * @param maxDistance The squared distance, at most `BSP_VOXEL_SIZE` squared, to search.
 */
static GameAiNodeId G_Ai_Node_FindClosestVoxel(GameAiNodeQueryFilter *filter, const float maxDistance) {

  const CollisionBsp *bsp = gi.Bsp();

  const Vec3 rel = Vec3_Subtract(filter->position, bsp->voxelBounds.mins);
  const int32_t x = (int32_t) floorf(rel.x / BSP_VOXEL_SIZE);
  const int32_t y = (int32_t) floorf(rel.y / BSP_VOXEL_SIZE);
  const int32_t z = (int32_t) floorf(rel.z / BSP_VOXEL_SIZE);

  GameAiNodeId closest = AI_NODE_INVALID;
  float closestDistance = maxDistance;

  for (int32_t dz = -2; dz <= 1; dz++) {
    for (int32_t dy = -1; dy <= 1; dy++) {
      for (int32_t dx = -1; dx <= 1; dx++) {

        const Vec3i xyz = MakeVec3i(x + dx, y + dy, z + dz);

        if (xyz.x < 0 || xyz.y < 0 || xyz.z < 0) {
          continue;
        }

        if (xyz.x >= bsp->voxelSize.x || xyz.y >= bsp->voxelSize.y || xyz.z >= bsp->voxelSize.z) {
          continue;
        }

        const int32_t voxel = (xyz.z * bsp->voxelSize.y + xyz.y) * bsp->voxelSize.x + xyz.x;
        const int32_t node = module.voxelNodes[voxel];

        if (node == -1) {
          continue;
        }

        float distance;
        if (!G_Ai_Node_FindClosestFilter(node, filter, &distance)) {
          continue;
        }

        if (distance <= closestDistance) {
          closestDistance = distance;
          closest = node;
        }
      }
    }
  }

  return closest;
}

/**
 * @brief Finds the closest navigation node to the given position within the specified distance.
 *
 * Uses the cached kd-tree spatial index (built lazily) to gather candidate
 * nodes within `maxDistance` and selects the best one that passes the
 * `onlyVisible` / `preferLevel` filters. Falls back to a linear scan if
 * the spatial index is unavailable.
 */
GameAiNodeId G_Ai_Node_FindClosest(const Vec3 position, const float maxDistance, const bool onlyVisible, const bool preferLevel) {

  if (!aiNodes || aiNodes->count == 0) {
    return AI_NODE_INVALID;
  }

  GameAiNodeId closest = AI_NODE_INVALID;
  float closestDist = 0;
  const float distSquared = maxDistance * maxDistance;

  GameAiNodeQueryFilter filter = {
    .position = position,
    .onlyVisible = onlyVisible,
    .preferLevel = preferLevel
  };

  if (module.compiled) {
    closest = G_Ai_Node_FindClosestVoxel(&filter, Minf(distSquared, BSP_VOXEL_SIZE * BSP_VOXEL_SIZE));
    if (closest != AI_NODE_INVALID) {
      return closest;
    }
  }

  if (G_Ai_Node_EnsureSpatialIndex()) {

    const size_t node = G_Ai_KdTreeQuery(aiNodesKdtree, position, maxDistance, G_Ai_Node_FindClosestFilter, &filter);

    return node == SIZE_MAX ? AI_NODE_INVALID : (GameAiNodeId) node;
  }

  // Fallback: linear scan (kd-tree build failed).
  for (size_t i = 0; i < aiNodes->count; i++) {
    const GameAiNode *node = AI_NODE(aiNodes, i);

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
GameAiNodeId G_Ai_Node_Create(const Vec3 position) {

  if (gi.PointContents(position) & CONTENTS_MASK_SOLID) {
    G_Ai_Debug("Rejected node at %s (inside solid)\n", vtos(position));
    return AI_NODE_INVALID;
  }

  if (!aiNodes) {
    aiNodes = $(alloc(Vector), initWithSize, sizeof(GameAiNode));
  }

  GameAiNode node = (GameAiNode) {
    .position = position,
    .voxel = -1,
    .contents = gi.PointContents(position) & CONTENTS_MASK_LIQUID
  };
  $(aiNodes, add, &node);

  G_Ai_Node_InvalidateSpatialIndex();

  G_Ai_Debug("Dropped new node %zu\n", aiNodes->count - 1);

  return aiNodes->count - 1;
}

/**
 * @brief Returns true if node a has a directed link to node b.
 */
bool G_Ai_Node_IsLinked(const GameAiNodeId a, const GameAiNodeId b) {
  const GameAiNode *nodeA = AI_NODE(aiNodes, a);

  if (nodeA->links) {
    for (size_t i = 0; i < nodeA->links->count; i++) {
      const GameAiLink *link = AI_LINK(nodeA->links, i);

      if (link->id == b) {
        return true;
      }
    }
  }

  return false;
}

/**
 * @return The link from node `a` to node `b`, or `NULL`.
 */
const GameAiLink *G_Ai_Node_GetLink(const GameAiNodeId a, const GameAiNodeId b) {

  if (!aiNodes || a >= aiNodes->count) {
    return NULL;
  }

  const GameAiNode *node = AI_NODE(aiNodes, a);

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
 * @see g_ai_node.h
 */
const NavJump *G_Ai_Node_GetJump(const int32_t index) {

  if (!module.jumps || index < 0 || index >= (int32_t) module.jumps->count) {
    return NULL;
  }

  return VectorElement(module.jumps, NavJump, index);
}

/**
 * @see g_ai_node.h
 */
const NavJumpStep *G_Ai_Node_GetJumpSteps(const NavJump *jump) {
  return VectorElement(module.jumpSteps, NavJumpStep, jump->firstStep);
}

/**
 * @brief Returns the array of outgoing links for the specified node.
 */
const Vector *G_Ai_Node_GetLinks(const GameAiNodeId a) {
  const GameAiNode *nodeA = AI_NODE(aiNodes, a);
  return nodeA->links;
}

/**
 * @brief Creates a directed link from node a to node b with the given traversal cost and travel type.
 */
void G_Ai_Node_LinkTravel(const GameAiNodeId a, const GameAiNodeId b, const float cost, const int32_t travelType) {

  if (!aiNodes || a >= aiNodes->count || b >= aiNodes->count) {
    return;
  }

  if (a == b || G_Ai_Node_IsLinked(a, b)) {
    return;
  }

  GameAiNode *nodeA = AI_NODE(aiNodes, a);

  if (!nodeA->links) {
    nodeA->links = $(alloc(Vector), initWithSize, sizeof(GameAiLink));
  }

  GameAiLink link = (GameAiLink) {
    .id = b,
    .cost = cost,
    .travelType = travelType,
    .jump = -1
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
 * @brief Creates a link between two nodes using their Euclidean distance as the cost.
 */
static inline void G_Ai_Node_LinkDefault(const GameAiNodeId a, const GameAiNodeId b, const bool bidirectional) {

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
static void G_Ai_Node_Unlink(const GameAiNodeId a, const GameAiNodeId b) {
  {
    GameAiNode *nodeA = AI_NODE(aiNodes, a);

    if (nodeA->links) {
      for (size_t i = 0; i < nodeA->links->count; i++) {
        const GameAiLink *link = AI_LINK(nodeA->links, i);

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
    GameAiNode *nodeB = AI_NODE(aiNodes, b);
  
    if (nodeB->links) {
      for (size_t i = 0; i < nodeB->links->count; i++) {
        const GameAiLink *link = AI_LINK(nodeB->links, i);

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
static void G_Ai_Node_UnlinkAll(const GameAiNodeId id) {
  const GameAiNode *node = AI_NODE(aiNodes, id);

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
static void G_Ai_Node_Adjust(const GameAiNodeId id) {

  for (size_t i = 0; i < aiNodes->count; i++) {
    const GameAiNode *node = AI_NODE(aiNodes, i);

    if (!node->links) {
      continue;
    }

    for (size_t l = 0; l < node->links->count; l++) {
      GameAiLink *link = AI_LINK(node->links, l);

      if (link->id >= id) {
        link->id--;
      }
    }
  }
}

/**
 * @brief Destroys a navigation node, removing all of its links and freeing its slot.
 */
void G_Ai_Node_Destroy(const GameAiNodeId id) {

  if (!aiNodes || id >= aiNodes->count) {
    G_Warn("Invalid node id %u\n", id);
    return;
  }

  if (module.compiled) {
    G_Warn("This map's nodes were compiled by quemap -nav; recompile them instead.\n");
    return;
  }

  G_Ai_Node_UnlinkAll(id);
  $(aiNodes, removeAt, id);

  G_Ai_Node_InvalidateSpatialIndex();

  if (!aiNodes->count) {
    release(aiNodes);
    aiNodes = NULL;
  } else {
    G_Ai_Node_Adjust(id);
  }
}

/**
 * @brief Returns true if the client entity is currently standing on solid ground.
 */
static bool G_Ai_Node_OnGround(const GameClient *cl) {
  const CollisionTrace tr = gi.Trace(cl->entity->s.origin,
                                 Vec3_Add(cl->entity->s.origin, MakeVec3(0, 0, -PM_GROUND_DIST)),
                                 cl->entity->s.bounds,
                                 NULL,
                                 CONTENTS_MASK_CLIP_CORPSE);

  return tr.fraction < 1.0f && tr.plane.normal.z > PM_STEP_NORMAL;
}

/**
 * @brief Returns the world position of the specified navigation node.
 */
Vec3 G_Ai_Node_GetPosition(const GameAiNodeId node) {

  return AI_NODE(aiNodes, node)->position;
}

/**
 * @brief Recalculates the traversal costs for all links incident to the given node.
 */
static void G_Ai_Node_UpdateCosts(const GameAiNodeId id) {

  for (size_t i = 0; i < aiNodes->count; i++) {
    const GameAiNode *node = AI_NODE(aiNodes, i);

    if (!node->links || !node->links->count) {
      continue;
    }

    for (size_t l = 0; l < node->links->count; l++) {
      GameAiLink *link = AI_LINK(node->links, l);

      if (id == i || link->id == id) {
        link->cost = G_Ai_Node_Cost(i, link->id);
      }
    }
  }
}

/**
 * @return The `func_plat` over the given position that is not at its bottom, or `NULL`.
 */
const GameEntity *G_Ai_Node_RaisedPlatform(const Vec3 position) {

  if (!aiPlatforms) {
    return NULL;
  }

  for (size_t i = 0; i < aiPlatforms->count; i++) {
    const GameEntity *plat = AI_PLATFORM(aiPlatforms, i);

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
 * @brief Check if the node we want to move towards is currently pathable.
 */
bool G_Ai_Node_CanPathTo(const Vec3 position) {

  // if we're heading onto a mover node, only allow us to go forth
  // if the mover is there
  const Vec3 end = Vec3_Subtract(position, MakeVec3(0, 0, PM_GROUND_DIST * 3.f));

  // check if the destination has ground
  CollisionTrace tr = gi.Trace(position, end, Box3_Expand3(G_PlayerBounds(), MakeVec3(1.f, 1.f, 0.f)), NULL, CONTENTS_MASK_CLIP_CORPSE | CONTENTS_MASK_LIQUID);

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

  return G_Ai_Node_RaisedPlatform(position) == NULL;
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
  return G_Ai_Node_CanPathTo(AI_NODE(aiNodes, AI_NODE_ID(path, index))->position);
}

/**
 * @brief The length of space that nodes will drop while walking.
 */
#define WALKING_DISTANCE  128.f

/**
 * @brief The length of space that couldn't be logically travelled by regular means.
 */
#define TELEPORT_DISTANCE  64.f

static void G_Ai_Node_RecordJump(GameClient *cl, const PMoveCmd *cmd);

/**
 * @brief Handles automatic node placement as the player moves through the map during development.
 */
void G_Ai_Node_PlayerRoam(GameClient *cl, const PMoveCmd *cmd) {

  if (!g_aiNodeDev->integer) {
    return;
  }

  if (module.compiled) {
    if (g_aiNodeDev->integer == 1) {
      G_Ai_Node_RecordJump(cl, cmd);
    }
    return;
  }

  GameEntity *ent = cl->entity;

  module.oldButtons = module.buttons;
  module.buttons = cmd->buttons;
  module.latchedButtons |= module.buttons & ~module.oldButtons;
  
  const bool allowAdjustments = g_aiNodeDev->integer == 1;
  const bool doNoding = allowAdjustments && module.dropNodes && cl->ps.pmState.type == PM_NORMAL;

  // we just switched between noclip/not noclip, clear some stuff
  // so we don't accidentally drop nodes
  if (module.doNoding != doNoding) {
    module.doNoding = doNoding;

    module.position = ent->s.origin;
    module.lastNodes[0] = module.lastNodes[1] = AI_NODE_INVALID;
    module.awaitLanding = true;
  }

  const bool inWater = gi.PointContents(ent->s.origin) & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA);
  const float lastNodeDistanceCompare = module.lastNodes[0] == AI_NODE_INVALID ? FLT_MAX : Vec3_Distance(ent->s.origin, G_Ai_Node_GetPosition(module.lastNodes[0]));
  const float playerDistanceCompare = Vec3_Distance(ent->s.origin, module.position);

  if (G_Ai_Node_OnGround(cl)) {
    module.floorPosition = ent->s.origin;
  }

  if (doNoding) {
    // we're waiting to land to drop a node; we jumped, fell, got sent by a jump pad, something like that.
    if (module.awaitLanding) {

      if (G_Ai_Node_OnGround(cl) || inWater) {

        // we landed!
        module.awaitLanding = false;
        module.position = ent->s.origin;

        GameAiNodeId landedNearNode = G_Ai_Node_FindClosest(ent->s.origin, WALKING_DISTANCE / 2, true, true);

        if (landedNearNode == AI_NODE_INVALID) {

          landedNearNode = G_Ai_Node_Create(ent->s.origin);
        }

        // one-way node from where we were to here
        if (module.lastNodes[0] != AI_NODE_INVALID) {
          bool bidirectional = false;

          if (module.isWaterJump) {
            bidirectional = true;
            G_Ai_Debug("Most likely waterjump; connecting both ends\n");
          } else if (module.isJumping || inWater) {
            const float zDiff = fabsf(G_Ai_Node_GetPosition(module.lastNodes[0]).z - G_Ai_Node_GetPosition(landedNearNode).z);

            if (zDiff < PM_STEP_HEIGHT || (inWater && zDiff < PM_STEP_HEIGHT * 3.f)) {
              bidirectional = true;
              G_Ai_Debug("Most likely jump or drop-into-water link; connecting both ends\n");
            }
          }

          G_Ai_Node_LinkDefault(module.lastNodes[0], landedNearNode, bidirectional);
        }

        module.lastNodes[1] = module.lastNodes[0];
        module.lastNodes[0] = landedNearNode;
        module.isWaterJump = false;
      }

      return;
    }

    // we probably teleported; no node, just start dropping here when we land
    if (playerDistanceCompare > TELEPORT_DISTANCE) {

      module.lastNodes[0] = module.lastNodes[1] = AI_NODE_INVALID;
      module.position = ent->s.origin;
      module.awaitLanding = true;

      G_Ai_Debug("Teleport detected; awaiting landing...\n");
      return;
    }

    // we just left the floor (or water); drop a node here
    if (!G_Ai_Node_OnGround(cl) && !inWater) {
      // for water leavings, we want to drop where we are, not where we went into the water from
      const Vec3 where = module.isWaterJump ? ent->s.origin : module.floorPosition;
      const GameAiNodeId jumpedNearNode = G_Ai_Node_FindClosest(where, WALKING_DISTANCE / 2, true, false);
      const bool isJump = cl->ps.pmState.velocity.z > 0;

      if (jumpedNearNode == AI_NODE_INVALID) {
        const GameAiNodeId id = G_Ai_Node_Create(where);

        if (module.lastNodes[0] != AI_NODE_INVALID) {

          G_Ai_Node_LinkDefault(module.lastNodes[0], id, true);
        }
      
        module.lastNodes[1] = module.lastNodes[0];
        module.lastNodes[0] = id;
      } else {
        
        if (module.lastNodes[0] != AI_NODE_INVALID) {

          G_Ai_Node_LinkDefault(module.lastNodes[0], jumpedNearNode, true);
        }
      
        module.lastNodes[1] = module.lastNodes[0];
        module.lastNodes[0] = jumpedNearNode;
      }

      module.awaitLanding = true;
      module.isJumping = isJump;

      // we didn't leave from water jump, so turn this off.
      if (!isJump) {
        module.isWaterJump = false;
      }

      G_Ai_Debug("Left ground; jumping? %s\n", isJump ? "yes" : "nop");
      return;
    }
  }

  // we're walkin'

  const GameAiNodeId closestNode = G_Ai_Node_FindClosest(ent->s.origin, WALKING_DISTANCE / 4, true, false);
  const bool onMover = ent->ground.ent && ((GameEntity *) ent->ground.ent)->s.number != 0;

  // attack button enables/disables placement
  if (allowAdjustments && (module.latchedButtons & BUTTON_ATTACK)) {
    module.dropNodes = !module.dropNodes;

    module.latchedButtons &= ~BUTTON_ATTACK;
  // "use" moves node
  } else if (allowAdjustments && ent->moveNode) {
    if (module.lastNodes[0] != AI_NODE_INVALID) {
      GameAiNode *node = AI_NODE(aiNodes, module.lastNodes[0]);
      node->position = ent->s.origin;

      if (cmd->up < 0) {
        const CollisionTrace tr = gi.Trace(node->position, Vec3_Subtract(node->position, MakeVec3(0.f, 0.f, MAX_WORLD_COORD)), Pm_Bounds(&ent->client->ps.pmState.params, false), ent, CONTENTS_MASK_SOLID);
        node->position = tr.end;
      }

      // recalculate links
      G_Ai_Node_UpdateCosts(module.lastNodes[0]);
    }

    ent->moveNode = false;
  // hook destroys node
  } else if (allowAdjustments && (module.latchedButtons & BUTTON_HOOK)) {
    if (module.lastNodes[0] != AI_NODE_INVALID) {
      G_Ai_Node_Destroy(module.lastNodes[0]);
      module.position = ent->s.origin;
      module.lastNodes[0] = module.lastNodes[1] = AI_NODE_INVALID;
      module.lastNodes[0] = G_Ai_Node_FindClosest(ent->s.origin, WALKING_DISTANCE * 2.5f, true, false);
    }
    module.latchedButtons &= ~BUTTON_HOOK;
  // score adjusts link connections
  } else if (allowAdjustments && (module.latchedButtons & BUTTON_SCORE)) {

    if (module.lastNodes[1] != AI_NODE_INVALID && module.lastNodes[0] != AI_NODE_INVALID) {
      uint8_t bits = 0;

      if (G_Ai_Node_IsLinked(module.lastNodes[0], module.lastNodes[1])) {
        bits |= 1;
      }
      if (G_Ai_Node_IsLinked(module.lastNodes[1], module.lastNodes[0])) {
        bits |= 2;
      }

      bits = (bits + 1) % 4;

      G_Ai_Node_Unlink(module.lastNodes[0], module.lastNodes[1]);
      G_Ai_Node_Unlink(module.lastNodes[1], module.lastNodes[0]);
      
      if (bits & 1) {
        G_Ai_Node_LinkDefault(module.lastNodes[0], module.lastNodes[1], false);
      }
      if (bits & 2) {
        G_Ai_Node_LinkDefault(module.lastNodes[1], module.lastNodes[0], false);
      }
    }
    
    module.latchedButtons &= ~BUTTON_SCORE;
  // we're stepping on/off a mover; connect us one-way
  } else if (onMover != module.onMover) {

    module.onMover = onMover;

    if (doNoding) {
      GameAiNodeId id = G_Ai_Node_FindClosest(ent->s.origin, WALKING_DISTANCE / 8, true, false);
      
      if (id == AI_NODE_INVALID) {
        id = G_Ai_Node_Create(ent->s.origin);
      }

      if (module.lastNodes[0] != AI_NODE_INVALID) {

        G_Ai_Node_LinkDefault(module.lastNodes[0], id, false);
      }
    
      module.lastNodes[1] = module.lastNodes[0];
      module.lastNodes[0] = id;
    }

  // if we touched another node and had another node lit up; connect us if we aren't already
  } else if (closestNode != AI_NODE_INVALID && closestNode != module.lastNodes[0] && G_Ai_Node_Visible(ent->s.origin, closestNode)) {

    if (doNoding && module.lastNodes[0] != AI_NODE_INVALID) {

      G_Ai_Node_LinkDefault(module.lastNodes[0], closestNode, !module.onMover);
    }
    
    module.lastNodes[1] = module.lastNodes[0];
    module.lastNodes[0] = closestNode;
  // we got far enough from the last node, so drop a new node
  } else if (lastNodeDistanceCompare > WALKING_DISTANCE) {

    if (doNoding) {
      GameAiNodeId id = G_Ai_Node_FindClosest(ent->s.origin, WALKING_DISTANCE / 2, true, !inWater);
      
      if (id == AI_NODE_INVALID) {
        id = G_Ai_Node_Create(ent->s.origin);
      }

      if (module.lastNodes[0] != AI_NODE_INVALID) {

        G_Ai_Node_LinkDefault(module.lastNodes[0], id, !module.onMover);
      }
    
      module.lastNodes[1] = module.lastNodes[0];
      module.lastNodes[0] = id;
    }
  }

  // we're currently in water; the only way we can get out
  // is by waterjumping, having something take us out of the water,
  // a ladder or walking straight out of water. mark waterjump as true,
  // we'll figure it out when we actually leave water what the intentions are.
  if (inWater) {
    module.isWaterJump = true;
  }

  module.position = ent->s.origin;
}

/**
 * @brief A compact representation of a directed link pair used for render deduplication.
 */
typedef struct {
  union {
    struct {
      GameAiNodeId a, b;
    };
    int32_t v;
  };
} GameAiUniqueLink;

typedef struct {
  GameAiUniqueLink link;
  int32_t bits;
} GameAiRenderLink;

/**
 * @brief Renders a single node link line for developer visualization.
 */
static void G_Ai_Node_RenderLink(const GameAiUniqueLink ulink, const int32_t bits) {
  
  const GameAiNode *nodeA = AI_NODE(aiNodes, ulink.a);
  const GameAiNode *nodeB = AI_NODE(aiNodes, ulink.b);

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
static bool G_Ai_NodeInPath(Vector *path, GameAiNodeId node) {

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
 * @brief The distance within which a compiled graph's nodes are rendered, as it has thousands.
 */
#define AI_NODE_RENDER_DISTANCE 768.f

/**
 * @brief Renders all navigation nodes and their links for developer visualization.
 */
void G_Ai_Node_Render(void) {

  if (!g_aiNodeDev->integer) {
    return;
  }

  if (!aiNodes) {
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

  Vector *uniqueLinks = $(alloc(Vector), initWithSize, sizeof(GameAiRenderLink));

  for (uint32_t i = 0; i < aiNodes->count; i++) {
    const GameAiNode *node = AI_NODE(aiNodes, i);

    if (module.compiled && Vec3_Distance(node->position, ent->s.origin) > AI_NODE_RENDER_DISTANCE) {
      continue;
    }

    const bool inPath = G_Ai_NodeInPath(module.testPath, i);

    if (G_Ai_Node_Visible(Vec3_Add(ent->s.origin, client->ps.pmState.viewOffset), i)) {

      gi.WriteByte(SV_CMD_TEMP_ENTITY);
      gi.WriteByte(TE_AI_NODE);
      gi.WritePosition(node->position);
      gi.WriteShort(i);

      byte bits = 0;

      if (inPath) {
        bits = 3;
      } else if (module.lastNodes[0] == i) {
        bits = 1;
      } else if (module.lastNodes[1] == i) {
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
        const GameAiLink *link = AI_LINK(node->links, l);
        GameAiUniqueLink ulink;
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
          GameAiRenderLink *renderLink = VectorElement(uniqueLinks, GameAiRenderLink, u);
          if (renderLink->link.v == ulink.v) {
            renderLink->bits |= bit;
            found = true;
            break;
          }
        }

        if (!found) {
          GameAiRenderLink renderLink = {
            .link = ulink,
            .bits = bit
          };
          $(uniqueLinks, add, &renderLink);
        }
      }
    }
  }

  for (size_t i = 0; i < uniqueLinks->count; i++) {
    const GameAiRenderLink *renderLink = VectorElement(uniqueLinks, GameAiRenderLink, i);
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

/**
 * @brief The version 2 on-disk link record.
 */
typedef struct {
  GameAiNodeId id;
  float cost;
} GameAiLinkRecord;

_Static_assert(sizeof(GameAiLinkRecord) == 8, "GameAiLinkRecord is on disk; changing it requires a new AI_NODE_VERSION");

/**
 * @brief Clips a replayed jump to the world alone, so that players and movers do not block it.
 */
static CollisionTrace G_Ai_Node_ReplayTrace(const Vec3 start, const Vec3 end, const Box3 bounds) {
  return gi.Clip(start, end, bounds, ge.entities[0], CONTENTS_MASK_CLIP_CORPSE);
}

/**
 * @brief Replays a learned jump with the current movement.
 * @return True if every step lands within `AI_JUMP_REPLAY_TOLERANCE` of where it was recorded.
 */
static bool G_Ai_Node_ReplayJump(const NavJump *jump, const NavJumpStep *steps) {

  PMove pm;
  memset(&pm, 0, sizeof(pm));

  pm.s.type = PM_NORMAL;
  pm.s.origin = jump->origin;
  pm.s.velocity = jump->velocity;
  pm.s.flags = (uint16_t) jump->flags;
  pm.s.time = (uint16_t) jump->time;
  pm.s.params = G_MovementParams();

  pm.PointContents = gi.PointContents;
  pm.BoxContents = gi.BoxContents;
  pm.Trace = G_Ai_Node_ReplayTrace;

  pm.Debug = gi.Debug;
  pm.DebugMask = gi.DebugMask;
  pm.debugMask = DEBUG_PMOVE_SERVER;

  for (int32_t i = 0; i < jump->numSteps; i++) {
    const NavJumpStep *step = &steps[i];

    pm.cmd = (PMoveCmd) {
      .msec = step->msec,
      .angles = step->angles,
      .forward = step->forward,
      .right = step->right,
      .up = step->up,
      .buttons = step->buttons
    };

    Pm_Move(&pm);

    if (Vec3_Distance(pm.s.origin, step->origin) > AI_JUMP_REPLAY_TOLERANCE) {
      return false;
    }
  }

  return true;
}

/**
 * @brief Adds a learned jump to the graph, as a link that replays it.
 */
static void G_Ai_Node_AddJump(const NavJump *jump, const NavJumpStep *steps) {

  NavJump copy = *jump;
  copy.firstStep = (int32_t) module.jumpSteps->count;

  for (int32_t i = 0; i < jump->numSteps; i++) {
    $(module.jumpSteps, add, (ident) &steps[i]);
  }

  $(module.jumps, add, &copy);

  if (copy.movement != G_MovementParams().movement) {
    return;
  }

  const Vec3 from = G_Ai_Node_GetPosition(copy.from);
  const Vec3 to = G_Ai_Node_GetPosition(copy.to);
  const float cost = Maxf(Vec3_Distance(from, to), copy.travelTime * PM_SPEED_RUN / 100.f);

  G_Ai_Node_LinkTravel(copy.from, copy.to, cost, NAV_TRAVEL_JUMP);

  GameAiNode *node = AI_NODE(aiNodes, copy.from);
  for (size_t i = 0; i < node->links->count; i++) {
    GameAiLink *link = AI_LINK(node->links, i);
    if (link->id == (GameAiNodeId) copy.to) {
      link->travelType = NAV_TRAVEL_JUMP;
      link->jump = (int32_t) module.jumps->count - 1;
    }
  }
}

/**
 * @brief Adds a compiled file's learned jumps to the graph. A jump learned under the current
 * movement is replayed first, and dropped if it no longer lands. A jump learned under another
 * movement is kept for saving, but not linked.
 */
static void G_Ai_Node_LoadJumps(const NavJump *jumps, int32_t numJumps, const NavJumpStep *steps, int32_t numSteps) {

  module.jumps = $(alloc(Vector), initWithSize, sizeof(NavJump));
  module.jumpSteps = $(alloc(Vector), initWithSize, sizeof(NavJumpStep));

  const int32_t movement = G_MovementParams().movement;
  int32_t numLinked = 0, numOther = 0;

  for (int32_t i = 0; i < numJumps; i++) {
    const NavJump *jump = &jumps[i];

    if (jump->from < 0 || jump->from >= (int32_t) aiNodes->count ||
        jump->to < 0 || jump->to >= (int32_t) aiNodes->count || jump->travelTime <= 0 ||
        jump->numSteps <= 0 || jump->numSteps > AI_JUMP_MAX_STEPS || jump->firstStep < 0 ||
        jump->firstStep > numSteps || jump->numSteps > numSteps - jump->firstStep) {
      G_Warn("Nav file has an invalid jump %d\n", i);
      continue;
    }

    if (jump->movement != movement) {
      G_Ai_Node_AddJump(jump, &steps[jump->firstStep]);
      numOther++;
      continue;
    }

    if (!G_Ai_Node_ReplayJump(jump, &steps[jump->firstStep])) {
      G_Warn("Learned jump %d from node %d to %d no longer lands; dropping it\n", i, jump->from, jump->to);
      continue;
    }

    G_Ai_Node_AddJump(jump, &steps[jump->firstStep]);
    numLinked++;
  }

  if (numJumps) {
    gi.Print("  Loaded %d learned jumps, and %d for other movements.\n", numLinked, numOther);
  }
}

/**
 * @brief Learns the jump that the recorder holds, if the graph lacks it and it replays.
 */
static void G_Ai_Node_LearnJump(const GameClient *cl) {

  GameAiJumpRecorder *rec = &module.recorder;

  const GameAiJumpSample *first = &rec->samples[0];
  const GameAiJumpSample *landing = &rec->samples[rec->landing];

  const GameAiNodeId from = G_Ai_Node_FindClosest(first->origin, AI_JUMP_NODE_DISTANCE, false, true);
  const GameAiNodeId to = G_Ai_Node_FindClosest(landing->step.origin, AI_JUMP_NODE_DISTANCE, false, true);

  if (from == AI_NODE_INVALID || to == AI_NODE_INVALID || from == to) {
    G_Ai_Debug("Jump does not start and end at different nodes\n");
    return;
  }

  NavJumpStep steps[AI_JUMP_MAX_STEPS];
  uint32_t millis = 0;

  for (int32_t i = 0; i <= rec->landing; i++) {
    steps[i] = rec->samples[i].step;
    millis += steps[i].msec;
  }

  const NavJump jump = {
    .from = (int32_t) from,
    .to = (int32_t) to,
    .movement = cl->ps.pmState.params.movement,
    .travelTime = Maxi(1, (int32_t) (millis + 9) / 10),
    .origin = first->origin,
    .velocity = first->velocity,
    .flags = first->flags,
    .time = first->time,
    .firstStep = 0,
    .numSteps = rec->landing + 1
  };

  if (G_Ai_Node_GetLink(from, to)) {
    gi.Print("Already linked node %u to %u\n", from, to);
    return;
  }

  const float cost = Maxf(Vec3_Distance(G_Ai_Node_GetPosition(from), G_Ai_Node_GetPosition(to)),
                          jump.travelTime * PM_SPEED_RUN / 100.f);

  Vector *path = G_Ai_Node_FindPath(NULL, from, to, G_Ai_Node_Heuristic, cost * AI_JUMP_SHORTCUT, NULL);
  if (path) {
    release(path);
    gi.Print("A path from node %u to %u exists; not learning this jump\n", from, to);
    return;
  }

  if (!G_Ai_Node_ReplayJump(&jump, steps)) {
    gi.Print("The jump from node %u to %u does not replay; not learning it\n", from, to);
    return;
  }

  G_Ai_Node_AddJump(&jump, steps);

  gi.Print("Learned a jump from node %u to %u: %d steps, %u ms\n", from, to, jump.numSteps, millis);
}

/**
 * @brief Records the developer's movement, and learns the jumps that the compiled graph lacks.
 * @details A jump starts with a run-up of `AI_JUMP_RUN_UP_MILLIS` on the ground and a takeoff with
 * the jump button held. It ends after `AI_JUMP_LAND_MILLIS` on the ground, so a jump on landing,
 * as in a trick jump, belongs to the same jump. Pushes, teleports, water and ladders abort it.
 */
static void G_Ai_Node_RecordJump(GameClient *cl, const PMoveCmd *cmd) {

  GameAiJumpRecorder *rec = &module.recorder;
  const GameEntity *ent = cl->entity;
  const bool grounded = ent->ground.ent != NULL;

  const GameAiJumpSample sample = {
    .step = {
      .angles = Vec3_Add(cmd->angles, cl->ps.pmState.deltaAngles),
      .origin = ent->s.origin,
      .forward = cmd->forward,
      .right = cmd->right,
      .up = cmd->up,
      .msec = cmd->msec,
      .buttons = cmd->buttons
    },
    .origin = rec->origin,
    .velocity = rec->velocity,
    .flags = rec->flags,
    .time = rec->time,
    .grounded = rec->grounded
  };

  const bool primed = rec->primed;

  rec->primed = true;
  rec->origin = ent->s.origin;
  rec->velocity = ent->velocity;
  rec->flags = cl->ps.pmState.flags;
  rec->time = cl->ps.pmState.time;
  rec->grounded = grounded;

  const bool abort = !primed || ent->dead || cl->ps.pmState.type != PM_NORMAL ||
    (cl->ps.pmState.flags & (PMF_TIME_PUSHED | PMF_TIME_TELEPORT | PMF_ON_LADDER)) ||
    ent->waterLevel > WATER_FEET || Vec3_Distance(sample.origin, ent->s.origin) > 64.f ||
    rec->count == AI_JUMP_MAX_STEPS || rec->millis > AI_JUMP_MAX_MILLIS;

  if (abort) {
    rec->count = 0;
    rec->state = AI_JUMP_IDLE;
    return;
  }

  switch (rec->state) {
    case AI_JUMP_IDLE:
      if (sample.grounded && grounded) {
        rec->samples[rec->count++] = sample;

        uint32_t millis = 0;
        for (int32_t i = 0; i < rec->count; i++) {
          millis += rec->samples[i].step.msec;
        }

        int32_t trim = 0;
        while (millis > AI_JUMP_RUN_UP_MILLIS && trim < rec->count - 1) {
          millis -= rec->samples[trim++].step.msec;
        }

        memmove(rec->samples, rec->samples + trim, sizeof(GameAiJumpSample) * (rec->count - trim));
        rec->count -= trim;
      } else if (sample.grounded && !grounded && cmd->up > 0) {
        rec->samples[rec->count++] = sample;
        rec->state = AI_JUMP_AIRBORNE;
        rec->millis = 0;
        for (int32_t i = 0; i < rec->count; i++) {
          rec->millis += rec->samples[i].step.msec;
        }
      } else {
        rec->count = 0;
      }
      break;

    case AI_JUMP_AIRBORNE:
      rec->samples[rec->count++] = sample;
      rec->millis += cmd->msec;

      if (grounded) {
        rec->state = AI_JUMP_LANDED;
        rec->landing = rec->count - 1;
        rec->landedMillis = 0;
      }
      break;

    case AI_JUMP_LANDED:
      rec->samples[rec->count++] = sample;
      rec->millis += cmd->msec;

      if (!grounded) {
        rec->state = AI_JUMP_AIRBORNE;
      } else {
        rec->landedMillis += cmd->msec;

        if (rec->landedMillis >= AI_JUMP_LAND_MILLIS) {
          G_Ai_Node_LearnJump(cl);
          rec->count = 0;
          rec->state = AI_JUMP_IDLE;
        }
      }
      break;
  }
}

/**
 * @brief The most links a compiled node can have, for validating a `.nav` file's link count.
 */
#define AI_NODE_MAX_LINKS 64

/**
 * @brief Reads the nodes and links of a `.nav` file compiled by `quemap -nav`, after its magic and
 * version, into `aiNodes`.
 * @return False if the file is malformed or was compiled from a different BSP, in which case the
 * nodes read so far must be discarded.
 */
static bool G_Ai_ReadCompiledNodes(File *file) {

  NavHeader header;

  if (gi.ReadFile(file, &header.voxelSize, sizeof(header) - offsetof(NavHeader, voxelSize), 1) != 1) {
    G_Warn("Nav file is truncated\n");
    return false;
  }

  const CollisionBsp *bsp = gi.Bsp();

  if (header.voxelSize.x != bsp->voxelSize.x ||
      header.voxelSize.y != bsp->voxelSize.y ||
      header.voxelSize.z != bsp->voxelSize.z ||
      !Vec3_Equal(header.voxelMins, bsp->voxelBounds.mins)) {
    G_Warn("Nav file was compiled from a different BSP; recompile it with quemap -nav\n");
    return false;
  }

  if (header.numNodes < 0 || header.numNodes > bsp->numVoxels || header.numLinks < 0 ||
      header.numLinks / AI_NODE_MAX_LINKS > header.numNodes || header.numJumps < 0 ||
      header.numJumpSteps < 0 || header.numJumpSteps / AI_JUMP_MAX_STEPS > header.numJumps) {
    G_Warn("Nav file has an invalid node, link or jump count\n");
    return false;
  }

  header.magic = NAV_MAGIC;
  header.version = NAV_VERSION;
  module.navFile = header;

  NavNode *nodes = module.navNodes = malloc(sizeof(NavNode) * Maxi(1, header.numNodes));
  NavLink *links = module.navLinks = malloc(sizeof(NavLink) * Maxi(1, header.numLinks));
  NavJump *jumps = malloc(sizeof(NavJump) * Maxi(1, header.numJumps));
  NavJumpStep *steps = malloc(sizeof(NavJumpStep) * Maxi(1, header.numJumpSteps));

  bool valid = nodes && links && jumps && steps;
  if (!valid) {
    G_Warn("Failed to allocate the nav file\n");
  }

  valid = valid && gi.ReadFile(file, nodes, sizeof(NavNode), header.numNodes) == header.numNodes &&
                   gi.ReadFile(file, links, sizeof(NavLink), header.numLinks) == header.numLinks &&
                   gi.ReadFile(file, jumps, sizeof(NavJump), header.numJumps) == header.numJumps &&
                   gi.ReadFile(file, steps, sizeof(NavJumpStep), header.numJumpSteps) == header.numJumpSteps;

  if (!valid) {
    G_Warn("Nav file is truncated\n");
  }

  aiNodes = $(alloc(Vector), initWithSize, sizeof(GameAiNode));

  module.voxelNodes = malloc(sizeof(int32_t) * bsp->numVoxels);
  if (!module.voxelNodes) {
    G_Warn("Failed to allocate the voxel index for %d voxels\n", bsp->numVoxels);
    valid = false;
  }

  for (int32_t i = 0; i < bsp->numVoxels && valid; i++) {
    module.voxelNodes[i] = -1;
  }

  for (int32_t i = 0; i < header.numNodes && valid; i++) {
    const NavNode *in = &nodes[i];

    if (in->voxel < 0 || in->voxel >= bsp->numVoxels || in->firstLink < 0 || in->numLinks < 0 ||
        in->firstLink > header.numLinks || in->numLinks > header.numLinks - in->firstLink) {
      G_Warn("Nav file has an invalid node %d\n", i);
      valid = false;
      break;
    }

    GameAiNode node = {
      .position = in->position,
      .voxel = in->voxel,
      .contents = in->contents
    };

    if (in->numLinks) {
      node.links = $(alloc(Vector), initWithSize, sizeof(GameAiLink));

      for (int32_t j = 0; j < in->numLinks; j++) {
        const NavLink *link = &links[in->firstLink + j];

        if (link->node < 0 || link->node >= header.numNodes || link->travelTime <= 0) {
          G_Warn("Nav file has an invalid link from node %d\n", i);
          valid = false;
          break;
        }

        const float distance = Vec3_Distance(in->position, nodes[link->node].position);

        GameAiLink out = {
          .id = link->node,
          .cost = Maxf(distance, link->travelTime * PM_SPEED_RUN / 100.f),
          .travelType = link->travelType,
          .jump = -1
        };

        $(node.links, add, &out);
      }
    }

    $(aiNodes, add, &node);
    module.voxelNodes[in->voxel] = i;
  }

  G_Ai_Node_InvalidateSpatialIndex();

  if (valid) {
    module.compiled = true;
    G_Ai_Node_LoadJumps(jumps, header.numJumps, steps, header.numJumpSteps);
    gi.Print("  Loaded %d compiled nodes with %d total links.\n", header.numNodes, header.numLinks);
  }

  free(jumps);
  free(steps);

  return valid;
}

/**
 * @brief Reads the nodes and links from an open .nav file into `aiNodes`.
 * @return False if the file is malformed, in which case the nodes read so far
 * must be discarded.
 */
static bool G_Ai_ReadNodes(File *file) {
  int32_t magic, version;

  if (gi.ReadFile(file, &magic, sizeof(magic), 1) != 1 || magic != AI_NODE_MAGIC) {
    G_Warn("Nav file invalid format!\n");
    return false;
  }

  if (gi.ReadFile(file, &version, sizeof(version), 1) != 1) {
    G_Warn("Nav file out of date!\n");
    return false;
  }

  if (version == NAV_VERSION) {
    return G_Ai_ReadCompiledNodes(file);
  }

  if (version != AI_NODE_VERSION) {
    G_Warn("Nav file out of date!\n");
    return false;
  }

  uint32_t numNodes;
  if (gi.ReadFile(file, &numNodes, sizeof(numNodes), 1) != 1 || numNodes >= AI_NODE_INVALID) {
    G_Warn("Nav file has an invalid node count\n");
    return false;
  }

  aiNodes = $(alloc(Vector), initWithSize, sizeof(GameAiNode));

  for (size_t i = 0; i < numNodes; i++) {
    GameAiNode node = { 0 };
    $(aiNodes, add, &node);
  }

  G_Ai_Node_InvalidateSpatialIndex();

  size_t totalLinks = 0;

  for (size_t i = 0; i < aiNodes->count; i++) {
    GameAiNode *node = AI_NODE(aiNodes, i);

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
      node->links = $(alloc(Vector), initWithSize, sizeof(GameAiLink));

      for (size_t l = 0; l < numLinks; l++) {
        GameAiLinkRecord record;

        if (gi.ReadFile(file, &record, sizeof(record), 1) != 1) {
          G_Warn("Nav file is truncated at node %zu\n", i);
          return false;
        }

        GameAiLink link = {
          .id = record.id,
          .cost = record.cost,
          .jump = -1
        };

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

  module.position = MakeVec3(MAX_WORLD_DIST, MAX_WORLD_DIST, MAX_WORLD_DIST);
  module.lastNodes[0] = module.lastNodes[1] = AI_NODE_INVALID;
  module.awaitLanding = true;

  char filename[MAX_OS_PATH];

  Str_Format(filename, sizeof(filename), "maps/%s.nav", gameLevel.name);

  if (!gi.FileExists(filename)) {
    G_Warn("No navigation file exists for this map; bots will be dumb!\nUse `g_aiNodeDev` to set up nodes.\n");
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

  module.fileNodes = (uint32_t) aiNodes->count;
  module.fileLinks = 0;

  for (size_t i = 0; i < aiNodes->count; i++) {
    const GameAiNode *node = AI_NODE(aiNodes, i);

    if (node->links) {
      module.fileLinks += (uint32_t) node->links->count;
    }
  }
}

/**
 * @brief Validates node integrity: warns about item entities with no nearby node and nodes inside solid.
 */
static void G_Ai_CheckNodes(void) {

  if (g_aiNodeDev->integer) {
    G_ForEachEntity(ent, {

      // only warn for item nodes
      if (!ent->item) {
        continue;
      }

      GameAiNodeId node = G_Ai_Node_FindClosest(ent->s.origin, WALKING_DISTANCE * 2.5f, true, false);

      if (node == AI_NODE_INVALID) {
        G_Warn("Entity %s @ %s appears to be unreachable by nodes\n", ent->classname, vtos(ent->s.origin));
      }
    });
  }

  if (module.compiled) {
    return;
  }

  for (size_t i = 0; i < aiNodes->count; i++) {
    const GameAiNode *node = AI_NODE(aiNodes, i);
    
    if (gi.PointContents(node->position) & CONTENTS_MASK_SOLID) {
      G_Warn("Node %zu @ %s is inside of solid\n", i, vtos(node->position));
    }
  }
}

/**
 * @brief The time step of a simulated jump pad flight, in seconds.
 */
#define AI_RIDE_STEP .025f

/**
 * @brief The longest simulated jump pad flight, in seconds.
 */
#define AI_RIDE_TIME 4.f

/**
 * @brief The distance within which a ride starts or ends at a node.
 */
#define AI_RIDE_NODE_DISTANCE 64.f

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
 * @brief Links the node on a `trigger_push` to the nodes that its push lands on.
 * @details An angled push is a ballistic flight, swept in short box traces. A vertical push links
 * to the ledges beside its column, which a bot steers onto in flight.
 */
static void G_Ai_Node_LinkPush(const GameEntity *push) {

  if (push->solid == SOLID_NOT) {
    return;
  }

  const Vec3 center = Box3_Center(push->absBounds);
  const Vec3 pad = MakeVec3(center.x, center.y, push->absBounds.mins.z - PM_BOUNDS.mins.z);

  const GameAiNodeId start = G_Ai_Node_FindClosest(pad, AI_RIDE_NODE_DISTANCE, false, false);
  if (start == AI_NODE_INVALID) {
    G_Ai_Debug("No node at %s @ %s\n", push->classname, vtos(pad));
    return;
  }

  const float gravity = G_LevelGravity();
  const Vec3 velocity = Vec3_Scale(push->moveDir, push->speed * 10.f);
  const Vec3 origin = G_Ai_Node_GetPosition(start);

  if (push->moveDir.z >= AI_RIDE_VERTICAL) {
    const float apex = velocity.z * velocity.z / (2.f * gravity);

    for (size_t i = 0; i < aiNodes->count; i++) {
      const GameAiNode *node = AI_NODE(aiNodes, i);
      const float rise = node->position.z - origin.z;

      if (node->voxel == -1 || rise <= PM_STEP_HEIGHT || rise > apex) {
        continue;
      }

      if (Vec2_Distance(Vec3_XY(node->position), Vec3_XY(origin)) > AI_RIDE_VERTICAL_REACH) {
        continue;
      }

      const Vec3 column = MakeVec3(origin.x, origin.y, node->position.z);

      if (gi.Trace(origin, column, PM_BOUNDS, NULL, CONTENTS_MASK_CLIP_PLAYER).fraction < 1.f) {
        continue;
      }

      if (gi.Trace(column, node->position, PM_BOUNDS, NULL, CONTENTS_MASK_CLIP_PLAYER).fraction < 1.f) {
        continue;
      }

      const float time = (velocity.z - sqrtf(velocity.z * velocity.z - 2.f * gravity * rise)) / gravity;
      G_Ai_Node_LinkTravel(start, (GameAiNodeId) i, G_Ai_Node_RideCost(time), NAV_TRAVEL_JUMP_PAD);
    }
    return;
  }

  Vec3 position = origin;

  for (float time = AI_RIDE_STEP; time <= AI_RIDE_TIME; time += AI_RIDE_STEP) {

    const Vec3 next = MakeVec3(origin.x + velocity.x * time,
                               origin.y + velocity.y * time,
                               origin.z + velocity.z * time - .5f * gravity * time * time);

    const CollisionTrace trace = gi.Trace(position, next, PM_BOUNDS, NULL, CONTENTS_MASK_CLIP_PLAYER);
    if (trace.startSolid) {
      return;
    }

    if (trace.fraction < 1.f) {
      if (trace.plane.normal.z < PM_STEP_NORMAL) {
        G_Ai_Debug("%s @ %s pushes into a wall at %s\n", push->classname, vtos(pad), vtos(trace.end));
        return;
      }

      const GameAiNodeId end = G_Ai_Node_FindClosest(trace.end, AI_RIDE_NODE_DISTANCE, false, false);
      if (end != AI_NODE_INVALID && end != start) {
        G_Ai_Node_LinkTravel(start, end, G_Ai_Node_RideCost(time), NAV_TRAVEL_JUMP_PAD);
      }
      return;
    }

    position = next;
  }
}

/**
 * @brief Links the compiled nodes at the given height around a plat to the plat's own node there,
 * in both directions.
 */
static void G_Ai_Node_LinkPlatLevel(const GameEntity *plat, const GameAiNodeId platNode) {

  const Vec3 position = G_Ai_Node_GetPosition(platNode);
  const float reach = Maxf(Box3_Size(plat->absBounds).x, Box3_Size(plat->absBounds).y) * .5f + AI_RIDE_NODE_DISTANCE;

  for (size_t i = 0; i < aiNodes->count; i++) {
    const GameAiNode *node = AI_NODE(aiNodes, i);

    if (node->voxel == -1 || fabsf(node->position.z - position.z) > PM_STEP_HEIGHT) {
      continue;
    }

    const float distance = Vec2_Distance(Vec3_XY(node->position), Vec3_XY(position));
    if (distance > reach) {
      continue;
    }

    if (gi.Trace(node->position, position, PM_BOUNDS, NULL, CONTENTS_MASK_SOLID).fraction < 1.f) {
      continue;
    }

    G_Ai_Node_LinkTravel((GameAiNodeId) i, platNode, distance, NAV_TRAVEL_WALK);
    G_Ai_Node_LinkTravel(platNode, (GameAiNodeId) i, distance, NAV_TRAVEL_WALK);
  }
}

/**
 * @brief Gives a `func_plat` a node at the bottom and at the top of its travel, links the bottom
 * to the top as an elevator, and links each to the compiled nodes beside it.
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
  G_Ai_Node_LinkTravel(lower, upper, G_Ai_Node_RideCost(rise / speed), NAV_TRAVEL_ELEVATOR);

  G_Ai_Node_LinkPlatLevel(plat, lower);
  G_Ai_Node_LinkPlatLevel(plat, upper);
}

/**
 * @brief Links a compiled graph's rides: jump pads and plats. Teleporters link themselves.
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

  int32_t counts[NAV_TRAVEL_ELEVATOR + 1] = { 0 };

  for (size_t i = 0; i < aiNodes->count; i++) {
    const Vector *links = AI_NODE(aiNodes, i)->links;

    for (size_t j = 0; links && j < links->count; j++) {
      const int32_t type = AI_LINK(links, j)->travelType;
      if (type >= NAV_TRAVEL_TELEPORT && type <= NAV_TRAVEL_ELEVATOR) {
        counts[type]++;
      }
    }
  }

  gi.Print("  Linked %d teleporters, %d jump pads and %d elevators.\n",
           counts[NAV_TRAVEL_TELEPORT], counts[NAV_TRAVEL_JUMP_PAD], counts[NAV_TRAVEL_ELEVATOR]);
}

/**
 * @brief Called after the first second of gameplay to allow the AI to pick up nodes
 * from late-spawning entities.
 */
void G_Ai_NodesReady(void) {

  if (!aiNodes) {
    return;
  }

  const size_t addedNodes = aiNodes->count - module.fileNodes;
  size_t addedLinks = 0;

  for (size_t i = 0; i < aiNodes->count; i++) {
    const GameAiNode *node = AI_NODE(aiNodes, i);

    if (node->links) {
      addedLinks += node->links->count;
    }
  }

  addedLinks -= module.fileLinks;
  gi.Print("  Game loaded %zu additional nodes with %zu new links.\n", addedNodes, addedLinks);

  if (module.compiled) {
    G_Ai_Node_LinkRides();
  }

  G_ForEachEntity(ent, {
    if (ent->classname && Str_Compare(ent->classname, "func_plat") == 0) {
      if (!aiPlatforms) {
        aiPlatforms = $(alloc(Vector), initWithSize, sizeof(GameEntity *));
      }
      $(aiPlatforms, add, &ent);
    }
  });

  /*if (g_aiNodeDev->integer != 1) {
    const uint32_t optimized = Ai_OptimizeNodes();
    gi.Print("  %u nodes optimized\n", optimized);
  }*/

  G_Ai_CheckNodes();
}

/**
 * @brief Writes the compiled graph that was loaded, with the jumps learned since, to `filename`.
 */
static void G_Ai_SaveCompiledNodes(const char *filename) {

  File *file = gi.OpenFileWrite(filename);
  if (!file) {
    G_Warn("Failed to open %s\n", filename);
    return;
  }

  NavHeader header = module.navFile;
  header.numJumps = (int32_t) module.jumps->count;
  header.numJumpSteps = (int32_t) module.jumpSteps->count;

  gi.WriteFile(file, &header, sizeof(header), 1);
  gi.WriteFile(file, module.navNodes, sizeof(NavNode), header.numNodes);
  gi.WriteFile(file, module.navLinks, sizeof(NavLink), header.numLinks);
  gi.WriteFile(file, module.jumps->elements, sizeof(NavJump), header.numJumps);
  gi.WriteFile(file, module.jumpSteps->elements, sizeof(NavJumpStep), header.numJumpSteps);

  gi.CloseFile(file);

  gi.Print("Wrote %d learned jumps to %s.\n", header.numJumps, gi.RealPath(filename));
}

/**
 * @brief Serializes all navigation nodes and their links to the current map's .nav file.
 */
void G_Ai_SaveNodes(void) {

  if (g_aiNodeDev->integer != 1) {
    G_Warn("This command only works with `g_aiNodeDev` set to 1.\n");
    return;
  }

  char filename[MAX_OS_PATH];

  Str_Format(filename, sizeof(filename), "maps/%s.nav", gameLevel.name);

  if (module.compiled) {
    G_Ai_SaveCompiledNodes(filename);
    return;
  }

  if (!aiNodes) {
    G_Warn("No nodes to write.\n");
    return;
  }

  File *file = gi.OpenFileWrite(filename);
  int32_t magic = AI_NODE_MAGIC;
  int32_t version = AI_NODE_VERSION;
  
  gi.WriteFile(file, &magic, sizeof(magic), 1);
  gi.WriteFile(file, &version, sizeof(version), 1);

  const uint32_t numNodes = (uint32_t) aiNodes->count;
  gi.WriteFile(file, &numNodes, sizeof(numNodes), 1);

  for (size_t i = 0; i < aiNodes->count; i++) {
    const GameAiNode *node = AI_NODE(aiNodes, i);

    gi.WriteFile(file, &node->position, sizeof(node->position), 1);

    if (node->links) {
      const uint32_t numLinks = (uint32_t) node->links->count;
      gi.WriteFile(file, &numLinks, sizeof(numLinks), 1);
      for (size_t l = 0; l < node->links->count; l++) {
        const GameAiLink *link = AI_LINK(node->links, l);
        const GameAiLinkRecord record = {
          .id = link->id,
          .cost = link->cost
        };
        gi.WriteFile(file, &record, sizeof(record), 1);
      }
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
 * @brief Frees the state of a compiled graph: the voxel index, the file data, and learned jumps.
 */
static void G_Ai_Node_FreeCompiled(void) {

  free(module.voxelNodes);
  module.voxelNodes = NULL;

  free(module.navNodes);
  module.navNodes = NULL;

  free(module.navLinks);
  module.navLinks = NULL;

  module.jumps = release(module.jumps);
  module.jumpSteps = release(module.jumpSteps);

  memset(&module.navFile, 0, sizeof(module.navFile));
  memset(&module.recorder, 0, sizeof(module.recorder));

  module.compiled = false;
}

/**
 * @brief Clears all navigation nodes, freeing their link arrays but retaining the backing array.
 */
void G_Ai_DeleteNodes(void) {

  if (aiNodes) {
    for (uint32_t i = 0; i < aiNodes->count; i++) {
      GameAiNode *node = AI_NODE(aiNodes, i);

      if (node->links) {
        release(node->links);
      }
    }

    $(aiNodes, removeAll);
  }

  aiPlatforms = release(aiPlatforms);

  G_Ai_Node_FreeCompiled();

  G_Ai_Node_InvalidateSpatialIndex();
  G_Ai_Node_FreePathPool();
}

/**
 * @brief Frees all navigation node data, including the backing node array.
 */
void G_Ai_ShutdownNodes(void) {

  if (aiNodes) {
    for (size_t i = 0; i < aiNodes->count; i++) {
      GameAiNode *node = AI_NODE(aiNodes, i);

      if (node->links) {
        release(node->links);
      }
    }

    aiNodes = release(aiNodes);
  }

  aiPlatforms = release(aiPlatforms);

  G_Ai_Node_FreeCompiled();

  G_Ai_Node_InvalidateSpatialIndex();
  G_Ai_Node_FreePathPool();
}

typedef struct {
  GameAiNodeId id;
  float priority;
} GameAiNodePriority;

static struct GameAiHeap *aiNodePathQueue;
static GameAiNodePriority *aiNodePathEntries;
static size_t aiNodePathCapacity;
static size_t aiNodePathCount;

/**
 * @brief Releases the pathfinding queue and its entries.
 */
static void G_Ai_Node_FreePathPool(void) {

  G_Ai_HeapFree(&aiNodePathQueue);
  free(aiNodePathEntries);
  aiNodePathEntries = NULL;
  aiNodePathCapacity = 0;
  aiNodePathCount = 0;
}

/**
 * @brief Readies the pathfinding queue and entry pool for `capacity` nodes, reusing them when they suffice.
 */
static bool G_Ai_Node_EnsurePathPool(const size_t capacity) {

  if (aiNodePathQueue && aiNodePathCapacity >= capacity) {
    G_Ai_HeapReset(aiNodePathQueue);
    aiNodePathCount = 0;
    return true;
  }

  G_Ai_Node_FreePathPool();

  aiNodePathQueue = G_Ai_HeapCreate(capacity);
  aiNodePathEntries = malloc(sizeof(GameAiNodePriority) * capacity);

  if (!aiNodePathQueue || !aiNodePathEntries) {
    G_Ai_Node_FreePathPool();
    return false;
  }

  aiNodePathCapacity = capacity;
  return true;
}

/**
 * @brief The next free pathfinding entry, or `NULL` when the pool is spent.
 */
static GameAiNodePriority *G_Ai_Node_AllocPathEntry(void) {

  if (aiNodePathCount == aiNodePathCapacity) {
    return NULL;
  }

  return &aiNodePathEntries[aiNodePathCount++];
}

#define AI_MAX_DROP_HEIGHT 512.f
#define AI_DROP_PENALTY_START 128.f
#define AI_DROP_PENALTY_SCALE 0.5f
#define AI_DROP_HEALTH_MARGIN 8.f
#define AI_DROP_DAMAGE_PENALTY_SCALE 6.f

/**
 * @brief The stored cost of the link from `a` to `b`.
 */
static inline float G_Ai_LinkCost(const GameAiNodeId a, const GameAiNodeId b) {
  const GameAiNode *node = AI_NODE(aiNodes, a);

  assert(node->links);

  for (uint32_t i = 0; i < node->links->count; i++) {
    const GameAiLink *link = AI_LINK(node->links, i);

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
 * @return The contents at the node: compiled with the graph, or tested.
 */
static int32_t G_Ai_Node_Contents(const GameAiNode *node) {
  return node->voxel != -1 ? node->contents : gi.PointContents(node->position);
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
Vector *G_Ai_Node_FindPath(const GameClient *cl, const GameAiNodeId start, const GameAiNodeId end, const GameAiNodeCostFunc heuristic, const float maxCost, float *length) {
  
  if (length) {
    *length = 0;
  }

  // sanity
  if (start == AI_NODE_INVALID || end == AI_NODE_INVALID) {
    return NULL;
  }
  
  // size on 64k nodes is, say, 8kb.
  const size_t costsStartedWords = ((size_t) aiNodes->count + 31u) / 32u;
  uint32_t *costsStarted = calloc(costsStartedWords, sizeof(*costsStarted));
  if (!costsStarted) {
    return NULL;
  }
  uint32_t visited = 0;
  // Min-heap open set (priority = f-cost). Replaces the previous sorted-array
  // queue (O(n) insertion) with an O(log n) binary heap from g_ai_grid.c.
  // Capacity is bounded by the node count plus headroom for re-insertions
  // (A* may push a better-cost duplicate before popping the stale one).
  const size_t heapCapacity = (size_t) aiNodes->count * 4 + 16;
  if (!G_Ai_Node_EnsurePathPool(heapCapacity)) {
    free(costsStarted);
    return NULL;
  }

  struct GameAiHeap *queue = aiNodePathQueue;
  bool finished = false;

  {
    GameAiNodePriority *e = G_Ai_Node_AllocPathEntry();
    e->id = start;
    e->priority = 0;
    G_Ai_HeapPush(queue, e->priority, e);
  }

  GameAiNode *startNode = AI_NODE(aiNodes, start);
  startNode->cost = 0;
  costsStarted[start / 32] |= (uint32_t)1 << (start % 32);
  visited++;

  for (;;) {
    GameAiNodePriority *current = (GameAiNodePriority *) G_Ai_HeapPop(queue);
    if (!current) {
      break;
    }

    if (current->id == end) {
      finished = true;
      break;
    }

    GameAiNode *node = AI_NODE(aiNodes, current->id);

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
    const int32_t nodeContents = G_Ai_Node_Contents(node);
    const bool fromHazard = (nodeContents & (CONTENTS_LAVA | CONTENTS_SLIME)) != 0;

    for (size_t i = 0; i < node->links->count; i++) {
      const GameAiLink *link = AI_LINK(node->links, i);
      GameAiNode *linkNode = AI_NODE(aiNodes, link->id);
      const float drop = node->position.z - linkNode->position.z;
      const int32_t linkContents = G_Ai_Node_Contents(linkNode);
      const bool toHazard = (linkContents & (CONTENTS_LAVA | CONTENTS_SLIME)) != 0;

      if (G_Ai_Node_RaisedPlatform(linkNode->position)) {
        continue;
      }

      if (fromHazard && toHazard) {
        continue;
      }

      const bool compiledLink = node->voxel != -1 && linkNode->voxel != -1;
      if (!fromHazard && !compiledLink && G_Ai_LinkPassesHazard(node->position, linkNode->position)) {
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

      if (maxCost > 0.f && newCost > maxCost) {
        continue;
      }

      GameAiNodeId linkIndex = G_Ai_Node_Index(linkNode);
      const bool found = (costsStarted[linkIndex / 32] & ((uint32_t) 1u << (linkIndex % 32))) != 0;
      if (!found) {
        costsStarted[linkIndex / 32] |= (uint32_t) 1u << (linkIndex % 32);
        visited++;
      }
      if (!found || newCost < linkNode->cost) {

        linkNode->cost = newCost;
        const float priority = newCost + heuristic(link->id, end);

        GameAiNodePriority *e = G_Ai_Node_AllocPathEntry();
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
        if (!G_Ai_HeapPush(queue, priority, e)) {
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

    returnPath = $(alloc(Vector), initWithSize, sizeof(GameAiNodeId));
    $(returnPath, insert, (void *) &end, 0);

    if (start != end) {
      GameAiNodeId from = end;

      for (;;) {
        const GameAiNode *fromNode = AI_NODE(aiNodes, from);
        from = fromNode->cameFrom;
        $(returnPath, insert, &from, 0);

        if (from == start) {
          break;
        }
      }

      if (length) {
        for (size_t i = 0; i < returnPath->count - 1; i++) {
          const GameAiNodeId a = AI_NODE_ID(returnPath, i);
          const GameAiNodeId b = AI_NODE_ID(returnPath, i + 1);

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

  if (module.compiled) {
    G_Warn("This map's nodes were compiled by quemap -nav; recompile them instead.\n");
    return;
  }

  Vec3 translate;

  if (gi.Argc() <= 1) {
    if (module.lastNodes[0] == AI_NODE_INVALID) {
      return;
    }

    const Vec3 node = G_Ai_Node_GetPosition(module.lastNodes[0]);
    const Vec3 playerPosition = module.position;
    translate = Vec3_Subtract(playerPosition, node);
  } else {
    const char *offset = gi.Argv(1);

    if (Parse_QuickPrimitive(offset, PARSER_DEFAULT, PARSE_DEFAULT, PARSE_FLOAT, &translate, 3) != 3) {
      return;
    }
  }

  for (uint32_t i = 0; i < aiNodes->count; i++) {
    GameAiNode *node = AI_NODE(aiNodes, i);
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
  const GameAiNodeId srcNode = G_Ai_Node_FindClosest(ent->s.origin, 512.f, true, true);

  if (srcNode == AI_NODE_INVALID) {
    return false;
  }

  // make a new node on the item
  CollisionTrace down = gi.Trace(ent->s.origin, Vec3_Subtract(ent->s.origin, MakeVec3(0, 0, MAX_WORLD_COORD)), Box3_Zero(), NULL, CONTENTS_MASK_SOLID);
  Vec3 pos;

  if (down.fraction == 1.0) {
    pos = ent->s.origin;
  } else {
    pos = Vec3_Subtract(down.end, MakeVec3(0.f, 0.f, G_PlayerBounds().mins.z));
  }

  // grab all the links of the node that brought us here
  const Vector *srcLinks = G_Ai_Node_GetLinks(srcNode);

  const GameAiNodeId newNode = G_Ai_Node_Create(pos);
  const float dist = Vec3_Distance(G_Ai_Node_GetPosition(srcNode), ent->s.origin);

  // bidirectionally connect us to source
  G_Ai_Node_Link(srcNode, newNode, dist);
  G_Ai_Node_Link(newNode, srcNode, dist);

  // if we had source links, link any connected bi-directional nodes
  // to the item as well
  if (srcLinks) {

    for (size_t i = 0; i < srcLinks->count; i++) {
      const GameAiLink *link = AI_LINK(srcLinks, i);

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
