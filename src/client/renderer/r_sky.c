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
 * @brief The number of samplers used by the sky pipeline.
 */
#define SKY_NUM_SAMPLERS R_SAMPLER_MATERIAL_TOTAL

enum {
  SKY_UNIFORMS_GLOBALS,
  SKY_UNIFORMS_MATERIAL,
};
#define SKY_NUM_UNIFORMS 2

/**
 * @brief The maximum number of cached material-stage pipelines (one per blend).
 */
#define MAX_STAGE_PIPELINES 16

/**
 * @brief Sky state holding the cubemap image.
 */
static struct {

  /**
   * @brief The sky cubemap.
   */
  r_image_t *image;

  /**
   * @brief The fallback sky cubemap.
   */
  Texture *fallback;
} r_sky;

/**
 * @brief Sky rendering resources.
 */
static struct {
  /**
   * @brief The pipeline.
   */
  GraphicsPipeline *pipeline;
  
  /**
   * @brief The cubemap sampler.
   */
  Sampler *sampler;
  
  /**
   * @brief The stage sampler for parallax scrolling sky effects.
   */
  Sampler *stage_sampler;

  /**
   * @brief The stage pipelines, cached by blend operator combinations.
   */
  struct {
    cm_blend_t src, dest;
    GraphicsPipeline *pipeline;
  } stage_pipelines[MAX_STAGE_PIPELINES];

  int32_t num_stage_pipelines;
} r_sky_draw;

/**
 * @brief Returns the cached sky stage pipeline for the given blend mode.
 */
static GraphicsPipeline *R_SkyStagePipeline(cm_blend_t src, cm_blend_t dest) {

  for (int32_t i = 0; i < r_sky_draw.num_stage_pipelines; i++) {
    if (r_sky_draw.stage_pipelines[i].src == src && r_sky_draw.stage_pipelines[i].dest == dest) {
      return r_sky_draw.stage_pipelines[i].pipeline;
    }
  }

  if (r_sky_draw.num_stage_pipelines == MAX_STAGE_PIPELINES) {
    return NULL;
  }

  const SDL_GPUBlendFactor s = R_BlendFactor(src);
  const SDL_GPUBlendFactor d = R_BlendFactor(dest);

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = r_scene_samples;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;

  info.depth_stencil_state.enable_depth_write = false;

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = &(SDL_GPUVertexBufferDescription) {
      .slot = 0,
      .pitch = sizeof(r_bsp_vertex_t),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    },
    .num_vertex_buffers = 1,
    .vertex_attributes = &(SDL_GPUVertexAttribute) {
      .location = 0,
      .buffer_slot = 0,
      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
      .offset = offsetof(r_bsp_vertex_t, position),
    },
    .num_vertex_attributes = 1,
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

  GraphicsPipeline *pipeline = $(r_context.device, loadGraphicsPipeline,
    "shaders/sky_vs", &(SDL_GPUShaderCreateInfo) {
      .stage = SDL_GPU_SHADERSTAGE_VERTEX,
      .num_uniform_buffers = SKY_NUM_UNIFORMS,
    },
    "shaders/sky_fs", &(SDL_GPUShaderCreateInfo) {
      .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
      .num_samplers = SKY_NUM_SAMPLERS,
      .num_uniform_buffers = SKY_NUM_UNIFORMS,
    },
    &info);

  r_sky_draw.stage_pipelines[r_sky_draw.num_stage_pipelines] = (typeof(r_sky_draw.stage_pipelines[0])) {
    .src = src, .dest = dest, .pipeline = pipeline,
  };

  return r_sky_draw.stage_pipelines[r_sky_draw.num_stage_pipelines++].pipeline;
}

/**
 * @brief Draws one sky material stage.
 */
