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
 * @brief The sprite program's own binding map, mirroring shaders/sprite_fs.glsl.
 * Not shared with any other pipeline.
 */
enum {
  SPRITE_SAMPLER_DIFFUSE,
  SPRITE_SAMPLER_NEXT_DIFFUSE,
  SPRITE_SAMPLER_DEPTH_ATTACHMENT,
};

/**
 * @brief The sprite pipeline (sprite_vs/sprite_fs) and its diffuse sampler.
 */
static struct {
  
  /**
   * @ brief The CPU-side storage for accumulated sprite vertexes.
   */
  r_sprite_vertex_t vertexes[MAX_SPRITE_INSTANCES * 4];

  /**
   * @ brief The vertex buffer.
   */
  Buffer *vertex_buffer;
  int32_t vertex_buffer_capacity; // in vertices

  /**
   *@ brief The elements buffer, statically pre-computed for @c MAX_SPRITE_INSTANCES quads.
   */
  Buffer *elements_buffer;
  
  /**
   * @brief The pipeline.
   */
  GraphicsPipeline *pipeline;
  
  /**
   * @brief The sampler.
   */
  Sampler *sampler;

  /**
   * @brief Nearest/clamp sampler for the scene depth attachment (soft particles).
   */
  Sampler *depth_sampler;
} r_sprite_draw;

/**
 * @brief Computes the texture coordinates for the given image, handling both atlas and regular images.
 */
static void R_SpriteTextureCoordinates(const r_image_t *image, vec2_t *tl, vec2_t *tr, vec2_t *br, vec2_t *bl) {

  if (image->media.type == R_MEDIA_ATLAS_IMAGE) {
    const r_atlas_image_t *atlas_image = (r_atlas_image_t *) image;
    *tl = Vec2(atlas_image->texcoords.x, atlas_image->texcoords.y);
    *tr = Vec2(atlas_image->texcoords.z, atlas_image->texcoords.y);
    *br = Vec2(atlas_image->texcoords.z, atlas_image->texcoords.w);
    *bl = Vec2(atlas_image->texcoords.x, atlas_image->texcoords.w);
  } else {
    *tl = Vec2(0.f, 0.f);
    *tr = Vec2(1.f, 0.f);
    *br = Vec2(1.f, 1.f);
    *bl = Vec2(0.f, 1.f);
  }
}

/**
 * @brief Resolves the image for a sprite, animating it if the media is an animation.
 */
static const r_image_t *R_ResolveSpriteImage(const r_media_t *media, const float life) {

  const r_image_t *image;

  if (media->type == R_MEDIA_ANIMATION) {
    image = R_ResolveAnimation((r_animation_t *) media, life, 0);
  } else {
    image = (r_image_t *) media;
  }

  return image;
}

/**
 * @brief Copies the specified sprite into the view structure, provided it
 * passes a basic visibility test, and returns a pointer to the sprite
 * slot it fills.
 */
r_sprite_t *R_AddSprite(r_view_t *view, const r_sprite_t *s) {

  assert(s->media);

  if (view->num_sprites == MAX_SPRITES) {
    Com_Debug(DEBUG_RENDERER, "MAX_SPRITES\n");
    return NULL;
  }

  r_sprite_t *out = &view->sprites[view->num_sprites++];
  *out = *s;

  return out;
}

/**
 * @brief Copies the specified sprite into the view structure, provided it
 * passes a basic visibility test, and returns a pointer to the sprite
 * slot it fills.
 */
r_beam_t *R_AddBeam(r_view_t *view, const r_beam_t *b) {

  if (view->num_beams == MAX_BEAMS) {
    Com_Debug(DEBUG_RENDERER, "MAX_BEAMS\n");
    return NULL;
  }

  r_beam_t *out = &view->beams[view->num_beams++];
  *out = *b;

  return out;
}

/**
 * @brief Allocates the next available sprite instance slot in the view.
 */
