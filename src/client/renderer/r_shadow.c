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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "r_local.h"

RenderShadowAtlas rShadowAtlas;

/**
 * @brief Per-face shadow uniform locals, pushed to vertex uniform slot 1.
 */
typedef struct {
  Mat4 model;
  Mat4 lightView;
  Vec4 lightOrigin;
  float lerp;
} RenderShadowLocals;

/**
 * @brief Shadow draw pipelines, samplers, and per-face transient state.
 */
static struct {
  /**
   * @brief The shadow hash of the atlas tile contents, per light index.
   * @details A light whose hash is unchanged from the frame its tile was last drawn
   * may reuse that tile indefinitely. Zero for tiles that have never been drawn.
   */
  uint64_t hashes[MAX_LIGHTS];

  /**
   * @brief The opaque BSP shadow pipeline.
   */
  GraphicsPipeline *bspOpaquePipeline;

  /**
   * @brief The BSP shadow pipeline for alpha-tested materials, discarding
   * transparent texels so foliage, fences and grates cast holes.
   */
  GraphicsPipeline *bspAlphaTestPipeline;

  /**
   * @brief The mesh shadow pipeline.
   */
  GraphicsPipeline *meshOpaquePipeline;

  /**
   * @brief The mesh shadow pipeline for alpha-tested materials, discarding
   * transparent texels so foliage, fences and grates cast holes.
   */
  GraphicsPipeline *meshAlphaTestPipeline;

  /**
   * @brief The shadow-atlas clear pipeline.
   */
  GraphicsPipeline *clearPipeline;

  /**
   * @brief The sampler used to bind alpha-tested materials' diffuse textures.
   */
  Sampler *repeatSampler;

  /**
   * @brief Cube-face view matrices for shadow lights.
   */
  Mat4 lightView[6];

  /**
   * @brief The cube face currently being rendered.
   */
  int32_t face;
} module;

/**
 * @brief Determines whether an entity hierarchy is the source of a light.
 */
static bool R_IsLightSource(const RenderLight *light, const RenderEntity *e) {

  while (e) {
    if (light->source && light->source == e->id) {
      return true;
    }
    e = e->parent;
  }

  return false;
}

/**
 * @brief Mixes the given bytes into an FNV-1a hash.
 */
static uint64_t R_HashShadowBytes(uint64_t hash, const void *bytes, size_t size) {

  const byte *b = bytes;

  while (size--) {
    hash = (hash ^ *b++) * 0x100000001b3ull;
  }

  return hash;
}

/**
 * @brief Mixes everything the shadow pass reads for one caster into the light's hash.
 */
static uint64_t R_HashLightEntity(uint64_t hash, const RenderEntity *e) {

  hash = R_HashShadowBytes(hash, &e->model, sizeof(e->model));
  hash = R_HashShadowBytes(hash, &e->matrix, sizeof(e->matrix));

  if (IS_MESH_MODEL(e->model)) {
    hash = R_HashShadowBytes(hash, &e->frame, sizeof(e->frame));
    hash = R_HashShadowBytes(hash, &e->oldFrame, sizeof(e->oldFrame));
    hash = R_HashShadowBytes(hash, &e->lerp, sizeof(e->lerp));
    hash = R_HashShadowBytes(hash, e->skins, e->model->mesh->numFaces * sizeof(e->skins[0]));
  }

  return hash;
}

/**
 * @brief Collects shadow-casting entities for one light, and hashes the inputs of its
 * shadow map so that an unchanged light may reuse its atlas tile.
 * @details The caster set is deliberately not culled against the view, so that the atlas
 * remains a function of world state alone and the hash holds still as the camera moves.
 * A hash of zero means the tile is left exactly as it is, which is only safe where the
 * shader cannot sample it: an occluded light illuminates nothing visible, and a light
 * that casts no shadows at all is given no tile. A light beyond the lighting distance
 * still hashes, and so still clears its tile once, because `bounds` may be clipped far
 * tighter than `radius`, by which the shader alone attenuates.
 */
