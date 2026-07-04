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
 * @remarks TODO(#864): occlusion culling (l->query) is deferred; every light is
 * uploaded and assigned a shadow atlas tile.
 */
void R_UpdateLights(r_view_t *view) {

  r_light_uniform_block_t *block = &r_lights.block;
  memset(block, 0, sizeof(*block));

  block->num_lights = view->num_lights;
  block->num_bsp_lights = r_models.world ? r_models.world->bsp->num_lights : 0;

  cm_trace_t tr = { 0 };
  if (r_draw_light_bounds->value) {
    const vec3_t end = Vec3_Fmaf(view->origin, MAX_WORLD_DIST, view->forward);
    tr = Cm_BoxTrace(view->origin, end, Box3_Zero(), 0, CONTENTS_SOLID);
  }

  const float inv_layer = r_shadow_atlas.layer_size > 0 ? 1.f / (float) r_shadow_atlas.layer_size : 0.f;

  const r_light_t *l = view->lights;
  for (int32_t i = 0; i < view->num_lights; i++, l++) {
    r_light_uniform_t *out = &block->lights[i];
    out->origin = Vec3_ToVec4(l->origin, l->radius);
    out->color = Vec3_ToVec4(l->color, l->intensity);

    if (r_draw_light_bounds->value && Vec3_Distance(tr.end, l->origin) < 64.f) {
      R_Draw3DBox(l->bounds, Color3fv(l->color), false);
    }

    // Every shadow-casting light (static or dynamic) gets an atlas tile; they
    // are visually identical and differ only in optimization (voxel selection,
    // tile caching). The single-layer atlas holds lights_per_layer tiles; lights
    // beyond that get no tile (shadow.z == 0 => lit) rather than a colliding one.
    if (inv_layer > 0.f && i < r_shadow_atlas.lights_per_layer && !(l->flags & R_LIGHT_NO_SHADOW)) {
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

  // Upload the count header plus the populated lights. Always upload at least
  // the header so num_lights reaches the shader (0 on a light-free frame).
  const uint32_t size = offsetof(r_light_uniform_block_t, lights)
      + view->num_lights * sizeof(r_light_uniform_t);
  $(r_lights.buffer, upload, block, size, 0, true);
}

/**
 * @brief Builds the dynamic-light cull bitmask for the given bounds into `mask`.
 * @details Dynamic light sources (rockets, explosions, etc.) occupy the tail of
 * the lights buffer and have no voxel grid, so each draw whittles them to those
 * intersecting its bounds. Bit `j` of the uvec4 bitmask selects dynamic light
 * `[num_bsp_lights + j]`; the lighting shaders add `num_bsp_lights` to recover the
 * absolute index. Static (voxel-gridded) lights are skipped.
 */
void R_ActiveLights(const r_view_t *view, const box3_t bounds, uint32_t mask[4]) {

  mask[0] = mask[1] = mask[2] = mask[3] = 0;

  const int32_t first = r_lights.block.num_bsp_lights;

  const r_light_t *l = view->lights + first;
  for (int32_t i = first; i < view->num_lights; i++, l++) {

    const int32_t j = i - first;
    if (j >= MAX_DYNAMIC_LIGHTS) {
      break;
    }

    // The lights at [num_bsp_lights, num_lights) are the dynamic tail; none are
    // static (voxel-gridded) BSP lights.
    assert(!l->bsp_light);

    if (Box3_Intersects(l->bounds, bounds)) {
      mask[j >> 5] |= 1u << (j & 31);
    }
  }
}

/**
 * @brief Initializes the lights storage buffer.
 */
void R_InitLights(void) {

  memset(&r_lights, 0, sizeof(r_lights));

  r_lights.buffer = $(r_context.device, createBuffer, &(SDL_GPUBufferCreateInfo) {
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
