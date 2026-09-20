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
 * @brief Vertex type for the fullscreen post-processing quad.
 */
typedef struct {
  Vec2 position;
  Vec2 texcoord;
} RenderPostVertex;

/**
 * @brief Post-processing stage selector.
 */
typedef enum {
  R_POST_BLOOM_EXTRACT,
  R_POST_BLOOM_BLUR_X,
  R_POST_BLOOM_BLUR_Y,
  R_POST_TONEMAP,
} RenderPostStage;

/**
 * @brief Per-pass post-processing uniforms.
 */
typedef struct {
  int32_t postStage;
  float bloom;
  float bloomThreshold;
  float padding;
} RenderPostLocals;

/**
 * @brief The post-processing state.
 */
static struct {

  /**
   * @brief Fullscreen quad vertex buffer.
   */
  Buffer *vertexBuffer;

  /**
   * @brief Half-resolution bloom ping-pong framebuffers.
   */
  Framebuffer *bloomFramebuffers[2];
  int32_t bloomWidth, bloomHeight;

  /**
   * @brief Bloom pipeline.
   */
  GraphicsPipeline *bloomPipeline;

  /**
   * @brief Composite pipeline.
   */
  GraphicsPipeline *compositePipeline;

  /**
   * @brief Sampler for scene and bloom textures.
   */
  Sampler *sampler;
} module;

/**
 * @brief Creates the bloom ping-pong framebuffers.
 */
static void R_CreateBloomFramebuffers(int32_t width, int32_t height) {

  module.bloomWidth  = width  / 2;
  module.bloomHeight = height / 2;

  if (module.bloomWidth  < 1) { module.bloomWidth  = 1; }
  if (module.bloomHeight < 1) { module.bloomHeight = 1; }

  for (int32_t i = 0; i < 2; i++) {

    module.bloomFramebuffers[i] = release(module.bloomFramebuffers[i]);

    module.bloomFramebuffers[i] = $(rContext.device, createFramebuffer, &(GPU_FramebufferCreateInfo) {
      .size = MakeSize(module.bloomWidth, module.bloomHeight),
      .colorAttachments = { { .format = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT } },
      .numColorTargets = 1,
      .sampleCount = SDL_GPU_SAMPLECOUNT_1,
    });
  }
}

/**
 * @brief Runs a fullscreen post-processing pass.
 */
static void R_PostPass(Framebuffer *target, GraphicsPipeline *pipeline,
                       Texture *color, Texture *bloom,
                       int32_t width, int32_t height, const RenderPostLocals *locals) {

  CommandBuffer *commands = rContext.device->commands;

  const SDL_GPUColorTargetInfo colorTarget =
      $(target, colorTargetInfo, 0, SDL_GPU_LOADOP_DONT_CARE, SDL_GPU_STOREOP_STORE);

  RenderPass *pass = $(commands, beginRenderPass, &colorTarget, 1, NULL);

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) width, .h = (float) height,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  $(pass, bindPipeline, pipeline);
  $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = module.vertexBuffer->buffer }, 1);

  $(pass, bindFragmentSamplers, 0, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = color->texture, .sampler = module.sampler->sampler },
    { .texture = bloom->texture, .sampler = module.sampler->sampler },
  }, 2);

  $(commands, pushFragmentUniformData, 0, locals, sizeof(*locals));

  $(pass, drawPrimitives, 6, 1, 0, 0);

  release(pass);
}

/**
 * @brief Applies bloom and tonemapping to the rendered scene.
 */
void R_DrawPost(const RenderView *view) {

  if (!rModels.world) {
    return;
  }

  CommandBuffer *commands = rContext.device->commands;
  if (!commands) {
    return;
  }

  Framebuffer *scene = view->framebuffer;
  Framebuffer *present = rContext.device->framebuffer;

  Texture *sceneColor = $(scene, resolveColorTexture, 0);

  if (scene->size.w != module.bloomWidth * 2 || scene->size.h != module.bloomHeight * 2) {
    R_CreateBloomFramebuffers((int32_t) scene->size.w, (int32_t) scene->size.h);
  }

  const bool bloom = r_bloom->value > 0.f;

  if (bloom) {

    R_PostPass(module.bloomFramebuffers[0], module.bloomPipeline,
               sceneColor, sceneColor,
               module.bloomWidth, module.bloomHeight,
               &(RenderPostLocals) {
                 .postStage = R_POST_BLOOM_EXTRACT,
                 .bloomThreshold = r_bloomThreshold->value,
               });

    const int32_t iterations = Clampf(r_bloomIterations->integer, 1, 8);
    for (int32_t i = 0; i < iterations; i++) {

      R_PostPass(module.bloomFramebuffers[1], module.bloomPipeline,
                 module.bloomFramebuffers[0]->colorAttachments[0].textures[0],
                 module.bloomFramebuffers[0]->colorAttachments[0].textures[0],
                 module.bloomWidth, module.bloomHeight,
                 &(RenderPostLocals) { .postStage = R_POST_BLOOM_BLUR_X });

      R_PostPass(module.bloomFramebuffers[0], module.bloomPipeline,
                 module.bloomFramebuffers[1]->colorAttachments[0].textures[0],
                 module.bloomFramebuffers[1]->colorAttachments[0].textures[0],
                 module.bloomWidth, module.bloomHeight,
                 &(RenderPostLocals) { .postStage = R_POST_BLOOM_BLUR_Y });
    }
  }

  R_PostPass(present, module.compositePipeline,
             sceneColor,
             bloom ? module.bloomFramebuffers[0]->colorAttachments[0].textures[0] : sceneColor,
             (int32_t) present->size.w, (int32_t) present->size.h,
             &(RenderPostLocals) {
               .postStage = R_POST_TONEMAP,
               .bloom = r_bloom->value,
             });
}

