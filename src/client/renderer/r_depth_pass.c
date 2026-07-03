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

r_depth_pass_program_t r_depth_pass_program;

/**
 * @brief Renders opaque world geometry into the view framebuffer's depth (depth
 * only), which the main 3D passes then LOAD for early-Z rejection.
 * @details BSP-only: occluders are world geometry. TODO(#864): the depth is
 * sampleable (resolveDepthTexture) for soft particles once ported.
 */
void R_DrawDepthPass(r_view_t *view) {

  if (!r_depth_pass->integer) {
    return;
  }

  if (!r_models.world || !r_depth_pass_program.pipeline) {
    return;
  }

  if (!view->framebuffer) {
    return;
  }

  CommandBuffer *commands = r_device.device->commands;
  if (!commands) {
    return;
  }

  const r_bsp_model_t *bsp = r_models.world->bsp;
  Framebuffer *framebuffer = view->framebuffer;

  // Depth-only: clear + write the shared depth attachment (no color target).
  const SDL_GPUDepthStencilTargetInfo depth =
      $(framebuffer, depthTargetInfo, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE, 1.f);

  RenderPass *pass = $(commands, beginRenderPass, NULL, 0, &depth);

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  // Position-only: the pre-pass needs globals (view/projection) and the world
  // model matrix (identity); no fragment uniforms.
  const mat4_t model = Mat4_Identity();
  $(commands, pushVertexUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));
  $(commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, model.array, sizeof(model));

  $(pass, bindPipeline, r_depth_pass_program.pipeline);
  $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = bsp->vertex_buffer->buffer }, 1);
  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) { .buffer = bsp->elements_buffer->buffer }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  // The worldspawn model carries pre-consolidated opaque (non-sky) depth-pass
  // indices into the shared elements buffer: the entire opaque world in one draw.
  const r_bsp_inline_model_t *world = bsp->inline_models;
  const Uint32 firstIndex = (Uint32) ((uintptr_t) world->depth_pass_elements / sizeof(uint32_t));
  $(pass, drawIndexedPrimitives, world->num_depth_pass_elements, 1, firstIndex, 0, 0);

  pass = release(pass);
}

/**
 * @brief Builds the depth pre-pass pipeline from the depth_pass_vs/fs shaders.
 */
void R_InitDepthPass(void) {

  Shader *vertexShader = $(r_device.device, loadShader, "shaders/depth_pass_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 2, // globals (binding 0) + locals/model (binding 1)
  });

  Shader *fragmentShader = $(r_device.device, loadShader, "shaders/depth_pass_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
  });

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.vertex_shader = vertexShader->shader;
  info.fragment_shader = fragmentShader->shader;

  // Bring-up: draw all faces regardless of winding (matches R_InitBspPipeline).
  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

  // Nudge the pre-pass depth slightly toward the far plane. On desktop GL the
  // shared `invariant gl_Position` made the pre-pass and main-pass depths bit
  // identical, so the main passes' LEQUAL test always accepted the coplanar
  // world fragments. SDL's Metal backend compiles MSL with options:nil (fast
  // math on, invariance preservation off), so `[[position, invariant]]` is
  // ignored and the two shaders' depths can differ by a few ULPs, causing the
  // main pass to reject its own geometry (Z-fighting). A tiny depth bias here
  // guarantees the stored pre-pass depth is >= the main-pass depth, restoring
  // the LEQUAL early-Z behavior regardless of invariance support.
  // TODO(#864): the proper fix is precompiling to .metallib with
  // -fpreserve-invariance; revisit if/when the shader toolchain grows a
  // METALLIB path.
  info.rasterizer_state.enable_depth_bias = true;
  info.rasterizer_state.depth_bias_constant_factor = 1.f;
  info.rasterizer_state.depth_bias_slope_factor = 1.f;

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

  // Depth-only: no color targets, just the shared D32F depth attachment.
  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .num_color_targets = 0,
    .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
    .has_depth_stencil_target = true,
  };

  r_depth_pass_program.pipeline = $(r_device.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);
}

/**
 * @brief Releases the depth pre-pass pipeline.
 */
void R_ShutdownDepthPass(void) {
  r_depth_pass_program.pipeline = release(r_depth_pass_program.pipeline);
}
