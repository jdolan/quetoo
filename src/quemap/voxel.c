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

#include "bsp.h"
#include "points.h"
#include "qlight.h"
#include "voxel.h"
#include <Objectively/Vector.h>

typedef struct {
  int32_t *indices;
  size_t count;
} VoxelLightIndices;

static void Voxel_CollectLightIndex(const HashTable *table, ident key, ident value, ident data) {
  VoxelLightIndices *collector = data;
  const Light *light = key;

  if (light->out) {
    collector->indices[collector->count++] = (int32_t) (ptrdiff_t) (light->out - bspFile.lights);
  }
}

Voxels voxels;

/**
 * @brief Create an `SDL_Surface` with the given voxel data.
 */
SDL_Surface *CreateVoxelSurface(int32_t w, int32_t h, size_t voxelSize, void *voxels) {

  SDL_Surface *surface = SDL_malloc(sizeof(SDL_Surface));

  surface->flags = SDL_SURFACE_PREALLOCATED;
  surface->w = w;
  surface->h = h;
  surface->reserved = (void *) voxelSize;
  surface->pixels = voxels;

  return surface;
}

/**
 * @brief Blits the `src` to the given `rect` in `dest`.
 */
int32_t BlitVoxelSurface(const SDL_Surface *src, SDL_Surface *dest, const SDL_Rect *rect) {

  assert(src);
  assert(src->pixels);

  assert(dest);
  assert(dest->pixels);

  assert(src->reserved == dest->reserved);

  assert(rect);

  assert(src->w == rect->w);
  assert(src->h == rect->h);

  const size_t voxelSize = (size_t) src->reserved;

  const byte *in = src->pixels;
  byte *out = dest->pixels;

  out += rect->y * dest->w * voxelSize + rect->x * voxelSize;

  for (int32_t x = 0; x < src->w; x++) {
    for (int32_t y = 0; y < src->h; y++) {

      const byte *inVoxel = in + y * src->w * voxelSize + x * voxelSize;
      byte *outVoxel = out + y * dest->w * voxelSize + x * voxelSize;

      memcpy(outVoxel, inVoxel, voxelSize);
    }
  }

  return 0;
}

/**
 * @brief Writes the voxel surface to a temporary file for debugging.
 */
int32_t WriteVoxelSurface(const SDL_Surface *in, const char *name) {

  assert(in);
  assert(in->pixels);

  SDL_Surface *out = NULL;

  const size_t voxelSize = (size_t) in->reserved;
  switch (voxelSize) {

    case sizeof(Vec3): {
      const Vec3 *inVoxel = (Vec3 *) in->pixels;

      out = SDL_CreateSurface(in->w, in->h, SDL_PIXELFORMAT_RGB24);
      Color24 *outVoxel = (Color24 *) out->pixels;

      for (int32_t x = 0; x < in->w; x++) {
        for (int32_t y = 0; y < in->h; y++) {
          *outVoxel++ = Color_Color24(Color3fv(*inVoxel++));
        }
      }
    }
      break;

    case sizeof(Color24): {
      const Color24 *inVoxel = (Color24 *) in->pixels;

      out = SDL_CreateSurface(in->w, in->h, SDL_PIXELFORMAT_RGB24);
      Color24 *outVoxel = (Color24 *) out->pixels;

      for (int32_t x = 0; x < in->w; x++) {
        for (int32_t y = 0; y < in->h; y++) {
          *outVoxel++ = *inVoxel++;
        }
      }
    }
      break;

    case sizeof(Color32): {
      const Color32 *inVoxel = (Color32 *) in->pixels;

      out = SDL_CreateSurface(in->w, in->h, SDL_PIXELFORMAT_RGBA32);
      Color32 *outVoxel = (Color32 *) out->pixels;

      for (int32_t x = 0; x < in->w; x++) {
        for (int32_t y = 0; y < in->h; y++) {
          *outVoxel++ = *inVoxel++;
        }
      }
    }
      break;
  }

  if (!out) {
    return -1;
  }

  const int32_t err = IMG_SavePNG(out, name);

  SDL_DestroySurface(out);

  return err;
}

/**
 * @brief Builds the voxel grid aligned to world coordinates at `BSP_VOXEL_SIZE` intervals.
 * Voxels are placed at ..., -64, -32, 0, 32, 64, 96, ... in all axes.
 */