void R_UpdateLightEntities(const RenderView *view, RenderLight *l, int32_t index) {

  l->numEntities = 0;
  l->hash = 0;

  if (l->flags & R_LIGHT_NO_SHADOW) {
    return;
  }

  if (l->occluded) {
    return;
  }

  uint64_t hash = 0xcbf29ce484222325ull;

  hash = R_HashShadowBytes(hash, &l->origin, sizeof(l->origin));
  hash = R_HashShadowBytes(hash, &l->radius, sizeof(l->radius));
  hash = R_HashShadowBytes(hash, &l->tile, sizeof(l->tile));
  hash = R_HashShadowBytes(hash, &r_alphaTest->value, sizeof(r_alphaTest->value));

  const bool bspLightGeometry = l->bspLight && l->bspLight->numDrawElements;
  hash = R_HashShadowBytes(hash, &bspLightGeometry, sizeof(bspLightGeometry));

  const Vec3 closestPoint = Box3_ClampPoint(l->bounds, view->origin);
  const float dist = Vec3_Distance(closestPoint, view->origin);

  if (dist <= r_lightingDistance->value + LIGHTING_LOD_BLEND_DIST) {

    const RenderEntity *e = view->entities;
    for (int32_t i = 0; i < view->numEntities; i++, e++) {

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

      if (!Box3_Intersects(l->bounds, e->absModelBounds)) {
        continue;
      }

      l->entities[l->numEntities++] = e;

      hash = R_HashLightEntity(hash, e);
    }
  }

  l->hash = hash ?: 1;

  if (l->hash == module.hashes[index]) {
    rStats->lightsCached++;
  }
}

/**
 * @brief Whether the light's atlas tile must be redrawn this frame.
 */
static bool R_LightShadowDirty(const RenderLight *l, int32_t index) {
  return l->hash && l->hash != module.hashes[index];
}

/**
 * @brief Draws one BSP draw elements entry for shadow casting, binding the alpha-test pipeline
 * and diffuse texture for alpha-tested entries, or the opaque pipeline otherwise.
 * @details The surface flags are consulted rather than the presence of a material, so that
 * this accepts a block's draw elements, which are batched per material, as well as the
 * depth pass elements, which lump all opaque faces under no material at all.
 * @return The pipeline now bound, so the caller can avoid redundant re-binds across calls.
 */
static GraphicsPipeline *R_DrawBspDrawElementsShadow(RenderPass *pass, const RenderBspDrawElements *draw, GraphicsPipeline *pipeline) {

  if (draw->surface & (SURF_SKY | SURF_MASK_BLEND | SURF_MATERIAL | SURF_LIQUID | SURF_MASK_NO_DRAW_ELEMENTS)) {
    return pipeline;
  }

  const bool alphaTest = (draw->surface & SURF_ALPHA_TEST) && draw->material;

  GraphicsPipeline *drawPipeline = alphaTest
    ? module.bspAlphaTestPipeline
    : module.bspOpaquePipeline;

  if (pipeline != drawPipeline) {
    pipeline = drawPipeline;
    $(pass, bindPipeline, pipeline);
  }

  if (alphaTest) {
    $(pass, bindFragmentSamplers, 0, &(SDL_GPUTextureSamplerBinding) {
      .texture = draw->material->texture->texture->texture,
      .sampler = module.repeatSampler->sampler,
    }, 1);

    const float alphaTestValue = draw->material->cm->alphaTest * r_alphaTest->value;
    $(pass->commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, &alphaTestValue, sizeof(alphaTestValue));
  }

  const uint32_t firstIndex = (uint32_t) ((uintptr_t) draw->elements / sizeof(uint32_t));
  $(pass, drawIndexedPrimitives, draw->numElements, 1, firstIndex, 0, 0);

  return pipeline;
}

/**
 * @brief Draws the given BSP draw elements array, handling pipeline toggles for alpha-test.
 */