static r_sprite_instance_t *R_AllocSpriteInstance(r_view_t *view) {

  if (view->num_sprite_instances == MAX_SPRITE_INSTANCES) {
    Com_Debug(DEBUG_RENDERER, "MAX_SPRITE_INSTANCES\n");
    return NULL;
  }

  const int32_t index = view->num_sprite_instances++;

  r_sprite_instance_t *in = &view->sprite_instances[index];
  memset(in, 0, sizeof(*in));

  in->vertexes = r_sprite_draw.vertexes + 4 * index;
  in->elements = (void *) (sizeof(uint32_t) * 6 * index);

  return in;
}

/**
 * @brief Builds a billboard quad sprite instance from the sprite definition, right, and up vectors.
 */
static void R_UpdateSpriteQuad(r_view_t *view, const r_sprite_t *s,
                              const vec3_t right, const vec3_t up) {

  r_sprite_instance_t *in = R_AllocSpriteInstance(view);
  if (!in) {
    return;
  }

  in->flags = s->flags;
  in->diffusemap = R_ResolveSpriteImage(s->media, s->life);

  const float aspect_ratio = (float) in->diffusemap->width / (float) in->diffusemap->height;
  const float half_width = (s->size ?: s->width) * .5f;
  const float half_height = (s->size ?: s->height) * .5f;

  const vec3_t u = Vec3_Scale(up, half_height),
         d = Vec3_Scale(up, -half_height),
         l = Vec3_Scale(right, -half_width * aspect_ratio),
         r = Vec3_Scale(right, half_width * aspect_ratio);

  in->vertexes[0].position = Vec3_Add(Vec3_Add(s->origin, u), l);
  in->vertexes[1].position = Vec3_Add(Vec3_Add(s->origin, u), r);
  in->vertexes[2].position = Vec3_Add(Vec3_Add(s->origin, d), r);
  in->vertexes[3].position = Vec3_Add(Vec3_Add(s->origin, d), l);

  R_SpriteTextureCoordinates(in->diffusemap, &in->vertexes[0].diffusemap,
                         &in->vertexes[1].diffusemap,
                         &in->vertexes[2].diffusemap,
                         &in->vertexes[3].diffusemap);

  if (s->media->type == R_MEDIA_ANIMATION) {
    const r_animation_t *anim = (const r_animation_t *) s->media;

    in->next_diffusemap = R_ResolveAnimation(anim, s->life, 1);

    R_SpriteTextureCoordinates(in->next_diffusemap, &in->vertexes[0].next_diffusemap,
                            &in->vertexes[1].next_diffusemap,
                            &in->vertexes[2].next_diffusemap,
                            &in->vertexes[3].next_diffusemap);

    const float frame = s->life * anim->num_frames;
    const float lerp = Clampf01(frame - floorf(frame));

    in->vertexes[0].lerp =
    in->vertexes[1].lerp =
    in->vertexes[2].lerp =
    in->vertexes[3].lerp = lerp * 255;
  } else {
    in->next_diffusemap = in->diffusemap;

    in->vertexes[0].next_diffusemap = in->vertexes[0].diffusemap;
    in->vertexes[1].next_diffusemap = in->vertexes[1].diffusemap;
    in->vertexes[2].next_diffusemap = in->vertexes[2].diffusemap;
    in->vertexes[3].next_diffusemap = in->vertexes[3].diffusemap;

    in->vertexes[0].lerp =
    in->vertexes[1].lerp =
    in->vertexes[2].lerp =
    in->vertexes[3].lerp = 0;
  }

  in->vertexes[0].color =
  in->vertexes[1].color =
  in->vertexes[2].color =
  in->vertexes[3].color = Color_Color24(Color3fv(s->color));

  in->vertexes[0].lighting =
  in->vertexes[1].lighting =
  in->vertexes[2].lighting =
  in->vertexes[3].lighting = Clampf01(s->lighting) * 255;

  in->bounds = Box3_FromPointsStride(in->vertexes, 4, sizeof(r_sprite_vertex_t));
}

