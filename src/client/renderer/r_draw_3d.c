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
  bool depthTest;

  /**
   * @brief First vertex.
   */
  uint32_t firstVertex;

  /**
   * @brief Vertex count.
   */
  uint32_t numVertexes;
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

  RenderDraw3dArrays drawArrays[MAX_DRAW_3D_ARRAYS];
  int32_t numDrawArrays;

  RenderDraw3dVertex vertexes[MAX_DRAW_3D_VERTEXES];
  int32_t numVertexes;

  Buffer *vertexBuffer;
  int32_t vertexBufferCapacity;

  /**
   * @brief The transfer buffer sourcing the vertex upload, held for the subsystem's
   * lifetime because the vertexes are uploaded every frame. Grown with the buffer it
   * sources.
   */
  TransferBuffer *transferBuffer;
} module;

/**
 * @brief 3D debug pipelines keyed by primitive mode and depth testing.
 */
static struct {
  GraphicsPipeline *lineList;
  GraphicsPipeline *lineListNoDepth;
  GraphicsPipeline *lineStrip;
  GraphicsPipeline *lineStripNoDepth;
} draw3dPipeline;

/**
 * @brief Returns the pipeline for the requested primitive mode and depth test.
 */
static GraphicsPipeline *R_Draw3DPipeline(SDL_GPUPrimitiveType mode, bool depthTest) {

  switch (mode) {
    case SDL_GPU_PRIMITIVETYPE_LINELIST:
      return depthTest ? draw3dPipeline.lineList : draw3dPipeline.lineListNoDepth;
    case SDL_GPU_PRIMITIVETYPE_LINESTRIP:
      return depthTest ? draw3dPipeline.lineStrip : draw3dPipeline.lineStripNoDepth;
    default:
      GPU_Assert(false, "unsupported 3D debug primitive mode %d", mode);
      return NULL;
  }
}

/**
 * @brief Appends a draw arrays batch to the 3D draw list.
 */
static void R_AddDraw3DArrays(const RenderDraw3dArrays *draw) {

  if (module.numDrawArrays == MAX_DRAW_3D_ARRAYS) {
    Com_Warn("MAX_DRAW_3D_ARRAYS\n");
    return;
  }

  if (draw->numVertexes == 0) {
    return;
  }

  module.drawArrays[module.numDrawArrays] = *draw;
  module.numDrawArrays++;
}

/**
 * @brief Appends a single vertex to the 3D draw vertex buffer.
 */
static void R_AddDraw3DVertex(const RenderDraw3dVertex *v) {

  if (module.numVertexes == MAX_DRAW_3D_VERTEXES) {
    Com_Warn("MAX_DRAW_3D_VERTEXES\n");
    return;
  }

  module.vertexes[module.numVertexes] = *v;
  module.numVertexes++;
}

/**
 * @brief Draws line strips or line lists in 3D space.
 */
void R_Draw3DLines(SDL_GPUPrimitiveType mode, const Vec3 *points, size_t count, const Color color, bool depthTest) {

  const RenderDraw3dArrays draw = {
    .mode = mode,
    .depthTest = depthTest,
    .firstVertex = (uint32_t) module.numVertexes,
    .numVertexes = (uint32_t) count,
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
void R_Draw3DBox(const Box3 bounds, const Color color, bool depthTest) {
  Vec3 points[8];

  Box3_ToPoints(bounds, points);

  R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINESTRIP, (const Vec3 []) {
    points[0],
    points[1],
    points[3],
    points[2],
    points[0],
  }, 5, color, depthTest);

  R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINESTRIP, (const Vec3 []) {
    points[4],
    points[5],
    points[7],
    points[6],
    points[4],
  }, 5, color, depthTest);

  R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, (const Vec3 []) {
    points[0],
    points[4],
  }, 2, color, depthTest);

  R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, (const Vec3 []) {
    points[2],
    points[6],
  }, 2, color, depthTest);

  R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, (const Vec3 []) {
    points[3],
    points[7],
  }, 2, color, depthTest);

  R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, (const Vec3 []) {
    points[1],
    points[5],
  }, 2, color, depthTest);
}

/**
 * @brief Accumulates per-vertex normal, tangent, and bitangent debug lines for
 * nearby world BSP vertices when `r_drawBspNormals` is enabled.
 */
