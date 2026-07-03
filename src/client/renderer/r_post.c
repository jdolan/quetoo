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
 * @brief Post-processing stage selector, mirroring the const int values in post_fs.glsl.
 */
typedef enum {
  R_POST_BLOOM_EXTRACT,
  R_POST_BLOOM_BLUR_X,
  R_POST_BLOOM_BLUR_Y,
  R_POST_TONEMAP,
} r_post_stage_t;

/**
 * @brief Per-pass locals, pushed to fragment uniform slot 0 (std140, 16 bytes).
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
   * @brief The shared fullscreen quad (position + texcoord).
   */
  Buffer *vertex_buffer;

  /**
   * @brief Half-resolution HDR bloom ping-pong targets. [0] receives the extract
   * pass and every second blur; [1] receives every first blur. Their stored size
   * (bloom_width/height) detects a viewport resize and triggers recreation.
   */
  Framebuffer *bloom_framebuffers[2];
  int32_t bloom_width, bloom_height;

  /**
   * @brief The post program targeting the HDR bloom buffers (extract + blur).
   */
  GraphicsPipeline *bloom_pipeline;

  /**
   * @brief The post program targeting the present framebuffer (bloom composite).
   */
  GraphicsPipeline *composite_pipeline;

  /**
   * @brief A linear, clamped sampler for the scene and bloom textures.
   */
  Sampler *sampler;
} r_post;

/**
 * @brief Creates or recreates the half-resolution bloom ping-pong framebuffers.
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
      .colorFormats = { R_SCENE_COLOR_FORMAT },
      .numColorTargets = 1,
      .depthFormat = SDL_GPU_TEXTUREFORMAT_INVALID,
      .sampleCount = SDL_GPU_SAMPLECOUNT_1,
    });
  }
}

/**
 * @brief Runs one fullscreen post pass into @p target with the given pipeline,
 * binding @p color and @p bloom at fragment sampler slots 0 and 1.
 */
static void R_PostPass(Framebuffer *target, GraphicsPipeline *pipeline,
                       Texture *color, Texture *bloom,
                       int32_t width, int32_t height, const r_post_locals_t *locals) {

  CommandBuffer *commands = r_context.device->commands;

  const SDL_GPUColorTargetInfo color_target =
      $(target, colorTargetInfo, 0, SDL_GPU_LOADOP_DONT_CARE, SDL_GPU_STOREOP_STORE, NULL);

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
 * @brief Composites the rendered scene into the present framebuffer, applying
 * bloom. The 3D passes render into the view's HDR scene framebuffer; the bloom
 * chain (extract -> separable Gaussian blur) runs at half resolution, and the
 * final pass adds the blurred bloom onto the scene color, clamps to LDR, and
 * writes the result into the present framebuffer (over which the UI is drawn).
 */
void R_DrawPost(const r_view_t *view) {

  if (!r_models.world) {
    return; // no 3D scene this frame; the present clear shows through for the UI
  }

  if (!view->framebuffer) {
    return;
  }

  CommandBuffer *commands = r_context.device->commands;
  if (!commands) {
    return;
  }

  Framebuffer *scene = view->framebuffer;
  Framebuffer *present = r_context.device->framebuffer;

  // The scene may be a different size than the present framebuffer (r_framebuffer_scale);
  // the composite samples it with a linear filter, up/downscaling into the present
  // framebuffer at its full resolution.
  Texture *scene_color = scene->colorTextures[0];

  // (Re)create the half-resolution bloom targets on a viewport resize.
  if (scene->size.w != r_post.bloom_width * 2 || scene->size.h != r_post.bloom_height * 2) {
    R_CreateBloomFramebuffers((int32_t) scene->size.w, (int32_t) scene->size.h);
  }

  const bool bloom = r_bloom->value > 0.f;

  if (bloom) {

    // Extract bright regions from the scene color into the first bloom target.
    R_PostPass(r_post.bloom_framebuffers[0], r_post.bloom_pipeline,
               scene_color, scene_color,
               r_post.bloom_width, r_post.bloom_height,
               &(r_post_locals_t) {
                 .post_stage = R_POST_BLOOM_EXTRACT,
                 .bloom_threshold = r_bloom_threshold->value,
               });

    // Separable Gaussian blur, ping-ponging between the two bloom targets: each
    // iteration blurs horizontally (0 -> 1) then vertically (1 -> 0).
    const int32_t iterations = Clampf(r_bloom_iterations->integer, 1, 8);
    for (int32_t i = 0; i < iterations; i++) {

      R_PostPass(r_post.bloom_framebuffers[1], r_post.bloom_pipeline,
                 r_post.bloom_framebuffers[0]->colorTextures[0],
                 r_post.bloom_framebuffers[0]->colorTextures[0],
                 r_post.bloom_width, r_post.bloom_height,
                 &(r_post_locals_t) { .post_stage = R_POST_BLOOM_BLUR_X });

      R_PostPass(r_post.bloom_framebuffers[0], r_post.bloom_pipeline,
                 r_post.bloom_framebuffers[1]->colorTextures[0],
                 r_post.bloom_framebuffers[1]->colorTextures[0],
                 r_post.bloom_width, r_post.bloom_height,
                 &(r_post_locals_t) { .post_stage = R_POST_BLOOM_BLUR_Y });
    }
  }

  // Composite: add the blurred bloom onto the scene, clamp to LDR, write present.
  // When bloom is disabled the glow term is scaled by zero, so the bound bloom
  // texture is irrelevant (the scene color is bound there as a placeholder).
  R_PostPass(present, r_post.composite_pipeline,
             scene_color,
             bloom ? r_post.bloom_framebuffers[0]->colorTextures[0] : scene_color,
             (int32_t) present->size.w, (int32_t) present->size.h,
             &(r_post_locals_t) {
               .post_stage = R_POST_TONEMAP,
               .bloom = r_bloom->value,
             });
}

/**
 * @brief Builds a post pipeline (post_vs/post_fs) targeting the given color format.
 */
static GraphicsPipeline *R_CreatePostPipeline(SDL_GPUTextureFormat format) {

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/post_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
  });

  Shader *fragmentShader = $(r_context.device, loadShader, "shaders/post_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = 2,        // texture_color_attachment + texture_bloom_attachment
    .num_uniform_buffers = 1, // per-pass locals
  });

  SDL_GPUGraphicsPipelineCreateInfo info = {
    .vertex_shader = vertexShader->shader,
    .fragment_shader = fragmentShader->shader,
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

  GraphicsPipeline *pipeline = $(r_context.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);

  return pipeline;
}

