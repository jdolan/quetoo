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

#include "r_local.h"

RenderLights r_lights;

/**
 * @brief Adds a light source to the view's light list.
 */
void R_AddLight(RenderView *view, const RenderLight *l) {

  if (view->numLights == MAX_LIGHTS) {
    Com_Debug(DEBUG_RENDERER, "MAX_LIGHTS\n");
    return;
  }

  RenderLight *out = &view->lights[view->numLights++];

  *out = *l;
}

/**
 * @brief Builds the dynamic light bitmask for the given bounds.
 */
void R_ActiveDynamicLights(const RenderView *view, const Box3 bounds, RenderActiveDynamicLights *out) {

  memset(out, 0, sizeof(*out));

  int32_t j = 0;

  const RenderLight *l = view->lights;
  for (int32_t i = 0; i < view->numLights; i++, l++) {

    if (l->bspLight) {
      continue;
    }

    if (j == MAX_DYNAMIC_LIGHTS) {
      break;
    }

    if (Box3_Intersects(l->bounds, bounds)) {
      out->mask[j >> 5] |= 1u << (j & 31);
    }

    j++;
  }
}

/**
 * @brief Uploads one light block through the transfer buffer held for that purpose.
 */
static void R_UploadLightBlock(CopyPass *copyPass, Buffer *buffer, const void *block, uint32_t size) {

  $(r_lights.transferBuffer, write, block, size, true);

  $(copyPass, uploadBuffer,
    &(SDL_GPUTransferBufferLocation) { .transfer_buffer = r_lights.transferBuffer->buffer },
    &(SDL_GPUBufferRegion) { .buffer = buffer->buffer, .size = size },
    true);
}

/**
 * @brief Uploads light buffers and caches per-block and per-entity dynamic
 * light masks for the frame.
 */
void R_UpdateLights(RenderView *view, CopyPass *copyPass) {

  RenderBspLightsUniformBlock *bspLights = &r_lights.bspBlock;
  RenderDynamicLightsUniformBlock *dynamicLights = &r_lights.dynamicBlock;

  memset(bspLights, 0, sizeof(*bspLights));
  memset(dynamicLights, 0, sizeof(*dynamicLights));

  bspLights->numLights = r_models.world ? r_models.world->bsp->numLights : 0;

  int32_t numDynamicLights = 0;

  RenderLight *l = view->lights;
  for (int32_t i = 0; i < view->numLights; i++, l++) {

    RenderLightUniform *out;
    if (l->bspLight) {
      const ptrdiff_t index = (ptrdiff_t) (l->bspLight - r_models.world->bsp->lights);
      out = &bspLights->lights[index];
    } else {
      if (numDynamicLights == MAX_DYNAMIC_LIGHTS) {
        Com_Debug(DEBUG_RENDERER, "MAX_DYNAMIC_LIGHTS\n");
        continue;
      }
      out = &dynamicLights->lights[numDynamicLights++];
    }

    out->origin = Vec3_ToVec4(l->origin, l->radius);
    out->color = Vec3_ToVec4(l->color, l->intensity);

    // a portal view cannot occlude at all: the queries were resolved for another camera, so it
    // culls its own frustum and nothing more
    if (view->type == VIEW_PORTAL) {
      l->occluded = R_CullBox(view, l->bounds);
    } else if (l->bspLight) {
      l->occluded = !l->bspLight->query->result;
    } else {
      l->occluded = R_CulludeBox(view, l->bounds);
    }

    if (l->occluded) {
      r_stats->lightsOccluded++;
    } else {
      r_stats->lightsVisible++;
    }

    if (l->flags & R_LIGHT_NO_SHADOW) {
      l->tile = MakeVec2(-1.f, -1.f);
    } else {
      const int32_t lightCol = i % SHADOW_ATLAS_LIGHTS_PER_ROW;
      const int32_t lightRow = i / SHADOW_ATLAS_LIGHTS_PER_ROW;
      l->tile = MakeVec2((float) (lightCol * r_shadow_atlas.tileSize),
                     (float) (lightRow * r_shadow_atlas.tileSize));
    }

    out->tile = l->tile;

    R_UpdateLightEntities(view, l, i);
  }

  dynamicLights->numLights = numDynamicLights;

  const uint32_t bspSize = offsetof(RenderBspLightsUniformBlock, lights) + bspLights->numLights * sizeof(RenderLightUniform);
  R_UploadLightBlock(copyPass, r_lights.bspBuffer, bspLights, bspSize);

  const uint32_t dynamicSize = offsetof(RenderDynamicLightsUniformBlock, lights) + dynamicLights->numLights * sizeof(RenderLightUniform);
  R_UploadLightBlock(copyPass, r_lights.dynamicBuffer, dynamicLights, dynamicSize);

  if (r_models.world) {
    const RenderBspInlineModel *in = &r_models.world->bsp->inlineModels[0];

    RenderBspBlock *block = in->blocks;
    for (int32_t i = 0; i < in->numBlocks; i++, block++) {

      // a portal view cannot use occlusion queries resolved for another camera
      const bool culled = view->type == VIEW_PORTAL
        ? R_CullBox(view, block->visibleBounds)
        : block->query->result == 0;

      if (culled) {
        continue;
      }

      R_ActiveDynamicLights(view, block->visibleBounds, &block->activeDynamicLights);
    }
  }
}

/**
 * @brief Initializes the BSP and dynamic lights storage buffers.
 */
void R_InitLights(void) {

  memset(&r_lights, 0, sizeof(r_lights));

  r_lights.bspBuffer = $(r_context.device, createBuffer, &(SDL_GPUBufferCreateInfo) {
    .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
    .size = sizeof(r_lights.bspBlock),
  });

  r_lights.dynamicBuffer = $(r_context.device, createBuffer, &(SDL_GPUBufferCreateInfo) {
    .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
    .size = sizeof(r_lights.dynamicBlock),
  });

  r_lights.transferBuffer = $(r_context.device, createTransferBuffer, &(SDL_GPUTransferBufferCreateInfo) {
    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    .size = Maxi(sizeof(r_lights.bspBlock), sizeof(r_lights.dynamicBlock)),
  });

  const int32_t noLights[2] = { 0, 0 };
  r_lights.voxelFallbackBuffer = $(r_context.device, createBufferWithConstMem,
      SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, noLights, sizeof(noLights));
}

/**
 * @brief Frees the BSP and dynamic lights storage buffers.
 */
void R_ShutdownLights(void) {

  r_lights.bspBuffer = release(r_lights.bspBuffer);
  r_lights.dynamicBuffer = release(r_lights.dynamicBuffer);
  r_lights.transferBuffer = release(r_lights.transferBuffer);
  r_lights.voxelFallbackBuffer = release(r_lights.voxelFallbackBuffer);
}
