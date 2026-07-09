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
 * @brief The BSP program's own binding map, mirroring @c shaders/bsp_vs.glsl and
 * @c bsp_fs.glsl. Not shared with any other pipeline: each pipeline owns its own
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
  BSP_STORAGE_VOXEL_LIGHT_DATA,
  BSP_NUM_STORAGE_BUFFERS,
};

enum {
  BSP_UNIFORMS_GLOBALS,
  BSP_UNIFORMS_LOCALS, // model matrix + active_lights (vertex) / active_lights bitmask (fragment)
  BSP_UNIFORMS_MATERIAL, // material + stage params combined -- see material.glsl,
  BSP_NUM_UNIFORMS
};

/**
 * @brief Per-draw vertex locals (@c locals_block in @c bsp_vs.glsl): the model
 * matrix and the dynamic light cull bitmask for per-vertex lighting.
 */
typedef struct {
  mat4_t model;
  uint32_t active_lights[MAX_DYNAMIC_LIGHTS / 32];
} r_bsp_locals_t;

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
   * @brief The material (and its surface flags, which the material uniforms
   * also encode) bound by the current pass's most recent base draw. Draws are
   * grouped per block, so consecutive draw elements frequently share both;
   * tracking them skips their redundant pipeline binds, sampler binds, and
   * material uniform pushes (SDL_gpu deliberately performs no such
   * deduplication itself). Reset at pass start and invalidated by material
   * stage draws, which disturb the pipeline and material uniforms.
   */
  const r_material_t *material;
  int32_t surface;

  /**
   * @brief Cache of material stage pipelines, one per unique (src, dest, depth_write)
   * combination. depth_write is part of the key because the same stage may be drawn
   * from the opaque bucket (SURF_MATERIAL, which writes depth like any other opaque
   * surface) or the blend bucket (SURF_MASK_BLEND, which must not write depth, so that
   * overlapping translucent surfaces blend against each other rather than occluding).
   */
  struct {
    cm_blend_t src, dest;
    bool depth_write;
    GraphicsPipeline *pipeline;
  } stages[MAX_STAGE_PIPELINES];

  int32_t num_stages;
} r_bsp_draw;

/**
 * @brief Returns the bsp pipeline for the given material-stage blend function and
 * depth-write policy, creating and caching it on first use.
 */
static GraphicsPipeline *R_DrawBspStagePipeline(cm_blend_t src, cm_blend_t dest, bool depth_write) {

  for (int32_t i = 0; i < r_bsp_draw.num_stages; i++) {
    if (r_bsp_draw.stages[i].src == src && r_bsp_draw.stages[i].dest == dest &&
        r_bsp_draw.stages[i].depth_write == depth_write) {
      return r_bsp_draw.stages[i].pipeline;
    }
  }

  if (r_bsp_draw.num_stages == MAX_STAGE_PIPELINES) {
    return NULL;
  }

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/bsp_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_storage_buffers = BSP_NUM_STORAGE_BUFFERS, // per-vertex lighting binds the same buffers
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
    .src = src, .dest = dest, .depth_write = depth_write, .pipeline = pipeline,
  };
  return r_bsp_draw.stages[r_bsp_draw.num_stages++].pipeline;
}

/**
 * @brief Binds the stage uniforms and textures and draws a single material stage of
 * a BSP draw-elements batch, layered over the already-lit base surface.
 * @param depth_write True if this stage should write depth (the opaque bucket, e.g.
 * SURF_MATERIAL surfaces), false if it must not (the blend bucket, e.g. SURF_MASK_BLEND
 * surfaces, which must not occlude other translucent surfaces behind them).
 */
static void R_DrawBspDrawElementsMaterialStage(const r_view_t *view, RenderPass *pass, CommandBuffer *commands,
                                               const r_entity_t *entity, const r_bsp_draw_elements_t *draw,
                                               const r_stage_t *stage, bool depth_write) {

  r_material_uniforms_t uniforms;
  R_MaterialUniforms(draw->material, draw->surface, &uniforms);

  SDL_GPUTexture *texture, *texture_next;
  if (!R_StageUniforms(view, entity, draw, stage, &uniforms, &texture, &texture_next)) {
    return;
  }

  GraphicsPipeline *pipeline = R_DrawBspStagePipeline(stage->cm->blend.src, stage->cm->blend.dest, depth_write);
  if (!pipeline) {
    return;
  }

  // Stage draws disturb the bound pipeline and the material uniforms; the next
  // base draw must re-establish them (see r_bsp_draw.material).
  r_bsp_draw.material = NULL;

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
 * @param depth_write True if these stages should write depth (the opaque bucket), false
 * if not (the blend bucket). See R_DrawBspDrawElementsMaterialStage.
 * @remarks The base draw's bindings (material/voxel/shadow samplers, storage buffers,
 * and the globals/model/material/active-lights uniforms) remain current; each stage
 * only rebinds the stage pipeline, its textures, and the per-stage uniforms.
 */
