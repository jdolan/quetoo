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
 * @brief The BSP program's own binding map, mirroring shaders/bsp_vs.glsl and
 * bsp_fs.glsl. Not shared with any other pipeline: each pipeline owns its own
 * dense-from-0 numbering instead of coordinating through a global map.
 */
enum {
  BSP_SAMPLER_MATERIAL,
  BSP_SAMPLER_SHADOW_ATLAS_0, // one sampler2DShadow per cube face (SDL_gpu forbids array depth targets)
  BSP_SAMPLER_SHADOW_ATLAS_1,
  BSP_SAMPLER_SHADOW_ATLAS_2,
  BSP_SAMPLER_SHADOW_ATLAS_3,
  BSP_SAMPLER_SHADOW_ATLAS_4,
  BSP_SAMPLER_SHADOW_ATLAS_5,
  BSP_SAMPLER_VOXEL_CAUSTICS,
  BSP_SAMPLER_VOXEL_OCCLUSION,
  BSP_SAMPLER_SKY_AMBIENT,
  BSP_SAMPLER_STAGE,
  BSP_SAMPLER_STAGE_NEXT,
  BSP_SAMPLER_WARP,
  BSP_NUM_SAMPLERS,
};

enum {
  BSP_STORAGE_LIGHTS,
  BSP_STORAGE_VOXEL_LIGHT_INDICES,
  BSP_STORAGE_VOXEL_LIGHT_DATA, // (first_index, count) pairs; a buffer because
                                // D3D12 can't sample R32G32_INT 3D textures
  BSP_NUM_STORAGE_BUFFERS,
};

enum {
  BSP_UNIFORMS_GLOBALS,
  BSP_UNIFORMS_LOCALS, // model matrix (vertex) / active_lights bitmask (fragment)
  BSP_UNIFORMS_MATERIAL, // material + stage params combined -- see material.glsl
};
#define BSP_NUM_UNIFORMS 3 // same count/slot for both stages

#define MAX_STAGE_PIPELINES 16

/**
 * @brief The BSP program (bsp_vs/bsp_fs): its graphics pipeline, samplers, and
 * the lazily-built cache of material-stage blend pipelines. Draws BSP geometry
 * with the material diffuse texture and clustered per-voxel lighting, and its
 * material stages via the same shader (a runtime branch on stage.flags, not a
 * pipeline swap): see R_DrawBspDrawElementsMaterialStage.
 */
static struct {

  /**
   * @brief The BSP graphics pipeline.
   */
  GraphicsPipeline *pipeline;

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
   * @brief Cache of MATERIAL_STAGES pipelines, one per unique (src, dest) blend
   * function. SDL_gpu bakes blend state into the pipeline, so stage blends can't
   * be set dynamically; they're realized lazily here (only a handful of combos occur).
   */
  struct {
    cm_blend_t src, dest;
    GraphicsPipeline *pipeline;
  } stages[MAX_STAGE_PIPELINES];

  int32_t num_stages;
} r_bsp_draw;

/**
 * @brief Returns the bsp pipeline for the given material-stage blend function,
 * creating and caching it on first use. Wraps the same bsp_vs/bsp_fs shader as
 * the opaque pipeline (a stage draw is a runtime branch, not a shader swap);
 * only the blend state differs, which SDL_gpu bakes into the pipeline.
 */
static GraphicsPipeline *R_StagePipeline(cm_blend_t src, cm_blend_t dest) {

  for (int32_t i = 0; i < r_bsp_draw.num_stages; i++) {
    if (r_bsp_draw.stages[i].src == src && r_bsp_draw.stages[i].dest == dest) {
      return r_bsp_draw.stages[i].pipeline;
    }
  }

  if (r_bsp_draw.num_stages == MAX_STAGE_PIPELINES) {
    return NULL;
  }

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/bsp_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = BSP_NUM_UNIFORMS,
  });

  Shader *fragmentShader = $(r_context.device, loadShader, "shaders/bsp_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = BSP_NUM_SAMPLERS,
    .num_storage_buffers = BSP_NUM_STORAGE_BUFFERS,
    .num_uniform_buffers = BSP_NUM_UNIFORMS,
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

  r_bsp_draw.stages[r_bsp_draw.num_stages] = (typeof(r_bsp_draw.stages[0])) {
    .src = src, .dest = dest, .pipeline = pipeline,
  };
  return r_bsp_draw.stages[r_bsp_draw.num_stages++].pipeline;
}

