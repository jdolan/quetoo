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
  Sampler *diffusemap_sampler;

} r_decal_pipeline;

/**
 * @brief The decal instances, shared by every block and read by decal_vs.
 * @remarks A ring: instances are appended as decals are clipped, and are never
 * revisited afterwards. A block's triangles reference their instance by index,
 * rather than each vertex carrying its own copy of the decal.
 */
static struct {
  r_decal_instance_t instances[MAX_DECAL_INSTANCES];

  Buffer *buffer;

  /**
   * @brief The ring cursor, and the generation it is presently writing.
   */
  uint32_t next;
  uint32_t generation;

  /**
   * @brief The instances appended since the last upload.
   */
  uint32_t first_pending;
  uint32_t num_pending;

} r_decals;

/**
 * @brief Adds a decal to the view for rendering in the current frame.
 */
void R_AddDecal(r_view_t *view, const r_decal_t *decal) {

  assert(decal);
  assert(decal->image);
  assert(decal->radius > 0.f);
  assert(decal->lifetime > 0);

  if (view->num_decals == MAX_DECALS) {
    Com_Warn("MAX_DECALS\n");
    return;
  }

  r_decal_t *out = &view->decals[view->num_decals++];

  *out = *decal;
}

/**
 * @brief Per-thread scratch windings for decal clipping, so that clipping a
 * decal to a face performs no allocations. Freed with the rest of
 * `MEM_TAG_POLYLIB` at shutdown.
 */
static _Thread_local struct {
  cm_winding_t *decal;
  cm_winding_t *face;
  cm_winding_t *a, *b;
  int32_t max_face_points;
  int32_t capacity;
} r_decal_windings;

/**
 * @brief Grows the per-thread scratch windings to accommodate a face of
 * `face_points` points.
 */
static void R_ReserveDecalWindings(int32_t face_points) {

  if (r_decal_windings.decal == NULL) {
    r_decal_windings.decal = Cm_AllocWinding(4);
  }

  if (face_points > r_decal_windings.max_face_points) {

    if (r_decal_windings.face) {
      Cm_FreeWinding(r_decal_windings.face);
      Cm_FreeWinding(r_decal_windings.a);
      Cm_FreeWinding(r_decal_windings.b);
    }

    r_decal_windings.capacity = 4 + 4 * face_points;
    r_decal_windings.face = Cm_AllocWinding(face_points);
    r_decal_windings.a = Cm_AllocWinding(r_decal_windings.capacity);
    r_decal_windings.b = Cm_AllocWinding(r_decal_windings.capacity);
    r_decal_windings.max_face_points = face_points;
  }
}

/**
 * @brief Appends the instance for a decal clipped to a single face.
 * @return The reference for the decal's vertexes to carry.
 */
static uint32_t R_AddDecalInstance(const r_decal_t *decal,
                                   const vec3_t normal,
                                   const vec3_t tangent,
                                   const vec3_t bitangent) {

  const uint32_t index = r_decals.next;
  const uint32_t generation = r_decals.generation;

  r_decal_instance_t *instance = r_decals.instances + index;

  instance->origin = Vec3_ToVec4(decal->origin, decal->radius);
  instance->normal = Vec3_ToVec4(normal, 0.f);
  instance->tangent = Vec3_ToVec4(tangent, 0.f);
  instance->bitangent = Vec3_ToVec4(bitangent, 0.f);
  instance->texcoords = decal->image->texcoords;
  instance->color = decal->color.vec4;
  instance->time = decal->time;
  instance->lifetime = decal->lifetime;
  instance->generation = generation;

  if (r_decals.num_pending == 0) {
    r_decals.first_pending = index;
  }

  if (r_decals.num_pending < MAX_DECAL_INSTANCES) {
    r_decals.num_pending++;
  }

  r_decals.next++;

  if (r_decals.next == MAX_DECAL_INSTANCES) {
    r_decals.next = 0;
    r_decals.generation = (r_decals.generation + 1) & 0xff;
  }

  return (generation << 24) | index;
}

/**
 * @brief Resolves the instance a decal vertex references.
 */
