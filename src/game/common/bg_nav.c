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

#include "bg_nav.h"
#include "bg_pmove.h"

/**
 * @brief The contents that a bot stands on and collides with: a player's, and monster clip, which
 * only bots collide with, so that a mapper MAY keep bots off a place that players use.
 */
#define NAV_MASK CONTENTS_MASK_CLIP_MONSTER

/**
 * @brief The height above a cell center at which its floor sample starts. Floors on a 16 unit
 * grid often lie exactly on cell centers, where neither a point test nor a trace ending there
 * finds them.
 */
#define NAV_SAMPLE_OFFSET .5f

/**
 * @brief The spacing of the fine floor samples in a cell where its coarse samples find no floor.
 */
#define NAV_FINE_STEP 8.f

/**
 * @brief The horizontal distance of one step of a simulated walk.
 */
#define NAV_WALK_STEP 8.f

/**
 * @brief The furthest a simulated walk goes past the edge of a floor, looking for the drop.
 */
#define NAV_LEDGE_DISTANCE (NAV_CELL_SIZE + 16.f)

/**
 * @brief The furthest fall that a drop link spans.
 */
#define NAV_MAX_DROP 512.f

/**
 * @brief The distance from a node within which an edge over the void makes it an edge node.
 */
#define NAV_EDGE_DISTANCE NAV_CELL_SIZE

/**
 * @brief The furthest a fall can land from the node it links to.
 */
#define NAV_LANDING_DISTANCE NAV_CELL_SIZE

/**
 * @brief The margin below the highest jump that a barrier jump may climb.
 */
#define NAV_BARRIER_MARGIN 4.f

/**
 * @brief The deepest water that a swim link rises through to the surface.
 */
#define NAV_MAX_SWIM_DEPTH 512.f

/**
 * @brief The step at which a swim up to the surface tests the water.
 */
#define NAV_SWIM_STEP 8.f

/**
 * @brief The depth below the surface at which a player who swims up floats, deep enough to water
 * jump.
 */
#define NAV_SWIM_FLOAT 16.f

/**
 * @brief The margin below the highest ledge that a water jump climbs onto, which a swim link may
 * climb out onto.
 */
#define NAV_WATER_JUMP_MARGIN 8.f

/**
 * @brief The fraction of the run speed at which a jump link is simulated, so that a bot that has
 * not reached full speed still clears the gap.
 */
#define NAV_JUMP_SPEED_FRACTION .8f

/**
 * @brief The fraction of the run speed at which a jump over the place of a door is simulated. The
 * door closes the gap only while a button holds it, so the jump is the way back across for a bot
 * that crossed on the door, which a jump at `NAV_JUMP_SPEED_FRACTION` would strand.
 */
#define NAV_GATED_JUMP_SPEED_FRACTION .9f

/**
 * @brief The distance before the edge of the floor at which a jump link is simulated to take off,
 * because a bot jumps up to one frame early.
 */
#define NAV_JUMP_TAKEOFF_MARGIN 16.f

/**
 * @brief The height above its target at which the arc of a jump link MUST arrive over it.
 */
#define NAV_JUMP_CLEARANCE 8.f

/**
 * @brief The most cells away, in one direction, that a jump link lands.
 */
#define NAV_MAX_JUMP_CELLS 3

/**
 * @brief The most walk steps that a jump link takes to the edge of the floor: more than the
 * diagonal of `NAV_MAX_JUMP_CELLS` cells.
 */
#define NAV_MAX_JUMP_STEPS 64

/**
 * @brief The distance past the landing of a jump link that MUST have floor, because the player
 * keeps its speed when it lands.
 */
#define NAV_JUMP_LANDING_DISTANCE 32.f

/**
 * @brief The time step in seconds of a simulated jump.
 */
#define NAV_JUMP_STEP .025f

/**
 * @brief The most links that one node can have: a walk or barrier jump to each of 4 levels, a drop
 * and a jump, in each of 8 directions, and swims to the cells around a node in water and around the
 * surface above it.
 */
#define NAV_MAX_NODE_LINKS 128

/**
 * @brief The margin by which the build widens a `trigger_push`, so that nodes and walks keep this
 * far from it, because a bot drifts a few units off its links, and a pad beside a link throws it.
 */
#define NAV_PUSH_MARGIN 16.f

/**
 * @brief The `start_off` spawn flag of a `trigger_push`.
 */
#define NAV_PUSH_START_OFF 4

/**
 * @brief The most `trigger_hurt` and `trigger_push` entities that the build avoids.
 */
#define NAV_MAX_TRIGGERS 256

/**
 * @brief The most closed doors and walls that the build treats as solid.
 */
#define NAV_MAX_BARRIERS 256

/**
 * @brief The `func_rotating` spawn flags that turn it about a horizontal axis: x_axis and y_axis.
 */
#define NAV_ROTATING_HORIZONTAL 0xc

/**
 * @brief The `func_wall` spawn flags: triggered, toggle and start on.
 */
#define NAV_WALL_SPAWN_FLAGS 0x7

/**
 * @brief The `func_wall` spawn flag of a wall that spawns solid.
 */
#define NAV_WALL_START_ON 0x4

/**
 * @brief The most climbs of the ladders of a level.
 */
#define NAV_MAX_CLIMBS 1024

/**
 * @brief The most brushes of the level's ladders.
 */
#define NAV_MAX_LADDER_BRUSHES 1024

/**
 * @brief The distance in units between two columns that climb one face of a ladder.
 */
#define NAV_LADDER_SPACING (NAV_FINE_STEP * 2.f)

/**
 * @brief The time in hundredths of a second that a climb takes beyond its height at
 * `PM_SPEED_LADDER`: to come to the ladder, turn to it and get on. A bot climbs about this much
 * slower than the speed alone.
 */
#define NAV_LADDER_DELAY 100

/**
 * @brief The distance in units ahead of a player that `Pm_CheckLadder` traces for a ladder.
 */
#define NAV_LADDER_REACH 4.f

/**
 * @brief The most height in units below the bottom of a ladder at which its floor, or the top of a
 * liquid in which a player swims to it, MAY be.
 */
#define NAV_LADDER_FLOOR (NAV_CELL_HEIGHT * 2.f)

/**
 * @brief The distance in units beyond the far side of the top of a ladder that a player walks onto
 * the floor there.
 */
#define NAV_LADDER_EXIT 16.f

/**
 * @brief The most that a detour through one other node in liquid MAY cost, as a multiple of the time
 * of a swim link, for `Nav_PruneSwims` to remove the link.
 */
#define NAV_SWIM_PRUNE 2.f

/**
 * @brief The outcome of a floor sample.
 */
typedef enum {
  NAV_SAMPLE_NONE,
  NAV_SAMPLE_NO_FIT,
  NAV_SAMPLE_TRIGGER,
  NAV_SAMPLE_NODE,
} NavSample;

/**
 * @brief A triggered door, a wall, a train, a bob or a rotating model: an inline model, where it
 * spawns.
 */
typedef struct {
  /**
   * @brief The index of the inline model.
   */
  int32_t model;

  /**
   * @brief True if the model's floors move with it, so that the nodes on them are linked only to
   * each other, and the game links them to the rest as the model moves.
   */
  bool moves;

  /**
   * @brief True for a door, whose doorway a walk MAY cross while the door is open.
   */
  bool gate;

  /**
   * @brief The first barrier of the doors with the door's `targetname`, which move together, or
   * the door's own.
   */
  int32_t group;

  /**
   * @brief The model's head node.
   */
  int32_t headNode;

  /**
   * @brief The entity's origin, by which its model is placed.
   */
  Vec3 origin;

  /**
   * @brief The model's bounds, placed.
   */
  Box3 bounds;
} NavBarrier;

/**
 * @brief The build state.
 */
static struct {
  const NavParams *params;
  NavGraph *graph;
  float gravity;
  float barrierHeight;
  float waterJumpHeight;
  int32_t capacity;

  /**
   * @brief The bounds of the level's `trigger_hurt` and `trigger_push` entities, which no node or
   * walk may touch: one hurts, and the other throws the bot off its walk.
   */
  Box3 triggers[NAV_MAX_TRIGGERS];
  int32_t numTriggers;

  /**
   * @brief The doors that a bot cannot open by walking up to them, and the walls, which the build
   * treats as solid as the world.
   */
  NavBarrier barriers[NAV_MAX_BARRIERS];
  int32_t numBarriers;

  /**
   * @brief The barrier whose model the last `Nav_Trace` hit first, or -1 for the world.
   */
  int32_t traceBarrier;

  /**
   * @brief The group of the doors that `Nav_Trace` passes through, to find the walks and jumps
   * through their doorways, or -1.
   */
  int32_t openGate;

  /**
   * @brief True while `Nav_JumpArc` simulates a jump at `NAV_GATED_JUMP_SPEED_FRACTION`.
   */
  bool gatedSpeed;

  /**
   * @brief True if `Nav_OnLadder` found a ladder face behind or level with another brush's face,
   * since `Nav_FindClimbs` began the ladder.
   */
  bool ladderHidden;

