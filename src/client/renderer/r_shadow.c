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

r_shadow_atlas_t r_shadow_atlas;

/**
 * @brief Per-face shadow locals, pushed to vertex uniform slot 1.
 */
typedef struct {
  mat4_t model;
  mat4_t light_view;
  vec4_t light_origin;
  float lerp;
} r_shadow_locals_t;

/**
 * @brief Shadow draw pipelines, samplers, and per-face transient state.
 */
static struct {

  /**
   * @brief The static-geometry (BSP) shadow pipeline.
   */
  GraphicsPipeline *pipeline;

  /**
   * @brief The mesh shadow pipeline.
   */
  GraphicsPipeline *mesh_pipeline;

  /**
   * @brief The mesh shadow pipeline for alpha-tested materials, discarding
   * transparent texels so foliage, fences and grates cast holes.
   */
  GraphicsPipeline *mesh_alpha_test_pipeline;

  /**
   * @brief The shadow-atlas clear pipeline.
   */
  GraphicsPipeline *clear_pipeline;

  /**
   * @brief The sampler used to bind alpha-tested materials' diffuse textures.
   */
  Sampler *repeat_sampler;

  /**
   * @brief Cube-face view matrices for shadow lights.
   */
  mat4_t light_view[6];

  /**
   * @brief The cube face currently being rendered.
   */
  int32_t face;
} r_shadow_draw;

/**
 * @brief Determines whether an entity hierarchy is the source of a light.
 */
static bool R_IsLightSource(const r_light_t *light, const r_entity_t *e) {

  while (e) {
    if (light->source && light->source == e->id) {
      return true;
    }
    e = e->parent;
  }

  return false;
}

/**
 * @brief Tests whether an entity's shadow bounds are outside the view.
 */
static bool R_CullLightEntity(const r_view_t *view, const r_light_t *light, const r_entity_t *e) {

  vec3_t corners[8];
  Box3_ToPoints(e->abs_model_bounds, corners);

  box3_t shadow_bounds = e->abs_model_bounds;
  for (int32_t i = 0; i < 8; i++) {
    const vec3_t to_corner = Vec3_Subtract(corners[i], light->origin);
    const vec3_t dir = Vec3_Normalize(to_corner);
    shadow_bounds = Box3_Append(shadow_bounds, Vec3_Fmaf(light->origin, light->radius, dir));
  }

  shadow_bounds = Box3_Expand(shadow_bounds, 32.f);

  return R_CulludeBox(view, shadow_bounds);
}

/**
 * @brief Collects shadow-casting entities for each visible light.
 */
void R_UpdateShadows(r_view_t *view) {

  r_light_t *l = view->lights;
  for (int32_t i = 0; i < view->num_lights; i++, l++) {

    l->num_entities = 0;

    if (l->flags & R_LIGHT_NO_SHADOW) {
      continue;
    }

    if (l->occluded) {
      continue;
    }

    const vec3_t closest_point = Box3_ClampPoint(l->bounds, view->origin);
    const float dist = Vec3_Distance(closest_point, view->origin);

    // If the light's nearest bound is beyond the lighting LOD cutoff, no
    // fragment this frame will be close enough to sample its shadow at all.
    if (!r_shadows->value || dist > r_lighting_distance->value + LIGHTING_LOD_BLEND_DIST) {
      continue;
    }

    const r_entity_t *e = view->entities;
    for (int32_t j = 0; j < view->num_entities; j++, e++) {

      if (e->model == NULL) {
        continue;
      }

      if (IS_WORLDSPAWN((e->model))) {
        continue;
      }

      if (e->effects & (EF_NO_SHADOW | EF_BLEND)) {
        continue;
      }
      
      if (R_IsLightSource(l, e)) {
        continue;
      }

      if (!Box3_Intersects(l->bounds, e->abs_model_bounds)) {
        continue;
      }

      if (R_CullLightEntity(view, l, e)) {
        continue;
      }

      l->entities[l->num_entities++] = e;
    }

    if (l->num_entities == 0 && l->shadow_cached && *l->shadow_cached) {
      r_stats.lights_cached++;
    }
  }
}

