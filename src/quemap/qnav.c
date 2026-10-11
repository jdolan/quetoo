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

#include "qnav.h"
#include "game/common/bg_pmove.h"

#include <SDL3/SDL.h>

/**
 * @brief The contents that a player stands on and collides with.
 */
#define NAV_MASK CONTENTS_MASK_CLIP_PLAYER

/**
 * @brief The height above a voxel center at which its floor sample starts. Floors on a 16 unit
 * grid often lie exactly on voxel centers, where neither a point test nor a trace ending there
 * finds them.
 */
#define NAV_SAMPLE_OFFSET .5f

/**
 * @brief The horizontal distance of one step of a simulated walk.
 */
#define NAV_WALK_STEP 8.f

/**
 * @brief The furthest a simulated walk goes past the edge of a floor, looking for the drop.
 */
#define NAV_LEDGE_DISTANCE 48.f

/**
 * @brief The furthest fall that a drop link spans.
 */
#define NAV_MAX_DROP 512.f

/**
 * @brief The furthest a fall can land from the node it links to.
 */
#define NAV_LANDING_DISTANCE (BSP_VOXEL_SIZE * 1.5f)

/**
 * @brief The gravity of a map that does not declare its own.
 */
#define NAV_DEFAULT_GRAVITY 800.f

/**
 * @brief Items further than this from every node are reported as unreachable.
 */
#define NAV_ITEM_DISTANCE 64.f

/**
 * @brief The most links that one node can have: a walk and a drop in each of 8 directions,
 * for each of 3 levels.
 */
#define NAV_MAX_NODE_LINKS 48

/**
 * @brief The outcome of a floor sample.
 */
typedef enum {
  SAMPLE_NONE,
  SAMPLE_NO_FIT,
  SAMPLE_NODE,
} SampleResult;

/**
 * @brief The floor sample of one voxel.
 */
typedef struct {
  /**
   * @brief The outcome of the sample.
   */
  SampleResult result;

  /**
   * @brief True if the player fits only at an offset from the voxel center.
   */
  bool offset;

  /**
   * @brief The node, if `result` is `SAMPLE_NODE`.
   */
  NavNode node;
} NavSample;

/**
 * @brief The links of one node, collected in parallel.
 */
typedef struct {
  int32_t numLinks;
  NavLink links[NAV_MAX_NODE_LINKS];
} NavNodeLinks;

/**
 * @brief The stage state.
 */
static struct {
  Vec3i size;
  Box3 bounds;
  int32_t numVoxels;
  float gravity;

  NavSample *samples;
  int32_t *voxelNodes;

  NavNode *nodes;
  int32_t numNodes;

  NavNodeLinks *nodeLinks;
  int32_t numLinks;
} module;

/**
 * @brief The horizontal sample offsets within a voxel, center first.
 */
static const Vec2 offsets[] = {
  { { 0.f, 0.f } },
  { { -8.f, 0.f } }, { { 8.f, 0.f } }, { { 0.f, -8.f } }, { { 0.f, 8.f } },
  { { -8.f, -8.f } }, { { 8.f, -8.f } }, { { -8.f, 8.f } }, { { 8.f, 8.f } },
};

/**
 * @brief The 8 horizontal directions to neighboring voxels.
 */
static const Vec3i directions[] = {
  { { 1, 0, 0 } }, { { -1, 0, 0 } }, { { 0, 1, 0 } }, { { 0, -1, 0 } },
  { { 1, 1, 0 } }, { { -1, 1, 0 } }, { { 1, -1, 0 } }, { { -1, -1, 0 } },
};

/**
 * @return The index of the voxel at the given grid coordinate, or -1 if it is outside the grid.
 */
static int32_t VoxelIndex(const Vec3i xyz) {

  if (xyz.x < 0 || xyz.y < 0 || xyz.z < 0) {
    return -1;
  }

  if (xyz.x >= module.size.x || xyz.y >= module.size.y || xyz.z >= module.size.z) {
    return -1;
  }

  return (xyz.z * module.size.y + xyz.y) * module.size.x + xyz.x;
}

