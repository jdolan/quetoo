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
 * @brief The decal pipeline (decal_vs/decal_fs) and its samplers.
 */
static struct {
  GraphicsPipeline *pipeline;

  /**
   * @brief The decal atlas sampler (linear, clamp).
   */
  Sampler *diffusemapSampler;

} decalPipeline;

/**
 * @brief The decal instances, shared by every block and read by decal_vs.
 * @remarks A ring: instances are appended as decals are clipped, and are never
 * revisited afterwards. A block's triangles reference their instance by index,
 * rather than each vertex carrying its own copy of the decal.
 */
static struct {
  RenderDecalInstance instances[MAX_DECAL_INSTANCES];

  Buffer *buffer;

  /**
   * @brief The transfer buffer sourcing the instance uploads, held for the subsystem's
   * lifetime because they run for as long as decals are pending.
   */
  TransferBuffer *transferBuffer;

  /**
   * @brief The ring cursor, and the generation it is presently writing.
   */
  uint32_t next;
  uint32_t generation;

  /**
   * @brief The instances appended since the last upload.
   */
  uint32_t firstPending;
  uint32_t numPending;

} module;

/**
 * @brief Adds a decal to the view for rendering in the current frame.
 */
void R_AddDecal(RenderView *view, const RenderDecal *decal) {

  assert(decal);
  assert(decal->image);
  assert(decal->radius > 0.f);
  assert(decal->lifetime > 0);

  if (view->numDecals == MAX_DECALS) {
    Com_Warn("MAX_DECALS\n");
    return;
  }

  RenderDecal *out = &view->decals[view->numDecals++];

  *out = *decal;
}

/**
 * @brief Per-thread scratch windings for decal clipping, so that clipping a
 * decal to a face performs no allocations. Freed with the rest of
 * `MEM_TAG_POLYLIB` at shutdown.
 */
static _Thread_local struct {
  CmWinding *decal;
  CmWinding *face;
  CmWinding *a, *b;
  int32_t maxFacePoints;
  int32_t capacity;
} decalWindings;

/**
 * @brief Grows the per-thread scratch windings to accommodate a face of
 * `facePoints` points.
 */
static void R_ReserveDecalWindings(int32_t facePoints) {

  if (decalWindings.decal == NULL) {
    decalWindings.decal = Cm_AllocWinding(4);
  }

  if (facePoints > decalWindings.maxFacePoints) {

    if (decalWindings.face) {
      Cm_FreeWinding(decalWindings.face);
      Cm_FreeWinding(decalWindings.a);
      Cm_FreeWinding(decalWindings.b);
    }

    decalWindings.capacity = 4 + 4 * facePoints;
    decalWindings.face = Cm_AllocWinding(facePoints);
    decalWindings.a = Cm_AllocWinding(decalWindings.capacity);
    decalWindings.b = Cm_AllocWinding(decalWindings.capacity);
    decalWindings.maxFacePoints = facePoints;
  }
}

/**
 * @brief Appends the instance for a decal clipped to a single face.
 * @return The reference for the decal's vertexes to carry.
 */
static uint32_t R_AddDecalInstance(const RenderDecal *decal,
                                   const Vec3 normal,
                                   const Vec3 tangent,
                                   const Vec3 bitangent) {

  const uint32_t index = module.next;
  const uint32_t generation = module.generation;

  RenderDecalInstance *instance = module.instances + index;

  instance->origin = Vec3_ToVec4(decal->origin, decal->radius);
  instance->normal = Vec3_ToVec4(normal, 0.f);
  instance->tangent = Vec3_ToVec4(tangent, 0.f);
  instance->bitangent = Vec3_ToVec4(bitangent, 0.f);
  instance->texcoords = decal->image->texcoords;
  instance->color = decal->color.vec4;
  instance->time = decal->time;
  instance->lifetime = decal->lifetime;
  instance->generation = generation;

  if (module.numPending == 0) {
    module.firstPending = index;
  }

  if (module.numPending < MAX_DECAL_INSTANCES) {
    module.numPending++;
  }

  module.next++;

  if (module.next == MAX_DECAL_INSTANCES) {
    module.next = 0;
    module.generation = (module.generation + 1) & 0xff;
  }

  return (generation << 24) | index;
}

/**
 * @brief Resolves the instance a decal vertex references.
 */
static const RenderDecalInstance *R_DecalInstance(uint32_t reference) {
  return module.instances + (reference & 0xffffff);
}

/**
 * @brief Uploads one range of instances through the transfer buffer held for that purpose.
 */