static void R_UpdateBspNormals(const RenderView *view) {

  if (!r_drawBspNormals->value || !rModels.world) {
    return;
  }

  const RenderBspModel *bsp = rModels.world->bsp;

  const RenderBspVertex *v = bsp->vertexes;
  for (int32_t i = 0; i < bsp->numVertexes; i++, v++) {

    const Vec3 pos = v->position;
    if (Vec3_Distance(pos, view->origin) > 512.f) {
      continue;
    }

    const Vec3 normal[] = { pos, Vec3_Fmaf(pos, 8.f, v->normal) };
    const Vec3 tangent[] = { pos, Vec3_Fmaf(pos, 8.f, v->tangent) };
    const Vec3 bitangent[] = { pos, Vec3_Fmaf(pos, 8.f, v->bitangent) };

    R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, normal, 2, color_red, true);

    if (r_drawBspNormals->integer > 1) {
      R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, tangent, 2, color_green, true);

      if (r_drawBspNormals->integer > 2) {
        R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, bitangent, 2, color_blue, true);
      }
    }
  }
}

/**
 * @brief Accumulates debug bounding boxes for all entities in the view when
 * `r_drawEntityBounds` is enabled.
 */
static void R_UpdateEntityBounds(const RenderView *view) {

  if (!r_drawEntityBounds->value) {
    return;
  }

  const RenderEntity *e = view->entities;
  for (int32_t i = 0; i < view->numEntities; i++, e++) {

    if (e->parent) {
      continue;
    }

    if (e->effects & (EF_WORLD | EF_SELF | EF_WEAPON)) {
      continue;
    }

    if (Box3_IsNull(e->absModelBounds)) {
      continue;
    }

    if (R_CulludeBox(view, e->absModelBounds)) {
      continue;
    }

    if (r_drawEntityBounds->integer == 2) {
      R_Draw3DBox(e->absModelBounds, Color4fv(e->color), true);
    } else {
      R_Draw3DBox(e->absBounds, Color4fv(e->color), true);
    }
  }
}

/**
 * @brief Accumulates debug bounding boxes for lights near the view's forward
 * trace when `r_drawLightBounds` is enabled.
 */
static void R_UpdateLightBounds(const RenderView *view) {

  if (!r_drawLightBounds->value) {
    return;
  }

  const Vec3 end = Vec3_Fmaf(view->origin, MAX_WORLD_DIST, view->forward);
  const CmTrace tr = Cm_BoxTrace(view->origin, end, Box3_Zero(), 0, CONTENTS_SOLID);

  const RenderLight *l = view->lights;
  for (int32_t i = 0; i < view->numLights; i++, l++) {
    if (Vec3_Distance(tr.end, l->origin) < 64.f) {
      R_Draw3DBox(l->bounds, Color3fv(l->color), false);
    }
  }
}

/**
 * @brief Adds debug bounds for occlusion queries and BSP blocks.
 */