/**
 * @brief Clears shadow atlas tiles for lights that will be redrawn.
 */
void R_ClearShadows(const r_view_t *view) {

  CommandBuffer *commands = r_context.device->commands;

  const int32_t ts = r_shadow_atlas.tile_size;

  for (int32_t face = 0; face < 6; face++) {

    const SDL_GPUDepthStencilTargetInfo depth = {
      .texture = r_shadow_atlas.textures[face]->texture,
      .load_op = SDL_GPU_LOADOP_LOAD,
      .store_op = SDL_GPU_STOREOP_STORE,
    };

    RenderPass *pass = $(commands, beginRenderPass, NULL, 0, &depth);

    $(pass, bindPipeline, r_shadow_draw.clear_pipeline);

    const r_light_t *l = view->lights;
    for (int32_t i = 0; i < view->num_lights; i++, l++) {

      if (l->num_entities == 0 && l->shadow_cached && *l->shadow_cached) {
        continue;
      }

      $(pass, setViewport, &(SDL_GPUViewport) {
        .x = l->tile.x, .y = l->tile.y, .w = (float) ts, .h = (float) ts,
        .min_depth = 0.f, .max_depth = 1.f,
      });
      $(pass, setScissor, &(SDL_Rect) { (int32_t) l->tile.x, (int32_t) l->tile.y, ts, ts });

      $(pass, drawPrimitives, 3, 1, 0, 0);
    }

    pass = release(pass);
  }
}

/**
 * @brief Draws BSP world and inline-model shadow geometry for one light,
 * on the cube face currently tracked by r_shadow_draw.
 */
static void R_DrawBspEntityShadows(const r_view_t *view, const r_light_t *l, RenderPass *pass) {

  const mat4_t light_view = r_shadow_draw.light_view[r_shadow_draw.face];
  const int32_t ts = r_shadow_atlas.tile_size;

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = l->tile.x, .y = l->tile.y, .w = (float) ts, .h = (float) ts,
    .min_depth = 0.f, .max_depth = 1.f,
  });
  $(pass, setScissor, &(SDL_Rect) { (int32_t) l->tile.x, (int32_t) l->tile.y, ts, ts });

  const r_bsp_model_t *bsp = r_models.world->bsp;

  uint32_t count, first_index;
  if (l->bsp_light && l->bsp_light->num_depth_pass_elements) {
    count = (uint32_t) l->bsp_light->num_depth_pass_elements;
    first_index = (uint32_t) ((uintptr_t) l->bsp_light->depth_pass_elements / sizeof(uint32_t));
  } else {
    const r_bsp_inline_model_t *world = bsp->inline_models;
    count = (uint32_t) world->num_depth_pass_elements;
    first_index = (uint32_t) ((uintptr_t) world->depth_pass_elements / sizeof(uint32_t));
  }

  $(r_context.device->commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &(const r_shadow_locals_t) {
    .model = Mat4_Identity(),
    .light_view = light_view,
    .light_origin = Vec3_ToVec4(l->origin, l->radius),
    .lerp = 0.f,
  }, sizeof(r_shadow_locals_t));

  $(pass, drawIndexedPrimitives, count, 1, first_index, 0, 0);

  for (int32_t j = 0; j < l->num_entities; j++) {

    const r_entity_t *e = l->entities[j];

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    const r_bsp_inline_model_t *in = e->model->bsp_inline;

    const uint32_t in_first_index = (uint32_t) ((uintptr_t) in->depth_pass_elements / sizeof(uint32_t));
    const uint32_t in_count = (uint32_t) in->num_depth_pass_elements;

    if (!in_count) {
      continue;
    }

    $(r_context.device->commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &(const r_shadow_locals_t) {
      .model = e->matrix,
      .light_view = light_view,
      .light_origin = Vec3_ToVec4(l->origin, l->radius),
      .lerp = 0.f,
    }, sizeof(r_shadow_locals_t));

    $(pass, drawIndexedPrimitives, in_count, 1, in_first_index, 0, 0);
  }
}

