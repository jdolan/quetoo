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
 * @brief The maximum number of cached material-stage pipelines (one per blend).
 */
#define MAX_STAGE_PIPELINES 16

/**
 * @brief The mesh program (mesh_vs/mesh_fs): its graphics pipeline and samplers.
 * Animated geometry with diffuse material, clustered per-voxel lighting, entity
 * color/tint, material stages, and the EF_SHELL effect.
 * @remarks TODO(#864): color/blend faces and the player-model view are ported later.
 */
static struct {

  /**
   * @brief The mesh graphics pipeline.
   */
  GraphicsPipeline *pipeline;

  /**
   * @brief The material sampler (trilinear, repeat).
   */
  Sampler *diffusemap_sampler;

  /**
   * @brief The voxel light-data sampler (nearest, clamp) for integer texelFetch.
   */
  Sampler *voxel_data_sampler;

  /**
   * @brief A linear, clamped sampler shared by the voxel caustics/occlusion
   * volumes and the sky cubemap (all sampled with normalized coordinates).
   */
  Sampler *ambient_sampler;

  /**
   * @brief The material stage sampler (linear, repeat) for stage textures.
   */
  Sampler *stage_sampler;

  /**
   * @brief The default shell environment map, loaded lazily for EF_SHELL entities.
   */
  r_image_t *shell;

  /**
   * @brief Cache of MATERIAL_STAGES mesh pipelines, one per unique (src, dest)
   * blend function (SDL_gpu bakes blend state into the pipeline).
   */
  struct {
    cm_blend_t src, dest;
    GraphicsPipeline *pipeline;
  } stages[MAX_STAGE_PIPELINES];

  int32_t num_stages;
} r_mesh_pipeline;

/**
 * @brief Per-entity mesh locals, pushed to vertex uniform slot 1.
 */
typedef struct {
  mat4_t model;
  float lerp;
  float padding[3];
  vec4_t color;
} r_mesh_locals_t;

/**
 * @brief Maps a `cm_blend_t` blend factor to its SDL_gpu equivalent.
 */
static SDL_GPUBlendFactor R_BlendFactor(cm_blend_t blend) {
  switch (blend) {
    case BLEND_ZERO:                return SDL_GPU_BLENDFACTOR_ZERO;
    case BLEND_ONE:                 return SDL_GPU_BLENDFACTOR_ONE;
    case BLEND_SRC_COLOR:           return SDL_GPU_BLENDFACTOR_SRC_COLOR;
    case BLEND_ONE_MINUS_SRC_COLOR: return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
    case BLEND_SRC_ALPHA:           return SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    case BLEND_ONE_MINUS_SRC_ALPHA: return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    case BLEND_DST_COLOR:           return SDL_GPU_BLENDFACTOR_DST_COLOR;
    default:                        return SDL_GPU_BLENDFACTOR_ONE;
  }
}

/**
 * @brief Returns the MATERIAL_STAGES mesh pipeline for the given stage blend
 * function, creating and caching it on first use.
 */
