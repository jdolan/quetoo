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
 * @brief The mesh program's own binding map, mirroring shaders/mesh_vs.glsl and
 * mesh_fs.glsl. Not shared with any other pipeline; mirrors the BSP shape (see
 * r_bsp_draw.c) exactly -- the fragment material UBO additionally carries
 * per-entity tint colors (r_mesh_material_uniforms_t), since bsp has no
 * equivalent, but the vertex stage's material UBO (no tint) is the same
 * r_material_uniforms_t bsp uses.
 */
enum {
  MESH_SAMPLER_MATERIAL,
  MESH_SAMPLER_VOXEL_LIGHT_DATA,
  MESH_SAMPLER_SHADOW_ATLAS_0, // one sampler2DShadow per cube face (SDL_gpu forbids array depth targets)
  MESH_SAMPLER_SHADOW_ATLAS_1,
  MESH_SAMPLER_SHADOW_ATLAS_2,
  MESH_SAMPLER_SHADOW_ATLAS_3,
  MESH_SAMPLER_SHADOW_ATLAS_4,
  MESH_SAMPLER_SHADOW_ATLAS_5,
  MESH_SAMPLER_VOXEL_CAUSTICS,
  MESH_SAMPLER_VOXEL_OCCLUSION,
  MESH_SAMPLER_SKY_AMBIENT,
  MESH_SAMPLER_STAGE,
  MESH_SAMPLER_STAGE_NEXT,
  MESH_NUM_SAMPLERS,
};

enum {
  MESH_STORAGE_LIGHTS,
  MESH_STORAGE_VOXEL_LIGHT_INDICES,
  MESH_NUM_STORAGE_BUFFERS,
};

enum {
  MESH_UNIFORMS_GLOBALS,
  MESH_UNIFORMS_LOCALS, // model/lerp/color (vertex) / active_lights bitmask (fragment)
  MESH_UNIFORMS_MATERIAL, // material + stage params (+ tints, fragment only) -- see material.glsl
  MESH_NUM_UNIFORMS
};

/**
 * @brief The maximum number of cached material-stage pipelines (one per blend).
 */
#define MAX_STAGE_PIPELINES 16

/**
 * @brief The mesh program (mesh_vs/mesh_fs): its graphics pipeline and samplers.
 * Animated geometry with diffuse material, clustered per-voxel lighting, entity
 * color/tint, material stages, and the EF_SHELL effect -- material stages and
 * shells share this shader with the base pass via a runtime branch on
 * material.flags, not a pipeline swap: see R_DrawMeshEntityMaterialStage.
 */
static struct {

  /**
   * @brief The mesh graphics pipeline.
   */
  GraphicsPipeline *pipeline;

  /**
   * @brief The mesh pipeline variant for translucent faces (@c SURF_MASK_BLEND or
   * @c EF_BLEND): no face culling, alpha blending, matching the opaque pipeline
   * otherwise (depth test and write remain enabled, matching the GL renderer).
   */
  GraphicsPipeline *blend_pipeline;

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
   * @brief The default shell environment map, loaded lazily for @c EF_SHELL entities.
   */
  r_image_t *shell;

  /**
   * @brief 1x1x1 fallback voxel textures for the player-model preview, which has
   * no loaded BSP (and therefore no real voxel data) to sample. Their contents
   * are never read: the fragment shader skips voxel/light sampling entirely for
   * @c VIEW_PLAYER_MODEL, but @c SDL_gpu still requires every declared sampler slot to
   * be bound at draw time.
   */
  Texture *voxel_light_data_fallback;
  Texture *voxel_caustics_fallback;
  Texture *voxel_occlusion_fallback;