static const r_decal_instance_t *R_DecalInstance(uint32_t reference) {
  return r_decals.instances + (reference & 0xffffff);
}

/**
 * @brief Uploads the instances appended since the last frame.
 * @remarks This does not cycle, because an instance is never written twice, and
 * the ring cannot have wrapped onto instances an in-flight frame may still read.
 */
static void R_UploadDecalInstances(CopyPass *pass) {

  if (r_decals.num_pending == 0) {
    return;
  }

  const uint32_t first = r_decals.num_pending == MAX_DECAL_INSTANCES ? 0 : r_decals.first_pending;
  const uint32_t head = (uint32_t) Mini((int32_t) r_decals.num_pending, (int32_t) (MAX_DECAL_INSTANCES - first));

  $(r_decals.buffer, uploadWithPass, pass, r_decals.instances + first,
    head * sizeof(r_decal_instance_t), first * sizeof(r_decal_instance_t), false);

  if (r_decals.num_pending > head) {
    $(r_decals.buffer, uploadWithPass, pass, r_decals.instances,
      (r_decals.num_pending - head) * sizeof(r_decal_instance_t), 0, false);
  }

  r_decals.num_pending = 0;
}

/**
 * @brief Clips a decal to a face and adds the resulting triangles to the face's block.
 */
static void R_ClipDecalToFace(const r_view_t *view,
                              const r_bsp_face_t *face,
                              const r_decal_t *decal,
                              const vec3_t normal,
                              const vec3_t tangent,
                              const vec3_t bitangent,
                              r_bsp_block_decals_t *decals) {

  vec3_t n = normal;
  vec3_t t = tangent, b = bitangent;

  if (decal->rotation != 0.f) {
    const float cos_rot = cosf(decal->rotation);
    const float sin_rot = sinf(decal->rotation);
    const vec3_t t_rot = Vec3_Add(Vec3_Scale(t, cos_rot), Vec3_Scale(b, sin_rot));
    const vec3_t b_rot = Vec3_Add(Vec3_Scale(b, cos_rot), Vec3_Scale(t, -sin_rot));
    t = t_rot;
    b = b_rot;
  }

  const vec3_t org = decal->origin;
  const float r = decal->radius;
  const vec3_t positions[] = {
    Vec3_Add(Vec3_Add(org, Vec3_Scale(t, -r)), Vec3_Scale(b, -r)),
    Vec3_Add(Vec3_Add(org, Vec3_Scale(t,  r)), Vec3_Scale(b, -r)),
    Vec3_Add(Vec3_Add(org, Vec3_Scale(t,  r)), Vec3_Scale(b,  r)),
    Vec3_Add(Vec3_Add(org, Vec3_Scale(t, -r)), Vec3_Scale(b,  r)),
  };

  const int32_t n_edge = face->patch ? (int32_t) sqrtf((float) face->num_vertexes) : 0;

  R_ReserveDecalWindings(face->patch ? 4 * (n_edge - 1) : face->num_vertexes);

  cm_winding_t *dw = r_decal_windings.decal;
  dw->num_points = 4;
  for (int32_t i = 0; i < dw->num_points; i++) {
    dw->points[i] = Vec3_Add(positions[i], n);
  }

  cm_winding_t *fw = r_decal_windings.face;
  if (face->patch) {
    fw->num_points = 0;
    for (int32_t i = 0; i < n_edge; i++)
      fw->points[fw->num_points++] = face->vertexes[i].position;
    for (int32_t j = 1; j < n_edge; j++)
      fw->points[fw->num_points++] = face->vertexes[j * n_edge + (n_edge - 1)].position;
    for (int32_t i = n_edge - 2; i >= 0; i--)
      fw->points[fw->num_points++] = face->vertexes[(n_edge - 1) * n_edge + i].position;
    for (int32_t j = n_edge - 2; j >= 1; j--)
      fw->points[fw->num_points++] = face->vertexes[j * n_edge].position;
  } else {
    fw->num_points = face->num_vertexes;
    for (int32_t i = 0; i < face->num_vertexes; i++) {
      fw->points[i] = face->vertexes[i].position;
    }
  }

  const cm_winding_t *w = Cm_ClipWindingToWindingInto(dw, fw, n, -1.f - ON_EPSILON,
                                                      r_decal_windings.a, r_decal_windings.b,
                                                      r_decal_windings.capacity);

  if (w == NULL || w->num_points < 3) {
    return;
  }

  const int32_t num_triangles = w->num_points - 2;
  const int32_t overflow = (int32_t) decals->triangles->count + num_triangles - MAX_BSP_BLOCK_DECALS;
  if (overflow > 0) {
    const int32_t remove_count = Mini(overflow, (int32_t) decals->triangles->count);
    for (int32_t i = 0; i < remove_count; i++) {
      $(decals->triangles, removeAtFast, 0);
    }
  }

  const uint32_t instance = R_AddDecalInstance(decal, normal, t, b);

  for (int32_t i = 0; i < num_triangles; i++) {
    if (decals->triangles->count == MAX_BSP_BLOCK_DECALS) {
      break;
    }

    r_decal_triangle_t triangle;

    const int32_t indices[3] = { 0, i + 1, i + 2 };

    for (int32_t j = 0; j < 3; j++) {
      triangle.vertexes[j].position = w->points[indices[j]];
      triangle.vertexes[j].instance = instance;
    }

    decals->image = (r_image_t *) decal->image;
    $(decals->triangles, add, &triangle);
  }

  decals->dirty = true;
}

