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
 * @brief The contents that a player stands on and collides with.
 */
#define AI_NAV_MASK CONTENTS_MASK_CLIP_PLAYER

/**
 * @brief The height above a cell center at which its floor sample starts. Floors on a 16 unit
 * grid often lie exactly on cell centers, where neither a point test nor a trace ending there
 * finds them.
 */
#define AI_NAV_SAMPLE_OFFSET .5f

/**
 * @brief The spacing of the fine floor samples in a cell where its coarse samples find no floor.
 */
#define AI_NAV_FINE_STEP 8.f

/**
 * @brief The horizontal distance of one step of a simulated walk.
 */
#define AI_NAV_WALK_STEP 8.f

/**
 * @brief The furthest a simulated walk goes past the edge of a floor, looking for the drop.
 */
#define AI_NAV_LEDGE_DISTANCE (AI_NAV_CELL_SIZE + 16.f)

/**
 * @brief The furthest fall that a drop link spans.
 */
#define AI_NAV_MAX_DROP 512.f

/**
 * @brief The furthest a fall can land from the node it links to.
 */
#define AI_NAV_LANDING_DISTANCE AI_NAV_CELL_SIZE

/**
 * @brief The margin below the highest jump that a barrier jump may climb.
 */
#define AI_NAV_BARRIER_MARGIN 4.f

/**
 * @brief The most links that one node can have: a walk or barrier jump to each of 4 levels, and
 * a drop, in each of 8 directions.
 */
#define AI_NAV_MAX_NODE_LINKS 48

/**
 * @brief The most `trigger_hurt` and `trigger_push` entities that the build avoids.
 */
#define AI_NAV_MAX_TRIGGERS 256

/**
 * @brief The most closed doors and walls that the build treats as solid.
 */
#define AI_NAV_MAX_BARRIERS 256

/**
 * @brief The outcome of a floor sample.
 */
typedef enum {
  AI_NAV_SAMPLE_NONE,
  AI_NAV_SAMPLE_NO_FIT,
  AI_NAV_SAMPLE_TRIGGER,
  AI_NAV_SAMPLE_NODE,
} GameAiNavSample;

/**
 * @brief The build state.
 */
static struct {
  GameAiNavGraph *graph;
  float gravity;
  float barrierHeight;
  int32_t capacity;

  /**
   * @brief The bounds of the level's `trigger_hurt` and `trigger_push` entities, which no node or
   * walk may touch: one hurts, and the other throws the bot off its walk.
   */
  Box3 triggers[AI_NAV_MAX_TRIGGERS];
  int32_t numTriggers;

  /**
   * @brief The doors that a bot cannot open by walking up to them, and the walls, which the build
   * treats as solid as the world.
   */
  const GameEntity *barriers[AI_NAV_MAX_BARRIERS];
  int32_t numBarriers;
} module;

/**
 * @brief The horizontal sample offsets within a cell, in quarters of its width, center first.
 */
static const Vec2 offsets[] = {
  { { 0.f, 0.f } },
  { { -1.f, 0.f } }, { { 1.f, 0.f } }, { { 0.f, -1.f } }, { { 0.f, 1.f } },
  { { -1.f, -1.f } }, { { 1.f, -1.f } }, { { -1.f, 1.f } }, { { 1.f, 1.f } },
};

/**
 * @brief The 8 horizontal directions to neighboring cells.
 */
static const Vec3i directions[] = {
  { { 1, 0, 0 } }, { { -1, 0, 0 } }, { { 0, 1, 0 } }, { { 0, -1, 0 } },
  { { 1, 1, 0 } }, { { -1, 1, 0 } }, { { 1, -1, 0 } }, { { -1, -1, 0 } },
};

/**
 * @return The index of the cell at the given grid coordinate, or -1 if it is outside the grid.
 */
static int32_t G_Ai_Nav_CellIndex(const Vec3i xyz) {

  const Vec3i size = module.graph->size;

  if (xyz.x < 0 || xyz.y < 0 || xyz.z < 0 || xyz.x >= size.x || xyz.y >= size.y || xyz.z >= size.z) {
    return -1;
  }

  return (xyz.z * size.y + xyz.y) * size.x + xyz.x;
}

