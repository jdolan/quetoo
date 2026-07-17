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
 * @brief Mesh has no samplers beyond the shared family (r_material.h).
 */
#define MESH_NUM_VERTEX_SAMPLERS R_SAMPLER_MATERIAL_TOTAL
#define MESH_NUM_SAMPLERS        R_SAMPLER_MATERIAL_TOTAL

enum {
  MESH_UNIFORMS_GLOBALS,
  MESH_UNIFORMS_LOCALS,
  MESH_UNIFORMS_MATERIAL,
  MESH_NUM_UNIFORMS
};

/**
 * @brief The maximum number of cached material-stage pipelines (one per blend).
 */
#define MAX_STAGE_PIPELINES 16

/**
 * @brief Mesh draw pipelines, samplers, and stage cache.
 */
static struct {

  /**
   * @brief The mesh graphics pipeline.
   */
  GraphicsPipeline *pipeline;

  /**
   * @brief The alpha-test mesh pipeline.
   */
  GraphicsPipeline *pipeline_alpha_test;

  /**
   * @brief The translucent mesh pipeline.
   */
  GraphicsPipeline *blend_pipeline;

  /**
   * @brief Sampler for material textures.
   */
  Sampler *diffusemap_sampler;

  /**
   * @brief Sampler for ambient voxel and sky textures.
   */
  Sampler *ambient_sampler;

  /**
   * @brief Sampler for material stage textures.
   */
  Sampler *stage_sampler;

  /**
   * @brief Default shell image.
   */
  r_image_t *shell;

  /**
   * @brief Fallback voxel textures for player-model views.
   */
  Texture *voxel_caustics_fallback;
  Texture *voxel_occlusion_fallback;

  /**
   * @brief Cached stage pipelines keyed by blend function.
   */
  struct {
    cm_blend_t src, dest;
    GraphicsPipeline *pipeline;
  } stages[MAX_STAGE_PIPELINES];

  int32_t num_stages;

  /**
   * @brief Active render pass state.
   */
  RenderPass *pass;
  CommandBuffer *commands;

  /**
   * @brief The material, stage-draw flag and dynamic light mask for the face in progress.
   */
  const r_material_t *material;
  bool draw_stages;
  const r_active_dynamic_lights_t *active_dynamic_lights;
} r_mesh_draw;

/**
 * @brief Per-entity mesh vertex uniforms.
 */
typedef struct {
  mat4_t model;
  float lerp;
  float padding[3];
  vec4_t color;
  r_active_dynamic_lights_t active_dynamic_lights;
} r_mesh_locals_t;

/**
 * @brief Per-entity fragment locals.
 */
typedef struct {
  r_active_dynamic_lights_t active_dynamic_lights;
} r_mesh_fragment_locals_t;

/**
 * @brief Returns the cached mesh stage pipeline for the specified blend function.
 */
static GraphicsPipeline *R_MeshStagePipeline(cm_blend_t src, cm_blend_t dest) {

  for (int32_t i = 0; i < r_mesh_draw.num_stages; i++) {
    if (r_mesh_draw.stages[i].src == src && r_mesh_draw.stages[i].dest == dest) {
      return r_mesh_draw.stages[i].pipeline;
    }
  }

  if (r_mesh_draw.num_stages == MAX_STAGE_PIPELINES) {
    return NULL;
  }

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/mesh_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_samplers = MESH_NUM_VERTEX_SAMPLERS,
    .num_storage_buffers = R_STORAGE_MATERIAL_TOTAL,
    .num_uniform_buffers = MESH_NUM_UNIFORMS,
  });

  Shader *fragmentShader = $(r_context.device, loadShader, "shaders/mesh_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = MESH_NUM_SAMPLERS,
    .num_storage_buffers = R_STORAGE_MATERIAL_TOTAL,
    .num_uniform_buffers = MESH_NUM_UNIFORMS,
  });

  const SDL_GPUBlendFactor s = R_BlendFactor(src);
  const SDL_GPUBlendFactor d = R_BlendFactor(dest);

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = r_scene_samples;
  info.vertex_shader = vertexShader->shader;
  info.fragment_shader = fragmentShader->shader;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;

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

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = (SDL_GPUColorTargetDescription[]) {
      {
        .format = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT,
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
    .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
    .has_depth_stencil_target = true,
  };

  GraphicsPipeline *pipeline = $(r_context.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);

  r_mesh_draw.stages[r_mesh_draw.num_stages] = (typeof(r_mesh_draw.stages[0])) {
    .src = src, .dest = dest, .pipeline = pipeline,
  };
  return r_mesh_draw.stages[r_mesh_draw.num_stages++].pipeline;
}