static void R_DrawSkyDrawElementsMaterialStage(const r_view_t *view, RenderPass *pass, CommandBuffer *commands,
                                               const r_bsp_draw_elements_t *draw, const r_stage_t *stage) {

  r_material_uniforms_t uniforms;
  R_MaterialUniforms(draw->material, draw->surface, &uniforms);

  SDL_GPUTexture *texture, *texture_next;
  if (!R_StageUniforms(view, NULL, draw, stage, &uniforms, &texture, &texture_next)) {
    return;
  }

  GraphicsPipeline *pipeline = R_SkyStagePipeline(stage->cm->blend.src, stage->cm->blend.dest);
  if (!pipeline) {
    return;
  }

  $(pass, bindPipeline, pipeline);

  $(pass, bindFragmentSamplers, R_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = texture, .sampler = r_sky_draw.stage_sampler->sampler },
    { .texture = texture_next, .sampler = r_sky_draw.stage_sampler->sampler },
  }, 2);

  $(commands, pushUniformData, SKY_UNIFORMS_MATERIAL, &uniforms, sizeof(uniforms));

  const Uint32 firstIndex = (Uint32) ((uintptr_t) draw->elements / sizeof(uint32_t));
  $(pass, drawIndexedPrimitives, draw->num_elements, 1, firstIndex, 0, 0);

  r_stats.bsp_triangles += draw->num_elements / 3;
}

/**
 * @brief Draws all active material stages for a sky draw-elements batch.
 */
static void R_DrawSkyDrawElementsMaterialStages(const r_view_t *view, RenderPass *pass, CommandBuffer *commands,
                                                const r_bsp_draw_elements_t *draw) {

  const r_material_t *material = draw->material;
  if (!(material->cm->stage_flags & STAGE_DRAW)) {
    return;
  }

  for (const r_stage_t *stage = material->stages; stage; stage = stage->next) {

    if (!(stage->cm->flags & STAGE_DRAW)) {
      continue;
    }

    R_DrawSkyDrawElementsMaterialStage(view, pass, commands, draw, stage);
  }
}

/**
 * @brief Draws world sky surfaces with the active sky cubemap.
 */
void R_DrawSky(RenderPass *pass, const r_view_t *view) {

  if (!r_models.world) {
    return;
  }

  if (!r_sky.image) {
    return;
  }

  const r_bsp_model_t *bsp = r_models.world->bsp;

  CommandBuffer *commands = r_context.device->commands;

  Framebuffer *framebuffer = view->framebuffer;

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  $(commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

  $(pass, bindPipeline, r_sky_draw.pipeline);
  $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = bsp->vertex_buffer->buffer }, 1);
  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) { .buffer = bsp->elements_buffer->buffer }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  $(pass, bindFragmentSamplers, R_SAMPLER_MATERIAL, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_context.null_texture->texture, .sampler = r_sky_draw.stage_sampler->sampler },
    { .texture = r_shadow_atlas.textures[0]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[1]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[2]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[3]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[4]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = r_shadow_atlas.textures[5]->texture, .sampler = r_shadow_atlas.sampler->sampler },
    { .texture = bsp->voxels.caustics->texture->texture, .sampler = r_sky_draw.stage_sampler->sampler },
    { .texture = bsp->voxels.occlusion->texture->texture, .sampler = r_sky_draw.stage_sampler->sampler },
  }, 9);

  $(pass, bindFragmentSamplers, R_SAMPLER_SKY, &(SDL_GPUTextureSamplerBinding) {
    .texture = r_sky.image->texture->texture,
    .sampler = r_sky_draw.sampler->sampler,
  }, 1);

  $(pass, bindFragmentSamplers, R_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = r_context.null_texture->texture, .sampler = r_sky_draw.stage_sampler->sampler },
    { .texture = r_context.null_texture->texture, .sampler = r_sky_draw.stage_sampler->sampler },
  }, 2);

  const r_bsp_inline_model_t *world = bsp->inline_models;
  const r_bsp_block_t *block = world->blocks;
  for (int32_t i = 0; i < world->num_blocks; i++, block++) {

    if (!(block->surface & SURF_SKY)) {
      continue;
    }

    const r_bsp_draw_elements_t *draw = block->draw_elements;
    for (int32_t j = 0; j < block->num_draw_elements; j++, draw++) {

      if (!(draw->surface & SURF_SKY)) {
        continue;
      }

      if (!draw->material) {
        continue;
      }

      $(pass, bindPipeline, r_sky_draw.pipeline);

      r_material_uniforms_t material;
      R_MaterialUniforms(draw->material, draw->surface, &material);
      $(commands, pushUniformData, SKY_UNIFORMS_MATERIAL, &material, sizeof(material));

      const Uint32 firstIndex = (Uint32) ((uintptr_t) draw->elements / sizeof(uint32_t));

      $(pass, drawIndexedPrimitives, draw->num_elements, 1, firstIndex, 0, 0);

      r_stats.bsp_triangles += draw->num_elements / 3;
      r_stats.bsp_draw_elements++;

      if (r_draw_material_stages->integer) {
        R_DrawSkyDrawElementsMaterialStages(view, pass, commands, draw);
      }
    }
  }
}