/**
 * @return The grid coordinate of the given cell index.
 */
static Vec3i G_Ai_Nav_CellCoordinate(const int32_t index) {

  const Vec3i size = module.graph->size;

  return MakeVec3i(index % size.x, (index / size.x) % size.y, index / (size.x * size.y));
}

/**
 * @return The grid coordinate of the cell that contains the given point.
 */
static Vec3i G_Ai_Nav_PointCoordinate(const Vec3 point) {

  const Vec3 rel = Vec3_Subtract(point, module.graph->mins);

  return MakeVec3i((int32_t) floorf(rel.x / AI_NAV_CELL_SIZE),
                   (int32_t) floorf(rel.y / AI_NAV_CELL_SIZE),
                   (int32_t) floorf(rel.z / AI_NAV_CELL_HEIGHT));
}

/**
 * @return The center of the cell at the given grid coordinate.
 */
static Vec3 G_Ai_Nav_CellOrigin(const Vec3i xyz) {
  return Vec3_Add(module.graph->mins, MakeVec3((xyz.x + .5f) * AI_NAV_CELL_SIZE,
                                               (xyz.y + .5f) * AI_NAV_CELL_SIZE,
                                               (xyz.z + .5f) * AI_NAV_CELL_HEIGHT));
}

/**
 * @return A player trace against the world and the barriers, so that players and other movers do
 * not block it.
 */
static CollisionTrace G_Ai_Nav_Trace(const Vec3 start, const Vec3 end, const Box3 bounds) {

  CollisionTrace trace = gi.Clip(start, end, bounds, ge.entities[0], AI_NAV_MASK);

  const Box3 swept = Box3_Union(Box3_Translate(bounds, start), Box3_Translate(bounds, end));

  for (int32_t i = 0; i < module.numBarriers; i++) {
    const GameEntity *barrier = module.barriers[i];

    if (!Box3_Intersects(swept, barrier->absBounds)) {
      continue;
    }

    const CollisionTrace tr = gi.Clip(start, end, bounds, barrier, AI_NAV_MASK);

    if (tr.allSolid || tr.fraction < trace.fraction) {
      const bool startSolid = trace.startSolid;
      trace = tr;
      trace.startSolid |= startSolid;
    } else if (tr.startSolid) {
      trace.startSolid = true;
    }
  }

  return trace;
}

/**
 * @return The leaf of the world's BSP that contains `point`.
 */
static const CollisionLeaf *G_Ai_Nav_PointLeaf(const CollisionBsp *bsp, const Vec3 point) {

  int32_t num = 0;

  while (num >= 0) {
    const CollisionNode *node = &bsp->nodes[num];
    const float dist = Vec3_Dot(point, node->plane->normal) - node->plane->dist;
    num = node->children[dist < 0.f];
  }

  return &bsp->leafs[-1 - num];
}

/**
 * @return The contents of the world brushes that contain `point`.
 * @remarks Leaf contents are the union of every brush in the leaf, and detail brushes do not
 * split leaves, so leaf contents are too coarse here. A leaf with no brushes is one that the BSP
 * filled as outside of the map.
 */
static int32_t G_Ai_Nav_PointContents(const Vec3 point) {

  const CollisionBsp *bsp = gi.Bsp();
  const CollisionLeaf *leaf = G_Ai_Nav_PointLeaf(bsp, point);

  if (leaf->numLeafBrushes == 0) {
    return leaf->contents;
  }

  int32_t contents = 0;

  for (int32_t i = 0; i < leaf->numLeafBrushes; i++) {
    const CollisionBrush *brush = &bsp->brushes[bsp->leafBrushes[leaf->firstLeafBrush + i]];
    if (gi.PointInsideBrush(point, brush)) {
      contents |= brush->contents;
    }
  }

  return contents;
}

/**
 * @return True if the bounds touch a `trigger_hurt` or a `trigger_push`.
 */
static bool G_Ai_Nav_TouchesTrigger(const Box3 bounds) {

  for (int32_t i = 0; i < module.numTriggers; i++) {
    if (Box3_Intersects(bounds, module.triggers[i])) {
      return true;
    }
  }

  return false;
}

