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
 * @brief The shadow depth pass pipeline (shadow_vs/shadow_fs) for static BSP
 * geometry (r_bsp_vertex_t layout).
 */
static GraphicsPipeline *r_shadow_pipeline;

/**
 * @brief The shadow depth pass pipeline for animated mesh casters (r_mesh_vertex_t
 * layout, two frame slots lerped in shadow_vs).
 */
static GraphicsPipeline *r_shadow_mesh_pipeline;

/**
 * @brief The shadow atlas "clear" pipeline (shadow_clear_vs/fs): draws a
 * fullscreen triangle, no vertex buffer, that unconditionally writes the
 * atlas's "far" value. Used to clear a single light's tile block via a
 * scissored draw before redrawing it -- see R_DrawShadows.
 */
static GraphicsPipeline *r_shadow_clear_pipeline;

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
 * @brief Returns true if the given entity (or any of its parents) is the source
 * of the specified light, so it should not cast a shadow from it (self-shadow).
 */
static bool R_IsLightSource(const r_light_t *light, const r_entity_t *e) {

  while (e) {
    if (light->source && light->source == e) {
      return true;
    }
    e = e->parent;
  }

  return false;
}

/**
 * @brief Returns true if only the static world casts into this light's shadow
 * (no inline BSP model entity or mesh entity caster intersects its bounds),
 * matching main's `is_shadow_cacheable`. Such a shadow never changes
 * frame-to-frame, so once rendered it can be skipped entirely until a caster
 * enters the light's bounds. Filters mirror the draw loops in R_DrawShadows
 * exactly, since any caster there invalidates the cache.
 */
static bool R_IsShadowCacheable(const r_view_t *view, const r_light_t *light) {

  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (IS_BSP_INLINE_MODEL(e->model) && !IS_WORLDSPAWN(e->model)) {

      if (e->effects & EF_NO_DRAW) {
        continue;
      }

      const r_bsp_inline_model_t *in = e->model->bsp_inline;
      if (!in->num_depth_pass_elements) {
        continue;
      }

      if (Box3_Intersects(light->bounds, e->abs_model_bounds)) {
        return false;
      }

    } else if (IS_MESH_MODEL(e->model)) {

      if (e->effects & (EF_NO_SHADOW | EF_BLEND)) {
        continue;
      }

      if (R_IsLightSource(light, e)) {
        continue;
      }

      const r_mesh_model_t *mesh = e->model->mesh;
      if (!mesh || !mesh->vertex_buffer) {
        continue;
      }

      if (Box3_Intersects(light->bounds, e->abs_model_bounds)) {
        return false;
      }
    }
  }

  return true;
}

/**
 * @brief Computes the tile origin (in atlas pixels) for a light index, within
 * whichever face texture is currently bound (see R_DrawShadows).
 */
static void R_ShadowAtlasTile(int32_t light_index, int32_t *x, int32_t *y) {

  const int32_t light_col = light_index % r_shadow_atlas.lights_per_row;
  const int32_t light_row = light_index / r_shadow_atlas.lights_per_row;
  const int32_t ts = r_shadow_atlas.tile_size;

  *x = light_col * ts;
  *y = light_row * ts;
}

/**
 * @brief Renders shadow depth maps for all visible, shadow-casting lights into
 * the shadow atlas. One depth-only render pass per cube face texture; each
 * light occupies the same tile position (light_index -> row/col) in all six.
 */
