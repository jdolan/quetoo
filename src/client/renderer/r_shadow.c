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
 * @brief Per-face shadow uniform locals, pushed to vertex uniform slot 1.
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
   * @brief If a given BSP light's only shadow caster is worldspawn, its shadowmap may be cached indefinitely.
   */
  bool cache[MAX_LIGHTS];

  /**
   * @brief The opaque BSP shadow pipeline.
   */
  GraphicsPipeline *bsp_opaque_pipeline;

  /**
   * @brief The BSP shadow pipeline for alpha-tested materials, discarding
   * transparent texels so foliage, fences and grates cast holes.
   */
  GraphicsPipeline *bsp_alpha_test_pipeline;

  /**
   * @brief The mesh shadow pipeline.
   */
  GraphicsPipeline *mesh_opaque_pipeline;

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
 * @brief Collects shadow-casting entities for one light, and marks its
 * shadow cache dirty if any non-worldspawn caster is present.
 */
void R_UpdateLightEntities(const r_view_t *view, r_light_t *l, int32_t index) {

  l->num_entities = 0;

  if (l->flags & R_LIGHT_NO_SHADOW) {
    return;
  }

  if (l->occluded) {
    return;
  }

  const vec3_t closest_point = Box3_ClampPoint(l->bounds, view->origin);
  const float dist = Vec3_Distance(closest_point, view->origin);

  if (dist > r_lighting_distance->value + LIGHTING_LOD_BLEND_DIST) {
    return;
  }

  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (e->model == NULL) {
      continue;
    }

    if (e->effects & (EF_NO_SHADOW | EF_BLEND)) {
      continue;
    }

    if (IS_MESH_MODEL(e->model) && !r_shadows->value) {
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

    if (!IS_WORLDSPAWN(e->model)) {
      r_shadow_draw.cache[index] = false;
    }
  }

  if (r_shadow_draw.cache[index]) {
    r_stats.lights_cached++;
  }
}

/**
 * @brief Draws one BSP draw elements entry for shadow casting, binding the alpha-test pipeline
 * and diffuse texture if the entry has a material, or the opaque pipeline otherwise.
 * @return The pipeline now bound, so the caller can avoid redundant re-binds across calls.
 */
static GraphicsPipeline *R_DrawBspDrawElementsShadow(RenderPass *pass, const r_bsp_draw_elements_t *draw, GraphicsPipeline *pipeline) {

  GraphicsPipeline *draw_pipeline = r_shadow_draw.bsp_opaque_pipeline;

  if (draw->material) {

    if (draw->material->cm->surface & (SURF_SKY | SURF_MASK_BLEND | SURF_MATERIAL)) {
      return pipeline;
    }

    draw_pipeline = r_shadow_draw.bsp_alpha_test_pipeline;
  }

  if (pipeline != draw_pipeline) {
    pipeline = draw_pipeline;
    $(pass, bindPipeline, pipeline);
  }

  if (draw->material) {
    $(pass, bindFragmentSamplers, 0, &(SDL_GPUTextureSamplerBinding) {
      .texture = draw->material->texture->texture->texture,
      .sampler = r_shadow_draw.repeat_sampler->sampler,
    }, 1);

    const float alpha_test_value = draw->material->cm->alpha_test * r_alpha_test->value;
    $(pass->commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, &alpha_test_value, sizeof(alpha_test_value));
  }

  const uint32_t first_index = (uint32_t) ((uintptr_t) draw->elements / sizeof(uint32_t));
  $(pass, drawIndexedPrimitives, draw->num_elements, 1, first_index, 0, 0);

  return pipeline;
}

/**
 * @brief Draws the given BSP draw elements array, handling pipeline toggles for alpha-test.
 */
static void R_DrawBspDrawElementsShadows(RenderPass *pass, const r_bsp_draw_elements_t *draw, int32_t count) {

  GraphicsPipeline *pipeline = r_shadow_draw.bsp_opaque_pipeline;

  for (int32_t i = 0; i < count; i++, draw++) {
    pipeline = R_DrawBspDrawElementsShadow(pass, draw, pipeline);
  }

  if (pipeline != r_shadow_draw.bsp_opaque_pipeline) {
    $(pass, bindPipeline, r_shadow_draw.bsp_opaque_pipeline);
  }
}

/**
 * @brief Draws BSP inline-model shadow geometry for one light and entity.
 */