/**
 * @brief Draws one material stage for a mesh face.
 */
static void R_DrawMeshEntityMaterialStage(const r_view_t *view, const r_entity_t *e, const r_mesh_face_t *face, const r_stage_t *stage) {

  const r_material_t *material = r_mesh_draw.material;

  r_mesh_material_uniforms_t uniforms = { 0 };
  R_MaterialUniforms(material, material->cm->surface, &uniforms.material);

  SDL_GPUTexture *texture, *texture_next;
  if (!R_StageUniforms(view, e, NULL, stage, &uniforms.material, &texture, &texture_next)) {
    return;
  }

  GraphicsPipeline *pipeline = R_MeshStagePipeline(stage->cm->blend.src, stage->cm->blend.dest);
  if (!pipeline) {
    return;
  }

  $(r_mesh_draw.pass, bindPipeline, pipeline);

  $(r_mesh_draw.pass, bindFragmentSamplers, R_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = texture, .sampler = r_mesh_draw.stage_sampler->sampler },
    { .texture = texture_next, .sampler = r_mesh_draw.stage_sampler->sampler },
  }, 2);

  $(r_mesh_draw.commands, pushVertexUniformData, MESH_UNIFORMS_MATERIAL, &uniforms.material, sizeof(uniforms.material));
  $(r_mesh_draw.commands, pushFragmentUniformData, MESH_UNIFORMS_MATERIAL, &uniforms, sizeof(uniforms));

  const uint32_t firstIndex = (uint32_t) ((uintptr_t) face->indices / sizeof(uint32_t));
  $(r_mesh_draw.pass, drawIndexedPrimitives, face->num_elements, 1, firstIndex, 0, 0);

  r_stats.mesh_triangles += face->num_elements / 3;
}

/**
 * @brief Draws a shell stage for a mesh face.
 */
static void R_DrawMeshEntityShellEffect(const r_view_t *view, const r_entity_t *e, const r_mesh_face_t *face) {

  if (!(e->effects & EF_SHELL)) {
    return;
  }

  if (!r_mesh_draw.shell) {
    r_mesh_draw.shell = R_LoadImage("textures/envmaps/white", IMG_PROGRAM);
    if (!r_mesh_draw.shell) {
      return;
    }
  }

  for (const r_stage_t *stage = r_mesh_draw.material->stages; stage; stage = stage->next) {
    if (stage->cm->flags & STAGE_SHELL) {
      R_DrawMeshEntityMaterialStage(view, e, face, stage);
      return;
    }
  }

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
    .media = (r_media_t *) r_mesh_draw.shell,
  };

  R_DrawMeshEntityMaterialStage(view, e, face, &default_shell);
}

/**
 * @brief Draws a mesh face's material stages and shell effect.
 */
static void R_DrawMeshEntityMaterialStages(const r_view_t *view, const r_entity_t *e, const r_mesh_face_t *face) {

  const r_material_t *material = r_mesh_draw.material;

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
    R_DrawMeshEntityMaterialStage(view, e, face, stage);
  }

  R_DrawMeshEntityShellEffect(view, e, face);
}

/**
 * @brief Binds per-face mesh uniforms and vertex buffers.
 */
