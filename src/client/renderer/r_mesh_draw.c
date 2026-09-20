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

/**
 * @brief Mesh has no samplers beyond the shared family (r_material.h).
 */
#define MESH_NUM_SAMPLERS R_SAMPLER_MATERIAL_TOTAL

/**
 * @brief mesh_vs only samples the voxel caustics/occlusion and sky textures
 * (see material.glsl), at its own compact 0..2 binding numbering -- not the
 * fragment stage's absolute R_SAMPLER_VOXEL_CAUSTICS/OCCLUSION/SKY indices.
 */
enum {
  MESH_VERTEX_SAMPLER_VOXEL_CAUSTICS,
  MESH_VERTEX_SAMPLER_VOXEL_OCCLUSION,
  MESH_VERTEX_SAMPLER_SKY,
  MESH_NUM_VERTEX_SAMPLERS,
};

enum {
  MESH_UNIFORMS_GLOBALS,
  MESH_UNIFORMS_LOCALS,
  MESH_UNIFORMS_MATERIAL,
  MESH_NUM_UNIFORMS
};

/**
 * @brief The maximum number of cached material-stage pipelines (one per blend).
 */
#define MAX_STAGE_PIPELINES 16

/**
 * @brief Mesh draw pipelines, samplers, and stage cache.
 */
static struct {

  /**
   * @brief The opaque mesh pipeline.
   */
  GraphicsPipeline *opaquePipeline;

  /**
   * @brief The alpha-test mesh pipeline.
   */
  GraphicsPipeline *alphaTestPipeline;

  /**
   * @brief The alpha-blended mesh pipeline.
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
   * @brief Default shell image.
   */
  RenderImage *shell;

  /**
   * @brief Fallback voxel textures for player-model views.
   */
  Texture *voxelCausticsFallback;
  Texture *voxelOcclusionFallback;
  Texture *skyFallback;

  /**
   * @brief Cached stage pipelines keyed by blend function.
   */
  RenderStagePipeline stagePipelines[MAX_STAGE_PIPELINES];
  int32_t numStagePipelines;

  /**
   * @brief The material, stage-draw flag and dynamic light mask for the face in progress.
   */
  const RenderMaterial *material;
  bool drawStages;
  const RenderActiveDynamicLights *activeDynamicLights;
} module;

/**
 * @brief Per-entity mesh vertex uniforms.
 */
typedef struct {
  Mat4 model;
  float lerp;
  float padding[3];
  Vec4 color;
  RenderActiveDynamicLights activeDynamicLights;
} RenderMeshLocals;

/**
 * @brief Per-entity fragment locals.
 */
typedef struct {
  RenderActiveDynamicLights activeDynamicLights;
} RenderMeshFragmentLocals;

/**
 * @brief Returns the cached mesh stage pipeline for the specified blend function.
 */