/**
 * @brief Draws mesh-entity shadow geometry for one light, on the cube face
 * currently tracked by r_shadow_draw. Opaque and alpha-tested faces use
 * separate pipelines; translucent faces cast no shadow.
 */
static void R_DrawMeshEntityShadows(const r_view_t *view, const r_light_t *l, RenderPass *pass) {

  const mat4_t light_view = r_shadow_draw.light_view[r_shadow_draw.face];
  const int32_t ts = r_shadow_atlas.tile_size;

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = l->tile.x, .y = l->tile.y, .w = (float) ts, .h = (float) ts,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  $(pass, setScissor, &(SDL_Rect) { (int32_t) l->tile.x, (int32_t) l->tile.y, ts, ts });

  for (int32_t j = 0; j < l->num_entities; j++) {
    const r_entity_t *e = l->entities[j];

    if (!IS_MESH_MODEL(e->model)) {
      continue;
    }

    const r_mesh_model_t *mesh = e->model->mesh;

    $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) {
      .buffer = mesh->elements_buffer->buffer
    }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    const uint32_t stride = sizeof(r_mesh_vertex_t);

    $(pass->commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &(const r_shadow_locals_t) {
      .model = e->matrix,
      .light_view = light_view,
      .light_origin = Vec3_ToVec4(l->origin, l->radius),
      .lerp = e->lerp,
    }, sizeof(r_shadow_locals_t));

    GraphicsPipeline *pipeline = NULL;

    const r_mesh_face_t *mf = mesh->faces;
    for (int32_t fi = 0; fi < mesh->num_faces; fi++, mf++) {

      const r_material_t *material = e->skins[fi] ?: mf->material;

      if (material->cm->surface & SURF_MASK_BLEND) {
        continue;
      }

      const bool alpha_test = (material->cm->surface & SURF_ALPHA_TEST) != 0;

      GraphicsPipeline *face_pipeline = alpha_test ? r_shadow_draw.mesh_alpha_test_pipeline : r_shadow_draw.mesh_pipeline;
      if (pipeline != face_pipeline) {
        pipeline = face_pipeline;
        $(pass, bindPipeline, pipeline);
      }

      if (alpha_test) {
        $(pass, bindFragmentSamplers, 0, &(SDL_GPUTextureSamplerBinding) {
          .texture = material->texture->texture->texture,
          .sampler = r_shadow_draw.repeat_sampler->sampler,
        }, 1);

        const float alpha_test_value = material->cm->alpha_test * r_alpha_test->value;
        $(pass->commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, &alpha_test_value, sizeof(alpha_test_value));
      }

      const uint32_t old_offset = (uint32_t) (mf->base_vertex + e->old_frame * mf->num_vertexes) * stride;
      const uint32_t cur_offset = (uint32_t) (mf->base_vertex + e->frame * mf->num_vertexes) * stride;

      $(pass, bindVertexBuffers, 0, (SDL_GPUBufferBinding[]) {
        { .buffer = mesh->vertex_buffer->buffer, .offset = old_offset },
        { .buffer = mesh->vertex_buffer->buffer, .offset = cur_offset },
      }, 2);

      const uint32_t first_index = (uint32_t) ((uintptr_t) mf->indices / sizeof(uint32_t));
      $(pass, drawIndexedPrimitives, mf->num_elements, 1, first_index, 0, 0);
    }
  }
}

/**
 * @brief Renders shadow maps for lights that need a redraw.
 */
