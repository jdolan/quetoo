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
 * @brief A 3D draw batch.
 */
typedef struct {

  /**
   * @brief Primitive mode.
   */
  SDL_GPUPrimitiveType mode;

  /**
   * @brief Depth-test flag.
   */
  bool depth_test;

  /**
   * @brief First vertex.
   */
  uint32_t first_vertex;

  /**
   * @brief Vertex count.
   */
  uint32_t num_vertexes;
} RenderDraw3dArrays;

#define MAX_DRAW_3D_ARRAYS 0x100000
#define MAX_DRAW_3D_VERTEXES (MAX_DRAW_3D_ARRAYS * 2)

/**
 * @brief 3D debug vertex.
 */
typedef struct {

  /**
   * @brief Vertex position.
   */
  Vec3 position;

  /**
   * @brief Vertex color.
   */
  Color32 color;
} RenderDraw3dVertex;

/**
 * @brief 3D debug draw state.
 */
static struct {

  RenderDraw3dArrays draw_arrays[MAX_DRAW_3D_ARRAYS];
  int32_t num_draw_arrays;

  RenderDraw3dVertex vertexes[MAX_DRAW_3D_VERTEXES];
  int32_t num_vertexes;

  Buffer *vertex_buffer;
  int32_t vertex_buffer_capacity;

  /**
   * @brief The transfer buffer sourcing the vertex upload, held for the subsystem's
   * lifetime because the vertexes are uploaded every frame. Grown with the buffer it
   * sources.
   */
  TransferBuffer *transfer_buffer;
} r_draw_3d;

/**
 * @brief 3D debug pipelines keyed by primitive mode and depth testing.
 */
static struct {
  GraphicsPipeline *line_list;
  GraphicsPipeline *line_list_no_depth;
  GraphicsPipeline *line_strip;
  GraphicsPipeline *line_strip_no_depth;
} r_draw_3d_pipeline;

/**
 * @brief Returns the pipeline for the requested primitive mode and depth test.
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
static void R_AddDraw3DArrays(const RenderDraw3dArrays *draw) {

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
static void R_AddDraw3DVertex(const RenderDraw3dVertex *v) {

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
void R_Draw3DLines(SDL_GPUPrimitiveType mode, const Vec3 *points, size_t count, const Color color, bool depth_test) {

  const RenderDraw3dArrays draw = {
    .mode = mode,
    .depth_test = depth_test,
    .first_vertex = (uint32_t) r_draw_3d.num_vertexes,
    .num_vertexes = (uint32_t) count,
  };

  const Vec3 *in = points;
  for (size_t i = 0; i < count; i++, in++) {
    R_AddDraw3DVertex(&(const RenderDraw3dVertex) {
      .position = *in,
      .color = Color_Color32(color)
    });
  }

  R_AddDraw3DArrays(&draw);
}

/**
 * @brief Draws the bounding box using line strips in 3D space.
 */
void R_Draw3DBox(const Box3 bounds, const Color color, bool depth_test) {
  Vec3 points[8];

  Box3_ToPoints(bounds, points);

  R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINESTRIP, (const Vec3 []) {
    points[0],
    points[1],
    points[3],
    points[2],
    points[0],
  }, 5, color, depth_test);

  R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINESTRIP, (const Vec3 []) {
    points[4],
    points[5],
    points[7],
    points[6],
    points[4],
  }, 5, color, depth_test);

  R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, (const Vec3 []) {
    points[0],
    points[4],
  }, 2, color, depth_test);

  R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, (const Vec3 []) {
    points[2],
    points[6],
  }, 2, color, depth_test);

  R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, (const Vec3 []) {
    points[3],
    points[7],
  }, 2, color, depth_test);

  R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, (const Vec3 []) {
    points[1],
    points[5],
  }, 2, color, depth_test);
}

/**
 * @brief Accumulates per-vertex normal, tangent, and bitangent debug lines for
 * nearby world BSP vertices when `r_draw_bsp_normals` is enabled.
 */
static void R_UpdateBspNormals(const RenderView *view) {

  if (!r_draw_bsp_normals->value || !r_models.world) {
    return;
  }

  const RenderBspModel *bsp = r_models.world->bsp;

  const RenderBspVertex *v = bsp->vertexes;
  for (int32_t i = 0; i < bsp->num_vertexes; i++, v++) {

    const Vec3 pos = v->position;
    if (Vec3_Distance(pos, view->origin) > 512.f) {
      continue;
    }

    const Vec3 normal[] = { pos, Vec3_Fmaf(pos, 8.f, v->normal) };
    const Vec3 tangent[] = { pos, Vec3_Fmaf(pos, 8.f, v->tangent) };
    const Vec3 bitangent[] = { pos, Vec3_Fmaf(pos, 8.f, v->bitangent) };

    R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, normal, 2, color_red, true);

    if (r_draw_bsp_normals->integer > 1) {
      R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, tangent, 2, color_green, true);

      if (r_draw_bsp_normals->integer > 2) {
        R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, bitangent, 2, color_blue, true);
      }
    }
  }
}

/**
 * @brief Accumulates debug bounding boxes for all entities in the view when
 * `r_draw_entity_bounds` is enabled.
 */