static GraphicsPipeline *R_MeshStagePipeline(CmBlend src, CmBlend dest) {

  RenderStagePipeline *p = module.stagePipelines;
  for (int32_t i = 0; i < module.numStagePipelines; i++, p++) {
    if (p->src == src && p->dest == dest) {
      return p->pipeline;
    }
  }

  if (module.numStagePipelines == MAX_STAGE_PIPELINES) {
    return NULL;
  }

  Shader *vertexShader = $(rContext.device, loadShader, "shaders/mesh_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_samplers = MESH_NUM_VERTEX_SAMPLERS,
    .num_storage_buffers = R_STORAGE_MATERIAL_TOTAL,
    .num_uniform_buffers = MESH_NUM_UNIFORMS,
  });

  Shader *fragmentShader = $(rContext.device, loadShader, "shaders/mesh_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = MESH_NUM_SAMPLERS,
    .num_storage_buffers = R_STORAGE_MATERIAL_TOTAL,
    .num_uniform_buffers = MESH_NUM_UNIFORMS,
  });

  const SDL_GPUBlendFactor s = R_BlendFactor(src);
  const SDL_GPUBlendFactor d = R_BlendFactor(dest);

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = rSceneSamples;
  info.vertex_shader = vertexShader->shader;
  info.fragment_shader = fragmentShader->shader;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;

  info.depth_stencil_state.enable_depth_write = false;

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {
      { .slot = 0, .pitch = sizeof(RenderMeshVertex), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
      { .slot = 1, .pitch = sizeof(RenderMeshVertex), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
    },
    .num_vertex_buffers = 2,
    .vertex_attributes = (SDL_GPUVertexAttribute[]) {
      { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderMeshVertex, position) },
      { .location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderMeshVertex, normal) },
      { .location = 2, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderMeshVertex, tangent) },
      { .location = 3, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderMeshVertex, bitangent) },
      { .location = 4, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = offsetof(RenderMeshVertex, diffusemap) },
      { .location = 5, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderMeshVertex, position) },
      { .location = 6, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderMeshVertex, normal) },
      { .location = 7, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderMeshVertex, tangent) },
      { .location = 8, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderMeshVertex, bitangent) },
    },
    .num_vertex_attributes = 9,
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

  GraphicsPipeline *pipeline = $(rContext.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);

  module.stagePipelines[module.numStagePipelines++] = (RenderStagePipeline) {
    .src = src, .dest = dest, .depthWrite = false, .pipeline = pipeline,
  };

  return pipeline;
}

/**
 * @brief Draws one material stage for a mesh face.
 */
static void R_DrawMeshEntityMaterialStage(const RenderView *view,
                                          const RenderEntity *e,
                                          const RenderMeshFace *face,
                                          const RenderStage *stage,
                                          RenderPass *pass) {

  const RenderMaterial *material = module.material;

  RenderMeshMaterialUniforms uniforms = { 0 };
  R_MaterialUniforms(material, material->cm->surface, &uniforms.material);

  SDL_GPUTexture *texture, *textureNext;
  if (!R_StageUniforms(view, e, NULL, stage, &uniforms.material, &texture, &textureNext)) {
    return;
  }

  GraphicsPipeline *pipeline = R_MeshStagePipeline(stage->cm->blend.src, stage->cm->blend.dest);
  if (!pipeline) {
    return;
  }

  $(pass, bindPipeline, pipeline);

  $(pass, bindFragmentSamplers, R_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = texture, .sampler = module.repeatSampler->sampler },
    { .texture = textureNext, .sampler = module.repeatSampler->sampler },
  }, 2);

  $(pass->commands, pushVertexUniformData, MESH_UNIFORMS_MATERIAL, &uniforms.material, sizeof(uniforms.material));
  $(pass->commands, pushFragmentUniformData, MESH_UNIFORMS_MATERIAL, &uniforms, sizeof(uniforms));

  const uint32_t firstIndex = (uint32_t) ((uintptr_t) face->indices / sizeof(uint32_t));
  $(pass, drawIndexedPrimitives, face->numElements, 1, firstIndex, 0, 0);

  rStats->meshTriangles += face->numElements / 3;
}

/**
 * @brief Draws a shell stage for a mesh face.
 */
