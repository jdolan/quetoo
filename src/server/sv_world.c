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

#include "sv_local.h"

// FIXME: Couldn't we use the BSP blocks for this now?

/**
 * @brief The world is divided into evenly sized sectors to aid in entity
 * management. This works like a meta-BSP tree, providing fast searches via
 * recursion to find entities within an arbitrary box.
 */
typedef struct ServerSector {
  int32_t axis; // -1 = leaf
  float dist;
  struct ServerSector *children[2];
  List *entities;
} ServerSector;

#define SECTOR_DEPTH  4
#define SECTOR_NODES  32

/**
 * @brief The world structure contains all sectors and also the current query
 * context issued to `Sv_BoxEntities`.
 */
typedef struct {
  ServerSector sectors[SECTOR_NODES];
  size_t numSectors;

  Box3 box;

  GameEntity **boxEntities;
  size_t numBoxEntities, maxBoxEntities;

  uint32_t boxType; // BOX_SOLID, BOX_TRIGGER, ..
} ServerWorld;

static ServerWorld svWorld;

/**
 * @brief Builds a uniformly subdivided tree for the given world size.
 */
static ServerSector *Sv_CreateSector(int32_t depth, const Box3 bounds) {
  ServerSector *sector = &svWorld.sectors[svWorld.numSectors];
  svWorld.numSectors++;

  if (depth == SECTOR_DEPTH) {
    sector->axis = -1;
    sector->children[0] = sector->children[1] = NULL;
    return sector;
  }

  Vec3 size = Box3_Size(bounds);

  if (size.x > size.y) {
    sector->axis = 0;
  } else {
    sector->axis = 1;
  }

  sector->dist = 0.5f * (bounds.maxs.xyz[sector->axis] + bounds.mins.xyz[sector->axis]);

  Box3 bounds1 = bounds, bounds2 = bounds;

  bounds1.maxs.xyz[sector->axis] = bounds2.mins.xyz[sector->axis] = sector->dist;

  sector->children[0] = Sv_CreateSector(depth + 1, bounds2);
  sector->children[1] = Sv_CreateSector(depth + 1, bounds1);

  return sector;
}

/**
 * @brief Initializes the world sector tree for spatial partitioning of entities.
 */
static void Sv_InitWorld(void) {

  for (size_t i = 0; i < svWorld.numSectors; i++) {
    svWorld.sectors[i].entities = release(svWorld.sectors[i].entities);
  }

  memset(&svWorld, 0, sizeof(svWorld));

  Sv_CreateSector(0, sv.cmModels[0]->bounds);
}

/**
 * @brief Initializes the world and spawns all entities for the current map.
 */
void Sv_SpawnEntities(const char *name, const CmEntity *props) {

  Sv_InitWorld();

  for (int32_t i = 0; i < sv_maxEntities->integer; i++) {
    sv.entities[i].gent = svs.game->entities[i];
  }

  if (editor->value) {
    Sv_LoadEditorMap();

    const int32_t numEntities = Cm_Bsp()->numEntities;

    if (numEntities > sv_maxEntities->integer) {
      Com_Error(ERROR_DROP, "Map has %d entities but sv_max_entities is %d\n",
        numEntities, sv_maxEntities->integer);
    }

    CmEntity **defs = Mem_TagMalloc(sizeof(CmEntity *) * numEntities, MEM_TAG_SERVER);
    for (int32_t i = 0; i < numEntities; i++) {
      defs[i] = Cm_CopyEntity(Cm_Bsp()->entities[i]);
    }

    svs.game->SpawnEntities(name, props, defs, numEntities);

    Mem_Free(defs);

    for (int32_t i = 0; i < numEntities; i++) {
      Sv_ConfigureEditorEntity(i);
    }
  } else {
    svs.game->SpawnEntities(name, props, Cm_Bsp()->entities, Cm_Bsp()->numEntities);
  }

  /*
   * Run a few game frames for entities to settle down. Failure to do
   * this will cause the entities to produce all but useless baselines,
   * which will in turn blow out the packet entities in Sv_EmitEntities.
   */

  for (int32_t i = 0; i < 3; i++) {
    svs.game->Frame();
  }
}