static void BuildVoxelExtents(void) {

  const BspModel *world = bspFile.models;
  
  // Align mins to voxel grid (round down to nearest multiple of BSP_VOXEL_SIZE)
  voxels.stuBounds.mins.x = floorf(world->visibleBounds.mins.x / BSP_VOXEL_SIZE) * BSP_VOXEL_SIZE;
  voxels.stuBounds.mins.y = floorf(world->visibleBounds.mins.y / BSP_VOXEL_SIZE) * BSP_VOXEL_SIZE;
  voxels.stuBounds.mins.z = floorf(world->visibleBounds.mins.z / BSP_VOXEL_SIZE) * BSP_VOXEL_SIZE;
  
  // Align maxs to voxel grid (round up to nearest multiple of BSP_VOXEL_SIZE)
  voxels.stuBounds.maxs.x = ceilf(world->visibleBounds.maxs.x / BSP_VOXEL_SIZE) * BSP_VOXEL_SIZE;
  voxels.stuBounds.maxs.y = ceilf(world->visibleBounds.maxs.y / BSP_VOXEL_SIZE) * BSP_VOXEL_SIZE;
  voxels.stuBounds.maxs.z = ceilf(world->visibleBounds.maxs.z / BSP_VOXEL_SIZE) * BSP_VOXEL_SIZE;
  
  // Calculate grid size (number of voxels in each dimension)
  for (int32_t i = 0; i < 3; i++) {
    voxels.size.xyz[i] = (int32_t) ((voxels.stuBounds.maxs.xyz[i] - voxels.stuBounds.mins.xyz[i]) / BSP_VOXEL_SIZE);
  }
}

/**
 * @brief Allocates voxel cells for the entire grid, computing origins and bounds for each cell.
 */
static void BuildVoxelVoxels(void) {

  voxels.numVoxels = voxels.size.x * voxels.size.y * voxels.size.z;

  if (voxels.numVoxels > MAX_BSP_VOXELS) {
    Com_Error(ERROR_FATAL, "MAX_BSP_VOXELS\n");
  }

  voxels.voxels = Mem_TagMalloc(voxels.numVoxels * sizeof(Voxel), (MemTag) MEM_TAG_VOXEL);

  Voxel *v = voxels.voxels;

  for (int32_t z = 0; z < voxels.size.z; z++) {
    for (int32_t y = 0; y < voxels.size.y; y++) {
      for (int32_t x = 0; x < voxels.size.x; x++) {

        const int32_t index = (z * voxels.size.y + y) * voxels.size.x + x;
        v = &voxels.voxels[index];

        v->xyz = MakeVec3i(x, y, z);

        v->origin = MakeVec3(
          voxels.stuBounds.mins.x + (v->xyz.x + 0.5f) * BSP_VOXEL_SIZE,
          voxels.stuBounds.mins.y + (v->xyz.y + 0.5f) * BSP_VOXEL_SIZE,
          voxels.stuBounds.mins.z + (v->xyz.z + 0.5f) * BSP_VOXEL_SIZE
        );

        v->bounds = Box3_FromCenterRadius(v->origin, BSP_VOXEL_SIZE * .5f);

        v->lights = $(alloc(HashTable), init, HashTableHashDirect, HashTableEqualDirect);
      }
    }
  }
}

/**
 * @brief Authors a .map file which can be imported into Radiant to view the voxel projections.
 */
static void DebugVoxels(void) {
#if 0
  const char *path = va("maps/%s.voxel.map", mapBase);

  File *file = Fs_OpenWrite(path);
  if (file == NULL) {
    Com_Warn("Failed to open %s\n", path);
    return;
  }

  Voxel *v = voxels.voxels;
  for (size_t i = 0; i < voxels.numVoxels; i++, v++) {

    Fs_Print(file, "{\n");
    Fs_Print(file, "  \"classname\" \"info_voxel\"\n");
    Fs_Print(file, "  \"origin\" \"%g %g %g\"\n", v->origin.x, v->origin.y, v->origin.z);
    Fs_Print(file, "  \"x\" \"%d\"\n", v->xyz.x);
    Fs_Print(file, "  \"y\" \"%d\"\n", v->xyz.y);
    Fs_Print(file, "  \"z\" \"%d\"\n", v->xyz.z);
    Fs_Print(file, "}\n");
  }

  Fs_Close(file);
#endif
}