static void R_DrawMeshEntityShellEffect(const RenderView *view, const RenderEntity *e, const RenderMeshFace *face, RenderPass *pass) {

  if (!(e->effects & EF_SHELL)) {
    return;
  }

  if (!module.shell) {
    module.shell = R_LoadImage("textures/envmaps/white", IMG_PROGRAM);
    if (!module.shell) {
      return;
    }
  }

  for (const RenderStage *stage = module.material->stages; stage; stage = stage->next) {
    if (stage->cm->flags & STAGE_SHELL) {
      R_DrawMeshEntityMaterialStage(view, e, face, stage, pass);
      return;
    }
  }

  const float radius = (e->effects & EF_WEAPON) ? .25f : 1.f;

  const CmStage cm = {
    .flags = STAGE_COLOR | STAGE_SHELL
        | STAGE_SCALE_S | STAGE_SCALE_T
        | STAGE_SCROLL_S | STAGE_SCROLL_T
        | STAGE_LIGHTING | STAGE_LIGHTING_FLAT | STAGE_ENVMAP,
    .color = Color4fv(e->shell),
    .blend = { .src = BLEND_SRC_ALPHA, .dest = BLEND_ONE },
    .scroll = { 1.f, 1.f },
    .scale = { .5f, .5f },
    .shell = { radius },
    .lighting = { 1.f, STAGE_LIGHTING_MODE_FLAT },
  };

  const RenderStage defaultShell = {
    .cm = &cm,
    .flags = cm.flags,
    .media = (RenderMedia *) module.shell,
  };

  R_DrawMeshEntityMaterialStage(view, e, face, &defaultShell, pass);
}

/**
 * @brief Draws a mesh face's material stages and shell effect.
 */
static void R_DrawMeshEntityMaterialStages(const RenderView *view, const RenderEntity *e, const RenderMeshFace *face, RenderPass *pass) {

  const RenderMaterial *material = module.material;

  if (!r_drawMaterialStages->integer) {
    return;
  }

  if (!(material->cm->stageFlags & STAGE_DRAW) && !(e->effects & EF_SHELL)) {
    return;
  }

  for (const RenderStage *stage = material->stages; stage; stage = stage->next) {
    if (!(stage->cm->flags & STAGE_DRAW)) {
      continue;
    }
    R_DrawMeshEntityMaterialStage(view, e, face, stage, pass);
  }

  R_DrawMeshEntityShellEffect(view, e, face, pass);
}

/**
 * @brief Binds per-face mesh uniforms and vertex buffers.
 */
static void R_BindMeshEntityFace(const RenderEntity *e, const RenderMeshModel *mesh, const RenderMeshFace *face, RenderPass *pass) {

  RenderMeshLocals locals = {
    .model = e->matrix,
    .lerp = e->lerp,
    .color = e->color,
  };

  if (e->effects & EF_MODULATE) {
    locals.color.xyz = Vec3_Scale(locals.color.xyz, r_modulateMesh->value);
  }

  memcpy(&locals.activeDynamicLights, module.activeDynamicLights, sizeof(locals.activeDynamicLights));

  switch (module.material->cm->surface & SURF_MASK_BLEND) {
    case SURF_BLEND_33:
      locals.color.w *= .333f;
      break;
    case SURF_BLEND_66:
      locals.color.w *= .666f;
      break;
    default:
      break;
  }

  $(pass->commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &locals, sizeof(locals));

  const uint32_t stride = sizeof(RenderMeshVertex);
  const uint32_t oldOffset = (uint32_t) (face->baseVertex + e->oldFrame * face->numVertexes) * stride;
  const uint32_t curOffset = (uint32_t) (face->baseVertex + e->frame * face->numVertexes) * stride;

  $(pass, bindVertexBuffers, 0, (SDL_GPUBufferBinding[]) {
    { .buffer = mesh->vertexBuffer->buffer, .offset = oldOffset },
    { .buffer = mesh->vertexBuffer->buffer, .offset = curOffset },
  }, 2);
}

/**
 * @brief Draws a single mesh face.
 */
