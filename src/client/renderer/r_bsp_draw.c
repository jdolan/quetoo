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
#define MAX_STAGE_PIPELINES 16

/**
 * @brief The BSP program: its graphics pipeline, samplers, and the lazily-built
 * cache of material-stage blend pipelines.
 */
static struct {

  /**
   * @brief The BSP graphics pipeline.
   */
  GraphicsPipeline *pipeline;

  /**
   * @brief A line-fill variant of the pipeline, bound when r_draw_wireframe is
   * set (SDL_gpu bakes fill mode into the pipeline, so it needs its own object).
   */
  GraphicsPipeline *wireframe_pipeline;

  /**
   * @brief An alpha-blend variant (depth test, no depth write) for translucent
   * SURF_MASK_BLEND surfaces.
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
   * @brief The material stage sampler (linear, repeat) for stage textures.
   */
  Sampler *stage_sampler;

  /**
   * @brief A linear, clamped sampler shared by the voxel caustics/occlusion
   * volumes and the sky cubemap (all sampled with normalized coordinates).
   */
  Sampler *ambient_sampler;

  /**
   * @brief Cache of MATERIAL_STAGES pipelines, one per unique (src, dest) blend
   * function. SDL_gpu bakes blend state into the pipeline, so stage blends can't
   * be set dynamically; they're realized lazily here (only a handful of combos occur).
   */
  struct {
    cm_blend_t src, dest;
    GraphicsPipeline *pipeline;
  } stages[MAX_STAGE_PIPELINES];

  int32_t num_stages;
} r_bsp_pipeline;

/**
 * @brief Builds the BSP pipeline from the bsp_vs/bsp_fs shaders.
 */
void R_InitBspPipeline(void) {

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/bsp_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 2, // globals (binding 0) + locals/model (binding 1)
  });

  Shader *fragmentShader = $(r_context.device, loadShader, "shaders/bsp_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = 6,         // material + voxel_light_data + shadow_atlas + voxel_caustics + voxel_occlusion + sky
    .num_storage_buffers = 2,  // lights_block + voxel_light_indices_block (read-only storage)
    .num_uniform_buffers = 3,  // globals + per-block active_lights + per-draw material
  });

  const Framebuffer *framebuffer = r_context.device->framebuffer;

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = r_scene_samples;
  info.vertex_shader = vertexShader->shader;
  info.fragment_shader = fragmentShader->shader;

  // Cull back faces; front faces are wound clockwise (matching the GL renderer's
  // glFrontFace(GL_CW)). If world geometry vanishes, flip to COUNTER_CLOCKWISE.
  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;

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

  // Two color targets: the HDR scene color, and a single-sample float depth copy
  // (gl_FragCoord.z, written by bsp_fs) that the sprite pass samples for soft
  // particles. SDL_gpu cannot sample the real depth buffer (least of all a
  // multisample one), so opaque geometry writes this copy instead. Named locals so
  // the blend states can be varied per pipeline variant.
  SDL_GPUColorTargetDescription color_targets[2] = {
    { .format = R_SCENE_COLOR_FORMAT, .blend_state = GPU_BlendStateOpaque },
    { .format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT, .blend_state = GPU_BlendStateOpaque },
  };

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = color_targets,
    .num_color_targets = 2,
    .depth_stencil_format = framebuffer->depthFormat,
    .has_depth_stencil_target = true,
  };

  r_bsp_pipeline.pipeline = $(r_context.device, createGraphicsPipeline, &info);

  // Translucent variant: alpha-blend over the opaque scene and depth-test but do
  // not write depth, for SURF_MASK_BLEND surfaces (water, glass, ...). Unlike the
  // opaque bring-up pipeline (cull disabled), translucent faces MUST backface-cull
  // or a two-sided surface blends front and back and looks near-opaque; cull BACK
  // with a clockwise front face matches the GL path's glFrontFace(GL_CW).
  // TODO(#864): if translucent faces vanish, the winding is inverted -> flip to
  // SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE (and enable culling on the opaque pass too).
  // Mask the depth copy: a translucent surface must not overwrite the opaque depth
  // beneath it, or sprites would soften against glass rather than through it.
  color_targets[0].blend_state = GPU_BlendStateAlpha;
  color_targets[1].blend_state = (SDL_GPUColorTargetBlendState) {
    .enable_color_write_mask = true, .color_write_mask = 0,
  };
  info.depth_stencil_state.enable_depth_write = false;
  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;
  r_bsp_pipeline.blend_pipeline = $(r_context.device, createGraphicsPipeline, &info);

  // A line-fill variant for r_draw_wireframe (bsp_fs shades wireframe fragments
  // flat grey). Best-effort: mappers use it to inspect quemap's brush cuts, so
  // the wide-platform caveat around line fill on Vulkan/Android is acceptable.
  color_targets[0].blend_state = GPU_BlendStateOpaque;
  color_targets[1].blend_state = GPU_BlendStateOpaque;
  info.depth_stencil_state.enable_depth_write = true;
  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
  info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_LINE;
  r_bsp_pipeline.wireframe_pipeline = $(r_context.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);

  r_bsp_pipeline.diffusemap_sampler = $(r_context.device, createSampler, &(SDL_GPUSamplerCreateInfo) {
    .min_filter = SDL_GPU_FILTER_LINEAR,
    .mag_filter = SDL_GPU_FILTER_LINEAR,
    .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
    .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    .enable_anisotropy = true,
    .max_anisotropy = R_Anisotropy(),
  });

  // Nearest / clamp: the light-data texture is fetched with integer coords.
  r_bsp_pipeline.voxel_data_sampler = $(r_context.device, createSampler, &(SDL_GPUSamplerCreateInfo) {
    .min_filter = SDL_GPU_FILTER_NEAREST,
    .mag_filter = SDL_GPU_FILTER_NEAREST,
    .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
    .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
  });

  r_bsp_pipeline.stage_sampler = $(r_context.device, createSampler, &(SDL_GPUSamplerCreateInfo) {
    .min_filter = SDL_GPU_FILTER_LINEAR,
    .mag_filter = SDL_GPU_FILTER_LINEAR,
    .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
    .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    .enable_anisotropy = true,
    .max_anisotropy = R_Anisotropy(),
  });

  // Linear / clamp: the voxel volumes and sky cubemap are sampled continuously.
  r_bsp_pipeline.ambient_sampler = $(r_context.device, createSampler, &(SDL_GPUSamplerCreateInfo) {
    .min_filter = SDL_GPU_FILTER_LINEAR,
    .mag_filter = SDL_GPU_FILTER_LINEAR,
    .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
    .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
  });
}

