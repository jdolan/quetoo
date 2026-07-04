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
 * @brief World units the pre-pass geometry is pushed along the camera's
 * forward vector, so its stored depth is >= the main pass's own (bit-inexact)
 * depth for the same surface under a strict LEQUAL test. See R_DrawDepthPass.
 */
#define DEPTH_PASS_BIAS 1.f

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

  CommandBuffer *commands = r_context.device->commands;
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
  // model matrix; no fragment uniforms. The model matrix is a small translation
  // along the camera's forward vector (not identity) -- this is the depth bias:
  // a uniform world-space push along `forward` adds exactly DEPTH_PASS_BIAS to
  // every vertex's view-space Z (since the view matrix's Z axis is `forward` by
  // construction), regardless of where that vertex sits in the frustum. Baking
  // it into `model` (rather than editing the shader's arithmetic) keeps
  // depth_pass_vs.glsl's gl_Position expression textually identical to
  // bsp_vs.glsl's, so the two shaders take the exact same floating-point path
  // through the projection -- restructuring the multiply chain (e.g. computing
  // view_model * position as a separate step before applying projection3D) is
  // NOT floating-point-equivalent to the chained `projection3D * view_model *
  // position` main.c and bsp_vs.glsl use, and reintroduces the very
  // bit-inexactness this bias is meant to paper over, just with a different
  // (and differently distance-dependent) error term.
  const mat4_t model = Mat4_FromTranslation(Vec3_Scale(view->forward, DEPTH_PASS_BIAS));
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

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/depth_pass_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 2, // globals (binding 0) + locals/model (binding 1)
  });

  Shader *fragmentShader = $(r_context.device, loadShader, "shaders/depth_pass_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
  });

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = r_scene_samples;
  info.vertex_shader = vertexShader->shader;
  info.fragment_shader = fragmentShader->shader;

  // Cull back faces, matching R_InitBspPipeline so the pre-pass depth agrees with
  // the main opaque pass (front faces clockwise, per the GL renderer).
  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;

  // No native SDL_GPU depth bias here: for a floating-point depth attachment
  // (D32_FLOAT, below), the hardware bias's "r" constant is implementation-
  // defined and, per the Vulkan/Metal/D3D12 spec language it shares, is
  // typically derived from the format alone, not the fragment's actual depth --
  // i.e. a small FIXED absolute step in NDC space. That's backwards for a
  // nonlinear projection: it's barely enough near the camera (dense NDC depth)
  // and nowhere near enough far away (compressed NDC depth), confirmed
  // empirically (insufficient past ~768 units; enable_depth_bias with
  // constant/slope factors of 1, 4, and 16 were all tried on this branch and
  // none resolved distant Z-fighting). See R_DrawDepthPass for the world-space
  // bias applied instead, via the model matrix.

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

  r_depth_pass_program.pipeline = $(r_context.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);
}

/**
 * @brief Releases the depth pre-pass pipeline.
 */
void R_ShutdownDepthPass(void) {
  r_depth_pass_program.pipeline = release(r_depth_pass_program.pipeline);
}