/**
 * @return The grid coordinate of the given voxel index.
 */
static Vec3i VoxelCoordinate(const int32_t index) {

  const int32_t x = index % module.size.x;
  const int32_t y = (index / module.size.x) % module.size.y;
  const int32_t z = index / (module.size.x * module.size.y);

  return MakeVec3i(x, y, z);
}

/**
 * @return The grid coordinate of the voxel that contains the given point.
 */
static Vec3i PointCoordinate(const Vec3 point) {

  const Vec3 rel = Vec3_Subtract(point, module.bounds.mins);

  return MakeVec3i((int32_t) floorf(rel.x / BSP_VOXEL_SIZE),
                   (int32_t) floorf(rel.y / BSP_VOXEL_SIZE),
                   (int32_t) floorf(rel.z / BSP_VOXEL_SIZE));
}

/**
 * @return The center of the voxel at the given grid coordinate.
 */
static Vec3 VoxelOrigin(const Vec3i xyz) {
  return Vec3_Add(module.bounds.mins, MakeVec3((xyz.x + .5f) * BSP_VOXEL_SIZE,
                                               (xyz.y + .5f) * BSP_VOXEL_SIZE,
                                               (xyz.z + .5f) * BSP_VOXEL_SIZE));
}

/**
 * @return A player trace against the world.
 */
static CollisionTrace Trace(const Vec3 start, const Vec3 end, const Box3 bounds) {
  return Cm_BoxTrace(start, end, bounds, 0, NAV_MASK);
}

/**
 * @return The contents of the brushes that contain `point`.
 * @remarks Leaf contents are the union of every brush in the leaf, and detail brushes do not
 * split leaves, so leaf contents are too coarse here. A leaf with no brushes is one that the BSP
 * filled as outside of the map.
 */
static int32_t PointContents(const Vec3 point) {

  const CollisionBsp *bsp = Cm_Bsp();
  const CollisionLeaf *leaf = &bsp->leafs[Cm_PointLeafnum(point, 0)];

  if (leaf->numLeafBrushes == 0) {
    return leaf->contents;
  }

  int32_t contents = 0;

  for (int32_t i = 0; i < leaf->numLeafBrushes; i++) {
    const CollisionBrush *brush = &bsp->brushes[bsp->leafBrushes[leaf->firstLeafBrush + i]];
    if (Cm_PointInsideBrush(point, brush)) {
      contents |= brush->contents;
    }
  }

  return contents;
}

/**
 * @return The bounding box for the given node flags.
 */
static Box3 NodeBounds(const int32_t flags) {
  return (flags & NAV_NODE_CROUCH) ? PM_CROUCHED_BOUNDS : PM_BOUNDS;
}

/**
 * @brief Rests the bounding box on the floor beneath `start`, as Pmove would.
 * @return True if the box fits at `start` and lands on a walkable floor within `distance`.
 */
