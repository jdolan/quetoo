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
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "r_local.h"

/**
 * @brief A single draw call: a run of vertexes to rasterize with a given
 * primitive mode and depth test flag.
 */
typedef struct {

  /**
   * @brief The draw mode, e.g. `SDL_GPU_PRIMITIVETYPE_LINESTRIP`.
   */
  SDL_GPUPrimitiveType mode;

  /**
   * @brief The depth test flag.
   */
  bool depth_test;

  /**
   * @brief The first vertex.
   */
  uint32_t first_vertex;

  /**
   * @brief The number of vertexes.
   */
  uint32_t num_vertexes;
} r_draw_3d_arrays_t;

#define MAX_DRAW_3D_ARRAYS 0x100000
#define MAX_DRAW_3D_VERTEXES (MAX_DRAW_3D_ARRAYS * 2)

/**
 * @brief 3D debug vertex type.
 */
typedef struct {

  /**
   * @brief The vertex position.
   */
  vec3_t position;

  /**
   * @brief The vertex color.
   */
  color32_t color;
} r_draw_3d_vertex_t;

/**
 * @brief 3D debug draw state, accumulated over the frame and flushed by R_Draw3D.
 */
static struct {

  r_draw_3d_arrays_t draw_arrays[MAX_DRAW_3D_ARRAYS];
  int32_t num_draw_arrays;

  r_draw_3d_vertex_t vertexes[MAX_DRAW_3D_VERTEXES];
  int32_t num_vertexes;

  Buffer *vertex_buffer;
  int32_t vertex_buffer_capacity; // in vertexes
} r_draw_3d;

/**
 * @brief The 3D debug pipelines, keyed by primitive mode and depth test flag
 * (SDL_gpu bakes both into the pipeline, unlike GL's per-draw-call state).
 */
static struct {
  GraphicsPipeline *line_list;
  GraphicsPipeline *line_list_no_depth;
  GraphicsPipeline *line_strip;
  GraphicsPipeline *line_strip_no_depth;
} r_draw_3d_pipeline;

/**
 * @brief Resolves the pipeline for the given primitive mode and depth test flag.
 * @remarks Only LINELIST and LINESTRIP are supported; callers needing other
 * primitive types should extend the pipeline set above.
 */
static GraphicsPipeline *R_Draw3DPipeline(SDL_GPUPrimitiveType mode, bool depth_test) {

  switch (mode) {
    case SDL_GPU_PRIMITIVETYPE_LINELIST:
      return depth_test ? r_draw_3d_pipeline.line_list : r_draw_3d_pipeline.line_list_no_depth;
    case SDL_GPU_PRIMITIVETYPE_LINESTRIP:
      return depth_test ? r_draw_3d_pipeline.line_strip : r_draw_3d_pipeline.line_strip_no_depth;
    default:
      GPU_Assert(false, "unsupported 3D debug primitive mode %d", mode);
      return NULL;
  }
}

/**
 * @brief Appends a draw arrays batch to the 3D draw list.
 */
static void R_AddDraw3DArrays(const r_draw_3d_arrays_t *draw) {

  if (r_draw_3d.num_draw_arrays == MAX_DRAW_3D_ARRAYS) {
    Com_Warn("MAX_DRAW_3D_ARRAYS\n");
    return;
  }

  if (draw->num_vertexes == 0) {
    return;
  }

  r_draw_3d.draw_arrays[r_draw_3d.num_draw_arrays] = *draw;
  r_draw_3d.num_draw_arrays++;
}

/**
 * @brief Appends a single vertex to the 3D draw vertex buffer.
 */
static void R_AddDraw3DVertex(const r_draw_3d_vertex_t *v) {

  if (r_draw_3d.num_vertexes == MAX_DRAW_3D_VERTEXES) {
    Com_Warn("MAX_DRAW_3D_VERTEXES\n");
    return;
  }

  r_draw_3d.vertexes[r_draw_3d.num_vertexes] = *v;
  r_draw_3d.num_vertexes++;
}