  /**
   * @brief The index in `NavGraph::ladders` of the face that `Nav_FindClimbs` climbs.
   */
  int32_t ladder;
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
static int32_t Nav_CellIndex(const Vec3i xyz) {

  const Vec3i size = module.graph->size;

  if (xyz.x < 0 || xyz.y < 0 || xyz.z < 0 || xyz.x >= size.x || xyz.y >= size.y || xyz.z >= size.z) {
    return -1;
  }

  return (xyz.z * size.y + xyz.y) * size.x + xyz.x;
}

/**
 * @return The grid coordinate of the given cell index.
 */
static Vec3i Nav_CellCoordinate(const int32_t index) {

  const Vec3i size = module.graph->size;

  return MakeVec3i(index % size.x, (index / size.x) % size.y, index / (size.x * size.y));
}

/**
 * @return The grid coordinate of the cell that contains the given point.
 */
static Vec3i Nav_PointCoordinate(const Vec3 point) {

  const Vec3 rel = Vec3_Subtract(point, module.graph->mins);

  return MakeVec3i((int32_t) floorf(rel.x / NAV_CELL_SIZE),
                   (int32_t) floorf(rel.y / NAV_CELL_SIZE),
                   (int32_t) floorf(rel.z / NAV_CELL_HEIGHT));
}

/**
 * @return The center of the cell at the given grid coordinate.
 */
static Vec3 Nav_CellOrigin(const Vec3i xyz) {
  return Vec3_Add(module.graph->mins, MakeVec3((xyz.x + .5f) * NAV_CELL_SIZE,
                                               (xyz.y + .5f) * NAV_CELL_SIZE,
                                               (xyz.z + .5f) * NAV_CELL_HEIGHT));
}

/**
 * @return A player trace against the world and the barriers, so that players and other movers do
 * not block it.
 */
static CollisionTrace Nav_Trace(const Vec3 start, const Vec3 end, const Box3 bounds) {

  CollisionTrace trace = module.params->BoxTrace(start, end, bounds, 0, NAV_MASK);

  module.traceBarrier = -1;

  const Box3 swept = Box3_Union(Box3_Translate(bounds, start), Box3_Translate(bounds, end));

  for (int32_t i = 0; i < module.numBarriers; i++) {
    const NavBarrier *barrier = &module.barriers[i];

    if (!Box3_Intersects(swept, barrier->bounds) || (barrier->gate && barrier->group == module.openGate)) {
      continue;
    }

    CollisionTrace tr = module.params->BoxTrace(Vec3_Subtract(start, barrier->origin),
                                                Vec3_Subtract(end, barrier->origin),
                                                bounds, barrier->headNode, NAV_MASK);
    tr.end = Vec3_Add(tr.end, barrier->origin);

    if (tr.allSolid || tr.fraction < trace.fraction) {
      const bool startSolid = trace.startSolid;
      trace = tr;
      trace.startSolid |= startSolid;
      module.traceBarrier = i;
    } else if (tr.startSolid) {
      trace.startSolid = true;
    }
  }

  return trace;
}

/**
 * @return The leaf of the world's BSP that contains `point`.
 */
static const CollisionLeaf *Nav_PointLeaf(const CollisionBsp *bsp, const Vec3 point) {

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
static int32_t Nav_PointContents(const Vec3 point) {

  const CollisionBsp *bsp = module.params->bsp;
  const CollisionLeaf *leaf = Nav_PointLeaf(bsp, point);

  if (leaf->numLeafBrushes == 0) {
    return leaf->contents;
  }

  int32_t contents = 0;

  for (int32_t i = 0; i < leaf->numLeafBrushes; i++) {
    const CollisionBrush *brush = &bsp->brushes[bsp->leafBrushes[leaf->firstLeafBrush + i]];
    if (module.params->PointInsideBrush(point, brush)) {
      contents |= brush->contents;
    }
  }

  return contents;
}

/**
 * @return True if the bounds touch a `trigger_hurt` or a `trigger_push`.
 */
static bool Nav_TouchesTrigger(const Box3 bounds) {

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
static Box3 Nav_Bounds(const bool crouch) {
  return crouch ? PM_CROUCHED_BOUNDS : PM_BOUNDS;
}

/**
 * @brief Rests the bounding box on the floor beneath `start`, as Pmove would.
 * @return True if the box fits at `start` and lands on a walkable floor within `distance`.
 */
static bool Nav_Rest(const Vec3 start, const float distance, const Box3 bounds, Vec3 *origin) {

  const CollisionTrace trace = Nav_Trace(start, Vec3_Subtract(start, Vec3_Scale(Vec3_Up(), distance)), bounds);

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
static NavSample Nav_SampleFloor(const Vec3 point, const Box3 footprint, NavNode *node) {

  if (Nav_PointContents(point) & NAV_MASK) {
    return NAV_SAMPLE_NONE;
  }

  const Vec3 end = Vec3_Subtract(point, Vec3_Scale(Vec3_Up(), NAV_CELL_HEIGHT));
  const CollisionTrace floor = Nav_Trace(point, end, footprint);

  if (floor.startSolid || floor.fraction == 1.f) {
    return NAV_SAMPLE_NONE;
  }

  if (floor.plane.normal.z < PM_STEP_NORMAL) {
    return NAV_SAMPLE_NONE;
  }

  const int32_t barrier = module.traceBarrier;

  const Vec3 normal = floor.plane.normal;
  const float lift = PM_BOUNDS.maxs.x * (fabsf(normal.x) + fabsf(normal.y)) / normal.z + 1.f;
  const float height = -PM_BOUNDS.mins.z;

  const Vec3 start = Vec3_Add(floor.end, Vec3_Scale(Vec3_Up(), height + lift));

  Vec3 origin;
  if (Nav_Rest(start, lift + 1.f, PM_BOUNDS, &origin)) {
    node->crouch = false;
  } else if (Nav_Rest(start, lift + 1.f, PM_CROUCHED_BOUNDS, &origin)) {
    node->crouch = true;
  } else {
    return NAV_SAMPLE_NO_FIT;
  }

  if (Nav_PointContents(origin) & CONTENTS_MASK_SOLID) {
    return NAV_SAMPLE_NO_FIT;
  }

  if (Nav_TouchesTrigger(Box3_Translate(Nav_Bounds(node->crouch), origin))) {
    return NAV_SAMPLE_TRIGGER;
  }

  node->position = origin;
  node->contents = Nav_PointContents(origin) & CONTENTS_MASK_LIQUID;

  if (barrier != -1 && module.barriers[barrier].moves) {
    node->model = module.barriers[barrier].model;
    node->local = Vec3_Subtract(origin, module.barriers[barrier].origin);
  }

  return NAV_SAMPLE_NODE;
}

/**
 * @brief Adds the node to the graph, in the given cell.
 */
static void Nav_AddNode(const int32_t cell, const NavNode *node) {

  NavGraph *graph = module.graph;

  if (graph->numNodes == module.capacity) {
    module.capacity = Maxi(1024, module.capacity * 2);
    graph->nodes = realloc(graph->nodes, sizeof(NavNode) * module.capacity);
    if (!graph->nodes) {
      module.params->Error(__func__, "Failed to allocate %d navigation nodes\n", module.capacity);
    }
  }

  graph->cellNodes[cell] = graph->numNodes;
  graph->nodes[graph->numNodes++] = *node;
}

/**
 * @brief Samples the floor of one cell, at its center first and then at small offsets, and adds
 * its node to the graph. Where those find no floor, points `NAV_FINE_STEP` apart across the
 * cell sample, to find a beam or a ledge between them, so that a node stands over its floor where it
 * can, and a bot does not steer onto the lip of a ledge. Last, the player's footprint samples at the
 * coarse points, for a floor too narrow for any point. A cell whose whole area finds no floor is
 * skipped. A point where a player only fits crouched is kept until every point is sampled, because
 * a player stands at another point of the cell, away from a wall's trim, more often than not.
 * @return The outcome of the sample.
 */
static NavSample Nav_SampleCell(const int32_t cell, bool *offset) {

  const Vec3 center = Vec3_Add(Nav_CellOrigin(Nav_CellCoordinate(cell)),
                               MakeVec3(0.f, 0.f, NAV_SAMPLE_OFFSET));

  const float half = NAV_CELL_SIZE * .5f;
  const Box3 area = MakeBox3(MakeVec3(-half, -half, 0.f), MakeVec3(half, half, 0.f));
  const CollisionTrace any = Nav_Trace(center, Vec3_Subtract(center, Vec3_Scale(Vec3_Up(), NAV_CELL_HEIGHT)), area);

  if (!any.startSolid && any.fraction == 1.f) {
    return NAV_SAMPLE_NONE;
  }

  const int32_t steps = (int32_t) (NAV_CELL_SIZE / NAV_FINE_STEP);
  const int32_t numFine = (int32_t) lengthof(offsets) + steps * steps;
  const int32_t numPoints = numFine + (int32_t) lengthof(offsets);

  const Box3 footprint = MakeBox3(MakeVec3(PM_BOUNDS.mins.x, PM_BOUNDS.mins.y, 0.f),
                                  MakeVec3(PM_BOUNDS.maxs.x, PM_BOUNDS.maxs.y, 0.f));

  NavSample sample = NAV_SAMPLE_NONE;

  NavNode crouched;
  int32_t crouchedPoint = -1;

  for (int32_t i = 0; i < numPoints; i++) {

    Vec2 delta;
    if (i < (int32_t) lengthof(offsets)) {
      delta = Vec2_Scale(offsets[i], NAV_CELL_SIZE * .25f);
    } else if (i < numFine) {
      const int32_t j = i - (int32_t) lengthof(offsets);
      delta = MakeVec2((j % steps + .5f) * NAV_FINE_STEP - half, (j / steps + .5f) * NAV_FINE_STEP - half);
    } else {
      delta = Vec2_Scale(offsets[i - numFine], NAV_CELL_SIZE * .25f);
    }

    const Vec3 point = Vec3_Add(center, MakeVec3(delta.x, delta.y, 0.f));

    NavNode node = { .cell = cell };

    const NavSample result = Nav_SampleFloor(point, i < numFine ? Box3_Zero() : footprint, &node);
    if (result == NAV_SAMPLE_NODE) {

      if (node.crouch) {
        if (crouchedPoint == -1) {
          crouched = node;
          crouchedPoint = i;
        }
        continue;
      }

      Nav_AddNode(cell, &node);

      *offset = i > 0;
      return NAV_SAMPLE_NODE;
    }

    if (result == NAV_SAMPLE_NO_FIT || result == NAV_SAMPLE_TRIGGER) {
      sample = Maxi(sample, result);
    }
  }

  if (crouchedPoint != -1) {
    Nav_AddNode(cell, &crouched);

    *offset = crouchedPoint > 0;
    return NAV_SAMPLE_NODE;
  }

  return sample;
}

/**
 * @brief Adds a swim node in a cell that has no floor, at its center, or else at the first of the
 * offsets of `offsets` where the player fits, as beside the edge of a ledge over the liquid: where
 * the point is in water, lava or slime, a player fits, and the player touches no hurt or push
 * trigger, so that a bot swims through the liquid rather than along its floor, and a bot that falls
 * into deep lava or slime has nodes near its surface by which to leave.
 * @return True if the node was added.
 */
static bool Nav_SampleLiquid(const int32_t cell) {

  const Vec3 center = Nav_CellOrigin(Nav_CellCoordinate(cell));

  for (size_t i = 0; i < lengthof(offsets); i++) {

    const Vec2 delta = Vec2_Scale(offsets[i], NAV_CELL_SIZE * .25f);
    const Vec3 point = Vec3_Add(center, MakeVec3(delta.x, delta.y, 0.f));
    const int32_t contents = Nav_PointContents(point);

    if (!(contents & CONTENTS_MASK_LIQUID)) {
      continue;
    }

    const CollisionTrace trace = Nav_Trace(point, point, PM_BOUNDS);
    if (trace.startSolid || trace.allSolid) {
      continue;
    }

    if (Nav_TouchesTrigger(Box3_Translate(PM_BOUNDS, point))) {
      continue;
    }

    const NavNode node = {
      .position = point,
      .cell = cell,
      .contents = contents & CONTENTS_MASK_LIQUID,
      .swim = true
    };

    Nav_AddNode(cell, &node);
    return true;
  }

  return false;
}

/**
 * @return True if the last `Nav_Trace` hit a model that moves first, so that a floor that it found
 * is only where the model spawned. A link between nodes that do not move MUST NOT stand on it.
 */
static bool Nav_TraceHitMover(void) {
  return module.traceBarrier != -1 && module.barriers[module.traceBarrier].moves;
}

/**
 * @brief Moves the bounding box toward `target`, as Pmove would: up by `height`, across, and down
 * onto the floor. If the raised box is blocked, as under the edge of a low ceiling, it moves across
 * at the height of `from`, as Pmove also tries.
 * @return True if the box arrives on a walkable floor.
 */
static bool Nav_StepMove(const Vec3 from, const Vec3 target, const Box3 bounds, const float height, Vec3 *to) {

  const CollisionTrace up = Nav_Trace(from, Vec3_Add(from, Vec3_Scale(Vec3_Up(), height)), bounds);
  if (up.startSolid) {
    return false;
  }

  const Vec3 across = MakeVec3(target.x, target.y, up.end.z);
  const CollisionTrace over = Nav_Trace(up.end, across, bounds);
  if (over.fraction == 1.f) {
    return Nav_Rest(over.end, height + PM_STEP_HEIGHT + 1.f, bounds, to);
  }

  const CollisionTrace flat = Nav_Trace(from, MakeVec3(target.x, target.y, from.z), bounds);
  if (flat.startSolid || flat.fraction < 1.f) {
    return false;
  }

  return Nav_Rest(flat.end, PM_STEP_HEIGHT + 1.f, bounds, to);
}

/**
 * @return The travel time for the given distance and speed, in hundredths of a second.
 */
static int32_t Nav_TravelTime(const float distance, const float speed) {
  return Maxi(1, (int32_t) ceilf(distance / speed * 100.f));
}

/**
 * @return The time to fall the given height, in hundredths of a second, or 0 with no gravity or
 * with gravity that lifts.
 */
static int32_t Nav_FallTime(const float height) {

  if (module.gravity <= 0.f || height <= 0.f) {
    return 0;
  }

  return (int32_t) ceilf(sqrtf(2.f * height / module.gravity) * 100.f);
}

/**
 * @return The node closest to `origin`, standing on the same floor within
 * `NAV_LANDING_DISTANCE`, and at least `reach` along `dir` from `start`, or -1.
 * @remarks A drop's landing node MUST lie past the edge that the walk leaves, because a bot steers
 * toward it to walk off. A node short of the edge, such as one under the floor that the walk starts
 * on, leads the bot nowhere. For the same reason, the player's footprint at the landing node MUST
 * be able to rise straight up to the height of `start`: a node beside a narrow hole, under the floor
 * around it, lies past the edge of the hole but under the floor, where a bot that steers to it never
 * falls. The footprint is flat, at the height of the player's origin, so that a crouch node, or a
 * ceiling that only a player's head would touch, does not fail the test.
 */
static int32_t Nav_NodeForOrigin(const Vec3 origin, const Vec3 start, const Vec3 dir, const float reach) {

  const NavGraph *graph = module.graph;

  const Vec3 floor = Vec3_Subtract(origin, Vec3_Scale(Vec3_Up(), -PM_BOUNDS.mins.z - NAV_CELL_HEIGHT * .5f));
  const Vec3i xyz = Nav_PointCoordinate(floor);

  const Box3 footprint = MakeBox3(MakeVec3(PM_BOUNDS.mins.x, PM_BOUNDS.mins.y, 0.f),
                                  MakeVec3(PM_BOUNDS.maxs.x, PM_BOUNDS.maxs.y, 0.f));

  int32_t best = -1;
  float bestDistance = NAV_LANDING_DISTANCE * NAV_LANDING_DISTANCE;

  for (int32_t dz = -1; dz <= 1; dz++) {
    for (int32_t dy = -1; dy <= 1; dy++) {
      for (int32_t dx = -1; dx <= 1; dx++) {

        const int32_t cell = Nav_CellIndex(MakeVec3i(xyz.x + dx, xyz.y + dy, xyz.z + dz));
        if (cell == -1 || graph->cellNodes[cell] == -1) {
          continue;
        }

        const NavNode *node = &graph->nodes[graph->cellNodes[cell]];
        if (node->model || node->swim || fabsf(node->position.z - origin.z) > PM_STEP_HEIGHT) {
          continue;
        }

        const Vec3 along = Vec3_Subtract(node->position, start);
        if (along.x * dir.x + along.y * dir.y < reach) {
          continue;
        }

        const float distance = Vec3_DistanceSquared(node->position, origin);
        if (distance >= bestDistance) {
          continue;
        }

        const Vec3 above = MakeVec3(node->position.x, node->position.y, start.z);
        const CollisionTrace rise = Nav_Trace(node->position, above, footprint);
        if (rise.startSolid || rise.fraction < 1.f) {
          continue;
        }

        bestDistance = distance;
        best = graph->cellNodes[cell];
      }
    }
  }

  return best;
}

/**
 * @brief Adds a link to `links` that leads to `node`, unless one exists.
 */
static void Nav_AddLink(NavLink *links, int32_t *numLinks, const int32_t node,
                        const NavTravelType type, const int32_t time, const int32_t gate) {

  for (int32_t i = 0; i < *numLinks; i++) {
    if (links[i].node == node) {
      return;
    }
  }

  if (*numLinks == NAV_MAX_NODE_LINKS) {
    module.params->Error(__func__, "A navigation node has more than %d links\n", NAV_MAX_NODE_LINKS);
  }

  links[(*numLinks)++] = (NavLink) {
    .node = node,
    .travelType = type,
    .travelTime = time,
    .gate = gate
  };
}

/**
 * @brief Adds a walk, or a crouch, from `node` to `other`, at the speed of the water if either is in
 * it, through the doorway of the door with inline model `gate`, or 0.
 */
static void Nav_AddWalk(NavLink *links, int32_t *numLinks, const NavNode *node, const NavNode *other,
                        const int32_t target, const float distance, const int32_t gate) {

  const bool crouch = node->crouch || other->crouch;
  const bool wade = (node->contents | other->contents) != 0;
  const NavTravelType type = crouch ? NAV_TRAVEL_CROUCH : NAV_TRAVEL_WALK;
  const float speed = wade ? PM_SPEED_WATER : crouch ? PM_SPEED_DUCKED : PM_SPEED_RUN;

  Nav_AddLink(links, numLinks, target, type, Nav_TravelTime(distance, speed), gate);
}

/**
 * @brief Simulates a walk from node `a` to node `b`.
 * @return True if the walk arrives at `b`.
 */
static bool Nav_Walk(const NavNode *a, const NavNode *b, float *distance) {

  const Box3 bounds = Nav_Bounds(a->crouch || b->crouch);

  const Vec3 delta = Vec3_Subtract(b->position, a->position);
  const float length = Vec2_Length(Vec3_XY(delta));
  const int32_t steps = Maxi(1, (int32_t) ceilf(length / NAV_WALK_STEP));

  Vec3 origin = a->position;
  *distance = 0.f;

  for (int32_t i = 1; i <= steps; i++) {

    const Vec3 target = Vec3_Mix(a->position, b->position, i / (float) steps);

    Vec3 next;
    if (!Nav_StepMove(origin, target, bounds, PM_STEP_HEIGHT, &next)) {
      return false;
    }

    if (!a->model && Nav_TraceHitMover()) {
      return false;
    }

    if (Nav_TouchesTrigger(Box3_Translate(bounds, next))) {
      return false;
    }

    *distance += Vec3_Distance(origin, next);
    origin = next;
  }

  return fabsf(origin.z - b->position.z) <= 1.f;
}

/**
 * @return True if barrier `i` is a door that touches the box, and the first barrier of its group
 * that does, so that the gate search opens each group once.
 */
static bool Nav_IsGateToTry(const int32_t i, const Box3 swept) {

  const NavBarrier *barrier = &module.barriers[i];

  if (!barrier->gate || !Box3_Intersects(swept, barrier->bounds)) {
    return false;
  }

  for (int32_t j = 0; j < i; j++) {
    const NavBarrier *other = &module.barriers[j];

    if (other->gate && other->group == barrier->group && Box3_Intersects(swept, other->bounds)) {
      return false;
    }
  }

  return true;
}

/**
 * @brief Simulates a walk from node `a` to node `b` through the doorway of a door, for a walk that
 * `Nav_Walk` found blocked: with the doors of each group that the walk's box touches left out of
 * the traces in turn, so that the link opens with the doors that block it, which move together.
 * @return The inline model of a door of that group, or 0 if no one group blocks the walk.
 */
static int32_t Nav_GatedWalk(const NavNode *a, const NavNode *b, float *distance) {

  if (a->model || b->model) {
    return 0;
  }

  const Box3 bounds = Nav_Bounds(a->crouch || b->crouch);
  const Box3 swept = Box3_Union(Box3_Translate(bounds, a->position), Box3_Translate(bounds, b->position));

  for (int32_t i = 0; i < module.numBarriers; i++) {
    if (!Nav_IsGateToTry(i, swept)) {
      continue;
    }

    module.openGate = module.barriers[i].group;
    const bool walked = Nav_Walk(a, b, distance);
    module.openGate = -1;

    if (walked) {
      return module.barriers[i].model;
    }
  }

  return 0;
}

/**
 * @brief Simulates a jump straight up from node `a` onto the ledge of node `b`.
 * @return True if the jump clears the ledge and lands at `b`, and the box touches no trigger as it
 * rises, up to the barrier height or a ceiling, and as it moves across to the ledge.
 */
static bool Nav_BarrierJump(const NavNode *a, const NavNode *b, float *distance) {

  if (a->crouch || b->crouch) {
    return false;
  }

  const float rise = b->position.z - a->position.z;
  if (rise <= PM_STEP_HEIGHT || rise > module.barrierHeight) {
    return false;
  }

  Vec3 origin;
  if (!Nav_StepMove(a->position, b->position, PM_BOUNDS, module.barrierHeight, &origin)) {
    return false;
  }

  if (!a->model && Nav_TraceHitMover()) {
    return false;
  }

  const Vec3 top = Nav_Trace(a->position, Vec3_Add(a->position, Vec3_Scale(Vec3_Up(), module.barrierHeight)), PM_BOUNDS).end;
  if (Nav_TouchesTrigger(Box3_Union(Box3_Translate(PM_BOUNDS, a->position), Box3_Translate(PM_BOUNDS, top)))) {
    return false;
  }

  *distance = Vec2_Distance(Vec3_XY(a->position), Vec3_XY(b->position));

  const Vec3 across = MakeVec3(b->position.x, b->position.y, top.z);
  const int32_t steps = Maxi(1, (int32_t) ceilf(*distance / NAV_WALK_STEP));

  for (int32_t i = 1; i <= steps; i++) {
    if (Nav_TouchesTrigger(Box3_Translate(PM_BOUNDS, Vec3_Mix(top, across, i / (float) steps)))) {
      return false;
    }
  }

  return fabsf(origin.z - b->position.z) <= 1.f;
}

/**
 * @brief Simulates a walk from `node` in `direction`, off a ledge, and the fall that follows.
 * @return The node that the fall lands at, or -1.
 */
static int32_t Nav_Drop(const NavNode *node, const Vec3i direction, float *distance, float *height) {

  const Box3 bounds = Nav_Bounds(node->crouch);

  const Vec3 dir = Vec3_Normalize(MakeVec3(direction.x, direction.y, 0.f));

  Vec3 origin = node->position;
  *distance = 0.f;

  for (float d = NAV_WALK_STEP; d <= NAV_LEDGE_DISTANCE; d += NAV_WALK_STEP) {

    const Vec3 target = Vec3_Add(node->position, Vec3_Scale(dir, d));

    const CollisionTrace across = Nav_Trace(origin, MakeVec3(target.x, target.y, origin.z), bounds);
    if (across.fraction < 1.f) {
      return -1;
    }

    *distance += Vec3_Distance(origin, across.end);
    origin = across.end;

    if (Nav_TouchesTrigger(Box3_Translate(bounds, origin))) {
      return -1;
    }

    Vec3 floor;
    if (Nav_Rest(origin, PM_STEP_HEIGHT + 1.f, bounds, &floor)) {
      if (Nav_TraceHitMover()) {
        return -1;
      }
      origin = floor;
      continue;
    }

    const Vec3 end = Vec3_Subtract(origin, Vec3_Scale(Vec3_Up(), NAV_MAX_DROP));
    const CollisionTrace fall = Nav_Trace(origin, end, bounds);

    if (fall.startSolid || fall.fraction == 1.f || fall.plane.normal.z < PM_STEP_NORMAL || Nav_TraceHitMover()) {
      return -1;
    }

    *height = node->position.z - fall.end.z;
    if (*height <= PM_STEP_HEIGHT) {
      return -1;
    }

    const Vec3 edge = Vec3_Subtract(origin, node->position);
    return Nav_NodeForOrigin(fall.end, node->position, dir, edge.x * dir.x + edge.y * dir.y - NAV_WALK_STEP);
  }

  return -1;
}

/**
 * @brief Finds the node that a walk off a ledge from `node` in `direction` lands at through the
 * doorway of a door, for a drop that `Nav_Drop` found blocked. The doors of each group within the
 * drop's reach are left out of the traces in turn.
 * @param gate Receives the inline model of a door of the group that the drop passes.
 * @return The node that the drop lands at, or -1.
 */
static int32_t Nav_GatedDrop(const NavNode *node, const Vec3i direction, float *distance, float *height, int32_t *gate) {

  Box3 bounds = Nav_Bounds(node->crouch);
  bounds.mins.z -= NAV_MAX_DROP;

  const Vec3 dir = Vec3_Normalize(MakeVec3(direction.x, direction.y, 0.f));
  const Vec3 reach = Vec3_Add(node->position, Vec3_Scale(dir, NAV_LEDGE_DISTANCE));
  const Box3 swept = Box3_Union(Box3_Translate(bounds, node->position), Box3_Translate(bounds, reach));

  for (int32_t i = 0; i < module.numBarriers; i++) {
    if (!Nav_IsGateToTry(i, swept)) {
      continue;
    }

    module.openGate = module.barriers[i].group;
    const int32_t target = Nav_Drop(node, direction, distance, height);
    module.openGate = -1;

    if (target != -1) {
      *gate = module.barriers[i].model;
      return target;
    }
  }

  return -1;
}

/**
 * @return True if a walk from the node in any of the 8 directions reaches, within
 * `NAV_EDGE_DISTANCE` and before a wall, an edge with no floor below it within `NAV_MAX_DROP`. A
 * node on a model that moves, or a swim node, is not tested.
 */
static bool Nav_IsEdge(const NavNode *node) {

  if (node->model || node->swim) {
    return false;
  }

  const Box3 bounds = Nav_Bounds(node->crouch);

  for (size_t i = 0; i < lengthof(directions); i++) {

    const Vec3 dir = Vec3_Normalize(MakeVec3(directions[i].x, directions[i].y, 0.f));

    Vec3 origin = node->position;

    for (float d = NAV_WALK_STEP; d <= NAV_EDGE_DISTANCE; d += NAV_WALK_STEP) {

      const Vec3 target = Vec3_Add(node->position, Vec3_Scale(dir, d));

      const CollisionTrace across = Nav_Trace(origin, MakeVec3(target.x, target.y, origin.z), bounds);
      if (across.startSolid || across.fraction < 1.f) {
        break;
      }

      origin = across.end;

      Vec3 floor;
      if (Nav_Rest(origin, PM_STEP_HEIGHT + 1.f, bounds, &floor)) {
        origin = floor;
        continue;
      }

      const CollisionTrace fall = Nav_Trace(origin, Vec3_Subtract(origin, Vec3_Scale(Vec3_Up(), NAV_MAX_DROP)), bounds);
      if (!fall.startSolid && fall.fraction == 1.f) {
        return true;
      }

      break;
    }
  }

  return false;
}

/**
 * @brief Simulates a running jump from node `a` across a gap to node `b`. The player walks straight
 * toward `b` to the edge of the floor, and jumps `NAV_JUMP_TAKEOFF_MARGIN` before it: the lowest
 * jump of all movements, at `NAV_JUMP_SPEED_FRACTION` of the run speed, or
 * `NAV_GATED_JUMP_SPEED_FRACTION` for a jump through a door's doorway or over its place. The arc MUST be
 * `NAV_JUMP_CLEARANCE` above `b` when it arrives over it.
 * @param seconds The time of the run to the takeoff point, and of the flight until the player lands
 * at `b`.
 * @return True if the floor ends on the way, with no wall or high step before it, and the arc from
 * the takeoff clears the world and every trigger, and lands at `b` with floor for
 * `NAV_JUMP_LANDING_DISTANCE` past it, up to a wall.
 */
static bool Nav_JumpArc(const NavNode *a, const NavNode *b, float *seconds) {

  float distance;
  const Vec3 dir = Vec3_NormalizeLength(MakeVec3(b->position.x - a->position.x, b->position.y - a->position.y, 0.f), &distance);

  Vec3 steps[NAV_MAX_JUMP_STEPS];
  int32_t numSteps = 0;

  steps[numSteps++] = a->position;

  while (true) {
    if (numSteps * NAV_WALK_STEP >= distance || numSteps == lengthof(steps)) {
      return false;
    }

    const Vec3 from = steps[numSteps - 1];

    const CollisionTrace up = Nav_Trace(from, Vec3_Add(from, Vec3_Scale(Vec3_Up(), PM_STEP_HEIGHT)), PM_BOUNDS);
    if (up.startSolid) {
      return false;
    }

    const Vec3 target = Vec3_Add(up.end, Vec3_Scale(dir, NAV_WALK_STEP));
    const CollisionTrace over = Nav_Trace(up.end, target, PM_BOUNDS);
    if (over.fraction < 1.f) {
      return false;
    }

    Vec3 next;
    if (!Nav_Rest(over.end, PM_STEP_HEIGHT * 2.f + 1.f, PM_BOUNDS, &next)) {
      break;
    }

    if (Nav_TraceHitMover()) {
      return false;
    }

    steps[numSteps++] = next;
  }

  const int32_t index = Maxi(0, numSteps - 1 - (int32_t) (NAV_JUMP_TAKEOFF_MARGIN / NAV_WALK_STEP));
  const Vec3 takeoff = steps[index];
  const float remaining = distance - index * NAV_WALK_STEP;
  const float speed = PM_SPEED_RUN * (module.gatedSpeed ? NAV_GATED_JUMP_SPEED_FRACTION : NAV_JUMP_SPEED_FRACTION);
  const float up = sqrtf(2.f * module.gravity * (module.barrierHeight + NAV_BARRIER_MARGIN));

  Vec3 origin = takeoff;

  for (float t = NAV_JUMP_STEP; ; t += NAV_JUMP_STEP) {

    const float across = Minf(speed * t, remaining);
    const float rise = up * t - .5f * module.gravity * t * t;

    const Vec3 next = Vec3_Add(Vec3_Add(takeoff, Vec3_Scale(dir, across)), MakeVec3(0.f, 0.f, rise));

    const CollisionTrace trace = Nav_Trace(origin, next, PM_BOUNDS);
    if (trace.startSolid || trace.fraction < 1.f) {
      return false;
    }

    if (Nav_TouchesTrigger(Box3_Translate(PM_BOUNDS, next))) {
      return false;
    }

    origin = next;

    if (t > up / module.gravity && origin.z < b->position.z + NAV_JUMP_CLEARANCE) {
      return false;
    }

    if (across >= remaining) {
      if (origin.z < b->position.z + NAV_JUMP_CLEARANCE) {
        return false;
      }

      Vec3 floor;
      if (!Nav_Rest(origin, origin.z - b->position.z + PM_STEP_HEIGHT + 1.f, PM_BOUNDS, &floor) || Nav_TraceHitMover()) {
        return false;
      }

      if (fabsf(floor.z - b->position.z) > 1.f) {
        return false;
      }

      Vec3 landing = floor;
      for (float d = NAV_WALK_STEP; d <= NAV_JUMP_LANDING_DISTANCE; d += NAV_WALK_STEP) {

        const CollisionTrace slide = Nav_Trace(landing, Vec3_Add(landing, Vec3_Scale(dir, NAV_WALK_STEP)), PM_BOUNDS);
        if (slide.fraction < 1.f) {
          break;
        }

        if (!Nav_Rest(slide.end, PM_STEP_HEIGHT + 1.f, PM_BOUNDS, &landing)) {
          return false;
        }
      }

      *seconds = index * NAV_WALK_STEP / PM_SPEED_RUN + t + sqrtf(2.f * Maxf(0.f, origin.z - floor.z) / module.gravity);
      return true;
    }
  }
}

/**
 * @brief Finds the node that a running jump from `node` in `direction` lands at: the nearest one,
 * 1 to `NAV_MAX_JUMP_CELLS` cells away, from two cells below up to the barrier jump height. A
 * level without gravity has no jumps, and no jump starts or lands at a crouch node or a node in
 * liquid.
 * @param seconds The time of the jump.
 * @return The node that the jump lands at, or -1.
 */
static int32_t Nav_Jump(const NavNode *node, const Vec3i direction, float *seconds) {

  const NavGraph *graph = module.graph;

  if (module.gravity <= 0.f || node->crouch || (node->contents & CONTENTS_MASK_LIQUID)) {
    return -1;
  }

  const Vec3i xyz = Nav_CellCoordinate(node->cell);
  const int32_t rise = (int32_t) ceilf(module.barrierHeight / NAV_CELL_HEIGHT);

  for (int32_t k = 1; k <= NAV_MAX_JUMP_CELLS; k++) {
    for (int32_t dz = -2; dz <= rise; dz++) {

      const int32_t cell = Nav_CellIndex(MakeVec3i(xyz.x + direction.x * k, xyz.y + direction.y * k, xyz.z + dz));
      if (cell == -1 || graph->cellNodes[cell] == -1) {
        continue;
      }

      const NavNode *other = &graph->nodes[graph->cellNodes[cell]];
      if (other == node || other->model || other->crouch || (other->contents & CONTENTS_MASK_LIQUID)) {
        continue;
      }

      if (Nav_JumpArc(node, other, seconds)) {
        return graph->cellNodes[cell];
      }
    }
  }

  return -1;
}

/**
 * @brief Finds the node that a running jump from `node` in `direction` lands at through the doorway
 * of a door, or over the place of a door that spawns open, for a jump that `Nav_Jump` found
 * blocked, at `NAV_GATED_JUMP_SPEED_FRACTION`. The doors of each group within the jump's reach,
 * from two steps below it, where the top of a door is its floor, are left out of the traces in
 * turn. The jump is gated only by a group without which it lands, and with which it does not, even
 * at that speed.
 * @param gate The inline model of the door that the jump passes.
 * @return The node that the jump lands at, or -1.
 */
static int32_t Nav_GatedJump(const NavNode *node, const Vec3i direction, float *seconds, int32_t *gate) {

  Box3 bounds = Nav_Bounds(false);
  bounds.mins.z -= PM_STEP_HEIGHT * 2.f;
  bounds.maxs.z += module.barrierHeight;

  const Vec3 reach = Vec3_Add(node->position, Vec3_Scale(MakeVec3(direction.x, direction.y, 0.f), NAV_CELL_SIZE * NAV_MAX_JUMP_CELLS));
  const Box3 swept = Box3_Union(Box3_Translate(bounds, node->position), Box3_Translate(bounds, reach));

  int32_t landing = -1;
  int32_t closed = -2;

  module.gatedSpeed = true;

  for (int32_t i = 0; i < module.numBarriers && landing == -1; i++) {
    if (!Nav_IsGateToTry(i, swept)) {
      continue;
    }

    module.openGate = module.barriers[i].group;
    landing = Nav_Jump(node, direction, seconds);
    module.openGate = -1;

    if (landing == -1) {
      continue;
    }

    if (closed == -2) {
      float time;
      closed = Nav_Jump(node, direction, &time);
    }

    if (closed != -1) {
      landing = -1;
      break;
    }

    *gate = module.barriers[i].model;
  }

  module.gatedSpeed = false;

  return landing;
}

/**
 * @return True if the point is in water, lava or slime, which a player can swim through.
 */
static bool Nav_InLiquid(const Vec3 point) {
  return (Nav_PointContents(point) & CONTENTS_MASK_LIQUID) != 0;
}

/**
 * @brief Finds where a player who swims straight up from `from` floats at the surface.
 * @return True if the player reaches the surface within `NAV_MAX_SWIM_DEPTH`.
 */
static bool Nav_SwimSurface(const Vec3 from, Vec3 *surface) {

  for (float z = NAV_SWIM_STEP; z <= NAV_MAX_SWIM_DEPTH; z += NAV_SWIM_STEP) {

    const Vec3 point = Vec3_Add(from, MakeVec3(0.f, 0.f, z));
    if (Nav_InLiquid(point)) {
      continue;
    }

    *surface = MakeVec3(from.x, from.y, Maxf(from.z, point.z - NAV_SWIM_FLOAT));

    const CollisionTrace trace = Nav_Trace(from, *surface, PM_BOUNDS);
    return !trace.startSolid && trace.fraction == 1.f;
  }

  return false;
}

/**
 * @return True if a link from `node` to `other` would enter lava or slime from outside it. No bot
 * drops, jumps or swims into lava or slime, so the build makes no such link.
 */
static bool Nav_EntersHazard(const NavNode *node, const NavNode *other) {

  const int32_t hazard = CONTENTS_LAVA | CONTENTS_SLIME;

  return (other->contents & hazard) && !(node->contents & hazard);
}

/**
 * @brief Links a node in liquid by swimming: to the nodes in liquid around it that the player can
 * swim straight to, and to the nodes on the ledges around the surface above it that a water jump
 * climbs onto. A node in water does not swim into lava or slime (`Nav_EntersHazard`).
 */
static void Nav_LinkSwims(const int32_t index, NavLink *links, int32_t *numLinks) {

  const NavGraph *graph = module.graph;
  const NavNode *node = &graph->nodes[index];

  const Vec3i xyz = Nav_CellCoordinate(node->cell);

  for (int32_t dz = -2; dz <= 2; dz++) {
    for (int32_t dy = -1; dy <= 1; dy++) {
      for (int32_t dx = -1; dx <= 1; dx++) {

        const int32_t cell = Nav_CellIndex(MakeVec3i(xyz.x + dx, xyz.y + dy, xyz.z + dz));
        if (cell == -1 || graph->cellNodes[cell] == -1 || graph->cellNodes[cell] == index) {
          continue;
        }

        const NavNode *other = &graph->nodes[graph->cellNodes[cell]];
        if (!(other->contents & CONTENTS_MASK_LIQUID) || other->model || other->crouch || node->crouch) {
          continue;
        }

        if (Nav_EntersHazard(node, other)) {
          continue;
        }

        const Vec3 middle = Vec3_Mix(node->position, other->position, .5f);
        if (!Nav_InLiquid(middle)) {
          continue;
        }

        const CollisionTrace trace = Nav_Trace(node->position, other->position, PM_BOUNDS);
        if (trace.startSolid || trace.fraction < 1.f) {
          continue;
        }

        const float distance = Vec3_Distance(node->position, other->position);
        Nav_AddLink(links, numLinks, graph->cellNodes[cell], NAV_TRAVEL_SWIM, Nav_TravelTime(distance, PM_SPEED_WATER), 0);
      }
    }
  }

  Vec3 surface;
  if (!Nav_SwimSurface(node->position, &surface)) {
    return;
  }

  const Vec3i top = Nav_PointCoordinate(surface);
  const int32_t rise = (int32_t) ceilf(module.waterJumpHeight / NAV_CELL_HEIGHT);

  for (int32_t dz = -1; dz <= rise; dz++) {
    for (int32_t dy = -1; dy <= 1; dy++) {
      for (int32_t dx = -1; dx <= 1; dx++) {

        const int32_t cell = Nav_CellIndex(MakeVec3i(top.x + dx, top.y + dy, top.z + dz));
        if (cell == -1 || graph->cellNodes[cell] == -1 || graph->cellNodes[cell] == index) {
          continue;
        }

        const NavNode *other = &graph->nodes[graph->cellNodes[cell]];
        if ((other->contents & CONTENTS_MASK_LIQUID) || other->model || other->crouch || other->position.z <= surface.z) {
          continue;
        }

        Vec3 origin;
        if (!Nav_StepMove(surface, other->position, PM_BOUNDS, module.waterJumpHeight, &origin)) {
          continue;
        }

        if (fabsf(origin.z - other->position.z) > 1.f) {
          continue;
        }

        const float distance = Vec3_Distance(node->position, surface) + Vec3_Distance(surface, other->position);
        Nav_AddLink(links, numLinks, graph->cellNodes[cell], NAV_TRAVEL_SWIM, Nav_TravelTime(distance, PM_SPEED_WATER), 0);
      }
    }
  }
}

/**
 * @return The node that does not move nearest `origin`, within `NAV_CELL_SIZE` across and `height`
 * up or down, which a player's box moves to from `origin` unblocked, or steps up onto, as onto the
 * top of the ladder brush itself, or -1.
 */
static int32_t Nav_LadderNode(const Vec3 origin, const float height) {

  const NavGraph *graph = module.graph;
  const Vec3i xyz = Nav_PointCoordinate(origin);
  const int32_t levels = (int32_t) ceilf(height / NAV_CELL_HEIGHT) + 1;

  int32_t best = -1;
  float bestDistance = FLT_MAX;

  for (int32_t dz = -levels; dz <= levels; dz++) {
    for (int32_t dy = -1; dy <= 1; dy++) {
      for (int32_t dx = -1; dx <= 1; dx++) {

        const int32_t cell = Nav_CellIndex(MakeVec3i(xyz.x + dx, xyz.y + dy, xyz.z + dz));
        if (cell == -1 || graph->cellNodes[cell] == -1) {
          continue;
        }

        const NavNode *node = &graph->nodes[graph->cellNodes[cell]];
        if (node->model || fabsf(node->position.z - origin.z) > height) {
          continue;
        }

        if (Vec2_Distance(Vec3_XY(node->position), Vec3_XY(origin)) > NAV_CELL_SIZE) {
          continue;
        }

        const float distance = Vec3_Distance(node->position, origin);
        if (distance >= bestDistance) {
          continue;
        }

        const Box3 bounds = Nav_Bounds(node->crouch);
        const CollisionTrace trace = Nav_Trace(origin, node->position, bounds);

        if (trace.startSolid || trace.fraction < 1.f) {
          Vec3 step;
          if (!Nav_StepMove(origin, node->position, bounds, PM_STEP_HEIGHT, &step) ||
              Vec3_Distance(step, node->position) > 1.f) {
            continue;
          }
        }

        bestDistance = distance;
        best = graph->cellNodes[cell];
      }
    }
  }

  return best;
}

/**
 * @return True if a player beside the ladder face with the outward `normal`, anywhere from `bottom`
 * up to `top`, each `NAV_FINE_STEP * 2` and at `top`, is on the ladder: the trace of
 * `Pm_CheckLadder` hits the ladder strictly before any other brush that a player collides with. A
 * trace tests a leaf's brushes only if the leaf's contents match its mask, and a leaf does not carry
 * `CONTENTS_LADDER`, so the ladder is traced as the player clip that it also is. The trace reports
 * only the first brush that it hits: a brush in front of the ladder hides it, and of two brushes on
 * the same plane, as the rungs that a ladder brush does not enclose, the one that it reports depends
 * on where the player stands, so that a player climbs there only at some places. Either sets
 * `module.ladderHidden`. A ladder brush that is also solid does not hide itself.
 */
static bool Nav_OnLadder(const Vec3 bottom, const Vec3 top, const Vec3 normal, const Box3 bounds) {

  for (float z = bottom.z; ; z += NAV_FINE_STEP * 2.f) {
    const Vec3 origin = MakeVec3(bottom.x, bottom.y, Minf(z, top.z));
    const Vec3 end = Vec3_Fmaf(origin, -NAV_LADDER_REACH, normal);

    const CollisionTrace ladder = module.params->BoxTrace(origin, end, bounds, 0, CONTENTS_LADDER | CONTENTS_PLAYER_CLIP);
    if (ladder.fraction == 1.f) {
      return false;
    }

    if (!(ladder.contents & CONTENTS_LADDER)) {
      module.ladderHidden = true;
      return false;
    }

    const CollisionTrace other = module.params->BoxTrace(origin, end, bounds, 0, NAV_MASK & ~CONTENTS_PLAYER_CLIP);
    if (other.fraction <= ladder.fraction && !(other.contents & CONTENTS_LADDER)) {
      module.ladderHidden = true;
      return false;
    }

    if (z >= top.z) {
      return true;
    }
  }
}

/**
 * @return True if a player walks `NAV_LADDER_RUN_UP` from `origin` in `dir` on floor, stepping up or
 * down at most `PM_STEP_HEIGHT`, with no wall, mover, trigger or liquid on the way.
 * @param end The player origin where the walk ends.
 */
static bool Nav_RunUp(const Vec3 origin, const Vec3 dir, Vec3 *end) {

  Vec3 from = origin;

  for (float d = 0.f; d < NAV_LADDER_RUN_UP; d += NAV_WALK_STEP) {

    const CollisionTrace up = Nav_Trace(from, Vec3_Add(from, Vec3_Scale(Vec3_Up(), PM_STEP_HEIGHT)), PM_BOUNDS);
    if (up.startSolid) {
      return false;
    }

    const CollisionTrace over = Nav_Trace(up.end, Vec3_Add(up.end, Vec3_Scale(dir, NAV_WALK_STEP)), PM_BOUNDS);
    if (over.fraction < 1.f) {
      return false;
    }

    if (!Nav_Rest(over.end, PM_STEP_HEIGHT * 2.f + 1.f, PM_BOUNDS, &from) || Nav_TraceHitMover()) {
      return false;
    }

    if (Nav_TouchesTrigger(Box3_Translate(PM_BOUNDS, from)) || (Nav_PointContents(from) & CONTENTS_MASK_LIQUID)) {
      return false;
    }
  }

  *end = from;
  return true;
}

/**
 * @brief Simulates a running jump from `node` onto the ladder beside `column`, whose outward normal
 * is `normal`. The player walks straight toward the column to the edge of the floor, and jumps
 * `NAV_JUMP_TAKEOFF_MARGIN` before it, as `Nav_JumpArc` does. The player grabs the ladder where the
 * arc arrives over the column, which MUST be from `low` up to `high`, where the column climbs.
 * @param grab The player origin where the player grabs the ladder.
 * @param runUp The player origin `NAV_LADDER_RUN_UP` behind the node on the line of the jump.
 * @param seconds The time of the run to the takeoff point, and of the flight to `grab`.
 * @return True if the node stands in front of the face, with `NAV_LADDER_RUN_UP` of floor behind it
 * on the line of the jump (`Nav_RunUp`), the floor ends on the way, with no wall or high step
 * before it, and the arc from the takeoff clears the world and every trigger, and stays above `low`
 * until it arrives over the column.
 */
static bool Nav_LadderJump(const NavNode *node, const Vec3 column, const Vec3 normal, const float low,
                           const float high, Vec3 *grab, Vec3 *runUp, float *seconds) {

  if (Vec2_Dot(Vec2_Subtract(Vec3_XY(node->position), Vec3_XY(column)), Vec3_XY(normal)) <= 0.f) {
    return false;
  }

  float distance;
  const Vec3 dir = Vec3_NormalizeLength(MakeVec3(column.x - node->position.x, column.y - node->position.y, 0.f), &distance);

  if (!Nav_RunUp(node->position, Vec3_Negate(dir), runUp)) {
    return false;
  }

  Vec3 steps[NAV_MAX_JUMP_STEPS];
  int32_t numSteps = 0;

  steps[numSteps++] = node->position;

  while (true) {
    if (numSteps * NAV_WALK_STEP >= distance || numSteps == lengthof(steps)) {
      return false;
    }

    const Vec3 from = steps[numSteps - 1];

    const CollisionTrace up = Nav_Trace(from, Vec3_Add(from, Vec3_Scale(Vec3_Up(), PM_STEP_HEIGHT)), PM_BOUNDS);
    if (up.startSolid) {
      return false;
    }

    const CollisionTrace over = Nav_Trace(up.end, Vec3_Add(up.end, Vec3_Scale(dir, NAV_WALK_STEP)), PM_BOUNDS);
    if (over.fraction < 1.f) {
      return false;
    }

    Vec3 next;
    if (!Nav_Rest(over.end, PM_STEP_HEIGHT * 2.f + 1.f, PM_BOUNDS, &next)) {
      break;
    }

    if (Nav_TraceHitMover()) {
      return false;
    }

    steps[numSteps++] = next;
  }

  const int32_t index = Maxi(0, numSteps - 1 - (int32_t) (NAV_JUMP_TAKEOFF_MARGIN / NAV_WALK_STEP));
  const Vec3 takeoff = steps[index];
  const float remaining = distance - index * NAV_WALK_STEP;
  const float speed = PM_SPEED_RUN * NAV_JUMP_SPEED_FRACTION;
  const float up = sqrtf(2.f * module.gravity * (module.barrierHeight + NAV_BARRIER_MARGIN));

  Vec3 origin = takeoff;

  for (float t = NAV_JUMP_STEP; ; t += NAV_JUMP_STEP) {

    const float across = Minf(speed * t, remaining);
    const float rise = up * t - .5f * module.gravity * t * t;

    const Vec3 next = Vec3_Add(Vec3_Add(takeoff, Vec3_Scale(dir, across)), MakeVec3(0.f, 0.f, rise));

    const CollisionTrace trace = Nav_Trace(origin, next, PM_BOUNDS);
    if (trace.startSolid || trace.fraction < 1.f) {
      return false;
    }

    if (Nav_TouchesTrigger(Box3_Translate(PM_BOUNDS, next))) {
      return false;
    }

    origin = next;

    if (t > up / module.gravity && origin.z < low) {
      return false;
    }

    if (across >= remaining) {
      if (origin.z < low || origin.z > high) {
        return false;
      }

      *grab = origin;
      *seconds = index * NAV_WALK_STEP / PM_SPEED_RUN + t;
      return true;
    }
  }
}

/**
 * @brief Adds the climb from node `a` to node `b`, unless the graph has it. A climb from the foot
 * of a ladder replaces a jump onto it from the same node.
 */
static void Nav_AddClimbLink(const int32_t a, const int32_t b, const int32_t time, const Vec3 column,
                             const Vec3 landing, const bool jump, const Vec3 runUp) {

  NavGraph *graph = module.graph;

  for (int32_t i = 0; i < graph->numClimbs; i++) {
    NavLadderClimb *climb = &graph->climbs[i];

    if (climb->bottom == a && climb->top == b) {
      if (climb->jump && !jump) {
        *climb = (NavLadderClimb) {
          .bottom = a,
          .top = b,
          .time = time,
          .column = column,
          .landing = landing,
          .runUp = runUp,
          .ladder = module.ladder
        };
      }
      return;
    }
  }

  if (graph->numClimbs == NAV_MAX_CLIMBS) {
    module.params->Warn(__func__, "More than %d ladder climbs; bots may not climb the rest\n", NAV_MAX_CLIMBS);
    return;
  }

  if (!graph->climbs) {
    graph->climbs = malloc(sizeof(NavLadderClimb) * NAV_MAX_CLIMBS);
    if (!graph->climbs) {
      module.params->Error(__func__, "Failed to allocate %d ladder climbs\n", NAV_MAX_CLIMBS);
    }
  }

  graph->climbs[graph->numClimbs++] = (NavLadderClimb) {
    .bottom = a,
    .top = b,
    .time = time,
    .column = column,
    .landing = landing,
    .jump = jump,
    .runUp = runUp,
    .ladder = module.ladder
  };
}

/**
 * @brief Adds the climbs of the column beside a ladder's face at `column`, whose outward normal is
 * `normal`, which go straight up beside the face until the player's box clears the top of the
 * ladder, then across its top, onto the floor on top of the ladder, or `NAV_LADDER_EXIT` beyond its
 * far side. A climb starts at the floor under the ladder's bottom, or if it has none within
 * `NAV_LADDER_FLOOR`, at the top of the liquid under it, where a swimmer touches the ladder. A
 * climb also starts at each node in front of the face, within `NAV_MAX_JUMP_CELLS` cells and
 * below the top, from which a running jump grabs the ladder (`Nav_LadderJump`), as over a pit, lava
 * or slime. A column with no room for the player's box, as against the wall that the ladder stands
 * on, a column where a player is not on the ladder all the way up (`Nav_OnLadder`), a column whose
 * top is blocked, and a top with no floor or no node across it, as a ladder that only dresses a
 * wall has, add no climb.
 * @return True if the column climbs the ladder, as a climb added now or before.
 */
static bool Nav_AddClimb(const Box3 ladder, const Vec3 column, const Vec3 normal) {

  const Box3 bounds = PM_BOUNDS;
  const float lowest = ladder.mins.z - bounds.mins.z;
  const float highest = ladder.maxs.z - bounds.mins.z + 1.f;

  Vec3 start = MakeVec3(column.x, column.y, lowest);
  while (start.z < highest && Nav_Trace(start, start, bounds).startSolid) {
    start.z += NAV_FINE_STEP;
  }

  if (start.z >= highest) {
    return false;
  }

  Vec3 bottom = start;
  const CollisionTrace floor = Nav_Trace(start, Vec3_Subtract(start, MakeVec3(0.f, 0.f, NAV_LADDER_FLOOR)), bounds);

  if (floor.fraction < 1.f) {
    bottom = floor.end;
  } else {
    while (!(Nav_PointContents(bottom) & CONTENTS_MASK_LIQUID) && bottom.z > floor.end.z) {
      bottom.z = Maxf(bottom.z - NAV_FINE_STEP, floor.end.z);
    }

    if (!(Nav_PointContents(bottom) & CONTENTS_MASK_LIQUID)) {
      bottom = start;
    }
  }

  const Vec3 top = MakeVec3(column.x, column.y, highest);
  const CollisionTrace rise = Nav_Trace(bottom, top, bounds);
  if (rise.startSolid || rise.fraction < 1.f) {
    return false;
  }

  if (!Nav_OnLadder(start, MakeVec3(column.x, column.y, ladder.maxs.z - bounds.mins.z - PM_STEP_HEIGHT), normal, bounds)) {
    return false;
  }

  const float depth = fabsf(normal.x) * (ladder.maxs.x - ladder.mins.x) + fabsf(normal.y) * (ladder.maxs.y - ladder.mins.y);
  const float distances[] = {
    bounds.maxs.x * 2.f + NAV_LADDER_EXIT,
    depth + bounds.maxs.x * 2.f + NAV_LADDER_EXIT
  };

  int32_t b = -1;
  float across = 0.f;
  Vec3 landing = Vec3_Zero();

  for (size_t i = 0; i < lengthof(distances) && b == -1; i++) {
    across = distances[i];
    const Vec3 over = Vec3_Fmaf(top, -across, normal);

    const CollisionTrace cross = Nav_Trace(top, over, bounds);
    if (cross.startSolid || cross.fraction < 1.f) {
      break;
    }

    if (!Nav_Rest(over, PM_STEP_HEIGHT * 2.f, bounds, &landing)) {
      continue;
    }

    if (Nav_TouchesTrigger(Box3_Translate(bounds, landing))) {
      continue;
    }

    b = Nav_LadderNode(landing, PM_STEP_HEIGHT);
  }

  if (b == -1) {
    return false;
  }

  const int32_t over = Nav_TravelTime(across, PM_SPEED_RUN);
  const bool foot = floor.fraction < 1.f || (Nav_PointContents(bottom) & CONTENTS_MASK_LIQUID);
  const int32_t a = foot ? Nav_LadderNode(bottom, PM_STEP_HEIGHT * 2.f) : -1;

  bool climbs = false;

  if (a != -1 && a != b) {
    Nav_AddClimbLink(a, b, NAV_LADDER_DELAY + Nav_TravelTime(top.z - bottom.z, PM_SPEED_LADDER) + over, bottom, landing, false, bottom);
    climbs = true;
  }

  if (module.gravity <= 0.f) {
    return climbs;
  }

  const NavGraph *graph = module.graph;
  const float high = ladder.maxs.z - bounds.mins.z - PM_STEP_HEIGHT;
  const float reach = NAV_MAX_JUMP_CELLS * NAV_CELL_SIZE;

  const Vec3i xyz = Nav_PointCoordinate(start);
  const int32_t below = (int32_t) ceilf(module.barrierHeight / NAV_CELL_HEIGHT);
  const int32_t above = (int32_t) ceilf((high - start.z) / NAV_CELL_HEIGHT);

  for (int32_t dz = -below; dz <= above; dz++) {
    for (int32_t dy = -NAV_MAX_JUMP_CELLS; dy <= NAV_MAX_JUMP_CELLS; dy++) {
      for (int32_t dx = -NAV_MAX_JUMP_CELLS; dx <= NAV_MAX_JUMP_CELLS; dx++) {

        const int32_t cell = Nav_CellIndex(MakeVec3i(xyz.x + dx, xyz.y + dy, xyz.z + dz));
        if (cell == -1 || graph->cellNodes[cell] == -1) {
          continue;
        }

        const int32_t n = graph->cellNodes[cell];
        const NavNode *node = &graph->nodes[n];

        if (n == a || n == b || node->model || node->crouch || (node->contents & CONTENTS_MASK_LIQUID)) {
          continue;
        }

        if (node->position.z > graph->nodes[b].position.z - PM_STEP_HEIGHT) {
          continue;
        }

        if (Vec2_Distance(Vec3_XY(node->position), Vec3_XY(start)) > reach) {
          continue;
        }

        Vec3 grab, runUp;
        float seconds;

        if (!Nav_LadderJump(node, start, normal, start.z, high, &grab, &runUp, &seconds)) {
          continue;
        }

        const int32_t time = NAV_LADDER_DELAY + (int32_t) (seconds * 100.f) +
                             Nav_TravelTime(top.z - grab.z, PM_SPEED_LADDER) + over;

        Nav_AddClimbLink(n, b, time, grab, landing, true, runUp);
        climbs = true;
      }
    }
  }

  return climbs;
}

/**
 * @brief Adds the face of `ladder` with the outward axial `normal` to the graph's ladders, as the
 * face whose climbs `Nav_AddClimb` adds next.
 * @param axis The axis of `normal`.
 * @param face The position of the face along `axis`.
 */
static void Nav_AddLadder(const Box3 ladder, const Vec3 normal, const int32_t axis,
                          const float face) {

  NavGraph *graph = module.graph;

  graph->ladders = realloc(graph->ladders, sizeof(NavLadder) * (graph->numLadders + 1));
  if (!graph->ladders) {
    module.params->Error(__func__, "Failed to allocate %d ladders\n", graph->numLadders + 1);
  }

  const int32_t other = 1 - axis;

  Vec3 lateral = Vec3_Zero();
  lateral.xyz[other] = 1.f;

  Vec3 center = Vec3_Zero();
  center.xyz[axis] = face + normal.xyz[axis] * (PM_BOUNDS.maxs.x + 1.f);
  center.xyz[other] = (ladder.mins.xyz[other] + ladder.maxs.xyz[other]) * .5f;
  center.z = ladder.mins.z - PM_BOUNDS.mins.z;

  module.ladder = graph->numLadders;

  graph->ladders[graph->numLadders++] = (NavLadder) {
    .bounds = ladder,
    .normal = normal,
    .lateral = lateral,
    .center = center,
    .halfWidth = (ladder.maxs.xyz[other] - ladder.mins.xyz[other]) * .5f,
    .top = ladder.maxs.z - PM_BOUNDS.mins.z
  };
}

/**
 * @brief Removes the last of the graph's ladders if no climb names it.
 */
static void Nav_TrimLadder(void) {

  NavGraph *graph = module.graph;
  const int32_t last = graph->numLadders - 1;

  for (int32_t i = 0; i < graph->numClimbs; i++) {
    if (graph->climbs[i].ladder == last) {
      return;
    }
  }

  graph->numLadders--;
}

/**
 * @return True if the brush has a side on the plane with the axial `normal` at `dist`.
 */
static bool Nav_HasSide(const CollisionBrush *brush, const Vec3 normal, const float dist) {

  for (int32_t i = 0; i < brush->numBrushSides; i++) {
    const CollisionPlane *plane = brush->brushSides[i].plane;

    if (Vec3_Equal(plane->normal, normal) && fabsf(plane->dist - dist) < 1.f) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Finds the climbs of the level's ladders. A ladder is a group of brushes of the world with
 * `CONTENTS_LADDER` that touch, as rails and rungs that are each a brush are. For each ladder, each
 * vertical face of its bounds on which one of its brushes has a side is climbed, at its center first
 * and then at columns `NAV_LADDER_SPACING` apart on each side of it, within the face, because a
 * player who faces any side climbs (`Pm_CheckLadder`). Only the widest faces are climbed, as the
 * front and the back of a ladder, and not its thin sides, which a bot could hold only by an edge
 * of its box. The first column that climbs between two nodes is the one that the climb keeps, so
 * a climb keeps to the middle of its ladder. A ladder of an inline model, which moves or is not
 * where the world is traced, and a sloped side, beside which a column would stand too far from the
 * ladder, add no climb. A ladder that no column climbs because its face is behind or level with
 * another brush's is reported, for its mapper.
 */
static void Nav_FindClimbs(void) {

  const CollisionBsp *bsp = module.params->bsp;

  int32_t brushes[NAV_MAX_LADDER_BRUSHES];
  int32_t groups[NAV_MAX_LADDER_BRUSHES];
  int32_t numBrushes = 0;

  for (int32_t i = 0; i < bsp->numBrushes; i++) {
    const CollisionBrush *brush = &bsp->brushes[i];

    if (!(brush->contents & CONTENTS_LADDER) || !(Nav_PointContents(Box3_Center(brush->bounds)) & CONTENTS_LADDER)) {
      continue;
    }

    if (numBrushes == NAV_MAX_LADDER_BRUSHES) {
      module.params->Warn(__func__, "More than %d ladder brushes; bots may not climb the rest\n", NAV_MAX_LADDER_BRUSHES);
      break;
    }

    brushes[numBrushes] = i;
    groups[numBrushes] = numBrushes;
    numBrushes++;
  }

  for (int32_t i = 0; i < numBrushes; i++) {
    const Box3 bounds = Box3_Expand(bsp->brushes[brushes[i]].bounds, 1.f);

    for (int32_t j = i + 1; j < numBrushes; j++) {
      if (groups[j] == groups[i] || !Box3_Intersects(bounds, bsp->brushes[brushes[j]].bounds)) {
        continue;
      }

      const int32_t from = groups[j];
      for (int32_t k = 0; k < numBrushes; k++) {
        if (groups[k] == from) {
          groups[k] = groups[i];
        }
      }
    }
  }

  for (int32_t g = 0; g < numBrushes; g++) {

    Box3 ladder = Box3_Null();
    for (int32_t i = 0; i < numBrushes; i++) {
      if (groups[i] == g) {
        ladder = Box3_Union(ladder, bsp->brushes[brushes[i]].bounds);
      }
    }

    if (Box3_IsNull(ladder)) {
      continue;
    }

    bool climbs = false;
    module.ladderHidden = false;

    const float widest = Maxf(ladder.maxs.x - ladder.mins.x, ladder.maxs.y - ladder.mins.y);

    for (int32_t axis = 0; axis < 2; axis++) {
      const int32_t other = 1 - axis;
      const float width = ladder.maxs.xyz[other] - ladder.mins.xyz[other];

      if (width < widest) {
        continue;
      }

      const int32_t columns = 1 + 2 * (int32_t) floorf(width * .5f / NAV_LADDER_SPACING);
      const float center = (ladder.mins.xyz[other] + ladder.maxs.xyz[other]) * .5f;

      for (int32_t sign = -1; sign <= 1; sign += 2) {
        Vec3 normal = Vec3_Zero();
        normal.xyz[axis] = (float) sign;

        const float face = sign > 0 ? ladder.maxs.xyz[axis] : ladder.mins.xyz[axis];

        bool side = false;
        for (int32_t i = 0; i < numBrushes && !side; i++) {
          side = groups[i] == g && Nav_HasSide(&bsp->brushes[brushes[i]], normal, face * sign);
        }

        if (!side) {
          continue;
        }

        Nav_AddLadder(ladder, normal, axis, face);

        for (int32_t j = 0; j < columns; j++) {
          const int32_t step = (j + 1) / 2 * (j % 2 ? 1 : -1);

          Vec3 column = Vec3_Zero();
          column.xyz[axis] = face + sign * (PM_BOUNDS.maxs.x + 1.f);
          column.xyz[other] = center + step * NAV_LADDER_SPACING;

          climbs |= Nav_AddClimb(ladder, column, normal);
        }

        Nav_TrimLadder();
      }
    }

    if (!climbs && module.ladderHidden) {
      module.params->Warn(__func__, "The ladder at %s %s is behind or level with the face of a brush that a player "
                          "collides with, so players grab it only at some places, if at all; extend it in front "
                          "of that brush\n", vtos(ladder.mins), vtos(ladder.maxs));
    }
  }
}

/**
 * @brief Links one node to its neighbors: walks to the nodes in adjacent cells, and drops and jumps
 * across gaps where no walk exists. A node on a model that moves walks only to the nodes on the same
 * model, and the game links it to the rest as the model moves. A node in liquid also swims, and a
 * swim node only swims. A walk to or from a node in liquid takes the time at `PM_SPEED_WATER`,
 * because a player who walks on a floor in liquid moves no faster than one who swims. A node at
 * the foot of a ladder's climb, or that jumps onto the ladder, links to its top. A bot does not
 * climb down a ladder: it falls, by a drop link. No drop, barrier jump, jump or swim enters lava or
 * slime (`Nav_EntersHazard`).
 * @return The count of links written to `links`.
 */
static int32_t Nav_LinkNode(const int32_t index, NavLink *links) {

  const NavGraph *graph = module.graph;
  const NavNode *node = &graph->nodes[index];

  const Vec3i xyz = Nav_CellCoordinate(node->cell);

  int32_t numLinks = 0;

  for (size_t i = 0; i < lengthof(directions) && !node->swim; i++) {

    bool walked = false;

    for (int32_t dz = -1; dz <= 2; dz++) {

      const Vec3i neighbor = MakeVec3i(xyz.x + directions[i].x, xyz.y + directions[i].y, xyz.z + dz);

      const int32_t cell = Nav_CellIndex(neighbor);
      if (cell == -1 || graph->cellNodes[cell] == -1) {
        continue;
      }

      const int32_t target = graph->cellNodes[cell];
      const NavNode *other = &graph->nodes[target];

      if (other->model != node->model || other->swim) {
        continue;
      }

      float distance;
      int32_t gate = 0;
      if (Nav_Walk(node, other, &distance)) {
        Nav_AddWalk(links, &numLinks, node, other, target, distance, 0);
        walked = true;
      } else if (!Nav_EntersHazard(node, other) && Nav_BarrierJump(node, other, &distance)) {
        const float fall = module.barrierHeight - (other->position.z - node->position.z);
        const int32_t time = Nav_TravelTime(distance, PM_SPEED_RUN) +
                             Nav_FallTime(module.barrierHeight) + Nav_FallTime(fall);
        Nav_AddLink(links, &numLinks, target, NAV_TRAVEL_BARRIER_JUMP, time, 0);
        walked = true;
      } else if ((gate = Nav_GatedWalk(node, other, &distance))) {
        Nav_AddWalk(links, &numLinks, node, other, target, distance, gate);
        walked = true;
      }
    }

    if (walked || node->model) {
      continue;
    }

    float distance, height;
    int32_t dropGate = 0;
    int32_t target = Nav_Drop(node, directions[i], &distance, &height);
    if (target == -1) {
      target = Nav_GatedDrop(node, directions[i], &distance, &height, &dropGate);
    }
    if (target != -1 && target != index && !Nav_EntersHazard(node, &graph->nodes[target])) {
      const int32_t time = Nav_TravelTime(distance, PM_SPEED_RUN) + Nav_FallTime(height);
      Nav_AddLink(links, &numLinks, target, NAV_TRAVEL_WALK_OFF_LEDGE, time, dropGate);
    }

    float seconds;
    int32_t gate = 0;
    int32_t landing = Nav_Jump(node, directions[i], &seconds);
    if (landing == -1) {
      landing = Nav_GatedJump(node, directions[i], &seconds, &gate);
    }
    if (landing != -1 && !Nav_EntersHazard(node, &graph->nodes[landing])) {
      Nav_AddLink(links, &numLinks, landing, NAV_TRAVEL_JUMP, (int32_t) ceilf(seconds * 100.f), gate);
    }
  }

  if ((node->contents & CONTENTS_MASK_LIQUID) && !node->model) {
    Nav_LinkSwims(index, links, &numLinks);
  }

  for (int32_t i = 0; i < module.graph->numClimbs; i++) {
    const NavLadderClimb *climb = &module.graph->climbs[i];

    if (climb->bottom == index) {
      Nav_AddLink(links, &numLinks, climb->top, NAV_TRAVEL_LADDER, climb->time, 0);
    }
  }

  return numLinks;
}

/**
 * @return The highest ledge that the level's movement jumps onto, less `NAV_BARRIER_MARGIN`. With
 * no gravity, a jump has no top, and the ledge is taken to be as high as a water jump climbs.
 */
static float Nav_BarrierHeight(void) {

  const float jump = module.params->movement.speedJump;
  const float height = module.gravity > 0.f ? jump * jump / (2.f * module.gravity) : PM_STEP_HEIGHT + Box3_Size(PM_BOUNDS).z;

  return height - NAV_BARRIER_MARGIN;
}

/**
 * @return The highest ledge, above the floating origin, that the level's movement water jumps onto,
 * less `NAV_WATER_JUMP_MARGIN`.
 * @remarks A water jump starts only if the player box fits `PM_STEP_HEIGHT` plus its own height
 * above the origin, over the ledge, which caps the climb whatever the jump speed and gravity.
 */
static float Nav_WaterJumpHeight(void) {

  const float jump = module.params->movement.speedWaterJump;
  const float cap = PM_STEP_HEIGHT + Box3_Size(PM_BOUNDS).z;
  const float height = module.gravity > 0.f ? Minf(cap, jump * jump / (2.f * module.gravity)) : cap;

  return height - NAV_WATER_JUMP_MARGIN;
}

/**
 * @return The entity's origin, by which its inline model is placed.
 */
static Vec3 Nav_EntityOrigin(const Entity *entity) {

  const Entity *origin = module.params->EntityValue(entity, "origin");

  return (origin->parsed & ENTITY_VEC3) ? origin->vec3 : Vec3_Zero();
}

/**
 * @return The entity's integer value for the key, or 0.
 */
static int32_t Nav_EntityInteger(const Entity *entity, const char *key) {

  const Entity *value = module.params->EntityValue(entity, key);

  return (value->parsed & ENTITY_INTEGER) ? value->integer : 0;
}

/**
 * @return True if the entity has a value for the key.
 */
static bool Nav_EntityHas(const Entity *entity, const char *key) {
  return module.params->EntityValue(entity, key)->parsed != 0;
}

/**
 * @brief Adds the `trigger_push` to the graph's pushes, with its velocity read as the game reads it.
 */
static void Nav_AddPush(const Entity *entity, const Box3 bounds, const int32_t spawnFlags) {

  NavGraph *graph = module.graph;

  graph->pushes = realloc(graph->pushes, sizeof(NavPush) * (graph->numPushes + 1));
  if (!graph->pushes) {
    module.params->Error(__func__, "Failed to allocate %d pushes\n", graph->numPushes + 1);
  }

  Vec3 angles = Vec3_Zero();

  const Entity *anglesValue = module.params->EntityValue(entity, "angles");
  if (anglesValue->parsed & ENTITY_VEC3) {
    angles = anglesValue->vec3;
  }

  const Entity *angleValue = module.params->EntityValue(entity, "angle");
  if (angleValue->parsed & ENTITY_FLOAT) {
    angles = MakeVec3(0.f, angleValue->value, 0.f);
  }

  Vec3 dir = Vec3_Zero();

  if (Vec3_Equal(angles, MakeVec3(0.f, -1.f, 0.f))) {
    dir = Vec3_Up();
  } else if (Vec3_Equal(angles, MakeVec3(0.f, -2.f, 0.f))) {
    dir = Vec3_Down();
  } else if (!Vec3_Equal(angles, Vec3_Zero())) {
    Vec3_Vectors(angles, &dir, NULL, NULL);
  }

  float speed = module.params->EntityValue(entity, "speed")->value;
  if (!speed) {
    speed = 100.f;
  }

  graph->pushes[graph->numPushes++] = (NavPush) {
    .bounds = bounds,
    .velocity = Vec3_Scale(dir, speed * 10.f),
    .startOff = (spawnFlags & NAV_PUSH_START_OFF) != 0
  };
}

/**
 * @brief Gathers the level's `trigger_hurt` and `trigger_push` entities, and the doors that a bot
 * cannot open by walking up to them, the walls and the rotating models, from the inline models of
 * the BSP.
 * @details A player touches a trigger where their bounds intersect, as `G_TouchOccupy` does, so
 * that is where no node may stand and no walk may pass. A `trigger_push` is widened by
 * `NAV_PUSH_MARGIN` at its sides and above it, because a player who walks over its top is thrown
 * too. A floor separates it from the space below it, so it is not widened downward. A jump pad's
 * own link starts at the nearest node beside it. A door with `health` or a `targetname` opens when
 * shot or triggered, and has no trigger that opens it for a player who walks up to it
 * (`G_func_door`). Such a door is sampled where the map places it, closed, even if it spawns open,
 * as a bridge over a trap does. A walk or a drop through its doorway, and a jump through it or over
 * its place, is a gated link (`Nav_GatedWalk`, `Nav_GatedDrop`, `Nav_GatedJump`), which the game
 * opens and closes with the door.
 * A wall is solid where it spawns so (`G_func_wall`). A door, a
 * `func_train`, a `func_bob` and a `func_rotating` that turns about the vertical axis carry their
 * floors with them, so their nodes move with them. A `func_rotating` that turns about a horizontal
 * axis is taken to be a smooth wheel, whose top stays where it spawns.
 */
static void Nav_InitModels(void) {

  const CollisionBsp *bsp = module.params->bsp;

  for (int32_t i = 1; i < bsp->numModels; i++) {
    const CollisionModel *model = &bsp->models[i];

    if (!model->entity) {
      continue;
    }

    const char *classname = module.params->EntityValue(model->entity, "classname")->string;
    const int32_t spawnFlags = Nav_EntityInteger(model->entity, "spawnflags");
    const Vec3 origin = Nav_EntityOrigin(model->entity);
    const Box3 bounds = Box3_Translate(model->bounds, origin);

    if (!Str_Compare(classname, "trigger_push")) {
      Nav_AddPush(model->entity, bounds, spawnFlags);
    }

    if (!Str_Compare(classname, "trigger_hurt") || !Str_Compare(classname, "trigger_push")) {

      if (module.numTriggers == NAV_MAX_TRIGGERS) {
        module.params->Warn(__func__, "More than %d hurt and push triggers; bots may walk into the rest\n", NAV_MAX_TRIGGERS);
        continue;
      }

      if (!Str_Compare(classname, "trigger_push")) {
        Box3 push = Box3_Expand3(bounds, MakeVec3(NAV_PUSH_MARGIN, NAV_PUSH_MARGIN, 0.f));
        push.maxs.z += NAV_PUSH_MARGIN;
        module.triggers[module.numTriggers++] = push;
      } else {
        module.triggers[module.numTriggers++] = bounds;
      }
      continue;
    }

    bool barrier = false, moves = false, gate = false;

    if (!Str_Compare(classname, "func_door") || !Str_Compare(classname, "func_door_rotating")) {
      barrier = Nav_EntityInteger(model->entity, "health") || Nav_EntityHas(model->entity, "targetname");
      moves = gate = true;
    } else if (!Str_Compare(classname, "func_train") || !Str_Compare(classname, "func_bob")) {
      barrier = moves = true;
    } else if (!Str_Compare(classname, "func_wall")) {
      barrier = (spawnFlags & NAV_WALL_SPAWN_FLAGS) == 0 || (spawnFlags & NAV_WALL_START_ON);
    } else if (!Str_Compare(classname, "func_rotating")) {
      barrier = true;
      moves = !(spawnFlags & NAV_ROTATING_HORIZONTAL);
    }

    if (!barrier) {
      continue;
    }

    if (module.numBarriers == NAV_MAX_BARRIERS) {
      module.params->Warn(__func__, "More than %d closed doors, walls and rotating models; bots may path through the rest\n", NAV_MAX_BARRIERS);
      continue;
    }

    int32_t group = module.numBarriers;
    const char *targetname = module.params->EntityValue(model->entity, "targetname")->string;

    for (int32_t j = 0; gate && *targetname && j < module.numBarriers; j++) {
      const Entity *other = module.params->bsp->models[module.barriers[j].model].entity;
      if (module.barriers[j].gate && !Str_Compare(module.params->EntityValue(other, "targetname")->string, targetname)) {
        group = module.barriers[j].group;
        break;
      }
    }

    module.barriers[module.numBarriers++] = (NavBarrier) {
      .model = i,
      .moves = moves,
      .gate = gate,
      .group = group,
      .headNode = model->headNode,
      .origin = origin,
      .bounds = bounds
    };
  }
}

/**
 * @brief Lays the cell grid over the world model's bounds, aligned to the cell size.
 */
static void Nav_InitGrid(NavGraph *graph) {

  const Box3 bounds = module.params->bsp->models[0].bounds;

  const Vec3 mins = MakeVec3(floorf(bounds.mins.x / NAV_CELL_SIZE) * NAV_CELL_SIZE,
                             floorf(bounds.mins.y / NAV_CELL_SIZE) * NAV_CELL_SIZE,
                             floorf(bounds.mins.z / NAV_CELL_HEIGHT) * NAV_CELL_HEIGHT);

  graph->mins = mins;
  graph->size = MakeVec3i((int32_t) ceilf((bounds.maxs.x - mins.x) / NAV_CELL_SIZE),
                          (int32_t) ceilf((bounds.maxs.y - mins.y) / NAV_CELL_SIZE),
                          (int32_t) ceilf((bounds.maxs.z - mins.z) / NAV_CELL_HEIGHT));
  graph->numCells = graph->size.x * graph->size.y * graph->size.z;

  graph->cellNodes = malloc(sizeof(int32_t) * Maxi(1, graph->numCells));
  if (!graph->cellNodes) {
    module.params->Error(__func__, "Failed to allocate %d navigation cells\n", graph->numCells);
  }

  for (int32_t i = 0; i < graph->numCells; i++) {
    graph->cellNodes[i] = -1;
  }
}

/**
 * @brief A swim link between two nodes in liquid, which `Nav_PruneSwims` MAY remove.
 */
typedef struct {
  /**
   * @brief The index of the node that the link leaves.
   */
  int32_t from;

  /**
   * @brief The index of the link in `NavGraph::links`.
   */
  int32_t link;
} NavSwim;

/**
 * @brief Orders swim links by their time, then by their index, so that the order is the same in
 * the game and the client game.
 * @return Less than, equal to or greater than 0, as `a` sorts before, with or after `b`.
 */
static int32_t Nav_PruneSwims_compare(const void *a, const void *b) {

  const NavSwim *x = a, *y = b;
  const NavGraph *graph = module.graph;

  const int32_t dt = graph->links[x->link].travelTime - graph->links[y->link].travelTime;

  return dt ? dt : x->link - y->link;
}

/**
 * @return The index of the swim link from node `a` to node `b` that `removed` keeps, and that is
 * faster than `time`, or -1.
 */
static int32_t Nav_PruneSwims_link(const int32_t a, const int32_t b, const int32_t time, const bool *removed) {

  const NavGraph *graph = module.graph;
  const NavNode *node = &graph->nodes[a];

  for (int32_t i = node->firstLink; i < node->firstLink + node->numLinks; i++) {
    const NavLink *link = &graph->links[i];

    if (link->node == b && link->travelType == NAV_TRAVEL_SWIM && !removed[i] && link->travelTime < time) {
      return i;
    }
  }

  return -1;
}

/**
 * @brief Removes the swim links between nodes in liquid that a detour through one other node in
 * liquid replaces, at no more than `NAV_SWIM_PRUNE` times their time. A node in liquid links to
 * every node in liquid in the 3 by 3 by 5 cells around it, which makes most of the links of a level
 * with deep water, and a search visits each link. The links are tried from the fastest, and a
 * detour uses only faster links that are kept, so each removed link keeps a route through the
 * links that stay. The links out of the liquid, which water jumps make, stay.
 */
static void Nav_PruneSwims(NavGraph *graph) {

  if (!graph->numLinks) {
    return;
  }

  NavSwim *swims = malloc(sizeof(NavSwim) * graph->numLinks);
  bool *removed = calloc(graph->numLinks, sizeof(bool));

  if (!swims || !removed) {
    module.params->Error(__func__, "Failed to allocate %d swim links\n", graph->numLinks);
  }

  int32_t numSwims = 0;

  for (int32_t i = 0; i < graph->numNodes; i++) {
    const NavNode *node = &graph->nodes[i];

    if (!(node->contents & CONTENTS_MASK_LIQUID)) {
      continue;
    }

    for (int32_t j = node->firstLink; j < node->firstLink + node->numLinks; j++) {
      const NavLink *link = &graph->links[j];

      if (link->travelType == NAV_TRAVEL_SWIM && (graph->nodes[link->node].contents & CONTENTS_MASK_LIQUID)) {
        swims[numSwims++] = (NavSwim) { .from = i, .link = j };
      }
    }
  }

  qsort(swims, numSwims, sizeof(NavSwim), Nav_PruneSwims_compare);

  for (int32_t i = 0; i < numSwims; i++) {
    const NavNode *node = &graph->nodes[swims[i].from];
    const NavLink *link = &graph->links[swims[i].link];

    for (int32_t j = node->firstLink; j < node->firstLink + node->numLinks; j++) {
      const NavLink *first = &graph->links[j];

      if (first->travelType != NAV_TRAVEL_SWIM || removed[j] || first->travelTime >= link->travelTime ||
          !(graph->nodes[first->node].contents & CONTENTS_MASK_LIQUID)) {
        continue;
      }

      const int32_t second = Nav_PruneSwims_link(first->node, link->node, link->travelTime, removed);
      if (second == -1) {
        continue;
      }

      if (first->travelTime + graph->links[second].travelTime <= link->travelTime * NAV_SWIM_PRUNE) {
        removed[swims[i].link] = true;
        break;
      }
    }
  }

  int32_t numLinks = 0;

  memset(graph->numTravel, 0, sizeof(graph->numTravel));
  graph->numGated = 0;

  for (int32_t i = 0; i < graph->numNodes; i++) {
    NavNode *node = &graph->nodes[i];

    const int32_t first = node->firstLink, count = node->numLinks;

    node->firstLink = numLinks;
    node->numLinks = 0;

    for (int32_t j = first; j < first + count; j++) {
      if (removed[j]) {
        continue;
      }

      graph->links[numLinks++] = graph->links[j];
      graph->numTravel[graph->links[j].travelType]++;
      graph->numGated += graph->links[j].gate != 0;
      node->numLinks++;
    }
  }

  graph->numLinks = numLinks;

  free(swims);
  free(removed);
}

/**
 * @see bg_nav.h
 */
bool Nav_Build(const NavParams *params, NavGraph *graph) {

  memset(graph, 0, sizeof(*graph));
  memset(&module, 0, sizeof(module));

  module.params = params;
  module.graph = graph;
  module.openGate = -1;
  module.gravity = params->movement.gravity;
  module.barrierHeight = Nav_BarrierHeight();
  module.waterJumpHeight = Nav_WaterJumpHeight();

  Nav_InitGrid(graph);

  Nav_InitModels();

  for (int32_t i = 0; i < graph->numCells; i++) {
    bool offset = false;

    switch (Nav_SampleCell(i, &offset)) {
      case NAV_SAMPLE_NODE:
        graph->numOffset += offset;
        break;
      case NAV_SAMPLE_NO_FIT:
        graph->numNoFit++;
        break;
      case NAV_SAMPLE_TRIGGER:
        graph->numTrigger++;
        break;
      case NAV_SAMPLE_NONE:
        graph->numSwim += Nav_SampleLiquid(i);
        break;
    }
  }

  if (!graph->numNodes) {
    Nav_Free(graph);
    return false;
  }

  Nav_FindClimbs();

  int32_t capacity = graph->numNodes * 8;
  graph->links = malloc(sizeof(NavLink) * capacity);
  if (!graph->links) {
    module.params->Error(__func__, "Failed to allocate %d navigation links\n", capacity);
  }

  for (int32_t i = 0; i < graph->numNodes; i++) {
    NavLink links[NAV_MAX_NODE_LINKS];

    graph->nodes[i].edge = Nav_IsEdge(&graph->nodes[i]);
    graph->numEdge += graph->nodes[i].edge;

    const int32_t numLinks = Nav_LinkNode(i, links);

    if (graph->numLinks + numLinks > capacity) {
      capacity *= 2;
      graph->links = realloc(graph->links, sizeof(NavLink) * capacity);
      if (!graph->links) {
        module.params->Error(__func__, "Failed to allocate %d navigation links\n", capacity);
      }
    }

    graph->nodes[i].firstLink = graph->numLinks;
    graph->nodes[i].numLinks = numLinks;

    for (int32_t j = 0; j < numLinks; j++) {
      graph->links[graph->numLinks++] = links[j];
      graph->numTravel[links[j].travelType]++;
      graph->numGated += links[j].gate != 0;
    }
  }

  Nav_PruneSwims(graph);

  return true;
}

/**
 * @see bg_nav.h
 */
bool Nav_FlyPush(const NavPush *push, PMove *pm, Vec3 *points, int32_t maxPoints, int32_t *numPoints, float *seconds) {

  const Box3 bounds = Pm_Bounds(&pm->s.params, false);
  const uint8_t msec = QUETOO_TICK_MILLIS;

  if (points) {
    *numPoints = 0;
  }

  for (uint32_t millis = 0; millis <= NAV_FLIGHT_MILLIS; millis += msec) {

    if (points && *numPoints < maxPoints) {
      points[(*numPoints)++] = pm->s.origin;
    }

    if (Box3_Intersects(Box3_Translate(bounds, pm->s.origin), push->bounds)) {
      pm->s.velocity = push->velocity;
      pm->s.flags |= PMF_TIME_PUSHED;
      pm->s.time = 240;
    } else if (pm->ground.ent && !(pm->s.flags & PMF_TIME_PUSHED)) {
      *seconds = MILLIS_TO_SECONDS(millis);
      return true;
    }

    pm->cmd = (PMoveCmd) {
      .msec = msec
    };

    Pm_Move(pm);
  }

  return false;
}

/**
 * @see bg_nav.h
 */
bool Nav_SameMovement(const PMoveParams *a, const PMoveParams *b) {
  return a->gravity == b->gravity && a->speedJump == b->speedJump && a->speedWaterJump == b->speedWaterJump &&
         a->frictionAir == b->frictionAir && a->frictionWater == b->frictionWater &&
         Box3_Equal(a->bounds, b->bounds);
}

/**
 * @see bg_nav.h
 */
void Nav_FreeNodes(NavGraph *graph) {

  free(graph->nodes);
  graph->nodes = NULL;

  free(graph->links);
  graph->links = NULL;
}

/**
 * @see bg_nav.h
 */
void Nav_Free(NavGraph *graph) {

  free(graph->nodes);
  free(graph->links);
  free(graph->cellNodes);
  free(graph->pushes);
  free(graph->climbs);
  free(graph->ladders);

  memset(graph, 0, sizeof(*graph));
}