static void R_UpdateOcclusionBounds(const RenderView *view) {

  if (r_drawOcclusionQueries->value) {
    const RenderOcclusionQuery *q = rOcclusion.queries;
    for (int32_t i = 0; i < rOcclusion.numQueries; i++, q++) {
      const float dist = Vec3_Distance(Box3_Center(q->bounds), view->origin);
      const float f = 1.f - Clampf01(dist / MAX_WORLD_COORD);
      if (!q->result) {
        R_Draw3DBox(q->bounds, Color3f(0.f, f, 0.f), false);
      } else {
        R_Draw3DBox(q->bounds, Color3f(f, 0.f, 0.f), false);
      }
    }
  }

  if (r_drawBspBlocks->value && rModels.world) {
    RenderBspBlock *b = rModels.world->bsp->inlineModels->blocks;
    for (int32_t i = 0; i < rModels.world->bsp->inlineModels->numBlocks; i++, b++) {
      const float dist = Vec3_Distance(Box3_Center(b->visibleBounds), view->origin);
      const float f = 1.f - Clampf01(dist / MAX_WORLD_COORD);
      if (!b->query->result) {
        R_Draw3DBox(b->visibleBounds, Color3f(0.f, f, 0.f), false);
      } else {
        R_Draw3DBox(b->visibleBounds, Color3f(f, 0.f, 0.f), false);
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

  if (module.numDrawArrays == 0) {
    return;
  }

  const uint32_t count = (uint32_t) module.numVertexes;

  if ((int32_t) count > module.vertexBufferCapacity) {
    module.vertexBuffer = release(module.vertexBuffer);
    module.vertexBuffer = $(rContext.device, createBuffer, &(SDL_GPUBufferCreateInfo) {
      .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
      .size = count * sizeof(RenderDraw3dVertex),
    });
    module.transferBuffer = release(module.transferBuffer);
    module.transferBuffer = $(rContext.device, createTransferBuffer, &(SDL_GPUTransferBufferCreateInfo) {
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      .size = count * sizeof(RenderDraw3dVertex),
    });

    module.vertexBufferCapacity = (int32_t) count;
  }

  const uint32_t size = count * sizeof(RenderDraw3dVertex);

  $(module.transferBuffer, write, module.vertexes, size, true);

  $(copyPass, uploadBuffer,
    &(SDL_GPUTransferBufferLocation) { .transfer_buffer = module.transferBuffer->buffer },
    &(SDL_GPUBufferRegion) { .buffer = module.vertexBuffer->buffer, .size = size },
    true);
}

/**
 * @brief Draws all 3D debug geometry accumulated for the current frame, into
 * the view's scene framebuffer.
 */
void R_Draw3D(const RenderView *view, RenderPass *pass) {

  if (module.numDrawArrays == 0) {
    return;
  }

  CommandBuffer *commands = rContext.device->commands;

  Framebuffer *framebuffer = view->framebuffer;

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  $(commands, pushVertexUniformData, SLOT_UNIFORMS_GLOBALS, &rUniforms.block, sizeof(rUniforms.block));

  GraphicsPipeline *pipeline = NULL;

  const RenderDraw3dArrays *draw = module.drawArrays;
  for (int32_t i = 0; i < module.numDrawArrays; i++, draw++) {

    GraphicsPipeline *next = R_Draw3DPipeline(draw->mode, draw->depthTest);
    if (next != pipeline) {
      pipeline = next;
      $(pass, bindPipeline, pipeline);
      $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = module.vertexBuffer->buffer }, 1);
    }

    $(pass, drawPrimitives, draw->numVertexes, 1, draw->firstVertex, 0);
  }

  module.numDrawArrays = 0;
  module.numVertexes = 0;
}

/**
 * @brief Builds a 3D debug pipeline for the given primitive mode and depth test flag.
 */
static GraphicsPipeline *R_InitDraw3DPipeline(SDL_GPUPrimitiveType mode, bool depthTest,
                                               Shader *vertexShader, Shader *fragmentShader) {

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = rSceneSamples;
  info.vertex_shader = vertexShader->shader;
  info.fragment_shader = fragmentShader->shader;

  info.primitive_type = mode;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

  info.depth_stencil_state.enable_depth_test = depthTest;
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

  return $(rContext.device, createGraphicsPipeline, &info);
}

/**
 * @brief Initializes the 3D debug draw subsystem: buffers and pipelines.
 */
void R_InitDraw3D(void) {

  memset(&module, 0, sizeof(module));

  Shader *vertexShader = $(rContext.device, loadShader, "shaders/draw_3d_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 1,
  });

  Shader *fragmentShader = $(rContext.device, loadShader, "shaders/draw_3d_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
  });

  draw3dPipeline.lineList = R_InitDraw3DPipeline(SDL_GPU_PRIMITIVETYPE_LINELIST, true, vertexShader, fragmentShader);
  draw3dPipeline.lineListNoDepth = R_InitDraw3DPipeline(SDL_GPU_PRIMITIVETYPE_LINELIST, false, vertexShader, fragmentShader);
  draw3dPipeline.lineStrip = R_InitDraw3DPipeline(SDL_GPU_PRIMITIVETYPE_LINESTRIP, true, vertexShader, fragmentShader);
  draw3dPipeline.lineStripNoDepth = R_InitDraw3DPipeline(SDL_GPU_PRIMITIVETYPE_LINESTRIP, false, vertexShader, fragmentShader);

  release(vertexShader);
  release(fragmentShader);
}

/**
 * @brief Shuts down the 3D debug draw subsystem, releasing pipelines and buffers.
 */
void R_ShutdownDraw3D(void) {

  draw3dPipeline.lineList = release(draw3dPipeline.lineList);
  draw3dPipeline.lineListNoDepth = release(draw3dPipeline.lineListNoDepth);
  draw3dPipeline.lineStrip = release(draw3dPipeline.lineStrip);
  draw3dPipeline.lineStripNoDepth = release(draw3dPipeline.lineStripNoDepth);

  module.vertexBuffer = release(module.vertexBuffer);
  module.transferBuffer = release(module.transferBuffer);
}

/**
 * @brief Rebuilds the 3D debug draw pipelines.
 */
void R_UpdateDraw3DPipeline(void) {
  R_ShutdownDraw3D();
  R_InitDraw3D();
}