/**
 * @brief Initializes the voxel grid, assigns lights to voxels, and returns the total voxel count.
 * @return The number of voxels in the grid.
 */
size_t BuildVoxels(void) {

  memset(&voxels, 0, sizeof(voxels));

  BuildVoxelExtents();

  BuildVoxelVoxels();

  DebugVoxels();

  return voxels.numVoxels;
}

/**
 * @brief Assigns lights to a voxel based on visibility traces to corners and center.
 */
void LightVoxel(int32_t voxelNum) {

  Voxel *voxel = &voxels.voxels[voxelNum];

  Vec3 points[9];
  points[0] = voxel->origin;
  Box3_ToPoints(voxel->bounds, &points[1]);

  for (size_t i = 0; i < lights->count; i++) {

    Light *light = VectorValue(lights, Light *, i);
    if (light->targetEntity != -1) {
      continue;
    }
    if (!Box3_Intersects(light->bounds, voxel->bounds)) {
      continue;
    }

    for (size_t j = 0; j < lengthof(points); j++) {

      const CmTrace toVoxel = Light_Trace(light->origin, points[j], 0, CONTENTS_MASK_SHADOW);
      if (toVoxel.fraction == 1.f || Box3_ContainsPoint(voxel->bounds, toVoxel.end)) {
        $(voxel->lights, set, light, light);
        break;
      }
    }
  }
}

/**
 * @brief Calculates light visible bounds by taking the union of all visible voxels and feathering
 * those bounds to ensure smooth shadowing with no visible voxel boundaries in-game.
 */
void FloodLights(void) {

  Voxel *v = voxels.voxels;

  for (size_t i = 0; i < lights->count; i++) {
    Light *l = VectorValue(lights, Light *, i);

    l->visibleBounds = Box3_Null();

    if (l->targetEntity != -1) {
      continue;
    }

    Box3 bounds = Box3_Null();

    for (size_t j = 0; j < voxels.numVoxels; j++) {
      if ($(v[j].lights, get, l) != NULL) {
        bounds = Box3_Union(bounds, v[j].bounds);
      }
    }

    bounds = Box3_Expand(bounds, BSP_VOXEL_SIZE * .5f);

    for (size_t j = 0; j < voxels.numVoxels; j++) {
      if (Box3_Intersects(bounds, v[j].bounds)) {
        $(v[j].lights, set, l, l);
      }
    }

    for (size_t j = 0; j < voxels.numVoxels; j++) {
      if ($(v[j].lights, get, l) != NULL) {
        l->visibleBounds = Box3_Union(l->visibleBounds, v[j].bounds);
      }
    }
  }
}

/**
 * @brief Per-voxel enumeration callback that appends the voxel's index to every light's
 * voxel list, inverting the voxel -> light assignments already computed by LightVoxel/FloodLights.
 */
typedef struct {
  Vector **lightVoxelLists;
  int32_t voxelIndex;
} LightVoxelData;

static void Voxel_AppendLightVoxel(const HashTable *table, ident key, ident value, ident data) {

  LightVoxelData *d = data;
  const Light *light = key;

  if (light->out) {
    const int32_t lightIndex = (int32_t) (light->out - bspFile.lights);
    $(d->lightVoxelLists[lightIndex], add, &d->voxelIndex);
  }
}

/**
 * @brief Assigns each light the list of voxel indices it touches, inverting the per-voxel
 * light assignments already computed by LightVoxel/FloodLights. This allows the renderer to
 * draw tight, per-voxel occlusion query geometry for each light instead of a single big AABB.
 * @remarks Must be called after EmitLights, so that `light->out` is valid.
 */