/**
 * @brief Computes orientation vectors and builds sprite quad instance(s) for a single sprite.
 */
static void R_UpdateSprite(r_view_t *view, const r_sprite_t *s) {

  if (s->flags & SPRITE_AXIAL) {
    // Two perpendicular world-space quads
    const vec3_t up1 = Vec3(0.f, 0.f, 1.f);
    const vec3_t right1 = Vec3(1.f, 0.f, 0.f);
    const vec3_t right2 = Vec3(0.f, 1.f, 0.f);

    R_UpdateSpriteQuad(view, s, right1, up1);
    R_UpdateSpriteQuad(view, s, right2, up1);
  }

  vec3_t dir, right, up;

  if (Vec3_Equal(s->dir, Vec3_Zero())) {

    if (s->axis == SPRITE_AXIS_ALL) {
      if (s->rotation) {
        dir = view->angles;
        dir.z = Degrees(s->rotation);
        Vec3_Vectors(dir, NULL, &right, &up);
      } else {
        right = view->right;
        up = view->up;
      }
    } else {
      dir = Vec3_Zero();

      for (int32_t i = 0; i < 3; i++) {
        if (s->axis & (1 << i)) {
          dir.xyz[i] = view->forward.xyz[i];
        }
      }

      dir = Vec3_Euler(Vec3_Normalize(dir));
      dir.z = Degrees(s->rotation);
      Vec3_Vectors(dir, NULL, &right, &up);
    }
  } else {
    dir = Vec3_Euler(s->dir);
    dir.z = Degrees(s->rotation);
    Vec3_Vectors(dir, NULL, &right, &up);
  }

  R_UpdateSpriteQuad(view, s, right, up);
}

static void R_UpdateBeamQuad(r_view_t *view, const r_beam_t *b,
                            const vec3_t right, const vec2_t texcoords[4]) {

  float step = 1.f;
  for (float frac = 0.f; frac < 1.f; ) {

    const vec3_t x = Vec3_Mix(b->start, b->end, frac);
    const vec3_t y = Vec3_Mix(b->start, b->end, frac + step);

    vec3_t positions[4];
    positions[0] = Vec3_Add(x, right);
    positions[1] = Vec3_Add(y, right);
    positions[2] = Vec3_Subtract(y, right);
    positions[3] = Vec3_Subtract(x, right);

    r_sprite_instance_t *in = R_AllocSpriteInstance(view);
    if (!in) {
      return;
    }

    in->flags = b->flags;
    in->diffusemap = in->next_diffusemap = b->image;

    in->vertexes[0].position = positions[0];
    in->vertexes[1].position = positions[1];
    in->vertexes[2].position = positions[2];
    in->vertexes[3].position = positions[3];

    in->vertexes[0].diffusemap = in->vertexes[0].next_diffusemap = texcoords[0];
    in->vertexes[1].diffusemap = in->vertexes[1].next_diffusemap = texcoords[1];
    in->vertexes[2].diffusemap = in->vertexes[2].next_diffusemap = texcoords[2];
    in->vertexes[3].diffusemap = in->vertexes[3].next_diffusemap = texcoords[3];

    const float xs = (texcoords[0].x * (1.f - frac)) + (texcoords[1].x * frac);
    const float ys = (texcoords[0].x * (1.f - (frac + step))) + (texcoords[1].x * (frac + step));

    in->vertexes[0].diffusemap.x = in->vertexes[0].next_diffusemap.x = xs;
    in->vertexes[1].diffusemap.x = in->vertexes[1].next_diffusemap.x = ys;
    in->vertexes[2].diffusemap.x = in->vertexes[2].next_diffusemap.x = ys;
    in->vertexes[3].diffusemap.x = in->vertexes[3].next_diffusemap.x = xs;

    in->vertexes[0].color =
    in->vertexes[1].color =
    in->vertexes[2].color =
    in->vertexes[3].color = Color_Color24(Color3fv(b->color));

    in->vertexes[0].lerp =
    in->vertexes[1].lerp =
    in->vertexes[2].lerp =
    in->vertexes[3].lerp = 0.f;

    in->vertexes[0].lighting =
    in->vertexes[1].lighting =
    in->vertexes[2].lighting =
    in->vertexes[3].lighting = b->lighting;

    in->bounds = Box3_FromPointsStride(in->vertexes, 4, sizeof(r_sprite_vertex_t));

    frac += step;
    step = 1.f - frac;
  }
}