static void R_UploadDecalInstanceRange(CopyPass *pass, const void *instances, uint32_t size, uint32_t offset) {

  $(module.transferBuffer, write, instances, size, true);

  $(pass, uploadBuffer,
    &(SDL_GPUTransferBufferLocation) { .transfer_buffer = module.transferBuffer->buffer },
    &(SDL_GPUBufferRegion) { .buffer = module.buffer->buffer, .offset = offset, .size = size },
    false);
}

/**
 * @brief Uploads the instances appended since the last frame.
 * @remarks This does not cycle, because an instance is never written twice, and
 * the ring cannot have wrapped onto instances an in-flight frame may still read.
 */
static void R_UploadDecalInstances(CopyPass *pass) {

  if (module.numPending == 0) {
    return;
  }

  const uint32_t first = module.numPending == MAX_DECAL_INSTANCES ? 0 : module.firstPending;
  const uint32_t head = (uint32_t) Mini((int32_t) module.numPending, (int32_t) (MAX_DECAL_INSTANCES - first));

  R_UploadDecalInstanceRange(pass, module.instances + first,
    head * sizeof(RenderDecalInstance), first * sizeof(RenderDecalInstance));

  if (module.numPending > head) {
    R_UploadDecalInstanceRange(pass, module.instances,
      (module.numPending - head) * sizeof(RenderDecalInstance), 0);
  }

  module.numPending = 0;
}

/**
 * @brief Clips a decal to a face and adds the resulting triangles to the face's block.
 */
static void R_ClipDecalToFace(const RenderView *view,
                              const RenderBspFace *face,
                              const RenderDecal *decal,
                              const Vec3 normal,
                              const Vec3 tangent,
                              const Vec3 bitangent,
                              RenderBspBlockDecals *decals) {

  Vec3 n = normal;
  Vec3 t = tangent, b = bitangent;

  if (decal->rotation != 0.f) {
    const float cosRot = cosf(decal->rotation);
    const float sinRot = sinf(decal->rotation);
    const Vec3 tRot = Vec3_Add(Vec3_Scale(t, cosRot), Vec3_Scale(b, sinRot));
    const Vec3 bRot = Vec3_Add(Vec3_Scale(b, cosRot), Vec3_Scale(t, -sinRot));
    t = tRot;
    b = bRot;
  }

  const Vec3 org = decal->origin;
  const float r = decal->radius;
  const Vec3 positions[] = {
    Vec3_Add(Vec3_Add(org, Vec3_Scale(t, -r)), Vec3_Scale(b, -r)),
    Vec3_Add(Vec3_Add(org, Vec3_Scale(t,  r)), Vec3_Scale(b, -r)),
    Vec3_Add(Vec3_Add(org, Vec3_Scale(t,  r)), Vec3_Scale(b,  r)),
    Vec3_Add(Vec3_Add(org, Vec3_Scale(t, -r)), Vec3_Scale(b,  r)),
  };

  const int32_t nEdge = face->patch ? (int32_t) sqrtf((float) face->numVertexes) : 0;

  R_ReserveDecalWindings(face->patch ? 4 * (nEdge - 1) : face->numVertexes);

  CmWinding *dw = decalWindings.decal;
  dw->numPoints = 4;
  for (int32_t i = 0; i < dw->numPoints; i++) {
    dw->points[i] = Vec3_Add(positions[i], n);
  }

  CmWinding *fw = decalWindings.face;
  if (face->patch) {
    fw->numPoints = 0;
    for (int32_t i = 0; i < nEdge; i++)
      fw->points[fw->numPoints++] = face->vertexes[i].position;
    for (int32_t j = 1; j < nEdge; j++)
      fw->points[fw->numPoints++] = face->vertexes[j * nEdge + (nEdge - 1)].position;
    for (int32_t i = nEdge - 2; i >= 0; i--)
      fw->points[fw->numPoints++] = face->vertexes[(nEdge - 1) * nEdge + i].position;
    for (int32_t j = nEdge - 2; j >= 1; j--)
      fw->points[fw->numPoints++] = face->vertexes[j * nEdge].position;
  } else {
    fw->numPoints = face->numVertexes;
    for (int32_t i = 0; i < face->numVertexes; i++) {
      fw->points[i] = face->vertexes[i].position;
    }
  }

  const CmWinding *w = Cm_ClipWindingToWindingInto(dw, fw, n, -1.f - ON_EPSILON,
                                                      decalWindings.a, decalWindings.b,
                                                      decalWindings.capacity);

  if (w == NULL || w->numPoints < 3) {
    return;
  }

  const int32_t numTriangles = w->numPoints - 2;
  const int32_t overflow = (int32_t) decals->triangles->count + numTriangles - MAX_BSP_BLOCK_DECALS;
  if (overflow > 0) {
    const int32_t removeCount = Mini(overflow, (int32_t) decals->triangles->count);
    for (int32_t i = 0; i < removeCount; i++) {
      $(decals->triangles, removeAtFast, 0);
    }
  }

  const uint32_t instance = R_AddDecalInstance(decal, normal, t, b);

  for (int32_t i = 0; i < numTriangles; i++) {
    if (decals->triangles->count == MAX_BSP_BLOCK_DECALS) {
      break;
    }

    RenderDecalTriangle triangle;

    const int32_t indices[3] = { 0, i + 1, i + 2 };

    for (int32_t j = 0; j < 3; j++) {
      triangle.vertexes[j].position = w->points[indices[j]];
      triangle.vertexes[j].instance = instance;
    }

    decals->image = (RenderImage *) decal->image;
    $(decals->triangles, add, &triangle);
  }

  decals->dirty = true;
}

