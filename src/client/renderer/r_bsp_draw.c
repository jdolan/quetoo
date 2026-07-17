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
 * @brief BSP's extra sampler beyond the shared family (r_material.h): liquid
 * warp (bsp_fs.glsl only).
 */
enum {
  BSP_SAMPLER_WARP = R_SAMPLER_MATERIAL_TOTAL,
  BSP_NUM_SAMPLERS,
};

#define BSP_NUM_VERTEX_SAMPLERS R_SAMPLER_MATERIAL_TOTAL

enum {
  BSP_UNIFORMS_GLOBALS,
  BSP_UNIFORMS_LOCALS,
  BSP_UNIFORMS_MATERIAL,
  BSP_NUM_UNIFORMS
};

/**
 * @brief Per-draw vertex locals (@c locals_block in @c bsp_vs.glsl): the model
 * matrix and the dynamic light cull bitmask for per-vertex lighting.
 */
typedef struct {
  mat4_t model;
  uint32_t active_dynamic_lights[MAX_DYNAMIC_LIGHTS / 32];
} r_bsp_locals_t;

/**
 * @brief Cache of material stage pipelines, one per unique combination.
 */
typedef struct {
  cm_blend_t src, dest;
  bool depth_write;
  GraphicsPipeline *pipeline;
} r_bsp_material_stage_pipeline_t;

#define MAX_STAGE_PIPELINES 16

/**
 * @brief The BSP program (@c bsp_vs/ @c bsp_fs): its graphics pipeline, samplers, and
 * the lazily-built cache of material-stage blend pipelines. Draws BSP geometry
 * with the material diffuse texture and clustered per-voxel lighting, and its
 * material stages via the same shader (a runtime branch on stage.flags, not a
 * pipeline swap): see @c R_DrawBspDrawElementsMaterialStage.
 */
static struct {

  /**
   * @brief The BSP graphics pipeline.
   */
  GraphicsPipeline *pipeline;

  /**
   * @brief An alpha-test variant of `pipeline`, for @c SURF_ALPHA_TEST surfaces.
   * @details Compiled from a separate `bsp_fs_alpha_test` variant that keeps the
   * `discard` used for alpha testing; `pipeline` itself (and every material stage
   * pipeline, which never takes the alpha-tested branch) is compiled from a
   * `discard`-free `bsp_fs`, so the GPU can early-Z reject the vast majority of
   * opaque world geometry. A fragment shader containing `discard` anywhere in its
   * source forces late (post-shader) depth testing for every invocation of that
   * shader/pipeline -- even ones that never reach the `discard` -- so keeping the
   * rare alpha-tested surfaces on their own pipeline preserves early-Z for
   * everything else, restoring the intended benefit of the depth pre-pass and
   * occlusion queries.
   */
  GraphicsPipeline *pipeline_alpha_test;

  /**
   * @brief An alpha-blend variant (depth test, no depth write) for translucent @c SURF_MASK_BLEND surfaces.
   */
  GraphicsPipeline *blend_pipeline;

  /**
   * @brief The material sampler (trilinear, repeat).
   */
  Sampler *diffusemap_sampler;

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
   * @brief A small procedural noise texture, for STAGE_WARP liquid surfaces
   * (sampled with stage_sampler). Generated once at init, matching main.
   */
  Texture *warp_texture;

  /**
   * @brief The render pass and command buffer for the current draw call,
   * shared by the whole family of R_DrawBsp* functions below.
   */
  RenderPass *pass;
  CommandBuffer *commands;

  /**
   * @brief The currently bound material, to avoid unnecessary texture binds.
   */
  const r_material_t *material;
  int32_t surface;

  /**
   * @brief The cached material stage pipelines.
   */
  r_bsp_material_stage_pipeline_t stage_pipelines[MAX_STAGE_PIPELINES];
  int32_t num_stage_pipelines;
} r_bsp_draw;

/**
 * @brief Returns the bsp pipeline for the given material-stage blend function and
 * depth-write policy, creating and caching it on first use.
 */