/**
 * @brief Binds the stage uniforms and textures and draws a single material stage of
 * a BSP draw-elements batch, layered over the already-lit base surface.
 */
static void R_DrawBspDrawElementsMaterialStage(const r_view_t *view, RenderPass *pass, CommandBuffer *commands,
                                               const r_entity_t *entity, const r_bsp_draw_elements_t *draw,
                                               const r_stage_t *stage) {

  r_material_uniforms_t uniforms;
  R_MaterialUniforms(draw->material, draw->surface, &uniforms);

  SDL_GPUTexture *texture, *texture_next;
  if (!R_StageUniforms(view, entity, draw, stage, &uniforms, &texture, &texture_next)) {
    return;
  }

  GraphicsPipeline *pipeline = R_StagePipeline(stage->cm->blend.src, stage->cm->blend.dest);
  //GraphicsPipeline *pipeline = R_StagePipeline(BLEND_SRC_ALPHA, BLEND_ONE);
  if (!pipeline) {
    return;
  }

  $(pass, bindPipeline, pipeline);

  $(pass, bindFragmentSamplers, BSP_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = texture, .sampler = r_bsp_draw.stage_sampler->sampler },
    { .texture = texture_next, .sampler = r_bsp_draw.stage_sampler->sampler },
  }, 2);

  $(commands, pushUniformData, BSP_UNIFORMS_MATERIAL, &uniforms, sizeof(uniforms));

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
                                                const r_entity_t *entity, const r_bsp_draw_elements_t *draw) {

  const r_material_t *material = draw->material;
  if (!(material->cm->stage_flags & STAGE_DRAW)) {
    return;
  }

  for (const r_stage_t *stage = material->stages; stage; stage = stage->next) {

    if (!(stage->cm->flags & STAGE_DRAW)) {
      continue;
    }

    R_DrawBspDrawElementsMaterialStage(view, pass, commands, entity, draw, stage);
  }
}

/**
 * @brief Draws the opaque (non-sky, non-blend) draw elements of one BSP block,
 * with the given base pipeline. The caller has already pushed the model matrix
 * and the active-lights bitmask for this block/entity.
 */
static void R_DrawBspBlockOpaque(const r_view_t *view, RenderPass *pass, CommandBuffer *commands,
                                 const r_entity_t *entity, const r_bsp_block_t *block) {

  const r_bsp_draw_elements_t *draw = block->draw_elements;
  for (int32_t j = 0; j < block->num_draw_elements; j++, draw++) {

    if (draw->surface & (SURF_SKY | SURF_MASK_BLEND)) {
      continue;
    }

    $(pass, bindPipeline, r_bsp_draw.pipeline);

    $(pass, bindFragmentSamplers, BSP_SAMPLER_MATERIAL, &(SDL_GPUTextureSamplerBinding) {
      .texture = draw->material->texture->texture->texture,
      .sampler = r_bsp_draw.diffusemap_sampler->sampler,
    }, 1);

    r_material_uniforms_t material;
    R_MaterialUniforms(draw->material, draw->surface, &material);
    $(commands, pushUniformData, BSP_UNIFORMS_MATERIAL, &material, sizeof(material));

    const Uint32 first_index = (Uint32) ((uintptr_t) draw->elements / sizeof(uint32_t));

    $(pass, drawIndexedPrimitives, draw->num_elements, 1, first_index, 0, 0);

    r_stats.bsp_triangles += draw->num_elements / 3;
    r_stats.bsp_draw_elements++;

    if (r_draw_material_stages->integer) {
      R_DrawBspDrawElementsMaterialStages(view, pass, commands, entity, draw);
    }
  }
}

/**
 * @brief Draws one BSP inline model entity's opaque surfaces, including the
 * world itself (the worldspawn entity is a BSP inline model entity like any
 * other -- see R_DrawOpaqueBspEntities). The world's dynamic lights are culled
 * per block (the unit of light culling for a mesh this large); other inline
 * models are small enough to light as a whole, culled to their entity bounds.
 */