static void R_BindMeshEntityFace(const r_entity_t *e, const r_mesh_model_t *mesh, const r_mesh_face_t *face) {

  r_mesh_locals_t locals = {
    .model = e->matrix,
    .lerp = e->lerp,
    .color = e->color,
  };
  memcpy(&locals.active_dynamic_lights, r_mesh_draw.active_dynamic_lights, sizeof(locals.active_dynamic_lights));
  switch (r_mesh_draw.material->cm->surface & SURF_MASK_BLEND) {
    case SURF_BLEND_33:
      locals.color.w *= .333f;
      break;
    case SURF_BLEND_66:
      locals.color.w *= .666f;
      break;
    default:
      break;
  }
  $(r_mesh_draw.commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &locals, sizeof(locals));

  const uint32_t stride = sizeof(r_mesh_vertex_t);
  const uint32_t old_offset = (uint32_t) (face->base_vertex + e->old_frame * face->num_vertexes) * stride;
  const uint32_t cur_offset = (uint32_t) (face->base_vertex + e->frame * face->num_vertexes) * stride;

  $(r_mesh_draw.pass, bindVertexBuffers, 0, (SDL_GPUBufferBinding[]) {
    { .buffer = mesh->vertex_buffer->buffer, .offset = old_offset },
    { .buffer = mesh->vertex_buffer->buffer, .offset = cur_offset },
  }, 2);
}

/**
 * @brief Draws a single mesh face.
 */
static void R_DrawMeshEntityFace(const r_view_t *view, const r_entity_t *e, const r_mesh_model_t *mesh,
                                 const r_mesh_face_t *face) {

  const r_material_t *material = r_mesh_draw.material;

  $(r_mesh_draw.pass, bindFragmentSamplers, R_SAMPLER_MATERIAL, &(SDL_GPUTextureSamplerBinding) {
    .texture = material->texture->texture->texture,
    .sampler = r_mesh_draw.diffusemap_sampler->sampler,
  }, 1);

  r_mesh_material_uniforms_t material_uniforms;
  R_MaterialUniforms(material, material->cm->surface, &material_uniforms.material);
  memcpy(material_uniforms.tint_colors, e->tints, sizeof(material_uniforms.tint_colors));

  for (size_t i = 0; i < lengthof(material_uniforms.tint_colors); i++) {
    if (!e->tints[i].w) {
      material_uniforms.tint_colors[i] = material->cm->tintmap_defaults[i];
    }
  }
  $(r_mesh_draw.commands, pushVertexUniformData, MESH_UNIFORMS_MATERIAL, &material_uniforms.material, sizeof(material_uniforms.material));
  $(r_mesh_draw.commands, pushFragmentUniformData, MESH_UNIFORMS_MATERIAL, &material_uniforms, sizeof(material_uniforms));

  R_BindMeshEntityFace(e, mesh, face);

  const uint32_t firstIndex = (uint32_t) ((uintptr_t) face->indices / sizeof(uint32_t));

  $(r_mesh_draw.pass, drawIndexedPrimitives, face->num_elements, 1, firstIndex, 0, 0);

  r_stats.mesh_draw_elements++;
  r_stats.mesh_triangles += face->num_elements / 3;

  if (r_mesh_draw.draw_stages) {
    R_DrawMeshEntityMaterialStages(view, e, face);
  }
}

/**
 * @brief Draws a mesh face's material stages without the base face draw.
 */
static void R_DrawMeshEntityFaceMaterialStages(const r_view_t *view, const r_entity_t *e,
                                               const r_mesh_model_t *mesh, const r_mesh_face_t *face) {

  const r_material_t *material = r_mesh_draw.material;

  $(r_mesh_draw.pass, bindFragmentSamplers, R_SAMPLER_MATERIAL, &(SDL_GPUTextureSamplerBinding) {
    .texture = material->texture->texture->texture,
    .sampler = r_mesh_draw.diffusemap_sampler->sampler,
  }, 1);

  R_BindMeshEntityFace(e, mesh, face);

  R_DrawMeshEntityMaterialStages(view, e, face);
}

/**
 * @brief Draws a mesh entity.
 */
