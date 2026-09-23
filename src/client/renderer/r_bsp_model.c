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

#include "r_local.h"
#include "r_local.h"

/**
 * @brief Loads BSP planes into renderer plane structures.
 */
static void R_LoadBspPlanes(RenderBspModel *bsp) {
  RenderBspPlane *out;

  const CmBspPlane *in = bsp->cm->planes;

  bsp->numPlanes = bsp->cm->numPlanes;
  bsp->planes = out = Mem_LinkMalloc(bsp->numPlanes * sizeof(*out), bsp);

  for (int32_t i = 0; i < bsp->numPlanes; i++, out++, in++) {
    out->cm = in;
  }
}

/**
 * @brief Loads and registers BSP materials.
 */
static void R_LoadBspMaterials(RenderModel *mod) {

  RenderMaterial **out;
  const BspMaterial *in = mod->bsp->cm->file->materials;

  mod->bsp->numMaterials = mod->bsp->cm->file->numMaterials;
  mod->bsp->materials = out = Mem_LinkMalloc(mod->bsp->numMaterials * sizeof(*out), mod->bsp);

  for (int32_t i = 0; i < mod->bsp->numMaterials; i++, in++, out++) {
    *out = R_LoadMaterial(in->name, ASSET_CONTEXT_TEXTURES);
    R_RegisterDependency((RenderMedia *) mod, (RenderMedia *) *out);
  }
}

/**
 * @brief Loads BSP brush sides.
 */
static void R_LoadBspBrushSides(RenderBspModel *bsp) {
  RenderBspBrushSide *out;

  const BspBrushSide *in = bsp->cm->file->brushSides;

  bsp->numBrushSides = bsp->cm->file->numBrushSides;
  bsp->brushSides = out = Mem_LinkMalloc(bsp->numBrushSides * sizeof(*out), bsp);

  for (int32_t i = 0; i < bsp->numBrushSides; i++, in++, out++) {

    out->plane = bsp->planes + in->plane;

    if (in->material > -1) {
      out->material = bsp->materials[in->material];
    }

    out->axis[0] = in->axis[0];
    out->axis[1] = in->axis[1];

    out->contents = in->contents;
    out->surface = in->surface;
    out->value = in->value;
  }
}

/**
 * @brief Loads BSP patches.
 */
static void R_LoadBspPatches(RenderBspModel *bsp) {

  const BspPatch *in = bsp->cm->file->patches;

  bsp->numPatches = bsp->cm->file->numPatches;
  RenderBspPatch *out = bsp->patches = Mem_LinkMalloc(bsp->numPatches * sizeof(*out), bsp);

  for (int32_t i = 0; i < bsp->numPatches; i++, in++, out++) {

    if (in->material > -1) {
      out->material = bsp->materials[in->material];
    }

    out->contents = in->contents;
    out->surface = in->surface;
  }
}

/**
 * @brief Loads BSP vertex data.
 */
static void R_LoadBspVertexes(RenderBspModel *bsp) {

  bsp->numVertexes = bsp->cm->file->numVertexes;
  RenderBspVertex *out = bsp->vertexes = Mem_LinkMalloc(bsp->numVertexes * sizeof(*out), bsp);

  const BspVertex *in = bsp->cm->file->vertexes;
  for (int32_t i = 0; i < bsp->numVertexes; i++, in++, out++) {

    out->position = in->position;
    out->normal = in->normal;
    out->tangent = in->tangent;
    out->bitangent = in->bitangent;
    out->diffusemap = in->diffusemap;
    out->color = in->color;
  }
}

/**
 * @brief Loads BSP triangle indices.
 */
static void R_LoadBspElements(RenderBspModel *bsp) {

  bsp->numElements = bsp->cm->file->numElements;
  uint32_t *out = bsp->elements = Mem_LinkMalloc(bsp->numElements * sizeof(*out), bsp);

  const int32_t *in = bsp->cm->file->elements;
  for (int32_t i = 0; i < bsp->numElements; i++, in++, out++) {
    *out = *in;
  }
}

/**
 * @brief Loads BSP faces.
 */
