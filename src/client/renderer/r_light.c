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

r_lights_t r_lights;

/**
 * @brief Adds a light source to the view's light list.
 */
void R_AddLight(r_view_t *view, const r_light_t *l) {

  if (view->num_lights == MAX_LIGHTS) {
    Com_Debug(DEBUG_RENDERER, "MAX_LIGHTS\n");
    return;
  }

  r_light_t *out = &view->lights[view->num_lights++];

  *out = *l;
}

/**
 * @brief Uploads light buffers and caches per-block and per-entity dynamic
 * light masks for the frame.
 */
void R_UpdateLights(r_view_t *view, CopyPass *copyPass) {

  r_bsp_lights_uniform_block_t *bsp_block = &r_lights.bsp_block;
  r_dynamic_lights_uniform_block_t *dynamic_block = &r_lights.dynamic_block;

  memset(bsp_block, 0, sizeof(*bsp_block));
  memset(dynamic_block, 0, sizeof(*dynamic_block));

  bsp_block->num_bsp_lights = r_models.world ? r_models.world->bsp->num_lights : 0;

  int32_t num_dynamic = 0;

  r_light_t *l = view->lights;
  for (int32_t i = 0; i < view->num_lights; i++, l++) {

    r_light_uniform_t *out;
    if (l->bsp_light) {
      const int32_t slot = (int32_t) (l->bsp_light - r_models.world->bsp->lights);
      assert(slot < bsp_block->num_bsp_lights);
      out = &bsp_block->bsp_lights[slot];
    } else {
      if (num_dynamic == MAX_DYNAMIC_LIGHTS) {
        Com_Debug(DEBUG_RENDERER, "MAX_DYNAMIC_LIGHTS\n");
        continue;
      }
      out = &dynamic_block->dynamic_lights[num_dynamic++];
    }

    out->origin = Vec3_ToVec4(l->origin, l->radius);
    out->color = Vec3_ToVec4(l->color, l->intensity);

    if (l->bsp_light) {
      l->occluded = !l->bsp_light->query->result;
    } else {
      l->occluded = R_CulludeBox(view, l->bounds);
    }

    if (l->occluded) {
      r_stats.lights_occluded++;
    } else {
      r_stats.lights_visible++;
    }

    const int32_t light_col = i % SHADOW_ATLAS_LIGHTS_PER_ROW;
    const int32_t light_row = i / SHADOW_ATLAS_LIGHTS_PER_ROW;
    l->tile = Vec2((float) (light_col * r_shadow_atlas.tile_size),
                   (float) (light_row * r_shadow_atlas.tile_size));

    out->shadow = (l->flags & R_LIGHT_NO_SHADOW) ? Vec2(-1.f, -1.f) : l->tile;
  }

  dynamic_block->num_dynamic_lights = num_dynamic;

  const uint32_t bsp_size = offsetof(r_bsp_lights_uniform_block_t, bsp_lights) + bsp_block->num_bsp_lights * sizeof(r_light_uniform_t);
  $(r_lights.bsp_buffer, uploadWithPass, copyPass, bsp_block, bsp_size, 0, true);

  const uint32_t dynamic_size = offsetof(r_dynamic_lights_uniform_block_t, dynamic_lights) + dynamic_block->num_dynamic_lights * sizeof(r_light_uniform_t);
  $(r_lights.dynamic_buffer, uploadWithPass, copyPass, dynamic_block, dynamic_size, 0, true);

  if (r_models.world) {
    const r_bsp_inline_model_t *in = &r_models.world->bsp->inline_models[0];

    r_bsp_block_t *block = in->blocks;
    for (int32_t i = 0; i < in->num_blocks; i++, block++) {
      R_ActiveDynamicLights(view, block->visible_bounds, block->active_dynamic_lights);
    }
  }

  r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {
    R_ActiveDynamicLights(view, e->abs_model_bounds, e->active_dynamic_lights);
  }
}

/**
 * @brief Builds the dynamic light bitmask for the given bounds.
 */
void R_ActiveDynamicLights(const r_view_t *view, const box3_t bounds, uint32_t mask[MAX_DYNAMIC_LIGHTS / 32]) {

  memset(mask, 0, sizeof(uint32_t) * (MAX_DYNAMIC_LIGHTS / 32));

  int32_t j = 0;

  const r_light_t *l = view->lights;
  for (int32_t i = 0; i < view->num_lights; i++, l++) {

    if (l->bsp_light) {
      continue;
    }

    if (j == MAX_DYNAMIC_LIGHTS) {
      break;
    }

    if (Box3_Intersects(l->bounds, bounds)) {
      mask[j >> 5] |= 1u << (j & 31);
    }

    j++;
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
}

/**
 * @brief Frees the BSP and dynamic lights storage buffers.
 */
void R_ShutdownLights(void) {

  r_lights.bsp_buffer = release(r_lights.bsp_buffer);
  r_lights.dynamic_buffer = release(r_lights.dynamic_buffer);
}