/**
 * @brief Projects a decal onto the faces under a BSP node.
 */
static void R_ClipDecalToNode(const RenderView *view,
                              const RenderBspNode *node,
                              const RenderDecal *decal) {

  if (node->contents > CONTENTS_NODE) {
    return;
  }

  const Box3 decalBounds = Box3_FromCenterRadius(decal->origin, decal->radius);

  const RenderBspFace *face = node->faces;
  for (int32_t i = 0; i < node->numFaces; i++, face++) {

    if (!face->patch) {
      continue;
    }

    if (!(face->patch->contents & CONTENTS_MASK_SOLID)) {
      continue;
    }

    if (face->patch->surface & (SURF_SKY | SURF_PORTAL)) {
      continue;
    }

    if (!Box3_Intersects(face->bounds, decalBounds)) {
      continue;
    }

    const Vec3 normal = face->vertexes[0].normal;
    const Vec3 tangent = face->vertexes[0].tangent;
    const Vec3 bitangent = face->vertexes[0].bitangent;

    const float faceDist = Vec3_Dot(Vec3_Subtract(decal->origin, face->vertexes[0].position), normal);
    if (fabsf(faceDist) > decal->radius) {
      continue;
    }

    RenderDecal faceProjected = *decal;
    faceProjected.origin = Vec3_Fmaf(decal->origin, -faceDist, normal);
    faceProjected.radius = sqrtf(decal->radius * decal->radius - faceDist * faceDist);

    if (faceProjected.radius >= 16.f) {
      const Vec3 pos = Vec3_Add(Box3_Center(face->bounds), normal);
      if (Cm_BoxTrace(decal->origin, pos, Box3_Zero(), 0, CONTENTS_SOLID).fraction < 1.f) {
        continue;
      }
    }

    RenderBspBlockDecals *decals = &face->block->decals;
    R_ClipDecalToFace(view, face, &faceProjected, normal, tangent, bitangent, decals);
  }

  const CmBspPlane *plane = node->plane->cm;
  const float dist = Cm_DistanceToPlane(decal->origin, plane);

  if (dist > decal->radius) {
    R_ClipDecalToNode(view, node->children[0], decal);
    return;
  }

  if (dist < -decal->radius) {
    R_ClipDecalToNode(view, node->children[1], decal);
    return;
  }

  RenderDecal projected = *decal;
  
  projected.origin = Vec3_Fmaf(decal->origin, -dist, plane->normal);
  projected.radius = sqrtf(decal->radius * decal->radius - dist * dist);

  const Box3 bounds = Box3_FromCenterRadius(projected.origin, projected.radius);

  face = node->faces;
  for (int32_t i = 0; i < node->numFaces; i++, face++) {

    if (face->patch) {
      continue;
    }

    if (!(face->brushSide->contents & CONTENTS_MASK_SOLID)) {
      continue;
    }

    if (face->brushSide->surface & (SURF_SKY | SURF_PORTAL)) {
      continue;
    }

    if (Cm_DistanceToPlane(decal->origin, face->plane->cm) < -SIDE_EPSILON) {
      continue;
    }

    if (!Box3_Intersects(face->bounds, bounds)) {
      continue;
    }

    if (projected.radius >= 16.f) {
      const Vec3 pos = Vec3_Add(Box3_Center(face->bounds), face->plane->cm->normal);
      if (Cm_BoxTrace(decal->origin, pos, Box3_Zero(), 0, CONTENTS_SOLID).fraction < 1.f) {
        continue;
      }
    }

    const Vec3 normal = face->plane->cm->normal;
    const Vec3 sdir = face->brushSide->axis[0].xyz;
    const Vec3 tdir = face->brushSide->axis[1].xyz;
    Vec3 tangent, bitangent;
    Vec3_Tangents(normal, sdir, tdir, &tangent, &bitangent);

    RenderBspBlockDecals *decals = &face->block->decals;
    R_ClipDecalToFace(view, face, &projected, normal, tangent, bitangent, decals);
  }

  R_ClipDecalToNode(view, node->children[0], decal);
  R_ClipDecalToNode(view, node->children[1], decal);
}