/**
 * @brief Projects a decal onto the faces under a BSP node.
 */
static void R_ClipDecalToNode(const r_view_t *view,
                              const r_bsp_node_t *node,
                              const r_decal_t *decal) {

  if (node->contents > CONTENTS_NODE) {
    return;
  }

  const box3_t decal_bounds = Box3_FromCenterRadius(decal->origin, decal->radius);

  const r_bsp_face_t *face = node->faces;
  for (int32_t i = 0; i < node->num_faces; i++, face++) {

    if (!face->patch) {
      continue;
    }

    if (!(face->patch->contents & CONTENTS_MASK_SOLID)) {
      continue;
    }

    if (face->patch->surface & SURF_SKY) {
      continue;
    }

    if (!Box3_Intersects(face->bounds, decal_bounds)) {
      continue;
    }

    const vec3_t normal = face->vertexes[0].normal;
    const vec3_t tangent = face->vertexes[0].tangent;
    const vec3_t bitangent = face->vertexes[0].bitangent;

    const float face_dist = Vec3_Dot(Vec3_Subtract(decal->origin, face->vertexes[0].position), normal);
    if (fabsf(face_dist) > decal->radius) {
      continue;
    }

    r_decal_t face_projected = *decal;
    face_projected.origin = Vec3_Fmaf(decal->origin, -face_dist, normal);
    face_projected.radius = sqrtf(decal->radius * decal->radius - face_dist * face_dist);

    if (face_projected.radius >= 16.f) {
      const vec3_t pos = Vec3_Add(Box3_Center(face->bounds), normal);
      if (Cm_BoxTrace(decal->origin, pos, Box3_Zero(), 0, CONTENTS_SOLID).fraction < 1.f) {
        continue;
      }
    }

    r_bsp_block_decals_t *decals = &face->block->decals;
    R_ClipDecalToFace(view, face, &face_projected, normal, tangent, bitangent, decals);
  }

  const cm_bsp_plane_t *plane = node->plane->cm;
  const float dist = Cm_DistanceToPlane(decal->origin, plane);

  if (dist > decal->radius) {
    R_ClipDecalToNode(view, node->children[0], decal);
    return;
  }

  if (dist < -decal->radius) {
    R_ClipDecalToNode(view, node->children[1], decal);
    return;
  }

  r_decal_t projected = *decal;
  
  projected.origin = Vec3_Fmaf(decal->origin, -dist, plane->normal);
  projected.radius = sqrtf(decal->radius * decal->radius - dist * dist);

  const box3_t bounds = Box3_FromCenterRadius(projected.origin, projected.radius);

  face = node->faces;
  for (int32_t i = 0; i < node->num_faces; i++, face++) {

    if (face->patch) {
      continue;
    }

    if (!(face->brush_side->contents & CONTENTS_MASK_SOLID)) {
      continue;
    }

    if (face->brush_side->surface & SURF_SKY) {
      continue;
    }

    if (Cm_DistanceToPlane(decal->origin, face->plane->cm) < -SIDE_EPSILON) {
      continue;
    }

    if (!Box3_Intersects(face->bounds, bounds)) {
      continue;
    }

    if (projected.radius >= 16.f) {
      const vec3_t pos = Vec3_Add(Box3_Center(face->bounds), face->plane->cm->normal);
      if (Cm_BoxTrace(decal->origin, pos, Box3_Zero(), 0, CONTENTS_SOLID).fraction < 1.f) {
        continue;
      }
    }

    const vec3_t normal = face->plane->cm->normal;
    const vec3_t sdir = face->brush_side->axis[0].xyz;
    const vec3_t tdir = face->brush_side->axis[1].xyz;
    vec3_t tangent, bitangent;
    Vec3_Tangents(normal, sdir, tdir, &tangent, &bitangent);

    r_bsp_block_decals_t *decals = &face->block->decals;
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
void R_UpdateDecals(const r_view_t *view, CopyPass *pass) {

  for (int32_t i = 0; i < view->num_decals; i++) {
    const r_decal_t *decal = &view->decals[i];

    const r_entity_t *e = view->entities;
    for (int32_t j = 0; j < view->num_entities; j++, e++) {

      if (!IS_BSP_INLINE_MODEL(e->model)) {
        continue;
      }

      r_bsp_inline_model_t *in = e->model->bsp_inline;

      r_decal_t d = *decal;
      d.time = view->ticks;
      d.origin = Mat4_Transform(e->inverse_matrix, decal->origin);

      R_ClipDecalToNode(view, in->head_node, &d);
    }
  }

  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    const bool culled = R_CullEntity(view, e);

    r_bsp_inline_model_t *in = e->model->bsp_inline;

    r_bsp_block_t *block = in->blocks;
    for (int32_t j = 0; j < in->num_blocks; j++, block++) {
      r_bsp_block_decals_t *decals = &block->decals;

      for (size_t k = decals->triangles->count; k > 0; ) {
        const r_decal_triangle_t *t = VectorElement(decals->triangles, r_decal_triangle_t, --k);

        const uint32_t reference = t->vertexes->instance;
        const r_decal_instance_t *instance = R_DecalInstance(reference);

        if (view->ticks - instance->time >= instance->lifetime ||
            (reference >> 24) != instance->generation) {
          $(decals->triangles, removeAtFast, k);
          decals->dirty = true;
        }
      }

      const int32_t num_vertexes = (int32_t) decals->triangles->count * 3;
      if (num_vertexes == 0 || !decals->dirty) {
        continue;
      }

      if (culled) {
        continue;
      }

      if (block->query && !block->query->result) {
        continue;
      }

      if (R_CulludeBox(view, block->visible_bounds)) {
        continue;
      }

      if (num_vertexes > decals->vertex_buffer_capacity) {
        decals->vertex_buffer = release(decals->vertex_buffer);
        decals->vertex_buffer = $(r_context.device, createBuffer, &(SDL_GPUBufferCreateInfo) {
          .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
          .size = num_vertexes * sizeof(r_decal_vertex_t),
        });
        decals->vertex_buffer_capacity = num_vertexes;
      }

      const void *data = VectorElement(decals->triangles, r_decal_triangle_t, 0);
      $(pass, uploadData, decals->vertex_buffer->buffer, data,
        num_vertexes * sizeof(r_decal_vertex_t), 0, true);

      decals->dirty = false;
    }
  }

  R_UploadDecalInstances(pass);
}

/**
 * @brief Renders decals projected onto BSP surfaces, alpha-blended and lit by the
 * clustered voxel lights, over the opaque scene (depth-tested, no depth write).
 */
void R_DrawDecals(const r_view_t *view, RenderPass *pass) {

  if (!r_models.world) {
    return;
  }

  CommandBuffer *commands = r_context.device->commands;

  const r_bsp_model_t *bsp = r_models.world->bsp;
  Framebuffer *framebuffer = view->framebuffer;

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  $(commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

  $(pass, bindPipeline, r_decal_pipeline.pipeline);

  SDL_GPUBuffer *storage[] = {
    r_lights.bsp_buffer->buffer,
    r_lights.dynamic_buffer->buffer,
    bsp->voxels.light_data_buffer->buffer,
    bsp->voxels.light_indices_buffer ? bsp->voxels.light_indices_buffer->buffer : r_lights.bsp_buffer->buffer,
  };
  $(pass, bindFragmentStorageBuffers, 0, storage, 4);

  SDL_GPUBuffer *instances[] = { r_decals.buffer->buffer };
  $(pass, bindVertexStorageBuffers, 0, instances, 1);

  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    if (R_CullEntity(view, e)) {
      continue;
    }

    $(commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, e->matrix.array, sizeof(e->matrix));

    if (!IS_WORLDSPAWN(e->model)) {
      $(commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, &e->active_dynamic_lights, sizeof(e->active_dynamic_lights));
    }

    const r_bsp_inline_model_t *in = e->model->bsp_inline;
    const r_bsp_block_t *block = in->blocks;
    for (int32_t j = 0; j < in->num_blocks; j++, block++) {

      if (block->query && !block->query->result) {
        continue;
      }

      if (R_CulludeBox(view, block->visible_bounds)) {
        continue;
      }

      if (IS_WORLDSPAWN(e->model)) {
        $(commands, pushFragmentUniformData, SLOT_UNIFORMS_LOCALS, &block->active_dynamic_lights, sizeof(block->active_dynamic_lights));
      }

      const r_bsp_block_decals_t *decals = &block->decals;

      const int32_t num_vertexes = (int32_t) decals->triangles->count * 3;
      if (num_vertexes == 0 || !decals->vertex_buffer || !decals->image || !decals->image->texture) {
        continue;
      }

      $(pass, bindFragmentSamplers, 0, &(SDL_GPUTextureSamplerBinding) {
        .texture = decals->image->texture->texture,
        .sampler = r_decal_pipeline.diffusemap_sampler->sampler,
      }, 1);

      $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = decals->vertex_buffer->buffer }, 1);

      $(pass, drawPrimitives, num_vertexes, 1, 0, 0);
    }
  }
}