void AssignLightVoxels(void) {

  const uint32_t start = (uint32_t) SDL_GetTicks();

  if (bspFile.numLights == 0) {
    return;
  }

  Vector **lightVoxelLists = Mem_TagMalloc(bspFile.numLights * sizeof(Vector *), (MemTag) MEM_TAG_VOXEL);
  for (int32_t i = 0; i < bspFile.numLights; i++) {
    lightVoxelLists[i] = $(alloc(Vector), initWithSize, sizeof(int32_t));
  }

  for (size_t i = 0; i < voxels.numVoxels; i++) {

    LightVoxelData data = {
      .lightVoxelLists = lightVoxelLists,
      .voxelIndex = (int32_t) i
    };

    $(voxels.voxels[i].lights, enumerate, Voxel_AppendLightVoxel, &data);
  }

  int32_t total = 0;
  for (int32_t i = 0; i < bspFile.numLights; i++) {
    total += (int32_t) lightVoxelLists[i]->count;
  }

  Bsp_AllocLump(&bspFile, BSP_LUMP_LIGHT_VOXELS, total);
  bspFile.numLightVoxels = total;

  int32_t *out = bspFile.lightVoxels;
  for (int32_t i = 0; i < bspFile.numLights; i++) {

    bspFile.lights[i].firstVoxel = (int32_t) (out - bspFile.lightVoxels);
    bspFile.lights[i].numVoxels = (int32_t) lightVoxelLists[i]->count;

    memcpy(out, lightVoxelLists[i]->elements, lightVoxelLists[i]->count * sizeof(int32_t));
    out += lightVoxelLists[i]->count;

    release(lightVoxelLists[i]);
  }

  Mem_Free(lightVoxelLists);

  Com_Print("\r%-24s [100%%] %d ms\n", "Assigning light voxels", (uint32_t) SDL_GetTicks() - start);
}

/**
 * @brief Builds a lookup from `CONTENTS_BLOCK` node index to the index of the block it defines
 * within `bspFile.blocks`, or -1 if the node is not a block.
 */
static int32_t *Voxel_BuildNodeToBlock(void) {

  int32_t *nodeToBlock = Mem_TagMalloc(bspFile.numNodes * sizeof(int32_t), (MemTag) MEM_TAG_VOXEL);

  for (int32_t i = 0; i < bspFile.numNodes; i++) {
    nodeToBlock[i] = -1;
  }

  for (int32_t i = 0; i < bspFile.numBlocks; i++) {
    nodeToBlock[bspFile.blocks[i].node] = i;
  }

  return nodeToBlock;
}

/**
 * @brief Builds parent index arrays for the BSP node tree, so that leafs and nodes may be
 * walked upward to their enclosing `CONTENTS_BLOCK` ancestor. The flattened, on-disk BSP tree
 * has no parent pointers, unlike quemap's transient tree-building `Node`, so we derive them
 * here with a single pass over the node array.
 */
static void Voxel_BuildParents(int32_t **nodeParent, int32_t **leafParent) {

  *nodeParent = Mem_TagMalloc(bspFile.numNodes * sizeof(int32_t), (MemTag) MEM_TAG_VOXEL);
  *leafParent = Mem_TagMalloc(bspFile.numLeafs * sizeof(int32_t), (MemTag) MEM_TAG_VOXEL);

  for (int32_t i = 0; i < bspFile.numNodes; i++) {
    (*nodeParent)[i] = -1;
  }
  for (int32_t i = 0; i < bspFile.numLeafs; i++) {
    (*leafParent)[i] = -1;
  }

  const BspNode *node = bspFile.nodes;
  for (int32_t i = 0; i < bspFile.numNodes; i++, node++) {
    for (int32_t c = 0; c < 2; c++) {
      const int32_t child = node->children[c];
      if (child >= 0) {
        (*nodeParent)[child] = i;
      } else {
        (*leafParent)[-1 - child] = i;
      }
    }
  }
}

/**
 * @brief Walks up from the given leaf to its enclosing `CONTENTS_BLOCK` ancestor, returning the
 * index of that block within `bspFile.blocks`, or -1 if none is found.
 */
static int32_t Voxel_BlockForLeaf(int32_t leafNum, const int32_t *nodeParent, const int32_t *leafParent,
                                   const int32_t *nodeToBlock) {

  int32_t nodeNum = leafParent[leafNum];

  while (nodeNum != -1 && bspFile.nodes[nodeNum].contents != CONTENTS_BLOCK) {
    nodeNum = nodeParent[nodeNum];
  }

  if (nodeNum == -1) {
    return -1;
  }

  return nodeToBlock[nodeNum];
}