void R_DrawShadows(const r_view_t *view) {

  CommandBuffer *commands = r_context.device->commands;

  const r_bsp_model_t *bsp = r_models.world->bsp;

  for (int32_t face = 0; face < 6; face++) {

    r_shadow_draw.face = face;

    const SDL_GPUDepthStencilTargetInfo depth = {
      .texture = r_shadow_atlas.textures[face]->texture,
      .load_op = SDL_GPU_LOADOP_LOAD,
      .store_op = SDL_GPU_STOREOP_STORE,
    };

    RenderPass *pass = $(commands, beginRenderPass, NULL, 0, &depth);

    $(pass, bindPipeline, r_shadow_draw.pipeline);

    $(pass, bindVertexBuffers, 0, (SDL_GPUBufferBinding[]) {
      { .buffer = bsp->vertex_buffer->buffer },
      { .buffer = bsp->vertex_buffer->buffer },
    }, 2);

    $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) {
      .buffer = bsp->elements_buffer->buffer
    }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    $(commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));


    const r_light_t *l = view->lights;
    for (int32_t i = 0; i < view->num_lights; i++, l++) {

      if (l->num_entities == 0 && l->shadow_cached && *l->shadow_cached) {
        continue;
      }

      R_DrawBspEntityShadows(view, l, pass);
    }

    $(commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

    l = view->lights;
    for (int32_t i = 0; i < view->num_lights; i++, l++) {

      if (l->num_entities == 0) {
        continue;
      }

      R_DrawMeshEntityShadows(view, l, pass);
    }

    pass = release(pass);
  }

  const r_light_t *l = view->lights;
  for (int32_t i = 0; i < view->num_lights; i++, l++) {
    if (l->shadow_cached) {
      *l->shadow_cached = l->num_entities == 0;
    }
  }
}

/**
 * @brief Initializes shadow atlas textures, samplers, and pipelines.
 */