static void R_LoadBspFaces(RenderBspModel *bsp) {

  const BspFace *in = bsp->cm->file->faces;
  RenderBspFace *out;

  bsp->numFaces = bsp->cm->file->numFaces;
  bsp->faces = out = Mem_LinkMalloc(bsp->numFaces * sizeof(*out), bsp);

  for (int32_t i = 0; i < bsp->numFaces; i++, in++, out++) {

    if (in->brushSide >= 0) {
      out->brushSide = bsp->brushSides + in->brushSide;
      out->plane = bsp->planes + in->plane;
    } else {
      out->patch = bsp->patches + in->patch;
      out->plane = NULL;
    }

    out->bounds = in->bounds;

    out->vertexes = bsp->vertexes + in->firstVertex;
    out->numVertexes = in->numVertexes;

    out->elements = (void *) (in->firstElement * sizeof(uint32_t));
    out->numElements = in->numElements;
  }
}

/**
 * @brief Loads BSP leafs.
 */
static void R_LoadBspLeafs(RenderBspModel *bsp) {
  RenderBspLeaf *out;

  const BspLeaf *in = bsp->cm->file->leafs;

  bsp->numLeafs = bsp->cm->file->numLeafs;
  bsp->leafs = out = Mem_LinkMalloc(bsp->numLeafs * sizeof(*out), bsp);

  for (int32_t i = 0; i < bsp->numLeafs; i++, in++, out++) {
    out->contents = in->contents;
    out->bounds = in->bounds;
  }
}

/**
 * @brief Loads BSP nodes.
 */
static void R_LoadBspNodes(RenderBspModel *bsp) {
  RenderBspNode *out;

  const BspNode *in = bsp->cm->file->nodes;

  bsp->numNodes = bsp->cm->file->numNodes;
  bsp->nodes = out = Mem_LinkMalloc(bsp->numNodes * sizeof(*out), bsp);

  for (int32_t i = 0; i < bsp->numNodes; i++, in++, out++) {

    out->contents = in->contents;
    out->plane = bsp->planes + in->plane;
    out->bounds = in->bounds;
    out->visibleBounds = in->visibleBounds;

    out->faces = bsp->faces + in->firstFace;
    out->numFaces = in->numFaces;

    RenderBspFace *f = out->faces;
    for (int32_t j = 0; j < out->numFaces; j++, f++) {
      f->node = out;
    }

    for (int32_t j = 0; j < 2; j++) {
      const int32_t c = in->children[j];
      if (c >= 0) {
        out->children[j] = bsp->nodes + c;
      } else {
        out->children[j] = (RenderBspNode *) (bsp->leafs + (-1 - c));
      }
    }
  }
}

/**
 * @brief Links a BSP node to its parent and inline model.
 */
static void R_SetupBspNode(RenderBspInlineModel *model, RenderBspNode *parent, RenderBspNode *node) {

  node->model = model;
  node->parent = parent;

  if (node->contents > CONTENTS_NODE) {
    return;
  }

  R_SetupBspNode(model, node, node->children[0]);
  R_SetupBspNode(model, node, node->children[1]);
}

/**
 * @brief Loads BSP draw batches.
 */
static void R_LoadBspDrawElements(RenderBspModel *bsp) {
  RenderBspDrawElements *out;

  bsp->numDrawElements = bsp->cm->file->numDrawElements;
  bsp->drawElements = out = Mem_LinkMalloc(bsp->numDrawElements * sizeof(*out), bsp);

  const BspDrawElements *in = bsp->cm->file->drawElements;
  for (int32_t i = 0; i < bsp->numDrawElements; i++, in++, out++) {

    if (in->material > -1) {
      out->material = bsp->materials[in->material];
    }
    out->surface = in->surface;

    out->bounds = in->bounds;

    out->elements = (void *) (in->firstElement * sizeof(uint32_t));
    out->numElements = in->numElements;

    if (out->material && out->material->cm->stageFlags & (STAGE_STRETCH | STAGE_ROTATE)) {

      Vec2 stMins = Vec2_Mins();
      Vec2 stMaxs = Vec2_Maxs();

      const uint32_t *e = bsp->elements + in->firstElement;
      for (int32_t j = 0; j < out->numElements; j++, e++) {
        const RenderBspVertex *v = &bsp->vertexes[*e];

        stMins = Vec2_Minf(stMins, v->diffusemap);
        stMaxs = Vec2_Maxf(stMaxs, v->diffusemap);
      }

      out->stOrigin = Vec2_Scale(Vec2_Add(stMins, stMaxs), .5f);
    }
  }
}

