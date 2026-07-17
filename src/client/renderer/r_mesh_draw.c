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
 * @brief The mesh pipeline and samplers.
 */
static struct {

  /**
   * @brief The mesh graphics pipeline.
   */
  GraphicsPipeline *pipeline;

  /**
   * @brief An alpha-test variant of `pipeline`, for @c SURF_ALPHA_TEST faces. See
   * r_bsp_draw.c's `pipeline_alpha_test` for why this needs its own pipeline
   * (a `discard`-carrying fragment shader defeats early-Z for every draw that
   * shares its pipeline, not just the ones that reach the `discard`).
   */
  GraphicsPipeline *pipeline_alpha_test;

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
} r_mesh_draw;

/**
 * @brief Per-entity mesh locals, pushed to vertex uniform slot 1.
 */
typedef struct {
  mat4_t model;
  float lerp;
  float padding[3]; // pads color to a 16-byte boundary; see mesh_vs.glsl's matching padding0/1/2
  vec4_t color;
  uint32_t active_dynamic_lights[MAX_DYNAMIC_LIGHTS / 32]; // for vertex_lighting, see light.glsl
} r_mesh_locals_t;

/**
 * @brief Per-entity fragment locals.
 */
typedef struct {
  uint32_t active_dynamic_lights[MAX_DYNAMIC_LIGHTS / 32];
} r_mesh_fragment_locals_t;

/**
 * @brief Returns the mesh pipeline for the given material-stage blend function,
 * creating and caching it on first use. Wraps the same mesh_vs/mesh_fs shader
 * as the opaque pipeline (a stage draw is a runtime branch, not a shader
 * swap); only the blend state differs, which SDL_gpu bakes into the pipeline.
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
    .num_storage_buffers = R_STORAGE_MATERIAL_TOTAL, // per-vertex lighting binds the same buffers
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

  $(pass, bindFragmentSamplers, R_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = texture, .sampler = r_mesh_draw.stage_sampler->sampler },
    { .texture = texture_next, .sampler = r_mesh_draw.stage_sampler->sampler },
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

  if (!r_mesh_draw.shell) {
    r_mesh_draw.shell = R_LoadImage("textures/envmaps/white", IMG_PROGRAM);
    if (!r_mesh_draw.shell) {
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
    .media = (r_media_t *) r_mesh_draw.shell,
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
 * @brief Pushes the per-face model/lerp/color/active_dynamic_lights locals and
 * binds the face's vertex buffer offsets. Shared by the base face draw and the
 * material-stage-only pass, both of which need this before drawing the same
 * face's geometry.
 * @param active_dynamic_lights The entity's dynamic-light cull bitmask,
 * precomputed once per entity by the caller (see R_DrawMeshEntity) -- shared
 * with the fragment stage's own locals push.
 */
static void R_BindMeshEntityFace(RenderPass *pass, CommandBuffer *commands, const r_entity_t *e,
                                 const r_mesh_model_t *mesh, const r_mesh_face_t *face, const r_material_t *material,
                                 const uint32_t active_dynamic_lights[MAX_DYNAMIC_LIGHTS / 32]) {

  // Scale entity alpha by the surface's blend amount (33/66/100%); a no-op
  // for opaque faces (SURF_MASK_BLEND unset falls through to the 1x default).
  r_mesh_locals_t locals = {
    .model = e->matrix,
    .lerp = e->lerp,
    .color = e->color,
  };
  memcpy(locals.active_dynamic_lights, active_dynamic_lights, sizeof(locals.active_dynamic_lights));
  switch (material->cm->surface & SURF_MASK_BLEND) {
    case SURF_BLEND_33:
      locals.color.w *= .333f;
      break;
    case SURF_BLEND_66:
      locals.color.w *= .666f;
      break;
    default:
      break;
  }
  $(commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &locals, sizeof(locals));

  // Two vertex buffer slots: the old frame (locations 0-4) and the current
  // frame (locations 5-8), the same model buffer bound at each frame offset.
  const uint32_t stride = sizeof(r_mesh_vertex_t);
  const uint32_t old_offset = (uint32_t) (face->base_vertex + e->old_frame * face->num_vertexes) * stride;
  const uint32_t cur_offset = (uint32_t) (face->base_vertex + e->frame * face->num_vertexes) * stride;

  $(pass, bindVertexBuffers, 0, (SDL_GPUBufferBinding[]) {
    { .buffer = mesh->vertex_buffer->buffer, .offset = old_offset },
    { .buffer = mesh->vertex_buffer->buffer, .offset = cur_offset },
  }, 2);
}