/**
 * @brief Releases the BSP pipeline and samplers.
 */
void R_ShutdownBspPipeline(void) {
  r_bsp_pipeline.pipeline = release(r_bsp_pipeline.pipeline);
  r_bsp_pipeline.wireframe_pipeline = release(r_bsp_pipeline.wireframe_pipeline);
  r_bsp_pipeline.blend_pipeline = release(r_bsp_pipeline.blend_pipeline);
  r_bsp_pipeline.diffusemap_sampler = release(r_bsp_pipeline.diffusemap_sampler);
  r_bsp_pipeline.voxel_data_sampler = release(r_bsp_pipeline.voxel_data_sampler);
  r_bsp_pipeline.stage_sampler = release(r_bsp_pipeline.stage_sampler);
  r_bsp_pipeline.ambient_sampler = release(r_bsp_pipeline.ambient_sampler);

  for (int32_t i = 0; i < r_bsp_pipeline.num_stages; i++) {
    r_bsp_pipeline.stages[i].pipeline = release(r_bsp_pipeline.stages[i].pipeline);
  }
  r_bsp_pipeline.num_stages = 0;
}

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
 * @brief Returns the MATERIAL_STAGES bsp pipeline for the given stage blend
 * function, creating and caching it on first use.
 */
static GraphicsPipeline *R_StagePipeline(cm_blend_t src, cm_blend_t dest) {

  for (int32_t i = 0; i < r_bsp_pipeline.num_stages; i++) {
    if (r_bsp_pipeline.stages[i].src == src && r_bsp_pipeline.stages[i].dest == dest) {
      return r_bsp_pipeline.stages[i].pipeline;
    }
  }

  if (r_bsp_pipeline.num_stages == MAX_STAGE_PIPELINES) {
    return NULL;
  }

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/bsp_stage_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 3, // globals (0) + model (1) + stage (2)
  });

  Shader *fragmentShader = $(r_context.device, loadShader, "shaders/bsp_stage_fs", &(SDL_GPUShaderCreateInfo) {
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

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;

  // Stages blend over the already-lit base surface; test depth but do not write it.
  info.depth_stencil_state.enable_depth_write = false;

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = &(SDL_GPUVertexBufferDescription) {
      .slot = 0,
      .pitch = sizeof(r_bsp_vertex_t),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    },
    .num_vertex_buffers = 1,
    .vertex_attributes = (SDL_GPUVertexAttribute[]) {
      { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_bsp_vertex_t, position) },
      { .location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_bsp_vertex_t, normal) },
      { .location = 2, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_bsp_vertex_t, tangent) },
      { .location = 3, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_bsp_vertex_t, bitangent) },
      { .location = 4, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = offsetof(r_bsp_vertex_t, diffusemap) },
      { .location = 5, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, .offset = offsetof(r_bsp_vertex_t, color) },
    },
    .num_vertex_attributes = 6,
  };

  // Two color targets to match the opaque pass this runs in; the depth copy
  // (color 1) is masked, since a stage overlays the already-established surface.
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

  r_bsp_pipeline.stages[r_bsp_pipeline.num_stages] = (typeof(r_bsp_pipeline.stages[0])) {
    .src = src, .dest = dest, .pipeline = pipeline,
  };
  return r_bsp_pipeline.stages[r_bsp_pipeline.num_stages++].pipeline;
}

