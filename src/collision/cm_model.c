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

#include "cm_local.h"

CollisionBsp collisionBsp = {};

/**
 * @brief Loads and parses the entity string lump into `collisionBsp.entities`.
 */
static void Cm_LoadBspEntities(CollisionBsp *bsp) {

  List *entities = Entity_LoadAll(bsp->file->entityString);

  bsp->numEntities = (int32_t) entities->count;
  bsp->entities = Mem_TagMalloc(sizeof(Entity *) * bsp->numEntities, MEM_TAG_COLLISION);

  Entity **out = bsp->entities;
  for (const ListNode *node = entities->head; node; node = node->next, out++) {
    *out = node->element;
  }

  release(entities);
}

/**
 * @brief Loads and converts the planes lump into `collisionBsp.planes`.
 */
static void Cm_LoadBspPlanes(CollisionBsp *bsp) {

  bsp->numPlanes = bsp->file->numPlanes;
  const BspPlane *in = bsp->file->planes;

  CollisionPlane *out = bsp->planes = Mem_TagMalloc(sizeof(CollisionPlane) * (bsp->numPlanes + 12), MEM_TAG_COLLISION); // extra for box hull

  for (int32_t i = 0; i < bsp->numPlanes; i++, in++, out++) {
    *out = Cm_Plane(in->normal, in->dist);
  }
}

/**
 * @brief Loads and converts the nodes lump into `collisionBsp.nodes`.
 */
static void Cm_LoadBspNodes(CollisionBsp *bsp) {

  bsp->numNodes = bsp->file->numNodes;
  const BspNode *in = bsp->file->nodes;

  CollisionNode *out = bsp->nodes = Mem_TagMalloc(sizeof(CollisionNode) * (bsp->numNodes + 6), MEM_TAG_COLLISION); // extra for box hull

  for (int32_t i = 0; i < bsp->numNodes; i++, in++, out++) {

    out->plane = bsp->planes + in->plane;

    for (int32_t j = 0; j < 2; j++) {
      out->children[j] = in->children[j];
    }
  }
}


/**
 * @brief Loads and converts the leafs lump into `collisionBsp.leafs`.
 */
static void Cm_LoadBspLeafs(CollisionBsp *bsp) {

  bsp->numLeafs = bsp->file->numLeafs;
  const BspLeaf *in = bsp->file->leafs;

  CollisionLeaf *out = bsp->leafs = Mem_TagMalloc(sizeof(CollisionLeaf) * (bsp->numLeafs + 1), MEM_TAG_COLLISION); // extra for box hull

  for (int32_t i = 0; i < bsp->numLeafs; i++, in++, out++) {
    out->contents = in->contents;
    out->firstLeafBrush = in->firstLeafBrush;
    out->numLeafBrushes = in->numLeafBrushes;
  }
}

/**
 * @brief Loads the leaf-brush index lump into `collisionBsp.leafBrushes`.
 */
static void Cm_LoadBspLeafBrushes(CollisionBsp *bsp) {

  bsp->numLeafBrushes = bsp->file->numLeafBrushes;
  const int32_t *in = bsp->file->leafBrushes;

  int32_t *out = bsp->leafBrushes = Mem_TagMalloc(sizeof(int32_t) * (bsp->numLeafBrushes + 1), MEM_TAG_COLLISION); // extra for box hull

  for (int32_t i = 0; i < bsp->numLeafBrushes; i++, in++, out++) {
    *out = *in;
  }
}

/**
 * @brief Loads and converts the brush sides lump into `collisionBsp.brushSides`.
 */
static void Cm_LoadBspBrushSides(CollisionBsp *bsp) {

  bsp->numBrushSides = bsp->file->numBrushSides;
  const BspBrushSide *in = bsp->file->brushSides;

  CollisionBrushSide *out = bsp->brushSides = Mem_TagMalloc(sizeof(CollisionBrushSide) *
        (bsp->numBrushSides + 6), MEM_TAG_COLLISION); // extra for box hull

  for (int32_t i = 0; i < bsp->numBrushSides; i++, in++, out++) {

    const int32_t p = in->plane;

    if (p >= bsp->numPlanes) {
      Com_Error(ERROR_DROP, "Brush side %d has invalid plane %d\n", i, p);
    }

    out->plane = &bsp->planes[p];

    if (in->material > -1) {
      if (in->material >= bsp->numMaterials) {
        Com_Error(ERROR_DROP, "Brush side %d has invalid material %d\n", i, in->material);
      }

      out->material = bsp->materials[in->material];
    }

    out->contents = in->contents;
    out->surface = in->surface;
    out->value = in->value;
  }
}