/**
 * @brief Draws line strips or line lists in 3D space.
 */
void R_Draw3DLines(SDL_GPUPrimitiveType mode, const vec3_t *points, size_t count, const color_t color, bool depth_test) {

  const r_draw_3d_arrays_t draw = {
    .mode = mode,
    .depth_test = depth_test,
    .first_vertex = (uint32_t) r_draw_3d.num_vertexes,
    .num_vertexes = (uint32_t) count,
  };

  const vec3_t *in = points;
  for (size_t i = 0; i < count; i++, in++) {
    R_AddDraw3DVertex(&(const r_draw_3d_vertex_t) {
      .position = *in,
      .color = Color_Color32(color)
    });
  }

  R_AddDraw3DArrays(&draw);
}

/**
 * @brief Draws the bounding box using line strips in 3D space.
 */
void R_Draw3DBox(const box3_t bounds, const color_t color, bool depth_test) {
  vec3_t points[8];

  Box3_ToPoints(bounds, points);

  R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINESTRIP, (const vec3_t []) {
    points[0],
    points[1],
    points[3],
    points[2],
    points[0],
  }, 5, color, depth_test);

  R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINESTRIP, (const vec3_t []) {
    points[4],
    points[5],
    points[7],
    points[6],
    points[4],
  }, 5, color, depth_test);

  R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, (const vec3_t []) {
    points[0],
    points[4],
  }, 2, color, depth_test);

  R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, (const vec3_t []) {
    points[2],
    points[6],
  }, 2, color, depth_test);

  R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, (const vec3_t []) {
    points[3],
    points[7],
  }, 2, color, depth_test);

  R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, (const vec3_t []) {
    points[1],
    points[5],
  }, 2, color, depth_test);
}

/**
 * @brief Draws all 3D debug geometry accumulated for the current frame, into
 * the view's scene framebuffer.
 */
void R_Draw3D(const r_view_t *view) {

  if (r_draw_3d.num_draw_arrays == 0) {
    return;
  }

  CommandBuffer *commands = r_context.device->commands;
  if (!commands || !view->framebuffer) {
    r_draw_3d.num_draw_arrays = r_draw_3d.num_vertexes = 0;
    return;
  }

  Framebuffer *framebuffer = view->framebuffer;

  const uint32_t count = (uint32_t) r_draw_3d.num_vertexes;

  if ((int32_t) count > r_draw_3d.vertex_buffer_capacity) {
    r_draw_3d.vertex_buffer = release(r_draw_3d.vertex_buffer);
    r_draw_3d.vertex_buffer = $(r_context.device, createBuffer, &(SDL_GPUBufferCreateInfo) {
      .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
      .size = count * sizeof(r_draw_3d_vertex_t),
    });
    r_draw_3d.vertex_buffer_capacity = (int32_t) count;
  }

  {
    CopyPass *copyPass = $(commands, beginCopyPass);
    $(copyPass, uploadData, r_draw_3d.vertex_buffer->buffer, r_draw_3d.vertexes,
      count * sizeof(r_draw_3d_vertex_t), 0, true);
    release(copyPass);
  }

  const SDL_GPUColorTargetInfo color =
      $(framebuffer, colorTargetInfo, 0, SDL_GPU_LOADOP_LOAD, SDL_GPU_STOREOP_STORE, NULL);
  const SDL_GPUDepthStencilTargetInfo depth =
      $(framebuffer, depthTargetInfo, SDL_GPU_LOADOP_LOAD, SDL_GPU_STOREOP_STORE, 1.f);

  RenderPass *pass = $(commands, beginRenderPass, &color, 1, &depth);

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  $(commands, pushVertexUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

  GraphicsPipeline *pipeline = NULL;

  const r_draw_3d_arrays_t *draw = r_draw_3d.draw_arrays;
  for (int32_t i = 0; i < r_draw_3d.num_draw_arrays; i++, draw++) {

    GraphicsPipeline *next = R_Draw3DPipeline(draw->mode, draw->depth_test);
    if (next != pipeline) {
      pipeline = next;
      $(pass, bindPipeline, pipeline);
      $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = r_draw_3d.vertex_buffer->buffer }, 1);
    }

    $(pass, drawPrimitives, draw->num_vertexes, 1, draw->first_vertex, 0);
  }

  pass = release(pass);

  r_draw_3d.num_draw_arrays = 0;
  r_draw_3d.num_vertexes = 0;
}

