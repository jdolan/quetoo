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
 * @brief Additional BSP sampler slot for liquid warp textures.
 */
enum {
  BSP_SAMPLER_WARP = R_SAMPLER_MATERIAL_TOTAL,
  BSP_SAMPLER_PORTAL,
  BSP_NUM_SAMPLERS,
};

/**
 * @brief bsp_vs only samples the voxel caustics/occlusion and sky textures
 * (see material.glsl), at its own compact 0..2 binding numbering -- not the
 * fragment stage's absolute R_SAMPLER_VOXEL_CAUSTICS/OCCLUSION/SKY indices.
 */
enum {
  BSP_VERTEX_SAMPLER_VOXEL_CAUSTICS,
  BSP_VERTEX_SAMPLER_VOXEL_OCCLUSION,
  BSP_VERTEX_SAMPLER_SKY,
  BSP_NUM_VERTEX_SAMPLERS,
};

enum {
  BSP_UNIFORMS_GLOBALS,
  BSP_UNIFORMS_LOCALS,
  BSP_UNIFORMS_MATERIAL,
  BSP_NUM_UNIFORMS
};

/**
 * @brief Per-draw BSP uniforms, shared by both stages.
 */
typedef struct {
  Mat4 model;
  RenderActiveDynamicLights activeDynamicLights;

  /**
   * @brief The layer of texture_portal this draw's faces sample, or `-1` for none.
   */
  int32_t portalLayer;
} RenderBspUniformLocals;

#define MAX_STAGE_PIPELINES 16

/**
 * @brief BSP draw pipeline state and cached material-stage pipelines.
 */
static struct {

  /**
   * @brief Opaque BSP pipeline.
   */
  GraphicsPipeline *opaquePipeline;

  /**
   * @brief Alpha-test BSP pipeline.
   */
  GraphicsPipeline *alphaTestPipeline;

  /**
   * @brief Translucent BSP pipeline.
   */
  GraphicsPipeline *blendPipeline;

  /**
   * @brief Repeating linear sampler, for tiled material and stage textures.
   */
  Sampler *repeatSampler;

  /**
   * @brief Clamped linear sampler, for voxel and sky textures.
   */
  Sampler *clampSampler;

  /**
   * @brief Procedural warp texture.
   */
  Texture *warpTexture;

  /**
   * @brief Cached bound material state.
   */
  const RenderMaterial *material;
  int32_t surface;

  /**
   * @brief The locals as last pushed, so that the portal layer can be updated alone.
   */
  RenderBspUniformLocals locals;

  /**
   * @brief Cached material-stage pipelines.
   */
  RenderStagePipeline stagePipelines[MAX_STAGE_PIPELINES];
  int32_t numStagePipelines;
} r_bsp_draw;

/**
 * @brief Pushes the per-model uniforms.
 * @details The portal layer resets to `-1` here, so a model holding no portal face never pushes
 * one; `R_PushBspPortalLayer` sets it for the draws that do.
 */
static inline void R_PushBspUniformLocals(const RenderBspUniformLocals *locals, RenderPass *pass) {

  r_bsp_draw.locals = *locals;
  r_bsp_draw.locals.portalLayer = -1;

  $(pass->commands, pushUniformData, SLOT_UNIFORMS_LOCALS, &r_bsp_draw.locals, sizeof(r_bsp_draw.locals));
}

/**
 * @brief Pushes the portal layer @p draw samples for @p view, if it differs from the one already
 * pushed.
 * @details The layer belongs to the draw elements rather than to the model, since a model may
 * hold several portal faces, so it is pushed only when it changes -- which for the overwhelming
 * majority of draws, none of which are portals, is never.
 *
 * A portal view always pushes `-1`, which is what keeps portals from recursing: seen through one,
 * a portal face falls back to its plain material. The layer belongs to the world's portal rather
 * than to the view being drawn, so a portal already drawn this frame would otherwise report its
 * layer here -- and portal views are bound the placeholder texture, not the portal texture they
 * are being drawn into, so the face would come out solid black rather than portalled.
 */