static void R_DrawBspDrawElementsMaterialStages(const r_view_t *view, RenderPass *pass, CommandBuffer *commands,
                                                const r_entity_t *entity, const r_bsp_draw_elements_t *draw,
                                                bool depth_write) {

  const r_material_t *material = draw->material;
  if (!(material->cm->stage_flags & STAGE_DRAW)) {
    return;
  }

  for (const r_stage_t *stage = material->stages; stage; stage = stage->next) {

    if (!(stage->cm->flags & STAGE_DRAW)) {
      continue;
    }

    R_DrawBspDrawElementsMaterialStage(view, pass, commands, entity, draw, stage, depth_write);
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

    if (draw->material != r_bsp_draw.material || draw->surface != r_bsp_draw.surface) {
      r_bsp_draw.material = draw->material;
      r_bsp_draw.surface = draw->surface;

      $(pass, bindPipeline, r_bsp_draw.pipeline);

      $(pass, bindFragmentSamplers, BSP_SAMPLER_MATERIAL, &(SDL_GPUTextureSamplerBinding) {
        .texture = draw->material->texture->texture->texture,
        .sampler = r_bsp_draw.diffusemap_sampler->sampler,
      }, 1);

      r_material_uniforms_t material;
      R_MaterialUniforms(draw->material, draw->surface, &material);
      $(commands, pushUniformData, BSP_UNIFORMS_MATERIAL, &material, sizeof(material));
    }

    const Uint32 first_index = (Uint32) ((uintptr_t) draw->elements / sizeof(uint32_t));

    if (!(draw->surface & SURF_MATERIAL)) {
      $(pass, drawIndexedPrimitives, draw->num_elements, 1, first_index, 0, 0);
      r_stats.bsp_triangles += draw->num_elements / 3;
    }

    r_stats.bsp_draw_elements++;

    if (r_draw_material_stages->integer) {
      R_DrawBspDrawElementsMaterialStages(view, pass, commands, entity, draw, true);
    }
  }
}

/**
 * @brief Draws one BSP inline model entity's opaque surfaces, including the
 * world itself (the worldspawn entity is a BSP inline model entity like any
 * other -- see @c R_DrawOpaqueBspEntities). The world's dynamic lights are culled
 * per block (the unit of light culling for a mesh this large); other inline
 * models are small enough to light as a whole, culled to their entity bounds.
 */
static void R_DrawOpaqueBspEntity(const r_view_t *view, RenderPass *pass, CommandBuffer *commands, const r_entity_t *entity) {

  r_bsp_locals_t locals = {
    .model = entity->matrix,
  };

  const r_bsp_inline_model_t *in = entity->model->bsp_inline;

  if (!IS_WORLDSPAWN(entity->model)) {
    R_ActiveLights(view, entity->abs_model_bounds, locals.active_lights);
    $(commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &locals, sizeof(locals));
    $(commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, locals.active_lights, sizeof(locals.active_lights));
  }

  const r_bsp_block_t *block = in->blocks;
  for (int32_t i = 0; i < in->num_blocks; i++, block++) {

    if (IS_WORLDSPAWN(entity->model)) {

      if (block->query->result == 0) {
        r_stats.blocks_occluded++;
        continue;
      }

      r_stats.blocks_visible++;

      R_ActiveLights(view, block->node->visible_bounds, locals.active_lights);
      $(commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &locals, sizeof(locals));
      $(commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, locals.active_lights, sizeof(locals.active_lights));
    }

    R_DrawBspBlockOpaque(view, pass, commands, entity, block);
  }

  r_stats.bsp_inline_models++;
}

/**
 * @brief Draws per-vertex normal, tangent, and bitangent lines for nearby BSP vertices when enabled.
 */
static void R_DrawBspNormals(const r_view_t *view, const r_bsp_model_t *bsp) {

  if (!r_draw_bsp_normals->value) {
    return;
  }

  const r_bsp_vertex_t *v = bsp->vertexes;
  for (int32_t i = 0; i < bsp->num_vertexes; i++, v++) {

    const vec3_t pos = v->position;
    if (Vec3_Distance(pos, view->origin) > 512.f) {
      continue;
    }

    const vec3_t normal[] = { pos, Vec3_Fmaf(pos, 8.f, v->normal) };
    const vec3_t tangent[] = { pos, Vec3_Fmaf(pos, 8.f, v->tangent) };
    const vec3_t bitangent[] = { pos, Vec3_Fmaf(pos, 8.f, v->bitangent) };

    R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, normal, 2, color_red, true);

    if (r_draw_bsp_normals->integer > 1) {
      R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, tangent, 2, color_green, true);

      if (r_draw_bsp_normals->integer > 2) {
        R_Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, bitangent, 2, color_blue, true);
      }
    }
  }
}