static GraphicsPipeline *R_DrawBspMaterialStagePipeline(cm_blend_t src, cm_blend_t dest, bool depth_write) {

  const r_bsp_material_stage_pipeline_t *p = r_bsp_draw.stage_pipelines;
  for (int32_t i = 0; i < r_bsp_draw.num_stage_pipelines; i++, p++) {
    if (p->src == src && p->dest == dest && p->depth_write == depth_write) {
      return p->pipeline;
    }
  }

  if (r_bsp_draw.num_stage_pipelines == MAX_STAGE_PIPELINES) {
    Com_Error(ERROR_DROP, "MAX_STAGE_PIPELINES\n");
  }

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/bsp_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_samplers = BSP_NUM_VERTEX_SAMPLERS,
    .num_storage_buffers = R_STORAGE_MATERIAL_TOTAL, // per-vertex lighting binds the same buffers
    .num_uniform_buffers = BSP_NUM_UNIFORMS,
  });

  Shader *fragmentShader = $(r_context.device, loadShader, "shaders/bsp_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = BSP_NUM_SAMPLERS,
    .num_storage_buffers = R_STORAGE_MATERIAL_TOTAL,
    .num_uniform_buffers = BSP_NUM_UNIFORMS,
  });

  const SDL_GPUBlendFactor s = R_BlendFactor(src);
  const SDL_GPUBlendFactor d = R_BlendFactor(dest);

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = r_scene_samples;
  info.vertex_shader = vertexShader->shader;
  info.fragment_shader = fragmentShader->shader;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;

  info.depth_stencil_state.enable_depth_write = depth_write;

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

  r_bsp_material_stage_pipeline_t *out = &r_bsp_draw.stage_pipelines[r_bsp_draw.num_stage_pipelines++];

  out->src = src;
  out->dest = dest;
  out->depth_write = depth_write;
  out->pipeline = pipeline;

  return out->pipeline;
}

/**
 * @brief Draws a single material stage of a BSP draw-elements batch.
 * @param depth_write True to write depth (opaque bucket), false to skip it (blend bucket).
 */
static void R_DrawBspDrawElementsMaterialStage(const r_view_t *view, const r_entity_t *entity,
                                               const r_bsp_draw_elements_t *draw,
                                               const r_stage_t *stage, bool depth_write) {

  r_material_uniforms_t uniforms;
  R_MaterialUniforms(draw->material, draw->surface, &uniforms);

  SDL_GPUTexture *texture, *texture_next;
  if (!R_StageUniforms(view, entity, draw, stage, &uniforms, &texture, &texture_next)) {
    return;
  }

  GraphicsPipeline *pipeline = R_DrawBspMaterialStagePipeline(stage->cm->blend.src, stage->cm->blend.dest, depth_write);
  $(r_bsp_draw.pass, bindPipeline, pipeline);

  $(r_bsp_draw.pass, bindFragmentSamplers, R_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = texture, .sampler = r_bsp_draw.stage_sampler->sampler },
    { .texture = texture_next, .sampler = r_bsp_draw.stage_sampler->sampler },
  }, 2);

  $(r_bsp_draw.commands, pushUniformData, BSP_UNIFORMS_MATERIAL, &uniforms, sizeof(uniforms));

  const Uint32 firstIndex = (Uint32) ((uintptr_t) draw->elements / sizeof(uint32_t));
  $(r_bsp_draw.pass, drawIndexedPrimitives, draw->num_elements, 1, firstIndex, 0, 0);

  r_stats.bsp_triangles += draw->num_elements / 3;
}

/**
 * @brief Draws all active material stages for a BSP draw-elements batch.
 */