static GraphicsPipeline *R_MeshStagePipeline(cm_blend_t src, cm_blend_t dest) {

  for (int32_t i = 0; i < r_mesh_pipeline.num_stages; i++) {
    if (r_mesh_pipeline.stages[i].src == src && r_mesh_pipeline.stages[i].dest == dest) {
      return r_mesh_pipeline.stages[i].pipeline;
    }
  }

  if (r_mesh_pipeline.num_stages == MAX_STAGE_PIPELINES) {
    return NULL;
  }

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/mesh_stage_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 3, // globals (0) + locals/model (1) + stage (2)
  });

  Shader *fragmentShader = $(r_context.device, loadShader, "shaders/mesh_stage_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = 8,        // material + voxel_light_data + shadow_atlas + voxel_caustics + voxel_occlusion + sky + stage + stage_next
    .num_storage_buffers = 2, // lights + voxel_light_indices
    .num_uniform_buffers = 4, // globals (0) + active_lights (1) + material (2) + stage (3)
  });

  const Framebuffer *framebuffer = r_context.device->framebuffer;

  const SDL_GPUBlendFactor s = R_BlendFactor(src);
  const SDL_GPUBlendFactor d = R_BlendFactor(dest);

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = r_scene_samples;
  info.vertex_shader = vertexShader->shader;
  info.fragment_shader = fragmentShader->shader;

  // Cull back faces; front faces are wound clockwise (matching the GL renderer).
  // If mesh models turn inside-out, flip to COUNTER_CLOCKWISE.
  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;

  // Stages blend over the already-lit base surface; test depth but do not write it.
  info.depth_stencil_state.enable_depth_write = false;

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {
      { .slot = 0, .pitch = sizeof(r_mesh_vertex_t), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
      { .slot = 1, .pitch = sizeof(r_mesh_vertex_t), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
    },
    .num_vertex_buffers = 2,
    .vertex_attributes = (SDL_GPUVertexAttribute[]) {
      { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, position) },
      { .location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, normal) },
      { .location = 2, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, tangent) },
      { .location = 3, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, bitangent) },
      { .location = 4, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = offsetof(r_mesh_vertex_t, diffusemap) },
      { .location = 5, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, position) },
      { .location = 6, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, normal) },
      { .location = 7, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, tangent) },
      { .location = 8, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, bitangent) },
    },
    .num_vertex_attributes = 9,
  };

  // Two color targets to match the opaque mesh pass; the depth copy (color 1) is
  // masked, since a stage overlays the already-established surface.
  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = (SDL_GPUColorTargetDescription[]) {
      {
        .format = R_SCENE_COLOR_FORMAT,
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
    .depth_stencil_format = framebuffer->depthFormat,
    .has_depth_stencil_target = true,
  };

  GraphicsPipeline *pipeline = $(r_context.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);

  r_mesh_pipeline.stages[r_mesh_pipeline.num_stages] = (typeof(r_mesh_pipeline.stages[0])) {
    .src = src, .dest = dest, .pipeline = pipeline,
  };
  return r_mesh_pipeline.stages[r_mesh_pipeline.num_stages++].pipeline;
}

/**
 * @brief Draws a single material stage of a mesh face, layered over the already-lit
 * base surface. The base draw's material/voxel/shadow samplers, storage buffers,
 * per-entity uniforms, and vertex buffers remain bound; this only rebinds the stage
 * pipeline, its textures, and the per-stage uniforms.
 */
static void R_DrawMeshFaceMaterialStage(const r_view_t *view, RenderPass *pass, CommandBuffer *commands,
                                        const r_entity_t *e, const r_mesh_face_t *face, const r_stage_t *stage) {

  r_stage_uniforms_t uniforms;
  SDL_GPUTexture *texture, *texture_next;
  if (!R_StageUniforms(view, e, NULL, stage, &uniforms, &texture, &texture_next)) {
    return;
  }

  GraphicsPipeline *pipeline = R_MeshStagePipeline(stage->cm->blend.src, stage->cm->blend.dest);
  if (!pipeline) {
    return;
  }

  $(pass, bindPipeline, pipeline);

  $(pass, bindFragmentSamplers, SLOT_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = texture, .sampler = r_mesh_pipeline.stage_sampler->sampler },
    { .texture = texture_next, .sampler = r_mesh_pipeline.stage_sampler->sampler },
  }, 2);

  $(commands, pushVertexUniformData, SLOT_UNIFORMS_STAGE_VERTEX, &uniforms, sizeof(uniforms));
  $(commands, pushFragmentUniformData, SLOT_UNIFORMS_STAGE_FRAGMENT, &uniforms, sizeof(uniforms));

  const uint32_t firstIndex = (uint32_t) ((uintptr_t) face->indices / sizeof(uint32_t));
  $(pass, drawIndexedPrimitives, face->num_elements, 1, firstIndex, 0, 0);

  r_stats.mesh_triangles += face->num_elements / 3;
}

/**
 * @brief Draws the shell effect on a mesh face if the entity has EF_SHELL set,
 * using a material-driven STAGE_SHELL stage if present, otherwise a synthesized
 * default shell (an environment-mapped, expanded, colored additive overlay).
 */