static bool Rest(const Vec3 start, const float distance, const Box3 bounds, Vec3 *origin) {

  const CollisionTrace trace = Trace(start, Vec3_Subtract(start, Vec3_Scale(Vec3_Up(), distance)), bounds);

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
 * @brief Samples the floor beneath `point`, within one voxel height.
 */
static SampleResult SampleFloor(const Vec3 point, NavNode *node) {

  if (PointContents(point) & NAV_MASK) {
    return SAMPLE_NONE;
  }

  const Vec3 end = Vec3_Subtract(point, Vec3_Scale(Vec3_Up(), BSP_VOXEL_SIZE));
  const CollisionTrace floor = Trace(point, end, Box3_Zero());

  if (floor.startSolid || floor.fraction == 1.f) {
    return SAMPLE_NONE;
  }

  if (floor.plane.normal.z < PM_STEP_NORMAL) {
    return SAMPLE_NONE;
  }

  const Vec3 normal = floor.plane.normal;
  const float lift = PM_BOUNDS.maxs.x * (fabsf(normal.x) + fabsf(normal.y)) / normal.z + 1.f;
  const float height = -PM_BOUNDS.mins.z;

  const Vec3 start = Vec3_Add(floor.end, Vec3_Scale(Vec3_Up(), height + lift));

  Vec3 origin;
  if (Rest(start, lift + 1.f, PM_BOUNDS, &origin)) {
    node->flags = 0;
  } else if (Rest(start, lift + 1.f, PM_CROUCHED_BOUNDS, &origin)) {
    node->flags = NAV_NODE_CROUCH;
  } else {
    return SAMPLE_NO_FIT;
  }

  if (PointContents(origin) & CONTENTS_MASK_SOLID) {
    return SAMPLE_NO_FIT;
  }

  node->position = origin;
  node->contents = PointContents(origin) & CONTENTS_MASK_LIQUID;

  return SAMPLE_NODE;
}

/**
 * @brief Samples the floor of one voxel, at its center first and then at small offsets.
 */
static void SampleVoxel(int32_t index) {

  NavSample *sample = &module.samples[index];

  const Vec3 center = Vec3_Add(VoxelOrigin(VoxelCoordinate(index)), Vec3_Scale(Vec3_Up(), NAV_SAMPLE_OFFSET));

  for (size_t i = 0; i < lengthof(offsets); i++) {

    const Vec3 point = Vec3_Add(center, MakeVec3(offsets[i].x, offsets[i].y, 0.f));

    NavNode node = { .voxel = index };

    const SampleResult result = SampleFloor(point, &node);
    if (result == SAMPLE_NODE) {
      sample->result = SAMPLE_NODE;
      sample->offset = i > 0;
      sample->node = node;
      return;
    }

    if (result == SAMPLE_NO_FIT) {
      sample->result = SAMPLE_NO_FIT;
    }
  }
}

/**
 * @brief Moves the bounding box one step toward `target`, as Pmove would: up by the step
 * height, across, and down onto the floor.
 * @return True if the box arrives on a walkable floor.
 */
static bool StepMove(const Vec3 from, const Vec3 target, const Box3 bounds, Vec3 *to) {

  const CollisionTrace up = Trace(from, Vec3_Add(from, Vec3_Scale(Vec3_Up(), PM_STEP_HEIGHT)), bounds);
  if (up.startSolid) {
    return false;
  }

  const Vec3 across = MakeVec3(target.x, target.y, up.end.z);
  const CollisionTrace over = Trace(up.end, across, bounds);
  if (over.fraction < 1.f) {
    return false;
  }

  return Rest(over.end, PM_STEP_HEIGHT * 2.f + 1.f, bounds, to);
}

/**
 * @return The travel time for the given distance and speed, in hundredths of a second.
 */
static int32_t TravelTime(const float distance, const float speed) {
  return Maxi(1, (int32_t) ceilf(distance / speed * 100.f));
}

/**
 * @return The time to fall the given height, in hundredths of a second.
 */
static int32_t FallTime(const float height) {
  return (int32_t) ceilf(sqrtf(2.f * height / module.gravity) * 100.f);
}

/**
 * @return The node closest to `origin`, standing on the same floor within
 * `NAV_LANDING_DISTANCE`, or -1.
 */
static int32_t NodeForOrigin(const Vec3 origin) {

  const Vec3i xyz = PointCoordinate(Vec3_Subtract(origin, Vec3_Scale(Vec3_Up(), -PM_BOUNDS.mins.z - BSP_VOXEL_SIZE * .5f)));

  int32_t best = -1;
  float bestDistance = NAV_LANDING_DISTANCE * NAV_LANDING_DISTANCE;

  for (int32_t dz = -1; dz <= 1; dz++) {
    for (int32_t dy = -1; dy <= 1; dy++) {
      for (int32_t dx = -1; dx <= 1; dx++) {

        const int32_t voxel = VoxelIndex(MakeVec3i(xyz.x + dx, xyz.y + dy, xyz.z + dz));
        if (voxel == -1 || module.voxelNodes[voxel] == -1) {
          continue;
        }

        const NavNode *node = &module.nodes[module.voxelNodes[voxel]];
        if (fabsf(node->position.z - origin.z) > PM_STEP_HEIGHT) {
          continue;
        }

        const float distance = Vec3_DistanceSquared(node->position, origin);
        if (distance < bestDistance) {
          bestDistance = distance;
          best = module.voxelNodes[voxel];
        }
      }
    }
  }

  return best;
}

/**
 * @brief Adds a link from `links` to `node`, unless one exists.
 */
static void AddLink(NavNodeLinks *links, const int32_t node, const NavTravelType type, const int32_t time) {

  for (int32_t i = 0; i < links->numLinks; i++) {
    if (links->links[i].node == node) {
      return;
    }
  }

  if (links->numLinks == NAV_MAX_NODE_LINKS) {
    Com_Error(ERROR_FATAL, "NAV_MAX_NODE_LINKS\n");
  }

  links->links[links->numLinks++] = (NavLink) {
    .node = node,
    .travelType = type,
    .travelTime = time
  };
}

/**
 * @brief Simulates a walk from node `a` to node `b`.
 * @return True if the walk arrives at `b`.
 */
static bool Walk(const NavNode *a, const NavNode *b, float *distance) {

  const Box3 bounds = NodeBounds(a->flags | b->flags);

  const Vec3 delta = Vec3_Subtract(b->position, a->position);
  const float length = Vec2_Length(Vec3_XY(delta));
  const int32_t steps = Maxi(1, (int32_t) ceilf(length / NAV_WALK_STEP));

  Vec3 origin = a->position;
  *distance = 0.f;

  for (int32_t i = 1; i <= steps; i++) {

    const Vec3 target = Vec3_Mix(a->position, b->position, i / (float) steps);

    Vec3 next;
    if (!StepMove(origin, target, bounds, &next)) {
      return false;
    }

    *distance += Vec3_Distance(origin, next);
    origin = next;
  }

  return fabsf(origin.z - b->position.z) <= 1.f;
}

/**
 * @brief Simulates a walk from `node` in `direction`, off a ledge, and the fall that follows.
 * @return The node that the fall lands at, or -1.
 */
static int32_t Drop(const NavNode *node, const Vec3i direction, float *distance, float *height) {

  const Box3 bounds = NodeBounds(node->flags);

  const Vec3 dir = Vec3_Normalize(MakeVec3(direction.x, direction.y, 0.f));

  Vec3 origin = node->position;
  *distance = 0.f;

  for (float d = NAV_WALK_STEP; d <= NAV_LEDGE_DISTANCE; d += NAV_WALK_STEP) {

    const Vec3 target = Vec3_Add(node->position, Vec3_Scale(dir, d));

    const CollisionTrace across = Trace(origin, MakeVec3(target.x, target.y, origin.z), bounds);
    if (across.fraction < 1.f) {
      return -1;
    }

    *distance += Vec3_Distance(origin, across.end);
    origin = across.end;

    Vec3 floor;
    if (Rest(origin, PM_STEP_HEIGHT + 1.f, bounds, &floor)) {
      origin = floor;
      continue;
    }

    const Vec3 end = Vec3_Subtract(origin, Vec3_Scale(Vec3_Up(), NAV_MAX_DROP));
    const CollisionTrace fall = Trace(origin, end, bounds);

    if (fall.startSolid || fall.fraction == 1.f || fall.plane.normal.z < PM_STEP_NORMAL) {
      return -1;
    }

    *height = node->position.z - fall.end.z;
    if (*height <= PM_STEP_HEIGHT) {
      return -1;
    }

    return NodeForOrigin(fall.end);
  }

  return -1;
}

/**
 * @brief Links one node to its neighbors: walks to the nodes in adjacent voxels, and drops
 * where no walk exists.
 */
static void LinkNode(int32_t index) {

  const NavNode *node = &module.nodes[index];
  NavNodeLinks *links = &module.nodeLinks[index];

  const Vec3i xyz = VoxelCoordinate(node->voxel);

  for (size_t i = 0; i < lengthof(directions); i++) {

    bool walked = false;

    for (int32_t dz = -1; dz <= 1; dz++) {

      const Vec3i neighbor = MakeVec3i(xyz.x + directions[i].x, xyz.y + directions[i].y, xyz.z + dz);

      const int32_t voxel = VoxelIndex(neighbor);
      if (voxel == -1 || module.voxelNodes[voxel] == -1) {
        continue;
      }

      const int32_t target = module.voxelNodes[voxel];
      const NavNode *other = &module.nodes[target];

      float distance;
      if (Walk(node, other, &distance)) {
        const bool crouch = (node->flags | other->flags) & NAV_NODE_CROUCH;
        const NavTravelType type = crouch ? NAV_TRAVEL_CROUCH : NAV_TRAVEL_WALK;
        AddLink(links, target, type, TravelTime(distance, crouch ? PM_SPEED_DUCKED : PM_SPEED_RUN));
        walked = true;
      }
    }

    if (walked) {
      continue;
    }

    float distance, height;
    const int32_t target = Drop(node, directions[i], &distance, &height);
    if (target != -1 && target != index) {
      const int32_t time = TravelTime(distance, PM_SPEED_RUN) + FallTime(height);
      AddLink(links, target, NAV_TRAVEL_WALK_OFF_LEDGE, time);
    }
  }
}

/**
 * @brief Collects the sampled nodes in voxel order.
 */
static void CollectNodes(void) {

  int32_t numNodes = 0, numOffset = 0, numNoFit = 0, numCrouch = 0;

  for (int32_t i = 0; i < module.numVoxels; i++) {
    switch (module.samples[i].result) {
      case SAMPLE_NODE:
        numNodes++;
        break;
      case SAMPLE_NO_FIT:
        numNoFit++;
        break;
      default:
        break;
    }
  }

  module.nodes = Mem_TagMalloc(sizeof(NavNode) * Maxi(1, numNodes), (MemTag) MEM_TAG_QNAV);
  module.voxelNodes = Mem_TagMalloc(sizeof(int32_t) * module.numVoxels, (MemTag) MEM_TAG_QNAV);

  for (int32_t i = 0; i < module.numVoxels; i++) {

    const NavSample *sample = &module.samples[i];

    if (sample->result != SAMPLE_NODE) {
      module.voxelNodes[i] = -1;
      continue;
    }

    module.voxelNodes[i] = module.numNodes;
    module.nodes[module.numNodes++] = sample->node;

    if (sample->offset) {
      numOffset++;
    }

    if (sample->node.flags & NAV_NODE_CROUCH) {
      numCrouch++;
    }
  }

  Com_Print("%9d nodes\n", module.numNodes);
  Com_Print("%9d crouch nodes\n", numCrouch);
  Com_Print("%9d nodes found at an offset\n", numOffset);
  Com_Print("%9d floors with no room for a player\n", numNoFit);
}

/**
 * @brief Assigns each node its range of links, and counts the links of each travel type.
 */
static void CollectLinks(void) {

  int32_t counts[NAV_TRAVEL_WALK_OFF_LEDGE + 1] = { 0 };

  for (int32_t i = 0; i < module.numNodes; i++) {

    const NavNodeLinks *links = &module.nodeLinks[i];

    module.nodes[i].firstLink = module.numLinks;
    module.nodes[i].numLinks = links->numLinks;
    module.numLinks += links->numLinks;

    for (int32_t j = 0; j < links->numLinks; j++) {
      counts[links->links[j].travelType]++;
    }
  }

  Com_Print("%9d links\n", module.numLinks);
  Com_Print("%9d walk\n", counts[NAV_TRAVEL_WALK]);
  Com_Print("%9d crouch\n", counts[NAV_TRAVEL_CROUCH]);
  Com_Print("%9d walk off ledge\n", counts[NAV_TRAVEL_WALK_OFF_LEDGE]);
}

/**
 * @brief Warns of every item that no node is near.
 */
static void CheckItems(void) {

  int32_t numItems = 0, numUnreachable = 0;

  Entity **e = Cm_Bsp()->entities;
  for (int32_t i = 0; i < Cm_Bsp()->numEntities; i++, e++) {

    const char *classname = Entity_Value(*e, "classname")->string;

    if (Str_CompareN(classname, "item_", 5) &&
        Str_CompareN(classname, "weapon_", 7) &&
        Str_CompareN(classname, "ammo_", 5)) {
      continue;
    }

    const Entity *origin = Entity_Value(*e, "origin");
    if (!(origin->parsed & ENTITY_VEC3)) {
      continue;
    }

    numItems++;

    bool reachable = false;
    for (int32_t j = 0; j < module.numNodes && !reachable; j++) {
      reachable = Vec3_Distance(module.nodes[j].position, origin->vec3) <= NAV_ITEM_DISTANCE;
    }

    if (!reachable) {
      Com_Warn("%s @ %s has no node within %g units\n", classname, vtos(origin->vec3), NAV_ITEM_DISTANCE);
      numUnreachable++;
    }
  }

  Com_Print("%9d of %d items with no node near\n", numUnreachable, numItems);
}

/**
 * @brief Writes the nodes and links to `maps/<name>.nav`.
 */
static void WriteNavFile(void) {

  const char *path = va("maps/%s.nav", mapBase);

  File *file = Fs_OpenWrite(path);
  if (!file) {
    Com_Error(ERROR_FATAL, "Failed to open %s\n", path);
  }

  const NavHeader header = {
    .magic = NAV_MAGIC,
    .version = NAV_VERSION,
    .voxelSize = module.size,
    .voxelMins = module.bounds.mins,
    .numNodes = module.numNodes,
    .numLinks = module.numLinks
  };

  Fs_Write(file, &header, sizeof(header), 1);
  Fs_Write(file, module.nodes, sizeof(NavNode), module.numNodes);

  for (int32_t i = 0; i < module.numNodes; i++) {
    const NavNodeLinks *links = &module.nodeLinks[i];
    Fs_Write(file, links->links, sizeof(NavLink), links->numLinks);
  }

  Fs_Close(file);

  Com_Print("Wrote %s\n", path);
}

/**
 * @see qnav.h
 */
int32_t Qnav_Main(void) {

  Com_Print("\n------------------------------------------\n");
  Com_Print("\nNavigating %s\n\n", bspName);

  const uint32_t start = (uint32_t) SDL_GetTicks();

  memset(&module, 0, sizeof(module));

  Cm_LoadBspModel(bspName, NULL);

  const CollisionBsp *bsp = Cm_Bsp();
  if (!bsp->numVoxels) {
    Com_Error(ERROR_FATAL, "%s has no voxels; compile it with -bsp first\n", bspName);
  }

  module.size = bsp->voxelSize;
  module.bounds = bsp->voxelBounds;
  module.numVoxels = bsp->numVoxels;

  const Entity *gravity = Entity_Value(bsp->entities[0], "gravity");
  if ((gravity->parsed & ENTITY_INTEGER) && gravity->integer > 0) {
    module.gravity = gravity->integer;
  } else {
    module.gravity = NAV_DEFAULT_GRAVITY;
  }

  module.samples = Mem_TagMalloc(sizeof(NavSample) * module.numVoxels, (MemTag) MEM_TAG_QNAV);

  Work("Sampling floors", SampleVoxel, module.numVoxels);

  CollectNodes();

  module.nodeLinks = Mem_TagMalloc(sizeof(NavNodeLinks) * Maxi(1, module.numNodes), (MemTag) MEM_TAG_QNAV);

  Work("Linking nodes", LinkNode, module.numNodes);

  CollectLinks();

  CheckItems();

  WriteNavFile();

  Mem_FreeTag((MemTag) MEM_TAG_QNAV);

  const uint32_t end = (uint32_t) SDL_GetTicks();
  Com_Print("\nNavigated %s in %d ms\n", bspName, (end - start));

  return 0;
}