/**
 * @brief Binds material and vertex attributes, then draws elements for a
 * single mesh face. Called for both opaque and translucent faces; the caller
 * selects the pipeline (opaque, alpha-test, or blend) before calling.
 * @param draw_stages True to also draw this face's material stages inline
 * (the blend bucket); false to skip them (opaque/alpha-test buckets draw
 * their stages separately, see R_DrawMeshEntityFaceMaterialStages).
 * @param active_dynamic_lights The entity's precomputed dynamic-light cull
 * bitmask, forwarded to R_BindMeshEntityFace for vertex_lighting.
 */
static void R_DrawMeshEntityFace(const r_view_t *view, RenderPass *pass, CommandBuffer *commands,
                                 const r_entity_t *e, const r_mesh_model_t *mesh,
                                 const r_mesh_face_t *face, const r_material_t *material, bool draw_stages,
                                 const uint32_t active_dynamic_lights[MAX_DYNAMIC_LIGHTS / 32]) {

  $(pass, bindFragmentSamplers, R_SAMPLER_MATERIAL, &(SDL_GPUTextureSamplerBinding) {
    .texture = material->texture->texture->texture,
    .sampler = r_mesh_draw.diffusemap_sampler->sampler,
  }, 1);

  // R_MaterialUniforms resets the stage fields to STAGE_NONE: a preceding
  // face's material stages may have left them set, and this draw's shader
  // branches on material.flags (mesh_fs.glsl).
  r_mesh_material_uniforms_t material_uniforms;
  R_MaterialUniforms(material, material->cm->surface, &material_uniforms.material);
  memcpy(material_uniforms.tint_colors, e->tints, sizeof(material_uniforms.tint_colors));

  // Tint channels the entity does not set (alpha == 0) take the material's
  // default tint colors; without this, untinted tintmap texels shade to black.
  for (size_t i = 0; i < lengthof(material_uniforms.tint_colors); i++) {
    if (!e->tints[i].w) {
      material_uniforms.tint_colors[i] = material->cm->tintmap_defaults[i];
    }
  }
  $(commands, pushVertexUniformData, MESH_UNIFORMS_MATERIAL, &material_uniforms.material, sizeof(material_uniforms.material));
  $(commands, pushFragmentUniformData, MESH_UNIFORMS_MATERIAL, &material_uniforms, sizeof(material_uniforms));

  R_BindMeshEntityFace(pass, commands, e, mesh, face, material, active_dynamic_lights);

  const uint32_t firstIndex = (uint32_t) ((uintptr_t) face->indices / sizeof(uint32_t));

  $(pass, drawIndexedPrimitives, face->num_elements, 1, firstIndex, 0, 0);

  r_stats.mesh_draw_elements++;
  r_stats.mesh_triangles += face->num_elements / 3;

  if (draw_stages) {
    R_DrawMeshEntityMaterialStages(view, pass, commands, e, face, material);
  }
}

/**
 * @brief Draws a single face's material stages and EF_SHELL overlay on their
 * own, without a base draw. Used by the opaque/alpha-test buckets, which draw
 * their stages in a dedicated pass -- see R_DrawMeshEntity.
 */