  /**
   * @brief Cache of @c MATERIAL_STAGES mesh pipelines, one per unique (src, dest)
   * blend function (@c SDL_gpu bakes blend state into the pipeline).
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
 * @brief Returns the mesh pipeline for the given material-stage blend function,
 * creating and caching it on first use. Wraps the same mesh_vs/mesh_fs shader
 * as the opaque pipeline (a stage draw is a runtime branch, not a shader
 * swap); only the blend state differs, which SDL_gpu bakes into the pipeline.
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

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/mesh_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = MESH_NUM_UNIFORMS,
  });

  Shader *fragmentShader = $(r_context.device, loadShader, "shaders/mesh_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = MESH_NUM_SAMPLERS,
    .num_storage_buffers = MESH_NUM_STORAGE_BUFFERS,
    .num_uniform_buffers = MESH_NUM_UNIFORMS,
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
static void R_DrawMeshEntityMaterialStage(const r_view_t *view, RenderPass *pass, CommandBuffer *commands,
                                        const r_entity_t *e, const r_mesh_face_t *face,
                                        const r_material_t *material, const r_stage_t *stage) {

  // The tint fields are irrelevant to the stage branch (mesh_fs.glsl only
  // reads them in the STAGE_NONE path), so leave them zeroed.
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

  $(pass, bindPipeline, pipeline);

  $(pass, bindFragmentSamplers, MESH_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = texture, .sampler = r_mesh_pipeline.stage_sampler->sampler },
    { .texture = texture_next, .sampler = r_mesh_pipeline.stage_sampler->sampler },
  }, 2);

  $(commands, pushVertexUniformData, MESH_UNIFORMS_MATERIAL, &uniforms.material, sizeof(uniforms.material));
  $(commands, pushFragmentUniformData, MESH_UNIFORMS_MATERIAL, &uniforms, sizeof(uniforms));

  const uint32_t firstIndex = (uint32_t) ((uintptr_t) face->indices / sizeof(uint32_t));
  $(pass, drawIndexedPrimitives, face->num_elements, 1, firstIndex, 0, 0);

  r_stats.mesh_triangles += face->num_elements / 3;
}

/**
 * @brief Draws the shell effect on a mesh face if the entity has EF_SHELL set,
 * using a material-driven STAGE_SHELL stage if present, otherwise a synthesized
 * default shell (an environment-mapped, expanded, colored additive overlay).
 */
static void R_DrawMeshEntityShellEffect(const r_view_t *view, RenderPass *pass, CommandBuffer *commands,
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
      R_DrawMeshEntityMaterialStage(view, pass, commands, e, face, material, stage);
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

  R_DrawMeshEntityMaterialStage(view, pass, commands, e, face, material, &default_shell);
}

/**
 * @brief Draws all active material stages of a mesh face, plus the shell effect.
 */
static void R_DrawMeshEntityMaterialStages(const r_view_t *view, RenderPass *pass, CommandBuffer *commands,
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
    R_DrawMeshEntityMaterialStage(view, pass, commands, e, face, material, stage);
  }

  R_DrawMeshEntityShellEffect(view, pass, commands, e, face, material);
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