static void R_DrawMeshEntityFace(const RenderView *view,
                                 const RenderEntity *e,
                                 const RenderMeshModel *mesh,
                                 const RenderMeshFace *face,
                                 RenderPass *pass) {

  const RenderMaterial *material = module.material;

  $(pass, bindFragmentSamplers, R_SAMPLER_MATERIAL, &(SDL_GPUTextureSamplerBinding) {
    .texture = material->texture->texture->texture,
    .sampler = module.repeatSampler->sampler,
  }, 1);

  RenderMeshMaterialUniforms materialUniforms;
  R_MaterialUniforms(material, material->cm->surface, &materialUniforms.material);
  memcpy(materialUniforms.tintColors, e->tints, sizeof(materialUniforms.tintColors));

  for (size_t i = 0; i < lengthof(materialUniforms.tintColors); i++) {
    if (!e->tints[i].w) {
      materialUniforms.tintColors[i] = material->cm->tintmapDefaults[i];
    }
  }
  $(pass->commands, pushVertexUniformData, MESH_UNIFORMS_MATERIAL, &materialUniforms.material, sizeof(materialUniforms.material));
  $(pass->commands, pushFragmentUniformData, MESH_UNIFORMS_MATERIAL, &materialUniforms, sizeof(materialUniforms));

  R_BindMeshEntityFace(e, mesh, face, pass);

  if (!(material->cm->surface & SURF_MATERIAL)) {

    const uint32_t firstIndex = (uint32_t) ((uintptr_t) face->indices / sizeof(uint32_t));

    $(pass, drawIndexedPrimitives, face->numElements, 1, firstIndex, 0, 0);

    rStats->meshDrawElements++;
    rStats->meshTriangles += face->numElements / 3;
  }

  if (module.drawStages) {
    R_DrawMeshEntityMaterialStages(view, e, face, pass);
  }
}

/**
 * @brief Draws a mesh face's material stages without the base face draw.
 */
static void R_DrawMeshEntityFaceMaterialStages(const RenderView *view,
                                               const RenderEntity *e,
                                               const RenderMeshModel *mesh,
                                               const RenderMeshFace *face,
                                               RenderPass *pass) {

  const RenderMaterial *material = module.material;

  $(pass, bindFragmentSamplers, R_SAMPLER_MATERIAL, &(SDL_GPUTextureSamplerBinding) {
    .texture = material->texture->texture->texture,
    .sampler = module.repeatSampler->sampler,
  }, 1);

  R_BindMeshEntityFace(e, mesh, face, pass);

  R_DrawMeshEntityMaterialStages(view, e, face, pass);
}

/**
 * @brief Draws a mesh entity.
 */