static inline void R_PushBspPortalLayer(const RenderView *view, const RenderBspDrawElements *draw, RenderPass *pass) {

  const int32_t layer = draw->portal && view->type != VIEW_PORTAL ? draw->portal->layer : -1;

  if (layer != r_bsp_draw.locals.portalLayer) {
    r_bsp_draw.locals.portalLayer = layer;

    $(pass->commands, pushUniformData, SLOT_UNIFORMS_LOCALS, &r_bsp_draw.locals, sizeof(r_bsp_draw.locals));
  }
}

/**
 * @brief Binds the state @p draw is drawn with: its pipeline, material and portal layer.
 * @param pipeline The pipeline to bind, or `NULL` to leave the bound one in place.
 * @details Material state is bound only when it changes, since draw elements arrive sorted by
 * material and surface. The portal layer keeps its own cache, because draw elements sharing a
 * material need not share a portal -- two portal faces cut from the same brush do not.
 */
static inline void R_BindBspDrawElements(const RenderView *view,
                                         const RenderBspDrawElements *draw,
                                         GraphicsPipeline *pipeline,
                                         RenderPass *pass) {

  if (draw->material != r_bsp_draw.material || draw->surface != r_bsp_draw.surface) {
    r_bsp_draw.material = draw->material;
    r_bsp_draw.surface = draw->surface;

    if (pipeline) {
      $(pass, bindPipeline, pipeline);
    }

    $(pass, bindFragmentSamplers, R_SAMPLER_MATERIAL, &(SDL_GPUTextureSamplerBinding) {
      .texture = draw->material->texture->texture->texture,
      .sampler = r_bsp_draw.repeatSampler->sampler,
    }, 1);

    RenderMaterialUniforms material;
    R_MaterialUniforms(draw->material, draw->surface, &material);
    $(pass->commands, pushUniformData, BSP_UNIFORMS_MATERIAL, &material, sizeof(material));
  }

  R_PushBspPortalLayer(view, draw, pass);
}

/**
 * @brief Returns the cached BSP material-stage pipeline for the given blend state.
 */
static GraphicsPipeline *R_DrawBspMaterialStagePipeline(CmBlend src, CmBlend dest, bool depthWrite) {

  const RenderStagePipeline *p = r_bsp_draw.stagePipelines;
  for (int32_t i = 0; i < r_bsp_draw.numStagePipelines; i++, p++) {
    if (p->src == src && p->dest == dest && p->depthWrite == depthWrite) {
      return p->pipeline;
    }
  }

  if (r_bsp_draw.numStagePipelines == MAX_STAGE_PIPELINES) {
    Com_Error(ERROR_DROP, "MAX_STAGE_PIPELINES\n");
  }

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/bsp_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_samplers = BSP_NUM_VERTEX_SAMPLERS,
    .num_storage_buffers = R_STORAGE_MATERIAL_TOTAL,
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

  info.depth_stencil_state.enable_depth_write = depthWrite;

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = &(SDL_GPUVertexBufferDescription) {
      .slot = 0,
      .pitch = sizeof(RenderBspVertex),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    },
    .num_vertex_buffers = 1,
    .vertex_attributes = (SDL_GPUVertexAttribute[]) {
      { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderBspVertex, position) },
      { .location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderBspVertex, normal) },
      { .location = 2, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderBspVertex, tangent) },
      { .location = 3, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderBspVertex, bitangent) },
      { .location = 4, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = offsetof(RenderBspVertex, diffusemap) },
      { .location = 5, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, .offset = offsetof(RenderBspVertex, color) },
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

  RenderStagePipeline *out = &r_bsp_draw.stagePipelines[r_bsp_draw.numStagePipelines++];

  out->src = src;
  out->dest = dest;
  out->depthWrite = depthWrite;
  out->pipeline = pipeline;

  return out->pipeline;
}

/**
 * @return True if @p block should be skipped when drawing @p view.
 * @details The main view has hardware occlusion queries, which subsume frustum culling. A
 * portal view cannot use them at all -- they were resolved for a camera somewhere else
 * entirely -- so it culls its own frustum and nothing more.
 */