  // The dynamic lights affecting this entity, culled to its bounds. Has
  // nothing to do with the material/stage/tint UBO -- see mesh_fs.glsl.
  uint32_t active_lights[MAX_DYNAMIC_LIGHTS / 32];
  R_ActiveLights(view, e->abs_model_bounds, active_lights);
  $(commands, pushFragmentUniformData, MESH_UNIFORMS_LOCALS, active_lights, sizeof(active_lights));

  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) {
    .buffer = mesh->elements_buffer->buffer
  }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  const uint32_t stride = sizeof(r_mesh_vertex_t);

  const r_mesh_face_t *face = mesh->faces;
  for (int32_t i = 0; i < mesh->num_faces; i++, face++) {

    const r_material_t *material = e->skins[i] ?: face->material;

    if ((material->cm->surface & SURF_MASK_BLEND) || (e->effects & EF_BLEND)) {
      continue;
    }

    if (!material->texture || !material->texture->texture) {
      continue;
    }

    // Re-bind the base pipeline: a preceding face's material stages may have left
    // a stage (blend) pipeline bound.
    $(pass, bindPipeline, r_mesh_pipeline.pipeline);

    $(pass, bindFragmentSamplers, MESH_SAMPLER_MATERIAL, &(SDL_GPUTextureSamplerBinding) {
      .texture = material->texture->texture->texture,
      .sampler = r_mesh_pipeline.diffusemap_sampler->sampler,
    }, 1);

    // R_MaterialUniforms resets the stage fields to STAGE_NONE: a preceding
    // face's material stages left them set, and this draw's shader branches
    // on material.flags (mesh_fs.glsl).
    r_mesh_material_uniforms_t material_uniforms;
    R_MaterialUniforms(material, material->cm->surface, &material_uniforms.material);
    memcpy(material_uniforms.tint_colors, e->tints, sizeof(material_uniforms.tint_colors));
    $(commands, pushVertexUniformData, MESH_UNIFORMS_MATERIAL, &material_uniforms.material, sizeof(material_uniforms.material));
    $(commands, pushFragmentUniformData, MESH_UNIFORMS_MATERIAL, &material_uniforms, sizeof(material_uniforms));

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
    R_DrawMeshEntityMaterialStages(view, pass, commands, e, face, material);
  }

  // Second pass: translucent faces, matching the GL renderer's per-entity
  // opaque-then-blend draw order (not a global back-to-front sort).
  face = mesh->faces;
  for (int32_t i = 0; i < mesh->num_faces; i++, face++) {

    const r_material_t *material = e->skins[i] ?: face->material;

    if (!((material->cm->surface & SURF_MASK_BLEND) || (e->effects & EF_BLEND))) {
      continue;
    }

    if (!material->texture || !material->texture->texture) {
      continue;
    }

    $(pass, bindPipeline, r_mesh_pipeline.blend_pipeline);

    $(pass, bindFragmentSamplers, MESH_SAMPLER_MATERIAL, &(SDL_GPUTextureSamplerBinding) {
      .texture = material->texture->texture->texture,
      .sampler = r_mesh_pipeline.diffusemap_sampler->sampler,
    }, 1);

    // R_MaterialUniforms resets the stage fields to STAGE_NONE: see the opaque loop above.
    r_mesh_material_uniforms_t material_uniforms;
    R_MaterialUniforms(material, material->cm->surface, &material_uniforms.material);
    memcpy(material_uniforms.tint_colors, e->tints, sizeof(material_uniforms.tint_colors));
    $(commands, pushVertexUniformData, MESH_UNIFORMS_MATERIAL, &material_uniforms.material, sizeof(material_uniforms.material));
    $(commands, pushFragmentUniformData, MESH_UNIFORMS_MATERIAL, &material_uniforms, sizeof(material_uniforms));

    r_mesh_locals_t blend_locals = locals;
    switch (material->cm->surface & SURF_MASK_BLEND) {
      case SURF_BLEND_33:
        blend_locals.color.w *= .333f;
        break;
      case SURF_BLEND_66:
        blend_locals.color.w *= .666f;
        break;
      default:
        break;
    }
    $(commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &blend_locals, sizeof(blend_locals));

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

    R_DrawMeshEntityMaterialStages(view, pass, commands, e, face, material);
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
  $(commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

  $(pass, bindPipeline, r_mesh_pipeline.pipeline);

  $(pass, bindFragmentSamplers, MESH_SAMPLER_VOXEL_LIGHT_DATA, &(SDL_GPUTextureSamplerBinding) {
    .texture = bsp->voxels.light_data->texture->texture,
    .sampler = r_mesh_pipeline.voxel_data_sampler->sampler,
  }, 1);

  // The point-light shadow atlas (comparison sampler), shared with the BSP pass.
  $(pass, bindFragmentSamplers, MESH_SAMPLER_SHADOW_ATLAS_0, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_shadow_atlas.textures[0]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[1]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[2]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[3]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[4]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[5]->texture, .sampler = r_shadow_atlas.sampler->sampler },
  }, 6);

  // The voxel caustics / occlusion volumes and the sky cubemap for ambient light.
  $(pass, bindFragmentSamplers, MESH_SAMPLER_VOXEL_CAUSTICS, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = bsp->voxels.caustics->texture->texture, .sampler = r_mesh_pipeline.ambient_sampler->sampler },
    { .texture = bsp->voxels.occlusion->texture->texture, .sampler = r_mesh_pipeline.ambient_sampler->sampler },
    { .texture = R_SkyTexture()->texture, .sampler = r_mesh_pipeline.ambient_sampler->sampler },
  }, 3);

  // Stage/stage-next: opaque and blend draws never sample these (mesh_fs's
  // STAGE_NONE branch doesn't touch them), but the shared shader declares
  // them, so SDL_gpu requires them bound regardless.
  $(pass, bindFragmentSamplers, MESH_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_context.null_texture->texture, .sampler = r_mesh_pipeline.stage_sampler->sampler },
    { .texture = r_context.null_texture->texture, .sampler = r_mesh_pipeline.stage_sampler->sampler },
  }, 2);

  SDL_GPUBuffer *storage[] = {
    r_lights.buffer->buffer,
    bsp->voxels.light_indices_buffer ? bsp->voxels.light_indices_buffer->buffer
                                     : r_lights.buffer->buffer,
  };
  $(pass, bindFragmentStorageBuffers, MESH_STORAGE_LIGHTS, storage, 2);

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
 * @brief Renders the player-model preview (menu player setup, etc.) into its own
 * framebuffer: mesh entities only, lit with a flat ambient since no world/voxel
 * data is loaded for this view. No BSP, sprites, decals, or shadows are drawn.
 */