/**
 * @brief Loads BSP blocks and their decal state.
 */
static void R_LoadBspBlocks(RenderBspModel *bsp) {
  RenderBspBlock *out;

  bsp->numBlocks = bsp->cm->file->numBlocks;
  bsp->blocks = out = Mem_LinkMalloc(bsp->numBlocks * sizeof(RenderBspBlock), bsp);

  const BspBlock *in = bsp->cm->file->blocks;
  for (int32_t i = 0; i < bsp->numBlocks; i++, in++, out++) {

    out->node = bsp->nodes + in->node;
    out->drawElements = bsp->drawElements + in->firstDrawElement;
    out->numDrawElements = in->numDrawElements;
    out->visibleBounds = in->visibleBounds;

    for (int32_t j = 0; j < out->numDrawElements; j++) {
      out->surface |= out->drawElements[j].surface;
    }

    RenderBspBlockDecals *decals = &out->decals;

    decals->triangles = $(alloc(Vector), initWithSize, sizeof(RenderDecalTriangle));

  }

  const BspFace *inFace = bsp->cm->file->faces;
  RenderBspFace *outFace = bsp->faces;
  for (int32_t i = 0; i < bsp->numFaces; i++, inFace++, outFace++) {
    if (inFace->block >= 0 && inFace->block < bsp->numBlocks) {
      outFace->block = &bsp->blocks[inFace->block];
    }
  }
}

/**
 * @brief Loads BSP inline models.
 */
static void R_LoadBspInlineModels(RenderBspModel *bsp) {
  RenderBspInlineModel *out;

  const BspModel *in = bsp->cm->file->models;

  bsp->numInlineModels = bsp->cm->file->numModels;
  bsp->inlineModels = out = Mem_LinkMalloc(bsp->numInlineModels * sizeof(*out), bsp);

  for (int32_t i = 0; i < bsp->numInlineModels; i++, in++, out++) {

    out->entity = bsp->cm->entities[in->entity];
    out->headNode = bsp->nodes + in->headNode;

    out->visibleBounds = in->visibleBounds;

    out->faces = bsp->faces + in->firstFace;
    out->numFaces = in->numFaces;

    out->depthPassElements = bsp->drawElements + in->firstDepthPassElements;
    out->numDepthPassElements = in->numDepthPassElements;

    out->drawElements = bsp->drawElements + in->firstDrawElements;
    out->numDrawElements = in->numDrawElements;

    out->blocks = bsp->blocks + in->firstBlock;
    out->numBlocks = in->numBlocks;

    R_SetupBspNode(out, NULL, out->headNode);
  }
}

/**
 * @brief Loads BSP portals, attaching each to the draw elements of the face that shows it.
 * @details The compiler resolves a portal's two frames and the draw elements its face was
 * emitted to, so all that is left here is to build the two frames, and to find the inline model
 * the face belongs to. The transform between them is composed per frame, since the model drawing
 * the face may be a mover.
 */
static void R_LoadBspPortals(RenderModel *mod) {

  RenderBspModel *bsp = mod->bsp;

  bsp->numPortals = bsp->cm->file->numPortals;
  if (!bsp->numPortals) {
    return;
  }

  RenderSubview *out = bsp->portals = Mem_LinkMalloc(sizeof(*out) * bsp->numPortals, bsp);

  const BspPortal *in = bsp->cm->file->portals;
  for (int32_t i = 0; i < bsp->numPortals; i++, in++, out++) {

    if (in->drawElements < 0 || in->drawElements >= bsp->numDrawElements) {
      Com_Warn("Portal @ %s has invalid draw elements %d\n", vtos(in->entryOrigin), in->drawElements);
      continue;
    }

    out->type = SUBVIEW_PORTAL;
    out->origin = in->entryOrigin;
    out->bounds = bsp->drawElements[in->drawElements].bounds;
    out->normal = Vec3_Negate(in->entryForward);

    out->entry = Mat4_FromVectors(in->entryForward,
                                  Vec3_Cross(in->entryForward, in->entryUp),
                                  in->entryUp,
                                  in->entryOrigin);

    out->exit = Mat4_FromVectors(in->exitForward,
                                 Vec3_Cross(in->exitForward, in->exitUp),
                                 in->exitUp,
                                 in->exitOrigin);

    bsp->drawElements[in->drawElements].subview = out;

    for (int32_t j = 0; j < bsp->numInlineModels; j++) {

      const RenderBspInlineModel *m = &bsp->inlineModels[j];
      const ptrdiff_t first = m->drawElements - bsp->drawElements;

      if (in->drawElements >= first && in->drawElements < first + m->numDrawElements) {
        out->model = (RenderModel *) R_FindMedia(va("%s#%d", mod->media.name, j), R_MEDIA_MODEL);
        break;
      }
    }
  }
}