/**
 * @brief Adds new decals and expires old decal triangles, then uploads dirty
 * per-block decal geometry for visible blocks, growing buffers on demand.
 * @remarks Blocks we can't see are skipped and stay dirty until they become
 * visible, matching the culling `R_DrawDecals` applies, so that painted-over
 * geometry elsewhere in the world doesn't re-upload every frame that one of its
 * decals expires.
 */
void R_UpdateDecals(const RenderView *view, CopyPass *pass) {

  for (int32_t i = 0; i < view->numDecals; i++) {
    const RenderDecal *decal = &view->decals[i];

    const RenderEntity *e = view->entities;
    for (int32_t j = 0; j < view->numEntities; j++, e++) {

      if (!IS_BSP_INLINE_MODEL(e->model)) {
        continue;
      }

      RenderBspInlineModel *in = e->model->bspInline;

      RenderDecal d = *decal;
      d.time = view->ticks;
      d.origin = Mat4_Transform(e->inverseMatrix, decal->origin);

      R_ClipDecalToNode(view, in->headNode, &d);
    }
  }

  const RenderEntity *e = view->entities;
  for (int32_t i = 0; i < view->numEntities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    const bool culled = R_CullEntity(view, e);

    RenderBspInlineModel *in = e->model->bspInline;

    RenderBspBlock *block = in->blocks;
    for (int32_t j = 0; j < in->numBlocks; j++, block++) {
      RenderBspBlockDecals *decals = &block->decals;

      for (size_t k = decals->triangles->count; k > 0; ) {
        const RenderDecalTriangle *t = VectorElement(decals->triangles, RenderDecalTriangle, --k);

        const uint32_t reference = t->vertexes->instance;
        const RenderDecalInstance *instance = R_DecalInstance(reference);

        if (view->ticks - instance->time >= instance->lifetime ||
            (reference >> 24) != instance->generation) {
          $(decals->triangles, removeAtFast, k);
          decals->dirty = true;
        }
      }

      const int32_t numVertexes = (int32_t) decals->triangles->count * 3;
      if (numVertexes == 0 || !decals->dirty) {
        continue;
      }

      if (culled) {
        continue;
      }

      if (block->query && !block->query->result) {
        continue;
      }

      if (R_CulludeBox(view, block->visibleBounds)) {
        continue;
      }

      if (numVertexes > decals->vertexBufferCapacity) {
        decals->vertexBuffer = release(decals->vertexBuffer);
        decals->vertexBuffer = $(rContext.device, createBuffer, &(SDL_GPUBufferCreateInfo) {
          .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
          .size = numVertexes * sizeof(RenderDecalVertex),
        });
        decals->vertexBufferCapacity = numVertexes;
      }

      const void *data = VectorElement(decals->triangles, RenderDecalTriangle, 0);
      $(pass, uploadData, decals->vertexBuffer->buffer, data,
        numVertexes * sizeof(RenderDecalVertex), 0, true);

      decals->dirty = false;
    }
  }

  R_UploadDecalInstances(pass);
}

/**
 * @brief Renders decals projected onto BSP surfaces, alpha-blended and lit by the
 * clustered voxel lights, over the opaque scene (depth-tested, no depth write).
 */