static void R_DrawMeshEntity(const r_view_t *view, const r_entity_t *e) {

  const r_mesh_model_t *mesh = e->model->mesh;
  assert(mesh);

  if (!mesh->vertex_buffer) {
    return;
  }

  if (e->effects & EF_WEAPON) {
    $(r_mesh_draw.pass, setViewport, &(SDL_GPUViewport) {
      .x = 0.f, .y = 0.f,
      .w = (float) view->framebuffer->size.w, .h = (float) view->framebuffer->size.h,
      .min_depth = 0.f, .max_depth = .1f,
    });
  }

  r_mesh_fragment_locals_t fragment_locals = { 0 };
  memcpy(&fragment_locals.active_dynamic_lights, &e->active_dynamic_lights, sizeof(fragment_locals.active_dynamic_lights));
  $(r_mesh_draw.commands, pushFragmentUniformData, MESH_UNIFORMS_LOCALS, &fragment_locals, sizeof(fragment_locals));

  r_mesh_draw.active_dynamic_lights = &fragment_locals.active_dynamic_lights;

  $(r_mesh_draw.pass, bindIndexBuffer, &(SDL_GPUBufferBinding) {
    .buffer = mesh->elements_buffer->buffer
  }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  const r_mesh_face_t *face = mesh->faces;
  for (int32_t i = 0; i < mesh->num_faces; i++, face++) {

    const r_material_t *material = e->skins[i] ?: face->material;

    if ((material->cm->surface & SURF_MASK_BLEND) || (e->effects & EF_BLEND)) {
      continue;
    }

    if (!material->texture || !material->texture->texture) {
      continue;
    }

    if (material->cm->surface & SURF_ALPHA_TEST) {
      continue;
    }

    $(r_mesh_draw.pass, bindPipeline, r_mesh_draw.pipeline);

    r_mesh_draw.material = material;
    r_mesh_draw.draw_stages = false;
    R_DrawMeshEntityFace(view, e, mesh, face);
  }

  face = mesh->faces;
  for (int32_t i = 0; i < mesh->num_faces; i++, face++) {

    const r_material_t *material = e->skins[i] ?: face->material;

    if ((material->cm->surface & SURF_MASK_BLEND) || (e->effects & EF_BLEND)) {
      continue;
    }

    if (!material->texture || !material->texture->texture) {
      continue;
    }

    if (!(material->cm->surface & SURF_ALPHA_TEST)) {
      continue;
    }

    $(r_mesh_draw.pass, bindPipeline, r_mesh_draw.pipeline_alpha_test);

    r_mesh_draw.material = material;
    r_mesh_draw.draw_stages = false;
    R_DrawMeshEntityFace(view, e, mesh, face);
  }

  if (r_draw_material_stages->integer) {

    face = mesh->faces;
    for (int32_t i = 0; i < mesh->num_faces; i++, face++) {

      const r_material_t *material = e->skins[i] ?: face->material;

      if ((material->cm->surface & SURF_MASK_BLEND) || (e->effects & EF_BLEND)) {
        continue;
      }

      if (!material->texture || !material->texture->texture) {
        continue;
      }

      r_mesh_draw.material = material;
      R_DrawMeshEntityFaceMaterialStages(view, e, mesh, face);
    }
  }

  face = mesh->faces;
  for (int32_t i = 0; i < mesh->num_faces; i++, face++) {

    const r_material_t *material = e->skins[i] ?: face->material;

    if (!((material->cm->surface & SURF_MASK_BLEND) || (e->effects & EF_BLEND))) {
      continue;
    }

    if (!material->texture || !material->texture->texture) {
      continue;
    }

    $(r_mesh_draw.pass, bindPipeline, r_mesh_draw.blend_pipeline);

    r_mesh_draw.material = material;
    r_mesh_draw.draw_stages = true;
    R_DrawMeshEntityFace(view, e, mesh, face);
  }

  if (e->effects & EF_WEAPON) {
    $(r_mesh_draw.pass, setViewport, &(SDL_GPUViewport) {
      .x = 0.f, .y = 0.f,
      .w = (float) view->framebuffer->size.w, .h = (float) view->framebuffer->size.h,
      .min_depth = 0.f, .max_depth = 1.f,
    });
  }

  r_stats.mesh_models++;
}

/**
 * @brief Draws mesh entities for the view.
 */
void R_DrawMeshEntities(RenderPass *pass, const r_view_t *view) {

  if (!r_models.world) {
    return;
  }

  CommandBuffer *commands = r_context.device->commands;

  r_mesh_draw.pass = pass;
  r_mesh_draw.commands = commands;

  const r_bsp_model_t *bsp = r_models.world->bsp;
  Framebuffer *framebuffer = view->framebuffer;

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  $(commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

  $(pass, bindPipeline, r_mesh_draw.pipeline);

  $(pass, bindFragmentSamplers, R_SAMPLER_SHADOW_ATLAS_0, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_shadow_atlas.textures[0]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[1]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[2]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[3]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[4]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[5]->texture, .sampler = r_shadow_atlas.sampler->sampler },
  }, 6);

  $(pass, bindFragmentSamplers, R_SAMPLER_VOXEL_CAUSTICS, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = bsp->voxels.caustics->texture->texture, .sampler = r_mesh_draw.ambient_sampler->sampler },
    { .texture = bsp->voxels.occlusion->texture->texture, .sampler = r_mesh_draw.ambient_sampler->sampler },
    { .texture = R_SkyTexture()->texture, .sampler = r_mesh_draw.ambient_sampler->sampler },
  }, 3);

  $(pass, bindVertexSamplers, R_SAMPLER_MATERIAL, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_context.null_texture->texture, .sampler = r_mesh_draw.stage_sampler->sampler },
    { .texture = r_shadow_atlas.textures[0]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[1]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[2]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[3]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[4]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[5]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = bsp->voxels.caustics->texture->texture, .sampler = r_mesh_draw.ambient_sampler->sampler },
    { .texture = bsp->voxels.occlusion->texture->texture, .sampler = r_mesh_draw.ambient_sampler->sampler },
    { .texture = R_SkyTexture()->texture, .sampler = r_mesh_draw.ambient_sampler->sampler },
    { .texture = r_context.null_texture->texture, .sampler = r_mesh_draw.stage_sampler->sampler },
    { .texture = r_context.null_texture->texture, .sampler = r_mesh_draw.stage_sampler->sampler },
  }, 12);

  $(pass, bindFragmentSamplers, R_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_context.null_texture->texture, .sampler = r_mesh_draw.stage_sampler->sampler },
    { .texture = r_context.null_texture->texture, .sampler = r_mesh_draw.stage_sampler->sampler },
  }, 2);

  SDL_GPUBuffer *storage[] = {
    r_lights.bsp_buffer->buffer,
    r_lights.dynamic_buffer->buffer,
    bsp->voxels.light_data_buffer->buffer,
    bsp->voxels.light_indices_buffer ? bsp->voxels.light_indices_buffer->buffer : r_lights.bsp_buffer->buffer,
  };
  $(pass, bindFragmentStorageBuffers, R_STORAGE_BSP_LIGHTS, storage, R_STORAGE_MATERIAL_TOTAL);
  $(pass, bindVertexStorageBuffers, R_STORAGE_BSP_LIGHTS, storage, R_STORAGE_MATERIAL_TOTAL);

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

    R_DrawMeshEntity(view, e);
    r_stats.entities_visible++;
  }

  r_mesh_draw.pass = NULL;
}