/**
 * @brief Loads BSP reflections, attaching each to the draw elements of the faces that show it.
 * @details The compiler resolves the plane and groups the draw elements sharing it, so all that
 * is left here is to find the inline model each belongs to.
 */
static void R_LoadBspReflections(RenderModel *mod) {

  RenderBspModel *bsp = mod->bsp;

  bsp->numReflections = bsp->cm->file->numReflections;
  if (!bsp->numReflections) {
    return;
  }

  RenderSubview *out = bsp->reflections = Mem_LinkMalloc(sizeof(*out) * bsp->numReflections, bsp);

  const BspReflection *in = bsp->cm->file->reflections;
  for (int32_t i = 0; i < bsp->numReflections; i++, in++, out++) {

    if (in->model < 0 || in->model >= bsp->numInlineModels) {
      Com_Warn("Reflection @ %s has invalid model %d\n", vtos(in->origin), in->model);
      continue;
    }

    out->type = SUBVIEW_REFLECTION;
    out->origin = in->origin;
    out->normal = in->normal;
    out->bounds = in->bounds;

    out->model = (RenderModel *) R_FindMedia(va("%s#%d", mod->media.name, in->model), R_MEDIA_MODEL);
  }

  RenderBspDrawElements *draw = bsp->drawElements;
  for (int32_t i = 0; i < bsp->numDrawElements; i++, draw++) {

    const int32_t reflection = bsp->cm->file->drawElements[i].reflection;
    if (reflection == -1) {
      continue;
    }

    if (reflection < 0 || reflection >= bsp->numReflections) {
      Com_Warn("Draw elements %d has invalid reflection %d\n", i, reflection);
      continue;
    }

    // a face shows one subview, and the layer it samples is one int, so a face that is both a
    // portal and reflective would silently lose one of them
    if (draw->subview) {
      Com_Warn("Draw elements %d is both a portal and reflective; ignoring the reflection\n", i);
      continue;
    }

    draw->subview = bsp->reflections + reflection;
  }
}

/**
 * @brief Loads BSP lights.
 *//**
 * @brief Loads BSP lights.
 */
static void R_LoadBspLights(RenderBspModel *bsp) {

  const BspLight *in = bsp->cm->file->lights;

  bsp->numLights = bsp->cm->file->numLights;
  RenderBspLight *out = bsp->lights = Mem_LinkMalloc(sizeof(*out) * bsp->numLights, bsp);

  for (int32_t i = 0; i < bsp->numLights; i++, in++, out++) {

    out->entity = bsp->cm->entities[in->entity];
    out->origin = in->origin;
    out->color = in->color;
    out->radius = in->radius;
    out->intensity = in->intensity;
    out->bounds = in->bounds;
    q_strlcpy(out->style, in->style, sizeof(out->style));
    out->drift = in->drift;
    out->drawElements = bsp->drawElements + in->firstDrawElements;
    out->numDrawElements = in->numDrawElements;
    out->targetEntity = in->targetEntity > 0 ? bsp->cm->entities[in->targetEntity] : NULL;
  }
}

/**
 * @brief Appends merged voxel bounds to an occlusion query.
 */
