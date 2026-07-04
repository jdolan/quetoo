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

  /**
   * @brief The voxel light-data sampler (nearest, clamp) for integer texelFetch.
   */
  Sampler *voxel_data_sampler;
} r_decal_pipeline;

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

static void R_RemoveDecalTriangleAtIndexFast(Vector *triangles, size_t index) {

  assert(triangles);
  assert(index < triangles->count);

  if (index < triangles->count - 1) {
    memcpy(
      VectorElement(triangles, r_decal_triangle_t, index),
      VectorElement(triangles, r_decal_triangle_t, triangles->count - 1),
      sizeof(r_decal_triangle_t)
    );
  }

  triangles->count--;
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

  cm_winding_t *dw = Cm_AllocWinding(4);
  dw->num_points = 4;
  for (int32_t i = 0; i < dw->num_points; i++) {
    dw->points[i] = Vec3_Add(positions[i], n);
  }

  cm_winding_t *fw;
  if (face->patch) {
    // Patch face vertices are a row-major grid, not a polygon winding.
    // Extract the perimeter vertices in winding order.
    const int32_t n_edge = (int32_t) sqrtf((float) face->num_vertexes);
    const int32_t perimeter = 4 * (n_edge - 1);
    fw = Cm_AllocWinding(perimeter);
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
    fw = Cm_AllocWinding(face->num_vertexes);
    fw->num_points = face->num_vertexes;
    for (int32_t i = 0; i < face->num_vertexes; i++) {
      fw->points[i] = face->vertexes[i].position;
    }
  }

  cm_winding_t *w = Cm_ClipWindingToWinding(dw, fw, n, -1.f - ON_EPSILON);
  //cm_winding_t *w = Cm_CopyWinding(dw);

  Cm_FreeWinding(dw);
  Cm_FreeWinding(fw);

  if (w == NULL || w->num_points < 3) {
    if (w) {
      Cm_FreeWinding(w);
    }
    return;
  }

  const color32_t color = Color_Color32(decal->color);
  const vec2_t atlas_min = decal->image->texcoords.xy;
  const vec2_t atlas_max = decal->image->texcoords.zw;
  const vec2_t atlas_size = Vec2_Subtract(atlas_max, atlas_min);

  const int32_t num_triangles = w->num_points - 2;
  const int32_t overflow = (int32_t) decals->triangles->count + num_triangles - MAX_BSP_BLOCK_DECALS;
  if (overflow > 0) {
    const int32_t remove_count = Mini(overflow, (int32_t) decals->triangles->count);
    for (int32_t i = 0; i < remove_count; i++) {
      $(decals->triangles, removeAt, 0);
    }
  }
  
  for (int32_t i = 0; i < num_triangles; i++) {
    if (decals->triangles->count == MAX_BSP_BLOCK_DECALS) {
      break;
    }

    r_decal_triangle_t triangle;
    
    const int32_t indices[3] = { 0, i + 1, i + 2 };

    for (int32_t j = 0; j < 3; j++) {
      const vec3_t pos = w->points[indices[j]];
      triangle.vertexes[j].position = pos;
      triangle.vertexes[j].normal = normal;

      const vec3_t delta = Vec3_Subtract(pos, org);
      const float x = (Vec3_Dot(delta, t) / r) * 0.5f + 0.5f;
      const float y = (Vec3_Dot(delta, b) / r) * 0.5f + 0.5f;

      triangle.vertexes[j].texcoord = Vec2_Add(atlas_min, Vec2(x * atlas_size.x, y * atlas_size.y));
      triangle.vertexes[j].color = color;
      triangle.vertexes[j].time = decal->time;
      triangle.vertexes[j].lifetime = decal->lifetime;
    }
    
    decals->image = (r_image_t *) decal->image;
    $(decals->triangles, add, &triangle);
  }

  decals->dirty = true;

  Cm_FreeWinding(w);
}

/**
 * @brief Recurses down the tree to project the decal onto faces.
 * @details Decal geometry is accumulated on the containing `r_bsp_block_t` node.
 */
static void R_ClipDecalToNode(const r_view_t *view,
                              const r_bsp_node_t *node,
                              const r_decal_t *decal) {

  if (node->contents > CONTENTS_NODE) {
    return;
  }

  // Patch faces may be at any orientation relative to the node plane,
  // so we must check them before the BSP plane early-outs below.

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

  // BSP plane recursion for brush faces

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
 * @brief Add new decals from the view and expiring the existing ones.
 */
void R_UpdateDecals(const r_view_t *view) {

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

    r_bsp_inline_model_t *in = e->model->bsp_inline;

    r_bsp_block_t *block = in->blocks;
    for (int32_t j = 0; j < in->num_blocks; j++, block++) {
      r_bsp_block_decals_t *decals = &block->decals;

      for (size_t k = decals->triangles->count; k > 0; ) {
        const r_decal_triangle_t *v = VectorElement(decals->triangles, r_decal_triangle_t, --k);

        if (view->ticks - v->vertexes->time >= v->vertexes->lifetime) {
          R_RemoveDecalTriangleAtIndexFast(decals->triangles, k);
          decals->dirty = true;
        }
      }
    }
  }
}

/**
 * @brief Uploads any dirty per-block decal geometry, growing buffers on demand.
 */
