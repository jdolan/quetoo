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

RenderDepthPipeline rDepthPipeline;

/**
 * @brief Draws world geometry into the view depth buffer.
 */
void R_DrawDepthPass(RenderView *view, CommandBuffer *commands) {

  if (!r_depthPass->integer) {
    return;
  }

  const RenderBspModel *bsp = rModels.world->bsp;
  Framebuffer *framebuffer = view->framebuffer;

  const SDL_GPUDepthStencilTargetInfo depth = $(framebuffer, depthTargetInfo, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE);

  RenderPass *pass = $(commands, beginRenderPass, NULL, 0, &depth);

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  const Mat4 model = Mat4_Identity();
  $(commands, pushVertexUniformData, SLOT_UNIFORMS_GLOBALS, &rUniforms.block, sizeof(rUniforms.block));
  $(commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, model.array, sizeof(model));

  $(pass, bindPipeline, rDepthPipeline.pipeline);
  $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = bsp->vertexBuffer->buffer }, 1);
  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) { .buffer = bsp->elementsBuffer->buffer }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  // The Z pre-pass has no sampler bindings, so only draw the lumped opaque entry (entry with
  // no material); alpha-tested faces are left to the color pass, same as before this refactor.
  const RenderBspInlineModel *world = bsp->inlineModels;
  const RenderBspDrawElements *draw = world->depthPassElements;
  for (int32_t i = 0; i < world->numDepthPassElements; i++, draw++) {

    if (draw->material) {
      continue;
    }

    const Uint32 firstIndex = (Uint32) ((uintptr_t) draw->elements / sizeof(uint32_t));
    $(pass, drawIndexedPrimitives, draw->numElements, 1, firstIndex, 0, 0);
  }

  pass = release(pass);
}

/**
 * @brief Builds the depth pre-pass pipeline from the depth_pass_vs/fs shaders.
 */
void R_InitDepthPass(void) {

  Shader *vertexShader = $(rContext.device, loadShader, "shaders/depth_pass_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 2,
  });

  Shader *fragmentShader = $(rContext.device, loadShader, "shaders/depth_pass_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
  });

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = rSceneSamples;
  info.vertex_shader = vertexShader->shader;
  info.fragment_shader = fragmentShader->shader;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;
  info.rasterizer_state.enable_depth_bias = true;
  info.rasterizer_state.depth_bias_constant_factor = 8.f;
  info.rasterizer_state.depth_bias_slope_factor = 8.f;

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
    .num_color_targets = 0,
    .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
    .has_depth_stencil_target = true,
  };

  rDepthPipeline.pipeline = $(rContext.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);
}

/**
 * @brief Releases the depth pre-pass pipeline.
 */
void R_ShutdownDepthPass(void) {
  rDepthPipeline.pipeline = release(rDepthPipeline.pipeline);
  rDepthPipeline.fence = release(rDepthPipeline.fence);
}

/**
 * @brief Rebuilds the depth pre-pass pipeline.
 */
void R_UpdateDepthPass(void) {
  R_ShutdownDepthPass();
  R_InitDepthPass();
}
