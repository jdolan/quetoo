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

#include <Objectively/Array.h>
#include <Objectively/Vector.h>

#include "r_local.h"

r_occlusion_t r_occlusion;

/**
 * @brief Checks BSP block occlusion queries to determine if the given box is occluded.
 * @details Note that we check the node's bounds, not the query's bounds. This is because the query
 * is tightly bound to just the BSP faces within each block, but here we want to test against each
 * block's spatial partition.
 */
bool R_OccludeBox(const r_view_t *view, const box3_t bounds) {

  if (!r_occlude->integer) {
    return false;
  }

  if (view->type == VIEW_PLAYER_MODEL) {
    return false;
  }

  const r_bsp_inline_model_t *in = r_models.world->bsp->inline_models;

  const r_bsp_block_t *block = in->blocks;
  for (int32_t i = 0; i < in->num_blocks; i++, block++) {

    if (!block->query->result) {
      if (Box3_Contains(block->query->bounds, bounds)) {
        return true;
      }
      continue;
    }

    if (Box3_Intersects(block->query->bounds, bounds)) {
      return false;
    }
  }

  return true;
}

/**
 * @brief Tests whether the given sphere origin is occluded by any occlusion query result.
 */
bool R_OccludeSphere(const r_view_t *view, const vec3_t origin, float radius) {
  return R_OccludeBox(view, Box3_FromCenterRadius(origin, radius));
}

/**
 * @return True if the specified box is occluded *or* culled by the view frustum, false otherwise.
 */
bool R_CulludeBox(const r_view_t *view, const box3_t bounds) {
  return R_CullBox(view, bounds) || R_OccludeBox(view, bounds);
}

/**
 * @return True if the specified sphere is occluded *or* culled by the view frustum, false otherwise.
 */
bool R_CulludeSphere(const r_view_t *view, const vec3_t point, const float radius) {
  return R_CullSphere(view, point, radius) || R_OccludeSphere(view, point, radius);
}

/**
 * @brief Allocates an occlusion query with the specified bounds.
 */
r_occlusion_query_t *R_AllocOcclusionQuery(const box3_t bounds) {

  GPU_Assert(r_occlusion.num_queries < MAX_OCCLUSION_QUERIES, "Exceeded MAX_OCCLUSION_QUERIES (%d)", MAX_OCCLUSION_QUERIES);

  r_occlusion_query_t *query = &r_occlusion.queries[r_occlusion.num_queries++];

  query->bounds = bounds;
  query->first_box = (int32_t) r_occlusion.boxes->count;
  query->num_boxes = 0;
  query->result = true;

  return query;
}

/**
 * @brief Appends a box to the given query's GPU-drawn instance geometry.
 */
void R_AppendOcclusionQueryBox(r_occlusion_query_t *query, box3_t bounds) {

  assert(query->first_box + query->num_boxes == (int32_t) r_occlusion.boxes->count);

  $(r_occlusion.boxes, add, &bounds);
  query->num_boxes++;
}

/**
 * @brief Resets the occlusion query pool, invalidating all previously allocated queries.
 */
void R_FreeOcclusionQueries(void) {

  r_occlusion.num_queries = 0;

  if (r_occlusion.boxes) {
    $(r_occlusion.boxes, removeAll);
  }

  r_occlusion.instanceBuffer = release(r_occlusion.instanceBuffer);
}

/**
 * @brief Builds the per-instance occlusion box buffer for the boxes the load appended.
 */
void R_LoadOcclusionQueries(void) {

  r_occlusion.instanceBuffer = release(r_occlusion.instanceBuffer);

  const int32_t num_boxes = (int32_t) r_occlusion.boxes->count;
  if (num_boxes) {
    r_occlusion.instanceBuffer = $(r_context.device, createBufferWithConstMem,
      SDL_GPU_BUFFERUSAGE_VERTEX, r_occlusion.boxes->elements, (Uint32) (num_boxes * sizeof(box3_t)));
  }
}

/**
 * @brief
 */