static void R_AppendOcclusionQueryVoxels(RenderOcclusionQuery *query, const BspVoxels *voxels, const int32_t *indices, int32_t numIndices) {

  if (!numIndices) {
    return;
  }

  Box3 *boxes = Mem_Malloc(numIndices * sizeof(Box3));

  const int32_t xy = voxels->size.x * voxels->size.y;

  for (int32_t i = 0; i < numIndices; i++) {
    const int32_t index = indices[i];

    const int32_t z = index / xy;
    const int32_t rem = index % xy;
    const int32_t y = rem / voxels->size.x;
    const int32_t x = rem % voxels->size.x;

    const Vec3 mins = Vec3_Add(voxels->bounds.mins, Vec3_Scale(MakeVec3(x, y, z), BSP_VOXEL_SIZE));
    const Vec3 maxs = Vec3_Add(mins, MakeVec3(BSP_VOXEL_SIZE, BSP_VOXEL_SIZE, BSP_VOXEL_SIZE));

    boxes[i] = MakeBox3(mins, maxs);
  }

  Box3 *merged;
  const size_t numMerged = Box3_Merge(boxes, numIndices, &merged);

  Mem_Free(boxes);

  for (size_t i = 0; i < numMerged; i++) {
    R_AppendOcclusionQueryBox(query, merged[i]);
  }

  free(merged);
}

/**
 * @brief Builds BSP block and light occlusion queries from voxel coverage.
 */
static void R_LoadBspOcclusionQueries(RenderBspModel *bsp) {

  const BspFile *file = bsp->cm->file;
  const BspVoxels *voxels = file->voxels;

  RenderBspBlock *block = bsp->blocks;
  const BspBlock *inBlock = file->blocks;
  for (int32_t i = 0; i < bsp->numBlocks; i++, block++, inBlock++) {

    const Box3 bounds = Box3_Union(block->node->bounds, block->visibleBounds);
    block->query = R_AllocOcclusionQuery(bounds);

    R_AppendOcclusionQueryVoxels(block->query, voxels,
                                 file->blockVoxels + inBlock->firstVoxel, inBlock->numVoxels);

    if (!block->query->numBoxes) {
      R_AppendOcclusionQueryBox(block->query, bounds);
    }
  }

  RenderBspLight *light = bsp->lights;
  const BspLight *inLight = file->lights;
  for (int32_t i = 0; i < bsp->numLights; i++, light++, inLight++) {

    light->query = R_AllocOcclusionQuery(light->bounds);

    R_AppendOcclusionQueryVoxels(light->query, voxels,
                                 file->lightVoxels + inLight->firstVoxel, inLight->numVoxels);

    if (!light->query->numBoxes) {
      R_AppendOcclusionQueryBox(light->query, light->bounds);
    }
  }
}

/**
 * @brief Loads BSP voxel lighting data.
 */