/**
 * @brief Assigns each world-space BSP block the list of voxel indices it touches, by resolving
 * the leafs each voxel occupies up to their enclosing `CONTENTS_BLOCK` ancestor. This allows the
 * renderer to draw tight, per-voxel occlusion query geometry for each block instead of a single
 * big AABB, while the block's loose bounds are retained for CPU-side visibility checks.
 * @remarks Blocks belonging to non-world (inline) models are left with zero voxels, since voxels
 * only cover the world model's space; the renderer falls back to their loose bounds in that case.
 */
void AssignBlockVoxels(void) {

  const uint32_t start = (uint32_t) SDL_GetTicks();

  if (bspFile.numBlocks == 0) {
    return;
  }

  int32_t *nodeParent, *leafParent;
  Voxel_BuildParents(&nodeParent, &leafParent);

  int32_t *nodeToBlock = Voxel_BuildNodeToBlock();

  Vector **blockVoxelLists = Mem_TagMalloc(bspFile.numBlocks * sizeof(Vector *), (MemTag) MEM_TAG_VOXEL);
  for (int32_t i = 0; i < bspFile.numBlocks; i++) {
    blockVoxelLists[i] = $(alloc(Vector), initWithSize, sizeof(int32_t));
  }

  const int32_t headNode = bspFile.models[0].headNode;

  static int32_t leafs[MAX_BSP_LEAFS];

  for (size_t i = 0; i < voxels.numVoxels; i++) {

    Progress("Assigning block voxels", 100.f * i / voxels.numVoxels);

    const Voxel *voxel = &voxels.voxels[i];

    const size_t numLeafs = Cm_BoxLeafnums(voxel->bounds, leafs, lengthof(leafs), NULL, headNode);

    int32_t seen[64];
    size_t numSeen = 0;

    for (size_t j = 0; j < numLeafs; j++) {

      const int32_t block = Voxel_BlockForLeaf(leafs[j], nodeParent, leafParent, nodeToBlock);
      if (block == -1) {
        continue;
      }

      bool dup = false;
      for (size_t k = 0; k < numSeen; k++) {
        if (seen[k] == block) {
          dup = true;
          break;
        }
      }

      if (dup) {
        continue;
      }

      if (numSeen < lengthof(seen)) {
        seen[numSeen++] = block;
      }

      int32_t voxelIndex = (int32_t) i;
      $(blockVoxelLists[block], add, &voxelIndex);
    }
  }

  int32_t total = 0;
  for (int32_t i = 0; i < bspFile.numBlocks; i++) {
    total += (int32_t) blockVoxelLists[i]->count;
  }

  Bsp_AllocLump(&bspFile, BSP_LUMP_BLOCK_VOXELS, total);
  bspFile.numBlockVoxels = total;

  int32_t *out = bspFile.blockVoxels;
  for (int32_t i = 0; i < bspFile.numBlocks; i++) {

    bspFile.blocks[i].firstVoxel = (int32_t) (out - bspFile.blockVoxels);
    bspFile.blocks[i].numVoxels = (int32_t) blockVoxelLists[i]->count;

    memcpy(out, blockVoxelLists[i]->elements, blockVoxelLists[i]->count * sizeof(int32_t));
    out += blockVoxelLists[i]->count;

    release(blockVoxelLists[i]);
  }

  Mem_Free(blockVoxelLists);
  Mem_Free(nodeParent);
  Mem_Free(leafParent);
  Mem_Free(nodeToBlock);

  Com_Print("\r%-24s [100%%] %d ms\n", "Assigning block voxels", (uint32_t) SDL_GetTicks() - start);
}

#define CAUSTICS_RADIUS 256.f

/**
 * @brief Calculates caustics direction and intensity based on visibility to nearby liquid brushes.
 */