static void R_DrawBspDrawElementsMaterialStages(const r_view_t *view,
                                                const r_entity_t *entity,
                                                const r_bsp_draw_elements_t *draw,
                                                bool depth_write) {

  const r_material_t *material = draw->material;
  if (!(material->cm->stage_flags & STAGE_DRAW)) {
    return;
  }

  // Stages sample R_SAMPLER_MATERIAL too (e.g. lighting 1 stages read the
  // normalmap). The dedicated material pass runs after the base passes, so it
  // can't rely on their bind -- rebind here, cached like the base passes.
  if (draw->material != r_bsp_draw.material || draw->surface != r_bsp_draw.surface) {
    r_bsp_draw.material = draw->material;
    r_bsp_draw.surface = draw->surface;

    $(r_bsp_draw.pass, bindFragmentSamplers, R_SAMPLER_MATERIAL, &(SDL_GPUTextureSamplerBinding) {
      .texture = draw->material->texture->texture->texture,
      .sampler = r_bsp_draw.diffusemap_sampler->sampler,
    }, 1);
  }

  for (const r_stage_t *stage = material->stages; stage; stage = stage->next) {

    if (!(stage->cm->flags & STAGE_DRAW)) {
      continue;
    }

    R_DrawBspDrawElementsMaterialStage(view, entity, draw, stage, depth_write);
  }
}

/**
 * @brief Draws one BSP inline model entity's material stages (opaque and alpha-test buckets).
 */
static void R_DrawBspEntityMaterialStages(const r_view_t *view, const r_entity_t *entity) {

  r_bsp_locals_t locals = {
    .model = entity->matrix,
  };

  const r_bsp_inline_model_t *in = entity->model->bsp_inline;

  if (!IS_WORLDSPAWN(entity->model)) {
    R_ActiveDynamicLights(view, entity->abs_model_bounds, locals.active_dynamic_lights);
    $(r_bsp_draw.commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &locals, sizeof(locals));
    $(r_bsp_draw.commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, locals.active_dynamic_lights, sizeof(locals.active_dynamic_lights));
  }

  const r_bsp_block_t *block = in->blocks;
  for (int32_t i = 0; i < in->num_blocks; i++, block++) {

    if (IS_WORLDSPAWN(entity->model)) {

      if (block->query->result == 0) {
        continue;
      }

      memcpy(locals.active_dynamic_lights, block->active_dynamic_lights, sizeof(locals.active_dynamic_lights));
      $(r_bsp_draw.commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &locals, sizeof(locals));
      $(r_bsp_draw.commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, locals.active_dynamic_lights, sizeof(locals.active_dynamic_lights));
    }

    const r_bsp_draw_elements_t *draw = block->draw_elements;
    for (int32_t j = 0; j < block->num_draw_elements; j++, draw++) {

      if (draw->surface & (SURF_SKY | SURF_MASK_BLEND)) {
        continue;
      }

      R_DrawBspDrawElementsMaterialStages(view, entity, draw, true);
    }
  }
}

/**
 * @brief Draws the opaque draw elements of one BSP block.
 */
static void R_DrawOpaqueBspBlock(const r_bsp_block_t *block) {

  const r_bsp_draw_elements_t *draw = block->draw_elements;
  for (int32_t j = 0; j < block->num_draw_elements; j++, draw++) {

    if (draw->surface & (SURF_SKY | SURF_MASK_BLEND | SURF_ALPHA_TEST)) {
      continue;
    }

    if (draw->material != r_bsp_draw.material || draw->surface != r_bsp_draw.surface) {
      r_bsp_draw.material = draw->material;
      r_bsp_draw.surface = draw->surface;

      $(r_bsp_draw.pass, bindPipeline, r_bsp_draw.pipeline);

      $(r_bsp_draw.pass, bindFragmentSamplers, R_SAMPLER_MATERIAL, &(SDL_GPUTextureSamplerBinding) {
        .texture = draw->material->texture->texture->texture,
        .sampler = r_bsp_draw.diffusemap_sampler->sampler,
      }, 1);

      r_material_uniforms_t material;
      R_MaterialUniforms(draw->material, draw->surface, &material);
      $(r_bsp_draw.commands, pushUniformData, BSP_UNIFORMS_MATERIAL, &material, sizeof(material));
    }

    const Uint32 first_index = (Uint32) ((uintptr_t) draw->elements / sizeof(uint32_t));

    if (!(draw->surface & SURF_MATERIAL)) {
      $(r_bsp_draw.pass, drawIndexedPrimitives, draw->num_elements, 1, first_index, 0, 0);
      r_stats.bsp_triangles += draw->num_elements / 3;
    }

    r_stats.bsp_draw_elements++;
  }
}

/**
 * @brief Draws the alpha-tested draw elements of one BSP block.
 */