static void R_UploadDecals(const r_view_t *view) {

  CommandBuffer *commands = r_context.device->commands;
  CopyPass *copyPass = NULL;

  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    const r_bsp_inline_model_t *in = e->model->bsp_inline;
    r_bsp_block_t *block = in->blocks;
    for (int32_t j = 0; j < in->num_blocks; j++, block++) {

      r_bsp_block_decals_t *decals = &block->decals;

      const int32_t num_vertexes = (int32_t) decals->triangles->count * 3;
      if (num_vertexes == 0 || !decals->dirty) {
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

      if (copyPass == NULL) {
        copyPass = $(commands, beginCopyPass);
      }

      const void *data = VectorElement(decals->triangles, r_decal_triangle_t, 0);
      $(copyPass, uploadData, decals->vertex_buffer->buffer, data,
        num_vertexes * sizeof(r_decal_vertex_t), 0, true);

      decals->dirty = false;
    }
  }

  if (copyPass) {
    release(copyPass);
  }
}

/**
 * @brief Renders decals projected onto BSP surfaces, alpha-blended and lit by the
 * clustered voxel lights, over the opaque scene (depth-tested, no depth write).
 */
void R_DrawDecals(const r_view_t *view) {

  if (!r_decal_pipeline.pipeline || !r_models.world) {
    return;
  }

  CommandBuffer *commands = r_context.device->commands;
  if (!commands) {
    return;
  }

  if (!view->framebuffer) {
    return;
  }

  R_UploadDecals(view);

  const r_bsp_model_t *bsp = r_models.world->bsp;
  Framebuffer *framebuffer = view->framebuffer;

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
  $(commands, pushFragmentUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

  $(pass, bindPipeline, r_decal_pipeline.pipeline);

  // Voxel light data + the shared lights / voxel-index storage (decal family:
  // samplers 0=atlas, 1=voxel data; storage 2=lights, 3=voxel indices).
  $(pass, bindFragmentSamplers, 1, &(SDL_GPUTextureSamplerBinding) {
    .texture = bsp->voxels.light_data->texture->texture,
    .sampler = r_decal_pipeline.voxel_data_sampler->sampler,
  }, 1);

  SDL_GPUBuffer *storage[] = {
    r_lights.buffer->buffer,
    bsp->voxels.light_indices_buffer ? bsp->voxels.light_indices_buffer->buffer
                                     : r_lights.buffer->buffer,
  };
  $(pass, bindFragmentStorageBuffers, 0, storage, 2);

  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    $(commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, e->matrix.array, sizeof(e->matrix));

    const r_bsp_inline_model_t *in = e->model->bsp_inline;
    const r_bsp_block_t *block = in->blocks;
    for (int32_t j = 0; j < in->num_blocks; j++, block++) {

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

  pass = release(pass);
}

/**
 * @brief Builds the decal pipeline (decal_vs/decal_fs) and its samplers.
 */
void R_InitDecals(void) {

  Shader *vertexShader = $(r_context.device, loadShader, "shaders/decal_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 2, // globals (0) + locals/model (1)
  });

  Shader *fragmentShader = $(r_context.device, loadShader, "shaders/decal_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = 2,        // texture_diffusemap (atlas) + texture_voxel_light_data
    .num_storage_buffers = 2, // lights_block + voxel_light_indices_block
    .num_uniform_buffers = 1, // globals (0)
  });

  const Framebuffer *framebuffer = r_context.device->framebuffer;

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = r_scene_samples;
  info.vertex_shader = vertexShader->shader;
  info.fragment_shader = fragmentShader->shader;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

  // Decals are coplanar with their surface; test against the opaque depth but do
  // not write it, and bias slightly toward the camera so they always win LEQUAL.
  info.depth_stencil_state.enable_depth_write = false;
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
      { .location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(r_decal_vertex_t, normal) },
      { .location = 2, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = offsetof(r_decal_vertex_t, texcoord) },
      { .location = 3, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, .offset = offsetof(r_decal_vertex_t, color) },
      { .location = 4, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT, .offset = offsetof(r_decal_vertex_t, time) },
      { .location = 5, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT, .offset = offsetof(r_decal_vertex_t, lifetime) },
    },
    .num_vertex_attributes = 6,
  };

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = &(SDL_GPUColorTargetDescription) {
      .format = R_SCENE_COLOR_FORMAT,
      .blend_state = GPU_BlendStateAlpha,
    },
    .num_color_targets = 1,
    .depth_stencil_format = framebuffer->depthFormat,
    .has_depth_stencil_target = true,
  };

  r_decal_pipeline.pipeline = $(r_context.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);

  r_decal_pipeline.diffusemap_sampler = $(r_context.device, createSampler, &(SDL_GPUSamplerCreateInfo) {
    .min_filter = SDL_GPU_FILTER_LINEAR,
    .mag_filter = SDL_GPU_FILTER_LINEAR,
    .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
    .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .enable_anisotropy = true,
    .max_anisotropy = R_Anisotropy(),
  });

  r_decal_pipeline.voxel_data_sampler = $(r_context.device, createSampler, &(SDL_GPUSamplerCreateInfo) {
    .min_filter = SDL_GPU_FILTER_NEAREST,
    .mag_filter = SDL_GPU_FILTER_NEAREST,
    .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
    .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
  });
}

/**
 * @brief Releases the decal pipeline and samplers.
 */
void R_ShutdownDecals(void) {

  r_decal_pipeline.pipeline = release(r_decal_pipeline.pipeline);
  r_decal_pipeline.diffusemap_sampler = release(r_decal_pipeline.diffusemap_sampler);
  r_decal_pipeline.voxel_data_sampler = release(r_decal_pipeline.voxel_data_sampler);
}