void CausticsVoxel(int32_t voxelNum) {

  Voxel *voxel = &voxels.voxels[voxelNum];
  
  const int32_t contents = Cm_BoxContents(voxel->bounds, 0);

  if (contents & CONTENTS_MASK_LIQUID) {
    voxel->caustics = Vec3_Down();
    return;
  }
  
  Vec3 points[9];
  points[0] = voxel->origin;
  Box3_ToPoints(voxel->bounds, &points[1]);

  voxel->caustics = Vec3_Zero();
  const float weight = 1.f / lengthof(points);

  for (int32_t i = 0; i < bspFile.numBrushes; i++) {
    const BspBrush *brush = &bspFile.brushes[i];
    
    if (!(brush->contents & CONTENTS_MASK_LIQUID)) {
      continue;
    }

    const Vec3 brushCenter = Box3_Center(brush->bounds);
    
    for (size_t j = 0; j < lengthof(points); j++) {

      Vec3 liquidPoint = Box3_ClampPoint(brush->bounds, points[j]);
      const float dist = Vec3_Distance(points[j], liquidPoint);
      
      if (dist > CAUSTICS_RADIUS) {
        continue;
      }

      // ClampPoint can land on brush AABB bounds outside the actual liquid brush volume.
      // Nudge toward the brush center and require that the target is truly liquid.
      const Vec3 toCenter = Vec3_Subtract(brushCenter, liquidPoint);
      float toCenterLength;
      const Vec3 toCenterDir = Vec3_NormalizeLength(toCenter, &toCenterLength);
      if (toCenterLength > 0.f) {
        liquidPoint = Vec3_Fmaf(liquidPoint, 1.f, toCenterDir);
      }

      if (!(Light_PointContents(liquidPoint, 0) & CONTENTS_MASK_LIQUID)) {
        continue;
      }
      
      const CmTrace trace = Light_Trace(points[j], liquidPoint, 0, CONTENTS_MASK_SHADOW);
      if (trace.fraction == 1.f) {
        const float strength = Clampf01(1.f - dist / CAUSTICS_RADIUS) * weight;
        const Vec3 dir = Vec3_Normalize(Vec3_Subtract(liquidPoint, points[j]));
        voxel->caustics = Vec3_Fmaf(voxel->caustics, strength, dir);
      }
    }
  }

  float intensity;
  voxel->caustics = Vec3_NormalizeLength(voxel->caustics, &intensity);
  voxel->caustics = Vec3_Scale(voxel->caustics, Clampf01(intensity));
}

/**
 * @brief Calculates exposure based on sky visibility.
 */
void ExposureVoxel(int32_t voxelNum) {

  Voxel *voxel = &voxels.voxels[voxelNum];
  
  // Use dome vectors to sample hemisphere for better coverage
  static const Vec3 dome_vectors[] = DOME_UNIFORM_16X;
  
  float exposureSum = 0.f;
  
  for (size_t i = 0; i < lengthof(dome_vectors); i++) {
    const Vec3 start = voxel->origin;
    const Vec3 dir = Vec3_Scale(dome_vectors[i], MAX_WORLD_AXIAL);
    const Vec3 end = Vec3_Add(start, dir);
    
    const CmTrace trace = Light_Trace(start, end, 0, CONTENTS_MASK_SHADOW);
    
    // If we hit sky or nothing, count as exposed
    if (trace.surface & SURF_SKY || trace.fraction == 1.f) {
      exposureSum += 1.f;
    } else {
      // Partial exposure based on distance traveled
      exposureSum += trace.fraction * 0.5f;
    }
  }
  
  voxel->exposure = exposureSum / (float)lengthof(dome_vectors);
}

#define OCCLUSION_RADIUS 256.f

/**
 * @brief Calculates spatial occlusion based on enclosure. Uses full-sphere
 * rays: low average fraction (walls close) yields high occlusion; open areas
 * yield low occlusion. Rays are distance capped so large rooms and outdoor
 * areas produce meaningfully low values rather than collapsing toward 1.0.
 * Used for both audio reverb and ambient occlusion in the renderer.
 */
void OccludeVoxel(int32_t voxelNum) {

  Voxel *voxel = &voxels.voxels[voxelNum];

  static const Vec3 sphere_vectors[] = SPHERE_UNIFORM_32X;

  float fractionSum = 0.f;

  for (size_t i = 0; i < lengthof(sphere_vectors); i++) {
    const Vec3 start = voxel->origin;
    const Vec3 dir = Vec3_Scale(sphere_vectors[i], OCCLUSION_RADIUS);
    const Vec3 end = Vec3_Add(start, dir);

    const CmTrace trace = Light_Trace(start, end, 0, CONTENTS_MASK_SOLID);
    fractionSum += trace.fraction;
  }

  voxel->occlusion = 1.f - (fractionSum / (float) lengthof(sphere_vectors));
}