static void R_DrawOpaqueBspEntity(const r_view_t *view, RenderPass *pass, CommandBuffer *commands, const r_entity_t *entity) {

  uint32_t active_lights[MAX_DYNAMIC_LIGHTS / 32];

  const r_bsp_inline_model_t *in = entity->model->bsp_inline;

  if (!IS_WORLDSPAWN(entity->model)) {
    R_ActiveLights(view, entity->abs_model_bounds, active_lights);
    $(commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, active_lights, sizeof(active_lights));
  }

  const r_bsp_block_t *block = in->blocks;
  for (int32_t i = 0; i < in->num_blocks; i++, block++) {

    if (IS_WORLDSPAWN(entity->model)) {
      
      if (block->query->result == 0) {
        r_stats.blocks_occluded++;
        continue;
      }
      
      r_stats.blocks_visible++;

      R_ActiveLights(view, block->node->visible_bounds, active_lights);
      $(commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, active_lights, sizeof(active_lights));
    }

    R_DrawBspBlockOpaque(view, pass, commands, entity, block);
  }

  r_stats.bsp_inline_models++;
}

/**
 * @brief Renders the opaque world BSP surfaces with their material diffuse texture,
 * clustered per-voxel static lighting, and per-block dynamic lighting.
 * @remarks Iterates every BSP inline model entity in the view, including the
 * world itself (see R_DrawOpaqueBspEntity); per block for the world, or per
 * entity bounds for inline models (doors, platforms, ...), the active dynamic
 * lights are culled. See R_DrawBlendBspEntities for the translucent counterpart.
 */
void R_DrawOpaqueBspEntities(const r_view_t *view) {

  if (!r_models.world) {
    return;
  }

  if (!view->framebuffer) {
    return;
  }

  CommandBuffer *commands = r_context.device->commands;

  const r_bsp_model_t *bsp = r_models.world->bsp;
  Framebuffer *framebuffer = view->framebuffer;

  const SDL_FColor clear_color = { 0.f, 0.f, 0.f, 1.f };
  const SDL_FColor clear_depth_color = { 1.f, 1.f, 1.f, 1.f };
  const SDL_GPUColorTargetInfo color[] = {
    $(framebuffer, colorTargetInfo, 0, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE, &clear_color),
    $(framebuffer, colorTargetInfo, 1, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE, &clear_depth_color),
  };
  
  const SDL_GPULoadOp depth_loadop = r_depth_pass->integer ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
  const SDL_GPUDepthStencilTargetInfo depth = $(framebuffer, depthTargetInfo, depth_loadop, SDL_GPU_STOREOP_STORE, 1.f);

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
  $(commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));
  $(commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, model.array, sizeof(model));

  $(pass, bindPipeline, r_bsp_draw.pipeline);
  $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = bsp->vertex_buffer->buffer }, 1);
  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) { .buffer = bsp->elements_buffer->buffer }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  // The point-light shadow atlas (comparison sampler).
  $(pass, bindFragmentSamplers, BSP_SAMPLER_SHADOW_ATLAS_0, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_shadow_atlas.textures[0]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[1]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[2]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[3]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[4]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[5]->texture, .sampler = r_shadow_atlas.sampler->sampler },
  }, 6);

  // The voxel caustics / occlusion volumes and the sky cubemap for ambient light.
  $(pass, bindFragmentSamplers, BSP_SAMPLER_VOXEL_CAUSTICS, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = bsp->voxels.caustics->texture->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
    { .texture = bsp->voxels.occlusion->texture->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
    { .texture = R_SkyTexture()->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
  }, 3);

  // Stage/stage-next: opaque and blend draws never sample these (bsp_fs's
  // STAGE_NONE branch doesn't touch them), but the shared shader declares
  // them, so SDL_gpu requires them bound regardless.
  $(pass, bindFragmentSamplers, BSP_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_context.null_texture->texture, .sampler = r_bsp_draw.stage_sampler->sampler },
    { .texture = r_context.null_texture->texture, .sampler = r_bsp_draw.stage_sampler->sampler },
  }, 2);

  // The procedural warp noise texture, for STAGE_WARP liquid surfaces.
  $(pass, bindFragmentSamplers, BSP_SAMPLER_WARP, &(SDL_GPUTextureSamplerBinding) {
    .texture = r_bsp_draw.warp_texture->texture,
    .sampler = r_bsp_draw.stage_sampler->sampler,
  }, 1);

  SDL_GPUBuffer *storage[] = {
    r_lights.buffer->buffer,
    bsp->voxels.light_indices_buffer ? bsp->voxels.light_indices_buffer->buffer
                                     : r_lights.buffer->buffer,
    bsp->voxels.light_data_buffer->buffer,
  };
  $(pass, bindFragmentStorageBuffers, BSP_STORAGE_LIGHTS, storage, BSP_NUM_STORAGE_BUFFERS);

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

    $(commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, e->matrix.array, sizeof(e->matrix));

    R_DrawOpaqueBspEntity(view, pass, commands, e);
  }

  pass = release(pass);
}

