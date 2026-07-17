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
  vec2_t position;
  vec2_t texcoord;
} r_post_vertex_t;

/**
 * @brief Post-processing stage selector.
 */
typedef enum {
  R_POST_BLOOM_EXTRACT,
  R_POST_BLOOM_BLUR_X,
  R_POST_BLOOM_BLUR_Y,
  R_POST_TONEMAP,
} r_post_stage_t;

/**
 * @brief Per-pass post-processing uniforms.
 */
typedef struct {
  int32_t post_stage;
  float bloom;
  float bloom_threshold;
  float padding;
} r_post_locals_t;

/**
 * @brief The post-processing state.
 */
static struct {

  /**
   * @brief Fullscreen quad vertex buffer.
   */
  Buffer *vertex_buffer;

  /**
   * @brief Half-resolution bloom ping-pong framebuffers.
   */
  Framebuffer *bloom_framebuffers[2];
  int32_t bloom_width, bloom_height;

  /**
   * @brief Bloom pipeline.
   */
  GraphicsPipeline *bloom_pipeline;

  /**
   * @brief Composite pipeline.
   */
  GraphicsPipeline *composite_pipeline;

  /**
   * @brief Sampler for scene and bloom textures.
   */
  Sampler *sampler;
} r_post;

/**
 * @brief Creates the bloom ping-pong framebuffers.
 */
static void R_CreateBloomFramebuffers(int32_t width, int32_t height) {

  r_post.bloom_width  = width  / 2;
  r_post.bloom_height = height / 2;

  if (r_post.bloom_width  < 1) { r_post.bloom_width  = 1; }
  if (r_post.bloom_height < 1) { r_post.bloom_height = 1; }

  for (int32_t i = 0; i < 2; i++) {

    r_post.bloom_framebuffers[i] = release(r_post.bloom_framebuffers[i]);

    r_post.bloom_framebuffers[i] = $(r_context.device, createFramebuffer, &(GPU_FramebufferCreateInfo) {
      .size = MakeSize(r_post.bloom_width, r_post.bloom_height),
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
                       int32_t width, int32_t height, const r_post_locals_t *locals) {

  CommandBuffer *commands = r_context.device->commands;

  const SDL_GPUColorTargetInfo color_target =
      $(target, colorTargetInfo, 0, SDL_GPU_LOADOP_DONT_CARE, SDL_GPU_STOREOP_STORE);

  RenderPass *pass = $(commands, beginRenderPass, &color_target, 1, NULL);

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) width, .h = (float) height,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  $(pass, bindPipeline, pipeline);
  $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = r_post.vertex_buffer->buffer }, 1);

  $(pass, bindFragmentSamplers, 0, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = color->texture, .sampler = r_post.sampler->sampler },
    { .texture = bloom->texture, .sampler = r_post.sampler->sampler },
  }, 2);

  $(commands, pushFragmentUniformData, 0, locals, sizeof(*locals));

  $(pass, drawPrimitives, 6, 1, 0, 0);

  release(pass);
}

/**
 * @brief Applies bloom and tonemapping to the rendered scene.
 */