void R_DrawShadows(const r_view_t *view) {

  if (!r_shadow_pipeline || !r_models.world) {
    return;
  }

  CommandBuffer *commands = r_context.device->commands;
  if (!commands) {
    return;
  }

  const r_bsp_model_t *bsp = r_models.world->bsp;
  if (!bsp->vertex_buffer) {
    return;
  }

  r_shadow_atlas.frame_count++;

  const int32_t ts = r_shadow_atlas.tile_size;

  for (int32_t face = 0; face < 6; face++) {

    // Depth-only pass: this face's texture stores linear distance-from-light
    // (1.0 = far/no occluder), sampled later through a comparison sampler.
    // LOAD, not CLEAR: the texture packs every light's tile, and SDL_gpu's
    // load_op always applies to the whole texture, not a scissored sub-rect --
    // clearing here would wipe every light's shadow, defeating the temporal
    // cache below. Each light being (re)drawn this frame instead clears just
    // its own tile via a scissored draw with r_shadow_clear_pipeline.
    const SDL_GPUDepthStencilTargetInfo depth = {
      .texture = r_shadow_atlas.textures[face]->texture,
      .load_op = SDL_GPU_LOADOP_LOAD,
      .store_op = SDL_GPU_STOREOP_STORE,
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

      if (i >= r_shadow_atlas.lights_per_layer) {
        continue;
      }

      if (l->flags & R_LIGHT_NO_SHADOW) {
        continue;
      }

      // Only render shadow maps for lights whose bounds are within the view frustum.
      if (R_CullBox(view, l->bounds)) {
        continue;
      }

      // If only the static world casts into this light, its shadow never
      // changes; skip the redraw once it's already in the atlas from a prior
      // frame. l->shadow_cached is NULL for dynamic lights (no persistent
      // identity across frames to cache against), so they always redraw.
      const bool cacheable = R_IsShadowCacheable(view, l);
      if (cacheable && l->shadow_cached && *l->shadow_cached) {
        continue;
      }

      int32_t tx, ty;
      R_ShadowAtlasTile(i, &tx, &ty);

      // Not using the cache: clear this light's tile before redrawing it,
      // since LOAD preserves whatever was there before.
      $(pass, setViewport, &(SDL_GPUViewport) {
        .x = (float) tx, .y = (float) ty, .w = (float) ts, .h = (float) ts,
        .min_depth = 0.f, .max_depth = 1.f,
      });
      $(pass, setScissor, &(SDL_Rect) { tx, ty, ts, ts });

      $(pass, bindPipeline, r_shadow_clear_pipeline);
      $(pass, drawPrimitives, 3, 1, 0, 0);

      $(pass, bindPipeline, r_shadow_pipeline);
      $(pass, bindVertexBuffers, 0, (SDL_GPUBufferBinding[]) {
        { .buffer = bsp->vertex_buffer->buffer },
        { .buffer = bsp->vertex_buffer->buffer },
      }, 2);
      $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) {
        .buffer = bsp->elements_buffer->buffer
      }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

      // World shadow geometry: static BSP lights use their precomputed element
      // subset (a compile-time optimization); dynamic lights cast the full
      // worldspawn model.
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

      if (face == 0) {
        r_stats.lights_visible++;
      }

      const r_shadow_locals_t locals = {
        .model = Mat4_Identity(),
        .light_view = r_shadow_light_view[face],
        .light_origin = Vec3_ToVec4(l->origin, 0.f),
        .lerp = 0.f,
      };
      $(commands, pushVertexUniformData, 1, &locals, sizeof(locals));

      $(pass, drawIndexedPrimitives, count, 1, firstIndex, 0, 0);

      // Inline BSP model entity casters (doors, platforms, ...): the same
      // pipeline and vertex/index buffers as the world (already bound above),
      // but each entity carries its own transform and precomputed depth-pass
      // element subset, matching R_DrawOpaqueBspEntities' inline-entity loop.
      const r_entity_t *be = view->entities;
      for (int32_t ei = 0; ei < view->num_entities; ei++, be++) {

        if (!IS_BSP_INLINE_MODEL(be->model) || IS_WORLDSPAWN(be->model)) {
          continue;
        }

        if (be->effects & EF_NO_DRAW) {
          continue;
        }

        if (!Box3_Intersects(l->bounds, be->abs_model_bounds)) {
          continue;
        }

        const r_bsp_inline_model_t *in = be->model->bsp_inline;
        if (!in->num_depth_pass_elements) {
          continue;
        }

        const uint32_t in_first_index = (uint32_t) ((uintptr_t) in->depth_pass_elements / sizeof(uint32_t));
        const uint32_t in_count = (uint32_t) in->num_depth_pass_elements;

        const r_shadow_locals_t in_locals = {
          .model = be->matrix,
          .light_view = r_shadow_light_view[face],
          .light_origin = Vec3_ToVec4(l->origin, 0.f),
          .lerp = 0.f,
        };
        $(commands, pushVertexUniformData, 1, &in_locals, sizeof(in_locals));

        $(pass, drawIndexedPrimitives, in_count, 1, in_first_index, 0, 0);
      }

      if (face == 5 && l->shadow_cached) {
        *l->shadow_cached = cacheable;
      }
    }

    // Mesh entity casters: same face pass, but the mesh-layout pipeline (two
    // animation frame slots). Render each shadow-casting mesh entity within
    // range of each visible light, into that light's tile in this face.
    if (r_shadow_mesh_pipeline) {

      $(pass, bindPipeline, r_shadow_mesh_pipeline);
      $(commands, pushVertexUniformData, 0, &r_uniforms.block, sizeof(r_uniforms.block));

      const r_light_t *ml = view->lights;
      for (int32_t i = 0; i < view->num_lights; i++, ml++) {

        if (i >= r_shadow_atlas.lights_per_layer) {
          continue;
        }

        if (ml->flags & R_LIGHT_NO_SHADOW) {
          continue;
        }

        if (R_CullBox(view, ml->bounds)) {
          continue;
        }

        // Same cache check as the world/inline-entity loop above (the whole
        // tile was already skipped there if cacheable, so this is a no-op
        // unless this light has a mesh caster and nothing else).
        if (ml->shadow_cached && *ml->shadow_cached) {
          continue;
        }

        int32_t tx, ty;
        R_ShadowAtlasTile(i, &tx, &ty);

        const r_entity_t *e = view->entities;
        for (int32_t ei = 0; ei < view->num_entities; ei++, e++) {

          if (!IS_MESH_MODEL(e->model)) {
            continue;
          }

          // EF_NO_DRAW entities may still cast; EF_NO_SHADOW/EF_BLEND never do.
          if (e->effects & (EF_NO_SHADOW | EF_BLEND)) {
            continue;
          }

          if (R_IsLightSource(ml, e)) {
            continue;
          }

          const r_mesh_model_t *mesh = e->model->mesh;
          if (!mesh || !mesh->vertex_buffer) {
            continue;
          }

          if (!Box3_Intersects(ml->bounds, e->abs_model_bounds)) {
            continue;
          }

          $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) {
            .buffer = mesh->elements_buffer->buffer
          }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

          const uint32_t stride = sizeof(r_mesh_vertex_t);

          $(pass, setViewport, &(SDL_GPUViewport) {
            .x = (float) tx, .y = (float) ty, .w = (float) ts, .h = (float) ts,
            .min_depth = 0.f, .max_depth = 1.f,
          });
          $(pass, setScissor, &(SDL_Rect) { tx, ty, ts, ts });

          const r_shadow_locals_t locals = {
            .model = e->matrix,
            .light_view = r_shadow_light_view[face],
            .light_origin = Vec3_ToVec4(ml->origin, 0.f),
            .lerp = e->lerp,
          };
          $(commands, pushVertexUniformData, 1, &locals, sizeof(locals));

          const r_mesh_face_t *mf = mesh->faces;
          for (int32_t fi = 0; fi < mesh->num_faces; fi++, mf++) {

            const uint32_t old_offset = (uint32_t) (mf->base_vertex + e->old_frame * mf->num_vertexes) * stride;
            const uint32_t cur_offset = (uint32_t) (mf->base_vertex + e->frame * mf->num_vertexes) * stride;

            $(pass, bindVertexBuffers, 0, (SDL_GPUBufferBinding[]) {
              { .buffer = mesh->vertex_buffer->buffer, .offset = old_offset },
              { .buffer = mesh->vertex_buffer->buffer, .offset = cur_offset },
            }, 2);

            const uint32_t firstIndex = (uint32_t) ((uintptr_t) mf->indices / sizeof(uint32_t));
            $(pass, drawIndexedPrimitives, mf->num_elements, 1, firstIndex, 0, 0);
          }
        }
      }
    }

    pass = release(pass);
  }
}

