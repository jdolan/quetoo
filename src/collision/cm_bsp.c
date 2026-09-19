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
  BSP_LUMP_SIZE_STRUCT(entity_string_size, entity_string, MAX_BSP_ENTITIES_SIZE),
  BSP_LUMP_NUM_STRUCT(num_materials, materials, MAX_BSP_MATERIALS),
  BSP_LUMP_NUM_STRUCT(num_planes, planes, MAX_BSP_PLANES),
  BSP_LUMP_NUM_STRUCT(num_brush_sides, brush_sides, MAX_BSP_BRUSH_SIDES),
  BSP_LUMP_NUM_STRUCT(num_brushes, brushes, MAX_BSP_BRUSHES),
  BSP_LUMP_NUM_STRUCT(num_patches, patches, MAX_BSP_PATCHES),
  BSP_LUMP_NUM_STRUCT(num_vertexes, vertexes, MAX_BSP_VERTEXES),
  BSP_LUMP_NUM_STRUCT(num_elements, elements, MAX_BSP_ELEMENTS),
  BSP_LUMP_NUM_STRUCT(num_faces, faces, MAX_BSP_FACES),
  BSP_LUMP_NUM_STRUCT(num_nodes, nodes, MAX_BSP_NODES),
  BSP_LUMP_NUM_STRUCT(num_leaf_brushes, leaf_brushes, MAX_BSP_LEAF_BRUSHES),
  BSP_LUMP_NUM_STRUCT(num_leafs, leafs, MAX_BSP_LEAFS),
  BSP_LUMP_NUM_STRUCT(num_draw_elements, draw_elements, MAX_BSP_DRAW_ELEMENTS),
  BSP_LUMP_NUM_STRUCT(num_blocks, blocks, MAX_BSP_BLOCKS),
  BSP_LUMP_NUM_STRUCT(num_models, models, MAX_BSP_MODELS),
  BSP_LUMP_NUM_STRUCT(num_lights, lights, MAX_BSP_LIGHTS),
  BSP_LUMP_SIZE_STRUCT(voxels_size, voxels, MAX_BSP_VOXELS_SIZE),
  BSP_LUMP_NUM_STRUCT(num_light_voxels, light_voxels, MAX_BSP_LIGHT_VOXELS),
  BSP_LUMP_NUM_STRUCT(num_block_voxels, block_voxels, MAX_BSP_BLOCK_VOXELS),
  BSP_LUMP_NUM_STRUCT(num_portals, portals, MAX_BSP_PORTALS),
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

  BspBrushSide *brush_side = (BspBrushSide *) lump;

  for (int32_t i = 0; i < num; i++) {

    brush_side->plane = LittleLong(brush_side->plane);
    brush_side->material = LittleLong(brush_side->material);
    brush_side->axis[0] = LittleVec4(brush_side->axis[0]);
    brush_side->axis[1] = LittleVec4(brush_side->axis[1]);
    brush_side->contents = LittleLong(brush_side->contents);
    brush_side->surface = LittleLong(brush_side->surface);
    brush_side->value = LittleLong(brush_side->value);

    brush_side++;
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
    brush->first_brush_side = LittleLong(brush->first_brush_side);
    brush->num_brush_sides = LittleLong(brush->num_brush_sides);
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

    const int32_t num_points = patch->width * patch->height;
    for (int32_t j = 0; j < num_points; j++) {
      patch->control_points[j].position = LittleVec3(patch->control_points[j].position);
      patch->control_points[j].st = LittleVec2(patch->control_points[j].st);
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

    face->brush_side = LittleLong(face->brush_side);
    face->plane = LittleLong(face->plane);
    face->patch = LittleLong(face->patch);
    face->node = LittleLong(face->node);
    face->block = LittleLong(face->block);

    face->bounds = LittleBounds(face->bounds);

    face->first_vertex = LittleLong(face->first_vertex);
    face->num_vertexes = LittleLong(face->num_vertexes);

    face->first_element = LittleLong(face->first_element);
    face->num_elements = LittleLong(face->num_elements);

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
    node->visible_bounds = LittleBounds(node->visible_bounds);

    node->first_face = LittleLong(node->first_face);
    node->num_faces = LittleLong(node->num_faces);

    node++;
  }
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapLeafBrushes(void *lump, const int32_t num) {

  int32_t *leaf_brush = (int32_t *) lump;

  for (int32_t i = 0; i < num; i++) {
    leaf_brush[i] = LittleLong(leaf_brush[i]);
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
    leaf->first_leaf_brush = LittleLong(leaf->first_leaf_brush);
    leaf->num_leaf_brushes = LittleLong(leaf->num_leaf_brushes);

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
    draw->first_element = LittleLong(draw->first_element);
    draw->num_elements = LittleLong(draw->num_elements);

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
    block->first_draw_element = LittleLong(block->first_draw_element);
    block->num_draw_elements = LittleLong(block->num_draw_elements);
    block->visible_bounds = LittleBounds(block->visible_bounds);
    block->first_voxel = LittleLong(block->first_voxel);
    block->num_voxels = LittleLong(block->num_voxels);

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
    model->head_node = LittleLong(model->head_node);

    model->bounds = LittleBounds(model->bounds);
    model->visible_bounds = LittleBounds(model->visible_bounds);

    model->first_face = LittleLong(model->first_face);
    model->num_faces = LittleLong(model->num_faces);

    model->first_depth_pass_elements = LittleLong(model->first_depth_pass_elements);
    model->num_depth_pass_elements = LittleLong(model->num_depth_pass_elements);

    model->first_draw_elements = LittleLong(model->first_draw_elements);
    model->num_draw_elements = LittleLong(model->num_draw_elements);

    model->first_block = LittleLong(model->first_block);
    model->num_blocks = LittleLong(model->num_blocks);

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
    light->first_draw_elements = LittleLong(light->first_draw_elements);
    light->num_draw_elements = LittleLong(light->num_draw_elements);
    light->first_voxel = LittleLong(light->first_voxel);
    light->num_voxels = LittleLong(light->num_voxels);
    light++;
  }
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapPortals(void *lump, const int32_t num) {

  BspPortal *portal = (BspPortal *) lump;

  for (int32_t i = 0; i < num; i++) {
    portal->brush_side = LittleLong(portal->brush_side);
    portal->draw_elements = LittleLong(portal->draw_elements);
    portal->entry_origin = LittleVec3(portal->entry_origin);
    portal->entry_forward = LittleVec3(portal->entry_forward);
    portal->entry_up = LittleVec3(portal->entry_up);
    portal->exit_origin = LittleVec3(portal->exit_origin);
    portal->exit_forward = LittleVec3(portal->exit_forward);
    portal->exit_up = LittleVec3(portal->exit_up);
    portal++;
  }
}

/**
 * @brief Swap function.
 */
static void Bsp_SwapVoxels(void *lump, const int32_t num) {

  BspVoxels *voxel = (BspVoxels *) lump;

  voxel->size = LittleVec3i(voxel->size);
  voxel->num_light_indices = LittleLong(voxel->num_light_indices);
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
static void Bsp_SwapLump(const BspLumpId lump_id, void *lump, int32_t count) {

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

  if (swap[lump_id]) {
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
    total += LittleLong(file->lumps[lump].file_len);
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
static void Bsp_GetLumpPosition(const BspHeader *file, const BspLumpId lump_id, BspLump *lump) {

  *lump = file->lumps[lump_id];
  lump->file_len = LittleLong(lump->file_len);
  lump->file_ofs = LittleLong(lump->file_ofs);
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
static bool Bsp_GetLumpOffsets(const BspFile *bsp, const BspLumpId lump_id, int32_t **count, void ***data) {

  if (lump_id >= BSP_LUMP_LAST) {
    return false;
  }

  BspLumpMeta *meta = &bsp_lump_meta[lump_id];

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
bool Bsp_LumpLoaded(const BspFile *bsp, const BspLumpId lump_id) {

  return bsp->loaded_lumps & (BspLumpId) (1 << lump_id);
}

/**
 * @brief Unloads the specified lump from memory.
 */
void Bsp_UnloadLump(BspFile *bsp, const BspLumpId lump_id) {

  if (!Bsp_LumpLoaded(bsp, lump_id)) {
    return;
  }

  int32_t *lump_count;
  void **lump_data;

  if (!Bsp_GetLumpOffsets(bsp, lump_id, &lump_count, &lump_data)) {
    Com_Error(ERROR_DROP, "Tried to load an invalid lump (%i)\n", lump_id);
  }

  // lump is valid but we're skipping it
  if (lump_count == LUMP_SKIPPED) {
    return;
  }

  // free memory
  if (*lump_data) {
    Mem_Free(*lump_data);
    *lump_data = NULL;
  }

  *lump_count = 0;

  bsp->loaded_lumps &= ~((BspLumpId) (1 << lump_id));
}

/**
 * @brief Unloads the specified lumps from memory.
 */
void Bsp_UnloadLumps(BspFile *bsp, const BspLumpId lump_bits) {

  for (BspLumpId lump = BSP_LUMP_FIRST; lump < BSP_LUMP_LAST; lump++) {
    if (lump_bits & (BspLumpId) (1 << lump)) {
      Bsp_UnloadLump(bsp, lump);
    }
  }
}

/**
 * @brief Load a lump into memory from the specified BSP file. Returns false
 * if an error occured during the load that is recoverable.
 */
bool Bsp_LoadLump(const BspHeader *file, BspFile *bsp, const BspLumpId lump_id) {

  int32_t *lump_count;
  void **lump_data;

  if (!Bsp_GetLumpOffsets(bsp, lump_id, &lump_count, &lump_data)) {
    Com_Error(ERROR_DROP, "Tried to load an invalid lump (%i)\n", lump_id);
  }

  // lump is valid but we're skipping it
  if (lump_count == LUMP_SKIPPED) {
    return true;
  }

  // unload the lump if we're already loaded
  Bsp_UnloadLump(bsp, lump_id);

  // find the lump in the file
  BspLump lump;
  Bsp_GetLumpPosition(file, lump_id, &lump);

  const size_t lump_type_size = bsp_lump_meta[lump_id].type_size;

  if (lump.file_len < 0 || lump.file_ofs < 0) {
    Com_Error(ERROR_DROP, "Lump (%i) has invalid offset (%i) or size (%i)\n",
              lump_id, lump.file_ofs, lump.file_len);
  }

  if (lump.file_len % lump_type_size) {
    Com_Error(ERROR_DROP, "Lump (%i) size (%i) doesn't match expected data type (%" PRIuPTR ")\n",
              lump_id, lump.file_len, lump_type_size);
  }

  *lump_count = lump.file_len / lump_type_size;

  if (*lump_count >= (int32_t) bsp_lump_meta[lump_id].max_count) {
    Com_Error(ERROR_DROP, "Lump (%i) count (%i) exceeds max (%" PRIuPTR ")\n", lump_id, *lump_count,
              bsp_lump_meta[lump_id].max_count);
  }

  if (*lump_count) {
    *lump_data = Mem_TagMalloc(lump.file_len, MEM_TAG_BSP | (lump_id << 16));

    // blit the data into memory
    if (lump.file_ofs && lump.file_len) {
      const byte *src = ((const byte *) file) + lump.file_ofs;

      memcpy(*lump_data, src, lump.file_len);

      Bsp_SwapLump(lump_id, *lump_data, *lump_count);
    }
  }

  bsp->loaded_lumps |= (BspLumpId) (1 << lump_id);

  return true;
}

/**
 * @brief Loads the specified lumps into memory. If a failure occurs at any point during
 * loading, it will stop trying to load more and return false.
 */
bool Bsp_LoadLumps(const BspHeader *file, BspFile *bsp, const BspLumpId lump_bits) {

  for (BspLumpId lump = BSP_LUMP_FIRST; lump < BSP_LUMP_LAST; lump++) {
    if (lump_bits & (BspLumpId) (1 << lump)) {
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
void Bsp_AllocLump(BspFile *bsp, const BspLumpId lump_id, const size_t count) {

  // mark as loaded
  if (!Bsp_LumpLoaded(bsp, lump_id)) {
    bsp->loaded_lumps |= (BspLumpId) (1 << lump_id);
  }

  int32_t *lump_count;
  void **lump_data;

  if (!Bsp_GetLumpOffsets(bsp, lump_id, &lump_count, &lump_data)) {
    Com_Error(ERROR_DROP, "Tried to allocate an invalid lump (%i)\n", lump_id);
  }

  // lump is valid but we're skipping it
  if (lump_count == LUMP_SKIPPED) {
    return;
  }

  // calculate size
  const size_t lump_type_size = bsp_lump_meta[lump_id].type_size;

  const size_t old_count = (size_t) *lump_count;

  *lump_data = Mem_Realloc(*lump_data, lump_type_size * count);

  // Mem_Realloc does not zero-initialize newly grown memory, unlike the initial
  // allocation (which uses calloc). Zero it explicitly so that callers who grow a
  // lump after populating it (e.g. quemap's -light stage growing the draw elements
  // lump after loading it from the -bsp/-vis stage) don't read garbage for fields
  // that are accumulated in place (e.g. `num_elements += ...`).
  if (count > old_count) {
    uint8_t *data = (uint8_t *) *lump_data;
    memset(data + old_count * lump_type_size, 0, (count - old_count) * lump_type_size);
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
  int64_t header_pos = Fs_Tell(file);
  Fs_Write(file, &header, sizeof(header), 1);

  // write out the lumps now

  int64_t current_position = Fs_Tell(file);
  memset(header.lumps, 0, sizeof(header.lumps));

  for (BspLumpId lump = BSP_LUMP_FIRST; lump < BSP_LUMP_LAST; lump++) {

    int32_t *lump_count;
    void **lump_data;

    const size_t size = bsp_lump_meta[lump].type_size;

    Bsp_GetLumpOffsets(bsp, lump, &lump_count, &lump_data);

    // lump is valid but we're skipping it
    if (lump_count == LUMP_SKIPPED || *lump_count == 0) {
      continue;
    }

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
    // swap lump to disk endianness
    if (bsp_swap_funcs[i]) {
      bsp_swap_funcs[i](*lump_data, *lump_count);
    }
#endif

    // write and increase position for next lump
    const int64_t len = Fs_Write(file, *lump_data, size, *lump_count);
    const int64_t lump_size = (int32_t) (len * size);

    header.lumps[lump].file_len = LittleLong((int32_t) lump_size);
    header.lumps[lump].file_ofs = LittleLong((int32_t) current_position);

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
    // swap back to memory endianness
    if (bsp_swap_funcs[i]) {
      bsp_swap_funcs[i](*lump_data, *lump_count);
    }
#endif

    current_position += lump_size;
  }

  // go back and write the finished header
  Fs_Seek(file, header_pos);
  Fs_Write(file, &header, sizeof(header), 1);

  // return to where we were
  Fs_Seek(file, current_position);
}
