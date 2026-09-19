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

#include "cm_local.h"

CmBsp cm_bsp = {};

/**
 * @brief Loads and parses the entity string lump into `cm_bsp`.entities`.
 */
static void Cm_LoadBspEntities(CmBsp *bsp) {

  List *entities = Cm_LoadEntities(bsp->file->entity_string);

  bsp->num_entities = (int32_t) entities->count;
  bsp->entities = Mem_TagMalloc(sizeof(CmEntity *) * bsp->num_entities, MEM_TAG_COLLISION);

  CmEntity **out = bsp->entities;
  for (const ListNode *node = entities->head; node; node = node->next, out++) {
    *out = node->element;
  }

  release(entities);
}

/**
 * @brief Loads and converts the planes lump into `cm_bsp`.planes`.
 */
static void Cm_LoadBspPlanes(CmBsp *bsp) {

  bsp->num_planes = bsp->file->num_planes;
  const BspPlane *in = bsp->file->planes;

  CmBspPlane *out = bsp->planes = Mem_TagMalloc(sizeof(CmBspPlane) * (bsp->num_planes + 12), MEM_TAG_COLLISION); // extra for box hull

  for (int32_t i = 0; i < bsp->num_planes; i++, in++, out++) {
    *out = Cm_Plane(in->normal, in->dist);
  }
}

/**
 * @brief Loads and converts the nodes lump into `cm_bsp`.nodes`.
 */
static void Cm_LoadBspNodes(CmBsp *bsp) {

  bsp->num_nodes = bsp->file->num_nodes;
  const BspNode *in = bsp->file->nodes;

  CmBspNode *out = bsp->nodes = Mem_TagMalloc(sizeof(CmBspNode) * (bsp->num_nodes + 6), MEM_TAG_COLLISION); // extra for box hull

  for (int32_t i = 0; i < bsp->num_nodes; i++, in++, out++) {

    out->plane = bsp->planes + in->plane;

    for (int32_t j = 0; j < 2; j++) {
      out->children[j] = in->children[j];
    }
  }
}


/**
 * @brief Loads and converts the leafs lump into `cm_bsp`.leafs`.
 */
static void Cm_LoadBspLeafs(CmBsp *bsp) {

  bsp->num_leafs = bsp->file->num_leafs;
  const BspLeaf *in = bsp->file->leafs;

  CmBspLeaf *out = bsp->leafs = Mem_TagMalloc(sizeof(CmBspLeaf) * (bsp->num_leafs + 1), MEM_TAG_COLLISION); // extra for box hull

  for (int32_t i = 0; i < bsp->num_leafs; i++, in++, out++) {
    out->contents = in->contents;
    out->first_leaf_brush = in->first_leaf_brush;
    out->num_leaf_brushes = in->num_leaf_brushes;
  }
}

/**
 * @brief Loads the leaf-brush index lump into `cm_bsp`.`leaf_brushes`.
 */
static void Cm_LoadBspLeafBrushes(CmBsp *bsp) {

  bsp->num_leaf_brushes = bsp->file->num_leaf_brushes;
  const int32_t *in = bsp->file->leaf_brushes;

  int32_t *out = bsp->leaf_brushes = Mem_TagMalloc(sizeof(int32_t) * (bsp->num_leaf_brushes + 1), MEM_TAG_COLLISION); // extra for box hull

  for (int32_t i = 0; i < bsp->num_leaf_brushes; i++, in++, out++) {
    *out = *in;
  }
}

/**
 * @brief Loads and converts the brush sides lump into `cm_bsp`.`brush_sides`.
 */