static void R_DrawOcclusionQueries_(const r_view_t *view, CommandBuffer *commands) {

  SDL_GPUDepthStencilTargetInfo depth = $(view->framebuffer, depthTargetInfo, SDL_GPU_LOADOP_LOAD, SDL_GPU_STOREOP_STORE, 1.f);

#ifdef SDL_GPU_QUERY_API
  depth.queryPool = r_occlusion.pool->pool;
#endif

  RenderPass *pass = $(commands, beginRenderPass, NULL, 0, &depth);

  if (r_occlusion.instanceBuffer) {
    $(pass, bindVertexBuffers, 0, (SDL_GPUBufferBinding[]) {
      { .buffer = r_occlusion.vertexBuffer->buffer },
      { .buffer = r_occlusion.instanceBuffer->buffer },
    }, 2);
    $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) {
      .buffer = r_occlusion.elementsBuffer->buffer
    }, SDL_GPU_INDEXELEMENTSIZE_32BIT);
  }

  $(commands, pushVertexUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

  $(pass, bindPipeline, r_occlusion.pipeline);

  r_occlusion_query_t *q = r_occlusion.queries;
  for (int32_t i = 0; i < r_occlusion.num_queries; i++, q++) {
    $(pass, beginQuery, r_occlusion.pool, i);
    $(pass, drawIndexedPrimitives, 36, q->num_boxes, 0, 0, q->first_box);
    $(pass, endQuery, r_occlusion.pool, i);
  }

  release(pass);
}

/**
 * @brief Draws and polls all occlusion queries for the current frame.
 */
void R_DrawOcclusionQueries(const r_view_t *view, CommandBuffer *commands) {

  if (r_depth_pipeline.fence) {

    if ($(r_depth_pipeline.fence, query)) {

      const Uint64 *results = $(r_occlusion.transfer, map, false);

      for (int32_t i = 0; i < r_occlusion.num_queries; i++) {
        r_occlusion.queries[i].result = results[i] > 0;
      }

      $(r_occlusion.transfer, unmap);

      r_depth_pipeline.fence = release(r_depth_pipeline.fence);

      if (r_occlude->integer) {
        R_DrawOcclusionQueries_(view, commands);
      }

      CopyPass *pass = $(commands, beginCopyPass);
      $(pass, downloadQueryResults, r_occlusion.pool, 0, r_occlusion.num_queries, &(SDL_GPUTransferBufferLocation) {
        .transfer_buffer = r_occlusion.transfer->buffer,
      });
      release(pass);
    }
  }

  r_occlusion_query_t *q = r_occlusion.queries;
  for (int32_t i = 0; i < r_occlusion.num_queries; i++, q++) {

    if (!r_occlude->integer) {
      q->result = true;
    } else {
      if (Box3_Intersects(q->bounds, Box3_FromCenterRadius(view->origin, BSP_VOXEL_SIZE))) {
        q->result = true;
      } else if (R_CullBox(view, q->bounds)) {
        q->result = false;
      }
    }
  }

  if (r_draw_stats->value) {
    const r_occlusion_query_t *q = r_occlusion.queries;
    for (int32_t i = 0; i < r_occlusion.num_queries; i++, q++) {
      r_stats.queries_allocated++;
      if (q->result) {
        r_stats.queries_visible++;
      } else {
        r_stats.queries_occluded++;
      }
    }
  }

  if (r_draw_occlusion_queries->value) {
    const r_occlusion_query_t *q = r_occlusion.queries;
    for (int32_t i = 0; i < r_occlusion.num_queries; i++, q++) {
      const float dist = Vec3_Distance(Box3_Center(q->bounds), view->origin);
      const float f = 1.f - Clampf01(dist / MAX_WORLD_COORD);
      if (!q->result) {
        R_Draw3DBox(q->bounds, Color3f(0.f, f, 0.f), false);
      } else {
        R_Draw3DBox(q->bounds, Color3f(f, 0.f, 0.f), false);
      }
    }
  }

  if (r_draw_bsp_blocks->value) {
    r_bsp_block_t *b = r_models.world->bsp->inline_models->blocks;
    for (int32_t i = 0; i < r_models.world->bsp->inline_models->num_blocks; i++, b++) {
      const float dist = Vec3_Distance(Box3_Center(b->visible_bounds), view->origin);
      const float f = 1.f - Clampf01(dist / MAX_WORLD_COORD);
      if (!b->query->result) {
        R_Draw3DBox(b->visible_bounds, Color3f(0.f, f, 0.f), false);
      } else {
        R_Draw3DBox(b->visible_bounds, Color3f(f, 0.f, 0.f), false);
      }
    }
  }
}

