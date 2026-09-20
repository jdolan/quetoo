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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "r_local.h"

/**
 * @brief The number of samplers used by the sky pipeline.
 */
#define SKY_NUM_SAMPLERS R_SAMPLER_MATERIAL_TOTAL

enum {
  SKY_UNIFORMS_GLOBALS,
  SKY_UNIFORMS_MATERIAL,
};
#define SKY_NUM_UNIFORMS 2

/**
 * @brief The maximum number of cached material-stage pipelines (one per blend).
 */
#define MAX_STAGE_PIPELINES 16

/**
 * @brief Sky rendering resources.
 */
static struct {
  /**
   * @brief The pipeline.
   */
  GraphicsPipeline *pipeline;
  
  /**
   * @brief Repeating linear sampler, for tiled stage textures.
   */
  Sampler *repeatSampler;

  /**
   * @brief Clamped linear sampler, for the sky cubemap and voxel textures.
   */
  Sampler *clampSampler;

  /**
   * @brief Cached material-stage pipelines.
   */
  RenderStagePipeline stagePipelines[MAX_STAGE_PIPELINES];
  int32_t numStagePipelines;
} module;

/**
 * @brief Returns the cached sky stage pipeline for the given blend mode.
 */
static GraphicsPipeline *R_SkyStagePipeline(CmBlend src, CmBlend dest) {

  RenderStagePipeline *p = module.stagePipelines;
  for (int32_t i = 0; i < module.numStagePipelines; i++, p++) {
    if (p->src == src && p->dest == dest) {
      return p->pipeline;
    }
  }

  if (module.numStagePipelines == MAX_STAGE_PIPELINES) {
    return NULL;
  }

  const SDL_GPUBlendFactor s = R_BlendFactor(src);
  const SDL_GPUBlendFactor d = R_BlendFactor(dest);

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = rSceneSamples;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;

  info.depth_stencil_state.enable_depth_write = false;

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = &(SDL_GPUVertexBufferDescription) {
      .slot = 0,
      .pitch = sizeof(RenderBspVertex),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    },
    .num_vertex_buffers = 1,
    .vertex_attributes = &(SDL_GPUVertexAttribute) {
      .location = 0,
      .buffer_slot = 0,
      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
      .offset = offsetof(RenderBspVertex, position),
    },
    .num_vertex_attributes = 1,
  };

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = (SDL_GPUColorTargetDescription[]) {
      {
        .format = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT,
        .blend_state = {
          .enable_blend = true,
          .src_color_blendfactor = s,
          .dst_color_blendfactor = d,
          .color_blend_op = SDL_GPU_BLENDOP_ADD,
          .src_alpha_blendfactor = s,
          .dst_alpha_blendfactor = d,
          .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
        },
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

  GraphicsPipeline *pipeline = $(rContext.device, loadGraphicsPipeline,
    "shaders/sky_vs", &(SDL_GPUShaderCreateInfo) {
      .stage = SDL_GPU_SHADERSTAGE_VERTEX,
      .num_uniform_buffers = SKY_NUM_UNIFORMS,
    },
    "shaders/sky_fs", &(SDL_GPUShaderCreateInfo) {
      .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
      .num_samplers = SKY_NUM_SAMPLERS,
      .num_uniform_buffers = SKY_NUM_UNIFORMS,
    },
    &info);

  module.stagePipelines[module.numStagePipelines++] = (typeof(module.stagePipelines[0])) {
    .src = src, .dest = dest, .depthWrite = false, .pipeline = pipeline,
  };

  return pipeline;
}

/**
 * @brief Draws one sky material stage.
 */
static void R_DrawSkyDrawElementsMaterialStage(const RenderView *view,
                                               const RenderBspDrawElements *draw,
                                               const RenderStage *stage,
                                               RenderPass *pass) {

  RenderMaterialUniforms uniforms;
  R_MaterialUniforms(draw->material, draw->surface, &uniforms);

  SDL_GPUTexture *texture, *textureNext;
  if (!R_StageUniforms(view, NULL, draw, stage, &uniforms, &texture, &textureNext)) {
    return;
  }

  GraphicsPipeline *pipeline = R_SkyStagePipeline(stage->cm->blend.src, stage->cm->blend.dest);
  if (!pipeline) {
    return;
  }

  $(pass, bindPipeline, pipeline);

  $(pass, bindFragmentSamplers, R_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = texture, .sampler = module.repeatSampler->sampler },
    { .texture = textureNext, .sampler = module.repeatSampler->sampler },
  }, 2);

  $(pass->commands, pushUniformData, SKY_UNIFORMS_MATERIAL, &uniforms, sizeof(uniforms));

  const Uint32 firstIndex = (Uint32) ((uintptr_t) draw->elements / sizeof(uint32_t));
  $(pass, drawIndexedPrimitives, draw->numElements, 1, firstIndex, 0, 0);

  rStats->bspTriangles += draw->numElements / 3;
}

/**
 * @brief Draws all active material stages for a sky draw-elements batch.
 */
static void R_DrawSkyDrawElementsMaterialStages(const RenderView *view,
                                                const RenderBspDrawElements *draw,
                                                RenderPass *pass) {

  const RenderMaterial *material = draw->material;
  if (!(material->cm->stageFlags & STAGE_DRAW)) {
    return;
  }

  for (const RenderStage *stage = material->stages; stage; stage = stage->next) {

    if (!(stage->cm->flags & STAGE_DRAW)) {
      continue;
    }

    R_DrawSkyDrawElementsMaterialStage(view, draw, stage, pass);
  }
}

/**
 * @brief Draws world sky surfaces with the active sky cubemap.
 */