static void R_DrawMeshFaceShell(const r_view_t *view, RenderPass *pass, CommandBuffer *commands,
                                const r_entity_t *e, const r_mesh_face_t *face, const r_material_t *material) {

  if (!(e->effects & EF_SHELL)) {
    return;
  }

  if (!r_mesh_pipeline.shell) {
    r_mesh_pipeline.shell = R_LoadImage("textures/envmaps/white", IMG_PROGRAM);
    if (!r_mesh_pipeline.shell) {
      return;
    }
  }

  for (const r_stage_t *stage = material->stages; stage; stage = stage->next) {
    if (stage->cm->flags & STAGE_SHELL) {
      R_DrawMeshFaceMaterialStage(view, pass, commands, e, face, stage);
      return;
    }
  }

  // No material-driven shell stage; synthesize a default one from EF_SHELL.
  const float radius = (e->effects & EF_WEAPON) ? .25f : 1.f;

  const cm_stage_t cm = {
    .flags = STAGE_COLOR | STAGE_SHELL
        | STAGE_SCALE_S | STAGE_SCALE_T
        | STAGE_SCROLL_S | STAGE_SCROLL_T
        | STAGE_LIGHTING | STAGE_LIGHTING_FLAT | STAGE_ENVMAP,
    .color = Color4fv(e->shell),
    .blend = { .src = BLEND_SRC_ALPHA, .dest = BLEND_ONE },
    .scroll = { 1.f, 1.f },
    .scale = { .5f, .5f },
    .shell = { radius },
    .lighting = { 1.f, STAGE_LIGHTING_MODE_FLAT },
  };

  const r_stage_t default_shell = {
    .cm = &cm,
    .media = (r_media_t *) r_mesh_pipeline.shell,
  };

  R_DrawMeshFaceMaterialStage(view, pass, commands, e, face, &default_shell);
}

/**
 * @brief Draws all active material stages of a mesh face, plus the shell effect.
 */
static void R_DrawMeshFaceMaterialStages(const r_view_t *view, RenderPass *pass, CommandBuffer *commands,
                                         const r_entity_t *e, const r_mesh_face_t *face, const r_material_t *material) {

  if (!r_draw_material_stages->integer) {
    return;
  }

  if (!(material->cm->stage_flags & STAGE_DRAW) && !(e->effects & EF_SHELL)) {
    return;
  }

  for (const r_stage_t *stage = material->stages; stage; stage = stage->next) {
    if (!(stage->cm->flags & STAGE_DRAW)) {
      continue;
    }
    R_DrawMeshFaceMaterialStage(view, pass, commands, e, face, stage);
  }

  R_DrawMeshFaceShell(view, pass, commands, e, face, material);
}

/**
 * @brief Sets up per-entity uniforms and draws all opaque faces of a mesh entity.
 */
static void R_DrawMeshEntity(RenderPass *pass, const r_view_t *view, const r_entity_t *e) {

  const r_mesh_model_t *mesh = e->model->mesh;
  assert(mesh);

  if (!mesh->vertex_buffer) {
    return;
  }

  CommandBuffer *commands = r_context.device->commands;

  const r_mesh_locals_t locals = {
    .model = e->matrix,
    .lerp = e->lerp,
    .color = e->color,
  };
  $(commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &locals, sizeof(locals));

  // The dynamic lights affecting this entity, culled to its bounds (fragment
  // slot 1 = active_lights bitmask, the same slot the vertex stage uses for the
  // model matrix).
  uint32_t active_lights[4];
  R_ActiveLights(view, e->abs_model_bounds, active_lights);
  $(commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, active_lights, sizeof(active_lights));

  // Per-entity tint colors for player-skin colorization (fragment slot 3).
  $(commands, pushFragmentUniformData, SLOT_UNIFORMS_TINTS, e->tints, sizeof(e->tints));

  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) {
    .buffer = mesh->elements_buffer->buffer
  }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  const uint32_t stride = sizeof(r_mesh_vertex_t);

  const r_mesh_face_t *face = mesh->faces;
  for (int32_t i = 0; i < mesh->num_faces; i++, face++) {

    const r_material_t *material = e->skins[i] ?: face->material;

    // TODO(#864): blended faces are drawn later.
    if ((material->cm->surface & SURF_MASK_BLEND) || (e->effects & EF_BLEND)) {
      continue;
    }

    if (!material->texture || !material->texture->texture) {
      continue;
    }

    // Re-bind the base pipeline: a preceding face's material stages may have left
    // a stage (blend) pipeline bound.
    $(pass, bindPipeline, r_mesh_pipeline.pipeline);

    $(pass, bindFragmentSamplers, SLOT_SAMPLER_MATERIAL, &(SDL_GPUTextureSamplerBinding) {
      .texture = material->texture->texture->texture,
      .sampler = r_mesh_pipeline.diffusemap_sampler->sampler,
    }, 1);

    r_material_uniforms_t material_uniforms;
    R_MaterialUniforms(material, material->cm->surface, &material_uniforms);
    $(commands, pushFragmentUniformData, SLOT_UNIFORMS_MATERIAL, &material_uniforms, sizeof(material_uniforms));

    // Two vertex buffer slots: the old frame (locations 0-4) and the current
    // frame (locations 5-8), the same model buffer bound at each frame offset.
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

    // Material stage effects and the EF_SHELL overlay, blended over this face.
    R_DrawMeshFaceMaterialStages(view, pass, commands, e, face, material);
  }

  r_stats.mesh_models++;
}