void R_DrawPost(const r_view_t *view) {

  if (!r_models.world) {
    return;
  }

  CommandBuffer *commands = r_context.device->commands;
  if (!commands) {
    return;
  }

  Framebuffer *scene = view->framebuffer;
  Framebuffer *present = r_context.device->framebuffer;

  Texture *scene_color = $(scene, resolveColorTexture, 0);

  if (scene->size.w != r_post.bloom_width * 2 || scene->size.h != r_post.bloom_height * 2) {
    R_CreateBloomFramebuffers((int32_t) scene->size.w, (int32_t) scene->size.h);
  }

  const bool bloom = r_bloom->value > 0.f;

  if (bloom) {

    R_PostPass(r_post.bloom_framebuffers[0], r_post.bloom_pipeline,
               scene_color, scene_color,
               r_post.bloom_width, r_post.bloom_height,
               &(r_post_locals_t) {
                 .post_stage = R_POST_BLOOM_EXTRACT,
                 .bloom_threshold = r_bloom_threshold->value,
               });

    const int32_t iterations = Clampf(r_bloom_iterations->integer, 1, 8);
    for (int32_t i = 0; i < iterations; i++) {

      R_PostPass(r_post.bloom_framebuffers[1], r_post.bloom_pipeline,
                 r_post.bloom_framebuffers[0]->colorAttachments[0].textures[0],
                 r_post.bloom_framebuffers[0]->colorAttachments[0].textures[0],
                 r_post.bloom_width, r_post.bloom_height,
                 &(r_post_locals_t) { .post_stage = R_POST_BLOOM_BLUR_X });

      R_PostPass(r_post.bloom_framebuffers[0], r_post.bloom_pipeline,
                 r_post.bloom_framebuffers[1]->colorAttachments[0].textures[0],
                 r_post.bloom_framebuffers[1]->colorAttachments[0].textures[0],
                 r_post.bloom_width, r_post.bloom_height,
                 &(r_post_locals_t) { .post_stage = R_POST_BLOOM_BLUR_Y });
    }
  }

  R_PostPass(present, r_post.composite_pipeline,
             scene_color,
             bloom ? r_post.bloom_framebuffers[0]->colorAttachments[0].textures[0] : scene_color,
             (int32_t) present->size.w, (int32_t) present->size.h,
             &(r_post_locals_t) {
               .post_stage = R_POST_TONEMAP,
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
        .pitch = sizeof(r_post_vertex_t),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
      },
      .num_vertex_buffers = 1,
      .vertex_attributes = (SDL_GPUVertexAttribute[]) {
        {
          .location = 0,
          .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
          .offset = offsetof(r_post_vertex_t, position),
        },
        {
          .location = 1,
          .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
          .offset = offsetof(r_post_vertex_t, texcoord),
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

  return $(r_context.device, loadGraphicsPipeline,
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

  memset(&r_post, 0, sizeof(r_post));

  const r_post_vertex_t vertexes[] = {
    { .position = Vec2(-1.f, -1.f), .texcoord = Vec2(0.f, 1.f) },
    { .position = Vec2( 1.f, -1.f), .texcoord = Vec2(1.f, 1.f) },
    { .position = Vec2( 1.f,  1.f), .texcoord = Vec2(1.f, 0.f) },
    { .position = Vec2(-1.f, -1.f), .texcoord = Vec2(0.f, 1.f) },
    { .position = Vec2( 1.f,  1.f), .texcoord = Vec2(1.f, 0.f) },
    { .position = Vec2(-1.f,  1.f), .texcoord = Vec2(0.f, 0.f) },
  };

  r_post.vertex_buffer = $(r_context.device, createBufferWithConstMem, SDL_GPU_BUFFERUSAGE_VERTEX, vertexes, sizeof(vertexes));

  r_post.bloom_pipeline = R_CreatePostPipeline(SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT);
  r_post.composite_pipeline = R_CreatePostPipeline(r_context.device->framebuffer->colorAttachments[0].format);

  r_post.sampler = $(r_context.device, createSamplerLinearClamp);
}

/**
 * @brief Shuts down post-processing resources.
 */
void R_ShutdownPost(void) {

  r_post.vertex_buffer = release(r_post.vertex_buffer);

  for (int32_t i = 0; i < 2; i++) {
    r_post.bloom_framebuffers[i] = release(r_post.bloom_framebuffers[i]);
  }

  r_post.bloom_pipeline = release(r_post.bloom_pipeline);
  r_post.composite_pipeline = release(r_post.composite_pipeline);
  r_post.sampler = release(r_post.sampler);
}

/**
 * @brief Rebuilds the post-processing pipelines and sampler.
 */
void R_UpdatePostPipeline(void) {
  R_ShutdownPost();
  R_InitPost();
}
