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

RenderOcclusion rOcclusion;

/**
 * @brief Compacts this frame's world block query bounds by visibility, so that
 * `R_OccludeBox` visits only the blocks each of its passes cares about.
 */
static void R_UpdateOcclusionBounds(void) {

  rOcclusion.numOccludedBounds = 0;
  rOcclusion.numVisibleBounds = 0;

  assert(rModels.world);

  const RenderBspInlineModel *in = rModels.world->bsp->inlineModels;

  const RenderBspBlock *block = in->blocks;
  for (int32_t i = 0; i < in->numBlocks; i++, block++) {

    if (block->query->result) {
      rOcclusion.visibleBounds[rOcclusion.numVisibleBounds++] = block->query->bounds;
    } else {
      rOcclusion.occludedBounds[rOcclusion.numOccludedBounds++] = block->query->bounds;
    }
  }
}

/**
 * @brief Returns true if the box is occluded by BSP block queries.
 * @remarks An occluded block that fully contains the box occludes it, regardless
 * of block order, so both tests are resolved in priority order rather than
 * interleaved.
 */
bool R_OccludeBox(const RenderView *view, const Box3 bounds) {

  if (!r_occlude->integer) {
    return false;
  }

  if (view->type == VIEW_PLAYER_MODEL || view->type == VIEW_PORTAL) {
    return false;
  }

  const Box3 *b = rOcclusion.occludedBounds;
  for (int32_t i = 0; i < rOcclusion.numOccludedBounds; i++, b++) {
    if (Box3_Contains(*b, bounds)) {
      return true;
    }
  }

  b = rOcclusion.visibleBounds;
  for (int32_t i = 0; i < rOcclusion.numVisibleBounds; i++, b++) {
    if (Box3_Intersects(*b, bounds)) {
      return false;
    }
  }

  return true;
}

/**
 * @brief Tests whether the given sphere origin is occluded by any occlusion query result.
 */
bool R_OccludeSphere(const RenderView *view, const Vec3 origin, float radius) {
  return R_OccludeBox(view, Box3_FromCenterRadius(origin, radius));
}

/**
 * @brief Returns true if the box is culled or occluded.
 */
bool R_CulludeBox(const RenderView *view, const Box3 bounds) {
  return R_CullBox(view, bounds) || R_OccludeBox(view, bounds);
}

/**
 * @brief Returns true if the sphere is culled or occluded.
 */
bool R_CulludeSphere(const RenderView *view, const Vec3 point, const float radius) {
  return R_CullSphere(view, point, radius) || R_OccludeSphere(view, point, radius);
}

/**
 * @brief Allocates an occlusion query with the specified bounds.
 */
RenderOcclusionQuery *R_AllocOcclusionQuery(const Box3 bounds) {

  GPU_Assert(rOcclusion.numQueries < MAX_OCCLUSION_QUERIES, "Exceeded MAX_OCCLUSION_QUERIES (%d)", MAX_OCCLUSION_QUERIES);

  RenderOcclusionQuery *query = &rOcclusion.queries[rOcclusion.numQueries++];

  query->bounds = bounds;
  query->firstBox = (int32_t) rOcclusion.boxes->count;
  query->numBoxes = 0;
  query->result = true;

  return query;
}

/**
 * @brief Appends a box to the given query's GPU-drawn instance geometry.
 */
void R_AppendOcclusionQueryBox(RenderOcclusionQuery *query, Box3 bounds) {

  assert(query->firstBox + query->numBoxes == (int32_t) rOcclusion.boxes->count);

  $(rOcclusion.boxes, add, &bounds);
  query->numBoxes++;
}

/**
 * @brief Resets the occlusion query pool, invalidating all previously allocated queries.
 */
void R_FreeOcclusionQueries(void) {

  rOcclusion.numQueries = 0;

  if (rOcclusion.boxes) {
    $(rOcclusion.boxes, removeAll);
  }

  rOcclusion.instanceBuffer = release(rOcclusion.instanceBuffer);
}

/**
 * @brief Builds the instance buffer for queued occlusion boxes.
 */
void R_LoadOcclusionQueries(void) {

  rOcclusion.instanceBuffer = release(rOcclusion.instanceBuffer);

  const int32_t numBoxes = (int32_t) rOcclusion.boxes->count;
  if (numBoxes) {
    rOcclusion.instanceBuffer = $(rContext.device, createBufferWithConstMem,
      SDL_GPU_BUFFERUSAGE_VERTEX, rOcclusion.boxes->elements, (Uint32) (numBoxes * sizeof(Box3)));
  }
}

/**
 * @brief Draws the active occlusion queries into the view depth buffer.
 */
static void R_DrawOcclusionQueries_(const RenderView *view, CommandBuffer *commands) {

  SDL_GPUDepthStencilTargetInfo depth = $(view->framebuffer, depthTargetInfo, SDL_GPU_LOADOP_LOAD, SDL_GPU_STOREOP_STORE);

#ifdef SDL_GPU_QUERY_API
  depth.query_pool = rOcclusion.pool->pool;
#endif

  RenderPass *pass = $(commands, beginRenderPass, NULL, 0, &depth);

  if (rOcclusion.instanceBuffer) {
    $(pass, bindVertexBuffers, 0, (SDL_GPUBufferBinding[]) {
      { .buffer = rOcclusion.vertexBuffer->buffer },
      { .buffer = rOcclusion.instanceBuffer->buffer },
    }, 2);
    $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) {
      .buffer = rOcclusion.elementsBuffer->buffer
    }, SDL_GPU_INDEXELEMENTSIZE_32BIT);
  }

  $(commands, pushVertexUniformData, SLOT_UNIFORMS_GLOBALS, &rUniforms.block, sizeof(rUniforms.block));

  $(pass, bindPipeline, rOcclusion.pipeline);

  RenderOcclusionQuery *q = rOcclusion.queries;
  for (int32_t i = 0; i < rOcclusion.numQueries; i++, q++) {
    $(pass, beginQuery, rOcclusion.pool, i);
    $(pass, drawIndexedPrimitives, 36, q->numBoxes, 0, 0, q->firstBox);
    $(pass, endQuery, rOcclusion.pool, i);
  }

  release(pass);
}