/**
 * @brief Builds the decal pipeline (decal_vs/decal_fs) and its samplers.
 */
static void R_InitDecalPipeline(void) {

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = r_scene_samples;

  info.depth_stencil_state.enable_depth_write = false;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  info.rasterizer_state.enable_depth_bias = true;
  info.rasterizer_state.depth_bias_constant_factor = -1.f;
  info.rasterizer_state.depth_bias_slope_factor = -1.f;

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = &(SDL_GPUVertexBufferDescription) {
      .slot = 0,
      .pitch = sizeof(r_decal_vertex_t),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    },
    .num_vertex_buffers = 1,
    .vertex_attributes = (SDL_GPUVertexAttribute[]) {
      { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_decal_vertex_t, position) },
      { .location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT, .offset = offsetof(r_decal_vertex_t, instance) },
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

  r_decal_pipeline.pipeline = $(r_context.device, loadGraphicsPipeline,
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

  r_decal_pipeline.diffusemap_sampler = $(r_context.device, createSamplerLinearClamp);
}

/**
 * @brief Releases the decal pipeline and samplers.
 */
static void R_ShutdownDecalPipeline(void) {

  r_decal_pipeline.pipeline = release(r_decal_pipeline.pipeline);
  r_decal_pipeline.diffusemap_sampler = release(r_decal_pipeline.diffusemap_sampler);
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

  memset(&r_decals, 0, sizeof(r_decals));

  r_decals.buffer = $(r_context.device, createBuffer, &(SDL_GPUBufferCreateInfo) {
    .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
    .size = sizeof(r_decals.instances),
  });

  R_InitDecalPipeline();
}

/**
 * @brief Releases the decal instance buffer and pipeline.
 */
void R_ShutdownDecals(void) {

  R_ShutdownDecalPipeline();

  r_decals.buffer = release(r_decals.buffer);
}