static void R_DrawBspEntityShadows(const r_light_t *l, const r_entity_t *e, RenderPass *pass) {

  const r_bsp_inline_model_t *in = e->model->bsp_inline;

  if (!in->num_depth_pass_elements) {
    return;
  }

  $(r_context.device->commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &(const r_shadow_locals_t) {
    .model = e->matrix,
    .light_view = r_shadow_draw.light_view[r_shadow_draw.face],
    .light_origin = Vec3_ToVec4(l->origin, l->radius),
    .lerp = 0.f,
  }, sizeof(r_shadow_locals_t));

  if (IS_WORLDSPAWN(e->model) && l->bsp_light && l->bsp_light->num_draw_elements) {
    R_DrawBspDrawElementsShadows(pass, l->bsp_light->draw_elements, l->bsp_light->num_draw_elements);
  } else {
    R_DrawBspDrawElementsShadows(pass, in->depth_pass_elements, in->num_depth_pass_elements);
  }
}

/**
 * @brief Draws BSP inline-model shadow geometry for one light to the current shadow tile.
 */
static void R_DrawBspEntitiesShadows(const r_view_t *view, const r_light_t *l, RenderPass *pass) {

  const Uint32 ts = r_shadow_atlas.tile_size;

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = l->tile.x,
    .y = l->tile.y,
    .w = (float) ts,
    .h = (float) ts,
    .min_depth = 0.f,
    .max_depth = 1.f,
  });

  $(pass, setScissor, &(SDL_Rect) { (int32_t) l->tile.x, (int32_t) l->tile.y, ts, ts });

  for (int32_t i = 0; i < l->num_entities; i++) {

    const r_entity_t *e = l->entities[i];

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    R_DrawBspEntityShadows(l, e, pass);
  }
}

/**
 * @brief
 */
static void R_DrawMeshEntityShadow(const r_view_t *view, const r_light_t *l, const r_entity_t *e, RenderPass *pass) {

  const r_mesh_model_t *mesh = e->model->mesh;

  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) {
    .buffer = mesh->elements_buffer->buffer
  }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  const uint32_t stride = sizeof(r_mesh_vertex_t);

  $(pass->commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &(const r_shadow_locals_t) {
    .model = e->matrix,
    .light_view =  r_shadow_draw.light_view[r_shadow_draw.face],
    .light_origin = Vec3_ToVec4(l->origin, l->radius),
    .lerp = e->lerp,
  }, sizeof(r_shadow_locals_t));

  const r_mesh_face_t *face = mesh->faces;
  for (int32_t i = 0; i < mesh->num_faces; i++, face++) {

    const r_material_t *material = e->skins[i] ?: face->material;

    if (material->cm->surface & SURF_MASK_BLEND) {
      continue;
    }

    GraphicsPipeline *pipeline = r_shadow_draw.mesh_opaque_pipeline;

    if (material->cm->surface & SURF_ALPHA_TEST) {
      pipeline = r_shadow_draw.mesh_alpha_test_pipeline;

      $(pass, bindFragmentSamplers, 0, &(SDL_GPUTextureSamplerBinding) {
        .texture = material->texture->texture->texture,
        .sampler = r_shadow_draw.repeat_sampler->sampler,
      }, 1);

      const float alpha_test_value = material->cm->alpha_test * r_alpha_test->value;
      $(pass->commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, &alpha_test_value, sizeof(alpha_test_value));
    }

    $(pass, bindPipeline, pipeline);

    const uint32_t old_offset = (uint32_t) (face->base_vertex + e->old_frame * face->num_vertexes) * stride;
    const uint32_t cur_offset = (uint32_t) (face->base_vertex + e->frame * face->num_vertexes) * stride;

    $(pass, bindVertexBuffers, 0, (SDL_GPUBufferBinding[]) {
      { .buffer = mesh->vertex_buffer->buffer, .offset = old_offset },
      { .buffer = mesh->vertex_buffer->buffer, .offset = cur_offset },
    }, 2);

    const uint32_t first_index = (uint32_t) ((uintptr_t) face->indices / sizeof(uint32_t));
    $(pass, drawIndexedPrimitives, face->num_elements, 1, first_index, 0, 0);
  }
}

/**
 * @brief Draws mesh-entity shadow geometry for one light, on the cube face
 * currently tracked by r_shadow_draw. Opaque and alpha-tested faces use
 * separate pipelines; translucent faces cast no shadow.
 */