/**
 * @return The bounding box of a standing or a crouched player.
 */
static Box3 G_Ai_Nav_Bounds(const bool crouch) {
  return crouch ? PM_CROUCHED_BOUNDS : PM_BOUNDS;
}

/**
 * @brief Rests the bounding box on the floor beneath `start`, as Pmove would.
 * @return True if the box fits at `start` and lands on a walkable floor within `distance`.
 */
static bool G_Ai_Nav_Rest(const Vec3 start, const float distance, const Box3 bounds, Vec3 *origin) {

  const CollisionTrace trace = G_Ai_Nav_Trace(start, Vec3_Subtract(start, Vec3_Scale(Vec3_Up(), distance)), bounds);

  if (trace.startSolid || trace.fraction == 1.f) {
    return false;
  }

  if (trace.plane.normal.z < PM_STEP_NORMAL) {
    return false;
  }

  *origin = trace.end;
  return true;
}

/**
 * @brief Samples the floor beneath `point`, within one cell height.
 * @param footprint The bounds that find the floor: a point, or the player's footprint, which also
 * finds a floor too narrow for any point to hit.
 */
static GameAiNavSample G_Ai_Nav_SampleFloor(const Vec3 point, const Box3 footprint, GameAiNavNode *node) {

  if (G_Ai_Nav_PointContents(point) & AI_NAV_MASK) {
    return AI_NAV_SAMPLE_NONE;
  }

  const Vec3 end = Vec3_Subtract(point, Vec3_Scale(Vec3_Up(), AI_NAV_CELL_HEIGHT));
  const CollisionTrace floor = G_Ai_Nav_Trace(point, end, footprint);

  if (floor.startSolid || floor.fraction == 1.f) {
    return AI_NAV_SAMPLE_NONE;
  }

  if (floor.plane.normal.z < PM_STEP_NORMAL) {
    return AI_NAV_SAMPLE_NONE;
  }

  const Vec3 normal = floor.plane.normal;
  const float lift = PM_BOUNDS.maxs.x * (fabsf(normal.x) + fabsf(normal.y)) / normal.z + 1.f;
  const float height = -PM_BOUNDS.mins.z;

  const Vec3 start = Vec3_Add(floor.end, Vec3_Scale(Vec3_Up(), height + lift));

  Vec3 origin;
  if (G_Ai_Nav_Rest(start, lift + 1.f, PM_BOUNDS, &origin)) {
    node->crouch = false;
  } else if (G_Ai_Nav_Rest(start, lift + 1.f, PM_CROUCHED_BOUNDS, &origin)) {
    node->crouch = true;
  } else {
    return AI_NAV_SAMPLE_NO_FIT;
  }

  if (G_Ai_Nav_PointContents(origin) & CONTENTS_MASK_SOLID) {
    return AI_NAV_SAMPLE_NO_FIT;
  }

  if (G_Ai_Nav_TouchesTrigger(Box3_Translate(G_Ai_Nav_Bounds(node->crouch), origin))) {
    return AI_NAV_SAMPLE_TRIGGER;
  }

  node->position = origin;
  node->contents = G_Ai_Nav_PointContents(origin) & CONTENTS_MASK_LIQUID;

  return AI_NAV_SAMPLE_NODE;
}

/**
 * @brief Samples the floor of one cell, at its center first and then at small offsets, and adds
 * its node to the graph. Where those find no floor, points `AI_NAV_FINE_STEP` apart across the
 * cell sample, to find a beam or a ledge between them, so that a node stands over its floor where it
 * can, and a bot does not steer onto the lip of a ledge. Last, the player's footprint samples at the
 * coarse points, for a floor too narrow for any point. A cell whose whole area finds no floor is
 * skipped.
 * @return The outcome of the sample.
 */
