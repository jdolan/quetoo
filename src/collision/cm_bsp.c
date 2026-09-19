#include "cm_local.h"

/**
 * @brief Metadata for BSP lumps
 */
typedef struct {

  /**
   * @brief Offset into `BspFile` to the lump element count.
   */
  size_t count_ofs;

  /**
   * @brief Offset into `BspFile` to the lump data pointer.
   */
  size_t data_ofs;

  /**
   * @brief Size in bytes of each lump element.
   */
  size_t type_size;

  /**
   * @brief Maximum allowed element count for this lump.
   */
  size_t max_count;
} BspLumpMeta;

#if !defined(BSP_SIZEOF)
#define BSP_SIZEOF(T, F) \
sizeof(*((T *) 0)->F)
#endif

#define BSP_LUMP_NUM_STRUCT(c, n, m) { \
.count_ofs = offsetof(BspFile, c), \
.data_ofs = offsetof(BspFile, n), \
.type_size = BSP_SIZEOF(BspFile, n), \
.max_count = m \
}

#define BSP_LUMP_SIZE_STRUCT(c, n, m) { \
.count_ofs = offsetof(BspFile, c), \
.data_ofs = offsetof(BspFile, n),\
.type_size = sizeof(byte), \
.max_count = m \
}

#define BSP_LUMP_SKIP { 0, 0, 0, 0 }

static BspLumpMeta bsp_lump_meta[BSP_LUMP_LAST] = {
  BSP_LUMP_SIZE_STRUCT(entityStringSize, entityString, MAX_BSP_ENTITIES_SIZE),
  BSP_LUMP_NUM_STRUCT(numMaterials, materials, MAX_BSP_MATERIALS),
  BSP_LUMP_NUM_STRUCT(numPlanes, planes, MAX_BSP_PLANES),
  BSP_LUMP_NUM_STRUCT(numBrushSides, brushSides, MAX_BSP_BRUSH_SIDES),
  BSP_LUMP_NUM_STRUCT(numBrushes, brushes, MAX_BSP_BRUSHES),
  BSP_LUMP_NUM_STRUCT(numPatches, patches, MAX_BSP_PATCHES),
  BSP_LUMP_NUM_STRUCT(numVertexes, vertexes, MAX_BSP_VERTEXES),
  BSP_LUMP_NUM_STRUCT(numElements, elements, MAX_BSP_ELEMENTS),
  BSP_LUMP_NUM_STRUCT(numFaces, faces, MAX_BSP_FACES),
  BSP_LUMP_NUM_STRUCT(numNodes, nodes, MAX_BSP_NODES),
  BSP_LUMP_NUM_STRUCT(numLeafBrushes, leafBrushes, MAX_BSP_LEAF_BRUSHES),
  BSP_LUMP_NUM_STRUCT(numLeafs, leafs, MAX_BSP_LEAFS),
  BSP_LUMP_NUM_STRUCT(numDrawElements, drawElements, MAX_BSP_DRAW_ELEMENTS),
  BSP_LUMP_NUM_STRUCT(numBlocks, blocks, MAX_BSP_BLOCKS),
  BSP_LUMP_NUM_STRUCT(numModels, models, MAX_BSP_MODELS),
  BSP_LUMP_NUM_STRUCT(numLights, lights, MAX_BSP_LIGHTS),
  BSP_LUMP_SIZE_STRUCT(voxelsSize, voxels, MAX_BSP_VOXELS_SIZE),
  BSP_LUMP_NUM_STRUCT(numLightVoxels, lightVoxels, MAX_BSP_LIGHT_VOXELS),
  BSP_LUMP_NUM_STRUCT(numBlockVoxels, blockVoxels, MAX_BSP_BLOCK_VOXELS),
  BSP_LUMP_NUM_STRUCT(numPortals, portals, MAX_BSP_PORTALS),
};

/**
 * @brief Table of swap functions.
 */
typedef void (*Bsp_SwapFunction) (void *lump, const int32_t num);

/**
 * @brief Swap function.
 */