/**
 * @brief Renders the opaque world BSP surfaces with their material diffuse texture,
 * clustered per-voxel static lighting, and per-block dynamic lighting.
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

  $(commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

  r_bsp_draw.material = NULL;

  $(pass, bindPipeline, r_bsp_draw.pipeline);
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

  $(pass, bindFragmentSamplers, BSP_SAMPLER_VOXEL_CAUSTICS, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = bsp->voxels.caustics->texture->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
    { .texture = bsp->voxels.occlusion->texture->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
    { .texture = R_SkyTexture()->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
  }, 3);

  $(pass, bindFragmentSamplers, BSP_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_context.null_texture->texture, .sampler = r_bsp_draw.stage_sampler->sampler },
    { .texture = r_context.null_texture->texture, .sampler = r_bsp_draw.stage_sampler->sampler },
  }, 2);

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
  $(pass, bindVertexStorageBuffers, BSP_STORAGE_LIGHTS, storage, BSP_NUM_STORAGE_BUFFERS);

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

    R_DrawOpaqueBspEntity(view, pass, commands, e);
  }

  pass = release(pass);

  R_DrawBspNormals(view, bsp);
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

    if (draw->material != r_bsp_draw.material || draw->surface != r_bsp_draw.surface) {
      r_bsp_draw.material = draw->material;
      r_bsp_draw.surface = draw->surface;

      $(pass, bindPipeline, r_bsp_draw.blend_pipeline);

      $(pass, bindFragmentSamplers, BSP_SAMPLER_MATERIAL, &(SDL_GPUTextureSamplerBinding) {
        .texture = draw->material->texture->texture->texture,
        .sampler = r_bsp_draw.diffusemap_sampler->sampler,
      }, 1);

      r_material_uniforms_t material;
      R_MaterialUniforms(draw->material, draw->surface, &material);
      $(commands, pushUniformData, BSP_UNIFORMS_MATERIAL, &material, sizeof(material));
    }

    const Uint32 first_index = (Uint32) ((uintptr_t) draw->elements / sizeof(uint32_t));
    $(pass, drawIndexedPrimitives, draw->num_elements, 1, first_index, 0, 0);

    r_stats.bsp_triangles += draw->num_elements / 3;
    r_stats.bsp_draw_elements++;

    if (r_draw_material_stages->integer) {
      R_DrawBspDrawElementsMaterialStages(view, pass, commands, entity, draw, false);
    }
  }
}

/**
 * @brief Draws one BSP inline model entity's translucent surfaces, including the world itself.
 */
static void R_DrawBlendBspEntity(const r_view_t *view, RenderPass *pass, CommandBuffer *commands,
                                 const r_entity_t *entity) {

  r_bsp_locals_t locals = {
    .model = entity->matrix,
  };

  const r_bsp_inline_model_t *in = entity->model->bsp_inline;

  if (!IS_WORLDSPAWN(entity->model)) {
    R_ActiveLights(view, entity->abs_model_bounds, locals.active_lights);
    $(commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &locals, sizeof(locals));
    $(commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, locals.active_lights, sizeof(locals.active_lights));
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

      R_ActiveLights(view, block->node->visible_bounds, locals.active_lights);
      $(commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &locals, sizeof(locals));
      $(commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, locals.active_lights, sizeof(locals.active_lights));
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

  $(commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

  r_bsp_draw.material = NULL;

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

  $(pass, bindFragmentSamplers, BSP_SAMPLER_VOXEL_CAUSTICS, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = bsp->voxels.caustics->texture->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
    { .texture = bsp->voxels.occlusion->texture->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
    { .texture = R_SkyTexture()->texture, .sampler = r_bsp_draw.ambient_sampler->sampler },
  }, 3);

  $(pass, bindFragmentSamplers, BSP_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_context.null_texture->texture, .sampler = r_bsp_draw.stage_sampler->sampler },
    { .texture = r_context.null_texture->texture, .sampler = r_bsp_draw.stage_sampler->sampler },
  }, 2);

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
  $(pass, bindVertexStorageBuffers, BSP_STORAGE_LIGHTS, storage, BSP_NUM_STORAGE_BUFFERS);

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
    .num_storage_buffers = BSP_NUM_STORAGE_BUFFERS, // per-vertex lighting binds the same buffers
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
