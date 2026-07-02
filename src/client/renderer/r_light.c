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
 * @brief Transform lights into their uniform representation and upload them to
 * the lights storage buffer.
 * @remarks TODO(#864): occlusion culling (l->query, r_draw_light_bounds) is
 * deferred; every light is uploaded and assigned a shadow atlas tile.
 */
void R_UpdateLights(r_view_t *view) {

  r_light_uniform_block_t *block = &r_lights.block;
  memset(block, 0, sizeof(*block));

  const float inv_layer = r_shadow_atlas.layer_size > 0 ? 1.f / (float) r_shadow_atlas.layer_size : 0.f;

  const r_light_t *l = view->lights;
  for (int32_t i = 0; i < view->num_lights; i++, l++) {
    r_light_uniform_t *out = &block->lights[i];
    out->origin = Vec3_ToVec4(l->origin, l->radius);
    out->color = Vec3_ToVec4(l->color, l->intensity);

    if (inv_layer > 0.f && !(l->flags & R_LIGHT_NO_SHADOW)) {
      const int32_t local_index = i % r_shadow_atlas.lights_per_layer;
      const int32_t light_col = local_index % r_shadow_atlas.lights_per_row;
      const int32_t light_row = local_index / r_shadow_atlas.lights_per_row;
      const float base_x = (float) (light_col * 3 * r_shadow_atlas.tile_size) * inv_layer;
      const float base_y = (float) (light_row * 2 * r_shadow_atlas.tile_size) * inv_layer;
      const float tile_uv = (float) r_shadow_atlas.tile_size * inv_layer;
      const float layer = (float) (i / r_shadow_atlas.lights_per_layer);
      out->shadow = Vec4(base_x, base_y, tile_uv, layer);
    } else {
      out->shadow = Vec4(0.f, 0.f, 0.f, 0.f);
    }
  }

  const uint32_t size = view->num_lights * sizeof(r_light_uniform_t);
  if (size) {
    $(r_lights.buffer, upload, block, size, 0, true);
  }
}

/**
 * @brief Writes the indexes of active lights intersecting bounds to the given uniform name.
 * @details In normal gameplay, these are dynamic light sources (rockets, explosions, etc).
 * @details When the in-game editor is enabled, all lights use this code path.
 * @remarks The uniform must be an `int[MAX_DYNAMIC_LIGHTS]`.
 */
void R_ActiveLights(const r_view_t *view, const box3_t bounds, int32_t name) {

  // TODO(#864): dynamic (active) lights not yet ported to the SDL_gpu path;
  // only static voxel-clustered BSP lights are drawn (see bsp_fs.glsl).
}

/**
 * @brief Initializes the lights storage buffer.
 */
void R_InitLights(void) {

  memset(&r_lights, 0, sizeof(r_lights));

  r_lights.buffer = $(r_device.device, createBuffer, &(SDL_GPUBufferCreateInfo) {
    .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
    .size = sizeof(r_lights.block),
  });
}

/**
 * @brief Frees the lights storage buffer.
 */
void R_ShutdownLights(void) {

  r_lights.buffer = release(r_lights.buffer);
}