static void R_DrawBspDrawElementsShadows(RenderPass *pass, const RenderBspDrawElements *draw, int32_t count) {

  GraphicsPipeline *pipeline = module.bspOpaquePipeline;

  for (int32_t i = 0; i < count; i++, draw++) {
    pipeline = R_DrawBspDrawElementsShadow(pass, draw, pipeline);
  }

  if (pipeline != module.bspOpaquePipeline) {
    $(pass, bindPipeline, module.bspOpaquePipeline);
  }
}

/**
 * @brief Draws the shadow geometry of the blocks the light reaches.
 * @details Geometry beyond the light's radius can neither receive its light nor occlude it,
 * so only the blocks intersecting its bounds need be drawn. This is what quemap precomputes
 * per BSP light, done at runtime instead, for lights that have no such geometry: dynamic
 * lights, and the lights of a map being edited, which move.
 * @remarks Block bounds are in model space, so this is only valid for worldspawn.
 */
static void R_DrawBspBlocksShadows(RenderPass *pass, const RenderLight *l, const RenderBspInlineModel *in) {

  GraphicsPipeline *pipeline = module.bspOpaquePipeline;

  const RenderBspBlock *block = in->blocks;
  for (int32_t i = 0; i < in->numBlocks; i++, block++) {

    if (!Box3_Intersects(l->bounds, block->visibleBounds)) {
      continue;
    }

    const RenderBspDrawElements *draw = block->drawElements;
    for (int32_t j = 0; j < block->numDrawElements; j++, draw++) {
      pipeline = R_DrawBspDrawElementsShadow(pass, draw, pipeline);
    }
  }

  if (pipeline != module.bspOpaquePipeline) {
    $(pass, bindPipeline, module.bspOpaquePipeline);
  }
}

/**
 * @brief Draws BSP inline-model shadow geometry for one light and entity.
 */
static void R_DrawBspEntityShadows(const RenderLight *l, const RenderEntity *e, RenderPass *pass) {

  const RenderBspInlineModel *in = e->model->bspInline;

  if (!in->numDepthPassElements) {
    return;
  }

  $(rContext.device->commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &(const RenderShadowLocals) {
    .model = e->matrix,
    .lightView = module.lightView[module.face],
    .lightOrigin = Vec3_ToVec4(l->origin, l->radius),
    .lerp = 0.f,
  }, sizeof(RenderShadowLocals));

  if (IS_WORLDSPAWN(e->model)) {
    if (l->bspLight && l->bspLight->numDrawElements) {
      R_DrawBspDrawElementsShadows(pass, l->bspLight->drawElements, l->bspLight->numDrawElements);
    } else {
      R_DrawBspBlocksShadows(pass, l, in);
    }
  } else {
    R_DrawBspDrawElementsShadows(pass, in->depthPassElements, in->numDepthPassElements);
  }
}

/**
 * @brief Draws BSP inline-model shadow geometry for one light to the current shadow tile.
 */
static void R_DrawBspEntitiesShadows(const RenderView *view, const RenderLight *l, RenderPass *pass) {

  const Uint32 ts = rShadowAtlas.tileSize;

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = l->tile.x,
    .y = l->tile.y,
    .w = (float) ts,
    .h = (float) ts,
    .min_depth = 0.f,
    .max_depth = 1.f,
  });

  $(pass, setScissor, &(SDL_Rect) { (int32_t) l->tile.x, (int32_t) l->tile.y, ts, ts });

  for (int32_t i = 0; i < l->numEntities; i++) {

    const RenderEntity *e = l->entities[i];

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    R_DrawBspEntityShadows(l, e, pass);
  }
}

/**
 * @brief
 */