static inline bool R_CullBspBlock(const RenderView *view, const RenderBspBlock *block) {

  if (view->type == VIEW_PORTAL) {
    return R_CullBox(view, block->visibleBounds);
  }

  return block->query->result == 0;
}

/**
 * @brief Draws one material stage for a BSP draw batch.
 */
static void R_DrawBspDrawElementsMaterialStage(const RenderView *view,
                                               const RenderEntity *entity,
                                               const RenderBspDrawElements *draw,
                                               const RenderStage *stage,
                                               bool depthWrite,
                                               RenderPass *pass) {

  RenderMaterialUniforms uniforms;
  R_MaterialUniforms(draw->material, draw->surface, &uniforms);

  SDL_GPUTexture *texture, *textureNext;
  if (!R_StageUniforms(view, entity, draw, stage, &uniforms, &texture, &textureNext)) {
    return;
  }

  GraphicsPipeline *pipeline = R_DrawBspMaterialStagePipeline(stage->cm->blend.src, stage->cm->blend.dest, depthWrite);
  $(pass, bindPipeline, pipeline);

  $(pass, bindFragmentSamplers, R_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = texture, .sampler = r_bsp_draw.repeatSampler->sampler },
    { .texture = textureNext, .sampler = r_bsp_draw.repeatSampler->sampler },
  }, 2);

  $(pass->commands, pushUniformData, BSP_UNIFORMS_MATERIAL, &uniforms, sizeof(uniforms));

  const Uint32 firstIndex = (Uint32) ((uintptr_t) draw->elements / sizeof(uint32_t));
  $(pass, drawIndexedPrimitives, draw->numElements, 1, firstIndex, 0, 0);

  r_stats->bspTriangles += draw->numElements / 3;
}

/**
 * @brief Draws all active material stages for a BSP draw batch.
 */
static void R_DrawBspDrawElementsMaterialStages(const RenderView *view,
                                                const RenderEntity *entity,
                                                const RenderBspDrawElements *draw,
                                                bool depthWrite,
                                                RenderPass *pass) {

  const RenderMaterial *material = draw->material;
  if (!(material->cm->stageFlags & STAGE_DRAW)) {
    return;
  }

  if (draw->material != r_bsp_draw.material || draw->surface != r_bsp_draw.surface) {
    r_bsp_draw.material = draw->material;
    r_bsp_draw.surface = draw->surface;

    $(pass, bindFragmentSamplers, R_SAMPLER_MATERIAL, &(SDL_GPUTextureSamplerBinding) {
      .texture = draw->material->texture->texture->texture,
      .sampler = r_bsp_draw.repeatSampler->sampler,
    }, 1);
  }

  R_PushBspPortalLayer(view, draw, pass);

  for (const RenderStage *stage = material->stages; stage; stage = stage->next) {

    if (!(stage->cm->flags & STAGE_DRAW)) {
      continue;
    }

    R_DrawBspDrawElementsMaterialStage(view, entity, draw, stage, depthWrite, pass);
  }
}

/**
 * @brief Draws material stages for a BSP inline model entity.
 */
static void R_DrawBspEntityMaterialStages(const RenderView *view, const RenderEntity *entity, RenderPass *pass) {

  RenderBspUniformLocals locals = {
    .model = entity->matrix,
  };

  const RenderBspInlineModel *in = entity->model->bspInline;

  if (!IS_WORLDSPAWN(entity->model)) {
    memcpy(&locals.activeDynamicLights, &entity->activeDynamicLights, sizeof(locals.activeDynamicLights));
    R_PushBspUniformLocals(&locals, pass);
  }

  const RenderBspBlock *block = in->blocks;
  for (int32_t i = 0; i < in->numBlocks; i++, block++) {

    if (IS_WORLDSPAWN(entity->model)) {

      if (R_CullBspBlock(view, block)) {
        continue;
      }

      memcpy(&locals.activeDynamicLights, &block->activeDynamicLights, sizeof(locals.activeDynamicLights));
      R_PushBspUniformLocals(&locals, pass);
    }

    const RenderBspDrawElements *draw = block->drawElements;
    for (int32_t j = 0; j < block->numDrawElements; j++, draw++) {

      if (draw->surface & (SURF_SKY | SURF_MASK_BLEND)) {
        continue;
      }

      R_DrawBspDrawElementsMaterialStages(view, entity, draw, true, pass);
    }
  }
}

