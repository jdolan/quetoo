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
 * @brief The shadow depth pass pipeline (shadow_vs/shadow_fs).
 * @remarks TODO(#864): BSP world casters only for now; mesh casters, temporal
 * caching, and the per-light entity culling are ported in later increments.
 */
static GraphicsPipeline *r_shadow_pipeline;

/**
 * @brief The six cube-face view matrices (light at origin), matching r_shadow.c
 * on the GL path and the face UV mapping in light.glsl.
 */
static mat4_t r_shadow_light_view[6];

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
 * @brief Computes the tile origin (in atlas pixels) for a light index and face.
 */
static void R_ShadowAtlasTile(int32_t light_index, int32_t face, int32_t *x, int32_t *y) {

  const int32_t local_index = light_index % r_shadow_atlas.lights_per_layer;
  const int32_t light_col = local_index % r_shadow_atlas.lights_per_row;
  const int32_t light_row = local_index / r_shadow_atlas.lights_per_row;
  const int32_t face_col = face % 3;
  const int32_t face_row = face / 3;
  const int32_t ts = r_shadow_atlas.tile_size;

  *x = (light_col * 3 + face_col) * ts;
  *y = (light_row * 2 + face_row) * ts;
}

/**
 * @brief Renders shadow depth maps for all visible, shadow-casting lights into
 * the shadow atlas. One depth-only render pass per array layer clears the layer
 * and renders each of its lights' six cube faces into their tiles.
 */
void R_DrawShadows(const r_view_t *view) {

  if (!r_shadow_pipeline || !r_models.world) {
    return;
  }

  CommandBuffer *commands = r_device.device->commands;
  if (!commands) {
    return;
  }

  const r_bsp_model_t *bsp = r_models.world->bsp;
  if (!bsp->vertex_buffer) {
    return;
  }

  r_shadow_atlas.frame_count++;

  const int32_t ts = r_shadow_atlas.tile_size;

  for (int32_t layer = 0; layer < r_shadow_atlas.num_layers; layer++) {

    // Depth-only pass: each layer stores linear distance-from-light (cleared to
    // 1.0 = far), sampled later through a comparison sampler.
    const SDL_GPUDepthStencilTargetInfo depth = {
      .texture = r_shadow_atlas.texture->texture,
      .clear_depth = 1.f,
      .load_op = SDL_GPU_LOADOP_CLEAR,
      .store_op = SDL_GPU_STOREOP_STORE,
      .layer = (Uint8) layer,
    };

    RenderPass *pass = $(commands, beginRenderPass, NULL, 0, &depth);

    $(pass, bindPipeline, r_shadow_pipeline);

    // BSP geometry is static: bind the position buffer to both frame slots.
    $(pass, bindVertexBuffers, 0, (SDL_GPUBufferBinding[]) {
      { .buffer = bsp->vertex_buffer->buffer },
      { .buffer = bsp->vertex_buffer->buffer },
    }, 2);

    $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) {
      .buffer = bsp->elements_buffer->buffer
    }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    $(commands, pushVertexUniformData, 0, &r_uniforms.block, sizeof(r_uniforms.block));

    const r_light_t *l = view->lights;
    for (int32_t i = 0; i < view->num_lights; i++, l++) {

      if (i / r_shadow_atlas.lights_per_layer != layer) {
        continue;
      }

      if (l->flags & R_LIGHT_NO_SHADOW) {
        continue;
      }

      // Perf guard (no shadow caching yet): only render shadow maps for lights
      // whose bounds are within the view frustum.
      if (R_CullBox(view, l->bounds)) {
        continue;
      }

      // World shadow geometry: static BSP lights use their precomputed element
      // subset (a compile-time optimization); dynamic lights cast the full
      // worldspawn model. TODO(#864): inline-model + mesh-entity casters.
      uint32_t firstIndex, count;
      if (l->bsp_light && l->bsp_light->num_depth_pass_elements) {
        firstIndex = (uint32_t) ((uintptr_t) l->bsp_light->depth_pass_elements / sizeof(uint32_t));
        count = (uint32_t) l->bsp_light->num_depth_pass_elements;
      } else {
        const r_bsp_inline_model_t *world = bsp->inline_models;
        firstIndex = (uint32_t) ((uintptr_t) world->depth_pass_elements / sizeof(uint32_t));
        count = (uint32_t) world->num_depth_pass_elements;
      }

      if (count == 0) {
        continue;
      }

      r_stats.lights_visible++;

      for (int32_t face = 0; face < 6; face++) {

        int32_t tx, ty;
        R_ShadowAtlasTile(i, face, &tx, &ty);

        $(pass, setViewport, &(SDL_GPUViewport) {
          .x = (float) tx, .y = (float) ty, .w = (float) ts, .h = (float) ts,
          .min_depth = 0.f, .max_depth = 1.f,
        });
        $(pass, setScissor, &(SDL_Rect) { tx, ty, ts, ts });

        const r_shadow_locals_t locals = {
          .model = Mat4_Identity(),
          .light_view = r_shadow_light_view[face],
          .light_origin = Vec3_ToVec4(l->origin, 0.f),
          .lerp = 0.f,
        };
        $(commands, pushVertexUniformData, 1, &locals, sizeof(locals));

        $(pass, drawIndexedPrimitives, count, 1, firstIndex, 0, 0);
      }
    }

    pass = release(pass);
  }
}

