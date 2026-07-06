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

/**
 * @brief The maximum number of concurrently allocated occlusion queries: one
 * per BSP block plus one per BSP light, the only two things that ever call
 * R_AllocOcclusionQuery.
 */
#define MAX_OCCLUSION_QUERIES (MAX_BSP_BLOCKS + MAX_BSP_LIGHTS)

/**
 * @brief The occlusion query accounting structure.
 * @details Occlusion queries are maintained in a dynamic pool that will grow to meet the demands
 * of the scene. Lists make allocating and freeing queries a constant O(1) operation.
 * @details Unlike GL, SDL_gpu query pools are fixed-size and index-addressed rather than
 * individually created/destroyed objects, so each r_occlusion_query_t is permanently
 * assigned a slot index into the single shared QueryPool the first time it's allocated;
 * returning it to the free list for reuse keeps that index for the life of the process.
 * @details SDL_gpu has no equivalent of GL_QUERY_RESULT_AVAILABLE (a non-blocking poll of
 * a single query), so results are instead downloaded for the whole active range once per
 * frame into one of two ping-ponged transfer buffers, and read back one frame later --
 * by then the GPU work that produced them has long since been submitted, giving the same
 * "effectively free" latency-hiding the original deferred-retrieval design intends,
 * without an explicit fence wait.
 */
static struct {

  /**
   * @brief The allocated occlusion queries.
   */
  List *allocated;

  /**
   * @brief The free occlusion queries.
   */
  List *free;

  /**
   * @brief If true, rebuild the vertex buffer at next frame.
   */
  bool dirty;

  /**
   * @brief The next unused slot index into `query_pool`. Only grows; freed
   * queries are recycled by index via the `free` list, not by decrementing this.
   */
  int32_t next_index;

  /**
   * @brief The shared occlusion query pool.
   */
  QueryPool *query_pool;

  /**
   * @brief The vertex buffer of occlusion box corners, rebuilt when dirty.
   */
  Buffer *vertex_buffer;

  /**
   * @brief The shared index buffer for a single box (reused for every query via base vertex).
   */
  Buffer *elements_buffer;

  /**
   * @brief The position-only, depth-only pipeline used to draw occlusion boxes.
   */
  GraphicsPipeline *pipeline;

  /**
   * @brief Ping-ponged download targets for query results; see the struct's details above.
   */
  SDL_GPUTransferBuffer *transfer[2];

  /**
   * @brief The frame counter driving which `transfer` buffer is downloaded into vs. read from.
   */
  int32_t frame;
} r_occlusion_queries;

/**
 * @brief Pops a query from the given list, if available.
 */
static r_occlusion_query_t *R_PopOcclusionQuery(List *queries) {

  if (queries == NULL || queries->head == NULL) {
    return NULL;
  }

  ListNode *node = queries->head;
  r_occlusion_query_t *query = node->element;

  $(queries, removeNode, node);

  return query;
}

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

  r_occlusion_query_t *query = R_PopOcclusionQuery(r_occlusion_queries.free);
  if (query == NULL) {
    GPU_Assert(r_occlusion_queries.next_index < MAX_OCCLUSION_QUERIES, "Exceeded MAX_OCCLUSION_QUERIES (%d)", MAX_OCCLUSION_QUERIES);

    query = Mem_TagMalloc(sizeof(r_occlusion_query_t), MEM_TAG_RENDERER);
    query->index = r_occlusion_queries.next_index++;
  }

  query->bounds = bounds;
  query->available = true;
  query->result = true;

  r_occlusion_queries.dirty = true;

  $(r_occlusion_queries.allocated, prepend, query);
  return query;
}

/**
 * @brief Frees the specified occlusion query.
 */
void R_FreeOcclusionQuery(r_occlusion_query_t *query) {

  if (!query) {
    return;
  }

  ListNode *node = $(r_occlusion_queries.allocated, nodeForElement, query);
  if (node) {
    $(r_occlusion_queries.allocated, removeNode, node);
  }
  $(r_occlusion_queries.free, prepend, query);

  r_occlusion_queries.dirty = true;
}