static void R_DrawMeshEntitiesShadows(const r_view_t *view, const r_light_t *l, RenderPass *pass) {

  const Uint32 ts = r_shadow_atlas.tile_size;

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = l->tile.x,
    .y = l->tile.y,
    .w = (float) ts,
    .h = (float) ts,
    .min_depth = 0.f,
    .max_depth = 1.f,
  });

  $(pass, setScissor, &(SDL_Rect) { (int32_t) l->tile.x, (int32_t) l->tile.y, ts, ts });

  for (int32_t j = 0; j < l->num_entities; j++) {
    const r_entity_t *e = l->entities[j];

    if (!IS_MESH_MODEL(e->model)) {
      continue;
    }

    R_DrawMeshEntityShadow(view, l, e, pass);
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

    const Uint32 ts = r_shadow_atlas.tile_size;

    $(pass, bindPipeline, r_shadow_draw.clear_pipeline);

    const r_light_t *l = view->lights;
    for (int32_t i = 0; i < view->num_lights; i++, l++) {

      if (l->occluded) {
        continue;
      }

      if (r_shadow_draw.cache[i]) {
        continue;
      }

      $(pass, setViewport, &(SDL_GPUViewport) {
        .x = l->tile.x,
        .y = l->tile.y,
        .w = (float) ts,
        .h = (float) ts,
        .min_depth = 0.f,
        .max_depth = 1.f,
      });

      $(pass, setScissor, &(SDL_Rect) { (int32_t) l->tile.x, (int32_t) l->tile.y, ts, ts });

      $(pass, drawPrimitives, 3, 1, 0, 0);
    }

    $(pass, bindPipeline, r_shadow_draw.bsp_opaque_pipeline);

    $(pass, bindVertexBuffers, 0, (SDL_GPUBufferBinding[]) {
      { .buffer = bsp->vertex_buffer->buffer },
      { .buffer = bsp->vertex_buffer->buffer },
    }, 2);

    $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) {
      .buffer = bsp->elements_buffer->buffer
    }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    $(commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

    l = view->lights;
    for (int32_t i = 0; i < view->num_lights; i++, l++) {

      if (l->occluded) {
        continue;
      }

      if (r_shadow_draw.cache[i]) {
        continue;
      }

      R_DrawBspEntitiesShadows(view, l, pass);
    }

    l = view->lights;
    for (int32_t i = 0; i < view->num_lights; i++, l++) {

      if (l->occluded) {
        continue;
      }

      if (r_shadow_draw.cache[i]) {
        continue;
      }

      R_DrawMeshEntitiesShadows(view, l, pass);
    }

    pass = release(pass);
  }

  const r_light_t *l = view->lights;
  for (int32_t i = 0; i < view->num_lights; i++, l++) {
    if (l->bsp_light && l->num_entities == 1 && IS_WORLDSPAWN(l->entities[0]->model)) {
      r_shadow_draw.cache[i] = true;
    }
  }
}

/**
 * @brief Initializes shadow atlas textures, samplers, and pipelines.
 */
void R_InitShadows(void) {

  memset(&r_shadow_draw, 0, sizeof(r_shadow_draw));

  memset(&r_shadow_atlas, 0, sizeof(r_shadow_atlas));

  r_shadow_atlas.tile_size = Maxi(r_shadow_tile_size->integer, 128);

  const Uint32 atlas_size = SHADOW_ATLAS_LIGHTS_PER_ROW * r_shadow_atlas.tile_size;

  for (int32_t face = 0; face < 6; face++) {
    r_shadow_atlas.textures[face] = $(r_context.device, createTexture, &(SDL_GPUTextureCreateInfo) {
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
      .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
      .width = atlas_size,
      .height = atlas_size,
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

  r_shadow_draw.bsp_opaque_pipeline = $(r_context.device, createGraphicsPipeline, &info);

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

  r_shadow_draw.mesh_opaque_pipeline = $(r_context.device, createGraphicsPipeline, &info);

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

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {
      { .slot = 0, .pitch = sizeof(r_bsp_vertex_t), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
      { .slot = 1, .pitch = sizeof(r_bsp_vertex_t), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
    },
    .num_vertex_buffers = 2,
    .vertex_attributes = (SDL_GPUVertexAttribute[]) {
      { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_bsp_vertex_t, position) },
      { .location = 1, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_bsp_vertex_t, position) },
      { .location = 2, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = offsetof(r_bsp_vertex_t, diffusemap) },
    },
    .num_vertex_attributes = 3,
  };

  r_shadow_draw.bsp_alpha_test_pipeline = $(r_context.device, createGraphicsPipeline, &info);

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

  r_shadow_draw.bsp_opaque_pipeline = release(r_shadow_draw.bsp_opaque_pipeline);
  r_shadow_draw.bsp_alpha_test_pipeline = release(r_shadow_draw.bsp_alpha_test_pipeline);
  r_shadow_draw.mesh_opaque_pipeline = release(r_shadow_draw.mesh_opaque_pipeline);
  r_shadow_draw.mesh_alpha_test_pipeline = release(r_shadow_draw.mesh_alpha_test_pipeline);
  r_shadow_draw.clear_pipeline = release(r_shadow_draw.clear_pipeline);
  r_shadow_draw.repeat_sampler = release(r_shadow_draw.repeat_sampler);
  r_shadow_atlas.sampler = release(r_shadow_atlas.sampler);

  for (int32_t face = 0; face < 6; face++) {
    r_shadow_atlas.textures[face] = release(r_shadow_atlas.textures[face]);
  }
}
