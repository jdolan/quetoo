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
 * @brief The BSP program pipeline (bsp_vs/bsp_fs). Draws BSP geometry with the
 * material diffuse texture and clustered per-voxel lighting.
 * @remarks TODO(#864): grow bsp_vs/bsp_fs toward the full material/stage/shadow
 * program, and draw inline BSP model entities (not just the world model).
 */
static GraphicsPipeline *r_bsp_pipeline;

/**
 * @brief The material sampler (trilinear, repeat).
 */
static Sampler *r_bsp_sampler;

/**
 * @brief The voxel light-data sampler (nearest, clamp) for integer texelFetch.
 */
static Sampler *r_voxel_sampler;

/**
 * @brief Builds the BSP pipeline from the bsp_vs/bsp_fs shaders.
 */
void R_InitBspProgram(void) {

  Shader *vertexShader = $(r_device.device, loadShader, "shaders/bsp_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 2, // globals (binding 0) + locals/model (binding 1)
  });

  Shader *fragmentShader = $(r_device.device, loadShader, "shaders/bsp_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = 3,         // texture_material + texture_voxel_light_data + texture_shadow_atlas
    .num_storage_buffers = 2,  // lights_block + voxel_light_indices_block (read-only storage)
    .num_uniform_buffers = 3,  // globals + per-block active_lights + per-draw material
  });

  const Framebuffer *framebuffer = r_device.device->framebuffer;

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.vertex_shader = vertexShader->shader;
  info.fragment_shader = fragmentShader->shader;

  // Bring-up: draw all faces regardless of winding so geometry is guaranteed visible.
  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = &(SDL_GPUVertexBufferDescription) {
      .slot = 0,
      .pitch = sizeof(r_bsp_vertex_t),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    },
    .num_vertex_buffers = 1,
    .vertex_attributes = (SDL_GPUVertexAttribute[]) {
      {
        .location = 0,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .offset = offsetof(r_bsp_vertex_t, position),
      },
      {
        .location = 1,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .offset = offsetof(r_bsp_vertex_t, normal),
      },
      {
        .location = 2,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .offset = offsetof(r_bsp_vertex_t, tangent),
      },
      {
        .location = 3,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .offset = offsetof(r_bsp_vertex_t, bitangent),
      },
      {
        .location = 4,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
        .offset = offsetof(r_bsp_vertex_t, diffusemap),
      },
      {
        .location = 5,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
        .offset = offsetof(r_bsp_vertex_t, color),
      },
    },
    .num_vertex_attributes = 6,
  };

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = &(SDL_GPUColorTargetDescription) {
      .format = framebuffer->colorFormats[0],
      .blend_state = GPU_BlendStateOpaque,
    },
    .num_color_targets = 1,
    .depth_stencil_format = framebuffer->depthFormat,
    .has_depth_stencil_target = true,
  };

  r_bsp_pipeline = $(r_device.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);

  r_bsp_sampler = $(r_device.device, createSampler, &(SDL_GPUSamplerCreateInfo) {
    .min_filter = SDL_GPU_FILTER_LINEAR,
    .mag_filter = SDL_GPU_FILTER_LINEAR,
    .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
    .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
  });

  // Nearest / clamp: the light-data texture is fetched with integer coords.
  r_voxel_sampler = $(r_device.device, createSampler, &(SDL_GPUSamplerCreateInfo) {
    .min_filter = SDL_GPU_FILTER_NEAREST,
    .mag_filter = SDL_GPU_FILTER_NEAREST,
    .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
    .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
  });
}

/**
 * @brief Releases the BSP pipeline and samplers.
 */
void R_ShutdownBspProgram(void) {
  r_bsp_pipeline = release(r_bsp_pipeline);
  r_bsp_sampler = release(r_bsp_sampler);
  r_voxel_sampler = release(r_voxel_sampler);
}

/**
 * @brief Renders the opaque world BSP surfaces with their material diffuse texture,
 * clustered per-voxel static lighting, and per-block dynamic lighting.
 * @remarks Iterates the worldspawn inline model's blocks (the unit of culling and,
 * later, the Hi-Z occlusion pyramid); per block, the active dynamic lights are
 * culled to the block bounds. TODO(#864): inline BSP model entities, blended
 * surfaces, sky, material stages, and per-block occlusion queries.
 */