static void R_DrawMeshEntity(const RenderView *view, const RenderEntity *e, RenderPass *pass) {

  const RenderMeshModel *mesh = e->model->mesh;
  assert(mesh);

  if (!mesh->vertexBuffer) {
    return;
  }

  if (e->effects & EF_WEAPON) {
    $(pass, setViewport, &(SDL_GPUViewport) {
      .x = 0.f, .y = 0.f,
      .w = (float) view->framebuffer->size.w, .h = (float) view->framebuffer->size.h,
      .min_depth = 0.f, .max_depth = .1f,
    });
  }

  RenderMeshFragmentLocals fragmentLocals = { 0 };
  memcpy(&fragmentLocals.activeDynamicLights, &e->activeDynamicLights, sizeof(fragmentLocals.activeDynamicLights));
  $(pass->commands, pushFragmentUniformData, MESH_UNIFORMS_LOCALS, &fragmentLocals, sizeof(fragmentLocals));

  module.activeDynamicLights = &fragmentLocals.activeDynamicLights;

  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) {
    .buffer = mesh->elementsBuffer->buffer
  }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  const RenderMeshFace *face = mesh->faces;
  for (int32_t i = 0; i < mesh->numFaces; i++, face++) {

    const RenderMaterial *material = R_MeshEntityFaceMaterial(e, face, i);
    if (!material) {
      continue;
    }

    if ((material->cm->surface & SURF_MASK_BLEND) || (e->effects & EF_BLEND)) {
      continue;
    }

    if (!material->texture || !material->texture->texture) {
      continue;
    }

    if (material->cm->surface & SURF_ALPHA_TEST) {
      continue;
    }

    $(pass, bindPipeline, module.opaquePipeline);

    module.material = material;
    module.drawStages = false;
    R_DrawMeshEntityFace(view, e, mesh, face, pass);
  }

  face = mesh->faces;
  for (int32_t i = 0; i < mesh->numFaces; i++, face++) {

    const RenderMaterial *material = R_MeshEntityFaceMaterial(e, face, i);
    if (!material) {
      continue;
    }

    if ((material->cm->surface & SURF_MASK_BLEND) || (e->effects & EF_BLEND)) {
      continue;
    }

    if (!material->texture || !material->texture->texture) {
      continue;
    }

    if (!(material->cm->surface & SURF_ALPHA_TEST)) {
      continue;
    }

    $(pass, bindPipeline, module.alphaTestPipeline);

    module.material = material;
    module.drawStages = false;
    R_DrawMeshEntityFace(view, e, mesh, face, pass);
  }

  if (r_drawMaterialStages->integer) {

    face = mesh->faces;
    for (int32_t i = 0; i < mesh->numFaces; i++, face++) {

      const RenderMaterial *material = R_MeshEntityFaceMaterial(e, face, i);
      if (!material) {
        continue;
      }

      if ((material->cm->surface & SURF_MASK_BLEND) || (e->effects & EF_BLEND)) {
        continue;
      }

      if (!material->texture || !material->texture->texture) {
        continue;
      }

      module.material = material;
      R_DrawMeshEntityFaceMaterialStages(view, e, mesh, face, pass);
    }
  }

  face = mesh->faces;
  for (int32_t i = 0; i < mesh->numFaces; i++, face++) {

    const RenderMaterial *material = R_MeshEntityFaceMaterial(e, face, i);
    if (!material) {
      continue;
    }

    if (!((material->cm->surface & SURF_MASK_BLEND) || (e->effects & EF_BLEND))) {
      continue;
    }

    if (!material->texture || !material->texture->texture) {
      continue;
    }

    $(pass, bindPipeline, module.blendPipeline);

    module.material = material;
    module.drawStages = true;

    R_DrawMeshEntityFace(view, e, mesh, face, pass);
  }

  if (e->effects & EF_WEAPON) {
    $(pass, setViewport, &(SDL_GPUViewport) {
      .x = 0.f, .y = 0.f,
      .w = (float) view->framebuffer->size.w, .h = (float) view->framebuffer->size.h,
      .min_depth = 0.f, .max_depth = 1.f,
    });
  }

  rStats->meshModels++;
}

/**
 * @brief Draws mesh entities for the view.
 */