/**
 * @brief Draws the player-model preview view.
 */
void R_DrawPlayerModelView(r_view_t *view) {

  if (!r_mesh_draw.pipeline || !view->framebuffer) {
    return;
  }

  R_UpdateUniforms(view);
  R_UpdateEntities(view);

  CommandBuffer *commands = r_context.device->commands;
  if (!commands) {
    return;
  }

  Framebuffer *framebuffer = view->framebuffer;

  const SDL_GPUColorTargetInfo color[] = {
    $(framebuffer, colorTargetInfo, 0, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE),
    $(framebuffer, colorTargetInfo, 1, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE),
  };
  const SDL_GPUDepthStencilTargetInfo depth =
      $(framebuffer, depthTargetInfo, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE);

  RenderPass *pass = $(commands, beginRenderPass, color, 2, &depth);

  r_mesh_draw.pass = pass;
  r_mesh_draw.commands = commands;

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  $(commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

  $(pass, bindPipeline, r_mesh_draw.pipeline);

  $(pass, bindFragmentSamplers, R_SAMPLER_SHADOW_ATLAS_0, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_shadow_atlas.textures[0]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[1]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[2]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[3]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[4]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[5]->texture, .sampler = r_shadow_atlas.sampler->sampler },
  }, 6);

  $(pass, bindFragmentSamplers, R_SAMPLER_VOXEL_CAUSTICS, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_mesh_draw.voxel_caustics_fallback->texture, .sampler = r_mesh_draw.ambient_sampler->sampler },
    { .texture = r_mesh_draw.voxel_occlusion_fallback->texture, .sampler = r_mesh_draw.ambient_sampler->sampler },
    { .texture = R_SkyTexture()->texture, .sampler = r_mesh_draw.ambient_sampler->sampler },
  }, 3);

  $(pass, bindVertexSamplers, R_SAMPLER_MATERIAL, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_context.null_texture->texture, .sampler = r_mesh_draw.stage_sampler->sampler },
    { .texture = r_shadow_atlas.textures[0]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[1]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[2]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[3]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[4]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[5]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_mesh_draw.voxel_caustics_fallback->texture, .sampler = r_mesh_draw.ambient_sampler->sampler },
    { .texture = r_mesh_draw.voxel_occlusion_fallback->texture, .sampler = r_mesh_draw.ambient_sampler->sampler },
    { .texture = R_SkyTexture()->texture, .sampler = r_mesh_draw.ambient_sampler->sampler },
    { .texture = r_context.null_texture->texture, .sampler = r_mesh_draw.stage_sampler->sampler },
    { .texture = r_context.null_texture->texture, .sampler = r_mesh_draw.stage_sampler->sampler },
  }, 12);

  $(pass, bindFragmentSamplers, R_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_context.null_texture->texture, .sampler = r_mesh_draw.stage_sampler->sampler },
    { .texture = r_context.null_texture->texture, .sampler = r_mesh_draw.stage_sampler->sampler },
  }, 2);

  SDL_GPUBuffer *storage[] = {
    r_lights.bsp_buffer->buffer, r_lights.dynamic_buffer->buffer,
    r_lights.bsp_buffer->buffer, r_lights.bsp_buffer->buffer,
  };
  $(pass, bindFragmentStorageBuffers, R_STORAGE_BSP_LIGHTS, storage, R_STORAGE_MATERIAL_TOTAL);
  $(pass, bindVertexStorageBuffers, R_STORAGE_BSP_LIGHTS, storage, R_STORAGE_MATERIAL_TOTAL);

  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_MESH_MODEL(e->model)) {
      continue;
    }

    if (e->effects & EF_NO_DRAW) {
      continue;
    }

    R_DrawMeshEntity(view, e);
    r_stats.entities_visible++;
  }

  r_mesh_draw.pass = NULL;

  pass = release(pass);
}