static void R_LoadBspVoxels(RenderModel *mod) {

  const BspVoxels *in = mod->bsp->cm->file->voxels;
  const byte *data = (byte *) in + sizeof(BspVoxels);

  RenderBspVoxels *out = &mod->bsp->voxels;

  out->size = in->size;
  out->numVoxels = out->size.x * out->size.y * out->size.z;
  out->numLightIndices = in->numLightIndices;
  out->bounds = in->bounds;

  const byte *causticsData = data;
  data += out->numVoxels * sizeof(byte) * 3;

  out->caustics = (RenderImage *) R_AllocMedia("voxel_caustics", sizeof(RenderImage), R_MEDIA_IMAGE);
  out->caustics->media.Free = R_FreeImage;
  out->caustics->type = IMG_VOXELS;
  out->caustics->width = out->size.x;
  out->caustics->height = out->size.y;
  out->caustics->depth = out->size.z;

  byte *causticsRgba = Mem_Malloc(out->numVoxels * 4);
  for (int32_t i = 0; i < out->numVoxels; i++) {
    causticsRgba[i * 4 + 0] = causticsData[i * 3 + 0];
    causticsRgba[i * 4 + 1] = causticsData[i * 3 + 1];
    causticsRgba[i * 4 + 2] = causticsData[i * 3 + 2];
    causticsRgba[i * 4 + 3] = 255;
  }

  out->caustics->texture = $(rContext.device, createTexture, &(SDL_GPUTextureCreateInfo) {
    .type = SDL_GPU_TEXTURETYPE_3D,
    .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
    .width = (Uint32) out->size.x,
    .height = (Uint32) out->size.y,
    .layer_count_or_depth = (Uint32) out->size.z,
    .num_levels = 1,
    .sample_count = SDL_GPU_SAMPLECOUNT_1,
  }, causticsRgba);

  Mem_Free(causticsRgba);

  const int32_t *lightData = (const int32_t *) data;
  data += out->numVoxels * sizeof(int32_t) * 2;

  out->lightData = (RenderImage *) R_AllocMedia("voxel_light_data", sizeof(RenderImage), R_MEDIA_IMAGE);
  out->lightData->media.Free = R_FreeImage;
  out->lightData->type = IMG_VOXELS;
  out->lightData->width = out->size.x;
  out->lightData->height = out->size.y;
  out->lightData->depth = out->size.z;

  out->lightDataBuffer = $(rContext.device, createBufferWithConstMem,
      SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
      lightData,
      out->numVoxels * sizeof(int32_t) * 2);

  const int32_t *lightIndicesData = (const int32_t *) data;
  data += out->numLightIndices * sizeof(int32_t);

  if (out->numLightIndices > 0) {
    out->lightIndicesBuffer = $(rContext.device, createBufferWithConstMem,
        SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
        lightIndicesData,
        out->numLightIndices * sizeof(int32_t));
  }

  out->lightIndices = (RenderImage *) R_AllocMedia("voxel_light_indices", sizeof(RenderImage), R_MEDIA_IMAGE);
  out->lightIndices->media.Free = R_FreeImage;
  out->lightIndices->type = IMG_VOXELS;

  const byte *occlusionData = data;

  out->occlusion = (RenderImage *) R_AllocMedia("voxel_occlusion", sizeof(RenderImage), R_MEDIA_IMAGE);
  out->occlusion->media.Free = R_FreeImage;
  out->occlusion->type = IMG_VOXELS;
  out->occlusion->width = out->size.x;
  out->occlusion->height = out->size.y;
  out->occlusion->depth = out->size.z;

  out->occlusion->texture = $(rContext.device, createTexture, &(SDL_GPUTextureCreateInfo) {
    .type = SDL_GPU_TEXTURETYPE_3D,
    .format = SDL_GPU_TEXTUREFORMAT_R8G8_UNORM,
    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
    .width = (Uint32) out->size.x,
    .height = (Uint32) out->size.y,
    .layer_count_or_depth = (Uint32) out->size.z,
    .num_levels = 1,
    .sample_count = SDL_GPU_SAMPLECOUNT_1,
  }, occlusionData);

  if (r_drawBspVoxels->value) {
    
    out->voxels = Mem_LinkMalloc(out->numVoxels * sizeof(RenderBspVoxel), mod->bsp);

    for (int32_t u = 0; u < out->size.z; u++) {
      for (int32_t t = 0; t < out->size.y; t++) {
        for (int32_t s = 0; s < out->size.x; s++) {
          const int32_t voxelIndex = (u * out->size.y + t) * out->size.x + s;
          RenderBspVoxel *voxel = &out->voxels[voxelIndex];

          const Vec3 voxelMins = MakeVec3(
            out->bounds.mins.x + s * BSP_VOXEL_SIZE,
            out->bounds.mins.y + t * BSP_VOXEL_SIZE,
            out->bounds.mins.z + u * BSP_VOXEL_SIZE
          );

          const Vec3 voxelMaxs = Vec3_Add(voxelMins, MakeVec3(BSP_VOXEL_SIZE, BSP_VOXEL_SIZE, BSP_VOXEL_SIZE));
          voxel->bounds = MakeBox3(voxelMins, voxelMaxs);

          const int32_t firstLightIndex = lightData[voxelIndex * 2 + 0];
          const int32_t numLightIndices = lightData[voxelIndex * 2 + 1];

          voxel->numLights = numLightIndices;

          if (voxel->numLights > 0) {
            voxel->lights = Mem_LinkMalloc(voxel->numLights * sizeof(RenderBspLight *), mod->bsp);

            for (int32_t i = 0; i < voxel->numLights; i++) {
              const int32_t lightId = lightIndicesData[firstLightIndex + i];

              if (lightId >= 0 && lightId < mod->bsp->numLights) {
                voxel->lights[i] = &mod->bsp->lights[lightId];
              } else {
                voxel->lights[i] = NULL;
              }
            }
          } else {
            voxel->lights = NULL;
          }
        }
      }
    }
  }

}

/**
 * @brief Creates BSP vertex and index buffers.
 */