static void R_UpdateEntityBounds(const RenderView *view) {

  if (!r_draw_entity_bounds->value) {
    return;
  }

  const RenderEntity *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (e->parent) {
      continue;
    }

    if (e->effects & (EF_WORLD | EF_SELF | EF_WEAPON)) {
      continue;
    }

    if (Box3_IsNull(e->abs_model_bounds)) {
      continue;
    }

    if (R_CulludeBox(view, e->abs_model_bounds)) {
      continue;
    }

    if (r_draw_entity_bounds->integer == 2) {
      R_Draw3DBox(e->abs_model_bounds, Color4fv(e->color), true);
    } else {
      R_Draw3DBox(e->abs_bounds, Color4fv(e->color), true);
    }
  }
}

/**
 * @brief Accumulates debug bounding boxes for lights near the view's forward
 * trace when `r_draw_light_bounds` is enabled.
 */
static void R_UpdateLightBounds(const RenderView *view) {

  if (!r_draw_light_bounds->value) {
    return;
  }

  const Vec3 end = Vec3_Fmaf(view->origin, MAX_WORLD_DIST, view->forward);
  const CmTrace tr = Cm_BoxTrace(view->origin, end, Box3_Zero(), 0, CONTENTS_SOLID);

  const RenderLight *l = view->lights;
  for (int32_t i = 0; i < view->num_lights; i++, l++) {
    if (Vec3_Distance(tr.end, l->origin) < 64.f) {
      R_Draw3DBox(l->bounds, Color3fv(l->color), false);
    }
  }
}

/**
 * @brief Adds debug bounds for occlusion queries and BSP blocks.
 */
static void R_UpdateOcclusionBounds(const RenderView *view) {

  if (r_draw_occlusion_queries->value) {
    const RenderOcclusionQuery *q = r_occlusion.queries;
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

  if (r_draw_bsp_blocks->value && r_models.world) {
    RenderBspBlock *b = r_models.world->bsp->inline_models->blocks;
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
 * @brief Accumulates this frame's 3D line geometry and uploads it, growing the
 * vertex buffer on demand.
 */
void R_UpdateDraw3D(const RenderView *view, CopyPass *copyPass) {

  R_UpdateBspNormals(view);
  R_UpdateEntityBounds(view);
  R_UpdateLightBounds(view);
  R_UpdateOcclusionBounds(view);

  if (r_draw_3d.num_draw_arrays == 0) {
    return;
  }

  const uint32_t count = (uint32_t) r_draw_3d.num_vertexes;

  if ((int32_t) count > r_draw_3d.vertex_buffer_capacity) {
    r_draw_3d.vertex_buffer = release(r_draw_3d.vertex_buffer);
    r_draw_3d.vertex_buffer = $(r_context.device, createBuffer, &(SDL_GPUBufferCreateInfo) {
      .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
      .size = count * sizeof(RenderDraw3dVertex),
    });
    r_draw_3d.transfer_buffer = release(r_draw_3d.transfer_buffer);
    r_draw_3d.transfer_buffer = $(r_context.device, createTransferBuffer, &(SDL_GPUTransferBufferCreateInfo) {
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      .size = count * sizeof(RenderDraw3dVertex),
    });

    r_draw_3d.vertex_buffer_capacity = (int32_t) count;
  }

  const uint32_t size = count * sizeof(RenderDraw3dVertex);

  $(r_draw_3d.transfer_buffer, write, r_draw_3d.vertexes, size, true);

  $(copyPass, uploadBuffer,
    &(SDL_GPUTransferBufferLocation) { .transfer_buffer = r_draw_3d.transfer_buffer->buffer },
    &(SDL_GPUBufferRegion) { .buffer = r_draw_3d.vertex_buffer->buffer, .size = size },
    true);
}

/**
 * @brief Draws all 3D debug geometry accumulated for the current frame, into
 * the view's scene framebuffer.
 */
void R_Draw3D(const RenderView *view, RenderPass *pass) {

  if (r_draw_3d.num_draw_arrays == 0) {
    return;
  }

  CommandBuffer *commands = r_context.device->commands;

  Framebuffer *framebuffer = view->framebuffer;

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  $(commands, pushVertexUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

  GraphicsPipeline *pipeline = NULL;

  const RenderDraw3dArrays *draw = r_draw_3d.draw_arrays;
  for (int32_t i = 0; i < r_draw_3d.num_draw_arrays; i++, draw++) {

    GraphicsPipeline *next = R_Draw3DPipeline(draw->mode, draw->depth_test);
    if (next != pipeline) {
      pipeline = next;
      $(pass, bindPipeline, pipeline);
      $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = r_draw_3d.vertex_buffer->buffer }, 1);
    }

    $(pass, drawPrimitives, draw->num_vertexes, 1, draw->first_vertex, 0);
  }

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
      .pitch = sizeof(RenderDraw3dVertex),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    },
    .num_vertex_buffers = 1,
    .vertex_attributes = (SDL_GPUVertexAttribute[]) {
      {
        .location = 0,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .offset = offsetof(RenderDraw3dVertex, position),
      },
      {
        .location = 1,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
        .offset = offsetof(RenderDraw3dVertex, color),
      },
    },
    .num_vertex_attributes = 2,
  };

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = (SDL_GPUColorTargetDescription[]) {
      {
        .format = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT,
        .blend_state = GPU_BlendStateAlpha,
      },
      {
        .format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT,
        .blend_state = { .enable_color_write_mask = true, .color_write_mask = 0 },
      },
    },
    .num_color_targets = 2,
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
    .num_uniform_buffers = 1,
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
  r_draw_3d.transfer_buffer = release(r_draw_3d.transfer_buffer);
}

/**
 * @brief Rebuilds the 3D debug draw pipelines.
 */
void R_UpdateDraw3DPipeline(void) {
  R_ShutdownDraw3D();
  R_InitDraw3D();
}