/**
 * @brief Draws the opaque draw elements in a BSP block.
 */
static void R_DrawOpaqueBspBlock(const RenderView *view, const RenderBspBlock *block, RenderPass *pass) {

  const RenderBspDrawElements *draw = block->drawElements;
  for (int32_t j = 0; j < block->numDrawElements; j++, draw++) {

    if (draw->surface & (SURF_SKY | SURF_MASK_BLEND | SURF_ALPHA_TEST)) {
      continue;
    }

    R_BindBspDrawElements(view, draw, r_bsp_draw.opaquePipeline, pass);

    const Uint32 firstIndex = (Uint32) ((uintptr_t) draw->elements / sizeof(uint32_t));

    if (!(draw->surface & SURF_MATERIAL)) {
      $(pass, drawIndexedPrimitives, draw->numElements, 1, firstIndex, 0, 0);
      r_stats->bspTriangles += draw->numElements / 3;
    }

    r_stats->bspDrawElements++;
  }
}

/**
 * @brief Draws the alpha-tested draw elements in a BSP block.
 */
static void R_DrawAlphaTestBspBlock(const RenderView *view, const RenderBspBlock *block, RenderPass *pass) {

  const RenderBspDrawElements *draw = block->drawElements;
  for (int32_t j = 0; j < block->numDrawElements; j++, draw++) {

    if (!(draw->surface & SURF_ALPHA_TEST) || (draw->surface & SURF_MASK_BLEND)) {
      continue;
    }

    R_BindBspDrawElements(view, draw, NULL, pass);

    const Uint32 firstIndex = (Uint32) ((uintptr_t) draw->elements / sizeof(uint32_t));

    if (!(draw->surface & SURF_MATERIAL)) {
      $(pass, drawIndexedPrimitives, draw->numElements, 1, firstIndex, 0, 0);
      r_stats->bspTriangles += draw->numElements / 3;
    }

    r_stats->bspDrawElements++;
  }
}

/**
 * @brief Draws opaque geometry for a BSP inline model entity.
 */
static void R_DrawOpaqueBspEntity(const RenderView *view, const RenderEntity *entity, RenderPass *pass) {

  RenderBspUniformLocals locals = {
    .model = entity->matrix,
  };

  const RenderBspInlineModel *in = entity->model->bspInline;

  if (!IS_WORLDSPAWN(entity->model)) {
    memcpy(&locals.activeDynamicLights, &entity->activeDynamicLights, sizeof(locals.activeDynamicLights));
    R_PushBspUniformLocals(&locals, pass);
  }

  const RenderBspBlock *block = in->blocks;
  for (int32_t i = 0; i < in->numBlocks; i++, block++) {

    if (IS_WORLDSPAWN(entity->model)) {

      if (R_CullBspBlock(view, block)) {
        r_stats->blocksOccluded++;
        continue;
      }

      r_stats->blocksVisible++;

      memcpy(&locals.activeDynamicLights, &block->activeDynamicLights, sizeof(locals.activeDynamicLights));
      R_PushBspUniformLocals(&locals, pass);
    }

    R_DrawOpaqueBspBlock(view, block, pass);
  }

  r_stats->bspInlineModels++;
}

/**
 * @brief Draws alpha-tested geometry for a BSP inline model entity.
 */