static GameAiNavSample G_Ai_Nav_SampleCell(const int32_t cell, bool *offset) {

  GameAiNavGraph *graph = module.graph;

  const Vec3 center = Vec3_Add(G_Ai_Nav_CellOrigin(G_Ai_Nav_CellCoordinate(cell)),
                               MakeVec3(0.f, 0.f, AI_NAV_SAMPLE_OFFSET));

  const float half = AI_NAV_CELL_SIZE * .5f;
  const Box3 area = MakeBox3(MakeVec3(-half, -half, 0.f), MakeVec3(half, half, 0.f));
  const CollisionTrace any = G_Ai_Nav_Trace(center, Vec3_Subtract(center, Vec3_Scale(Vec3_Up(), AI_NAV_CELL_HEIGHT)), area);

  if (!any.startSolid && any.fraction == 1.f) {
    return AI_NAV_SAMPLE_NONE;
  }

  const int32_t steps = (int32_t) (AI_NAV_CELL_SIZE / AI_NAV_FINE_STEP);
  const int32_t numFine = (int32_t) lengthof(offsets) + steps * steps;
  const int32_t numPoints = numFine + (int32_t) lengthof(offsets);

  const Box3 footprint = MakeBox3(MakeVec3(PM_BOUNDS.mins.x, PM_BOUNDS.mins.y, 0.f),
                                  MakeVec3(PM_BOUNDS.maxs.x, PM_BOUNDS.maxs.y, 0.f));

  GameAiNavSample sample = AI_NAV_SAMPLE_NONE;

  for (int32_t i = 0; i < numPoints; i++) {

    Vec2 delta;
    if (i < (int32_t) lengthof(offsets)) {
      delta = Vec2_Scale(offsets[i], AI_NAV_CELL_SIZE * .25f);
    } else if (i < numFine) {
      const int32_t j = i - (int32_t) lengthof(offsets);
      delta = MakeVec2((j % steps + .5f) * AI_NAV_FINE_STEP - half, (j / steps + .5f) * AI_NAV_FINE_STEP - half);
    } else {
      delta = Vec2_Scale(offsets[i - numFine], AI_NAV_CELL_SIZE * .25f);
    }

    const Vec3 point = Vec3_Add(center, MakeVec3(delta.x, delta.y, 0.f));

    GameAiNavNode node = { .cell = cell };

    const GameAiNavSample result = G_Ai_Nav_SampleFloor(point, i < numFine ? Box3_Zero() : footprint, &node);
    if (result == AI_NAV_SAMPLE_NODE) {

      if (graph->numNodes == module.capacity) {
        module.capacity = Maxi(1024, module.capacity * 2);
        graph->nodes = realloc(graph->nodes, sizeof(GameAiNavNode) * module.capacity);
        if (!graph->nodes) {
          G_Error("Failed to allocate %d navigation nodes\n", module.capacity);
        }
      }

      graph->cellNodes[cell] = graph->numNodes;
      graph->nodes[graph->numNodes++] = node;

      *offset = i > 0;
      return AI_NAV_SAMPLE_NODE;
    }

    if (result == AI_NAV_SAMPLE_NO_FIT || result == AI_NAV_SAMPLE_TRIGGER) {
      sample = Maxi(sample, result);
    }
  }

  return sample;
}

/**
 * @brief Moves the bounding box toward `target`, as Pmove would: up by `height`, across, and down
 * onto the floor.
 * @return True if the box arrives on a walkable floor.
 */
static bool G_Ai_Nav_StepMove(const Vec3 from, const Vec3 target, const Box3 bounds, const float height, Vec3 *to) {

  const CollisionTrace up = G_Ai_Nav_Trace(from, Vec3_Add(from, Vec3_Scale(Vec3_Up(), height)), bounds);
  if (up.startSolid) {
    return false;
  }

  const Vec3 across = MakeVec3(target.x, target.y, up.end.z);
  const CollisionTrace over = G_Ai_Nav_Trace(up.end, across, bounds);
  if (over.fraction < 1.f) {
    return false;
  }

  return G_Ai_Nav_Rest(over.end, height + PM_STEP_HEIGHT + 1.f, bounds, to);
}

/**
 * @return The travel time for the given distance and speed, in hundredths of a second.
 */
static int32_t G_Ai_Nav_TravelTime(const float distance, const float speed) {
  return Maxi(1, (int32_t) ceilf(distance / speed * 100.f));
}

/**
 * @return The time to fall the given height, in hundredths of a second.
 */