static void Cm_LoadBspBrushSides(CmBsp *bsp) {

  bsp->num_brush_sides = bsp->file->num_brush_sides;
  const BspBrushSide *in = bsp->file->brush_sides;

  CmBspBrushSide *out = bsp->brush_sides = Mem_TagMalloc(sizeof(CmBspBrushSide) *
        (bsp->num_brush_sides + 6), MEM_TAG_COLLISION); // extra for box hull

  for (int32_t i = 0; i < bsp->num_brush_sides; i++, in++, out++) {

    const int32_t p = in->plane;

    if (p >= bsp->num_planes) {
      Com_Error(ERROR_DROP, "Brush side %d has invalid plane %d\n", i, p);
    }

    out->plane = &bsp->planes[p];

    if (in->material > -1) {
      if (in->material >= bsp->num_materials) {
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
 * @brief Loads and converts the brushes lump into `cm_bsp`.brushes`.
 */
static void Cm_LoadBspBrushes(CmBsp *bsp) {

  bsp->num_brushes = bsp->file->num_brushes;
  const BspBrush *in = bsp->file->brushes;

  CmBspBrush *out = bsp->brushes = Mem_TagMalloc(sizeof(CmBspBrush) * (bsp->num_brushes + 1), MEM_TAG_COLLISION); // extra for box hull

  for (int32_t i = 0; i < bsp->num_brushes; i++, in++, out++) {

    if (in->entity < 0 || in->entity >= bsp->num_entities) {
      Com_Warn("Brush %d: invalid entity index %d\n", i, in->entity);
      out->entity = NULL;
    } else {
      out->entity = bsp->entities[in->entity];
    }
    out->contents = in->contents;
    out->brush_sides = bsp->brush_sides + in->first_brush_side;
    out->num_brush_sides = in->num_brush_sides;
    out->bounds = in->bounds;
  }
}

/**
 * @brief Loads and converts the inline models lump into `cm_bsp`.models`.
 */
static void Cm_LoadBspInlineModels(CmBsp *bsp) {

  bsp->num_models = bsp->file->num_models;
  const BspModel *in = bsp->file->models;

  CmBspModel *out = bsp->models = Mem_TagMalloc(sizeof(CmBspModel) * bsp->num_models, MEM_TAG_COLLISION);

  for (int32_t i = 0; i < bsp->num_models; i++, in++, out++) {
    
    if (in->entity < 0 || in->entity >= bsp->num_entities) {
      Com_Warn("Model %d: invalid entity index %d\n", i, in->entity);
      out->entity = NULL;
    } else {
      out->entity = bsp->entities[in->entity];
    }
    out->head_node = in->head_node;
    out->bounds = in->bounds;
  }
}

/**
 * @brief Loads and resolves materials referenced by the BSP into `cm_bsp`.materials`.
 */
static void Cm_LoadBspMaterials(CmBsp *bsp) {

  bsp->num_materials = bsp->file->num_materials;

  CmMaterial **out = bsp->materials = Mem_TagMalloc(sizeof(CmMaterial *) * bsp->num_materials, MEM_TAG_COLLISION);

  const BspMaterial *in = bsp->file->materials;
  for (int32_t i = 0; i < bsp->num_materials; i++, in++, out++) {

    *out = Cm_LoadMaterial(in->name, ASSET_CONTEXT_TEXTURES);

    *out = Mem_Link(*out, bsp->materials);
  }
}

/**
 * @brief Decodes the voxel lump into a flat CmVoxel array on the CmBsp.
 */
static void Cm_LoadBspVoxels(CmBsp *bsp) {

  if (!bsp->file->voxels) {
    return;
  }

  const BspVoxels *v = bsp->file->voxels;

  bsp->voxel_size = v->size;
  bsp->voxel_bounds = v->bounds;
  bsp->num_voxels = v->size.x * v->size.y * v->size.z;

  CmVoxel *out = bsp->voxels = Mem_TagMalloc(sizeof(CmVoxel) * bsp->num_voxels, MEM_TAG_COLLISION);

  const byte *rgb = (const byte *) (v + 1);
  const byte *occlusion = rgb +
    bsp->num_voxels * 3 +
    bsp->num_voxels * (int32_t) sizeof(int32_t) * 2 +
    (size_t) v->num_light_indices * sizeof(int32_t);

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
CmBspModel *Cm_LoadBspModel(const char *name, int64_t *size) {
  static BspFile file;

  Bsp_UnloadLumps(&file, BSP_LUMPS_ALL);

  // free dynamic memory
  Mem_Free(cm_bsp.planes);
  Mem_Free(cm_bsp.nodes);
  Mem_Free(cm_bsp.leafs);
  Mem_Free(cm_bsp.leaf_brushes);
  Mem_Free(cm_bsp.brushes);
  Mem_Free(cm_bsp.brush_sides);
  Mem_Free(cm_bsp.models);
  Mem_Free(cm_bsp.entities);
  Mem_Free(cm_bsp.materials);
  Mem_Free(cm_bsp.voxels);

  memset(&cm_bsp, 0, sizeof(cm_bsp));
  cm_bsp.file = &file;

  // clean up and return
  if (!name) {
    if (size) {
      *size = 0;
    }
    return &cm_bsp.models[0];
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
    cm_bsp.size = *size = Bsp_Size(header);
    cm_bsp.mod_time = Fs_LastModTime(name);
  }

  q_strlcpy(cm_bsp.name, name, sizeof(cm_bsp.name));

  Fs_Free(header);

  Cm_LoadBspMaterials(&cm_bsp);
  Cm_LoadBspEntities(&cm_bsp);
  Cm_LoadBspPlanes(&cm_bsp);
  Cm_LoadBspNodes(&cm_bsp);
  Cm_LoadBspLeafs(&cm_bsp);
  Cm_LoadBspLeafBrushes(&cm_bsp);
  Cm_LoadBspBrushSides(&cm_bsp);
  Cm_LoadBspBrushes(&cm_bsp);
  Cm_LoadBspInlineModels(&cm_bsp);
  Cm_LoadBspVoxels(&cm_bsp);

  Cm_InitBoxHull(&cm_bsp);

  return &cm_bsp.models[0];
}

/**
 * @brief Returns the inline BSP model with the given name (e.g. "*1").
 */
CmBspModel *Cm_Model(const char *name) {

  if (!name || name[0] != '*') {
    Com_Error(ERROR_DROP, "Bad name\n");
  }

  const int32_t num = atoi(name + 1);

  if (num < 0 || num >= cm_bsp.num_models) {
    Com_Error(ERROR_DROP, "Bad number: %d\n", num);
  }

  return &cm_bsp.models[num];
}

/**
 * @brief Returns the number of inline BSP models in the loaded BSP file.
 */
int32_t Cm_NumModels(void) {
  return cm_bsp.file->num_models;
}

/**
 * @brief Returns the raw entity string from the loaded BSP file.
 */
const char *Cm_EntityString(void) {
  return cm_bsp.file->entity_string;
}

/**
 * @brief Returns the worldspawn entity (first entity in the loaded BSP).
 */
const CmEntity *Cm_Worldspawn(void) {
  return *cm_bsp.entities;
}

/**
 * @brief Returns the contents mask for the given leaf number.
 */
int32_t Cm_LeafContents(const int32_t leaf_num) {

  if (leaf_num < 0 || leaf_num >= cm_bsp.num_leafs) {
    Com_Error(ERROR_DROP, "Bad number: %d\n", leaf_num);
  }

  return cm_bsp.leafs[leaf_num].contents;
}

/**
 * @brief Returns a const pointer to the global BSP collision model.
 */
const CmBsp *Cm_Bsp(void) {
  return &cm_bsp;
}