/**
 * @brief Computes the shadow atlas tile layout for the current tile size.
 */
static void R_InitShadowLayout(void) {

  r_shadow_atlas.tile_size = Maxi(r_shadow_tile_size->integer, 128);

  const int32_t layer_dim = Mini(8192, r_config.max_texture_size);

  r_shadow_atlas.lights_per_row = layer_dim / (3 * r_shadow_atlas.tile_size);
  if (r_shadow_atlas.lights_per_row < 2) {
    r_shadow_atlas.lights_per_row = 2;
  }
  r_shadow_atlas.lights_per_row &= ~1;

  r_shadow_atlas.lights_per_col = r_shadow_atlas.lights_per_row * 3 / 2;
  r_shadow_atlas.layer_size = r_shadow_atlas.lights_per_row * 3 * r_shadow_atlas.tile_size;

  r_shadow_atlas.lights_per_layer = r_shadow_atlas.lights_per_row * r_shadow_atlas.lights_per_col;

  // SDL_gpu forbids DEPTH_STENCIL_TARGET on array textures, so the depth atlas is
  // a single non-array 2D layer. Shadow-casting lights are therefore capped at
  // lights_per_layer for now (increment 1 bring-up); further lights get no shadow.
  r_shadow_atlas.num_layers = 1;

  Com_Verbose("   Shadow atlas: %dx%d (single 2D layer, %d lights max, %d tile size)\n",
      r_shadow_atlas.layer_size, r_shadow_atlas.layer_size,
      r_shadow_atlas.lights_per_layer, r_shadow_atlas.tile_size);
}

/**
 * @brief Initializes all shadow mapping resources: atlas texture, comparison
 * sampler, and the depth pass pipeline.
 */
void R_InitShadows(void) {

  memset(&r_shadow_atlas, 0, sizeof(r_shadow_atlas));

  R_InitShadowLayout();

  r_shadow_atlas.texture = $(r_device.device, createTexture, &(SDL_GPUTextureCreateInfo) {
    .type = SDL_GPU_TEXTURETYPE_2D,
    .format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
    .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
    .width = (Uint32) r_shadow_atlas.layer_size,
    .height = (Uint32) r_shadow_atlas.layer_size,
    .layer_count_or_depth = 1,
    .num_levels = 1,
    .sample_count = SDL_GPU_SAMPLECOUNT_1,
  }, NULL);

  // Comparison sampler (GL_COMPARE_REF_TO_TEXTURE / LEQUAL) with linear filtering
  // for hardware PCF, matching the GL shadow atlas.
  r_shadow_atlas.sampler = $(r_device.device, createSampler, &(SDL_GPUSamplerCreateInfo) {
    .min_filter = SDL_GPU_FILTER_LINEAR,
    .mag_filter = SDL_GPU_FILTER_LINEAR,
    .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
    .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
    .enable_compare = true,
  });

  Shader *vertexShader = $(r_device.device, loadShader, "shaders/shadow_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 2, // globals (binding 0) + locals (binding 1)
  });

  Shader *fragmentShader = $(r_device.device, loadShader, "shaders/shadow_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
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
      .enable_depth_clip = false, // == GL_DEPTH_CLAMP
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

  r_shadow_pipeline = $(r_device.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);

  // Cube-face view matrices, light at the origin.
  r_shadow_light_view[0] = Mat4_LookAt(Vec3_Zero(), Vec3( 1.f,  0.f,  0.f), Vec3(0.f, -1.f,  0.f));
  r_shadow_light_view[1] = Mat4_LookAt(Vec3_Zero(), Vec3(-1.f,  0.f,  0.f), Vec3(0.f, -1.f,  0.f));
  r_shadow_light_view[2] = Mat4_LookAt(Vec3_Zero(), Vec3( 0.f,  1.f,  0.f), Vec3(0.f,  0.f,  1.f));
  r_shadow_light_view[3] = Mat4_LookAt(Vec3_Zero(), Vec3( 0.f, -1.f,  0.f), Vec3(0.f,  0.f, -1.f));
  r_shadow_light_view[4] = Mat4_LookAt(Vec3_Zero(), Vec3( 0.f,  0.f,  1.f), Vec3(0.f, -1.f,  0.f));
  r_shadow_light_view[5] = Mat4_LookAt(Vec3_Zero(), Vec3( 0.f,  0.f, -1.f), Vec3(0.f, -1.f,  0.f));
}

/**
 * @brief Shuts down all shadow mapping resources.
 */
void R_ShutdownShadows(void) {

  r_shadow_pipeline = release(r_shadow_pipeline);
  r_shadow_atlas.sampler = release(r_shadow_atlas.sampler);
  r_shadow_atlas.texture = release(r_shadow_atlas.texture);
}