static int32_t G_Ai_Nav_FallTime(const float height) {
  return (int32_t) ceilf(sqrtf(2.f * height / module.gravity) * 100.f);
}

/**
 * @return The node closest to `origin`, standing on the same floor within
 * `AI_NAV_LANDING_DISTANCE`, or -1.
 */
static int32_t G_Ai_Nav_NodeForOrigin(const Vec3 origin) {

  const GameAiNavGraph *graph = module.graph;

  const Vec3 floor = Vec3_Subtract(origin, Vec3_Scale(Vec3_Up(), -PM_BOUNDS.mins.z - AI_NAV_CELL_HEIGHT * .5f));
  const Vec3i xyz = G_Ai_Nav_PointCoordinate(floor);

  int32_t best = -1;
  float bestDistance = AI_NAV_LANDING_DISTANCE * AI_NAV_LANDING_DISTANCE;

  for (int32_t dz = -1; dz <= 1; dz++) {
    for (int32_t dy = -1; dy <= 1; dy++) {
      for (int32_t dx = -1; dx <= 1; dx++) {

        const int32_t cell = G_Ai_Nav_CellIndex(MakeVec3i(xyz.x + dx, xyz.y + dy, xyz.z + dz));
        if (cell == -1 || graph->cellNodes[cell] == -1) {
          continue;
        }

        const GameAiNavNode *node = &graph->nodes[graph->cellNodes[cell]];
        if (fabsf(node->position.z - origin.z) > PM_STEP_HEIGHT) {
          continue;
        }

        const float distance = Vec3_DistanceSquared(node->position, origin);
        if (distance < bestDistance) {
          bestDistance = distance;
          best = graph->cellNodes[cell];
        }
      }
    }
  }

  return best;
}

/**
 * @brief Adds a link to `links` that leads to `node`, unless one exists.
 */
static void G_Ai_Nav_AddLink(GameAiNavLink *links, int32_t *numLinks, const int32_t node,
                             const GameAiTravelType type, const int32_t time) {

  for (int32_t i = 0; i < *numLinks; i++) {
    if (links[i].node == node) {
      return;
    }
  }

  if (*numLinks == AI_NAV_MAX_NODE_LINKS) {
    G_Error("A navigation node has more than %d links\n", AI_NAV_MAX_NODE_LINKS);
  }

  links[(*numLinks)++] = (GameAiNavLink) {
    .node = node,
    .travelType = type,
    .travelTime = time
  };
}

/**
 * @brief Simulates a walk from node `a` to node `b`.
 * @return True if the walk arrives at `b`.
 */
static bool G_Ai_Nav_Walk(const GameAiNavNode *a, const GameAiNavNode *b, float *distance) {

  const Box3 bounds = G_Ai_Nav_Bounds(a->crouch || b->crouch);

  const Vec3 delta = Vec3_Subtract(b->position, a->position);
  const float length = Vec2_Length(Vec3_XY(delta));
  const int32_t steps = Maxi(1, (int32_t) ceilf(length / AI_NAV_WALK_STEP));

  Vec3 origin = a->position;
  *distance = 0.f;

  for (int32_t i = 1; i <= steps; i++) {

    const Vec3 target = Vec3_Mix(a->position, b->position, i / (float) steps);

    Vec3 next;
    if (!G_Ai_Nav_StepMove(origin, target, bounds, PM_STEP_HEIGHT, &next)) {
      return false;
    }

    if (G_Ai_Nav_TouchesTrigger(Box3_Translate(bounds, next))) {
      return false;
    }

    *distance += Vec3_Distance(origin, next);
    origin = next;
  }

  return fabsf(origin.z - b->position.z) <= 1.f;
}

/**
 * @brief Simulates a jump straight up from node `a` onto the ledge of node `b`.
 * @return True if the jump clears the ledge and lands at `b`.
 */
