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
 */
void R_UpdateLights(r_view_t *view) {

  r_light_uniform_block_t *block = &r_lights.block;
  memset(block, 0, sizeof(*block));

  block->num_bsp_lights = r_models.world ? r_models.world->bsp->num_lights : 0;

  cm_trace_t tr = { 0 };
  if (r_draw_light_bounds->value) {
    const vec3_t end = Vec3_Fmaf(view->origin, MAX_WORLD_DIST, view->forward);
    tr = Cm_BoxTrace(view->origin, end, Box3_Zero(), 0, CONTENTS_SOLID);
  }

  int32_t num_dynamic = 0;

  r_light_t *l = view->lights;
  for (int32_t i = 0; i < view->num_lights; i++, l++) {

    // BSP lights occupy their lump position, so that the compiled voxel light
    // indices address them correctly even when the cgame skips or re-adds lump
    // lights as dynamic (e.g. targeted lights); dynamic lights pack the slots
    // at [num_bsp_lights, ...) in view order, matching R_ActiveLights.
    int32_t slot;
    if (l->bsp_light) {
      slot = (int32_t) (l->bsp_light - r_models.world->bsp->lights);
      assert(slot < block->num_bsp_lights);
    } else {
      if (num_dynamic == MAX_DYNAMIC_LIGHTS) {
        Com_Debug(DEBUG_RENDERER, "MAX_DYNAMIC_LIGHTS\n");
        continue;
      }
      slot = block->num_bsp_lights + num_dynamic++;
    }

    r_light_uniform_t *out = &block->lights[slot];
    out->origin = Vec3_ToVec4(l->origin, l->radius);
    out->color = Vec3_ToVec4(l->color, l->intensity);

    // Static lights consult their persistent occlusion query; dynamic lights
    // (no bsp_light) are tested fresh against the view each frame.
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

    // The light's tile is the same in every atlas face texture; the shader
    // normalizes these pixel coordinates to UV space itself via textureSize().
    const int32_t light_col = i % SHADOW_ATLAS_LIGHTS_PER_ROW;
    const int32_t light_row = i / SHADOW_ATLAS_LIGHTS_PER_ROW;
    l->tile = Vec2((float) (light_col * r_shadow_atlas.tile_size),
                   (float) (light_row * r_shadow_atlas.tile_size));

    out->shadow = (l->flags & R_LIGHT_NO_SHADOW) ? Vec2(-1.f, -1.f) : l->tile;

    if (r_draw_light_bounds->value && Vec3_Distance(tr.end, l->origin) < 64.f) {
      R_Draw3DBox(l->bounds, Color3fv(l->color), false);
    }
  }

  // The shaders derive the dynamic count as num_lights - num_bsp_lights, so
  // num_lights spans the occupied slots, not the view's light count (lump
  // lights the cgame skipped leave zeroed, zero-radius gaps).
  block->num_lights = block->num_bsp_lights + num_dynamic;

  // Upload the count header plus the populated lights. Always upload at least
  // the header so num_lights reaches the shader (0 on a light-free frame).
  // Recorded on the frame's command buffer: Buffer::upload would acquire and
  // submit its own, an extra queue flush every frame.
  const uint32_t size = offsetof(r_light_uniform_block_t, lights)
      + block->num_lights * sizeof(r_light_uniform_t);

  CopyPass *copyPass = $(r_context.device->commands, beginCopyPass);
  $(copyPass, uploadData, r_lights.buffer->buffer, block, size, 0, true);
  release(copyPass);
}

/**
 * @brief Builds the dynamic-light cull bitmask for the given bounds into `mask`.
 * @details Dynamic light sources (rockets, explosions, targeted BSP lights, etc.)
 * occupy the buffer slots at `[num_bsp_lights, num_lights)` and have no voxel
 * grid, so each draw whittles them to those intersecting its bounds. Bit `j` of
 * the bitmask selects dynamic light `[num_bsp_lights + j]`; the lighting shaders
 * add `num_bsp_lights` to recover the absolute index. Dynamic lights may be
 * interleaved with static (voxel-gridded) BSP lights in the view, so `j` counts
 * them in view order, matching the slot assignment in R_UpdateLights.
 */
void R_ActiveLights(const r_view_t *view, const box3_t bounds, uint32_t mask[MAX_DYNAMIC_LIGHTS / 32]) {

  memset(mask, 0, sizeof(uint32_t) * (MAX_DYNAMIC_LIGHTS / 32));

  int32_t j = 0;

  const r_light_t *l = view->lights;
  for (int32_t i = 0; i < view->num_lights; i++, l++) {

    // Static (voxel-gridded) BSP lights are skipped.
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