void R_DrawSky(const RenderView *view, RenderPass *pass) {

  assert(rModels.world);

  const RenderBspModel *bsp = rModels.world->bsp;

  Framebuffer *framebuffer = view->framebuffer;

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  $(pass->commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &rUniforms.block, sizeof(rUniforms.block));

  $(pass, bindPipeline, module.pipeline);
  $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = bsp->vertexBuffer->buffer }, 1);
  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) { .buffer = bsp->elementsBuffer->buffer }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  $(pass, bindFragmentSamplers, R_SAMPLER_MATERIAL, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = rContext.nullTexture->texture, .sampler = module.repeatSampler->sampler },
    { .texture = rShadowAtlas.textures[0]->texture, .sampler = rShadowAtlas.sampler->sampler },
    { .texture = rShadowAtlas.textures[1]->texture, .sampler = rShadowAtlas.sampler->sampler },
    { .texture = rShadowAtlas.textures[2]->texture, .sampler = rShadowAtlas.sampler->sampler },
    { .texture = rShadowAtlas.textures[3]->texture, .sampler = rShadowAtlas.sampler->sampler },
    { .texture = rShadowAtlas.textures[4]->texture, .sampler = rShadowAtlas.sampler->sampler },
    { .texture = rShadowAtlas.textures[5]->texture, .sampler = rShadowAtlas.sampler->sampler },
    { .texture = bsp->voxels.caustics->texture->texture, .sampler = module.clampSampler->sampler },
    { .texture = bsp->voxels.occlusion->texture->texture, .sampler = module.clampSampler->sampler },
  }, 9);

  $(pass, bindFragmentSamplers, R_SAMPLER_SKY, &(SDL_GPUTextureSamplerBinding) {
    .texture = bsp->sky->texture->texture,
    .sampler = module.clampSampler->sampler,
  }, 1);

  $(pass, bindFragmentSamplers, R_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = rContext.nullTexture->texture, .sampler = module.repeatSampler->sampler },
    { .texture = rContext.nullTexture->texture, .sampler = module.repeatSampler->sampler },
  }, 2);

  const RenderBspInlineModel *world = bsp->inlineModels;
  const RenderBspBlock *block = world->blocks;
  for (int32_t i = 0; i < world->numBlocks; i++, block++) {

    if (!(block->surface & SURF_SKY)) {
      continue;
    }

    const RenderBspDrawElements *draw = block->drawElements;
    for (int32_t j = 0; j < block->numDrawElements; j++, draw++) {

      if (!(draw->surface & SURF_SKY)) {
        continue;
      }

      if (!draw->material) {
        continue;
      }

      $(pass, bindPipeline, module.pipeline);

      RenderMaterialUniforms material;
      R_MaterialUniforms(draw->material, draw->surface, &material);
      $(pass->commands, pushUniformData, SKY_UNIFORMS_MATERIAL, &material, sizeof(material));

      const Uint32 firstIndex = (Uint32) ((uintptr_t) draw->elements / sizeof(uint32_t));

      $(pass, drawIndexedPrimitives, draw->numElements, 1, firstIndex, 0, 0);

      rStats->bspTriangles += draw->numElements / 3;
      rStats->bspDrawElements++;

      if (r_drawMaterialStages->integer) {
        R_DrawSkyDrawElementsMaterialStages(view, draw, pass);
      }
    }
  }
}

/**
 * @brief Builds the sky pipeline (position-only vertex input) and cubemap sampler.
 */
static void R_InitSkyPipeline(void) {

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = rSceneSamples;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = &(SDL_GPUVertexBufferDescription) {
      .slot = 0,
      .pitch = sizeof(RenderBspVertex),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    },
    .num_vertex_buffers = 1,
    .vertex_attributes = &(SDL_GPUVertexAttribute) {
      .location = 0,
      .buffer_slot = 0,
      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
      .offset = offsetof(RenderBspVertex, position),
    },
    .num_vertex_attributes = 1,
  };

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = (SDL_GPUColorTargetDescription[]) {
      {
        .format = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT,
        .blend_state = GPU_BlendStateOpaque,
      },
      {
        .format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT,
        .blend_state = GPU_BlendStateOpaque,
      },
    },
    .num_color_targets = 2,
    .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
    .has_depth_stencil_target = true,
  };

  module.pipeline = $(rContext.device, loadGraphicsPipeline,
    "shaders/sky_vs", &(SDL_GPUShaderCreateInfo) {
      .stage = SDL_GPU_SHADERSTAGE_VERTEX,
      .num_uniform_buffers = SKY_NUM_UNIFORMS,
    },
    "shaders/sky_fs", &(SDL_GPUShaderCreateInfo) {
      .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
      .num_samplers = SKY_NUM_SAMPLERS,
      .num_uniform_buffers = SKY_NUM_UNIFORMS,
    },
    &info);

  module.clampSampler = $(rContext.device, createSamplerLinearClamp);
  module.repeatSampler = $(rContext.device, createSamplerLinearRepeat);
}

/**
 * @brief Initializes the sky subsystem and sky pipeline.
 */
void R_InitSky(void) {

  memset(&module, 0, sizeof(module));

  R_InitSkyPipeline();
}

/**
 * @brief Shuts down the sky subsystem, releasing the pipeline and sampler.
 */
void R_ShutdownSky(void) {

  module.pipeline = release(module.pipeline);
  module.clampSampler = release(module.clampSampler);
  module.repeatSampler = release(module.repeatSampler);

  for (int32_t i = 0; i < module.numStagePipelines; i++) {
    module.stagePipelines[i].pipeline = release(module.stagePipelines[i].pipeline);
  }

  module.numStagePipelines = 0;
}

/**
 * @brief Rebuilds sky pipeline resources.
 */
void R_UpdateSky(void) {

  R_ShutdownSky();
  R_InitSky();
}