static void R_DrawAlphaTestBspBlock(const r_bsp_block_t *block) {

  const r_bsp_draw_elements_t *draw = block->draw_elements;
  for (int32_t j = 0; j < block->num_draw_elements; j++, draw++) {

    if (!(draw->surface & SURF_ALPHA_TEST) || (draw->surface & SURF_MASK_BLEND)) {
      continue;
    }

    if (draw->material != r_bsp_draw.material || draw->surface != r_bsp_draw.surface) {
      r_bsp_draw.material = draw->material;
      r_bsp_draw.surface = draw->surface;

      $(r_bsp_draw.pass, bindFragmentSamplers, R_SAMPLER_MATERIAL, &(SDL_GPUTextureSamplerBinding) {
        .texture = draw->material->texture->texture->texture,
        .sampler = r_bsp_draw.diffusemap_sampler->sampler,
      }, 1);

      r_material_uniforms_t material;
      R_MaterialUniforms(draw->material, draw->surface, &material);
      $(r_bsp_draw.commands, pushUniformData, BSP_UNIFORMS_MATERIAL, &material, sizeof(material));
    }

    const Uint32 first_index = (Uint32) ((uintptr_t) draw->elements / sizeof(uint32_t));

    if (!(draw->surface & SURF_MATERIAL)) {
      $(r_bsp_draw.pass, drawIndexedPrimitives, draw->num_elements, 1, first_index, 0, 0);
      r_stats.bsp_triangles += draw->num_elements / 3;
    }

    r_stats.bsp_draw_elements++;
  }
}

/**
 * @brief Draws opaque geometry for the given BSP inline model entity.
 */
static void R_DrawOpaqueBspEntity(const r_view_t *view, const r_entity_t *entity) {

  r_bsp_locals_t locals = {
    .model = entity->matrix,
  };

  const r_bsp_inline_model_t *in = entity->model->bsp_inline;

  if (!IS_WORLDSPAWN(entity->model)) {
    R_ActiveDynamicLights(view, entity->abs_model_bounds, locals.active_dynamic_lights);
    $(r_bsp_draw.commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &locals, sizeof(locals));
    $(r_bsp_draw.commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, locals.active_dynamic_lights, sizeof(locals.active_dynamic_lights));
  }

  const r_bsp_block_t *block = in->blocks;
  for (int32_t i = 0; i < in->num_blocks; i++, block++) {

    if (IS_WORLDSPAWN(entity->model)) {

      if (block->query->result == 0) {
        r_stats.blocks_occluded++;
        continue;
      }

      r_stats.blocks_visible++;

      memcpy(locals.active_dynamic_lights, block->active_dynamic_lights, sizeof(locals.active_dynamic_lights));
      $(r_bsp_draw.commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &locals, sizeof(locals));
      $(r_bsp_draw.commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, locals.active_dynamic_lights, sizeof(locals.active_dynamic_lights));
    }

    R_DrawOpaqueBspBlock(block);
  }

  r_stats.bsp_inline_models++;
}

/**
 * @brief Draws alpha test geometry for the given BSP inline model entity.
 */
static void R_DrawAlphaTestBspEntity(const r_view_t *view, const r_entity_t *entity) {

  r_bsp_locals_t locals = {
    .model = entity->matrix,
  };

  const r_bsp_inline_model_t *in = entity->model->bsp_inline;

  if (!IS_WORLDSPAWN(entity->model)) {
    R_ActiveDynamicLights(view, entity->abs_model_bounds, locals.active_dynamic_lights);
    $(r_bsp_draw.commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &locals, sizeof(locals));
    $(r_bsp_draw.commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, locals.active_dynamic_lights, sizeof(locals.active_dynamic_lights));
  }

  const r_bsp_block_t *block = in->blocks;
  for (int32_t i = 0; i < in->num_blocks; i++, block++) {

    if (IS_WORLDSPAWN(entity->model)) {

      if (block->query->result == 0) {
        continue;
      }

      memcpy(locals.active_dynamic_lights, block->active_dynamic_lights, sizeof(locals.active_dynamic_lights));
      $(r_bsp_draw.commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &locals, sizeof(locals));
      $(r_bsp_draw.commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, locals.active_dynamic_lights, sizeof(locals.active_dynamic_lights));
    }

    R_DrawAlphaTestBspBlock(block);
  }
}