void R_DrawMeshEntities(const RenderView *view, RenderPass *pass) {

  // The player model preview must never bind the current world's voxel/sky
  // data: its view origin has no relation to the loaded map's lighting, so
  // doing so would produce seemingly random lighting on the preview model.
  const RenderBspModel *bsp = view->type == VIEW_PLAYER_MODEL ? NULL : rModels.world->bsp;
  Framebuffer *framebuffer = view->framebuffer;

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  $(pass->commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &rUniforms.block, sizeof(rUniforms.block));

  $(pass, bindPipeline, module.opaquePipeline);

  $(pass, bindFragmentSamplers, R_SAMPLER_SHADOW_ATLAS_0, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = rShadowAtlas.textures[0]->texture, .sampler = rShadowAtlas.sampler->sampler },
    { .texture = rShadowAtlas.textures[1]->texture, .sampler = rShadowAtlas.sampler->sampler },
    { .texture = rShadowAtlas.textures[2]->texture, .sampler = rShadowAtlas.sampler->sampler },
    { .texture = rShadowAtlas.textures[3]->texture, .sampler = rShadowAtlas.sampler->sampler },
    { .texture = rShadowAtlas.textures[4]->texture, .sampler = rShadowAtlas.sampler->sampler },
    { .texture = rShadowAtlas.textures[5]->texture, .sampler = rShadowAtlas.sampler->sampler },
  }, 6);

  Texture *caustics = bsp ? bsp->voxels.caustics->texture : module.voxelCausticsFallback;
  Texture *occlusion = bsp ? bsp->voxels.occlusion->texture : module.voxelOcclusionFallback;
  Texture *sky = bsp ? bsp->sky->texture : module.skyFallback;

  $(pass, bindFragmentSamplers, R_SAMPLER_VOXEL_CAUSTICS, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = caustics->texture, .sampler = module.clampSampler->sampler },
    { .texture = occlusion->texture, .sampler = module.clampSampler->sampler },
  }, 2);

  $(pass, bindFragmentSamplers, R_SAMPLER_SKY, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = sky->texture, .sampler = module.clampSampler->sampler },
  }, 1);

  $(pass, bindVertexSamplers, MESH_VERTEX_SAMPLER_VOXEL_CAUSTICS, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = caustics->texture, .sampler = module.clampSampler->sampler },
    { .texture = occlusion->texture, .sampler = module.clampSampler->sampler },
    { .texture = sky->texture, .sampler = module.clampSampler->sampler },
  }, 3);

  $(pass, bindFragmentSamplers, R_SAMPLER_STAGE, (SDL_GPUTextureSamplerBinding[]) {
    { .texture = rContext.nullTexture->texture, .sampler = module.repeatSampler->sampler },
    { .texture = rContext.nullTexture->texture, .sampler = module.repeatSampler->sampler },
  }, 2);

  SDL_GPUBuffer *storage[] = {
    rLights.bspBuffer->buffer,
    rLights.dynamicBuffer->buffer,
    bsp && bsp->voxels.lightDataBuffer ? bsp->voxels.lightDataBuffer->buffer : rLights.voxelFallbackBuffer->buffer,
    bsp && bsp->voxels.lightIndicesBuffer ? bsp->voxels.lightIndicesBuffer->buffer : rLights.voxelFallbackBuffer->buffer,
  };
  $(pass, bindFragmentStorageBuffers, R_STORAGE_BSP_LIGHTS, storage, R_STORAGE_MATERIAL_TOTAL);
  $(pass, bindVertexStorageBuffers, R_STORAGE_BSP_LIGHTS, storage, R_STORAGE_MATERIAL_TOTAL);

  const RenderEntity *e = view->entities;
  for (int32_t i = 0; i < view->numEntities; i++, e++) {

    if (!IS_MESH_MODEL(e->model)) {
      continue;
    }

    if (e->effects & EF_NO_DRAW) {
      continue;
    }

    if (R_CullEntity(view, e)) {
      rStats->entitiesOccluded++;
      continue;
    }

    R_DrawMeshEntity(view, e, pass);
    rStats->entitiesVisible++;
  }
}

/**
 * @brief Initializes the mesh pipelines and samplers.
 */
