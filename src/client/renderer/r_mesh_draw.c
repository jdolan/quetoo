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
 * @brief The mesh program pipeline (mesh_vs/mesh_fs). Animated geometry with
 * diffuse material and clustered per-voxel lighting.
 * @remarks TODO(#864): stages, shells, tints, color/blend, specular/normal maps,
 * and the player-model view are ported in later increments.
 */
static GraphicsPipeline *r_mesh_pipeline;

/**
 * @brief The material sampler (trilinear, repeat) and voxel sampler (nearest, clamp).
 */
static Sampler *r_mesh_sampler;
static Sampler *r_mesh_voxel_sampler;

/**
 * @brief Per-entity mesh locals, pushed to vertex uniform slot 1.
 */
typedef struct {
  mat4_t model;
  float lerp;
} r_mesh_locals_t;

/**
 * @brief Sets up per-entity uniforms and draws all opaque faces of a mesh entity.
 */
static void R_DrawMeshEntity(RenderPass *pass, const r_view_t *view, const r_entity_t *e) {

  const r_mesh_model_t *mesh = e->model->mesh;
  assert(mesh);

  if (!mesh->vertex_buffer) {
    return;
  }

  CommandBuffer *commands = r_device.device->commands;

  const r_mesh_locals_t locals = {
    .model = e->matrix,
    .lerp = e->lerp,
  };
  $(commands, pushVertexUniformData, 1, &locals, sizeof(locals));

  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) {
    .buffer = mesh->elements_buffer->buffer
  }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  const uint32_t stride = sizeof(r_mesh_vertex_t);

  const r_mesh_face_t *face = mesh->faces;
  for (int32_t i = 0; i < mesh->num_faces; i++, face++) {

    const r_material_t *material = e->skins[i] ?: face->material;

    // TODO(#864): blended faces + material stages/shells are drawn later.
    if ((material->cm->surface & SURF_MASK_BLEND) || (e->effects & EF_BLEND)) {
      continue;
    }

    if (!material->texture || !material->texture->texture) {
      continue;
    }

    $(pass, bindFragmentSamplers, 0, &(SDL_GPUTextureSamplerBinding) {
      .texture = material->texture->texture->texture,
      .sampler = r_mesh_sampler->sampler,
    }, 1);

    // Two vertex buffer slots: the old frame (locations 0-4) and the current
    // frame (locations 5-6), the same model buffer bound at each frame offset.
    const uint32_t old_offset = (uint32_t) (face->base_vertex + e->old_frame * face->num_vertexes) * stride;
    const uint32_t cur_offset = (uint32_t) (face->base_vertex + e->frame * face->num_vertexes) * stride;

    $(pass, bindVertexBuffers, 0, (SDL_GPUBufferBinding[]) {
      { .buffer = mesh->vertex_buffer->buffer, .offset = old_offset },
      { .buffer = mesh->vertex_buffer->buffer, .offset = cur_offset },
    }, 2);

    const uint32_t firstIndex = (uint32_t) ((uintptr_t) face->indices / sizeof(uint32_t));

    $(pass, drawIndexedPrimitives, face->num_elements, 1, firstIndex, 0, 0);

    r_stats.mesh_draw_elements++;
    r_stats.mesh_triangles += face->num_elements / 3;
  }

  r_stats.mesh_models++;
}

/**
 * @brief Draws mesh entities.
 */
void R_DrawMeshEntities(const r_view_t *view) {

  if (!r_mesh_pipeline || !r_models.world) {
    return;
  }

  CommandBuffer *commands = r_device.device->commands;
  if (!commands) {
    return;
  }

  if (!view->framebuffer || !view->framebuffer->framebuffer) {
    return;
  }

  const r_bsp_model_t *bsp = r_models.world->bsp;
  Framebuffer *framebuffer = view->framebuffer->framebuffer;

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

  // Per-frame globals to both stages; lighting resources shared with the BSP pass.
  $(commands, pushVertexUniformData, 0, &r_uniforms.block, sizeof(r_uniforms.block));
  $(commands, pushFragmentUniformData, 0, &r_uniforms.block, sizeof(r_uniforms.block));

  $(pass, bindPipeline, r_mesh_pipeline);

  $(pass, bindFragmentSamplers, 1, &(SDL_GPUTextureSamplerBinding) {
    .texture = bsp->voxels.light_data->texture->texture,
    .sampler = r_mesh_voxel_sampler->sampler,
  }, 1);

  SDL_GPUBuffer *storage[] = {
    r_lights.buffer->buffer,
    bsp->voxels.light_indices_buffer ? bsp->voxels.light_indices_buffer->buffer
                                     : r_lights.buffer->buffer,
  };
  $(pass, bindFragmentStorageBuffers, 0, storage, 2);

  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_MESH_MODEL(e->model)) {
      continue;
    }

    if (e->effects & EF_NO_DRAW) {
      continue;
    }

    if (R_CullEntity(view, e)) {
      r_stats.entities_occluded++;
      continue;
    }

    R_DrawMeshEntity(pass, view, e);
    r_stats.entities_visible++;
  }

  pass = release(pass);
}

/**
 * @brief Builds the mesh pipeline from the mesh_vs/mesh_fs shaders.
 */
void R_InitMeshProgram(void) {

  Shader *vertexShader = $(r_device.device, loadShader, "shaders/mesh_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 2, // globals (binding 0) + locals (binding 1)
  });

  Shader *fragmentShader = $(r_device.device, loadShader, "shaders/mesh_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = 2,         // texture_material + texture_voxel_light_data
    .num_storage_buffers = 2,  // lights_block + voxel_light_indices_block
    .num_uniform_buffers = 1,  // globals (binding 0)
  });

  const Framebuffer *framebuffer = r_device.device->framebuffer;

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.vertex_shader = vertexShader->shader;
  info.fragment_shader = fragmentShader->shader;

  // Bring-up: draw all faces regardless of winding so geometry is guaranteed visible.
  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {
      { .slot = 0, .pitch = sizeof(r_mesh_vertex_t), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
      { .slot = 1, .pitch = sizeof(r_mesh_vertex_t), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
    },
    .num_vertex_buffers = 2,
    .vertex_attributes = (SDL_GPUVertexAttribute[]) {
      // old frame (slot 0)
      { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, position) },
      { .location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, normal) },
      { .location = 4, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = offsetof(r_mesh_vertex_t, diffusemap) },
      // current frame (slot 1)
      { .location = 5, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, position) },
      { .location = 6, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, normal) },
    },
    .num_vertex_attributes = 5,
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

  r_mesh_pipeline = $(r_device.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);

  r_mesh_sampler = $(r_device.device, createSampler, &(SDL_GPUSamplerCreateInfo) {
    .min_filter = SDL_GPU_FILTER_LINEAR,
    .mag_filter = SDL_GPU_FILTER_LINEAR,
    .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
    .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
  });

  r_mesh_voxel_sampler = $(r_device.device, createSampler, &(SDL_GPUSamplerCreateInfo) {
    .min_filter = SDL_GPU_FILTER_NEAREST,
    .mag_filter = SDL_GPU_FILTER_NEAREST,
    .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
    .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
  });
}

/**
 * @brief Releases the mesh pipeline and samplers.
 */
void R_ShutdownMeshProgram(void) {
  r_mesh_pipeline = release(r_mesh_pipeline);
  r_mesh_sampler = release(r_mesh_sampler);
  r_mesh_voxel_sampler = release(r_mesh_voxel_sampler);
}