/**
 * @brief Renders the opaque world BSP surfaces with their material diffuse texture,
 * clustered per-voxel static lighting, and per-block dynamic lighting.
 */
void R_DrawOpaqueBspEntities(RenderPass *pass, const r_view_t *view) {

  if (!r_models.world) {
    return;
  }

  CommandBuffer *commands = r_context.device->commands;

  const r_bsp_model_t *bsp = r_models.world->bsp;
  Framebuffer *framebuffer = view->framebuffer;

  r_bsp_draw.pass = pass;
  r_bsp_draw.commands = commands;

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  $(commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

  r_bsp_draw.material = NULL;

  $(pass, bindPipeline, r_bsp_draw.pipeline);
  $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = bsp->vertex_buffer->buffer }, 1);
  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) { .buffer = bsp->elements_buffer->buffer }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  $(pass, bindVertexSamplers, R_SAMPLER_MATERIAL, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_context.null_texture->texture, .sampler = r_bsp_draw.stage_sampler->sampler }, // material (unused)
    { .texture = r_shadow_atlas.textures[0]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[1]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[2]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[3]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[4]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[5]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = bsp->voxels.caustics->texture->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
    { .texture = bsp->voxels.occlusion->texture->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
    { .texture = R_SkyTexture()->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
    { .texture = r_context.null_texture->texture, .sampler = r_bsp_draw.stage_sampler->sampler }, // stage (unused)
    { .texture = r_context.null_texture->texture, .sampler = r_bsp_draw.stage_sampler->sampler }, // stage_next (unused)
  }, 12);

  $(pass, bindFragmentSamplers, R_SAMPLER_SHADOW_ATLAS_0, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_shadow_atlas.textures[0]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[1]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[2]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[3]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[4]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[5]->texture, .sampler = r_shadow_atlas.sampler->sampler },
  }, 6);

  $(pass, bindFragmentSamplers, R_SAMPLER_VOXEL_CAUSTICS, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = bsp->voxels.caustics->texture->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
    { .texture = bsp->voxels.occlusion->texture->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
    { .texture = R_SkyTexture()->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
  }, 3);

  $(pass, bindFragmentSamplers, R_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_context.null_texture->texture, .sampler = r_bsp_draw.stage_sampler->sampler },
    { .texture = r_context.null_texture->texture, .sampler = r_bsp_draw.stage_sampler->sampler },
  }, 2);

  $(pass, bindFragmentSamplers, BSP_SAMPLER_WARP, &(SDL_GPUTextureSamplerBinding) {
    .texture = r_bsp_draw.warp_texture->texture,
    .sampler = r_bsp_draw.stage_sampler->sampler,
  }, 1);

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

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    if (e->effects & EF_NO_DRAW) {
      continue;
    }

    if (!IS_WORLDSPAWN(e->model) && R_CullEntity(view, e)) {
      r_stats.entities_occluded++;
      continue;
    }

    R_DrawOpaqueBspEntity(view, e);
  }

  // Alpha-tested surfaces are their own pass so pipeline_alpha_test is bound
  // once here instead of interleaving with the discard-free opaque pipeline.
  r_bsp_draw.material = NULL;

  $(pass, bindPipeline, r_bsp_draw.pipeline_alpha_test);

  e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    if (e->effects & EF_NO_DRAW) {
      continue;
    }

    if (!IS_WORLDSPAWN(e->model) && R_CullEntity(view, e)) {
      continue;
    }

    R_DrawAlphaTestBspEntity(view, e);
  }

  // Material stages are their own pass too, so their pipeline binds don't
  // interleave with the opaque/alpha-test base pipelines above. Must still
  // run before the blend pass -- see R_DrawBspBlockMaterial.
  if (r_draw_material_stages->integer) {

    r_bsp_draw.material = NULL;

    e = view->entities;
    for (int32_t i = 0; i < view->num_entities; i++, e++) {

      if (!IS_BSP_INLINE_MODEL(e->model)) {
        continue;
      }

      if (e->effects & EF_NO_DRAW) {
        continue;
      }

      if (!IS_WORLDSPAWN(e->model) && R_CullEntity(view, e)) {
        continue;
      }

      R_DrawBspEntityMaterialStages(view, e);
    }
  }

  r_bsp_draw.pass = NULL;
}