static void R_DrawAlphaTestBspEntity(const RenderView *view, const RenderEntity *entity, RenderPass *pass) {

  RenderBspUniformLocals locals = {
    .model = entity->matrix,
  };

  const RenderBspInlineModel *in = entity->model->bspInline;

  if (!IS_WORLDSPAWN(entity->model)) {
    memcpy(&locals.activeDynamicLights, &entity->activeDynamicLights, sizeof(locals.activeDynamicLights));
    R_PushBspUniformLocals(&locals, pass);
  }

  const RenderBspBlock *block = in->blocks;
  for (int32_t i = 0; i < in->numBlocks; i++, block++) {

    if (IS_WORLDSPAWN(entity->model)) {

      if (R_CullBspBlock(view, block)) {
        continue;
      }

      memcpy(&locals.activeDynamicLights, &block->activeDynamicLights, sizeof(locals.activeDynamicLights));
      R_PushBspUniformLocals(&locals, pass);
    }

    R_DrawAlphaTestBspBlock(view, block, pass);
  }
}


/**
 * @brief Draws opaque, alpha-tested, and material-stage BSP inline model geometry.
 */
void R_DrawOpaqueBspEntities(const RenderView *view, RenderPass *pass) {

  assert(r_models.world);

  R_DrawSky(view, pass);

  const RenderBspModel *bsp = r_models.world->bsp;
  Framebuffer *framebuffer = view->framebuffer;

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  $(pass->commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

  r_bsp_draw.material = NULL;

  $(pass, bindPipeline, r_bsp_draw.opaquePipeline);
  $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = bsp->vertexBuffer->buffer }, 1);
  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) { .buffer = bsp->elementsBuffer->buffer }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  $(pass, bindVertexSamplers, BSP_VERTEX_SAMPLER_VOXEL_CAUSTICS, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = bsp->voxels.caustics->texture->texture, .sampler = r_bsp_draw.clampSampler->sampler },
    { .texture = bsp->voxels.occlusion->texture->texture, .sampler = r_bsp_draw.clampSampler->sampler },
    { .texture = bsp->sky->texture->texture, .sampler = r_bsp_draw.clampSampler->sampler },
  }, 3);

  $(pass, bindFragmentSamplers, R_SAMPLER_SHADOW_ATLAS_0, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_shadow_atlas.textures[0]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[1]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[2]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[3]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[4]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[5]->texture, .sampler = r_shadow_atlas.sampler->sampler },
  }, 6);

  $(pass, bindFragmentSamplers, R_SAMPLER_VOXEL_CAUSTICS, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = bsp->voxels.caustics->texture->texture, .sampler = r_bsp_draw.clampSampler->sampler },
    { .texture = bsp->voxels.occlusion->texture->texture, .sampler = r_bsp_draw.clampSampler->sampler },
    { .texture = bsp->sky->texture->texture, .sampler = r_bsp_draw.clampSampler->sampler },
  }, 3);

  $(pass, bindFragmentSamplers, R_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_context.nullTexture->texture, .sampler = r_bsp_draw.repeatSampler->sampler },
    { .texture = r_context.nullTexture->texture, .sampler = r_bsp_draw.repeatSampler->sampler },
  }, 2);

  $(pass, bindFragmentSamplers, BSP_SAMPLER_WARP, &(SDL_GPUTextureSamplerBinding) {
    .texture = r_bsp_draw.warpTexture->texture,
    .sampler = r_bsp_draw.repeatSampler->sampler,
  }, 1);

  $(pass, bindFragmentSamplers, BSP_SAMPLER_PORTAL, &(SDL_GPUTextureSamplerBinding) {
    .texture = R_PortalTexture(view),
    .sampler = r_bsp_draw.clampSampler->sampler,
  }, 1);

  SDL_GPUBuffer *storage[] = {
    r_lights.bspBuffer->buffer,
    r_lights.dynamicBuffer->buffer,
    bsp->voxels.lightDataBuffer->buffer,
    bsp->voxels.lightIndicesBuffer ? bsp->voxels.lightIndicesBuffer->buffer : r_lights.voxelFallbackBuffer->buffer,
  };
  $(pass, bindFragmentStorageBuffers, R_STORAGE_BSP_LIGHTS, storage, R_STORAGE_MATERIAL_TOTAL);
  $(pass, bindVertexStorageBuffers, R_STORAGE_BSP_LIGHTS, storage, R_STORAGE_MATERIAL_TOTAL);

  const RenderEntity *e = view->entities;
  for (int32_t i = 0; i < view->numEntities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    if (e->effects & EF_NO_DRAW) {
      continue;
    }

    if (!IS_WORLDSPAWN(e->model) && R_CullEntity(view, e)) {
      r_stats->entitiesOccluded++;
      continue;
    }

    R_DrawOpaqueBspEntity(view, e, pass);
  }

  r_bsp_draw.material = NULL;

  $(pass, bindPipeline, r_bsp_draw.alphaTestPipeline);

  e = view->entities;
  for (int32_t i = 0; i < view->numEntities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    if (e->effects & EF_NO_DRAW) {
      continue;
    }

    if (!IS_WORLDSPAWN(e->model) && R_CullEntity(view, e)) {
      continue;
    }

    R_DrawAlphaTestBspEntity(view, e, pass);
  }

  if (r_draw_material_stages->integer) {

    r_bsp_draw.material = NULL;

    e = view->entities;
    for (int32_t i = 0; i < view->numEntities; i++, e++) {

      if (!IS_BSP_INLINE_MODEL(e->model)) {
        continue;
      }

      if (e->effects & EF_NO_DRAW) {
        continue;
      }

      if (!IS_WORLDSPAWN(e->model) && R_CullEntity(view, e)) {
        continue;
      }

      R_DrawBspEntityMaterialStages(view, e, pass);
    }
  }
}