void R_DrawPlayerModelView(r_view_t *view) {

  if (!r_mesh_pipeline.pipeline || !view->framebuffer) {
    return;
  }

  R_UpdateUniforms(view);
  R_UpdateEntities(view);

  CommandBuffer *commands = r_context.device->commands;
  if (!commands) {
    return;
  }

  Framebuffer *framebuffer = view->framebuffer;

  const SDL_FColor clear_color = { 0.f, 0.f, 0.f, 0.f };
  const SDL_FColor clear_depth_copy = { 1.f, 1.f, 1.f, 1.f };
  const SDL_GPUColorTargetInfo color[] = {
    $(framebuffer, colorTargetInfo, 0, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE, &clear_color),
    $(framebuffer, colorTargetInfo, 1, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE, &clear_depth_copy),
  };
  const SDL_GPUDepthStencilTargetInfo depth =
      $(framebuffer, depthTargetInfo, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE, 1.f);

  RenderPass *pass = $(commands, beginRenderPass, color, 2, &depth);

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  $(commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

  $(pass, bindPipeline, r_mesh_pipeline.pipeline);

  // No world is loaded for this view, so bind the 1x1x1 fallbacks; the shader's
  // VIEW_PLAYER_MODEL branch never actually samples them (see mesh_fs.glsl).
  $(pass, bindFragmentSamplers, MESH_SAMPLER_VOXEL_LIGHT_DATA, &(SDL_GPUTextureSamplerBinding) {
    .texture = r_mesh_pipeline.voxel_light_data_fallback->texture,
    .sampler = r_mesh_pipeline.voxel_data_sampler->sampler,
  }, 1);

  $(pass, bindFragmentSamplers, MESH_SAMPLER_SHADOW_ATLAS_0, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_shadow_atlas.textures[0]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[1]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[2]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[3]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[4]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[5]->texture, .sampler = r_shadow_atlas.sampler->sampler },
  }, 6);

  $(pass, bindFragmentSamplers, MESH_SAMPLER_VOXEL_CAUSTICS, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_mesh_pipeline.voxel_caustics_fallback->texture, .sampler = r_mesh_pipeline.ambient_sampler->sampler },
    { .texture = r_mesh_pipeline.voxel_occlusion_fallback->texture, .sampler = r_mesh_pipeline.ambient_sampler->sampler },
    { .texture = R_SkyTexture()->texture, .sampler = r_mesh_pipeline.ambient_sampler->sampler },
  }, 3);

  // Stage/stage-next: see R_DrawMeshEntities.
  $(pass, bindFragmentSamplers, MESH_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_context.null_texture->texture, .sampler = r_mesh_pipeline.stage_sampler->sampler },
    { .texture = r_context.null_texture->texture, .sampler = r_mesh_pipeline.stage_sampler->sampler },
  }, 2);

  SDL_GPUBuffer *storage[] = { r_lights.buffer->buffer, r_lights.buffer->buffer };
  $(pass, bindFragmentStorageBuffers, MESH_STORAGE_LIGHTS, storage, 2);

  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_MESH_MODEL(e->model)) {
      continue;
    }

    if (e->effects & EF_NO_DRAW) {
      continue;
    }

    // No culling: R_CullEntity always returns false for VIEW_PLAYER_MODEL.
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
    .num_uniform_buffers = MESH_NUM_UNIFORMS, // globals + locals + material(+stage)
  });

  Shader *fragmentShader = $(r_context.device, loadShader, "shaders/mesh_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = MESH_NUM_SAMPLERS,               // material, voxel_light_data, shadow_atlas,
                                                      // voxel_caustics, voxel_occlusion, sky, stage, stage_next
    .num_storage_buffers = MESH_NUM_STORAGE_BUFFERS, // lights + voxel_light_indices
    .num_uniform_buffers = MESH_NUM_UNIFORMS, // globals + active_lights + material(+stage+tints)
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
  SDL_GPUColorTargetDescription color_targets[] = {
    { .format = R_SCENE_COLOR_FORMAT, .blend_state = GPU_BlendStateOpaque },
    { .format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT, .blend_state = GPU_BlendStateOpaque },
  };

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = color_targets,
    .num_color_targets = 2,
    .depth_stencil_format = framebuffer->depthFormat,
    .has_depth_stencil_target = true,
  };

  r_mesh_pipeline.pipeline = $(r_context.device, createGraphicsPipeline, &info);

  // Translucent faces: no culling (matching the GL renderer, which disables
  // GL_CULL_FACE for mesh blend faces), alpha blend on color 0. Depth test and
  // write stay enabled -- unlike BSP blend surfaces, GL does not disable
  // glDepthMask for mesh blend faces -- so color 1 (the depth copy) is left
  // unmasked too, consistent with real depth being written.
  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  color_targets[0].blend_state = GPU_BlendStateAlpha;

  r_mesh_pipeline.blend_pipeline = $(r_context.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);

  r_mesh_pipeline.diffusemap_sampler = $(r_context.device, createSamplerLinearRepeat);
  r_mesh_pipeline.ambient_sampler = $(r_context.device, createSamplerLinearClamp);
  r_mesh_pipeline.stage_sampler = $(r_context.device, createSamplerLinearRepeat);
  r_mesh_pipeline.voxel_data_sampler = $(r_context.device, createSamplerNearestClamp);

  // 1x1x1 fallback voxel textures for the player-model preview (see the struct
  // field docs above). Contents are never sampled, so uninitialized (NULL) data
  // is fine -- the branch that would read them never executes for that view.
  r_mesh_pipeline.voxel_light_data_fallback = $(r_context.device, createTexture, &(SDL_GPUTextureCreateInfo) {
    .type = SDL_GPU_TEXTURETYPE_3D,
    .format = SDL_GPU_TEXTUREFORMAT_R32G32_INT,
    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
    .width = 1, .height = 1, .layer_count_or_depth = 1,
    .num_levels = 1,
    .sample_count = SDL_GPU_SAMPLECOUNT_1,
  }, NULL);

  r_mesh_pipeline.voxel_caustics_fallback = $(r_context.device, createTexture, &(SDL_GPUTextureCreateInfo) {
    .type = SDL_GPU_TEXTURETYPE_3D,
    .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
    .width = 1, .height = 1, .layer_count_or_depth = 1,
    .num_levels = 1,
    .sample_count = SDL_GPU_SAMPLECOUNT_1,
  }, NULL);

  r_mesh_pipeline.voxel_occlusion_fallback = $(r_context.device, createTexture, &(SDL_GPUTextureCreateInfo) {
    .type = SDL_GPU_TEXTURETYPE_3D,
    .format = SDL_GPU_TEXTUREFORMAT_R8G8_UNORM,
    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
    .width = 1, .height = 1, .layer_count_or_depth = 1,
    .num_levels = 1,
    .sample_count = SDL_GPU_SAMPLECOUNT_1,
  }, NULL);
}