/**
 * @brief Applies a 3×3×3 box blur to the occlusion, exposure, and caustics fields
 * of all voxels. This smooths the sharp 32-unit grid discontinuities that result
 * from sampling each voxel only at its center.
 */
void SmoothVoxels(void) {

  const size_t n = voxels.numVoxels;

  float *smoothOcc = Mem_TagMalloc(n * sizeof(float), (MemTag) MEM_TAG_VOXEL);
  float *smoothExp = Mem_TagMalloc(n * sizeof(float), (MemTag) MEM_TAG_VOXEL);
  Vec3 *smoothCaust = Mem_TagMalloc(n * sizeof(Vec3), (MemTag) MEM_TAG_VOXEL);

  for (int32_t z = 0; z < voxels.size.z; z++) {
    for (int32_t y = 0; y < voxels.size.y; y++) {
      for (int32_t x = 0; x < voxels.size.x; x++) {

        float occSum = 0.f, expSum = 0.f;
        Vec3 caustSum = Vec3_Zero();
        int32_t count = 0;

        for (int32_t dz = -1; dz <= 1; dz++) {
          for (int32_t dy = -1; dy <= 1; dy++) {
            for (int32_t dx = -1; dx <= 1; dx++) {
              const int32_t nx = x + dx, ny = y + dy, nz = z + dz;
              if (nx < 0 || nx >= voxels.size.x ||
                  ny < 0 || ny >= voxels.size.y ||
                  nz < 0 || nz >= voxels.size.z) {
                continue;
              }
              const int32_t ni = (nz * voxels.size.y + ny) * voxels.size.x + nx;
              occSum += voxels.voxels[ni].occlusion;
              expSum += voxels.voxels[ni].exposure;
              caustSum = Vec3_Add(caustSum, voxels.voxels[ni].caustics);
              count++;
            }
          }
        }

        const int32_t i = (z * voxels.size.y + y) * voxels.size.x + x;
        smoothOcc[i] = occSum / (float) count;
        smoothExp[i] = expSum / (float) count;
        smoothCaust[i] = Vec3_Scale(caustSum, 1.f / (float) count);
      }
    }
  }

  for (size_t i = 0; i < n; i++) {
    voxels.voxels[i].occlusion = smoothOcc[i];
    voxels.voxels[i].exposure = smoothExp[i];
    voxels.voxels[i].caustics = smoothCaust[i];
  }

  Mem_Free(smoothOcc);
  Mem_Free(smoothExp);
  Mem_Free(smoothCaust);
}

static int IntCompare(const void *a, const void *b) {
  return *(const int32_t *) a - *(const int32_t *) b;
}

/**
 * @brief Serializes the voxel grid (caustics direction/strength, exposure, and light indices) into the BSP voxels lump.
 */