void R_DrawBspEntities(const r_view_t *view) {

  if (!r_models.world || !r_bsp_pipeline) {
    return;
  }

  CommandBuffer *commands = r_device.device->commandBuffer;
  if (!commands) {
    return;
  }

  const r_bsp_model_t *bsp = r_models.world->bsp;
  Framebuffer *framebuffer = r_device.device->framebuffer;

  const SDL_GPUColorTargetInfo color =
      $(framebuffer, colorTargetInfo, 0, SDL_GPU_LOADOP_LOAD, SDL_GPU_STOREOP_STORE, NULL);
  const SDL_GPUDepthStencilTargetInfo depth =
      $(framebuffer, depthTargetInfo, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE, 1.f);

  RenderPass *pass = $(commands, beginRenderPass, &color, 1, &depth);

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  // Vertex uniforms: globals + per-draw locals (the world model matrix).
  // Fragment uniforms: the same globals (lighting scalars); active_lights and the
  // material parameters are pushed per block / per draw below.
  const mat4_t model = Mat4_Identity();
  $(commands, pushVertexUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));
  $(commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, model.array, sizeof(model));
  $(commands, pushFragmentUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

  $(pass, bindPipeline, r_bsp_pipeline);
  $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = bsp->vertex_buffer->buffer }, 1);
  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) { .buffer = bsp->elements_buffer->buffer }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  // The per-voxel light-data 3D texture (first_index, count).
  $(pass, bindFragmentSamplers, SLOT_SAMPLER_VOXEL_LIGHT_DATA, &(SDL_GPUTextureSamplerBinding) {
    .texture = bsp->voxels.light_data->texture->texture,
    .sampler = r_voxel_sampler->sampler,
  }, 1);

  // The point-light shadow atlas (comparison sampler).
  $(pass, bindFragmentSamplers, SLOT_SAMPLER_SHADOW_ATLAS, &(SDL_GPUTextureSamplerBinding) {
    .texture = r_shadow_atlas.texture->texture,
    .sampler = r_shadow_atlas.sampler->sampler,
  }, 1);

  // Storage: per-frame lights, then the voxel light index vector. Fall back to the
  // lights buffer for the index slot on maps with no light indices (the voxel
  // counts are then zero, so it is never read); SDL still requires a bind.
  SDL_GPUBuffer *storage[] = {
    r_lights.buffer->buffer,
    bsp->voxels.light_indices_buffer ? bsp->voxels.light_indices_buffer->buffer
                                     : r_lights.buffer->buffer,
  };
  $(pass, bindFragmentStorageBuffers, SLOT_STORAGE_LIGHTS, storage, 2);

  // Iterate the worldspawn inline model's blocks: the unit of light culling (and,
  // later, of the Hi-Z occlusion pyramid). Per block, cull dynamic lights to the
  // block bounds and push the resulting bitmask, then draw the block's elements.
  const r_bsp_inline_model_t *world = bsp->inline_models;
  const r_bsp_block_t *block = world->blocks;
  for (int32_t i = 0; i < world->num_blocks; i++, block++) {

    // TODO(#864): per-block occlusion cull here (block->query) once the Hi-Z
    // depth-pyramid compute pass lands; blocks are its building blocks.

    uint32_t active_lights[4];
    R_ActiveLights(view, block->node->visible_bounds, active_lights);
    $(commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, active_lights, sizeof(active_lights));

    const r_bsp_draw_elements_t *draw = block->draw_elements;
    for (int32_t j = 0; j < block->num_draw_elements; j++, draw++) {

      // TODO(#864): sky (cubemap) and blended surfaces need dedicated passes; skip for now.
      if (draw->surface & (SURF_SKY | SURF_MASK_BLEND)) {
        continue;
      }

      if (!draw->material || !draw->material->texture || !draw->material->texture->texture) {
        continue;
      }

      $(pass, bindFragmentSamplers, SLOT_SAMPLER_MATERIAL, &(SDL_GPUTextureSamplerBinding) {
        .texture = draw->material->texture->texture->texture,
        .sampler = r_bsp_sampler->sampler,
      }, 1);

      r_material_uniforms_t material;
      R_MaterialUniforms(draw->material, draw->surface, &material);
      $(commands, pushFragmentUniformData, SLOT_UNIFORMS_MATERIAL, &material, sizeof(material));

      const Uint32 firstIndex = (Uint32) ((uintptr_t) draw->elements / sizeof(uint32_t));

      $(pass, drawIndexedPrimitives, draw->num_elements, 1, firstIndex, 0, 0);

      r_stats.bsp_triangles += draw->num_elements / 3;
      r_stats.bsp_draw_elements++;
    }
  }

  pass = release(pass);
}