void R_DrawDecals(const RenderView *view, RenderPass *pass) {

  assert(rModels.world);

  CommandBuffer *commands = rContext.device->commands;

  const RenderBspModel *bsp = rModels.world->bsp;
  Framebuffer *framebuffer = view->framebuffer;

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  $(commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &rUniforms.block, sizeof(rUniforms.block));

  $(pass, bindPipeline, decalPipeline.pipeline);

  SDL_GPUBuffer *storage[] = {
    rLights.bspBuffer->buffer,
    rLights.dynamicBuffer->buffer,
    bsp->voxels.lightDataBuffer->buffer,
    bsp->voxels.lightIndicesBuffer ? bsp->voxels.lightIndicesBuffer->buffer : rLights.voxelFallbackBuffer->buffer,
  };
  $(pass, bindFragmentStorageBuffers, 0, storage, 4);

  SDL_GPUBuffer *instances[] = { module.buffer->buffer };
  $(pass, bindVertexStorageBuffers, 0, instances, 1);

  const RenderEntity *e = view->entities;
  for (int32_t i = 0; i < view->numEntities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    if (R_CullEntity(view, e)) {
      continue;
    }

    $(commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, e->matrix.array, sizeof(e->matrix));

    if (!IS_WORLDSPAWN(e->model)) {
      $(commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, &e->activeDynamicLights, sizeof(e->activeDynamicLights));
    }

    const RenderBspInlineModel *in = e->model->bspInline;
    const RenderBspBlock *block = in->blocks;
    for (int32_t j = 0; j < in->numBlocks; j++, block++) {

      if (block->query && !block->query->result) {
        continue;
      }

      if (R_CulludeBox(view, block->visibleBounds)) {
        continue;
      }

      if (IS_WORLDSPAWN(e->model)) {
        $(commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, &block->activeDynamicLights, sizeof(block->activeDynamicLights));
      }

      const RenderBspBlockDecals *decals = &block->decals;

      const int32_t numVertexes = (int32_t) decals->triangles->count * 3;
      if (numVertexes == 0 || !decals->vertexBuffer || !decals->image || !decals->image->texture) {
        continue;
      }

      $(pass, bindFragmentSamplers, 0, &(SDL_GPUTextureSamplerBinding) {
        .texture = decals->image->texture->texture,
        .sampler = decalPipeline.diffusemapSampler->sampler,
      }, 1);

      $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = decals->vertexBuffer->buffer }, 1);

      $(pass, drawPrimitives, numVertexes, 1, 0, 0);

      rStats->decalDrawElements++;
    }
  }
}

/**
 * @brief Builds the decal pipeline (decal_vs/decal_fs) and its samplers.
 */
static void R_InitDecalPipeline(void) {

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = rSceneSamples;

  info.depth_stencil_state.enable_depth_write = false;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  info.rasterizer_state.enable_depth_bias = true;
  info.rasterizer_state.depth_bias_constant_factor = -1.f;
  info.rasterizer_state.depth_bias_slope_factor = -1.f;

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = &(SDL_GPUVertexBufferDescription) {
      .slot = 0,
      .pitch = sizeof(RenderDecalVertex),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    },
    .num_vertex_buffers = 1,
    .vertex_attributes = (SDL_GPUVertexAttribute[]) {
      { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(RenderDecalVertex, position) },
      { .location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT, .offset = offsetof(RenderDecalVertex, instance) },
    },
    .num_vertex_attributes = 2,
  };

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = (SDL_GPUColorTargetDescription[]) {
      {
        .format = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT,
        .blend_state = GPU_BlendStateAlpha,
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

  decalPipeline.pipeline = $(rContext.device, loadGraphicsPipeline,
    "shaders/decal_vs", &(SDL_GPUShaderCreateInfo) {
      .stage = SDL_GPU_SHADERSTAGE_VERTEX,
      .num_storage_buffers = 1,
      .num_uniform_buffers = 2,
    },
    "shaders/decal_fs", &(SDL_GPUShaderCreateInfo) {
      .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
      .num_samplers = 1,
      .num_storage_buffers = 4,
      .num_uniform_buffers = 2,
    },
    &info);

  decalPipeline.diffusemapSampler = $(rContext.device, createSamplerLinearClamp);
}

/**
 * @brief Releases the decal pipeline and samplers.
 */
static void R_ShutdownDecalPipeline(void) {

  decalPipeline.pipeline = release(decalPipeline.pipeline);
  decalPipeline.diffusemapSampler = release(decalPipeline.diffusemapSampler);
}

/**
 * @brief Rebuilds the decal pipeline and samplers, leaving the instances that
 * the decals presently in the world reference intact.
 */
void R_UpdateDecalPipeline(void) {
  R_ShutdownDecalPipeline();
  R_InitDecalPipeline();
}

/**
 * @brief Builds the decal instance buffer and pipeline.
 */
void R_InitDecals(void) {

  memset(&module, 0, sizeof(module));

  module.buffer = $(rContext.device, createBuffer, &(SDL_GPUBufferCreateInfo) {
    .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
    .size = sizeof(module.instances),
  });

  module.transferBuffer = $(rContext.device, createTransferBuffer, &(SDL_GPUTransferBufferCreateInfo) {
    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    .size = sizeof(module.instances),
  });

  R_InitDecalPipeline();
}

/**
 * @brief Releases the decal instance buffer and pipeline.
 */
void R_ShutdownDecals(void) {

  R_ShutdownDecalPipeline();

  module.buffer = release(module.buffer);
  module.transferBuffer = release(module.transferBuffer);
}