static void R_LoadBspVertexArray(RenderModel *mod) {

  RenderBspModel *bsp = mod->bsp;

  bsp->vertexBuffer = $(rContext.device, createBufferWithConstMem, SDL_GPU_BUFFERUSAGE_VERTEX,
                         bsp->vertexes, bsp->numVertexes * sizeof(RenderBspVertex));

  bsp->elementsBuffer = $(rContext.device, createBufferWithConstMem, SDL_GPU_BUFFERUSAGE_INDEX,
                           bsp->elements, bsp->numElements * sizeof(uint32_t));

  $(bsp->vertexBuffer, setName, va("%s vertexes", mod->media.name));
  $(bsp->elementsBuffer, setName, va("%s elements", mod->media.name));
}

/**
 * @brief Creates renderer models for BSP inline models.
 */
static void R_SetupBspInlineModels(RenderModel *mod) {

  RenderBspInlineModel *in = mod->bsp->inlineModels;
  for (int32_t i = 0; i < mod->bsp->numInlineModels; i++, in++) {

    char name[MAX_QPATH];
    q_snprintf(name, sizeof(name), "%s#%d", mod->media.name, i);

    RenderModel *out = (RenderModel *) R_AllocMedia(name, sizeof(RenderModel), R_MEDIA_MODEL);

    out->type = MODEL_BSP_INLINE;
    out->bspInline = in;
    out->bounds = in->visibleBounds;

    mod->bounds = Box3_Union(mod->bounds, out->bounds);
    if (i == 0) {
      mod->bsp->worldspawn = out;
    }

    R_RegisterDependency(&mod->media, &out->media);
  }
}

/**
 * @brief Loads the sky cubemap specified in worldspawn.
 */
static void R_LoadBspSky(RenderModel *mod) {

  const char *name = Cm_EntityValue(Cm_Worldspawn(), "sky")->nullableString;
  if (name) {
    mod->bsp->sky = R_LoadImage(va("sky/%s", name), IMG_CUBEMAP);
  } else {
    mod->bsp->sky = NULL;
  }

  if (mod->bsp->sky == NULL) {
    if (name) {
      Com_Warn("Failed to load sky sky/%s\n", name);
    } else {
      Com_Warn("Failed to load sky (no sky key on worldspawn)\n");
    }

    mod->bsp->sky = R_LoadImage("sky/template", IMG_CUBEMAP);
    if (mod->bsp->sky == NULL) {
      Com_Error(ERROR_DROP, "Failed to load default sky\n");
    }
  }
}

/**
 * @brief BSP lumps required by the renderer.
 */
#define R_BSP_LUMPS ( \
  (1 << BSP_LUMP_PATCHES) | \
  (1 << BSP_LUMP_VERTEXES) | \
  (1 << BSP_LUMP_ELEMENTS) | \
  (1 << BSP_LUMP_FACES) | \
  (1 << BSP_LUMP_DRAW_ELEMENTS) | \
  (1 << BSP_LUMP_BLOCKS) | \
  (1 << BSP_LUMP_LIGHTS) | \
  (1 << BSP_LUMP_VOXELS) | \
  (1 << BSP_LUMP_LIGHT_VOXELS) | \
  (1 << BSP_LUMP_BLOCK_VOXELS) | \
  (1 << BSP_LUMP_PORTALS) | \
  (1 << BSP_LUMP_REFLECTIONS) \
)

/**
 * @brief Loads a BSP model into renderer structures.
 */