static void R_DrawMeshEntityShadow(const RenderView *view, const RenderLight *l, const RenderEntity *e, RenderPass *pass) {

  const RenderMeshModel *mesh = e->model->mesh;

  if (!mesh->elementsBuffer) {
    return;
  }

  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) {
    .buffer = mesh->elementsBuffer->buffer
  }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  const uint32_t stride = sizeof(RenderMeshVertex);

  $(pass->commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &(const RenderShadowLocals) {
    .model = e->matrix,
    .lightView =  module.lightView[module.face],
    .lightOrigin = Vec3_ToVec4(l->origin, l->radius),
    .lerp = e->lerp,
  }, sizeof(RenderShadowLocals));

  const RenderMeshFace *face = mesh->faces;
  for (int32_t i = 0; i < mesh->numFaces; i++, face++) {

    const RenderMaterial *material = R_MeshEntityFaceMaterial(e, face, i);
    if (!material) {
      continue;
    }

    if (material->cm->surface & SURF_MASK_BLEND) {
      continue;
    }

    GraphicsPipeline *pipeline = module.meshOpaquePipeline;

    if (material->cm->surface & SURF_ALPHA_TEST) {
      pipeline = module.meshAlphaTestPipeline;

      $(pass, bindFragmentSamplers, 0, &(SDL_GPUTextureSamplerBinding) {
        .texture = material->texture->texture->texture,
        .sampler = module.repeatSampler->sampler,
      }, 1);

      const float alphaTestValue = material->cm->alphaTest * r_alphaTest->value;
      $(pass->commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, &alphaTestValue, sizeof(alphaTestValue));
    }

    $(pass, bindPipeline, pipeline);

    const uint32_t oldOffset = (uint32_t) (face->baseVertex + e->oldFrame * face->numVertexes) * stride;
    const uint32_t curOffset = (uint32_t) (face->baseVertex + e->frame * face->numVertexes) * stride;

    $(pass, bindVertexBuffers, 0, (SDL_GPUBufferBinding[]) {
      { .buffer = mesh->vertexBuffer->buffer, .offset = oldOffset },
      { .buffer = mesh->vertexBuffer->buffer, .offset = curOffset },
    }, 2);

    const uint32_t firstIndex = (uint32_t) ((uintptr_t) face->indices / sizeof(uint32_t));
    $(pass, drawIndexedPrimitives, face->numElements, 1, firstIndex, 0, 0);
  }
}

/**
 * @brief Draws mesh-entity shadow geometry for one light, on the cube face
 * currently tracked by module. Opaque and alpha-tested faces use
 * separate pipelines; translucent faces cast no shadow.
 */
static void R_DrawMeshEntitiesShadows(const RenderView *view, const RenderLight *l, RenderPass *pass) {

  const Uint32 ts = rShadowAtlas.tileSize;

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = l->tile.x,
    .y = l->tile.y,
    .w = (float) ts,
    .h = (float) ts,
    .min_depth = 0.f,
    .max_depth = 1.f,
  });

  $(pass, setScissor, &(SDL_Rect) { (int32_t) l->tile.x, (int32_t) l->tile.y, ts, ts });

  for (int32_t j = 0; j < l->numEntities; j++) {
    const RenderEntity *e = l->entities[j];

    if (!IS_MESH_MODEL(e->model)) {
      continue;
    }

    R_DrawMeshEntityShadow(view, l, e, pass);
  }
}

/**
 * @brief Renders shadow maps for lights that need a redraw.
 */