/**
 * @brief Break the beam into segments based on blend depth transitions.
 * Draws two perpendicular quads per segment for a volumetric cross section.
 */
void R_UpdateBeam(r_view_t *view, const r_beam_t *b) {
  float length;

  const vec3_t up = Vec3_NormalizeLength(Vec3_Subtract(b->start, b->end), &length);
  length /= b->image->width * (b->size / b->image->height);

  const float half_size = b->size * .5f;

  // Two fixed perpendicular axes for cross-quad rendering
  const vec3_t arbitrary = fabsf(up.z) < .9f ? Vec3(0.f, 0.f, 1.f) : Vec3(1.f, 0.f, 0.f);
  const vec3_t right1 = Vec3_Scale(Vec3_Normalize(Vec3_Cross(up, arbitrary)), half_size);
  const vec3_t right2 = Vec3_Scale(Vec3_Normalize(Vec3_Cross(up, right1)), half_size);

  vec2_t texcoords[4];
  R_SpriteTextureCoordinates(b->image, &texcoords[0], &texcoords[1], &texcoords[2], &texcoords[3]);

  if (b->flags & SPRITE_BEAM_REPEAT) {

    if (b->stretch) {
      length *= b->stretch;
    }

    texcoords[1].x *= length;
    texcoords[2].x = texcoords[1].x;

    if (b->translate) {
      for (int32_t i = 0; i < 4; i++) {
        texcoords[i].x += b->translate;
      }
    }
  }

  R_UpdateBeamQuad(view, b, right1, texcoords);
  R_UpdateBeamQuad(view, b, right2, texcoords);
}

/**
 * @brief Generate sprite instances from sprites and beams, and update the vertex array.
 */
void R_UpdateSprites(r_view_t *view) {

  const r_sprite_t *s = view->sprites;
  for (int32_t i = 0; i < view->num_sprites; i++, s++) {
    R_UpdateSprite(view, s);
  }

  const r_beam_t *b = view->beams;
  for (int32_t i = 0; i < view->num_beams; i++, b++) {
    R_UpdateBeam(view, b);
  }
}

/**
 * @brief Renders all batched sprite instances to the present framebuffer, blended
 * additively over the opaque scene and depth-tested against it.
 */