/**
 * @brief Loads and converts the brushes lump into `collisionBsp.brushes`.
 */
static void Cm_LoadBspBrushes(CollisionBsp *bsp) {

  bsp->numBrushes = bsp->file->numBrushes;
  const BspBrush *in = bsp->file->brushes;

  CollisionBrush *out = bsp->brushes = Mem_TagMalloc(sizeof(CollisionBrush) * (bsp->numBrushes + 1), MEM_TAG_COLLISION); // extra for box hull

  for (int32_t i = 0; i < bsp->numBrushes; i++, in++, out++) {

    if (in->entity < 0 || in->entity >= bsp->numEntities) {
      Com_Warn("Brush %d: invalid entity index %d\n", i, in->entity);
      out->entity = NULL;
    } else {
      out->entity = bsp->entities[in->entity];
    }
    out->contents = in->contents;
    out->brushSides = bsp->brushSides + in->firstBrushSide;
    out->numBrushSides = in->numBrushSides;
    out->bounds = in->bounds;
  }
}

/**
 * @brief Loads and converts the inline models lump into `collisionBsp.models`.
 */
static void Cm_LoadBspInlineModels(CollisionBsp *bsp) {

  bsp->numModels = bsp->file->numModels;
  const BspModel *in = bsp->file->models;

  CollisionModel *out = bsp->models = Mem_TagMalloc(sizeof(CollisionModel) * bsp->numModels, MEM_TAG_COLLISION);

  for (int32_t i = 0; i < bsp->numModels; i++, in++, out++) {
    
    if (in->entity < 0 || in->entity >= bsp->numEntities) {
      Com_Warn("Model %d: invalid entity index %d\n", i, in->entity);
      out->entity = NULL;
    } else {
      out->entity = bsp->entities[in->entity];
    }
    out->headNode = in->headNode;
    out->bounds = in->bounds;
  }
}

/**
 * @brief Loads and resolves materials referenced by the BSP into `collisionBsp.materials`.
 */
static void Cm_LoadBspMaterials(CollisionBsp *bsp) {

  bsp->numMaterials = bsp->file->numMaterials;

  Material **out = bsp->materials = Mem_TagMalloc(sizeof(Material *) * bsp->numMaterials, MEM_TAG_COLLISION);

  const BspMaterial *in = bsp->file->materials;
  for (int32_t i = 0; i < bsp->numMaterials; i++, in++, out++) {

    *out = Material_Load(in->name, ASSET_CONTEXT_TEXTURES);

    *out = Mem_Link(*out, bsp->materials);
  }
}

/**
 * @brief Decodes the voxel lump into a flat Voxel array on the CollisionBsp.
 */
static void Cm_LoadBspVoxels(CollisionBsp *bsp) {

  if (!bsp->file->voxels) {
    return;
  }

  const BspVoxels *v = bsp->file->voxels;

  bsp->voxelSize = v->size;
  bsp->voxelBounds = v->bounds;
  bsp->numVoxels = v->size.x * v->size.y * v->size.z;

  Voxel *out = bsp->voxels = Mem_TagMalloc(sizeof(Voxel) * bsp->numVoxels, MEM_TAG_COLLISION);

  const byte *rgb = (const byte *) (v + 1);
  const byte *occlusion = rgb +
    bsp->numVoxels * 3 +
    bsp->numVoxels * (int32_t) sizeof(int32_t) * 2 +
    (size_t) v->numLightIndices * sizeof(int32_t);

  for (int32_t z = 0; z < v->size.z; z++) {
    for (int32_t y = 0; y < v->size.y; y++) {
      for (int32_t x = 0; x < v->size.x; x++, out++, rgb += 3, occlusion += 2) {

        out->origin = Vec3_Add(v->bounds.mins,
          Vec3_Scale(MakeVec3((float) x + 0.5f, (float) y + 0.5f, (float) z + 0.5f), BSP_VOXEL_SIZE));

        out->caustics = MakeVec3(
          (rgb[0] / 255.f) * 2.f - 1.f,
          (rgb[1] / 255.f) * 2.f - 1.f,
          (rgb[2] / 255.f) * 2.f - 1.f
        );

        out->occlusion = occlusion[0] / 255.f;
        out->exposure  = occlusion[1] / 255.f;
      }
    }
  }
}

/**
 * @brief Lumps we need to load for the CM subsystem.
 */