/**
 * @brief Called before moving or freeing an entity to remove it from the clipping
 * hull.
 */
void Sv_UnlinkEntity(GameEntity *ent) {

  ServerEntity *sent = &sv.entities[ent->s.number];

  if (sent->sector) {
    ServerSector *sector = (ServerSector *) sent->sector;
    if (sector->entities) {
      for (ListNode *node = sector->entities->head; node; node = node->next) {
        if (node->element == ent) {
          $(sector->entities, removeNode, node);
          break;
        }
      }
    }

    GameEntity *gent = sent->gent;
    memset(sent, 0, sizeof(*sent));
    sent->gent = gent;
  }
}

/**
 * @brief Called whenever an entity changes origin, mins, maxs, or solid to add it to
 * the clipping hull.
 */
void Sv_LinkEntity(GameEntity *ent) {

  // remove it from its current sector
  Sv_UnlinkEntity(ent);

  if (!ent->inUse) { // and if its free, we're done
    return;
  }

  // set the size
  ent->size = Box3_Size(ent->bounds);

  // encode the size into the entity state for client prediction
  ent->s.solid = ent->solid;
  switch (ent->s.solid) {
    case SOLID_TRIGGER:
    case SOLID_PROJECTILE:
    case SOLID_DEAD:
    case SOLID_BOX:
    case SOLID_EDITOR:
      ent->s.bounds = ent->bounds;
      break;
    default:
      ent->s.bounds = Box3_Zero();
      break;
  }

  const Vec3 angles = ent->solid == SOLID_BSP ? ent->s.angles : Vec3_Zero();

  ServerEntity *sent = &sv.entities[ent->s.number];

  sent->matrix = Mat4_FromRotationTranslationScale(angles, ent->s.origin, 1.f);
  sent->inverseMatrix = Mat4_Inverse(sent->matrix);
  ent->absBounds = Cm_EntityBounds(ent->solid, sent->matrix, ent->bounds);

  if (ent->solid == SOLID_NOT) {
    return;
  }

  // find the first sector that the ent's box crosses
  ServerSector *sector = svWorld.sectors;
  while (true) {

    if (sector->axis == -1) {
      break;
    }

    if (ent->absBounds.mins.xyz[sector->axis] > sector->dist) {
      sector = sector->children[0];
    } else if (ent->absBounds.maxs.xyz[sector->axis] < sector->dist) {
      sector = sector->children[1];
    } else {
      break; // crosses the node
    }
  }

  // add it to the sector
  sent->sector = sector;
  if (!sector->entities) {
    sector->entities = $(alloc(List), init);
  }
  $(sector->entities, prepend, ent);
}

/**
 * @return True if the entity matches the current world filter, false otherwise.
 */
static bool Sv_BoxEntities_Filter(const GameEntity *ent) {

  switch (ent->solid) {
    case SOLID_TRIGGER:
    case SOLID_PROJECTILE:
      if (svWorld.boxType & BOX_OCCUPY) {
        return true;
      }
      break;

    case SOLID_DEAD:
    case SOLID_BOX:
    case SOLID_BSP:
      if (svWorld.boxType & BOX_COLLIDE) {
        return true;
      }
      break;

    case SOLID_NOT:
    case SOLID_EDITOR:
      break;
  }

  return false;
}

/**
 * @brief Recursively collects entities from the sector tree that overlap the query box.
 */