/**
 * @brief Initializes occlusion query state, allocating the shared query pool, occlusion
 * box geometry, and the pipeline used to draw it.
 */
void R_InitOcclusionQueries(void) {

  memset(&r_occlusion, 0, sizeof(r_occlusion));

  r_occlusion.boxes = $(alloc(Vector), initWithSize, sizeof(box3_t));

  r_occlusion.pool = $(r_context.device, createQueryPool, &(SDL_GPUQueryPoolCreateInfo) {
    .type = SDL_GPU_QUERY_PRECISE_OCCLUSION,
    .query_count = MAX_OCCLUSION_QUERIES,
  });

  // The constant unit cube, ordered to match Box3_ToPoints (bit 0 = X, bit 1 = Y,
  // bit 2 = Z; unset = mins, set = maxs). Scaled and offset per-instance in
  // occlude_vs.glsl by the (mins, maxs) pulled from instance_buffer.
  vec3_t cube[8];
  Box3_ToPoints(Box3(Vec3(0.f, 0.f, 0.f), Vec3(1.f, 1.f, 1.f)), cube);

  r_occlusion.vertexBuffer = $(r_context.device, createBufferWithConstMem,
    SDL_GPU_BUFFERUSAGE_VERTEX, cube, sizeof(cube));

  const uint32_t elements[] = {
    // bottom
    0, 1, 3, 0, 3, 2,
    // top
    6, 7, 4, 7, 5, 4,
    // front
    4, 5, 0, 5, 1, 0,
    // back
    7, 6, 3, 6, 2, 3,
    // left
    6, 4, 2, 4, 0, 2,
    // right
    5, 7, 1, 7, 3, 1,
  };

  r_occlusion.elementsBuffer = $(r_context.device, createBufferWithConstMem,
    SDL_GPU_BUFFERUSAGE_INDEX, elements, sizeof(elements));

  r_occlusion.transfer = $(r_context.device, createTransferBuffer, &(SDL_GPUTransferBufferCreateInfo) {
    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
    .size = MAX_OCCLUSION_QUERIES * sizeof(Uint64),
  });

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/occlude_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 1,
  });

  Shader *fragmentShader = $(r_context.device, loadShader, "shaders/depth_pass_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
  });

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = r_scene_samples;
  info.vertex_shader = vertexShader->shader;
  info.fragment_shader = fragmentShader->shader;

  // Occlusion boxes are synthetic geometry with no guaranteed winding; cull
  // nothing rather than risk silently invisible (and thus useless) queries.
  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {
      {
        .slot = 0,
        .pitch = sizeof(vec3_t),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
      },
      {
        .slot = 1,
        .pitch = sizeof(box3_t),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE,
      },
    },
    .num_vertex_buffers = 2,
    .vertex_attributes = (SDL_GPUVertexAttribute[]) {
      {
        .location = 0,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .offset = 0,
      },
      {
        .location = 1,
        .buffer_slot = 1,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .offset = offsetof(box3_t, mins),
      },
      {
        .location = 2,
        .buffer_slot = 1,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .offset = offsetof(box3_t, maxs),
      },
    },
    .num_vertex_attributes = 3,
  };

  // Test against the depth already written by the real scene, but never write
  // to it or color: the box's only purpose is to report whether it *would*
  // have been visible.
  info.depth_stencil_state = (SDL_GPUDepthStencilState) {
    .compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
    .enable_depth_test = true,
    .enable_depth_write = false,
  };

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .num_color_targets = 0,
    .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
    .has_depth_stencil_target = true,
  };

  r_occlusion.pipeline = $(r_context.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);
}

/**
 * @brief Shuts down occlusion query state, freeing all GPU resources.
 */
void R_ShutdownOcclusionQueries(void) {

  r_occlusion.pipeline = release(r_occlusion.pipeline);
  r_occlusion.vertexBuffer = release(r_occlusion.vertexBuffer);
  r_occlusion.instanceBuffer = release(r_occlusion.instanceBuffer);
  r_occlusion.elementsBuffer = release(r_occlusion.elementsBuffer);
  r_occlusion.pool = release(r_occlusion.pool);
  r_occlusion.transfer = release(r_occlusion.transfer);
  r_occlusion.boxes = release(r_occlusion.boxes);
}