/**
 * @brief Draws the translucent (@c SURF_MASK_BLEND) draw elements of a single
 * block, assuming its active-lights bitmask has already been pushed.
 */
static void R_DrawBspBlockBlend(const r_view_t *view, RenderPass *pass, CommandBuffer *commands,
                                const r_entity_t *entity, const r_bsp_block_t *block) {

  const r_bsp_draw_elements_t *draw = block->draw_elements;
  for (int32_t j = 0; j < block->num_draw_elements; j++, draw++) {

    if (!(draw->surface & SURF_MASK_BLEND) || (draw->surface & SURF_SKY)) {
      continue;
    }

    $(pass, bindPipeline, r_bsp_draw.blend_pipeline);

    $(pass, bindFragmentSamplers, BSP_SAMPLER_MATERIAL, &(SDL_GPUTextureSamplerBinding) {
      .texture = draw->material->texture->texture->texture,
      .sampler = r_bsp_draw.diffusemap_sampler->sampler,
    }, 1);

    r_material_uniforms_t material;
    R_MaterialUniforms(draw->material, draw->surface, &material);
    $(commands, pushUniformData, BSP_UNIFORMS_MATERIAL, &material, sizeof(material));

    const Uint32 first_index = (Uint32) ((uintptr_t) draw->elements / sizeof(uint32_t));
    $(pass, drawIndexedPrimitives, draw->num_elements, 1, first_index, 0, 0);

    r_stats.bsp_triangles += draw->num_elements / 3;
    r_stats.bsp_draw_elements++;

    if (r_draw_material_stages->integer) {
      R_DrawBspDrawElementsMaterialStages(view, pass, commands, entity, draw);
    }
  }
}

/**
 * @brief Draws one BSP inline model entity's translucent surfaces, including the world itself.
 */
static void R_DrawBlendBspEntity(const r_view_t *view, RenderPass *pass, CommandBuffer *commands,
                                 const r_entity_t *entity) {

  uint32_t active_lights[MAX_DYNAMIC_LIGHTS / 32];

  const r_bsp_inline_model_t *in = entity->model->bsp_inline;

  if (!IS_WORLDSPAWN(entity->model)) {
    R_ActiveLights(view, entity->abs_model_bounds, active_lights);
    $(commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, active_lights, sizeof(active_lights));
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

      R_ActiveLights(view, block->node->visible_bounds, active_lights);
      $(commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, active_lights, sizeof(active_lights));
    }

    R_DrawBspBlockBlend(view, pass, commands, entity, block);
  }
}

/**
 * @brief Renders the translucent (SURF_MASK_BLEND) world and inline BSP model
 * surfaces over the opaque scene, alpha-blended and depth-tested but without
 * writing depth.
 * @remarks Mirrors R_DrawOpaqueBspEntities but with the blend pipeline and the inverse
 * surface filter, including its material stages (animated water/glass overlays).
 * No back-to-front sorting, matching main -- single-layer translucency (water,
 * glass) is correct without it.
 */