/**
 * @brief Draws and polls all occlusion queries for the current frame.
 */
void R_DrawOcclusionQueries(const RenderView *view, CommandBuffer *commands) {

  if (rDepthPipeline.fence) {

    if ($(rDepthPipeline.fence, query)) {

      const Uint64 *results = $(rOcclusion.transfer, map, false);

      for (int32_t i = 0; i < rOcclusion.numQueries; i++) {
        rOcclusion.queries[i].result = results[i] > 0;
      }

      $(rOcclusion.transfer, unmap);

      rDepthPipeline.fence = release(rDepthPipeline.fence);

      if (r_occlude->integer) {
        R_DrawOcclusionQueries_(view, commands);
      }

      CopyPass *pass = $(commands, beginCopyPass);
      $(pass, downloadQueryResults, rOcclusion.pool, 0, rOcclusion.numQueries, &(SDL_GPUTransferBufferLocation) {
        .transfer_buffer = rOcclusion.transfer->buffer,
      });
      release(pass);
    }
  }

  RenderOcclusionQuery *q = rOcclusion.queries;
  for (int32_t i = 0; i < rOcclusion.numQueries; i++, q++) {

    if (!r_occlude->integer) {
      q->result = true;
    } else {
      if (Box3_Intersects(q->bounds, Box3_FromCenterRadius(view->origin, BSP_VOXEL_SIZE))) {
        q->result = true;
      } else if (R_CullBox(view, q->bounds)) {
        q->result = false;
      }
    }

    rStats->queriesAllocated++;
    if (q->result) {
      rStats->queriesVisible++;
    } else {
      rStats->queriesOccluded++;
    }
  }

  R_UpdateOcclusionBounds();
}

/**
 * @brief Initializes occlusion query state and GPU resources.
 */
void R_InitOcclusionQueries(void) {

  memset(&rOcclusion, 0, sizeof(rOcclusion));

  rOcclusion.boxes = $(alloc(Vector), initWithSize, sizeof(Box3));

  rOcclusion.pool = $(rContext.device, createQueryPool, &(SDL_GPUQueryPoolCreateInfo) {
    .type = SDL_GPU_QUERY_PRECISE_OCCLUSION,
    .query_count = MAX_OCCLUSION_QUERIES,
  });

  Vec3 cube[8];
  Box3_ToPoints(MakeBox3(MakeVec3(0.f, 0.f, 0.f), MakeVec3(1.f, 1.f, 1.f)), cube);

  rOcclusion.vertexBuffer = $(rContext.device, createBufferWithConstMem,
    SDL_GPU_BUFFERUSAGE_VERTEX, cube, sizeof(cube));

  const uint32_t elements[] = {
    0, 1, 3, 0, 3, 2,
    6, 7, 4, 7, 5, 4,
    4, 5, 0, 5, 1, 0,
    7, 6, 3, 6, 2, 3,
    6, 4, 2, 4, 0, 2,
    5, 7, 1, 7, 3, 1,
  };

  rOcclusion.elementsBuffer = $(rContext.device, createBufferWithConstMem,
    SDL_GPU_BUFFERUSAGE_INDEX, elements, sizeof(elements));

  rOcclusion.transfer = $(rContext.device, createTransferBuffer, &(SDL_GPUTransferBufferCreateInfo) {
    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
    .size = MAX_OCCLUSION_QUERIES * sizeof(Uint64),
  });

  Shader *vertexShader = $(rContext.device, loadShader, "shaders/occlude_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 1,
  });

  Shader *fragmentShader = $(rContext.device, loadShader, "shaders/depth_pass_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
  });

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = rSceneSamples;
  info.vertex_shader = vertexShader->shader;
  info.fragment_shader = fragmentShader->shader;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {
      {
        .slot = 0,
        .pitch = sizeof(Vec3),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
      },
      {
        .slot = 1,
        .pitch = sizeof(Box3),
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
        .offset = offsetof(Box3, mins),
      },
      {
        .location = 2,
        .buffer_slot = 1,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .offset = offsetof(Box3, maxs),
      },
    },
    .num_vertex_attributes = 3,
  };

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

  rOcclusion.pipeline = $(rContext.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);
}

/**
 * @brief Shuts down occlusion query state, freeing all GPU resources.
 */
void R_ShutdownOcclusionQueries(void) {

  rOcclusion.pipeline = release(rOcclusion.pipeline);
  rOcclusion.vertexBuffer = release(rOcclusion.vertexBuffer);
  rOcclusion.instanceBuffer = release(rOcclusion.instanceBuffer);
  rOcclusion.elementsBuffer = release(rOcclusion.elementsBuffer);
  rOcclusion.pool = release(rOcclusion.pool);
  rOcclusion.transfer = release(rOcclusion.transfer);
  rOcclusion.boxes = release(rOcclusion.boxes);
}