static bool G_Ai_Nav_BarrierJump(const GameAiNavNode *a, const GameAiNavNode *b, float *distance) {

  if (a->crouch || b->crouch) {
    return false;
  }

  const float rise = b->position.z - a->position.z;
  if (rise <= PM_STEP_HEIGHT || rise > module.barrierHeight) {
    return false;
  }

  Vec3 origin;
  if (!G_Ai_Nav_StepMove(a->position, b->position, PM_BOUNDS, module.barrierHeight, &origin)) {
    return false;
  }

  *distance = Vec2_Distance(Vec3_XY(a->position), Vec3_XY(b->position));

  return fabsf(origin.z - b->position.z) <= 1.f;
}

/**
 * @brief Simulates a walk from `node` in `direction`, off a ledge, and the fall that follows.
 * @return The node that the fall lands at, or -1.
 */
static int32_t G_Ai_Nav_Drop(const GameAiNavNode *node, const Vec3i direction, float *distance, float *height) {

  const Box3 bounds = G_Ai_Nav_Bounds(node->crouch);

  const Vec3 dir = Vec3_Normalize(MakeVec3(direction.x, direction.y, 0.f));

  Vec3 origin = node->position;
  *distance = 0.f;

  for (float d = AI_NAV_WALK_STEP; d <= AI_NAV_LEDGE_DISTANCE; d += AI_NAV_WALK_STEP) {

    const Vec3 target = Vec3_Add(node->position, Vec3_Scale(dir, d));

    const CollisionTrace across = G_Ai_Nav_Trace(origin, MakeVec3(target.x, target.y, origin.z), bounds);
    if (across.fraction < 1.f) {
      return -1;
    }

    *distance += Vec3_Distance(origin, across.end);
    origin = across.end;

    if (G_Ai_Nav_TouchesTrigger(Box3_Translate(bounds, origin))) {
      return -1;
    }

    Vec3 floor;
    if (G_Ai_Nav_Rest(origin, PM_STEP_HEIGHT + 1.f, bounds, &floor)) {
      origin = floor;
      continue;
    }

    const Vec3 end = Vec3_Subtract(origin, Vec3_Scale(Vec3_Up(), AI_NAV_MAX_DROP));
    const CollisionTrace fall = G_Ai_Nav_Trace(origin, end, bounds);

    if (fall.startSolid || fall.fraction == 1.f || fall.plane.normal.z < PM_STEP_NORMAL) {
      return -1;
    }

    *height = node->position.z - fall.end.z;
    if (*height <= PM_STEP_HEIGHT) {
      return -1;
    }

    return G_Ai_Nav_NodeForOrigin(fall.end);
  }

  return -1;
}

/**
 * @brief Links one node to its neighbors: walks to the nodes in adjacent cells, and drops where no
 * walk exists.
 * @return The count of links written to `links`.
 */
static int32_t G_Ai_Nav_LinkNode(const int32_t index, GameAiNavLink *links) {

  const GameAiNavGraph *graph = module.graph;
  const GameAiNavNode *node = &graph->nodes[index];

  const Vec3i xyz = G_Ai_Nav_CellCoordinate(node->cell);

  int32_t numLinks = 0;

  for (size_t i = 0; i < lengthof(directions); i++) {

    bool walked = false;

    for (int32_t dz = -1; dz <= 2; dz++) {

      const Vec3i neighbor = MakeVec3i(xyz.x + directions[i].x, xyz.y + directions[i].y, xyz.z + dz);

      const int32_t cell = G_Ai_Nav_CellIndex(neighbor);
      if (cell == -1 || graph->cellNodes[cell] == -1) {
        continue;
      }

      const int32_t target = graph->cellNodes[cell];
      const GameAiNavNode *other = &graph->nodes[target];

      float distance;
      if (G_Ai_Nav_Walk(node, other, &distance)) {
        const bool crouch = node->crouch || other->crouch;
        const GameAiTravelType type = crouch ? AI_TRAVEL_CROUCH : AI_TRAVEL_WALK;
        G_Ai_Nav_AddLink(links, &numLinks, target, type,
                         G_Ai_Nav_TravelTime(distance, crouch ? PM_SPEED_DUCKED : PM_SPEED_RUN));
        walked = true;
      } else if (G_Ai_Nav_BarrierJump(node, other, &distance)) {
        const float fall = module.barrierHeight - (other->position.z - node->position.z);
        const int32_t time = G_Ai_Nav_TravelTime(distance, PM_SPEED_RUN) +
                             G_Ai_Nav_FallTime(module.barrierHeight) + G_Ai_Nav_FallTime(fall);
        G_Ai_Nav_AddLink(links, &numLinks, target, AI_TRAVEL_BARRIER_JUMP, time);
        walked = true;
      }
    }

    if (walked) {
      continue;
    }

    float distance, height;
    const int32_t target = G_Ai_Nav_Drop(node, directions[i], &distance, &height);
    if (target != -1 && target != index) {
      const int32_t time = G_Ai_Nav_TravelTime(distance, PM_SPEED_RUN) + G_Ai_Nav_FallTime(height);
      G_Ai_Nav_AddLink(links, &numLinks, target, AI_TRAVEL_WALK_OFF_LEDGE, time);
    }
  }

  return numLinks;
}