static void R_LoadBspModel(RenderModel *mod, void *buffer) {

  BspHeader *header = (BspHeader *) buffer;

  mod->bsp = Mem_LinkMalloc(sizeof(RenderBspModel), mod);
  mod->bsp->cm = Cm_Bsp();

  Bsp_LoadLumps(header, mod->bsp->cm->file, R_BSP_LUMPS);

  R_LoadBspPlanes(mod->bsp);
  R_LoadBspMaterials(mod);
  R_LoadBspBrushSides(mod->bsp);
  R_LoadBspPatches(mod->bsp);
  R_LoadBspVertexes(mod->bsp);
  R_LoadBspElements(mod->bsp);
  R_LoadBspFaces(mod->bsp);
  R_LoadBspLeafs(mod->bsp);
  R_LoadBspNodes(mod->bsp);
  R_LoadBspDrawElements(mod->bsp);
  R_LoadBspBlocks(mod->bsp);
  R_LoadBspInlineModels(mod->bsp);
  R_LoadBspVertexArray(mod);
  R_SetupBspInlineModels(mod);
  R_LoadBspLights(mod->bsp);
  R_LoadBspPortals(mod);
  R_LoadBspReflections(mod);
  R_FreeOcclusionQueries();
  R_LoadBspOcclusionQueries(mod->bsp);
  R_LoadBspVoxels(mod);
  R_LoadBspSky(mod);

  Bsp_UnloadLumps(mod->bsp->cm->file, R_BSP_LUMPS);

  Com_Debug(DEBUG_RENDERER, "!================================\n");
  Com_Debug(DEBUG_RENDERER, "!R_LoadBspModel:  %s\n", mod->media.name);
  Com_Debug(DEBUG_RENDERER, "!  Planes:        %d\n", mod->bsp->numPlanes);
  Com_Debug(DEBUG_RENDERER, "!  Materials:     %d\n", mod->bsp->numMaterials);
  Com_Debug(DEBUG_RENDERER, "!  Brush sides:   %d\n", mod->bsp->numBrushSides);
  Com_Debug(DEBUG_RENDERER, "!  Patches:       %d\n", mod->bsp->numPatches);
  Com_Debug(DEBUG_RENDERER, "!  Vertexes:      %d\n", mod->bsp->numVertexes);
  Com_Debug(DEBUG_RENDERER, "!  Elements:      %d\n", mod->bsp->numElements);
  Com_Debug(DEBUG_RENDERER, "!  Faces:         %d\n", mod->bsp->numFaces);
  Com_Debug(DEBUG_RENDERER, "!  Leafs:         %d\n", mod->bsp->numLeafs);
  Com_Debug(DEBUG_RENDERER, "!  Nodes:         %d\n", mod->bsp->numNodes);
  Com_Debug(DEBUG_RENDERER, "!  Draw elements: %d\n", mod->bsp->numDrawElements);
  Com_Debug(DEBUG_RENDERER, "!  Blocks:        %d\n", mod->bsp->numBlocks);
  Com_Debug(DEBUG_RENDERER, "!  Inline models: %d\n", mod->bsp->numInlineModels);
  Com_Debug(DEBUG_RENDERER, "!  Lights:        %d\n", mod->bsp->numLights);
  Com_Debug(DEBUG_RENDERER, "!  Portals:       %d\n", mod->bsp->numPortals);
  Com_Debug(DEBUG_RENDERER, "!  Reflections:   %d\n", mod->bsp->numReflections);
  Com_Debug(DEBUG_RENDERER, "!  Voxels:        %d\n", mod->bsp->voxels.numVoxels);
  Com_Debug(DEBUG_RENDERER, "!================================\n");
}

/**
 * @brief Registers BSP model media dependencies.
 */
static void R_RegisterBspModel(RenderMedia *self) {

  RenderModel *mod = (RenderModel *) self;

  R_RegisterDependency(self, (RenderMedia *) mod->bsp->voxels.caustics);
  R_RegisterDependency(self, (RenderMedia *) mod->bsp->voxels.occlusion);
  R_RegisterDependency(self, (RenderMedia *) mod->bsp->voxels.lightData);
  R_RegisterDependency(self, (RenderMedia *) mod->bsp->voxels.lightIndices);
  R_RegisterDependency(self, (RenderMedia *) mod->bsp->sky);

  rModels.world = mod;
}

/**
 * @brief Frees BSP model GPU resources.
 */
static void R_FreeBspModel(RenderMedia *self) {
  RenderModel *mod = (RenderModel *) self;

  RenderBspModel *bsp = mod->bsp;

  bsp->vertexBuffer = release(bsp->vertexBuffer);
  bsp->elementsBuffer = release(bsp->elementsBuffer);

  RenderBspBlock *block = bsp->blocks;
  for (int32_t i = 0; i < bsp->numBlocks; i++, block++) {

    release(block->decals.triangles);
    block->decals.vertexBuffer = release(block->decals.vertexBuffer);
  }

  bsp->voxels.lightDataBuffer = release(bsp->voxels.lightDataBuffer);
  bsp->voxels.lightIndicesBuffer = release(bsp->voxels.lightIndicesBuffer);

}

/**
 * @brief BSP model format descriptor.
 */
const RenderModelFormat rBspModelFormat = {
  .extension = "bsp",
  .type = MODEL_BSP,
  .Load = R_LoadBspModel,
  .Register = R_RegisterBspModel,
  .Free = R_FreeBspModel
};