/**
 * @brief Draws mesh entities.
 */
void R_DrawMeshEntities(const r_view_t *view) {

  if (!r_mesh_pipeline.pipeline || !r_models.world) {
    return;
  }

  CommandBuffer *commands = r_context.device->commands;
  if (!commands) {
    return;
  }

  if (!view->framebuffer) {
    return;
  }

  const r_bsp_model_t *bsp = r_models.world->bsp;
  Framebuffer *framebuffer = view->framebuffer;

  // LOAD both the scene color and the depth copy; mesh entities write their
  // gl_FragCoord.z into color 1 so sprites soften against them too.
  const SDL_GPUColorTargetInfo color[] = {
    $(framebuffer, colorTargetInfo, 0, SDL_GPU_LOADOP_LOAD, SDL_GPU_STOREOP_STORE, NULL),
    $(framebuffer, colorTargetInfo, 1, SDL_GPU_LOADOP_LOAD, SDL_GPU_STOREOP_STORE, NULL),
  };
  const SDL_GPUDepthStencilTargetInfo depth =
      $(framebuffer, depthTargetInfo, SDL_GPU_LOADOP_LOAD, SDL_GPU_STOREOP_STORE, 1.f);

  RenderPass *pass = $(commands, beginRenderPass, color, 2, &depth);

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  // Per-frame globals to both stages; lighting resources shared with the BSP pass.
  $(commands, pushVertexUniformData, 0, &r_uniforms.block, sizeof(r_uniforms.block));
  $(commands, pushFragmentUniformData, 0, &r_uniforms.block, sizeof(r_uniforms.block));

  $(pass, bindPipeline, r_mesh_pipeline.pipeline);

  $(pass, bindFragmentSamplers, SLOT_SAMPLER_VOXEL_LIGHT_DATA, &(SDL_GPUTextureSamplerBinding) {
    .texture = bsp->voxels.light_data->texture->texture,
    .sampler = r_mesh_pipeline.voxel_data_sampler->sampler,
  }, 1);

  // The point-light shadow atlas (comparison sampler), shared with the BSP pass.
  $(pass, bindFragmentSamplers, SLOT_SAMPLER_SHADOW_ATLAS, &(SDL_GPUTextureSamplerBinding) {
    .texture = r_shadow_atlas.texture->texture,
    .sampler = r_shadow_atlas.sampler->sampler,
  }, 1);

  // The voxel caustics / occlusion volumes and the sky cubemap for ambient light.
  $(pass, bindFragmentSamplers, SLOT_SAMPLER_VOXEL_CAUSTICS, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = bsp->voxels.caustics->texture->texture, .sampler = r_mesh_pipeline.ambient_sampler->sampler },
    { .texture = bsp->voxels.occlusion->texture->texture, .sampler = r_mesh_pipeline.ambient_sampler->sampler },
    { .texture = R_SkyTexture()->texture, .sampler = r_mesh_pipeline.ambient_sampler->sampler },
  }, 3);

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
void R_InitMeshPipeline(void) {

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/mesh_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 2, // globals (binding 0) + locals (binding 1)
  });

  Shader *fragmentShader = $(r_context.device, loadShader, "shaders/mesh_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = 6,         // material + voxel_light_data + shadow_atlas + voxel_caustics + voxel_occlusion + sky
    .num_storage_buffers = 2,  // lights_block + voxel_light_indices_block
    .num_uniform_buffers = 4,  // globals (0) + active_lights (1) + material (2) + tints (3)
  });

  const Framebuffer *framebuffer = r_context.device->framebuffer;

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = r_scene_samples;
  info.vertex_shader = vertexShader->shader;
  info.fragment_shader = fragmentShader->shader;

  // Cull back faces, matching the base mesh pipeline (front faces clockwise).
  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {
      { .slot = 0, .pitch = sizeof(r_mesh_vertex_t), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
      { .slot = 1, .pitch = sizeof(r_mesh_vertex_t), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
    },
    .num_vertex_buffers = 2,
    .vertex_attributes = (SDL_GPUVertexAttribute[]) {
      // old frame (slot 0): position, normal, tangent, bitangent, diffusemap
      { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, position) },
      { .location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, normal) },
      { .location = 2, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, tangent) },
      { .location = 3, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, bitangent) },
      { .location = 4, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = offsetof(r_mesh_vertex_t, diffusemap) },
      // current frame (slot 1): position, normal, tangent, bitangent (diffusemap shared)
      { .location = 5, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, position) },
      { .location = 6, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, normal) },
      { .location = 7, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, tangent) },
      { .location = 8, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, bitangent) },
    },
    .num_vertex_attributes = 9,
  };

  // Two color targets: the HDR scene color and the float depth copy (mesh_fs
  // writes gl_FragCoord.z to color 1) so mesh entities appear in the depth the
  // sprite pass samples for soft particles.
  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = (SDL_GPUColorTargetDescription[]) {
      { .format = R_SCENE_COLOR_FORMAT, .blend_state = GPU_BlendStateOpaque },
      { .format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT, .blend_state = GPU_BlendStateOpaque },
    },
    .num_color_targets = 2,
    .depth_stencil_format = framebuffer->depthFormat,
    .has_depth_stencil_target = true,
  };

  r_mesh_pipeline.pipeline = $(r_context.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);

  r_mesh_pipeline.diffusemap_sampler = $(r_context.device, createSampler, &(SDL_GPUSamplerCreateInfo) {
    .min_filter = SDL_GPU_FILTER_LINEAR,
    .mag_filter = SDL_GPU_FILTER_LINEAR,
    .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
    .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
  });

  r_mesh_pipeline.voxel_data_sampler = $(r_context.device, createSampler, &(SDL_GPUSamplerCreateInfo) {
    .min_filter = SDL_GPU_FILTER_NEAREST,
    .mag_filter = SDL_GPU_FILTER_NEAREST,
    .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
    .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
  });

  // Linear / clamp: the voxel volumes and sky cubemap are sampled continuously.
  r_mesh_pipeline.ambient_sampler = $(r_context.device, createSampler, &(SDL_GPUSamplerCreateInfo) {
    .min_filter = SDL_GPU_FILTER_LINEAR,
    .mag_filter = SDL_GPU_FILTER_LINEAR,
    .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
    .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
  });

  r_mesh_pipeline.stage_sampler = $(r_context.device, createSampler, &(SDL_GPUSamplerCreateInfo) {
    .min_filter = SDL_GPU_FILTER_LINEAR,
    .mag_filter = SDL_GPU_FILTER_LINEAR,
    .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
    .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
  });
}

/**
 * @brief Releases the mesh pipeline and samplers.
 */
void R_ShutdownMeshPipeline(void) {
  r_mesh_pipeline.pipeline = release(r_mesh_pipeline.pipeline);
  r_mesh_pipeline.diffusemap_sampler = release(r_mesh_pipeline.diffusemap_sampler);
  r_mesh_pipeline.voxel_data_sampler = release(r_mesh_pipeline.voxel_data_sampler);
  r_mesh_pipeline.ambient_sampler = release(r_mesh_pipeline.ambient_sampler);
  r_mesh_pipeline.stage_sampler = release(r_mesh_pipeline.stage_sampler);

  for (int32_t i = 0; i < r_mesh_pipeline.num_stages; i++) {
    r_mesh_pipeline.stages[i].pipeline = release(r_mesh_pipeline.stages[i].pipeline);
  }
  r_mesh_pipeline.num_stages = 0;

  // r_mesh_pipeline.shell is a media object owned by the media cache; not released here.
}