void R_DrawBlendBspEntities(const r_view_t *view) {

  if (!r_models.world) {
    return;
  }

  CommandBuffer *commands = r_context.device->commands;

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
  $(commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));
  $(commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, model.array, sizeof(model));

  $(pass, bindPipeline, r_bsp_draw.blend_pipeline);
  $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = bsp->vertex_buffer->buffer }, 1);
  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) { .buffer = bsp->elements_buffer->buffer }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  $(pass, bindFragmentSamplers, BSP_SAMPLER_SHADOW_ATLAS_0, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_shadow_atlas.textures[0]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[1]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[2]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[3]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[4]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[5]->texture, .sampler = r_shadow_atlas.sampler->sampler },
  }, 6);

  // The voxel caustics / occlusion volumes and the sky cubemap for ambient light.
  $(pass, bindFragmentSamplers, BSP_SAMPLER_VOXEL_CAUSTICS, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = bsp->voxels.caustics->texture->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
    { .texture = bsp->voxels.occlusion->texture->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
    { .texture = R_SkyTexture()->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
  }, 3);

  // Stage/stage-next: opaque and blend draws never sample these (bsp_fs's
  // STAGE_NONE branch doesn't touch them), but the shared shader declares
  // them, so SDL_gpu requires them bound regardless.
  $(pass, bindFragmentSamplers, BSP_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_context.null_texture->texture, .sampler = r_bsp_draw.stage_sampler->sampler },
    { .texture = r_context.null_texture->texture, .sampler = r_bsp_draw.stage_sampler->sampler },
  }, 2);

  // The procedural warp noise texture, for STAGE_WARP liquid surfaces.
  $(pass, bindFragmentSamplers, BSP_SAMPLER_WARP, &(SDL_GPUTextureSamplerBinding) {
    .texture = r_bsp_draw.warp_texture->texture,
    .sampler = r_bsp_draw.stage_sampler->sampler,
  }, 1);

  SDL_GPUBuffer *storage[] = {
    r_lights.buffer->buffer,
    bsp->voxels.light_indices_buffer ? bsp->voxels.light_indices_buffer->buffer
                                     : r_lights.buffer->buffer,
    bsp->voxels.light_data_buffer->buffer,
  };
  $(pass, bindFragmentStorageBuffers, BSP_STORAGE_LIGHTS, storage, BSP_NUM_STORAGE_BUFFERS);

  // Every BSP inline model entity in the view, including the world itself; see
  // R_DrawBlendBspEntity.
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

    $(commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, e->matrix.array, sizeof(e->matrix));

    R_DrawBlendBspEntity(view, pass, commands, e);
  }

  pass = release(pass);
}

/**
 * @brief Builds the BSP pipeline from the @c bsp_vs / @c bsp_fs shaders.
 */
void R_InitBspPipeline(void) {

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/bsp_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = BSP_NUM_UNIFORMS,
  });

  Shader *fragmentShader = $(r_context.device, loadShader, "shaders/bsp_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = BSP_NUM_SAMPLERS,
    .num_storage_buffers = BSP_NUM_STORAGE_BUFFERS,
    .num_uniform_buffers = BSP_NUM_UNIFORMS,
  });

  const Framebuffer *framebuffer = r_context.device->framebuffer;

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
    { .format = R_SCENE_COLOR_FORMAT, .blend_state = GPU_BlendStateOpaque },
    { .format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT, .blend_state = GPU_BlendStateOpaque },
  };

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = color_targets,
    .num_color_targets = 2,
    .depth_stencil_format = framebuffer->depthFormat,
    .has_depth_stencil_target = true,
  };

  r_bsp_draw.pipeline = $(r_context.device, createGraphicsPipeline, &info);

  // Alpha-blend variant (depth test, no depth write) for translucent
  // SURF_MASK_BLEND surfaces; the depth copy (color 1) stays masked off, since
  // a translucent surface must not overwrite the opaque depth sprites sample.
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
  r_bsp_draw.blend_pipeline = release(r_bsp_draw.blend_pipeline);
  r_bsp_draw.diffusemap_sampler = release(r_bsp_draw.diffusemap_sampler);
  r_bsp_draw.stage_sampler = release(r_bsp_draw.stage_sampler);
  r_bsp_draw.ambient_sampler = release(r_bsp_draw.ambient_sampler);
  r_bsp_draw.warp_texture = release(r_bsp_draw.warp_texture);

  for (int32_t i = 0; i < r_bsp_draw.num_stages; i++) {
    r_bsp_draw.stages[i].pipeline = release(r_bsp_draw.stages[i].pipeline);
  }
  
  r_bsp_draw.num_stages = 0;
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