static void R_DrawMeshEntityFaceMaterialStages(const r_view_t *view, RenderPass *pass, CommandBuffer *commands,
                                               const r_entity_t *e, const r_mesh_model_t *mesh,
                                               const r_mesh_face_t *face, const r_material_t *material,
                                               const uint32_t active_dynamic_lights[MAX_DYNAMIC_LIGHTS / 32]) {

  // Unlike R_DrawMeshEntityFace, this face has no base draw of its own to bind
  // texture_material, but a STAGE_LIGHTING (non-flat) stage or shell still
  // samples it (see mesh_fragment_lighting in mesh_fs.glsl). Without this, it
  // would read whatever face's material was last bound by the base-pass loops
  // in R_DrawMeshEntity -- a different, effectively random texture.
  $(pass, bindFragmentSamplers, R_SAMPLER_MATERIAL, &(SDL_GPUTextureSamplerBinding) {
    .texture = material->texture->texture->texture,
    .sampler = r_mesh_draw.diffusemap_sampler->sampler,
  }, 1);

  R_BindMeshEntityFace(pass, commands, e, mesh, face, material, active_dynamic_lights);

  R_DrawMeshEntityMaterialStages(view, pass, commands, e, face, material);
}

/**
 * @brief Sets up per-entity uniforms and draws all faces of a mesh entity:
 * opaque first, then translucent, matching the per-entity opaque-then-blend
 * draw order (not a global back-to-front sort).
 */
static void R_DrawMeshEntity(RenderPass *pass, const r_view_t *view, const r_entity_t *e) {

  const r_mesh_model_t *mesh = e->model->mesh;
  assert(mesh);

  if (!mesh->vertex_buffer) {
    return;
  }

  CommandBuffer *commands = r_context.device->commands;

  // Compress the view weapon into the front tenth of the depth range so that
  // it never clips into world geometry (glDepthRange(0.0, 0.1) under GL).
  if (e->effects & EF_WEAPON) {
    $(pass, setViewport, &(SDL_GPUViewport) {
      .x = 0.f, .y = 0.f,
      .w = (float) view->framebuffer->size.w, .h = (float) view->framebuffer->size.h,
      .min_depth = 0.f, .max_depth = .1f,
    });
  }

  r_mesh_fragment_locals_t fragment_locals = { 0 };
  R_ActiveDynamicLights(view, e->abs_model_bounds, fragment_locals.active_dynamic_lights);
  $(commands, pushFragmentUniformData, MESH_UNIFORMS_LOCALS, &fragment_locals, sizeof(fragment_locals));

  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) {
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

    $(pass, bindPipeline, r_mesh_draw.pipeline);

    R_DrawMeshEntityFace(view, pass, commands, e, mesh, face, material, false, fragment_locals.active_dynamic_lights);
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

    $(pass, bindPipeline, r_mesh_draw.pipeline_alpha_test);

    R_DrawMeshEntityFace(view, pass, commands, e, mesh, face, material, false, fragment_locals.active_dynamic_lights);
  }

  // Opaque and alpha-tested faces' material stages, in their own pass so
  // stage pipeline binds don't interleave with the base pipelines above.
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

      R_DrawMeshEntityFaceMaterialStages(view, pass, commands, e, mesh, face, material, fragment_locals.active_dynamic_lights);
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

    // Re-bind the blend pipeline: a preceding face's material stages may have
    // left a stage pipeline bound.
    $(pass, bindPipeline, r_mesh_draw.blend_pipeline);

    R_DrawMeshEntityFace(view, pass, commands, e, mesh, face, material, true, fragment_locals.active_dynamic_lights);
  }

  // Restore the full depth range for subsequent entities.
  if (e->effects & EF_WEAPON) {
    $(pass, setViewport, &(SDL_GPUViewport) {
      .x = 0.f, .y = 0.f,
      .w = (float) view->framebuffer->size.w, .h = (float) view->framebuffer->size.h,
      .min_depth = 0.f, .max_depth = 1.f,
    });
  }

  r_stats.mesh_models++;
}

/**
 * @brief Draws mesh entities.
 */