/**
 * @brief Rebuilds the occlusion box vertex buffer when queries are added or removed.
 */
void R_UpdateOcclusionQueries(const r_view_t *view) {

  if (!r_occlusion_queries.dirty) {
    return;
  }

  release(r_occlusion_queries.vertex_buffer);
  r_occlusion_queries.vertex_buffer = NULL;

  const size_t num_queries = r_occlusion_queries.allocated->count;
  if (num_queries) {

    vec3_t *vertexes = malloc(num_queries * sizeof(vec3_t) * 8);

    int32_t base_vertex = 0;
    for (const ListNode *node = r_occlusion_queries.allocated->head; node; node = node->next) {
      r_occlusion_query_t *query = node->element;

      query->base_vertex = base_vertex;
      Box3_ToPoints(query->bounds, vertexes + base_vertex);
      base_vertex += 8;
    }

    r_occlusion_queries.vertex_buffer = $(r_context.device, createBufferWithConstMem,
      SDL_GPU_BUFFERUSAGE_VERTEX, vertexes, (Uint32) (num_queries * sizeof(vec3_t) * 8));

    free(vertexes);
  }

  r_occlusion_queries.dirty = false;
}

/**
 * @brief Draws and polls all occlusion queries for the current frame.
 * @details Renders each allocated query's bounding box into the view framebuffer's depth
 * target (already populated by the real world geometry in R_DrawDepthPass, LOADed here so
 * those results persist), bracketing each box draw in a query so the next frame's
 * R_OccludeBox/R_OccludeSphere calls can consult `query->result`. Boxes that can be
 * trivially resolved (occlusion disabled, the view origin is inside the box, or the box is
 * already frustum-culled) skip the GPU query entirely.
 */
void R_DrawOcclusionQueries(const r_view_t *view) {

  if (!r_occlusion_queries.pipeline || !view->framebuffer) {
    return;
  }

  CommandBuffer *commands = r_context.device->commands;
  if (!commands) {
    return;
  }

  // Read back last frame's downloaded results before reissuing this frame's queries.
  if (r_occlusion_queries.frame > 0) {
    const int32_t read = (r_occlusion_queries.frame + 1) % 2;
    const Uint64 *results = $(r_context.device, mapTransferBuffer, r_occlusion_queries.transfer[read], false);

    for (const ListNode *node = r_occlusion_queries.allocated->head; node; node = node->next) {
      r_occlusion_query_t *query = node->element;
      query->available = true;
      query->result = results[query->index] != 0;
    }

    $(r_context.device, unmapTransferBuffer, r_occlusion_queries.transfer[read]);
  }

  if (r_occlusion_queries.allocated->count) {

    const SDL_GPUDepthStencilTargetInfo depth = $(view->framebuffer, depthTargetInfo, SDL_GPU_LOADOP_LOAD, SDL_GPU_STOREOP_STORE, 1.f);

    RenderPass *pass = $(commands, beginRenderPass, NULL, 0, &depth);

    $(pass, bindPipeline, r_occlusion_queries.pipeline);
    $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = r_occlusion_queries.vertex_buffer->buffer }, 1);
    $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) { .buffer = r_occlusion_queries.elements_buffer->buffer }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    const mat4_t model = Mat4_Identity();
    $(commands, pushVertexUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));
    $(commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, model.array, sizeof(model));

    for (const ListNode *node = r_occlusion_queries.allocated->head; node; node = node->next) {
      r_occlusion_query_t *query = node->element;

      if (!r_occlude->integer) {
        query->available = true;
        query->result = true;
        continue;
      }

      if (Box3_Intersects(query->bounds, Box3_FromCenterRadius(view->origin, BSP_VOXEL_SIZE))) {
        query->available = true;
        query->result = true;
        continue;
      }

      if (R_CullBox(view, query->bounds)) {
        query->available = true;
        query->result = false;
        continue;
      }

      $(pass, beginQuery, r_occlusion_queries.query_pool, query->index);
      $(pass, drawIndexedPrimitives, 36, 1, 0, query->base_vertex, 0);
      $(pass, endQuery, r_occlusion_queries.query_pool, query->index);
    }

    release(pass);

    CopyPass *copyPass = $(commands, beginCopyPass);
    $(copyPass, downloadQueryResults, r_occlusion_queries.query_pool, 0, r_occlusion_queries.next_index, &(SDL_GPUTransferBufferLocation) {
      .transfer_buffer = r_occlusion_queries.transfer[r_occlusion_queries.frame % 2],
    });
    release(copyPass);

    if (r_draw_occlusion_queries->value) {
      for (const ListNode *node = r_occlusion_queries.allocated->head; node; node = node->next) {
        const r_occlusion_query_t *query = node->element;
        const float dist = Vec3_Distance(Box3_Center(query->bounds), view->origin);
        const float f = 1.f - Clampf01(dist / MAX_WORLD_COORD);
        if (!query->result) {
          R_Draw3DBox(query->bounds, Color3f(0.f, f, 0.f), false);
        } else {
          R_Draw3DBox(query->bounds, Color3f(f, 0.f, 0.f), false);
        }
      }
    }
  }

  if (r_draw_bsp_blocks->value && r_models.world) {
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

  r_occlusion_queries.frame++;

  r_stats.queries_allocated = (int32_t) r_occlusion_queries.allocated->count;
}