/**
 * @brief Draws the translucent draw elements in a BSP block.
 */
static void R_DrawBlendBspBlock(const RenderView *view, const RenderEntity *entity, const RenderBspBlock *block, RenderPass *pass) {

  const RenderBspDrawElements *draw = block->drawElements;
  for (int32_t j = 0; j < block->numDrawElements; j++, draw++) {

    if (!(draw->surface & SURF_MASK_BLEND) || (draw->surface & SURF_SKY)) {
      continue;
    }

    R_BindBspDrawElements(view, draw, r_bsp_draw.blendPipeline, pass);

    const Uint32 firstIndex = (Uint32) ((uintptr_t) draw->elements / sizeof(uint32_t));
    $(pass, drawIndexedPrimitives, draw->numElements, 1, firstIndex, 0, 0);

    r_stats->bspTriangles += draw->numElements / 3;
    r_stats->bspDrawElements++;

    if (r_draw_material_stages->integer) {
      R_DrawBspDrawElementsMaterialStages(view, entity, draw, false, pass);

      r_bsp_draw.material = NULL;
    }
  }
}

/**
 * @brief Draws translucent geometry for a BSP inline model entity.
 */
static void R_DrawBlendBspEntity(const RenderView *view, const RenderEntity *entity, RenderPass *pass) {

  RenderBspUniformLocals locals = {
    .model = entity->matrix,
  };

  const RenderBspInlineModel *in = entity->model->bspInline;

  if (!IS_WORLDSPAWN(entity->model)) {
    memcpy(&locals.activeDynamicLights, &entity->activeDynamicLights, sizeof(locals.activeDynamicLights));
    R_PushBspUniformLocals(&locals, pass);
  }

  const RenderBspBlock *block = in->blocks;
  for (int32_t i = 0; i < in->numBlocks; i++, block++) {

    if (!(block->surface & SURF_MASK_BLEND)) {
      continue;
    }

    if (IS_WORLDSPAWN(entity->model)) {

      if (R_CullBspBlock(view, block)) {
        continue;
      }

      memcpy(&locals.activeDynamicLights, &block->activeDynamicLights, sizeof(locals.activeDynamicLights));
      R_PushBspUniformLocals(&locals, pass);
    }

    R_DrawBlendBspBlock(view, entity, block, pass);
  }
}

/**
 * @brief Draws translucent BSP inline model geometry.
 */