void R_DrawMeshEntities(RenderPass *pass, const r_view_t *view) {

  if (!r_models.world) {
    return;
  }

  CommandBuffer *commands = r_context.device->commands;

  const r_bsp_model_t *bsp = r_models.world->bsp;
  Framebuffer *framebuffer = view->framebuffer;

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  // Per-frame globals to both stages; lighting resources shared with the BSP pass.
  $(commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

  $(pass, bindPipeline, r_mesh_draw.pipeline);

  // The point-light shadow atlas (comparison sampler), shared with the BSP pass.
  $(pass, bindFragmentSamplers, R_SAMPLER_SHADOW_ATLAS_0, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_shadow_atlas.textures[0]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[1]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[2]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[3]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[4]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[5]->texture, .sampler = r_shadow_atlas.sampler->sampler },
  }, 6);

  // The voxel caustics / occlusion volumes and the sky cubemap for ambient light.
  $(pass, bindFragmentSamplers, R_SAMPLER_VOXEL_CAUSTICS, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = bsp->voxels.caustics->texture->texture, .sampler = r_mesh_draw.ambient_sampler->sampler },
    { .texture = bsp->voxels.occlusion->texture->texture, .sampler = r_mesh_draw.ambient_sampler->sampler },
    { .texture = R_SkyTexture()->texture, .sampler = r_mesh_draw.ambient_sampler->sampler },
  }, 3);

  $(pass, bindVertexSamplers, R_SAMPLER_MATERIAL, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_context.null_texture->texture, .sampler = r_mesh_draw.stage_sampler->sampler }, // material (unused)
    { .texture = r_shadow_atlas.textures[0]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[1]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[2]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[3]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[4]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[5]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = bsp->voxels.caustics->texture->texture, .sampler = r_mesh_draw.ambient_sampler->sampler },
    { .texture = bsp->voxels.occlusion->texture->texture, .sampler = r_mesh_draw.ambient_sampler->sampler },
    { .texture = R_SkyTexture()->texture, .sampler = r_mesh_draw.ambient_sampler->sampler },
    { .texture = r_context.null_texture->texture, .sampler = r_mesh_draw.stage_sampler->sampler }, // stage (unused)
    { .texture = r_context.null_texture->texture, .sampler = r_mesh_draw.stage_sampler->sampler }, // stage_next (unused)
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

    R_DrawMeshEntity(pass, view, e);
    r_stats.entities_visible++;
  }
}

/**
 * @brief Renders the player-model preview (menu player setup, etc.) into its own
 * framebuffer: mesh entities only, lit with a flat ambient since no world/voxel
 * data is loaded for this view. No BSP, sprites, decals, or shadows are drawn.
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

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  $(commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

  $(pass, bindPipeline, r_mesh_draw.pipeline);

  // No world is loaded for this view, so bind the 1x1x1 fallbacks; the shader's
  // VIEW_PLAYER_MODEL branch never actually samples them (see mesh_fs.glsl).
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
    { .texture = r_context.null_texture->texture, .sampler = r_mesh_draw.stage_sampler->sampler }, // material (unused)
    { .texture = r_shadow_atlas.textures[0]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[1]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[2]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[3]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[4]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[5]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_mesh_draw.voxel_caustics_fallback->texture, .sampler = r_mesh_draw.ambient_sampler->sampler },
    { .texture = r_mesh_draw.voxel_occlusion_fallback->texture, .sampler = r_mesh_draw.ambient_sampler->sampler },
    { .texture = R_SkyTexture()->texture, .sampler = r_mesh_draw.ambient_sampler->sampler },
    { .texture = r_context.null_texture->texture, .sampler = r_mesh_draw.stage_sampler->sampler }, // stage (unused)
    { .texture = r_context.null_texture->texture, .sampler = r_mesh_draw.stage_sampler->sampler }, // stage_next (unused)
  }, 12);

  // Stage/stage-next: see R_DrawMeshEntities.
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
    .num_samplers = MESH_NUM_VERTEX_SAMPLERS,
    .num_storage_buffers = R_STORAGE_MATERIAL_TOTAL, // per-vertex lighting binds the same buffers
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
 * @brief Releases the mesh pipeline and samplers.
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

  // r_mesh_pipeline.shell is a media object owned by the media cache; not released here.
}

/**
 * @brief Rebuilds the mesh pipeline and samplers in place, for pipeline-bound
 * cvar changes (@c r_antialias, @c r_anisotropy, ...) that would otherwise require
 * an @c r_restart. See @c R_UpdatePipelines.
 */
void R_UpdateMeshPipeline(void) {
  R_ShutdownMeshPipeline();
  R_InitMeshPipeline();
}