/**
 * @brief Binds the stage uniforms and textures and draws a single material stage of
 * a BSP draw-elements batch, layered over the already-lit base surface.
 */
static void R_DrawBspDrawElementsMaterialStage(const r_view_t *view, RenderPass *pass, CommandBuffer *commands,
                                               const r_bsp_draw_elements_t *draw, const r_stage_t *stage) {

  r_stage_uniforms_t uniforms;
  SDL_GPUTexture *texture, *texture_next;
  if (!R_StageUniforms(view, NULL, draw, stage, &uniforms, &texture, &texture_next)) {
    return;
  }

  GraphicsPipeline *pipeline = R_StagePipeline(stage->cm->blend.src, stage->cm->blend.dest);
  if (!pipeline) {
    return;
  }

  $(pass, bindPipeline, pipeline);

  $(pass, bindFragmentSamplers, SLOT_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = texture, .sampler = r_bsp_pipeline.stage_sampler->sampler },
    { .texture = texture_next, .sampler = r_bsp_pipeline.stage_sampler->sampler },
  }, 2);

  $(commands, pushVertexUniformData, SLOT_UNIFORMS_STAGE_VERTEX, &uniforms, sizeof(uniforms));
  $(commands, pushFragmentUniformData, SLOT_UNIFORMS_STAGE_FRAGMENT, &uniforms, sizeof(uniforms));

  const Uint32 firstIndex = (Uint32) ((uintptr_t) draw->elements / sizeof(uint32_t));
  $(pass, drawIndexedPrimitives, draw->num_elements, 1, firstIndex, 0, 0);

  r_stats.bsp_triangles += draw->num_elements / 3;
}

/**
 * @brief Draws all active material stages for a BSP draw-elements batch.
 * @remarks The base draw's bindings (material/voxel/shadow samplers, storage buffers,
 * and the globals/model/material/active-lights uniforms) remain current; each stage
 * only rebinds the stage pipeline, its textures, and the per-stage uniforms.
 */
static void R_DrawBspDrawElementsMaterialStages(const r_view_t *view, RenderPass *pass, CommandBuffer *commands,
                                                const r_bsp_draw_elements_t *draw) {

  const r_material_t *material = draw->material;
  if (!(material->cm->stage_flags & STAGE_DRAW)) {
    return;
  }

  for (const r_stage_t *stage = material->stages; stage; stage = stage->next) {

    if (!(stage->cm->flags & STAGE_DRAW)) {
      continue;
    }

    R_DrawBspDrawElementsMaterialStage(view, pass, commands, draw, stage);
  }
}

/**
 * @brief Draws the opaque (non-sky, non-blend) draw elements of one BSP block,
 * with the given base pipeline. The caller has already pushed the model matrix
 * and the active-lights bitmask for this block/entity.
 */