void R_DrawBlendBspEntities(const RenderView *view, RenderPass *pass) {

  assert(r_models.world);

  const RenderBspModel *bsp = r_models.world->bsp;

  Framebuffer *framebuffer = view->framebuffer;

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  $(pass->commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

  r_bsp_draw.material = NULL;

  $(pass, bindPipeline, r_bsp_draw.blendPipeline);
  $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = bsp->vertexBuffer->buffer }, 1);
  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) { .buffer = bsp->elementsBuffer->buffer }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  $(pass, bindVertexSamplers, BSP_VERTEX_SAMPLER_VOXEL_CAUSTICS, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = bsp->voxels.caustics->texture->texture, .sampler = r_bsp_draw.clampSampler->sampler },
    { .texture = bsp->voxels.occlusion->texture->texture, .sampler = r_bsp_draw.clampSampler->sampler },
    { .texture = bsp->sky->texture->texture, .sampler = r_bsp_draw.clampSampler->sampler },
  }, 3);

  $(pass, bindFragmentSamplers, R_SAMPLER_SHADOW_ATLAS_0, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_shadow_atlas.textures[0]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[1]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[2]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[3]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[4]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[5]->texture, .sampler = r_shadow_atlas.sampler->sampler },
  }, 6);

  $(pass, bindFragmentSamplers, R_SAMPLER_VOXEL_CAUSTICS, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = bsp->voxels.caustics->texture->texture, .sampler = r_bsp_draw.clampSampler->sampler },
    { .texture = bsp->voxels.occlusion->texture->texture, .sampler = r_bsp_draw.clampSampler->sampler },
    { .texture = bsp->sky->texture->texture, .sampler = r_bsp_draw.clampSampler->sampler },
  }, 3);

  $(pass, bindFragmentSamplers, R_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_context.nullTexture->texture, .sampler = r_bsp_draw.repeatSampler->sampler },
    { .texture = r_context.nullTexture->texture, .sampler = r_bsp_draw.repeatSampler->sampler },
  }, 2);

  $(pass, bindFragmentSamplers, BSP_SAMPLER_WARP, &(SDL_GPUTextureSamplerBinding) {
    .texture = r_bsp_draw.warpTexture->texture,
    .sampler = r_bsp_draw.repeatSampler->sampler,
  }, 1);

  $(pass, bindFragmentSamplers, BSP_SAMPLER_PORTAL, &(SDL_GPUTextureSamplerBinding) {
    .texture = R_PortalTexture(view),
    .sampler = r_bsp_draw.clampSampler->sampler,
  }, 1);

  SDL_GPUBuffer *storage[] = {
    r_lights.bspBuffer->buffer,
    r_lights.dynamicBuffer->buffer,
    bsp->voxels.lightDataBuffer->buffer,
    bsp->voxels.lightIndicesBuffer ? bsp->voxels.lightIndicesBuffer->buffer : r_lights.voxelFallbackBuffer->buffer,
  };
  $(pass, bindFragmentStorageBuffers, R_STORAGE_BSP_LIGHTS, storage, R_STORAGE_MATERIAL_TOTAL);
  $(pass, bindVertexStorageBuffers, R_STORAGE_BSP_LIGHTS, storage, R_STORAGE_MATERIAL_TOTAL);

  const RenderEntity *e = view->entities;
  for (int32_t i = 0; i < view->numEntities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    if (e->effects & EF_NO_DRAW) {
      continue;
    }

    if (!IS_WORLDSPAWN(e->model) && R_CullEntity(view, e)) {
      continue;
    }

    R_DrawBlendBspEntity(view, e, pass);
  }
}

/**
 * @brief Creates the BSP draw pipelines and samplers.
 */