/**
 * @brief Initializes occlusion query state, allocating the shared query pool, occlusion
 * box geometry, and the pipeline used to draw it.
 */
void R_InitOcclusionQueries(void) {

  memset(&r_occlusion_queries, 0, sizeof(r_occlusion_queries));

  r_occlusion_queries.free = $(alloc(List), init);
  r_occlusion_queries.allocated = $(alloc(List), init);

  r_occlusion_queries.query_pool = $(r_context.device, createQueryPool, &(SDL_GPUQueryPoolCreateInfo) {
    .type = SDL_GPU_QUERY_PRECISE_OCCLUSION,
    .query_count = MAX_OCCLUSION_QUERIES,
  });

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

  r_occlusion_queries.elements_buffer = $(r_context.device, createBufferWithConstMem,
    SDL_GPU_BUFFERUSAGE_INDEX, elements, sizeof(elements));

  for (int32_t i = 0; i < 2; i++) {
    r_occlusion_queries.transfer[i] = $(r_context.device, createTransferBuffer, &(SDL_GPUTransferBufferCreateInfo) {
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
      .size = MAX_OCCLUSION_QUERIES * sizeof(Uint64),
    });
  }

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/depth_pass_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 2,
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
    .vertex_buffer_descriptions = &(SDL_GPUVertexBufferDescription) {
      .slot = 0,
      .pitch = sizeof(vec3_t),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    },
    .num_vertex_buffers = 1,
    .vertex_attributes = &(SDL_GPUVertexAttribute) {
      .location = 0,
      .buffer_slot = 0,
      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
      .offset = 0,
    },
    .num_vertex_attributes = 1,
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

  r_occlusion_queries.pipeline = $(r_context.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);
}

/**
 * @brief Shuts down occlusion query state, freeing all GPU resources and allocated queries.
 */
void R_ShutdownOcclusionQueries(void) {

  r_occlusion_queries.pipeline = release(r_occlusion_queries.pipeline);
  r_occlusion_queries.vertex_buffer = release(r_occlusion_queries.vertex_buffer);
  r_occlusion_queries.elements_buffer = release(r_occlusion_queries.elements_buffer);
  r_occlusion_queries.query_pool = release(r_occlusion_queries.query_pool);

  for (int32_t i = 0; i < 2; i++) {
    $(r_context.device, releaseTransferBuffer, r_occlusion_queries.transfer[i]);
  }

  r_occlusion_queries.allocated->destroy = Mem_Free;
  r_occlusion_queries.allocated = release(r_occlusion_queries.allocated);

  r_occlusion_queries.free->destroy = Mem_Free;
  r_occlusion_queries.free = release(r_occlusion_queries.free);
}