void EmitVoxels(void) {

  const uint32_t start = (uint32_t) SDL_GetTicks();

  voxels.numLightIndices = 0;

  Voxel *v = voxels.voxels;
  int32_t minLights = INT32_MAX, maxLights = 0;
  size_t totalLights = 0;
  
  for (size_t i = 0; i < voxels.numVoxels; i++, v++) {
    v->lightsOffset = (int32_t) voxels.numLightIndices;
    v->lightsCount = (int32_t) v->lights->count;

    voxels.numLightIndices += v->lightsCount;
    
    totalLights += v->lightsCount;
    if (v->lightsCount < minLights) {
      minLights = v->lightsCount;
    }
    if (v->lightsCount > maxLights) {
      maxLights = v->lightsCount;
    }
  }
  
  Com_Verbose("Voxel light stats: min=%d max=%d avg=%.1f total=%zd\n",
              minLights, maxLights, (float)totalLights / voxels.numVoxels, totalLights);

  bspFile.voxelsSize = sizeof(BspVoxels);
  bspFile.voxelsSize += voxels.numVoxels * sizeof(byte) * 3; // caustics xyz (RGB)
  bspFile.voxelsSize += voxels.numVoxels * sizeof(int32_t) * 2; // light indices offset and count
  bspFile.voxelsSize += voxels.numLightIndices * sizeof(int32_t);
  bspFile.voxelsSize += voxels.numVoxels * sizeof(byte) * 2; // occlusion + exposure (RG)

  Bsp_AllocLump(&bspFile, BSP_LUMP_VOXELS, bspFile.voxelsSize);
  memset(bspFile.voxels, 0, bspFile.voxelsSize);

  bspFile.voxels->size = voxels.size;
  bspFile.voxels->numLightIndices = (int32_t) voxels.numLightIndices;
  bspFile.voxels->bounds = voxels.stuBounds;

  byte *out = (byte *) bspFile.voxels + sizeof(BspVoxels);
  
  byte *outData = out;
  out += voxels.numVoxels * sizeof(byte) * 3; // RGB = caustics xyz
  
  for (int32_t z = 0; z < voxels.size.z; z++) {

    Progress("Emitting voxels", 100.f * z / voxels.size.z);
    
    for (int32_t y = 0; y < voxels.size.y; y++) {
      for (int32_t x = 0; x < voxels.size.x; x++) {

        const int32_t index = (z * voxels.size.y + y) * voxels.size.x + x;
        const Voxel *voxel = &voxels.voxels[index];

        const Vec3 caustics = Vec3_Clamp(voxel->caustics, Vec3_Negate(Vec3_One()), Vec3_One());
        *outData++ = (byte)((caustics.x * 0.5f + 0.5f) * 255.f);
        *outData++ = (byte)((caustics.y * 0.5f + 0.5f) * 255.f);
        *outData++ = (byte)((caustics.z * 0.5f + 0.5f) * 255.f);
      }
    }
  }

  int32_t *outLightData = (int32_t *) out;
  out += voxels.numVoxels * sizeof(int32_t) * 2;

  for (int32_t z = 0; z < voxels.size.z; z++) {
    for (int32_t y = 0; y < voxels.size.y; y++) {
      for (int32_t x = 0; x < voxels.size.x; x++) {

        const int32_t index = (z * voxels.size.y + y) * voxels.size.x + x;
        const Voxel *voxel = &voxels.voxels[index];

        *outLightData++ = voxel->lightsOffset;
        *outLightData++ = voxel->lightsCount;
      }
    }
  }

  int32_t *outLightIndices = (int32_t *) out;
  out += voxels.numLightIndices * sizeof(int32_t);

  for (int32_t z = 0; z < voxels.size.z; z++) {
    for (int32_t y = 0; y < voxels.size.y; y++) {
      for (int32_t x = 0; x < voxels.size.x; x++) {

        const int32_t index = (z * voxels.size.y + y) * voxels.size.x + x;
        const Voxel *voxel = &voxels.voxels[index];

        int32_t *indices = outLightIndices;
        VoxelLightIndices collector = {
          .indices = outLightIndices
        };

        $(voxel->lights, enumerate, Voxel_CollectLightIndex, &collector);
        outLightIndices += collector.count;

        const int32_t count = (int32_t) (outLightIndices - indices);
        if (count > 1) {
          qsort(indices, count, sizeof(int32_t), IntCompare);
        }
      }
    }
  }

  byte *outOcclusion = (byte *) outLightIndices;

  for (int32_t z = 0; z < voxels.size.z; z++) {
    for (int32_t y = 0; y < voxels.size.y; y++) {
      for (int32_t x = 0; x < voxels.size.x; x++) {

        const int32_t index = (z * voxels.size.y + y) * voxels.size.x + x;
        const Voxel *voxel = &voxels.voxels[index];

        *outOcclusion++ = (byte)(Clampf01(voxel->occlusion) * 255.f);
        *outOcclusion++ = (byte)(Clampf01(voxel->exposure) * 255.f);
      }
    }
  }

  Com_Debug(DEBUG_ALL, "Emitted voxels=%zd indices=%zd\n", voxels.numVoxels, voxels.numLightIndices);

  Com_Print("\r%-24s [100%%] %d ms\n", "Emitting voxels", (uint32_t) SDL_GetTicks() - start);
}

/**
 * @brief Frees all per-voxel light hash tables and releases the voxel memory pool.
 */
void FreeVoxels(void) {

  Voxel *v = voxels.voxels;
  for (size_t i = 0; i < voxels.numVoxels; i++, v++) {
    release(v->lights);
  }

  Mem_FreeTag((MemTag) MEM_TAG_VOXEL);
}