/**
 * @brief Initializes the mesh pipelines and samplers.
 */
void R_InitMeshPipeline(void) {

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/mesh_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_samplers = MESH_NUM_VERTEX_SAMPLERS,
    .num_storage_buffers = R_STORAGE_MATERIAL_TOTAL,
    .num_uniform_buffers = MESH_NUM_UNIFORMS,
  });

  Shader *fragmentShader = $(r_context.device, loadShader, "shaders/mesh_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = MESH_NUM_SAMPLERS,
    .num_storage_buffers = R_STORAGE_MATERIAL_TOTAL,
    .num_uniform_buffers = MESH_NUM_UNIFORMS,
  });

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = r_scene_samples;
  info.vertex_shader = vertexShader->shader;
  info.fragment_shader = fragmentShader->shader;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;

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

  SDL_GPUColorTargetDescription color_targets[] = {
    { .format = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT, .blend_state = GPU_BlendStateOpaque },
    { .format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT, .blend_state = GPU_BlendStateOpaque },
  };

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = color_targets,
    .num_color_targets = 2,
    .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
    .has_depth_stencil_target = true,
  };

  r_mesh_draw.pipeline = $(r_context.device, createGraphicsPipeline, &info);

  Shader *alphaTestFragmentShader = $(r_context.device, loadShader, "shaders/mesh_fs_alpha_test", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = MESH_NUM_SAMPLERS,
    .num_storage_buffers = R_STORAGE_MATERIAL_TOTAL,
    .num_uniform_buffers = MESH_NUM_UNIFORMS,
  });

  info.fragment_shader = alphaTestFragmentShader->shader;
  r_mesh_draw.pipeline_alpha_test = $(r_context.device, createGraphicsPipeline, &info);
  release(alphaTestFragmentShader);

  info.fragment_shader = fragmentShader->shader;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  color_targets[0].blend_state = GPU_BlendStateAlpha;

  r_mesh_draw.blend_pipeline = $(r_context.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);

  r_mesh_draw.diffusemap_sampler = $(r_context.device, createSamplerLinearRepeat);
  r_mesh_draw.ambient_sampler = $(r_context.device, createSamplerLinearClamp);
  r_mesh_draw.stage_sampler = $(r_context.device, createSamplerLinearRepeat);

  r_mesh_draw.voxel_caustics_fallback = $(r_context.device, createTexture, &(SDL_GPUTextureCreateInfo) {
    .type = SDL_GPU_TEXTURETYPE_3D,
    .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
    .width = 1, .height = 1, .layer_count_or_depth = 1,
    .num_levels = 1,
    .sample_count = SDL_GPU_SAMPLECOUNT_1,
  }, NULL);

  r_mesh_draw.voxel_occlusion_fallback = $(r_context.device, createTexture, &(SDL_GPUTextureCreateInfo) {
    .type = SDL_GPU_TEXTURETYPE_3D,
    .format = SDL_GPU_TEXTUREFORMAT_R8G8_UNORM,
    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
    .width = 1, .height = 1, .layer_count_or_depth = 1,
    .num_levels = 1,
    .sample_count = SDL_GPU_SAMPLECOUNT_1,
  }, NULL);
}