#define CM_BSP_LUMPS \
  (1 << BSP_LUMP_ENTITIES) | \
  (1 << BSP_LUMP_MATERIALS) | \
  (1 << BSP_LUMP_PLANES) | \
  (1 << BSP_LUMP_NODES) | \
  (1 << BSP_LUMP_LEAFS) | \
  (1 << BSP_LUMP_LEAF_BRUSHES) | \
  (1 << BSP_LUMP_BRUSHES) | \
  (1 << BSP_LUMP_BRUSH_SIDES) | \
  (1 << BSP_LUMP_MODELS) | \
  (1 << BSP_LUMP_VOXELS)

/**
 * @brief Loads in the BSP and all sub-models for collision detection. This
 * function can also be used to initialize or clean up the collision model by
 * invoking with `NULL`.
 */
CollisionModel *Cm_LoadBspModel(const char *name, int64_t *size) {
  static BspFile file;

  Bsp_UnloadLumps(&file, BSP_LUMPS_ALL);

  // free dynamic memory
  Mem_Free(collisionBsp.planes);
  Mem_Free(collisionBsp.nodes);
  Mem_Free(collisionBsp.leafs);
  Mem_Free(collisionBsp.leafBrushes);
  Mem_Free(collisionBsp.brushes);
  Mem_Free(collisionBsp.brushSides);
  Mem_Free(collisionBsp.models);
  Mem_Free(collisionBsp.entities);
  Mem_Free(collisionBsp.materials);
  Mem_Free(collisionBsp.voxels);

  memset(&collisionBsp, 0, sizeof(collisionBsp));
  collisionBsp.file = &file;

  // clean up and return
  if (!name) {
    if (size) {
      *size = 0;
    }
    return &collisionBsp.models[0];
  }

  // load the common BSP structure and the lumps we need
  BspHeader *header;

  if (Fs_Load(name, (void **) &header) == -1) {
    Com_Error(ERROR_DROP, "Failed to load %s\n", name);
  }

  if (Bsp_Verify(header) == -1) {
    Fs_Free(header);
    Com_Error(ERROR_DROP, "Failed to verify %s\n", name);
  }

  if (!Bsp_LoadLumps(header, &file, CM_BSP_LUMPS)) {
    Fs_Free(header);
    Com_Error(ERROR_DROP, "Lump error loading %s\n", name);
  }

  // in theory, by this point the BSP is valid - now we have to create the cm_
  // structures out of the raw file data
  if (size) {
    collisionBsp.size = *size = Bsp_Size(header);
    collisionBsp.modTime = Fs_LastModTime(name);
  }

  q_strlcpy(collisionBsp.name, name, sizeof(collisionBsp.name));

  Fs_Free(header);

  Cm_LoadBspMaterials(&collisionBsp);
  Cm_LoadBspEntities(&collisionBsp);
  Cm_LoadBspPlanes(&collisionBsp);
  Cm_LoadBspNodes(&collisionBsp);
  Cm_LoadBspLeafs(&collisionBsp);
  Cm_LoadBspLeafBrushes(&collisionBsp);
  Cm_LoadBspBrushSides(&collisionBsp);
  Cm_LoadBspBrushes(&collisionBsp);
  Cm_LoadBspInlineModels(&collisionBsp);
  Cm_LoadBspVoxels(&collisionBsp);

  Cm_InitBoxHull(&collisionBsp);

  return &collisionBsp.models[0];
}

/**
 * @brief Returns the inline BSP model with the given name (e.g. "*1").
 */
CollisionModel *Cm_Model(const char *name) {

  if (!name || name[0] != '*') {
    Com_Error(ERROR_DROP, "Bad name\n");
  }

  const int32_t num = atoi(name + 1);

  if (num < 0 || num >= collisionBsp.numModels) {
    Com_Error(ERROR_DROP, "Bad number: %d\n", num);
  }

  return &collisionBsp.models[num];
}

/**
 * @brief Returns the number of inline BSP models in the loaded BSP file.
 */
int32_t Cm_NumModels(void) {
  return collisionBsp.file->numModels;
}

/**
 * @brief Returns the raw entity string from the loaded BSP file.
 */
const char *Cm_EntityString(void) {
  return collisionBsp.file->entityString;
}

/**
 * @brief Returns the worldspawn entity (first entity in the loaded BSP).
 */
const Entity *Cm_Worldspawn(void) {
  return *collisionBsp.entities;
}

/**
 * @brief Returns the contents mask for the given leaf number.
 */
int32_t Cm_LeafContents(const int32_t leafNum) {

  if (leafNum < 0 || leafNum >= collisionBsp.numLeafs) {
    Com_Error(ERROR_DROP, "Bad number: %d\n", leafNum);
  }

  return collisionBsp.leafs[leafNum].contents;
}

/**
 * @brief Returns a const pointer to the global BSP collision model.
 */
const CollisionBsp *Cm_Bsp(void) {
  return &collisionBsp;
}