static void R_DrawBspBlockOpaque(const r_view_t *view, RenderPass *pass, CommandBuffer *commands,
                                 GraphicsPipeline *base, const r_bsp_block_t *block) {

  const r_bsp_draw_elements_t *draw = block->draw_elements;
  for (int32_t j = 0; j < block->num_draw_elements; j++, draw++) {

    if (draw->surface & (SURF_SKY | SURF_MASK_BLEND)) {
      continue;
    }

    if (!draw->material || !draw->material->texture || !draw->material->texture->texture) {
      continue;
    }

    // Re-bind the base pipeline: a preceding surface's material stages may have
    // left a stage (blend) pipeline bound.
    $(pass, bindPipeline, base);

    $(pass, bindFragmentSamplers, SLOT_SAMPLER_MATERIAL, &(SDL_GPUTextureSamplerBinding) {
      .texture = draw->material->texture->texture->texture,
      .sampler = r_bsp_pipeline.diffusemap_sampler->sampler,
    }, 1);

    r_material_uniforms_t material;
    R_MaterialUniforms(draw->material, draw->surface, &material);
    $(commands, pushFragmentUniformData, SLOT_UNIFORMS_MATERIAL, &material, sizeof(material));

    const Uint32 firstIndex = (Uint32) ((uintptr_t) draw->elements / sizeof(uint32_t));

    $(pass, drawIndexedPrimitives, draw->num_elements, 1, firstIndex, 0, 0);

    r_stats.bsp_triangles += draw->num_elements / 3;
    r_stats.bsp_draw_elements++;

    if (r_draw_material_stages->integer && !r_draw_wireframe->integer) {
      R_DrawBspDrawElementsMaterialStages(view, pass, commands, draw);
    }
  }
}

/**
 * @brief Renders the opaque world BSP surfaces with their material diffuse texture,
 * clustered per-voxel static lighting, and per-block dynamic lighting.
 * @remarks Iterates the worldspawn inline model's blocks (the unit of light
 * culling); per block, the active dynamic lights are culled to the block bounds.
 * TODO(#864): inline BSP model entities, blended surfaces, material stages.
 */