/**
 * @brief Draws the translucent (@c SURF_MASK_BLEND) draw elements of a single
 * block, assuming its active-lights bitmask has already been pushed.
 */
static void R_DrawBlendBspBlock(const r_view_t *view, const r_entity_t *entity, const r_bsp_block_t *block) {

  const r_bsp_draw_elements_t *draw = block->draw_elements;
  for (int32_t j = 0; j < block->num_draw_elements; j++, draw++) {

    if (!(draw->surface & SURF_MASK_BLEND) || (draw->surface & SURF_SKY)) {
      continue;
    }

    if (draw->material != r_bsp_draw.material || draw->surface != r_bsp_draw.surface) {
      r_bsp_draw.material = draw->material;
      r_bsp_draw.surface = draw->surface;

      $(r_bsp_draw.pass, bindPipeline, r_bsp_draw.blend_pipeline);

      $(r_bsp_draw.pass, bindFragmentSamplers, R_SAMPLER_MATERIAL, &(SDL_GPUTextureSamplerBinding) {
        .texture = draw->material->texture->texture->texture,
        .sampler = r_bsp_draw.diffusemap_sampler->sampler,
      }, 1);

      r_material_uniforms_t material;
      R_MaterialUniforms(draw->material, draw->surface, &material);
      $(r_bsp_draw.commands, pushUniformData, BSP_UNIFORMS_MATERIAL, &material, sizeof(material));
    }

    const Uint32 first_index = (Uint32) ((uintptr_t) draw->elements / sizeof(uint32_t));
    $(r_bsp_draw.pass, drawIndexedPrimitives, draw->num_elements, 1, first_index, 0, 0);

    r_stats.bsp_triangles += draw->num_elements / 3;
    r_stats.bsp_draw_elements++;

    if (r_draw_material_stages->integer) {
      R_DrawBspDrawElementsMaterialStages(view, entity, draw, false);

      // A stage left a stage pipeline bound; force the next draw element to
      // rebind blend_pipeline even if it shares this material/surface.
      r_bsp_draw.material = NULL;
    }
  }
}

/**
 * @brief Draws one BSP inline model entity's translucent surfaces, including the world itself.
 */
static void R_DrawBlendBspEntity(const r_view_t *view, const r_entity_t *entity) {

  r_bsp_locals_t locals = {
    .model = entity->matrix,
  };

  const r_bsp_inline_model_t *in = entity->model->bsp_inline;

  if (!IS_WORLDSPAWN(entity->model)) {
    R_ActiveDynamicLights(view, entity->abs_model_bounds, locals.active_dynamic_lights);
    $(r_bsp_draw.commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &locals, sizeof(locals));
    $(r_bsp_draw.commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, locals.active_dynamic_lights, sizeof(locals.active_dynamic_lights));
  }

  const r_bsp_block_t *block = in->blocks;
  for (int32_t i = 0; i < in->num_blocks; i++, block++) {

    if (!(block->surface & SURF_MASK_BLEND)) {
      continue;
    }

    if (IS_WORLDSPAWN(entity->model)) {

      if (block->query->result == 0) {
        continue;
      }

      memcpy(locals.active_dynamic_lights, block->active_dynamic_lights, sizeof(locals.active_dynamic_lights));
      $(r_bsp_draw.commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &locals, sizeof(locals));
      $(r_bsp_draw.commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, locals.active_dynamic_lights, sizeof(locals.active_dynamic_lights));
    }

    R_DrawBlendBspBlock(view, entity, block);
  }
}

/**
 * @brief Renders the translucent (@c SURF_MASK_BLEND) world and inline BSP model
 * surfaces over the opaque scene, alpha-blended and depth-tested but without
 * writing depth.
 * @remarks Mirrors @c R_DrawOpaqueBspEntities but with the blend pipeline and the inverse
 * surface filter, including its material stages (animated water/glass overlays).
 * No back-to-front sorting, matching main -- single-layer translucency (water,
 * glass) is correct without it.
 */