void R_DrawSprites(const r_view_t *view) {

  if (!r_sprite_draw.pipeline || view->num_sprite_instances == 0 || !r_models.world) {
    return;
  }

  CommandBuffer *commands = r_context.device->commands;
  if (!commands) {
    return;
  }

  if (!view->framebuffer) {
    return;
  }

  const r_bsp_model_t *bsp = r_models.world->bsp;
  Framebuffer *framebuffer = view->framebuffer;

  const uint32_t count = (uint32_t) view->num_sprite_instances * 4;

  if ((int32_t) count > r_sprite_draw.vertex_buffer_capacity) {
    r_sprite_draw.vertex_buffer = release(r_sprite_draw.vertex_buffer);
    r_sprite_draw.vertex_buffer = $(r_context.device, createBuffer, &(SDL_GPUBufferCreateInfo) {
      .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
      .size = count * sizeof(r_sprite_vertex_t),
    });
    r_sprite_draw.vertex_buffer_capacity = (int32_t) count;
  }

  {
    CopyPass *copyPass = $(commands, beginCopyPass);
    $(copyPass, uploadData, r_sprite_draw.vertex_buffer->buffer, r_sprite_draw.vertexes,
      count * sizeof(r_sprite_vertex_t), 0, true);
    release(copyPass);
  }

  // No depth attachment: the soft-particle fade (sprite_fs) samples the scene
  // depth instead, which also occludes sprites behind the world.
  const SDL_GPUColorTargetInfo color =
      $(framebuffer, colorTargetInfo, 0, SDL_GPU_LOADOP_LOAD, SDL_GPU_STOREOP_STORE, NULL);

  RenderPass *pass = $(commands, beginRenderPass, &color, 1, NULL);

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  $(commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &r_uniforms.block, sizeof(r_uniforms.block));

  $(pass, bindPipeline, r_sprite_draw.pipeline);
  $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = r_sprite_draw.vertex_buffer->buffer }, 1);
  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) { .buffer = r_sprite_draw.elements_buffer->buffer }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  // The float depth copy (scene color target 1), sampled for the soft-particle
  // fade. The opaque lit passes write gl_FragCoord.z there; unlike the real depth
  // buffer it can be sampled (and resolves to single-sample under MSAA).
  Texture *depth_texture = $(framebuffer, resolveColorTexture, 1);
  $(pass, bindFragmentSamplers, SPRITE_SAMPLER_DEPTH_ATTACHMENT, &(SDL_GPUTextureSamplerBinding) {
    .texture = depth_texture->texture,
    .sampler = r_sprite_draw.depth_sampler->sampler,
  }, 1);

  // Clustered voxel lighting for absorptive sprites (sprite family: samplers 0/1
  // diffuse+next, 2 depth, 3 voxel data; storage 0/1 lights + voxel indices).
  // texelFetch ignores the sampler, so the nearest depth sampler is reused.
  $(pass, bindFragmentSamplers, 3, &(SDL_GPUTextureSamplerBinding) {
    .texture = bsp->voxels.light_data->texture->texture,
    .sampler = r_sprite_draw.depth_sampler->sampler,
  }, 1);

  SDL_GPUBuffer *storage[] = {
    r_lights.buffer->buffer,
    bsp->voxels.light_indices_buffer ? bsp->voxels.light_indices_buffer->buffer
                                     : r_lights.buffer->buffer,
  };
  $(pass, bindFragmentStorageBuffers, 0, storage, 2);

  // Draw runs of consecutive instances sharing the same diffuse/next-diffuse image.
  int32_t i = 0;
  while (i < view->num_sprite_instances) {

    const r_sprite_instance_t *in = view->sprite_instances + i;

    if (!in->diffusemap || !in->diffusemap->texture || !in->next_diffusemap || !in->next_diffusemap->texture) {
      i++;
      continue;
    }

    int32_t batch_size = 1;
    for (int32_t j = i + 1; j < view->num_sprite_instances; j++) {
      const r_sprite_instance_t *batch = view->sprite_instances + j;
      if (batch->diffusemap != in->diffusemap || batch->next_diffusemap != in->next_diffusemap) {
        break;
      }
      batch_size++;
    }

    $(pass, bindFragmentSamplers, SPRITE_SAMPLER_DIFFUSE, (SDL_GPUTextureSamplerBinding[]) {
      { .texture = in->diffusemap->texture->texture, .sampler = r_sprite_draw.sampler->sampler },
      { .texture = in->next_diffusemap->texture->texture, .sampler = r_sprite_draw.sampler->sampler },
    }, 2);

    $(pass, drawIndexedPrimitives, (uint32_t) batch_size * 6, 1, (uint32_t) i * 6, 0, 0);

    r_stats.sprite_draw_elements++;

    i += batch_size;
  }

  pass = release(pass);
}

/**
 * @brief Builds the sprite pipeline and diffuse sampler.
 */