/**
 * @brief Computes the shadow atlas tile layout for the current tile size.
 * @details SDL_gpu forbids DEPTH_STENCIL_TARGET on array textures (asserted in
 * its own shared validation code, not a driver quirk -- confirmed the hard
 * way), so multi-layer capacity isn't an option. Instead, each of the six cube
 * faces gets its own plain 2D depth texture (see R_InitShadows), and each is
 * sized to exactly 32 tiles per side, scaling with tile_size rather than
 * capping it -- a light needs only one tile per face texture (not the old
 * 3-wide x 2-tall per-light block), so this is a square grid capped only by
 * the device's max 2D texture dimension.
 */
static void R_InitShadowLayout(void) {

  r_shadow_atlas.tile_size = Maxi(r_shadow_tile_size->integer, 128);

  const int32_t layer_dim = Mini(32 * r_shadow_atlas.tile_size, r_config.max_texture_size);

  r_shadow_atlas.lights_per_row = Maxi(1, layer_dim / r_shadow_atlas.tile_size);
  r_shadow_atlas.layer_size = r_shadow_atlas.lights_per_row * r_shadow_atlas.tile_size;
  r_shadow_atlas.lights_per_layer = r_shadow_atlas.lights_per_row * r_shadow_atlas.lights_per_row;

  Com_Verbose("   Shadow atlas: 6x %dx%d (%d lights max, %d tile size)\n",
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

  // One plain 2D depth texture per cube face (SDL_gpu forbids array depth
  // targets); all six share the same tile grid, so a light's tile lands at
  // the same (row, col) in every face.
  for (int32_t face = 0; face < 6; face++) {
    r_shadow_atlas.textures[face] = $(r_context.device, createTexture, &(SDL_GPUTextureCreateInfo) {
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
      .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
      .width = (Uint32) r_shadow_atlas.layer_size,
      .height = (Uint32) r_shadow_atlas.layer_size,
      .layer_count_or_depth = 1,
      .num_levels = 1,
      .sample_count = SDL_GPU_SAMPLECOUNT_1,
    }, NULL);
  }

  // Comparison sampler (GL_COMPARE_REF_TO_TEXTURE / LEQUAL) with linear filtering
  // for hardware PCF, matching the GL shadow atlas.
  r_shadow_atlas.sampler = $(r_context.device, createSamplerShadowCompare);

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/shadow_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 2, // globals (binding 0) + locals (binding 1)
  });

  Shader *fragmentShader = $(r_context.device, loadShader, "shaders/shadow_fs", &(SDL_GPUShaderCreateInfo) {
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

  r_shadow_pipeline = $(r_context.device, createGraphicsPipeline, &info);

  // A mesh-layout variant (r_mesh_vertex_t stride) for animated mesh casters.
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

  r_shadow_mesh_pipeline = $(r_context.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);

  // The atlas "clear" pipeline: no vertex input (a fullscreen triangle from
  // gl_VertexIndex alone), depth test/compare disabled so the write always
  // succeeds regardless of the scissored rect's existing contents.
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

  r_shadow_clear_pipeline = $(r_context.device, loadGraphicsPipeline,
    "shaders/shadow_clear_vs", &(SDL_GPUShaderCreateInfo) {
      .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    },
    "shaders/shadow_clear_fs", &(SDL_GPUShaderCreateInfo) {
      .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    },
    &clear_info);

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
  r_shadow_mesh_pipeline = release(r_shadow_mesh_pipeline);
  r_shadow_clear_pipeline = release(r_shadow_clear_pipeline);
  r_shadow_atlas.sampler = release(r_shadow_atlas.sampler);

  for (int32_t face = 0; face < 6; face++) {
    r_shadow_atlas.textures[face] = release(r_shadow_atlas.textures[face]);
  }
}
