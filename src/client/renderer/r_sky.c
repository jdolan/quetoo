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
 * @brief Sky state holding the cubemap image.
 */
static struct {

  /**
   * @brief The sky cubemap.
   */
  r_image_t *image;

  /**
   * @brief A 1x1 black fallback cubemap, returned by R_SkyTexture when no sky is
   * loaded, so the lit pass always has a valid cube to bind at the ambient slot.
   */
  Texture *fallback;
} r_sky;

/**
 * @brief The sky pipeline (sky_vs/sky_fs) and its cubemap sampler.
 * @remarks TODO(#864): material-stage skies (azimuthal projection) are deferred.
 */
static struct {
  GraphicsPipeline *pipeline;
  Sampler *sampler;
} r_sky_pipeline;

/**
 * @brief Renders all sky surfaces of the world model as a window into the sky
 * cubemap. Drawn after the opaque world so it fills only unoccluded sky texels.
 */
void R_DrawSky(const r_view_t *view, const r_bsp_model_t *bsp) {

  if (!r_sky_pipeline.pipeline || !r_sky.image || !r_sky.image->texture || !r_sky.image->texture->texture) {
    return;
  }

  if (!bsp->vertex_buffer) {
    return;
  }

  CommandBuffer *commands = r_context.device->commands;
  if (!commands) {
    return;
  }

  if (!view->framebuffer) {
    return;
  }

  Framebuffer *framebuffer = view->framebuffer;

  // Composite over the opaque scene, depth-testing against it (LOAD, no clear) so
  // sky surfaces occluded by world geometry are discarded.
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

  $(pass, bindPipeline, r_sky_pipeline.pipeline);
  $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = bsp->vertex_buffer->buffer }, 1);
  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) { .buffer = bsp->elements_buffer->buffer }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  // texture_sky sits at the fixed global slot SLOT_SAMPLER_SKY. SDL_gpu requires
  // every slot in [0, num_samplers) to be bound (it validates presence, not just
  // the slots the shader samples), so fill the leading unused slots with the same
  // binding; only slot SLOT_SAMPLER_SKY is actually sampled.
  SDL_GPUTextureSamplerBinding sky[SLOT_SAMPLER_SKY + 1];
  for (int32_t i = 0; i <= SLOT_SAMPLER_SKY; i++) {
    sky[i] = (SDL_GPUTextureSamplerBinding) {
      .texture = r_sky.image->texture->texture,
      .sampler = r_sky_pipeline.sampler->sampler,
    };
  }
  $(pass, bindFragmentSamplers, 0, sky, SLOT_SAMPLER_SKY + 1);

  const r_bsp_inline_model_t *world = bsp->inline_models;
  const r_bsp_block_t *block = world->blocks;
  for (int32_t i = 0; i < world->num_blocks; i++, block++) {

    if (!(block->surface & SURF_SKY)) {
      continue;
    }

    const r_bsp_draw_elements_t *draw = block->draw_elements;
    for (int32_t j = 0; j < block->num_draw_elements; j++, draw++) {

      if (!(draw->surface & SURF_SKY)) {
        continue;
      }

      const Uint32 firstIndex = (Uint32) ((uintptr_t) draw->elements / sizeof(uint32_t));

      $(pass, drawIndexedPrimitives, draw->num_elements, 1, firstIndex, 0, 0);

      r_stats.bsp_triangles += draw->num_elements / 3;
      r_stats.bsp_draw_elements++;
    }
  }

  pass = release(pass);
}

/**
 * @brief Returns the sky cubemap for image-based ambient in the lit pass, or a
 * black fallback cube when no sky is loaded, so the ambient sampler is always bound.
 */
Texture *R_SkyTexture(void) {

  if (r_sky.image && r_sky.image->texture) {
    return r_sky.image->texture;
  }

  return r_sky.fallback;
}

/**
 * @brief Builds the sky pipeline (position-only vertex input) and cubemap sampler.
 */
static void R_InitSkyPipeline(void) {

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/sky_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 1, // globals (binding 0)
  });

  Shader *fragmentShader = $(r_context.device, loadShader, "shaders/sky_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = BINDING_SAMPLER_SKY + 1, // texture_sky at the fixed (sparse) sky slot
  });

  const Framebuffer *framebuffer = r_context.device->framebuffer;

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.vertex_shader = vertexShader->shader;
  info.fragment_shader = fragmentShader->shader;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = &(SDL_GPUVertexBufferDescription) {
      .slot = 0,
      .pitch = sizeof(r_bsp_vertex_t),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    },
    .num_vertex_buffers = 1,
    .vertex_attributes = &(SDL_GPUVertexAttribute) {
      .location = 0,
      .buffer_slot = 0,
      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
      .offset = offsetof(r_bsp_vertex_t, position),
    },
    .num_vertex_attributes = 1,
  };

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = &(SDL_GPUColorTargetDescription) {
      .format = R_SCENE_COLOR_FORMAT,
      .blend_state = GPU_BlendStateOpaque,
    },
    .num_color_targets = 1,
    .depth_stencil_format = framebuffer->depthFormat,
    .has_depth_stencil_target = true,
  };

  r_sky_pipeline.pipeline = $(r_context.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);

  r_sky_pipeline.sampler = $(r_context.device, createSampler, &(SDL_GPUSamplerCreateInfo) {
    .min_filter = SDL_GPU_FILTER_LINEAR,
    .mag_filter = SDL_GPU_FILTER_LINEAR,
    .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
    .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
  });
}

/**
 * @brief Initializes the sky subsystem and sky pipeline.
 */
void R_InitSky(void) {

  memset(&r_sky, 0, sizeof(r_sky));

  R_InitSkyPipeline();

  // A 1x1 black fallback cube (six faces) for the lit ambient sampler.
  const byte black[6 * 4] = { 0 };
  r_sky.fallback = $(r_context.device, createTexture, &(SDL_GPUTextureCreateInfo) {
    .type = SDL_GPU_TEXTURETYPE_CUBE,
    .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
    .width = 1, .height = 1,
    .layer_count_or_depth = 6,
    .num_levels = 1,
    .sample_count = SDL_GPU_SAMPLECOUNT_1,
  }, black);
}

/**
 * @brief Shuts down the sky subsystem, releasing the pipeline and sampler.
 */
void R_ShutdownSky(void) {

  r_sky_pipeline.pipeline = release(r_sky_pipeline.pipeline);
  r_sky_pipeline.sampler = release(r_sky_pipeline.sampler);

  r_sky.fallback = release(r_sky.fallback);
}

/**
 * @brief Loads the sky cubemap specified in worldspawn.
 */
void R_LoadSky(void) {

  const char *name = Cm_EntityValue(Cm_Worldspawn(), "sky")->nullable_string;
  if (name) {
    r_sky.image = R_LoadImage(va("sky/%s", name), IMG_CUBEMAP);
  } else {
    r_sky.image = NULL;
  }

  if (r_sky.image == NULL) {
    if (name) {
      Com_Warn("Failed to load sky sky/%s\n", name);
    } else {
      Com_Warn("Failed to load sky (no sky key on worldspawn)\n");
    }

    r_sky.image = R_LoadImage("sky/template", IMG_CUBEMAP);
    if (r_sky.image == NULL) {
      Com_Error(ERROR_DROP, "Failed to load default sky\n");
    }
  }
}
