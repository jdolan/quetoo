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
 * @brief The sky program's own binding map, mirroring shaders/sky_vs.glsl and
 * sky_fs.glsl. Not shared with any other pipeline.
 */
enum {
  SKY_SAMPLER_SKY,
  SKY_SAMPLER_STAGE,
  SKY_SAMPLER_STAGE_NEXT,
  SKY_NUM_SAMPLERS,
};

enum {
  SKY_UNIFORMS_GLOBALS,
  SKY_UNIFORMS_MATERIAL, // material + stage params combined -- see material.glsl
};
#define SKY_NUM_UNIFORMS 2 // same count/slot for both stages

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
   * @brief A 1x1 black fallback cubemap, returned by R_SkyTexture when no sky is
   * loaded, so the lit pass always has a valid cube to bind at the ambient slot.
   */
  Texture *fallback;
} r_sky;

/**
 * @brief The sky program (sky_vs/sky_fs): its graphics pipeline, samplers, and
 * the lazily-built cache of material-stage blend pipelines (azimuthally
 * projected clouds, moons, ...) -- drawn via the same shader as the base
 * cubemap window, a runtime branch on material.flags, not a pipeline swap.
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
  } stages[MAX_STAGE_PIPELINES];

  int32_t num_stages;
} r_sky_draw;

/**
 * @brief Returns the sky pipeline for the given material-stage blend function,
 * creating and caching it on first use. Wraps the same sky_vs/sky_fs shader as
 * the base pipeline (a stage draw is a runtime branch, not a shader swap);
 * only the blend state differs, which SDL_gpu bakes into the pipeline.
 */
static GraphicsPipeline *R_SkyStagePipeline(cm_blend_t src, cm_blend_t dest) {

  for (int32_t i = 0; i < r_sky_draw.num_stages; i++) {
    if (r_sky_draw.stages[i].src == src && r_sky_draw.stages[i].dest == dest) {
      return r_sky_draw.stages[i].pipeline;
    }
  }

  if (r_sky_draw.num_stages == MAX_STAGE_PIPELINES) {
    return NULL;
  }

  const Framebuffer *framebuffer = r_context.device->framebuffer;

  const SDL_GPUBlendFactor s = R_BlendFactor(src);
  const SDL_GPUBlendFactor d = R_BlendFactor(dest);

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = r_scene_samples;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;

  // Stages blend over the already-drawn cubemap window; test depth but don't write it.
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
    .color_target_descriptions = &(SDL_GPUColorTargetDescription) {
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
    .num_color_targets = 1,
    .depth_stencil_format = framebuffer->depthFormat,
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

  r_sky_draw.stages[r_sky_draw.num_stages] = (typeof(r_sky_draw.stages[0])) {
    .src = src, .dest = dest, .pipeline = pipeline,
  };
  return r_sky_draw.stages[r_sky_draw.num_stages++].pipeline;
}

/**
 * @brief Binds the stage uniforms and textures and draws a single material stage
 * of a sky draw-elements batch, layered over the already-drawn cubemap window.
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

  $(pass, bindFragmentSamplers, SKY_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
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
 * @brief Renders all sky surfaces of the world model as a window into the sky
 * cubemap. Drawn after the opaque world so it fills only unoccluded sky texels.
 */
void R_DrawSky(const r_view_t *view, const r_bsp_model_t *bsp) {

  if (!r_sky_draw.pipeline || !r_sky.image || !r_sky.image->texture || !r_sky.image->texture->texture) {
    return;
  }

  if (!bsp->vertex_buffer) {
    return;
  }

  CommandBuffer *commands = r_context.device->commands;
  if (!commands) {
    return;
  }

  if (!view->framebuffer) {
    return;
  }

  Framebuffer *framebuffer = view->framebuffer;

  // Composite over the opaque scene, depth-testing against it (LOAD, no clear) so
  // sky surfaces occluded by world geometry are discarded.
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

  $(commands, pushVertexUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

  $(pass, bindPipeline, r_sky_draw.pipeline);
  $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = bsp->vertex_buffer->buffer }, 1);
  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) { .buffer = bsp->elements_buffer->buffer }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  $(pass, bindFragmentSamplers, SKY_SAMPLER_SKY, &(SDL_GPUTextureSamplerBinding) {
    .texture = r_sky.image->texture->texture,
    .sampler = r_sky_draw.sampler->sampler,
  }, 1);

  // Stage/stage-next: the base cubemap window never samples these (sky_fs's
  // STAGE_NONE branch doesn't touch them), but the shared shader declares
  // them, so SDL_gpu requires them bound regardless.
  $(pass, bindFragmentSamplers, SKY_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
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

      // Re-bind the base pipeline: a preceding surface's material stages may
      // have left a stage (blend) pipeline bound (see R_DrawBspBlockOpaque).
      $(pass, bindPipeline, r_sky_draw.pipeline);

      // R_MaterialUniforms zeroes the whole struct, so the stage fields land
      // at their STAGE_NONE default, undoing any stage a preceding surface
      // left pushed.
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

  pass = release(pass);
}

/**
 * @brief Returns the sky cubemap for image-based ambient in the lit pass, or a
 * black fallback cube when no sky is loaded, so the ambient sampler is always bound.
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

  const Framebuffer *framebuffer = r_context.device->framebuffer;

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
    .color_target_descriptions = &(SDL_GPUColorTargetDescription) {
      .format = R_SCENE_COLOR_FORMAT,
      .blend_state = GPU_BlendStateOpaque,
    },
    .num_color_targets = 1,
    .depth_stencil_format = framebuffer->depthFormat,
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

  // A 1x1 black fallback cube (six faces) for the lit ambient sampler.
  r_sky.fallback = $(r_context.device, createSolidColorTexture, SDL_GPU_TEXTURETYPE_CUBE, 6, 0x00000000);
}

/**
 * @brief Shuts down the sky subsystem, releasing the pipeline and sampler.
 */
void R_ShutdownSky(void) {

  r_sky_draw.pipeline = release(r_sky_draw.pipeline);
  r_sky_draw.sampler = release(r_sky_draw.sampler);
  r_sky_draw.stage_sampler = release(r_sky_draw.stage_sampler);

  for (int32_t i = 0; i < r_sky_draw.num_stages; i++) {
    r_sky_draw.stages[i].pipeline = release(r_sky_draw.stages[i].pipeline);
  }
  r_sky_draw.num_stages = 0;

  r_sky.fallback = release(r_sky.fallback);
}

/**
 * @brief Rebuilds the sky pipeline and sampler in place, for pipeline-bound
 * cvar changes (r_antialias, r_anisotropy, ...) that would otherwise require
 * an r_restart. See R_UpdatePipelines.
 */
void R_UpdateSky(void) {
  R_ShutdownSky();
  R_InitSky();
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