/**
 * @return The highest ledge that every movement can jump onto: the smallest jump height over all
 * movements, less `AI_NAV_BARRIER_MARGIN`.
 */
static float G_Ai_Nav_BarrierHeight(void) {

  float height = FLT_MAX;

  for (size_t i = 0; i < Pm_MovementCount(); i++) {

    const PMoveParams *params = Pm_Movement((PMovement) i)->params;

    const float jump = params ? params->speedJump : PM_SPEED_JUMP;
    const float gravity = (params && params->gravity) ? Maxf(params->gravity, module.gravity) : module.gravity;

    height = Minf(height, jump * jump / (2.f * gravity));
  }

  return height - AI_NAV_BARRIER_MARGIN;
}

/**
 * @brief Gathers the bounds of the level's `trigger_hurt` and `trigger_push` entities. A player
 * touches a trigger where their bounds intersect, as `G_TouchOccupy` does, so that is where no node
 * may stand and no walk may pass. A jump pad's own link starts at the nearest node beside it.
 */
static void G_Ai_Nav_InitTriggers(void) {

  G_ForEachEntity(ent, {
    if (!ent->classname) {
      continue;
    }

    if (Str_Compare(ent->classname, "trigger_hurt") && Str_Compare(ent->classname, "trigger_push")) {
      continue;
    }

    if (module.numTriggers == AI_NAV_MAX_TRIGGERS) {
      G_Warn("More than %d hurt and push triggers; bots may walk into the rest\n", AI_NAV_MAX_TRIGGERS);
      break;
    }

    module.triggers[module.numTriggers++] = ent->absBounds;
  });
}

/**
 * @brief Gathers the doors that a bot cannot open by walking up to them, and the walls. A door with
 * `health` or a `targetname` opens when shot or triggered, and has no trigger that opens it for a
 * player who walks up to it (`G_func_door`).
 */
static void G_Ai_Nav_InitBarriers(void) {

  G_ForEachEntity(ent, {
    if (!ent->classname || ent->solid != SOLID_BSP) {
      continue;
    }

    const bool door = !Str_Compare(ent->classname, "func_door") || !Str_Compare(ent->classname, "func_door_rotating");
    const bool wall = !Str_Compare(ent->classname, "func_wall");

    if (!wall && !(door && (ent->health || ent->targetName))) {
      continue;
    }

    if (module.numBarriers == AI_NAV_MAX_BARRIERS) {
      G_Warn("More than %d closed doors and walls; bots may path through the rest\n", AI_NAV_MAX_BARRIERS);
      break;
    }

    module.barriers[module.numBarriers++] = ent;
  });
}

/**
 * @brief Lays the cell grid over the world model's bounds, aligned to the cell size.
 */