/**
 * @brief Returns the current sky cubemap or the fallback cubemap.
 */
Texture *R_SkyTexture(void) {

  if (r_sky.image && r_sky.image->texture) {
    return r_sky.image->texture;
  }

  return r_sky.fallback;
}

/**
 * @brief Builds the sky pipeline (position-only vertex input) and cubemap sampler.
 */
static void R_InitSkyPipeline(void) {

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = r_scene_samples;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = &(SDL_GPUVertexBufferDescription) {
      .slot = 0,
      .pitch = sizeof(r_bsp_vertex_t),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    },
    .num_vertex_buffers = 1,
    .vertex_attributes = &(SDL_GPUVertexAttribute) {
      .location = 0,
      .buffer_slot = 0,
      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
      .offset = offsetof(r_bsp_vertex_t, position),
    },
    .num_vertex_attributes = 1,
  };

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = (SDL_GPUColorTargetDescription[]) {
      {
        .format = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT,
        .blend_state = GPU_BlendStateOpaque,
      },
      {
        .format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT,
        .blend_state = GPU_BlendStateOpaque,
      },
    },
    .num_color_targets = 2,
    .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
    .has_depth_stencil_target = true,
  };

  r_sky_draw.pipeline = $(r_context.device, loadGraphicsPipeline,
    "shaders/sky_vs", &(SDL_GPUShaderCreateInfo) {
      .stage = SDL_GPU_SHADERSTAGE_VERTEX,
      .num_uniform_buffers = SKY_NUM_UNIFORMS,
    },
    "shaders/sky_fs", &(SDL_GPUShaderCreateInfo) {
      .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
      .num_samplers = SKY_NUM_SAMPLERS,
      .num_uniform_buffers = SKY_NUM_UNIFORMS,
    },
    &info);

  r_sky_draw.sampler = $(r_context.device, createSamplerLinearClamp);
  r_sky_draw.stage_sampler = $(r_context.device, createSamplerLinearRepeat);
}

/**
 * @brief Initializes the sky subsystem and sky pipeline.
 */
void R_InitSky(void) {

  memset(&r_sky, 0, sizeof(r_sky));

  R_InitSkyPipeline();

  r_sky.fallback = $(r_context.device, createSolidColorTexture, SDL_GPU_TEXTURETYPE_CUBE, 6, 0x00000000);
}

/**
 * @brief Shuts down the sky subsystem, releasing the pipeline and sampler.
 */
void R_ShutdownSky(void) {

  r_sky_draw.pipeline = release(r_sky_draw.pipeline);
  r_sky_draw.sampler = release(r_sky_draw.sampler);
  r_sky_draw.stage_sampler = release(r_sky_draw.stage_sampler);

  for (int32_t i = 0; i < r_sky_draw.num_stage_pipelines; i++) {
    r_sky_draw.stage_pipelines[i].pipeline = release(r_sky_draw.stage_pipelines[i].pipeline);
  }
  r_sky_draw.num_stage_pipelines = 0;

  r_sky.fallback = release(r_sky.fallback);
}

/**
 * @brief Rebuilds sky pipeline resources.
 * @remarks Preserves the loaded sky cubemap across the rebuild.
 */
void R_UpdateSky(void) {

  r_image_t *image = r_sky.image;

  R_ShutdownSky();
  R_InitSky();

  r_sky.image = image;
}

/**
 * @brief Loads the sky cubemap specified in worldspawn.
 */
void R_LoadSky(void) {

  const char *name = Cm_EntityValue(Cm_Worldspawn(), "sky")->nullable_string;
  if (name) {
    r_sky.image = R_LoadImage(va("sky/%s", name), IMG_CUBEMAP);
  } else {
    r_sky.image = NULL;
  }

  if (r_sky.image == NULL) {
    if (name) {
      Com_Warn("Failed to load sky sky/%s\n", name);
    } else {
      Com_Warn("Failed to load sky (no sky key on worldspawn)\n");
    }

    r_sky.image = R_LoadImage("sky/template", IMG_CUBEMAP);
    if (r_sky.image == NULL) {
      Com_Error(ERROR_DROP, "Failed to load default sky\n");
    }
  }
}