/**
 * @brief Creates a post-processing pipeline for the specified color format.
 */
static GraphicsPipeline *R_CreatePostPipeline(SDL_GPUTextureFormat format) {

  SDL_GPUGraphicsPipelineCreateInfo info = {
    .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
    .vertex_input_state = {
      .vertex_buffer_descriptions = &(SDL_GPUVertexBufferDescription) {
        .slot = 0,
        .pitch = sizeof(RenderPostVertex),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
      },
      .num_vertex_buffers = 1,
      .vertex_attributes = (SDL_GPUVertexAttribute[]) {
        {
          .location = 0,
          .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
          .offset = offsetof(RenderPostVertex, position),
        },
        {
          .location = 1,
          .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
          .offset = offsetof(RenderPostVertex, texcoord),
        },
      },
      .num_vertex_attributes = 2,
    },
    .rasterizer_state = {
      .fill_mode = SDL_GPU_FILLMODE_FILL,
      .cull_mode = SDL_GPU_CULLMODE_NONE,
      .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
    },
    .target_info = {
      .color_target_descriptions = &(SDL_GPUColorTargetDescription) {
        .format = format,
        .blend_state = GPU_BlendStateOpaque,
      },
      .num_color_targets = 1,
    },
  };

  return $(rContext.device, loadGraphicsPipeline,
    "shaders/post_vs", &(SDL_GPUShaderCreateInfo) {
      .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    },
    "shaders/post_fs", &(SDL_GPUShaderCreateInfo) {
      .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
      .num_samplers = 2,
      .num_uniform_buffers = 1,
    },
    &info);
}

/**
 * @brief Initializes post-processing resources.
 */
void R_InitPost(void) {

  memset(&module, 0, sizeof(module));

  const RenderPostVertex vertexes[] = {
    { .position = MakeVec2(-1.f, -1.f), .texcoord = MakeVec2(0.f, 1.f) },
    { .position = MakeVec2( 1.f, -1.f), .texcoord = MakeVec2(1.f, 1.f) },
    { .position = MakeVec2( 1.f,  1.f), .texcoord = MakeVec2(1.f, 0.f) },
    { .position = MakeVec2(-1.f, -1.f), .texcoord = MakeVec2(0.f, 1.f) },
    { .position = MakeVec2( 1.f,  1.f), .texcoord = MakeVec2(1.f, 0.f) },
    { .position = MakeVec2(-1.f,  1.f), .texcoord = MakeVec2(0.f, 0.f) },
  };

  module.vertexBuffer = $(rContext.device, createBufferWithConstMem, SDL_GPU_BUFFERUSAGE_VERTEX, vertexes, sizeof(vertexes));

  module.bloomPipeline = R_CreatePostPipeline(SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT);
  module.compositePipeline = R_CreatePostPipeline(rContext.device->framebuffer->colorAttachments[0].format);

  module.sampler = $(rContext.device, createSamplerLinearClamp);
}

/**
 * @brief Shuts down post-processing resources.
 */
void R_ShutdownPost(void) {

  module.vertexBuffer = release(module.vertexBuffer);

  for (int32_t i = 0; i < 2; i++) {
    module.bloomFramebuffers[i] = release(module.bloomFramebuffers[i]);
  }

  module.bloomPipeline = release(module.bloomPipeline);
  module.compositePipeline = release(module.compositePipeline);
  module.sampler = release(module.sampler);
}

/**
 * @brief Rebuilds the post-processing pipelines and sampler.
 */
void R_UpdatePostPipeline(void) {
  R_ShutdownPost();
  R_InitPost();
}