static void Sv_BoxEntities_r(ServerSector *sector) {

  if (sector->entities) {
    for (const ListNode *node = sector->entities->head; node; node = node->next) {
      GameEntity *ent = (GameEntity *) node->element;

      if (Sv_BoxEntities_Filter(ent)) {

        if (Box3_Intersects(ent->absBounds, svWorld.box)) {

          svWorld.boxEntities[svWorld.numBoxEntities] = ent;
          svWorld.numBoxEntities++;

          if (svWorld.numBoxEntities == svWorld.maxBoxEntities) {
            Com_Warn("sv_world.max_box_entities\n");
            return;
          }
        }
      }
    }
  }

  if (sector->axis == -1) {
    return; // terminal node
  }

  // recurse down both sides
  if (svWorld.box.maxs.xyz[sector->axis] > sector->dist) {
    Sv_BoxEntities_r(sector->children[0]);
  }

  if (svWorld.box.mins.xyz[sector->axis] < sector->dist) {
    Sv_BoxEntities_r(sector->children[1]);
  }
}

/**
 * @brief Populates an array of entities with those which have bounding boxes
 * that intersect the given box. It is possible for a non-axial BSP model to
 * be returned that doesn't actually intersect the box.
 *
 * @return The number of entities found.
 */
size_t Sv_BoxEntities(const Box3 bounds, GameEntity **list, const size_t len, uint32_t type) {

  svWorld.box = bounds;
  svWorld.boxEntities = list;
  svWorld.numBoxEntities = 0;
  svWorld.maxBoxEntities = len;
  svWorld.boxType = type;

  Sv_BoxEntities_r(svWorld.sectors);

  svWorld.box = Box3_Zero();
  svWorld.boxEntities = NULL;

  return svWorld.numBoxEntities;
}

/**
 * @brief Prepares the collision model to clip to the specified entity. For
 * mesh models, the box hull must be set to reflect the bounds of the entity.
 */
static int32_t Sv_HullForEntity(const GameEntity *ent) {

  switch (ent->solid) {

    case SOLID_DEAD:
      return Cm_SetBoxHull(ent->bounds, CONTENTS_DEAD_MONSTER);

    case SOLID_BOX: {
      if (ent->client) {
        return Cm_SetBoxHull(ent->bounds, CONTENTS_MONSTER);
      } else {
        return Cm_SetBoxHull(ent->bounds, CONTENTS_SOLID);
      }
    }

    case SOLID_BSP: {
      const CmBspModel *mod = sv.cmModels[ent->s.model1];
      if (!mod) {
        Com_Error(ERROR_DROP, "SOLID_BSP with no model\n");
      }
      return mod->headNode;
    }

    case SOLID_EDITOR:
      return Cm_SetBoxHull(ent->bounds, CONTENTS_EDITOR);

    default:
      return -1;
  }
}

/**
 * @brief Returns the contents mask for the specified point. This includes world
 * contents as well as contents for any solid entities this point intersects.
 */
int32_t Sv_PointContents(const Vec3 point) {
  GameEntity *entities[MAX_ENTITIES];

  // get base contents from world
  int32_t contents = Cm_PointContents(point, 0, Mat4_Identity());

  // as well as contents from all intersected entities
  const size_t len = Sv_BoxEntities(Box3_FromCenter(point), entities, lengthof(entities), BOX_COLLIDE);

  // iterate the box entities, checking each one for an intersection
  for (size_t i = 0; i < len; i++) {
    const GameEntity *ent = entities[i];

    const int32_t headNode = Sv_HullForEntity(ent);
    if (headNode != -1) {

      const ServerEntity *sent = &sv.entities[ent->s.number];
      contents |= Cm_PointContents(point, headNode, sent->inverseMatrix);
    }
  }

  return contents;
}

/**
 * @brief Returns the contents mask for the specified bounds. This includes world
 * contents as well as contents for any solid entities this point intersects.
 */
int32_t Sv_BoxContents(const Box3 bounds) {
  GameEntity *entities[MAX_ENTITIES];

  // get base contents from world
  int32_t contents = Cm_BoxContents(bounds, 0);

  // as well as contents from all intersected entities
  const size_t len = Sv_BoxEntities(bounds, entities, lengthof(entities), BOX_COLLIDE);

  // iterate the box entities, checking each one for an intersection
  for (size_t i = 0; i < len; i++) {
    const GameEntity *ent = entities[i];

    const int32_t headNode = Sv_HullForEntity(ent);
    if (headNode != -1) {

      const ServerEntity *sent = &sv.entities[ent->s.number];
      contents |= Cm_BoxContents(Mat4_TransformBounds(sent->inverseMatrix, bounds), headNode);
    }
  }

  return contents;
}

