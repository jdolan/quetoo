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

  if (view->num_lights == MAX_LIGHTS) {
    Com_Debug(DEBUG_RENDERER, "MAX_LIGHTS\n");
    return;
  }

  RenderLight *out = &view->lights[view->num_lights++];

  *out = *l;
}

/**
 * @brief Builds the dynamic light bitmask for the given bounds.
 */
void R_ActiveDynamicLights(const RenderView *view, const Box3 bounds, RenderActiveDynamicLights *out) {

  memset(out, 0, sizeof(*out));

  int32_t j = 0;

  const RenderLight *l = view->lights;
  for (int32_t i = 0; i < view->num_lights; i++, l++) {

    if (l->bsp_light) {
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

  $(r_lights.transfer_buffer, write, block, size, true);

  $(copyPass, uploadBuffer,
    &(SDL_GPUTransferBufferLocation) { .transfer_buffer = r_lights.transfer_buffer->buffer },
    &(SDL_GPUBufferRegion) { .buffer = buffer->buffer, .size = size },
    true);
}

/**
 * @brief Uploads light buffers and caches per-block and per-entity dynamic
 * light masks for the frame.
 */
void R_UpdateLights(RenderView *view, CopyPass *copyPass) {

  RenderBspLightsUniformBlock *bsp_lights = &r_lights.bsp_block;
  RenderDynamicLightsUniformBlock *dynamic_lights = &r_lights.dynamic_block;

  memset(bsp_lights, 0, sizeof(*bsp_lights));
  memset(dynamic_lights, 0, sizeof(*dynamic_lights));

  bsp_lights->num_lights = r_models.world ? r_models.world->bsp->num_lights : 0;

  int32_t num_dynamic_lights = 0;

  RenderLight *l = view->lights;
  for (int32_t i = 0; i < view->num_lights; i++, l++) {

    RenderLightUniform *out;
    if (l->bsp_light) {
      const ptrdiff_t index = (ptrdiff_t) (l->bsp_light - r_models.world->bsp->lights);
      out = &bsp_lights->lights[index];
    } else {
      if (num_dynamic_lights == MAX_DYNAMIC_LIGHTS) {
        Com_Debug(DEBUG_RENDERER, "MAX_DYNAMIC_LIGHTS\n");
        continue;
      }
      out = &dynamic_lights->lights[num_dynamic_lights++];
    }

    out->origin = Vec3_ToVec4(l->origin, l->radius);
    out->color = Vec3_ToVec4(l->color, l->intensity);

    // a portal view cannot occlude at all: the queries were resolved for another camera, so it
    // culls its own frustum and nothing more
    if (view->type == VIEW_PORTAL) {
      l->occluded = R_CullBox(view, l->bounds);
    } else if (l->bsp_light) {
      l->occluded = !l->bsp_light->query->result;
    } else {
      l->occluded = R_CulludeBox(view, l->bounds);
    }

    if (l->occluded) {
      r_stats->lights_occluded++;
    } else {
      r_stats->lights_visible++;
    }

    if (l->flags & R_LIGHT_NO_SHADOW) {
      l->tile = MakeVec2(-1.f, -1.f);
    } else {
      const int32_t light_col = i % SHADOW_ATLAS_LIGHTS_PER_ROW;
      const int32_t light_row = i / SHADOW_ATLAS_LIGHTS_PER_ROW;
      l->tile = MakeVec2((float) (light_col * r_shadow_atlas.tile_size),
                     (float) (light_row * r_shadow_atlas.tile_size));
    }

    out->tile = l->tile;

    R_UpdateLightEntities(view, l, i);
  }

  dynamic_lights->num_lights = num_dynamic_lights;

  const uint32_t bsp_size = offsetof(RenderBspLightsUniformBlock, lights) + bsp_lights->num_lights * sizeof(RenderLightUniform);
  R_UploadLightBlock(copyPass, r_lights.bsp_buffer, bsp_lights, bsp_size);

  const uint32_t dynamic_size = offsetof(RenderDynamicLightsUniformBlock, lights) + dynamic_lights->num_lights * sizeof(RenderLightUniform);
  R_UploadLightBlock(copyPass, r_lights.dynamic_buffer, dynamic_lights, dynamic_size);

  if (r_models.world) {
    const RenderBspInlineModel *in = &r_models.world->bsp->inline_models[0];

    RenderBspBlock *block = in->blocks;
    for (int32_t i = 0; i < in->num_blocks; i++, block++) {

      // a portal view cannot use occlusion queries resolved for another camera
      const bool culled = view->type == VIEW_PORTAL
        ? R_CullBox(view, block->visible_bounds)
        : block->query->result == 0;

      if (culled) {
        continue;
      }

      R_ActiveDynamicLights(view, block->visible_bounds, &block->active_dynamic_lights);
    }
  }
}

/**
 * @brief Initializes the BSP and dynamic lights storage buffers.
 */
void R_InitLights(void) {

  memset(&r_lights, 0, sizeof(r_lights));

  r_lights.bsp_buffer = $(r_context.device, createBuffer, &(SDL_GPUBufferCreateInfo) {
    .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
    .size = sizeof(r_lights.bsp_block),
  });

  r_lights.dynamic_buffer = $(r_context.device, createBuffer, &(SDL_GPUBufferCreateInfo) {
    .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
    .size = sizeof(r_lights.dynamic_block),
  });

  r_lights.transfer_buffer = $(r_context.device, createTransferBuffer, &(SDL_GPUTransferBufferCreateInfo) {
    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    .size = Maxi(sizeof(r_lights.bsp_block), sizeof(r_lights.dynamic_block)),
  });

  const int32_t no_lights[2] = { 0, 0 };
  r_lights.voxel_fallback_buffer = $(r_context.device, createBufferWithConstMem,
      SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, no_lights, sizeof(no_lights));
}

/**
 * @brief Frees the BSP and dynamic lights storage buffers.
 */
void R_ShutdownLights(void) {

  r_lights.bsp_buffer = release(r_lights.bsp_buffer);
  r_lights.dynamic_buffer = release(r_lights.dynamic_buffer);
  r_lights.transfer_buffer = release(r_lights.transfer_buffer);
  r_lights.voxel_fallback_buffer = release(r_lights.voxel_fallback_buffer);
}