void R_InitMeshPipeline(void) {

  Shader *vertexShader = $(rContext.device, loadShader, "shaders/mesh_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_samplers = MESH_NUM_VERTEX_SAMPLERS,
    .num_storage_buffers = R_STORAGE_MATERIAL_TOTAL,
    .num_uniform_buffers = MESH_NUM_UNIFORMS,
  });

  Shader *fragmentShader = $(rContext.device, loadShader, "shaders/mesh_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = MESH_NUM_SAMPLERS,
    .num_storage_buffers = R_STORAGE_MATERIAL_TOTAL,
    .num_uniform_buffers = MESH_NUM_UNIFORMS,
  });

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = rSceneSamples;
  info.vertex_shader = vertexShader->shader;
  info.fragment_shader = fragmentShader->shader;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {
      { .slot = 0, .pitch = sizeof(RenderMeshVertex), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
      { .slot = 1, .pitch = sizeof(RenderMeshVertex), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
    },
    .num_vertex_buffers = 2,
    .vertex_attributes = (SDL_GPUVertexAttribute[]) {
      { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderMeshVertex, position) },
      { .location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderMeshVertex, normal) },
      { .location = 2, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderMeshVertex, tangent) },
      { .location = 3, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderMeshVertex, bitangent) },
      { .location = 4, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = offsetof(RenderMeshVertex, diffusemap) },
      { .location = 5, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderMeshVertex, position) },
      { .location = 6, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderMeshVertex, normal) },
      { .location = 7, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderMeshVertex, tangent) },
      { .location = 8, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderMeshVertex, bitangent) },
    },
    .num_vertex_attributes = 9,
  };

  SDL_GPUColorTargetDescription colorTargets[] = {
    { .format = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT, .blend_state = GPU_BlendStateOpaque },
    { .format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT, .blend_state = GPU_BlendStateOpaque },
  };

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = colorTargets,
    .num_color_targets = 2,
    .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
    .has_depth_stencil_target = true,
  };

  module.opaquePipeline = $(rContext.device, createGraphicsPipeline, &info);

  Shader *alphaTestFragmentShader = $(rContext.device, loadShader, "shaders/mesh_fs_alpha_test", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = MESH_NUM_SAMPLERS,
    .num_storage_buffers = R_STORAGE_MATERIAL_TOTAL,
    .num_uniform_buffers = MESH_NUM_UNIFORMS,
  });

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

  info.fragment_shader = alphaTestFragmentShader->shader;
  module.alphaTestPipeline = $(rContext.device, createGraphicsPipeline, &info);
  release(alphaTestFragmentShader);

  info.fragment_shader = fragmentShader->shader;

  colorTargets[0].blend_state = GPU_BlendStateAlpha;

  module.blendPipeline = $(rContext.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);

  module.repeatSampler = $(rContext.device, createSamplerLinearRepeat);
  module.clampSampler = $(rContext.device, createSamplerLinearClamp);

  const Uint8 causticsTexel[4] = { 128, 128, 128, 255 };
  module.voxelCausticsFallback = $(rContext.device, createTexture, &(SDL_GPUTextureCreateInfo) {
    .type = SDL_GPU_TEXTURETYPE_3D,
    .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
    .width = 1, .height = 1, .layer_count_or_depth = 1,
    .num_levels = 1,
    .sample_count = SDL_GPU_SAMPLECOUNT_1,
  }, causticsTexel);

  const Uint8 occlusionTexel[2] = { 0, 0 };
  module.voxelOcclusionFallback = $(rContext.device, createTexture, &(SDL_GPUTextureCreateInfo) {
    .type = SDL_GPU_TEXTURETYPE_3D,
    .format = SDL_GPU_TEXTUREFORMAT_R8G8_UNORM,
    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
    .width = 1, .height = 1, .layer_count_or_depth = 1,
    .num_levels = 1,
    .sample_count = SDL_GPU_SAMPLECOUNT_1,
  }, occlusionTexel);

  module.skyFallback = $(rContext.device, createSolidColorTexture, SDL_GPU_TEXTURETYPE_CUBE, 6, 0x00000000);
}

/**
 * @brief Releases the mesh pipelines and samplers.
 */
void R_ShutdownMeshPipeline(void) {
  module.opaquePipeline = release(module.opaquePipeline);
  module.alphaTestPipeline = release(module.alphaTestPipeline);
  module.blendPipeline = release(module.blendPipeline);
  module.repeatSampler = release(module.repeatSampler);
  module.clampSampler = release(module.clampSampler);
  module.voxelCausticsFallback = release(module.voxelCausticsFallback);
  module.voxelOcclusionFallback = release(module.voxelOcclusionFallback);
  module.skyFallback = release(module.skyFallback);

  for (int32_t i = 0; i < module.numStagePipelines; i++) {
    release(module.stagePipelines[i].pipeline);
  }

  module.numStagePipelines = 0;
}

/**
 * @brief Rebuilds the mesh pipelines and samplers.
 */
void R_UpdateMeshPipeline(void) {
  R_ShutdownMeshPipeline();
  R_InitMeshPipeline();
}