void R_DrawBlendBspEntities(RenderPass *pass, const r_view_t *view) {

  if (!r_models.world) {
    return;
  }

  CommandBuffer *commands = r_context.device->commands;

  if (!view->framebuffer) {
    return;
  }

  const r_bsp_model_t *bsp = r_models.world->bsp;
  Framebuffer *framebuffer = view->framebuffer;

  r_bsp_draw.pass = pass;
  r_bsp_draw.commands = commands;

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  $(commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

  r_bsp_draw.material = NULL;

  $(pass, bindPipeline, r_bsp_draw.blend_pipeline);
  $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = bsp->vertex_buffer->buffer }, 1);
  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) { .buffer = bsp->elements_buffer->buffer }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  $(pass, bindVertexSamplers, R_SAMPLER_MATERIAL, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_context.null_texture->texture, .sampler = r_bsp_draw.stage_sampler->sampler }, // material (unused)
    { .texture = r_shadow_atlas.textures[0]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[1]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[2]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[3]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[4]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[5]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = bsp->voxels.caustics->texture->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
    { .texture = bsp->voxels.occlusion->texture->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
    { .texture = R_SkyTexture()->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
    { .texture = r_context.null_texture->texture, .sampler = r_bsp_draw.stage_sampler->sampler }, // stage (unused)
    { .texture = r_context.null_texture->texture, .sampler = r_bsp_draw.stage_sampler->sampler }, // stage_next (unused)
  }, 12);

  $(pass, bindFragmentSamplers, R_SAMPLER_SHADOW_ATLAS_0, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_shadow_atlas.textures[0]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[1]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[2]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[3]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[4]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[5]->texture, .sampler = r_shadow_atlas.sampler->sampler },
  }, 6);

  $(pass, bindFragmentSamplers, R_SAMPLER_VOXEL_CAUSTICS, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = bsp->voxels.caustics->texture->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
    { .texture = bsp->voxels.occlusion->texture->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
    { .texture = R_SkyTexture()->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
  }, 3);

  $(pass, bindFragmentSamplers, R_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_context.null_texture->texture, .sampler = r_bsp_draw.stage_sampler->sampler },
    { .texture = r_context.null_texture->texture, .sampler = r_bsp_draw.stage_sampler->sampler },
  }, 2);

  $(pass, bindFragmentSamplers, BSP_SAMPLER_WARP, &(SDL_GPUTextureSamplerBinding) {
    .texture = r_bsp_draw.warp_texture->texture,
    .sampler = r_bsp_draw.stage_sampler->sampler,
  }, 1);

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

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    if (e->effects & EF_NO_DRAW) {
      continue;
    }

    if (!IS_WORLDSPAWN(e->model) && R_CullEntity(view, e)) {
      continue;
    }

    R_DrawBlendBspEntity(view, e);
  }

  r_bsp_draw.pass = NULL;
}

/**
 * @brief Builds the BSP pipeline from the @c bsp_vs / @c bsp_fs shaders.
 */