static void G_Ai_Nav_InitGrid(GameAiNavGraph *graph) {

  const Box3 bounds = gi.Bsp()->models[0].bounds;

  const Vec3 mins = MakeVec3(floorf(bounds.mins.x / AI_NAV_CELL_SIZE) * AI_NAV_CELL_SIZE,
                             floorf(bounds.mins.y / AI_NAV_CELL_SIZE) * AI_NAV_CELL_SIZE,
                             floorf(bounds.mins.z / AI_NAV_CELL_HEIGHT) * AI_NAV_CELL_HEIGHT);

  graph->mins = mins;
  graph->size = MakeVec3i((int32_t) ceilf((bounds.maxs.x - mins.x) / AI_NAV_CELL_SIZE),
                          (int32_t) ceilf((bounds.maxs.y - mins.y) / AI_NAV_CELL_SIZE),
                          (int32_t) ceilf((bounds.maxs.z - mins.z) / AI_NAV_CELL_HEIGHT));
  graph->numCells = graph->size.x * graph->size.y * graph->size.z;

  graph->cellNodes = malloc(sizeof(int32_t) * Maxi(1, graph->numCells));
  if (!graph->cellNodes) {
    G_Error("Failed to allocate %d navigation cells\n", graph->numCells);
  }

  for (int32_t i = 0; i < graph->numCells; i++) {
    graph->cellNodes[i] = -1;
  }
}

/**
 * @see g_ai_nav.h
 */
bool G_Ai_Nav_Build(GameAiNavGraph *graph) {

  memset(graph, 0, sizeof(*graph));
  memset(&module, 0, sizeof(module));

  module.graph = graph;
  module.gravity = G_LevelGravity();
  module.barrierHeight = G_Ai_Nav_BarrierHeight();

  G_Ai_Nav_InitGrid(graph);

  G_Ai_Nav_InitTriggers();

  G_Ai_Nav_InitBarriers();

  int32_t numOffset = 0, numNoFit = 0, numTrigger = 0;

  for (int32_t i = 0; i < graph->numCells; i++) {
    bool offset = false;

    switch (G_Ai_Nav_SampleCell(i, &offset)) {
      case AI_NAV_SAMPLE_NODE:
        numOffset += offset;
        break;
      case AI_NAV_SAMPLE_NO_FIT:
        numNoFit++;
        break;
      case AI_NAV_SAMPLE_TRIGGER:
        numTrigger++;
        break;
      default:
        break;
    }
  }

  if (!graph->numNodes) {
    G_Ai_Nav_Free(graph);
    return false;
  }

  int32_t capacity = graph->numNodes * 8;
  graph->links = malloc(sizeof(GameAiNavLink) * capacity);
  if (!graph->links) {
    G_Error("Failed to allocate %d navigation links\n", capacity);
  }

  int32_t counts[AI_TRAVEL_ELEVATOR + 1] = { 0 };

  for (int32_t i = 0; i < graph->numNodes; i++) {
    GameAiNavLink links[AI_NAV_MAX_NODE_LINKS];

    const int32_t numLinks = G_Ai_Nav_LinkNode(i, links);

    if (graph->numLinks + numLinks > capacity) {
      capacity *= 2;
      graph->links = realloc(graph->links, sizeof(GameAiNavLink) * capacity);
      if (!graph->links) {
        G_Error("Failed to allocate %d navigation links\n", capacity);
      }
    }

    graph->nodes[i].firstLink = graph->numLinks;
    graph->nodes[i].numLinks = numLinks;

    for (int32_t j = 0; j < numLinks; j++) {
      graph->links[graph->numLinks++] = links[j];
      counts[links[j].travelType]++;
    }
  }

  gi.Print("  Built %d navigation nodes, %d at an offset, and found %d floors with no room for a player "
           "and %d in a hurt or push trigger\n", graph->numNodes, numOffset, numNoFit, numTrigger);
  gi.Print("  Built %d walk, %d crouch, %d barrier jump and %d walk off ledge links\n",
           counts[AI_TRAVEL_WALK], counts[AI_TRAVEL_CROUCH], counts[AI_TRAVEL_BARRIER_JUMP],
           counts[AI_TRAVEL_WALK_OFF_LEDGE]);

  return true;
}

/**
 * @see g_ai_nav.h
 */
void G_Ai_Nav_FreeNodes(GameAiNavGraph *graph) {

  free(graph->nodes);
  graph->nodes = NULL;

  free(graph->links);
  graph->links = NULL;
}

/**
 * @see g_ai_nav.h
 */
void G_Ai_Nav_Free(GameAiNavGraph *graph) {

  free(graph->nodes);
  free(graph->links);
  free(graph->cellNodes);

  memset(graph, 0, sizeof(*graph));
}