/**
 * @brief Releases the mesh pipelines and samplers.
 */
void R_ShutdownMeshPipeline(void) {
  r_mesh_draw.pipeline = release(r_mesh_draw.pipeline);
  r_mesh_draw.pipeline_alpha_test = release(r_mesh_draw.pipeline_alpha_test);
  r_mesh_draw.blend_pipeline = release(r_mesh_draw.blend_pipeline);
  r_mesh_draw.diffusemap_sampler = release(r_mesh_draw.diffusemap_sampler);
  r_mesh_draw.ambient_sampler = release(r_mesh_draw.ambient_sampler);
  r_mesh_draw.stage_sampler = release(r_mesh_draw.stage_sampler);
  r_mesh_draw.voxel_caustics_fallback = release(r_mesh_draw.voxel_caustics_fallback);
  r_mesh_draw.voxel_occlusion_fallback = release(r_mesh_draw.voxel_occlusion_fallback);

  for (int32_t i = 0; i < r_mesh_draw.num_stages; i++) {
    r_mesh_draw.stages[i].pipeline = release(r_mesh_draw.stages[i].pipeline);
  }
  r_mesh_draw.num_stages = 0;

}

/**
 * @brief Rebuilds the mesh pipelines and samplers.
 */
void R_UpdateMeshPipeline(void) {
  R_ShutdownMeshPipeline();
  R_InitMeshPipeline();
}