void R_InitBspPipeline(void) {

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/bsp_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_samplers = BSP_NUM_VERTEX_SAMPLERS,
    .num_storage_buffers = R_STORAGE_MATERIAL_TOTAL,
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
      .pitch = sizeof(RenderBspVertex),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    },
    .num_vertex_buffers = 1,
    .vertex_attributes = (SDL_GPUVertexAttribute[]) {
      {
        .location = 0,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .offset = offsetof(RenderBspVertex, position),
      },
      {
        .location = 1,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .offset = offsetof(RenderBspVertex, normal),
      },
      {
        .location = 2,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .offset = offsetof(RenderBspVertex, tangent),
      },
      {
        .location = 3,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .offset = offsetof(RenderBspVertex, bitangent),
      },
      {
        .location = 4,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
        .offset = offsetof(RenderBspVertex, diffusemap),
      },
      {
        .location = 5,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
        .offset = offsetof(RenderBspVertex, color),
      },
    },
    .num_vertex_attributes = 6,
  };

  SDL_GPUColorTargetDescription colorTargets[2] = {
    { .format = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT, .blend_state = GPU_BlendStateOpaque },
    { .format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT, .blend_state = GPU_BlendStateOpaque },
  };

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = colorTargets,
    .num_color_targets = 2,
    .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
    .has_depth_stencil_target = true,
  };

  r_bsp_draw.opaquePipeline = $(r_context.device, createGraphicsPipeline, &info);

  Shader *alphaTestFragmentShader = $(r_context.device, loadShader, "shaders/bsp_fs_alpha_test", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = BSP_NUM_SAMPLERS,
    .num_storage_buffers = R_STORAGE_MATERIAL_TOTAL,
    .num_uniform_buffers = BSP_NUM_UNIFORMS,
  });

  info.fragment_shader = alphaTestFragmentShader->shader;
  r_bsp_draw.alphaTestPipeline = $(r_context.device, createGraphicsPipeline, &info);
  release(alphaTestFragmentShader);

  info.fragment_shader = fragmentShader->shader;
  colorTargets[0].blend_state = GPU_BlendStateAlpha;
  colorTargets[1].blend_state = (SDL_GPUColorTargetBlendState) {
    .enable_color_write_mask = true, .color_write_mask = 0,
  };
  info.depth_stencil_state.enable_depth_write = false;
  r_bsp_draw.blendPipeline = $(r_context.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);

  r_bsp_draw.repeatSampler = $(r_context.device, createSamplerLinearRepeat);
  r_bsp_draw.clampSampler = $(r_context.device, createSamplerLinearClamp);

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

  r_bsp_draw.warpTexture = $(r_context.device, createTexture, &(SDL_GPUTextureCreateInfo) {
    .type = SDL_GPU_TEXTURETYPE_2D,
    .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
    .width = WARP_IMAGE_SIZE,
    .height = WARP_IMAGE_SIZE,
    .layer_count_or_depth = 1,
    .num_levels = 5,
    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
  }, data);
  #undef WARP_IMAGE_SIZE

  CommandBuffer *commands = $(r_context.device, acquireCommandBuffer);
  $(commands, generateMipmaps, r_bsp_draw.warpTexture->texture);
  $(commands, submit);
  release(commands);
}

/**
 * @brief Releases the BSP draw pipelines and samplers.
 */
void R_ShutdownBspPipeline(void) {
  r_bsp_draw.opaquePipeline = release(r_bsp_draw.opaquePipeline);
  r_bsp_draw.alphaTestPipeline = release(r_bsp_draw.alphaTestPipeline);
  r_bsp_draw.blendPipeline = release(r_bsp_draw.blendPipeline);
  r_bsp_draw.repeatSampler = release(r_bsp_draw.repeatSampler);
  r_bsp_draw.clampSampler = release(r_bsp_draw.clampSampler);
  r_bsp_draw.warpTexture = release(r_bsp_draw.warpTexture);

  for (int32_t i = 0; i < r_bsp_draw.numStagePipelines; i++) {
    r_bsp_draw.stagePipelines[i].pipeline = release(r_bsp_draw.stagePipelines[i].pipeline);
  }
  
  r_bsp_draw.numStagePipelines = 0;
}

/**
 * @brief Rebuilds the BSP draw pipelines and samplers.
 */
void R_UpdateBspPipeline(void) {
  R_ShutdownBspPipeline();
  R_InitBspPipeline();
}