void R_DrawBspEntities(const r_view_t *view) {

  if (!r_models.world || !r_bsp_pipeline.pipeline) {
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

  // First scene pass: clear the color attachment and the depth copy (to the far
  // plane, 1.0). When the depth pre-pass ran, LOAD its depth so occluded fragments
  // are rejected before shading (early-Z); otherwise clear depth and write it here.
  // This makes r_depth_pass the A/B.
  const SDL_FColor clear_color = { 0.f, 0.f, 0.f, 1.f };
  const SDL_FColor clear_depth_copy = { 1.f, 1.f, 1.f, 1.f };
  const SDL_GPUColorTargetInfo color[] = {
    $(framebuffer, colorTargetInfo, 0, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE, &clear_color),
    $(framebuffer, colorTargetInfo, 1, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE, &clear_depth_copy),
  };
  const SDL_GPUDepthStencilTargetInfo depth =
      $(framebuffer, depthTargetInfo, r_depth_pass->integer ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE, 1.f);

  RenderPass *pass = $(commands, beginRenderPass, color, 2, &depth);

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

  // The wireframe variant replaces the base pipeline throughout the pass; stage
  // (blend) overlays are skipped below when it is active.
  GraphicsPipeline *base = r_draw_wireframe->integer
      ? r_bsp_pipeline.wireframe_pipeline : r_bsp_pipeline.pipeline;

  $(pass, bindPipeline, base);
  $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = bsp->vertex_buffer->buffer }, 1);
  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) { .buffer = bsp->elements_buffer->buffer }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  // The per-voxel light-data 3D texture (first_index, count).
  $(pass, bindFragmentSamplers, SLOT_SAMPLER_VOXEL_LIGHT_DATA, &(SDL_GPUTextureSamplerBinding) {
    .texture = bsp->voxels.light_data->texture->texture,
    .sampler = r_bsp_pipeline.voxel_data_sampler->sampler,
  }, 1);

  // The point-light shadow atlas (comparison sampler).
  $(pass, bindFragmentSamplers, SLOT_SAMPLER_SHADOW_ATLAS, &(SDL_GPUTextureSamplerBinding) {
    .texture = r_shadow_atlas.texture->texture,
    .sampler = r_shadow_atlas.sampler->sampler,
  }, 1);

  // The voxel caustics / occlusion volumes and the sky cubemap for ambient light.
  $(pass, bindFragmentSamplers, SLOT_SAMPLER_VOXEL_CAUSTICS, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = bsp->voxels.caustics->texture->texture, .sampler = r_bsp_pipeline.ambient_sampler->sampler },
    { .texture = bsp->voxels.occlusion->texture->texture, .sampler = r_bsp_pipeline.ambient_sampler->sampler },
    { .texture = R_SkyTexture()->texture, .sampler = r_bsp_pipeline.ambient_sampler->sampler },
  }, 3);

  // Storage: per-frame lights, then the voxel light index vector. Fall back to the
  // lights buffer for the index slot on maps with no light indices (the voxel
  // counts are then zero, so it is never read); SDL still requires a bind.
  SDL_GPUBuffer *storage[] = {
    r_lights.buffer->buffer,
    bsp->voxels.light_indices_buffer ? bsp->voxels.light_indices_buffer->buffer
                                     : r_lights.buffer->buffer,
  };
  $(pass, bindFragmentStorageBuffers, SLOT_STORAGE_LIGHTS, storage, 2);

  // Iterate the worldspawn inline model's blocks: the unit of light culling. Per
  // block, cull dynamic lights to its bounds, push the bitmask, and draw its
  // elements. TODO(#864): per-block occlusion cull once revisited (GPU-driven).
  const r_bsp_inline_model_t *world = bsp->inline_models;
  const r_bsp_block_t *block = world->blocks;
  for (int32_t i = 0; i < world->num_blocks; i++, block++) {

    uint32_t active_lights[4];
    R_ActiveLights(view, block->node->visible_bounds, active_lights);
    $(commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, active_lights, sizeof(active_lights));

    R_DrawBspBlockOpaque(view, pass, commands, base, block);
  }

  // Inline BSP model entities (doors, platforms, buttons, ...): the same pass and
  // resources, but each carries its own model matrix and is lit as a whole (its
  // dynamic-light set culled to the entity bounds rather than per block).
  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    // The worldspawn is itself a BSP inline model entity, but it is drawn above
    // via bsp->inline_models[0]; skip it here to avoid drawing it twice.
    if (IS_WORLDSPAWN(e->model)) {
      continue;
    }

    if (e->effects & EF_NO_DRAW) {
      continue;
    }

    if (R_CullEntity(view, e)) {
      r_stats.entities_occluded++;
      continue;
    }

    $(commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, e->matrix.array, sizeof(e->matrix));

    uint32_t active_lights[4];
    R_ActiveLights(view, e->abs_model_bounds, active_lights);
    $(commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, active_lights, sizeof(active_lights));

    const r_bsp_inline_model_t *in = e->model->bsp_inline;
    const r_bsp_block_t *b = in->blocks;
    for (int32_t j = 0; j < in->num_blocks; j++, b++) {
      R_DrawBspBlockOpaque(view, pass, commands, base, b);
    }

    r_stats.entities_visible++;
  }

  pass = release(pass);
}

/**
 * @brief Draws the translucent (SURF_MASK_BLEND) draw elements of a single
 * block, assuming its active-lights bitmask has already been pushed.
 */