void R_DrawShadows(const RenderView *view) {

  CommandBuffer *commands = rContext.device->commands;

  const RenderBspModel *bsp = rModels.world ? rModels.world->bsp : NULL;

  for (int32_t face = 0; face < 6; face++) {

    module.face = face;

    const SDL_GPUDepthStencilTargetInfo depth = {
      .texture = rShadowAtlas.textures[face]->texture,
      .load_op = SDL_GPU_LOADOP_LOAD,
      .store_op = SDL_GPU_STOREOP_STORE,
    };

    RenderPass *pass = $(commands, beginRenderPass, NULL, 0, &depth);

    const Uint32 ts = rShadowAtlas.tileSize;

    $(pass, bindPipeline, module.clearPipeline);

    const RenderLight *l = view->lights;
    for (int32_t i = 0; i < view->numLights; i++, l++) {

      if (!R_LightShadowDirty(l, i)) {
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

    $(pass, bindPipeline, module.bspOpaquePipeline);

    if (bsp) {
      $(pass, bindVertexBuffers, 0, (SDL_GPUBufferBinding[]) {
        { .buffer = bsp->vertexBuffer->buffer },
        { .buffer = bsp->vertexBuffer->buffer },
      }, 2);

      $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) {
        .buffer = bsp->elementsBuffer->buffer
      }, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    }

    $(commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &rUniforms.block, sizeof(rUniforms.block));

    l = view->lights;
    for (int32_t i = 0; i < view->numLights; i++, l++) {

      if (!R_LightShadowDirty(l, i)) {
        continue;
      }

      if (bsp) {
        R_DrawBspEntitiesShadows(view, l, pass);
      }
    }

    l = view->lights;
    for (int32_t i = 0; i < view->numLights; i++, l++) {

      if (!R_LightShadowDirty(l, i)) {
        continue;
      }

      R_DrawMeshEntitiesShadows(view, l, pass);
    }

    pass = release(pass);
  }

  const RenderLight *l = view->lights;
  for (int32_t i = 0; i < view->numLights; i++, l++) {
    if (R_LightShadowDirty(l, i)) {
      module.hashes[i] = l->hash;
    }
  }
}

/**
 * @brief Invalidates all cached shadow atlas tiles.
 */
void R_ClearShadows(void) {

  memset(module.hashes, 0, sizeof(module.hashes));
}

/**
 * @brief Initializes shadow atlas textures, samplers, and pipelines.
 */
void R_InitShadows(void) {

  memset(&module, 0, sizeof(module));

  memset(&rShadowAtlas, 0, sizeof(rShadowAtlas));

  rShadowAtlas.tileSize = Maxi(r_shadowTileSize->integer, 128);

  const Uint32 atlasSize = SHADOW_ATLAS_LIGHTS_PER_ROW * rShadowAtlas.tileSize;

  for (int32_t face = 0; face < 6; face++) {
    rShadowAtlas.textures[face] = $(rContext.device, createTexture, &(SDL_GPUTextureCreateInfo) {
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
      .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
      .width = atlasSize,
      .height = atlasSize,
      .layer_count_or_depth = 1,
      .num_levels = 1,
      .sample_count = SDL_GPU_SAMPLECOUNT_1,
    }, NULL);

    $(rShadowAtlas.textures[face], setName, va("shadow atlas %d", face));
  }

  rShadowAtlas.sampler = $(rContext.device, createSamplerShadowCompare);

  Shader *vertexShader = $(rContext.device, loadShader, "shaders/shadow_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 2,
  });

  Shader *fragmentShader = $(rContext.device, loadShader, "shaders/shadow_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_uniform_buffers = 1,
  });

  SDL_GPUGraphicsPipelineCreateInfo info = {
    .vertex_shader = vertexShader->shader,
    .fragment_shader = fragmentShader->shader,
    .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
    .vertex_input_state = {
      .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {
        { .slot = 0, .pitch = sizeof(RenderBspVertex), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
        { .slot = 1, .pitch = sizeof(RenderBspVertex), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
      },
      .num_vertex_buffers = 2,
      .vertex_attributes = (SDL_GPUVertexAttribute[]) {
        { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderBspVertex, position) },
        { .location = 1, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderBspVertex, position) },
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

  module.bspOpaquePipeline = $(rContext.device, createGraphicsPipeline, &info);

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {
      { .slot = 0, .pitch = sizeof(RenderMeshVertex), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
      { .slot = 1, .pitch = sizeof(RenderMeshVertex), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
    },
    .num_vertex_buffers = 2,
    .vertex_attributes = (SDL_GPUVertexAttribute[]) {
      { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderMeshVertex, position) },
      { .location = 1, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderMeshVertex, position) },
    },
    .num_vertex_attributes = 2,
  };

  module.meshOpaquePipeline = $(rContext.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);

  Shader *alphaTestVertexShader = $(rContext.device, loadShader, "shaders/shadow_vs_alpha_test", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 2,
  });

  Shader *alphaTestFragmentShader = $(rContext.device, loadShader, "shaders/shadow_fs_alpha_test", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = 1,
    .num_uniform_buffers = 2,
  });

  info.vertex_shader = alphaTestVertexShader->shader;
  info.fragment_shader = alphaTestFragmentShader->shader;
  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {
      { .slot = 0, .pitch = sizeof(RenderMeshVertex), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
      { .slot = 1, .pitch = sizeof(RenderMeshVertex), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
    },
    .num_vertex_buffers = 2,
    .vertex_attributes = (SDL_GPUVertexAttribute[]) {
      { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderMeshVertex, position) },
      { .location = 1, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderMeshVertex, position) },
      { .location = 2, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = offsetof(RenderMeshVertex, diffusemap) },
    },
    .num_vertex_attributes = 3,
  };

  module.meshAlphaTestPipeline = $(rContext.device, createGraphicsPipeline, &info);

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {
      { .slot = 0, .pitch = sizeof(RenderBspVertex), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
      { .slot = 1, .pitch = sizeof(RenderBspVertex), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
    },
    .num_vertex_buffers = 2,
    .vertex_attributes = (SDL_GPUVertexAttribute[]) {
      { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderBspVertex, position) },
      { .location = 1, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderBspVertex, position) },
      { .location = 2, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = offsetof(RenderBspVertex, diffusemap) },
    },
    .num_vertex_attributes = 3,
  };

  module.bspAlphaTestPipeline = $(rContext.device, createGraphicsPipeline, &info);

  release(alphaTestVertexShader);
  release(alphaTestFragmentShader);

  module.repeatSampler = $(rContext.device, createSamplerLinearRepeat);

  SDL_GPUGraphicsPipelineCreateInfo clearInfo = {
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

  module.clearPipeline = $(rContext.device, loadGraphicsPipeline,
    "shaders/shadow_clear_vs", &(SDL_GPUShaderCreateInfo) {
      .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    },
    "shaders/shadow_clear_fs", &(SDL_GPUShaderCreateInfo) {
      .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    },
    &clearInfo);

  module.lightView[0] = Mat4_LookAt(Vec3_Zero(), MakeVec3( 1.f,  0.f,  0.f), MakeVec3(0.f, -1.f,  0.f));
  module.lightView[1] = Mat4_LookAt(Vec3_Zero(), MakeVec3(-1.f,  0.f,  0.f), MakeVec3(0.f, -1.f,  0.f));
  module.lightView[2] = Mat4_LookAt(Vec3_Zero(), MakeVec3( 0.f,  1.f,  0.f), MakeVec3(0.f,  0.f,  1.f));
  module.lightView[3] = Mat4_LookAt(Vec3_Zero(), MakeVec3( 0.f, -1.f,  0.f), MakeVec3(0.f,  0.f, -1.f));
  module.lightView[4] = Mat4_LookAt(Vec3_Zero(), MakeVec3( 0.f,  0.f,  1.f), MakeVec3(0.f, -1.f,  0.f));
  module.lightView[5] = Mat4_LookAt(Vec3_Zero(), MakeVec3( 0.f,  0.f, -1.f), MakeVec3(0.f, -1.f,  0.f));
}

/**
 * @brief Shuts down all shadow mapping resources.
 */
void R_ShutdownShadows(void) {

  module.bspOpaquePipeline = release(module.bspOpaquePipeline);
  module.bspAlphaTestPipeline = release(module.bspAlphaTestPipeline);
  module.meshOpaquePipeline = release(module.meshOpaquePipeline);
  module.meshAlphaTestPipeline = release(module.meshAlphaTestPipeline);
  module.clearPipeline = release(module.clearPipeline);
  module.repeatSampler = release(module.repeatSampler);
  rShadowAtlas.sampler = release(rShadowAtlas.sampler);

  for (int32_t face = 0; face < 6; face++) {
    rShadowAtlas.textures[face] = release(rShadowAtlas.textures[face]);
  }
}