void R_InitBspPipeline(void) {

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/bsp_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_samplers = BSP_NUM_VERTEX_SAMPLERS,
    .num_storage_buffers = R_STORAGE_MATERIAL_TOTAL, // per-vertex lighting binds the same buffers
    .num_uniform_buffers = BSP_NUM_UNIFORMS,
  });

  Shader *fragmentShader = $(r_context.device, loadShader, "shaders/bsp_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = BSP_NUM_SAMPLERS,
    .num_storage_buffers = R_STORAGE_MATERIAL_TOTAL,
    .num_uniform_buffers = BSP_NUM_UNIFORMS,
  });

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = r_scene_samples;
  info.vertex_shader = vertexShader->shader;
  info.fragment_shader = fragmentShader->shader;

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

  SDL_GPUColorTargetDescription color_targets[2] = {
    { .format = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT, .blend_state = GPU_BlendStateOpaque },
    { .format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT, .blend_state = GPU_BlendStateOpaque },
  };

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = color_targets,
    .num_color_targets = 2,
    .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
    .has_depth_stencil_target = true,
  };

  r_bsp_draw.pipeline = $(r_context.device, createGraphicsPipeline, &info);

  Shader *alphaTestFragmentShader = $(r_context.device, loadShader, "shaders/bsp_fs_alpha_test", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = BSP_NUM_SAMPLERS,
    .num_storage_buffers = R_STORAGE_MATERIAL_TOTAL,
    .num_uniform_buffers = BSP_NUM_UNIFORMS,
  });

  info.fragment_shader = alphaTestFragmentShader->shader;
  r_bsp_draw.pipeline_alpha_test = $(r_context.device, createGraphicsPipeline, &info);
  release(alphaTestFragmentShader);

  info.fragment_shader = fragmentShader->shader;
  color_targets[0].blend_state = GPU_BlendStateAlpha;
  color_targets[1].blend_state = (SDL_GPUColorTargetBlendState) {
    .enable_color_write_mask = true, .color_write_mask = 0,
  };
  info.depth_stencil_state.enable_depth_write = false;
  r_bsp_draw.blend_pipeline = $(r_context.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);

  r_bsp_draw.diffusemap_sampler = $(r_context.device, createSamplerLinearRepeat);
  r_bsp_draw.stage_sampler = $(r_context.device, createSamplerLinearRepeat);
  r_bsp_draw.ambient_sampler = $(r_context.device, createSamplerLinearClamp);

  #define WARP_IMAGE_SIZE 16
  byte data[WARP_IMAGE_SIZE][WARP_IMAGE_SIZE][4];
  for (int32_t i = 0; i < WARP_IMAGE_SIZE; i++) {
    for (int32_t j = 0; j < WARP_IMAGE_SIZE; j++) {
      data[i][j][0] = (byte) RandomRangeu(0, 48);
      data[i][j][1] = (byte) RandomRangeu(0, 48);
      data[i][j][2] = 0;
      data[i][j][3] = 255;
    }
  }

  r_bsp_draw.warp_texture = $(r_context.device, createTexture, &(SDL_GPUTextureCreateInfo) {
    .type = SDL_GPU_TEXTURETYPE_2D,
    .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
    .width = WARP_IMAGE_SIZE,
    .height = WARP_IMAGE_SIZE,
    .layer_count_or_depth = 1,
    .num_levels = 5, // log2(WARP_IMAGE_SIZE) + 1
    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
  }, data);
  #undef WARP_IMAGE_SIZE

  CommandBuffer *commands = $(r_context.device, acquireCommandBuffer);
  $(commands, generateMipmaps, r_bsp_draw.warp_texture->texture);
  $(commands, submit);
  release(commands);
}

/**
 * @brief Releases the BSP pipeline and samplers.
 */
void R_ShutdownBspPipeline(void) {
  r_bsp_draw.pipeline = release(r_bsp_draw.pipeline);
  r_bsp_draw.pipeline_alpha_test = release(r_bsp_draw.pipeline_alpha_test);
  r_bsp_draw.blend_pipeline = release(r_bsp_draw.blend_pipeline);
  r_bsp_draw.diffusemap_sampler = release(r_bsp_draw.diffusemap_sampler);
  r_bsp_draw.stage_sampler = release(r_bsp_draw.stage_sampler);
  r_bsp_draw.ambient_sampler = release(r_bsp_draw.ambient_sampler);
  r_bsp_draw.warp_texture = release(r_bsp_draw.warp_texture);

  for (int32_t i = 0; i < r_bsp_draw.num_stage_pipelines; i++) {
    r_bsp_draw.stage_pipelines[i].pipeline = release(r_bsp_draw.stage_pipelines[i].pipeline);
  }
  
  r_bsp_draw.num_stage_pipelines = 0;
}

/**
 * @brief Rebuilds the BSP pipeline and samplers in place, for pipeline-bound
 * cvar changes (r_antialias, r_anisotropy, ...) that would otherwise require
 * an r_restart. See R_UpdatePipelines.
 */
void R_UpdateBspPipeline(void) {
  R_ShutdownBspPipeline();
  R_InitBspPipeline();
}
