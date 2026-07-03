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
    .num_samplers = 3,         // texture_material + texture_voxel_light_data + texture_shadow_atlas
    .num_storage_buffers = 2,  // lights_block + voxel_light_indices_block (read-only storage)
    .num_uniform_buffers = 3,  // globals + per-block active_lights + per-draw material
  });

  const Framebuffer *framebuffer = r_context.device->framebuffer;

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

  // A named color target so the blend state can be varied per pipeline variant.
  SDL_GPUColorTargetDescription color_target = {
    .format = framebuffer->colorFormats[0],
    .blend_state = GPU_BlendStateOpaque,
  };

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = &color_target,
    .num_color_targets = 1,
    .depth_stencil_format = framebuffer->depthFormat,
    .has_depth_stencil_target = true,
  };

  r_bsp_pipeline.pipeline = $(r_context.device, createGraphicsPipeline, &info);

  // Translucent variant: alpha-blend over the opaque scene and depth-test but do
  // not write depth, for SURF_MASK_BLEND surfaces (water, glass, ...).
  color_target.blend_state = GPU_BlendStateAlpha;
  info.depth_stencil_state.enable_depth_write = false;
  r_bsp_pipeline.blend_pipeline = $(r_context.device, createGraphicsPipeline, &info);

  // A line-fill variant for r_draw_wireframe (bsp_fs shades wireframe fragments
  // flat grey). Best-effort: mappers use it to inspect quemap's brush cuts, so
  // the wide-platform caveat around line fill on Vulkan/Android is acceptable.
  color_target.blend_state = GPU_BlendStateOpaque;
  info.depth_stencil_state.enable_depth_write = true;
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
    .num_samplers = 5,        // material + voxel_light_data + shadow_atlas + stage + stage_next
    .num_storage_buffers = 2, // lights + voxel_light_indices
    .num_uniform_buffers = 4, // globals (0) + active_lights (1) + material (2) + stage (3)
  });

  const Framebuffer *framebuffer = r_context.device->framebuffer;

  const SDL_GPUBlendFactor s = R_BlendFactor(src);
  const SDL_GPUBlendFactor d = R_BlendFactor(dest);

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.vertex_shader = vertexShader->shader;
  info.fragment_shader = fragmentShader->shader;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

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

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = &(SDL_GPUColorTargetDescription) {
      .format = framebuffer->colorFormats[0],
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
    .num_color_targets = 1,
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

  // First scene pass: clear the color attachment. When the depth pre-pass ran,
  // LOAD its depth so occluded fragments are rejected before shading (early-Z);
  // otherwise clear depth and write it here. This makes r_depth_pass the A/B.
  const SDL_FColor clear_color = { 0.f, 0.f, 0.f, 1.f };
  const SDL_GPUColorTargetInfo color =
      $(framebuffer, colorTargetInfo, 0, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE, &clear_color);
  const SDL_GPUDepthStencilTargetInfo depth =
      $(framebuffer, depthTargetInfo, r_depth_pass->integer ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE, 1.f);

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
 * @brief Renders the translucent (SURF_MASK_BLEND) world surfaces over the opaque
 * scene, alpha-blended and depth-tested but without writing depth.
 * @remarks Mirrors R_DrawBspEntities but with the blend pipeline and the inverse
 * surface filter. TODO(#864): inline BSP model blend entities, back-to-front
 * sorting, and material stages on blended surfaces are deferred; single-layer
 * translucency (water, glass) is correct without sorting.
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

  pass = release(pass);
}