// an entity's movement, with allowed exceptions and other info
typedef struct {
  Vec3 start, end;
  Box3 bounds; // size of the moving object
  Box3 absBounds; // enclose the test object along entire move
  CmTrace trace;
  const GameEntity *skip;
  int32_t contents;
} ServerTrace;

/**
 * @brief Clips the specified trace to the specified entity.
 */
static void Sv_ClipTraceToEntity(ServerTrace *trace, const GameEntity *ent) {

  if (trace->skip) { // see if we can skip it

    if (ent == trace->skip) {
      return; // explicitly (ourselves)
    }

    if (ent->owner == trace->skip) {
      return; // or via ownership (we own it)
    }

    if (trace->skip->owner) {

      if (ent == trace->skip->owner) {
        return; // which is bidirectional (inverse of previous case)
      }

      if (ent->owner == trace->skip->owner) {
        return; // and commutative (we are both owned by the same)
      }
    }

    // triggers only clip to the world (while other entities can occupy triggers)
    if (trace->skip->solid == SOLID_TRIGGER) {

      if (ent->solid != SOLID_BSP) {
        return;
      }
    }
  }

  const int32_t headNode = Sv_HullForEntity(ent);
  if (headNode == -1) {
    return;
  }

  if (svs.game->ClipEntity && !svs.game->ClipEntity(trace->skip, ent)) {
    return;
  }

  const ServerEntity *sent = &sv.entities[ent->s.number];

  CmTrace tr;
  
  if (Mat4_Equal(sent->matrix, Mat4_Identity())) {
    tr = Cm_BoxTrace(trace->start, trace->end, trace->bounds, headNode, trace->contents);
  } else {
    tr = Cm_TransformedBoxTrace(trace->start, trace->end, trace->bounds, headNode, trace->contents, sent->matrix, sent->inverseMatrix);
  }

  // check for a full or partial intersection
  if (tr.allSolid || tr.fraction < trace->trace.fraction) {

    trace->trace = tr;
    trace->trace.ent = (GameEntity *) ent;

    if (tr.allSolid) { // we were actually blocked
      return;
    }
  }
}

/**
 * @brief Clips the specified trace to other entities in its bounds. This is the basis of all
 * collision and interaction for the server. Tread carefully.
 */
static void Sv_ClipTraceToEntities(ServerTrace *trace) {
  GameEntity *e[MAX_ENTITIES];

  const size_t len = Sv_BoxEntities(trace->absBounds, e, lengthof(e), BOX_COLLIDE);
  for (size_t i = 0; i < len; i++) {
    Sv_ClipTraceToEntity(trace, e[i]);
  }
}

/**
 * @brief Moves the given box volume through the world from start to end.
 *
 * The skipped edict, and edicts owned by him, are explicitly not checked.
 * This prevents players from clipping against their own projectiles, etc.
 */
CmTrace Sv_Trace(const Vec3 start, const Vec3 end, const Box3 bounds,
                    const GameEntity *skip, int32_t contents) {

  ServerTrace trace = {
    .start = start,
    .end = end,
    .bounds = bounds,
    .absBounds = Cm_TraceBounds(start, end, bounds),
    .skip = skip,
    .contents = contents,
    .trace = {
      .fraction = 1.f,
      .end = end,
    }
  };

  Sv_ClipTraceToEntities(&trace);

  return trace.trace;
}

/**
 * @brief Tests a clip of the specified translation against the specified entity.
 */
CmTrace Sv_Clip(const Vec3 start, const Vec3 end, const Box3 bounds,
                   const GameEntity *test, int32_t contents) {

  ServerTrace trace = {
    .trace = {
      .fraction = 1.f
    },
    .start = start,
    .end = end,
    .bounds = bounds,
    .contents = contents
  };

  Sv_ClipTraceToEntity(&trace, test);

  return trace.trace;
}