/**
 * @brief Builds a 3D debug pipeline for the given primitive mode and depth test flag.
 */
static GraphicsPipeline *R_InitDraw3DPipeline(SDL_GPUPrimitiveType mode, bool depth_test,
                                               Shader *vertexShader, Shader *fragmentShader) {

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = r_scene_samples;
  info.vertex_shader = vertexShader->shader;
  info.fragment_shader = fragmentShader->shader;

  info.primitive_type = mode;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

  info.depth_stencil_state.enable_depth_test = depth_test;
  info.depth_stencil_state.enable_depth_write = false;

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = &(SDL_GPUVertexBufferDescription) {
      .slot = 0,
      .pitch = sizeof(r_draw_3d_vertex_t),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    },
    .num_vertex_buffers = 1,
    .vertex_attributes = (SDL_GPUVertexAttribute[]) {
      {
        .location = 0,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .offset = offsetof(r_draw_3d_vertex_t, position),
      },
      {
        .location = 1,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
        .offset = offsetof(r_draw_3d_vertex_t, color),
      },
    },
    .num_vertex_attributes = 2,
  };

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = &(SDL_GPUColorTargetDescription) {
      .format = R_SCENE_COLOR_FORMAT,
      .blend_state = GPU_BlendStateAlpha,
    },
    .num_color_targets = 1,
    .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
    .has_depth_stencil_target = true,
  };

  return $(r_context.device, createGraphicsPipeline, &info);
}

/**
 * @brief Initializes the 3D debug draw subsystem: buffers and pipelines.
 */
void R_InitDraw3D(void) {

  memset(&r_draw_3d, 0, sizeof(r_draw_3d));

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/draw_3d_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 1, // globals (binding 0)
  });

  Shader *fragmentShader = $(r_context.device, loadShader, "shaders/draw_3d_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
  });

  r_draw_3d_pipeline.line_list = R_InitDraw3DPipeline(SDL_GPU_PRIMITIVETYPE_LINELIST, true, vertexShader, fragmentShader);
  r_draw_3d_pipeline.line_list_no_depth = R_InitDraw3DPipeline(SDL_GPU_PRIMITIVETYPE_LINELIST, false, vertexShader, fragmentShader);
  r_draw_3d_pipeline.line_strip = R_InitDraw3DPipeline(SDL_GPU_PRIMITIVETYPE_LINESTRIP, true, vertexShader, fragmentShader);
  r_draw_3d_pipeline.line_strip_no_depth = R_InitDraw3DPipeline(SDL_GPU_PRIMITIVETYPE_LINESTRIP, false, vertexShader, fragmentShader);

  release(vertexShader);
  release(fragmentShader);
}

/**
 * @brief Shuts down the 3D debug draw subsystem, releasing pipelines and buffers.
 */
void R_ShutdownDraw3D(void) {

  r_draw_3d_pipeline.line_list = release(r_draw_3d_pipeline.line_list);
  r_draw_3d_pipeline.line_list_no_depth = release(r_draw_3d_pipeline.line_list_no_depth);
  r_draw_3d_pipeline.line_strip = release(r_draw_3d_pipeline.line_strip);
  r_draw_3d_pipeline.line_strip_no_depth = release(r_draw_3d_pipeline.line_strip_no_depth);

  r_draw_3d.vertex_buffer = release(r_draw_3d.vertex_buffer);
}