void R_InitShadows(void) {

  memset(&r_shadow_atlas, 0, sizeof(r_shadow_atlas));

  r_shadow_atlas.tile_size = Maxi(r_shadow_tile_size->integer, 128);

  const Uint32 layer_size = SHADOW_ATLAS_LIGHTS_PER_ROW * (Uint32) r_shadow_atlas.tile_size;

  Com_Verbose("   Shadow atlas: 6x %dx%d (%d tile size)\n",
      layer_size, layer_size, r_shadow_atlas.tile_size);

  for (int32_t face = 0; face < 6; face++) {
    r_shadow_atlas.textures[face] = $(r_context.device, createTexture, &(SDL_GPUTextureCreateInfo) {
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
      .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
      .width = layer_size,
      .height = layer_size,
      .layer_count_or_depth = 1,
      .num_levels = 1,
      .sample_count = SDL_GPU_SAMPLECOUNT_1,
    }, NULL);
  }

  r_shadow_atlas.sampler = $(r_context.device, createSamplerShadowCompare);

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/shadow_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 2,
  });

  Shader *fragmentShader = $(r_context.device, loadShader, "shaders/shadow_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_uniform_buffers = 1,
  });

  SDL_GPUGraphicsPipelineCreateInfo info = {
    .vertex_shader = vertexShader->shader,
    .fragment_shader = fragmentShader->shader,
    .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
    .vertex_input_state = {
      .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {
        { .slot = 0, .pitch = sizeof(r_bsp_vertex_t), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
        { .slot = 1, .pitch = sizeof(r_bsp_vertex_t), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
      },
      .num_vertex_buffers = 2,
      .vertex_attributes = (SDL_GPUVertexAttribute[]) {
        { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_bsp_vertex_t, position) },
        { .location = 1, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_bsp_vertex_t, position) },
      },
      .num_vertex_attributes = 2,
    },
    .rasterizer_state = {
      .fill_mode = SDL_GPU_FILLMODE_FILL,
      .cull_mode = SDL_GPU_CULLMODE_NONE,
      .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
      .enable_depth_clip = false,
    },
    .depth_stencil_state = {
      .compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
      .enable_depth_test = true,
      .enable_depth_write = true,
    },
    .target_info = {
      .num_color_targets = 0,
      .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
      .has_depth_stencil_target = true,
    },
  };

  r_shadow_draw.pipeline = $(r_context.device, createGraphicsPipeline, &info);

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {
      { .slot = 0, .pitch = sizeof(r_mesh_vertex_t), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
      { .slot = 1, .pitch = sizeof(r_mesh_vertex_t), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
    },
    .num_vertex_buffers = 2,
    .vertex_attributes = (SDL_GPUVertexAttribute[]) {
      { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, position) },
      { .location = 1, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, position) },
    },
    .num_vertex_attributes = 2,
  };

  r_shadow_draw.mesh_pipeline = $(r_context.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);

  Shader *alphaTestVertexShader = $(r_context.device, loadShader, "shaders/shadow_vs_alpha_test", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 2,
  });

  Shader *alphaTestFragmentShader = $(r_context.device, loadShader, "shaders/shadow_fs_alpha_test", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = 1,
    .num_uniform_buffers = 2,
  });

  info.vertex_shader = alphaTestVertexShader->shader;
  info.fragment_shader = alphaTestFragmentShader->shader;
  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {
      { .slot = 0, .pitch = sizeof(r_mesh_vertex_t), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
      { .slot = 1, .pitch = sizeof(r_mesh_vertex_t), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
    },
    .num_vertex_buffers = 2,
    .vertex_attributes = (SDL_GPUVertexAttribute[]) {
      { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, position) },
      { .location = 1, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_mesh_vertex_t, position) },
      { .location = 2, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = offsetof(r_mesh_vertex_t, diffusemap) },
    },
    .num_vertex_attributes = 3,
  };

  r_shadow_draw.mesh_alpha_test_pipeline = $(r_context.device, createGraphicsPipeline, &info);

  release(alphaTestVertexShader);
  release(alphaTestFragmentShader);

  r_shadow_draw.repeat_sampler = $(r_context.device, createSamplerLinearRepeat);

  SDL_GPUGraphicsPipelineCreateInfo clear_info = {
    .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
    .rasterizer_state = {
      .fill_mode = SDL_GPU_FILLMODE_FILL,
      .cull_mode = SDL_GPU_CULLMODE_NONE,
      .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
    },
    .depth_stencil_state = {
      .compare_op = SDL_GPU_COMPAREOP_ALWAYS,
      .enable_depth_test = true,
      .enable_depth_write = true,
    },
    .target_info = {
      .num_color_targets = 0,
      .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
      .has_depth_stencil_target = true,
    },
  };

  r_shadow_draw.clear_pipeline = $(r_context.device, loadGraphicsPipeline,
    "shaders/shadow_clear_vs", &(SDL_GPUShaderCreateInfo) {
      .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    },
    "shaders/shadow_clear_fs", &(SDL_GPUShaderCreateInfo) {
      .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    },
    &clear_info);

  r_shadow_draw.light_view[0] = Mat4_LookAt(Vec3_Zero(), Vec3( 1.f,  0.f,  0.f), Vec3(0.f, -1.f,  0.f));
  r_shadow_draw.light_view[1] = Mat4_LookAt(Vec3_Zero(), Vec3(-1.f,  0.f,  0.f), Vec3(0.f, -1.f,  0.f));
  r_shadow_draw.light_view[2] = Mat4_LookAt(Vec3_Zero(), Vec3( 0.f,  1.f,  0.f), Vec3(0.f,  0.f,  1.f));
  r_shadow_draw.light_view[3] = Mat4_LookAt(Vec3_Zero(), Vec3( 0.f, -1.f,  0.f), Vec3(0.f,  0.f, -1.f));
  r_shadow_draw.light_view[4] = Mat4_LookAt(Vec3_Zero(), Vec3( 0.f,  0.f,  1.f), Vec3(0.f, -1.f,  0.f));
  r_shadow_draw.light_view[5] = Mat4_LookAt(Vec3_Zero(), Vec3( 0.f,  0.f, -1.f), Vec3(0.f, -1.f,  0.f));
}

/**
 * @brief Shuts down all shadow mapping resources.
 */
void R_ShutdownShadows(void) {

  r_shadow_draw.pipeline = release(r_shadow_draw.pipeline);
  r_shadow_draw.mesh_pipeline = release(r_shadow_draw.mesh_pipeline);
  r_shadow_draw.mesh_alpha_test_pipeline = release(r_shadow_draw.mesh_alpha_test_pipeline);
  r_shadow_draw.clear_pipeline = release(r_shadow_draw.clear_pipeline);
  r_shadow_draw.repeat_sampler = release(r_shadow_draw.repeat_sampler);
  r_shadow_atlas.sampler = release(r_shadow_atlas.sampler);

  for (int32_t face = 0; face < 6; face++) {
    r_shadow_atlas.textures[face] = release(r_shadow_atlas.textures[face]);
  }
}