/**
 * @brief Releases the mesh pipeline and samplers.
 */
void R_ShutdownMeshPipeline(void) {
  r_mesh_pipeline.pipeline = release(r_mesh_pipeline.pipeline);
  r_mesh_pipeline.blend_pipeline = release(r_mesh_pipeline.blend_pipeline);
  r_mesh_pipeline.diffusemap_sampler = release(r_mesh_pipeline.diffusemap_sampler);
  r_mesh_pipeline.voxel_data_sampler = release(r_mesh_pipeline.voxel_data_sampler);
  r_mesh_pipeline.ambient_sampler = release(r_mesh_pipeline.ambient_sampler);
  r_mesh_pipeline.stage_sampler = release(r_mesh_pipeline.stage_sampler);
  r_mesh_pipeline.voxel_light_data_fallback = release(r_mesh_pipeline.voxel_light_data_fallback);
  r_mesh_pipeline.voxel_caustics_fallback = release(r_mesh_pipeline.voxel_caustics_fallback);
  r_mesh_pipeline.voxel_occlusion_fallback = release(r_mesh_pipeline.voxel_occlusion_fallback);

  for (int32_t i = 0; i < r_mesh_pipeline.num_stages; i++) {
    r_mesh_pipeline.stages[i].pipeline = release(r_mesh_pipeline.stages[i].pipeline);
  }
  r_mesh_pipeline.num_stages = 0;

  // r_mesh_pipeline.shell is a media object owned by the media cache; not released here.
}

/**
 * @brief Rebuilds the mesh pipeline and samplers in place, for pipeline-bound
 * cvar changes (r_antialias, r_anisotropy, ...) that would otherwise require
 * an r_restart. See R_UpdatePipelines.
 */
void R_UpdateMeshPipeline(void) {
  R_ShutdownMeshPipeline();
  R_InitMeshPipeline();
}