static void R_DrawBspBlockBlend(RenderPass *pass, CommandBuffer *commands, const r_bsp_block_t *block) {

  const r_bsp_draw_elements_t *draw = block->draw_elements;
  for (int32_t j = 0; j < block->num_draw_elements; j++, draw++) {

    if (!(draw->surface & SURF_MASK_BLEND) || (draw->surface & SURF_SKY)) {
      continue;
    }

    if (!draw->material || !draw->material->texture || !draw->material->texture->texture) {
      continue;
    }

    $(pass, bindFragmentSamplers, SLOT_SAMPLER_MATERIAL, &(SDL_GPUTextureSamplerBinding) {
      .texture = draw->material->texture->texture->texture,
      .sampler = r_bsp_pipeline.diffusemap_sampler->sampler,
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

/**
 * @brief Renders the translucent (SURF_MASK_BLEND) world and inline BSP model
 * surfaces over the opaque scene, alpha-blended and depth-tested but without
 * writing depth.
 * @remarks Mirrors R_DrawBspEntities but with the blend pipeline and the inverse
 * surface filter. TODO(#864): back-to-front sorting and material stages on
 * blended surfaces are deferred; single-layer translucency (water, glass) is
 * correct without sorting.
 */
void R_DrawBlendBspEntities(const r_view_t *view) {

  if (!r_models.world || !r_bsp_pipeline.blend_pipeline) {
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

  // Composite over the opaque scene: LOAD color + depth, test against the opaque
  // depth (LEQUAL) but do not write it (the blend pipeline disables depth write).
  // Color 1 (depth copy) is attached to match the pipeline but masked off, so
  // translucent surfaces do not overwrite the opaque depth the sprites sample.
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

  const mat4_t model = Mat4_Identity();
  $(commands, pushVertexUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));
  $(commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, model.array, sizeof(model));
  $(commands, pushFragmentUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

  $(pass, bindPipeline, r_bsp_pipeline.blend_pipeline);
  $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = bsp->vertex_buffer->buffer }, 1);
  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) { .buffer = bsp->elements_buffer->buffer }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  $(pass, bindFragmentSamplers, SLOT_SAMPLER_VOXEL_LIGHT_DATA, &(SDL_GPUTextureSamplerBinding) {
    .texture = bsp->voxels.light_data->texture->texture,
    .sampler = r_bsp_pipeline.voxel_data_sampler->sampler,
  }, 1);

  $(pass, bindFragmentSamplers, SLOT_SAMPLER_SHADOW_ATLAS, &(SDL_GPUTextureSamplerBinding) {
    .texture = r_shadow_atlas.texture->texture,
    .sampler = r_shadow_atlas.sampler->sampler,
  }, 1);

  // The voxel caustics / occlusion volumes and the sky cubemap for ambient light.
  $(pass, bindFragmentSamplers, SLOT_SAMPLER_VOXEL_CAUSTICS, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = bsp->voxels.caustics->texture->texture, .sampler = r_bsp_pipeline.ambient_sampler->sampler },
    { .texture = bsp->voxels.occlusion->texture->texture, .sampler = r_bsp_pipeline.ambient_sampler->sampler },
    { .texture = R_SkyTexture()->texture, .sampler = r_bsp_pipeline.ambient_sampler->sampler },
  }, 3);

  SDL_GPUBuffer *storage[] = {
    r_lights.buffer->buffer,
    bsp->voxels.light_indices_buffer ? bsp->voxels.light_indices_buffer->buffer
                                     : r_lights.buffer->buffer,
  };
  $(pass, bindFragmentStorageBuffers, SLOT_STORAGE_LIGHTS, storage, 2);

  const r_bsp_inline_model_t *world = bsp->inline_models;
  const r_bsp_block_t *block = world->blocks;
  for (int32_t i = 0; i < world->num_blocks; i++, block++) {

    if (!(block->surface & SURF_MASK_BLEND)) {
      continue;
    }

    uint32_t active_lights[4];
    R_ActiveLights(view, block->node->visible_bounds, active_lights);
    $(commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, active_lights, sizeof(active_lights));

    R_DrawBspBlockBlend(pass, commands, block);
  }

  // Inline BSP model entities (doors, platforms, buttons, ...): the same pass and
  // resources, but each carries its own model matrix and is lit as a whole (its
  // dynamic-light set culled to the entity bounds rather than per block),
  // matching R_DrawBspEntities' opaque inline-entity loop.
  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    // The worldspawn is itself a BSP inline model entity, but it is drawn above
    // via bsp->inline_models[0]; skip it here to avoid drawing it twice.
    if (IS_WORLDSPAWN(e->model)) {
      continue;
    }

    if (e->effects & EF_NO_DRAW) {
      continue;
    }

    if (R_CullEntity(view, e)) {
      continue;
    }

    $(commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, e->matrix.array, sizeof(e->matrix));

    uint32_t active_lights[4];
    R_ActiveLights(view, e->abs_model_bounds, active_lights);
    $(commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, active_lights, sizeof(active_lights));

    const r_bsp_inline_model_t *in = e->model->bsp_inline;
    const r_bsp_block_t *b = in->blocks;
    for (int32_t j = 0; j < in->num_blocks; j++, b++) {

      if (!(b->surface & SURF_MASK_BLEND)) {
        continue;
      }

      R_DrawBspBlockBlend(pass, commands, b);
    }
  }

  pass = release(pass);
}