/**
 * @brief Initializes the post-processing subsystem: the fullscreen quad, the
 * bloom and composite pipelines, and the sampler.
 */
void R_InitPost(void) {

  memset(&r_post, 0, sizeof(r_post));

  // A fullscreen quad in clip space. Texcoords map the top-left of the screen
  // (clip y = +1) to the top-left texel (v = 0), matching the SDL_gpu convention
  // (framebuffer origin top-left) so the composited scene is not vertically flipped.
  const r_post_vertex_t vertexes[] = {
    { .position = Vec2(-1.f, -1.f), .texcoord = Vec2(0.f, 1.f) },
    { .position = Vec2( 1.f, -1.f), .texcoord = Vec2(1.f, 1.f) },
    { .position = Vec2( 1.f,  1.f), .texcoord = Vec2(1.f, 0.f) },
    { .position = Vec2(-1.f, -1.f), .texcoord = Vec2(0.f, 1.f) },
    { .position = Vec2( 1.f,  1.f), .texcoord = Vec2(1.f, 0.f) },
    { .position = Vec2(-1.f,  1.f), .texcoord = Vec2(0.f, 0.f) },
  };

  r_post.vertex_buffer = $(r_context.device, createBufferWithConstMem,
      SDL_GPU_BUFFERUSAGE_VERTEX, vertexes, sizeof(vertexes));

  r_post.bloom_pipeline = R_CreatePostPipeline(R_SCENE_COLOR_FORMAT);
  r_post.composite_pipeline = R_CreatePostPipeline(r_context.device->framebuffer->colorFormats[0]);

  r_post.sampler = $(r_context.device, createSampler, &(SDL_GPUSamplerCreateInfo) {
    .min_filter = SDL_GPU_FILTER_LINEAR,
    .mag_filter = SDL_GPU_FILTER_LINEAR,
    .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
    .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
  });
}

/**
 * @brief Shuts down the post-processing subsystem, releasing all GPU resources.
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