static void R_InitSpritePipeline(void) {

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = r_scene_samples;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

  // Sprites sample the scene depth for the soft-particle fade, which also serves
  // as the occlusion test, so the pass carries no depth attachment (and the
  // pipeline no depth-stencil state).
  info.depth_stencil_state = (SDL_GPUDepthStencilState) { 0 };

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = &(SDL_GPUVertexBufferDescription) {
      .slot = 0,
      .pitch = sizeof(r_sprite_vertex_t),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    },
    .num_vertex_buffers = 1,
    .vertex_attributes = (SDL_GPUVertexAttribute[]) {
      {
        .location = 0,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .offset = offsetof(r_sprite_vertex_t, position),
      },
      {
        .location = 1,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
        .offset = offsetof(r_sprite_vertex_t, diffusemap),
      },
      {
        .location = 2,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
        .offset = offsetof(r_sprite_vertex_t, next_diffusemap),
      },
      {
        // .rgb is the sprite color; the fourth byte (lerp) is read but unused here.
        .location = 3,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
        .offset = offsetof(r_sprite_vertex_t, color),
      },
      {
        // .x = lerp, .y = lighting (adjacent bytes).
        .location = 4,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE2_NORM,
        .offset = offsetof(r_sprite_vertex_t, lerp),
      },
    },
    .num_vertex_attributes = 5,
  };

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = &(SDL_GPUColorTargetDescription) {
      .format = R_SCENE_COLOR_FORMAT,
      .blend_state = GPU_BlendStateAdditive,
    },
    .num_color_targets = 1,
  };

  r_sprite_draw.pipeline = $(r_context.device, loadGraphicsPipeline,
    "shaders/sprite_vs", &(SDL_GPUShaderCreateInfo) {
      .stage = SDL_GPU_SHADERSTAGE_VERTEX,
      .num_uniform_buffers = 1,
    },
    "shaders/sprite_fs", &(SDL_GPUShaderCreateInfo) {
      .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
      .num_samplers = 4,
      .num_storage_buffers = 2,
      .num_uniform_buffers = 1,
    },
    &info);

  r_sprite_draw.sampler = $(r_context.device, createSamplerLinearClamp);
  r_sprite_draw.depth_sampler = $(r_context.device, createSamplerNearestClamp);
}

/**
 * @brief Initializes the sprite subsystem: the static quad index buffer and pipeline.
 */
void R_InitSprites(void) {

  memset(&r_sprite_draw, 0, sizeof(r_sprite_draw));

  // Two triangles per instance: (0,1,2) (0,2,3), over MAX_SPRITE_INSTANCES quads.
  const size_t num_elements = MAX_SPRITE_INSTANCES * 6;
  uint32_t *elements = malloc(num_elements * sizeof(uint32_t));

  for (int32_t i = 0, v = 0, e = 0; i < MAX_SPRITE_INSTANCES; i++, v += 4, e += 6) {
    elements[e + 0] = v + 0;
    elements[e + 1] = v + 1;
    elements[e + 2] = v + 2;
    elements[e + 3] = v + 0;
    elements[e + 4] = v + 2;
    elements[e + 5] = v + 3;
  }

  r_sprite_draw.elements_buffer = $(r_context.device, createBufferWithConstMem,
      SDL_GPU_BUFFERUSAGE_INDEX, elements, (Uint32) (num_elements * sizeof(uint32_t)));

  free(elements);

  R_InitSpritePipeline();
}

/**
 * @brief Shuts down the sprite subsystem, releasing the pipeline, sampler and buffers.
 */
void R_ShutdownSprites(void) {

  r_sprite_draw.pipeline = release(r_sprite_draw.pipeline);
  r_sprite_draw.sampler = release(r_sprite_draw.sampler);
  r_sprite_draw.depth_sampler = release(r_sprite_draw.depth_sampler);
  r_sprite_draw.vertex_buffer = release(r_sprite_draw.vertex_buffer);
  r_sprite_draw.elements_buffer = release(r_sprite_draw.elements_buffer);
}

/**
 * @brief Rebuilds the sprite pipeline and samplers in place, for pipeline-bound
 * cvar changes (r_antialias, r_anisotropy, ...) that would otherwise require
 * an r_restart. See R_UpdatePipelines.
 */
void R_UpdateSpritePipeline(void) {
  R_ShutdownSprites();
  R_InitSprites();
}