static void Bsp_SwapPlanes(void *lump, const int32_t num) {

  BspPlane *plane = (BspPlane *) lump;

  for (int32_t i = 0; i < num; i++) {

    plane->normal = LittleVec3(plane->normal);
    plane->dist = LittleFloat(plane->dist);

    plane++;
  }
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapBrushSides(void *lump, const int32_t num) {

  BspBrushSide *brushSide = (BspBrushSide *) lump;

  for (int32_t i = 0; i < num; i++) {

    brushSide->plane = LittleLong(brushSide->plane);
    brushSide->material = LittleLong(brushSide->material);
    brushSide->axis[0] = LittleVec4(brushSide->axis[0]);
    brushSide->axis[1] = LittleVec4(brushSide->axis[1]);
    brushSide->contents = LittleLong(brushSide->contents);
    brushSide->surface = LittleLong(brushSide->surface);
    brushSide->value = LittleLong(brushSide->value);

    brushSide++;
  }
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapBrushes(void *lump, const int32_t num) {

  BspBrush *brush = (BspBrush *) lump;

  for (int32_t i = 0; i < num; i++) {

    brush->entity = LittleLong(brush->entity);
    brush->contents = LittleLong(brush->contents);
    brush->firstBrushSide = LittleLong(brush->firstBrushSide);
    brush->numBrushSides = LittleLong(brush->numBrushSides);
    brush->bounds = LittleBounds(brush->bounds);

    brush++;
  }
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapPatches(void *lump, const int32_t num) {

  BspPatch *patch = (BspPatch *) lump;

  for (int32_t i = 0; i < num; i++) {

    patch->entity = LittleLong(patch->entity);
    patch->material = LittleLong(patch->material);
    patch->contents = LittleLong(patch->contents);
    patch->surface = LittleLong(patch->surface);
    patch->width = LittleLong(patch->width);
    patch->height = LittleLong(patch->height);

    if (patch->width < 1 || patch->height < 1) {
      Com_Error(ERROR_DROP, "MIN_PATCH_SIZE\n");
    }

    if (patch->width > MAX_PATCH_SIZE || patch->height > MAX_PATCH_SIZE) {
      Com_Error(ERROR_DROP, "MAX_PATCH_SIZE\n");
    }

    const int32_t numPoints = patch->width * patch->height;
    for (int32_t j = 0; j < numPoints; j++) {
      patch->controlPoints[j].position = LittleVec3(patch->controlPoints[j].position);
      patch->controlPoints[j].st = LittleVec2(patch->controlPoints[j].st);
    }

    patch++;
  }
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapVertexes(void *lump, const int32_t num) {

  BspVertex *vertex = (BspVertex *) lump;

  for (int32_t i = 0; i < num; i++) {

    vertex->position = LittleVec3(vertex->position);
    vertex->normal = LittleVec3(vertex->normal);
    vertex->tangent = LittleVec3(vertex->tangent);
    vertex->bitangent = LittleVec3(vertex->bitangent);
    vertex->diffusemap = LittleVec2(vertex->diffusemap);
    vertex->color.rgba = LittleLong(vertex->color.rgba);

    vertex++;
  }
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapElements(void *lump, const int32_t num) {

  int32_t *element = (int32_t *) lump;

  for (int32_t i = 0; i < num; i++) {
    element[i] = LittleLong(element[i]);
  }
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapFaces(void *lump, const int32_t num) {

  BspFace *face = (BspFace *) lump;

  for (int32_t i = 0; i < num; i++) {

    face->brushSide = LittleLong(face->brushSide);
    face->plane = LittleLong(face->plane);
    face->patch = LittleLong(face->patch);
    face->node = LittleLong(face->node);
    face->block = LittleLong(face->block);

    face->bounds = LittleBounds(face->bounds);

    face->firstVertex = LittleLong(face->firstVertex);
    face->numVertexes = LittleLong(face->numVertexes);

    face->firstElement = LittleLong(face->firstElement);
    face->numElements = LittleLong(face->numElements);

    face++;
  }
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapNodes(void *lump, const int32_t num) {

  BspNode *node = (BspNode *) lump;

  for (int32_t i = 0; i < num; i++) {

    node->plane = LittleLong(node->plane);
    node->children[0] = LittleLong(node->children[0]);
    node->children[1] = LittleLong(node->children[1]);
    node->contents = LittleLong(node->contents);

    node->bounds = LittleBounds(node->bounds);
    node->visibleBounds = LittleBounds(node->visibleBounds);

    node->firstFace = LittleLong(node->firstFace);
    node->numFaces = LittleLong(node->numFaces);

    node++;
  }
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapLeafBrushes(void *lump, const int32_t num) {

  int32_t *leafBrush = (int32_t *) lump;

  for (int32_t i = 0; i < num; i++) {
    leafBrush[i] = LittleLong(leafBrush[i]);
  }
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapLeafs(void *lump, const int32_t num) {

  BspLeaf *leaf = (BspLeaf *) lump;

  for (int32_t i = 0; i < num; i++) {

    leaf->contents = LittleLong(leaf->contents);
    leaf->bounds = LittleBounds(leaf->bounds);
    leaf->firstLeafBrush = LittleLong(leaf->firstLeafBrush);
    leaf->numLeafBrushes = LittleLong(leaf->numLeafBrushes);

    leaf++;
  }
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapDrawElements(void *lump, const int32_t num) {

  BspDrawElements *draw = (BspDrawElements *) lump;

  for (int32_t i = 0; i < num; i++) {

    draw->material = LittleLong(draw->material);
    draw->surface = LittleLong(draw->surface);
    draw->bounds = LittleBounds(draw->bounds);
    draw->firstElement = LittleLong(draw->firstElement);
    draw->numElements = LittleLong(draw->numElements);

    draw++;
  }
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapBlocks(void *lump, const int32_t num) {

  BspBlock *block = (BspBlock *) lump;

  for (int32_t i = 0; i < num; i++) {

    block->node = LittleLong(block->node);
    block->firstDrawElement = LittleLong(block->firstDrawElement);
    block->numDrawElements = LittleLong(block->numDrawElements);
    block->visibleBounds = LittleBounds(block->visibleBounds);
    block->firstVoxel = LittleLong(block->firstVoxel);
    block->numVoxels = LittleLong(block->numVoxels);

    block++;
  }
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapModels(void *lump, const int32_t num) {

  BspModel *model = (BspModel *) lump;

  for (int32_t i = 0; i < num; i++) {

    model->entity = LittleLong(model->entity);
    model->headNode = LittleLong(model->headNode);

    model->bounds = LittleBounds(model->bounds);
    model->visibleBounds = LittleBounds(model->visibleBounds);

    model->firstFace = LittleLong(model->firstFace);
    model->numFaces = LittleLong(model->numFaces);

    model->firstDepthPassElements = LittleLong(model->firstDepthPassElements);
    model->numDepthPassElements = LittleLong(model->numDepthPassElements);

    model->firstDrawElements = LittleLong(model->firstDrawElements);
    model->numDrawElements = LittleLong(model->numDrawElements);

    model->firstBlock = LittleLong(model->firstBlock);
    model->numBlocks = LittleLong(model->numBlocks);

    model++;
  }
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapLights(void *lump, const int32_t num) {

  BspLight *light = (BspLight *) lump;

  for (int32_t i = 0; i < num; i++) {
    light->entity = LittleLong(light->entity);
    light->origin = LittleVec3(light->origin);
    light->radius = LittleFloat(light->radius);
    light->color = LittleVec3(light->color);
    light->intensity = LittleFloat(light->intensity);
    light->bounds = LittleBounds(light->bounds);
    light->drift = LittleFloat(light->drift);
    light->firstDrawElements = LittleLong(light->firstDrawElements);
    light->numDrawElements = LittleLong(light->numDrawElements);
    light->firstVoxel = LittleLong(light->firstVoxel);
    light->numVoxels = LittleLong(light->numVoxels);
    light++;
  }
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapPortals(void *lump, const int32_t num) {

  BspPortal *portal = (BspPortal *) lump;

  for (int32_t i = 0; i < num; i++) {
    portal->brushSide = LittleLong(portal->brushSide);
    portal->drawElements = LittleLong(portal->drawElements);
    portal->entryOrigin = LittleVec3(portal->entryOrigin);
    portal->entryForward = LittleVec3(portal->entryForward);
    portal->entryUp = LittleVec3(portal->entryUp);
    portal->exitOrigin = LittleVec3(portal->exitOrigin);
    portal->exitForward = LittleVec3(portal->exitForward);
    portal->exitUp = LittleVec3(portal->exitUp);
    portal++;
  }
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapVoxels(void *lump, const int32_t num) {

  BspVoxels *voxel = (BspVoxels *) lump;

  voxel->size = LittleVec3i(voxel->size);
  voxel->numLightIndices = LittleLong(voxel->numLightIndices);
  voxel->bounds = LittleBounds(voxel->bounds);
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapLightVoxels(void *lump, const int32_t num) {

  int32_t *voxel = (int32_t *) lump;

  for (int32_t i = 0; i < num; i++) {
    voxel[i] = LittleLong(voxel[i]);
  }
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapBlockVoxels(void *lump, const int32_t num) {

  int32_t *voxel = (int32_t *) lump;

  for (int32_t i = 0; i < num; i++) {
    voxel[i] = LittleLong(voxel[i]);
  }
}

/**
 * @brief Swap entry point.
 */
static void Bsp_SwapLump(const BspLumpId lumpId, void *lump, int32_t count) {

  const Bsp_SwapFunction swap[BSP_LUMP_LAST] = {
    NULL,
    NULL,
    Bsp_SwapPlanes,
    Bsp_SwapBrushSides,
    Bsp_SwapBrushes,
    Bsp_SwapPatches,
    Bsp_SwapVertexes,
    Bsp_SwapElements,
    Bsp_SwapFaces,
    Bsp_SwapNodes,
    Bsp_SwapLeafBrushes,
    Bsp_SwapLeafs,
    Bsp_SwapDrawElements,
    Bsp_SwapBlocks,
    Bsp_SwapModels,
    Bsp_SwapLights,
    Bsp_SwapVoxels,
    Bsp_SwapLightVoxels,
    Bsp_SwapBlockVoxels,
    Bsp_SwapPortals,
  };

  if (swap[lumpId]) {
#if SDL_BYTEORDER != SDL_LIL_ENDIAN
    swap[lump_id](lump, count);
#endif
  }
}

/**
 * @brief Calculates the effective size of the BSP file.
 */
int64_t Bsp_Size(const BspHeader *file) {
  int64_t total = 0;

  for (BspLumpId lump = BSP_LUMP_FIRST; lump < BSP_LUMP_LAST; lump++) {
    total += LittleLong(file->lumps[lump].fileLen);
  }

  return total;
}

/**
 * @brief Verifies that a file is indeed a BSP file and returns
 * the version number. Returns -1 if it is not a valid BSP.
 */
int32_t Bsp_Verify(const BspHeader *file) {

  if (LittleLong(file->ident) != BSP_IDENT) {
    return -1;
  }

  if (LittleLong(file->version) != BSP_VERSION) {
    return -1;
  }

  return BSP_VERSION;
}

/**
 * @brief Read the lump length/offset from the BSP file.
 */
static void Bsp_GetLumpPosition(const BspHeader *file, const BspLumpId lumpId, BspLump *lump) {

  *lump = file->lumps[lumpId];
  lump->fileLen = LittleLong(lump->fileLen);
  lump->fileOfs = LittleLong(lump->fileOfs);
}

/**
 * @brief Set if the lump is valid but the ptrs do not exist.
 */
#define LUMP_SKIPPED (int32_t *) ((ptrdiff_t) -1u)

/**
 * @brief Convenience to calculate `bsp_file` offset in bytes.
 */
#define BSP_BYTE_OFFSET(bsp, bytes) (((intptr_t) bsp) + bytes)

/**
 * @brief Get the lump offset data for the specified lump. Returns false if the
 * lump is not valid. count and data will be filled with the pointer to the lump's
 * count and data pointers in memory. They may be empty. If count is `LUMP_SKIPPED`,
 * the lump is a valid lump but not stored/used by the library.
 */
static bool Bsp_GetLumpOffsets(const BspFile *bsp, const BspLumpId lumpId, int32_t **count, void ***data) {

  if (lumpId >= BSP_LUMP_LAST) {
    return false;
  }

  BspLumpMeta *meta = &bsp_lump_meta[lumpId];

  if (!meta->type_size) {

    if (count) {
      *count = LUMP_SKIPPED;
    }

  } else {

    if (count) {
      *count = (int32_t *) BSP_BYTE_OFFSET(bsp, meta->count_ofs);
    }

    if (data) {
      *data = (void **) BSP_BYTE_OFFSET(bsp, meta->data_ofs);
    }
  }

  return true;
}

/**
 * @brief Check whether the specified lump is loaded in memory or not.
 */
bool Bsp_LumpLoaded(const BspFile *bsp, const BspLumpId lumpId) {

  return bsp->loadedLumps & (BspLumpId) (1 << lumpId);
}

/**
 * @brief Unloads the specified lump from memory.
 */
void Bsp_UnloadLump(BspFile *bsp, const BspLumpId lumpId) {

  if (!Bsp_LumpLoaded(bsp, lumpId)) {
    return;
  }

  int32_t *lumpCount;
  void **lumpData;

  if (!Bsp_GetLumpOffsets(bsp, lumpId, &lumpCount, &lumpData)) {
    Com_Error(ERROR_DROP, "Tried to load an invalid lump (%i)\n", lumpId);
  }

  // lump is valid but we're skipping it
  if (lumpCount == LUMP_SKIPPED) {
    return;
  }

  // free memory
  if (*lumpData) {
    Mem_Free(*lumpData);
    *lumpData = NULL;
  }

  *lumpCount = 0;

  bsp->loadedLumps &= ~((BspLumpId) (1 << lumpId));
}

/**
 * @brief Unloads the specified lumps from memory.
 */
void Bsp_UnloadLumps(BspFile *bsp, const BspLumpId lumpBits) {

  for (BspLumpId lump = BSP_LUMP_FIRST; lump < BSP_LUMP_LAST; lump++) {
    if (lumpBits & (BspLumpId) (1 << lump)) {
      Bsp_UnloadLump(bsp, lump);
    }
  }
}

/**
 * @brief Load a lump into memory from the specified BSP file. Returns false
 * if an error occured during the load that is recoverable.
 */
bool Bsp_LoadLump(const BspHeader *file, BspFile *bsp, const BspLumpId lumpId) {

  int32_t *lumpCount;
  void **lumpData;

  if (!Bsp_GetLumpOffsets(bsp, lumpId, &lumpCount, &lumpData)) {
    Com_Error(ERROR_DROP, "Tried to load an invalid lump (%i)\n", lumpId);
  }

  // lump is valid but we're skipping it
  if (lumpCount == LUMP_SKIPPED) {
    return true;
  }

  // unload the lump if we're already loaded
  Bsp_UnloadLump(bsp, lumpId);

  // find the lump in the file
  BspLump lump;
  Bsp_GetLumpPosition(file, lumpId, &lump);

  const size_t lumpTypeSize = bsp_lump_meta[lumpId].type_size;

  if (lump.fileLen < 0 || lump.fileOfs < 0) {
    Com_Error(ERROR_DROP, "Lump (%i) has invalid offset (%i) or size (%i)\n",
              lumpId, lump.fileOfs, lump.fileLen);
  }

  if (lump.fileLen % lumpTypeSize) {
    Com_Error(ERROR_DROP, "Lump (%i) size (%i) doesn't match expected data type (%" PRIuPTR ")\n",
              lumpId, lump.fileLen, lumpTypeSize);
  }

  *lumpCount = lump.fileLen / lumpTypeSize;

  if (*lumpCount >= (int32_t) bsp_lump_meta[lumpId].max_count) {
    Com_Error(ERROR_DROP, "Lump (%i) count (%i) exceeds max (%" PRIuPTR ")\n", lumpId, *lumpCount,
              bsp_lump_meta[lumpId].max_count);
  }

  if (*lumpCount) {
    *lumpData = Mem_TagMalloc(lump.fileLen, MEM_TAG_BSP | (lumpId << 16));

    // blit the data into memory
    if (lump.fileOfs && lump.fileLen) {
      const byte *src = ((const byte *) file) + lump.fileOfs;

      memcpy(*lumpData, src, lump.fileLen);

      Bsp_SwapLump(lumpId, *lumpData, *lumpCount);
    }
  }

  bsp->loadedLumps |= (BspLumpId) (1 << lumpId);

  return true;
}

/**
 * @brief Loads the specified lumps into memory. If a failure occurs at any point during
 * loading, it will stop trying to load more and return false.
 */
bool Bsp_LoadLumps(const BspHeader *file, BspFile *bsp, const BspLumpId lumpBits) {

  for (BspLumpId lump = BSP_LUMP_FIRST; lump < BSP_LUMP_LAST; lump++) {
    if (lumpBits & (BspLumpId) (1 << lump)) {
      if (!Bsp_LoadLump(file, bsp, lump)) {
        return false;
      }
    }
  }

  return true;
}

/**
 * @brief Allocates data for the specified lump in the BSP. If the lump is already loaded,
 * the data will either be expanded or truncated to the specified count. Note that `count`
 * is not in bytes, but rather the number of components to allocate - this depends on the
 * `lump_id` (for instance, the `VERTEXES` lump will allocate `count * BspVertex`). This
 * will count as loading a lump as well. Since realloc is used here, be careful that you
 * aren't storing a pointer to the old lump data. The lump size pointer (as in, `num_x` or
 * `x_size`) won't be modified by this function call, so be careful!
 */
void Bsp_AllocLump(BspFile *bsp, const BspLumpId lumpId, const size_t count) {

  // mark as loaded
  if (!Bsp_LumpLoaded(bsp, lumpId)) {
    bsp->loadedLumps |= (BspLumpId) (1 << lumpId);
  }

  int32_t *lumpCount;
  void **lumpData;

  if (!Bsp_GetLumpOffsets(bsp, lumpId, &lumpCount, &lumpData)) {
    Com_Error(ERROR_DROP, "Tried to allocate an invalid lump (%i)\n", lumpId);
  }

  // lump is valid but we're skipping it
  if (lumpCount == LUMP_SKIPPED) {
    return;
  }

  // calculate size
  const size_t lumpTypeSize = bsp_lump_meta[lumpId].type_size;

  const size_t oldCount = (size_t) *lumpCount;

  *lumpData = Mem_Realloc(*lumpData, lumpTypeSize * count);

  // Mem_Realloc does not zero-initialize newly grown memory, unlike the initial
  // allocation (which uses calloc). Zero it explicitly so that callers who grow a
  // lump after populating it (e.g. quemap's -light stage growing the draw elements
  // lump after loading it from the -bsp/-vis stage) don't read garbage for fields
  // that are accumulated in place (e.g. `num_elements += ...`).
  if (count > oldCount) {
    uint8_t *data = (uint8_t *) *lumpData;
    memset(data + oldCount * lumpTypeSize, 0, (count - oldCount) * lumpTypeSize);
  }
}

/**
 * @brief Writes the specified BSP to the file. This will write from the current
 * position of the file.
 */
void Bsp_Write(File *file, const BspFile *bsp) {

  // create the header
  BspHeader header;

  header.ident = LittleLong(BSP_IDENT);
  header.version = LittleLong(BSP_VERSION);

  // store where we are, write what we got
  int64_t headerPos = Fs_Tell(file);
  Fs_Write(file, &header, sizeof(header), 1);

  // write out the lumps now

  int64_t currentPosition = Fs_Tell(file);
  memset(header.lumps, 0, sizeof(header.lumps));

  for (BspLumpId lump = BSP_LUMP_FIRST; lump < BSP_LUMP_LAST; lump++) {

    int32_t *lumpCount;
    void **lumpData;

    const size_t size = bsp_lump_meta[lump].type_size;

    Bsp_GetLumpOffsets(bsp, lump, &lumpCount, &lumpData);

    // lump is valid but we're skipping it
    if (lumpCount == LUMP_SKIPPED || *lumpCount == 0) {
      continue;
    }

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
    // swap lump to disk endianness
    if (bsp_swap_funcs[i]) {
      bsp_swap_funcs[i](*lump_data, *lump_count);
    }
#endif

    // write and increase position for next lump
    const int64_t len = Fs_Write(file, *lumpData, size, *lumpCount);
    const int64_t lumpSize = (int32_t) (len * size);

    header.lumps[lump].fileLen = LittleLong((int32_t) lumpSize);
    header.lumps[lump].fileOfs = LittleLong((int32_t) currentPosition);

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
    // swap back to memory endianness
    if (bsp_swap_funcs[i]) {
      bsp_swap_funcs[i](*lump_data, *lump_count);
    }
#endif

    currentPosition += lumpSize;
  }

  // go back and write the finished header
  Fs_Seek(file, headerPos);
  Fs_Write(file, &header, sizeof(header), 1);

  // return to where we were
  Fs_Seek(file, currentPosition);
}
