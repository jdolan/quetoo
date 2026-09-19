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
 * @brief Sprite sampler bindings.
 */
enum {
  SPRITE_SAMPLER_DIFFUSE,
  SPRITE_SAMPLER_NEXT_DIFFUSE,
  SPRITE_SAMPLER_DEPTH_ATTACHMENT,
};

/**
 * @brief Per-batch sprite lighting data.
 */
typedef struct {
  RenderActiveDynamicLights activeDynamicLights;
} RenderSpriteLocals;

/**
 * @brief Sprite rendering resources.
 */
static struct {
  
  /**
   * @brief The sprite instance cache, and the buffer it uploads to.
   */
  RenderSpriteInstance instances[MAX_SPRITE_INSTANCES];
  Buffer *instanceBuffer;

  /**
   * @brief The transfer buffer sourcing the instance upload, held for the subsystem's
   * lifetime because the instances are uploaded every frame.
   */
  TransferBuffer *transferBuffer;

  /**
   * @brief The index buffer.
   */
  Buffer *elementsBuffer;
  
  /**
   * @brief The pipeline.
   */
  GraphicsPipeline *pipeline;
  
  /**
   * @brief The sampler.
   */
  Sampler *sampler;

  /**
   * @brief The scene depth sampler.
   */
  Sampler *depthSampler;
} module;

/**
 * @brief Resolves the texture coordinate rect for a sprite image.
 */
static Vec4 R_SpriteTextureCoordinates(const RenderImage *image) {

  if (image->media.type == R_MEDIA_ATLAS_IMAGE) {
    return ((const RenderAtlasImage *) image)->texcoords;
  }

  return MakeVec4(0.f, 0.f, 1.f, 1.f);
}

/**
 * @brief Resolves the bounds of the quad `center + (±a) + (±b)`.
 */
static Box3 R_SpriteBounds(const Vec3 center, const Vec3 a, const Vec3 b) {

  const Vec3 extents = Vec3_Add(Vec3_Fabsf(a), Vec3_Fabsf(b));

  return MakeBox3(Vec3_Subtract(center, extents), Vec3_Add(center, extents));
}

/**
 * @brief Resolves the current sprite image.
 */
static const RenderImage *R_ResolveSpriteImage(const RenderMedia *media, const float life) {

  const RenderImage *image;

  if (media->type == R_MEDIA_ANIMATION) {
    image = R_ResolveAnimation((RenderAnimation *) media, life, 0);
  } else {
    image = (RenderImage *) media;
  }

  return image;
}

/**
 * @brief Adds a sprite to the view.
 */
RenderSprite *R_AddSprite(RenderView *view, const RenderSprite *s) {

  assert(s->media);

  if (view->numSprites == MAX_SPRITES) {
    Com_Debug(DEBUG_RENDERER, "MAX_SPRITES\n");
    return NULL;
  }

  RenderSprite *out = &view->sprites[view->numSprites++];
  *out = *s;

  return out;
}

/**
 * @brief Adds a beam to the view.
 */
RenderBeam *R_AddBeam(RenderView *view, const RenderBeam *b) {

  if (view->numBeams == MAX_BEAMS) {
    Com_Debug(DEBUG_RENDERER, "MAX_BEAMS\n");
    return NULL;
  }

  RenderBeam *out = &view->beams[view->numBeams++];
  *out = *b;

  return out;
}

/**
 * @brief Allocates the next available sprite instance slot in the view.
 * @param instance Filled with the instance to be uploaded, parallel by index.
 */
static RenderSpriteBatch *R_AllocSpriteInstance(RenderView *view, RenderSpriteInstance **instance) {

  if (view->numSpriteInstances == MAX_SPRITE_INSTANCES) {
    Com_Debug(DEBUG_RENDERER, "MAX_SPRITE_INSTANCES\n");
    return NULL;
  }

  const int32_t index = view->numSpriteInstances++;

  RenderSpriteBatch *batch = &view->spriteBatches[index];
  memset(batch, 0, sizeof(*batch));

  *instance = &module.instances[index];
  memset(*instance, 0, sizeof(**instance));

  return batch;
}

/**
 * @brief Builds one sprite quad instance.
 */
static void R_UpdateSpriteQuad(RenderView *view, const RenderSprite *s,
                              const Vec3 right, const Vec3 up) {

  RenderSpriteInstance *instance;

  RenderSpriteBatch *batch = R_AllocSpriteInstance(view, &instance);
  if (!batch) {
    return;
  }

  batch->diffusemap = R_ResolveSpriteImage(s->media, s->life);

  const float aspectRatio = (float) batch->diffusemap->width / (float) batch->diffusemap->height;
  const float halfWidth = (s->size ?: s->width) * .5f;
  const float halfHeight = (s->size ?: s->height) * .5f;

  const Vec3 a = Vec3_Scale(up, halfHeight);
  const Vec3 b = Vec3_Scale(right, halfWidth * aspectRatio);

  instance->center = Vec3_ToVec4(s->origin, 0.f);
  instance->a = Vec3_ToVec4(a, Clampf01(s->lighting));
  instance->b = Vec3_ToVec4(b, 0.f);

  instance->texcoords = R_SpriteTextureCoordinates(batch->diffusemap);

  if (s->media->type == R_MEDIA_ANIMATION) {
    const RenderAnimation *anim = (const RenderAnimation *) s->media;

    batch->nextDiffusemap = R_ResolveAnimation(anim, s->life, 1);
    instance->nextTexcoords = R_SpriteTextureCoordinates(batch->nextDiffusemap);

    const float frame = s->life * anim->numFrames;
    instance->center.w = Clampf01(frame - floorf(frame));
  } else {
    batch->nextDiffusemap = batch->diffusemap;
    instance->nextTexcoords = instance->texcoords;
  }

  instance->color = Vec3_ToVec4(Vec3_Maxf(s->color, Vec3_Zero()), 1.f);

  batch->bounds = R_SpriteBounds(s->origin, a, b);
}

/**
 * @brief Builds sprite instances for a sprite.
 */
static void R_UpdateSprite(RenderView *view, const RenderSprite *s) {

  if (s->flags & SPRITE_AXIAL) {
    const Vec3 up1 = MakeVec3(0.f, 0.f, 1.f);
    const Vec3 right1 = MakeVec3(1.f, 0.f, 0.f);
    const Vec3 right2 = MakeVec3(0.f, 1.f, 0.f);

    R_UpdateSpriteQuad(view, s, right1, up1);
    R_UpdateSpriteQuad(view, s, right2, up1);
  }

  Vec3 dir, right, up;

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

static void R_UpdateBeamQuad(RenderView *view, const RenderBeam *b,
                            const Vec3 right, const Vec4 texcoords) {

  float step = 1.f;
  for (float frac = 0.f; frac < 1.f; ) {

    const Vec3 x = Vec3_Mix(b->start, b->end, frac);
    const Vec3 y = Vec3_Mix(b->start, b->end, frac + step);

    RenderSpriteInstance *instance;

    RenderSpriteBatch *batch = R_AllocSpriteInstance(view, &instance);
    if (!batch) {
      return;
    }

    batch->diffusemap = batch->nextDiffusemap = b->image;

    const Vec3 center = Vec3_Mix(x, y, .5f);
    const Vec3 half = Vec3_Scale(Vec3_Subtract(y, x), .5f);

    instance->center = Vec3_ToVec4(center, 0.f);
    instance->a = Vec3_ToVec4(right, Clampf01(b->lighting));
    instance->b = Vec3_ToVec4(half, 0.f);

    const float xs = Mixf(texcoords.x, texcoords.z, frac);
    const float ys = Mixf(texcoords.x, texcoords.z, frac + step);

    instance->texcoords = instance->nextTexcoords = MakeVec4(xs, texcoords.y, ys, texcoords.w);

    instance->color = Vec3_ToVec4(Vec3_Maxf(b->color, Vec3_Zero()), 1.f);

    batch->bounds = R_SpriteBounds(center, right, half);

    frac += step;
    step = 1.f - frac;
  }
}

/**
 * @brief Builds sprite instances for a beam.
 */
void R_UpdateBeam(RenderView *view, const RenderBeam *b) {
  float length;

  const Vec3 up = Vec3_NormalizeLength(Vec3_Subtract(b->start, b->end), &length);
  length /= b->image->width * (b->size / b->image->height);

  const float halfSize = b->size * .5f;

  const Vec3 arbitrary = fabsf(up.z) < .9f ? MakeVec3(0.f, 0.f, 1.f) : MakeVec3(1.f, 0.f, 0.f);
  const Vec3 right1 = Vec3_Scale(Vec3_Normalize(Vec3_Cross(up, arbitrary)), halfSize);
  const Vec3 right2 = Vec3_Scale(Vec3_Normalize(Vec3_Cross(up, right1)), halfSize);

  Vec4 texcoords = R_SpriteTextureCoordinates(b->image);

  if (b->flags & SPRITE_BEAM_REPEAT) {

    if (b->stretch) {
      length *= b->stretch;
    }

    texcoords.z *= length;

    if (b->translate) {
      texcoords.x += b->translate;
      texcoords.z += b->translate;
    }
  }

  R_UpdateBeamQuad(view, b, right1, texcoords);
  R_UpdateBeamQuad(view, b, right2, texcoords);
}

/**
 * @brief Builds sprite instances and uploads their vertices.
 */
void R_UpdateSprites(RenderView *view, CopyPass *copyPass) {

  const RenderSprite *s = view->sprites;
  for (int32_t i = 0; i < view->numSprites; i++, s++) {
    R_UpdateSprite(view, s);
  }

  const RenderBeam *b = view->beams;
  for (int32_t i = 0; i < view->numBeams; i++, b++) {
    R_UpdateBeam(view, b);
  }

  if (view->numSpriteInstances == 0) {
    return;
  }

  const uint32_t size = (uint32_t) view->numSpriteInstances * sizeof(RenderSpriteInstance);

  $(module.transferBuffer, write, module.instances, size, true);

  $(copyPass, uploadBuffer,
    &(SDL_GPUTransferBufferLocation) { .transfer_buffer = module.transferBuffer->buffer },
    &(SDL_GPUBufferRegion) { .buffer = module.instanceBuffer->buffer, .size = size },
    true);
}

/**
 * @brief Draws batched sprite instances.
 */
void R_DrawSprites(const RenderView *view, RenderPass *pass) {

  assert(rModels.world);

  if (view->numSpriteInstances == 0) {
    return;
  }

  CommandBuffer *commands = rContext.device->commands;

  const RenderBspModel *bsp = rModels.world->bsp;
  Framebuffer *framebuffer = view->framebuffer;

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  $(commands, pushUniformData, SLOT_UNIFORMS_GLOBALS, &rUniforms.block, sizeof(rUniforms.block));

  $(pass, bindPipeline, module.pipeline);
  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) { .buffer = module.elementsBuffer->buffer }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  Texture *depthTexture = $(framebuffer, previousColorTexture, 1);
  $(pass, bindFragmentSamplers, SPRITE_SAMPLER_DEPTH_ATTACHMENT, &(SDL_GPUTextureSamplerBinding) {
    .texture = depthTexture->texture,
    .sampler = module.depthSampler->sampler,
  }, 1);

  SDL_GPUBuffer *storage[] = {
    rLights.bspBuffer->buffer,
    rLights.dynamicBuffer->buffer,
    bsp->voxels.lightDataBuffer->buffer,
    bsp->voxels.lightIndicesBuffer ? bsp->voxels.lightIndicesBuffer->buffer : rLights.voxelFallbackBuffer->buffer,
    module.instanceBuffer->buffer,
  };
  $(pass, bindVertexStorageBuffers, 0, storage, 5);

  int32_t i = 0;
  while (i < view->numSpriteInstances) {

    const RenderSpriteBatch *in = view->spriteBatches + i;

    if (!in->diffusemap || !in->diffusemap->texture || !in->nextDiffusemap || !in->nextDiffusemap->texture) {
      i++;
      continue;
    }

    int32_t batchSize = 1;
    Box3 batchBounds = in->bounds;
    for (int32_t j = i + 1; j < view->numSpriteInstances; j++) {
      const RenderSpriteBatch *batch = view->spriteBatches + j;
      if (batch->diffusemap != in->diffusemap || batch->nextDiffusemap != in->nextDiffusemap) {
        break;
      }
      batchBounds = Box3_Union(batchBounds, batch->bounds);
      batchSize++;
    }

    RenderSpriteLocals locals = { 0 };
    R_ActiveDynamicLights(view, batchBounds, &locals.activeDynamicLights);
    $(commands, pushVertexUniformData, SLOT_UNIFORMS_LOCALS, &locals, sizeof(locals));

    $(pass, bindFragmentSamplers, SPRITE_SAMPLER_DIFFUSE, (SDL_GPUTextureSamplerBinding[]) {
      { .texture = in->diffusemap->texture->texture, .sampler = module.sampler->sampler },
      { .texture = in->nextDiffusemap->texture->texture, .sampler = module.sampler->sampler },
    }, 2);

    $(pass, drawIndexedPrimitives, (uint32_t) batchSize * 6, 1, (uint32_t) i * 6, 0, 0);

    rStats->spriteDrawElements++;

    i += batchSize;
  }
}

/**
 * @brief Builds the sprite pipeline and diffuse sampler.
 */
static void R_InitSpritePipeline(void) {

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.multisample_state.sample_count = rSceneSamples;

  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

  info.depth_stencil_state = (SDL_GPUDepthStencilState) {
    .compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
    .enable_depth_test = true,
    .enable_depth_write = false,
  };

  /*
   * Sprites have no vertex buffer at all; sprite_vs expands each quad from
   * gl_VertexIndex against the static index buffer. Stated explicitly so the
   * pipeline cannot acquire vertex attributes from the shared template.
   */
  info.vertex_input_state = (SDL_GPUVertexInputState) { 0 };

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = (SDL_GPUColorTargetDescription[]) {
      {
        .format = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT,
        .blend_state = GPU_BlendStateAdditive,
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

  module.pipeline = $(rContext.device, loadGraphicsPipeline,
    "shaders/sprite_vs", &(SDL_GPUShaderCreateInfo) {
      .stage = SDL_GPU_SHADERSTAGE_VERTEX,
      .num_storage_buffers = 5,
      .num_uniform_buffers = 2,
    },
    "shaders/sprite_fs", &(SDL_GPUShaderCreateInfo) {
      .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
      .num_samplers = 3,
      .num_uniform_buffers = 1,
    },
    &info);

  module.sampler = $(rContext.device, createSamplerLinearClamp);
  module.depthSampler = $(rContext.device, createSamplerNearestClamp);
}

/**
 * @brief Initializes the sprite subsystem: the static quad index buffer and pipeline.
 */
void R_InitSprites(void) {

  memset(&module, 0, sizeof(module));

  const size_t numElements = MAX_SPRITE_INSTANCES * 6;
  uint32_t *elements = malloc(numElements * sizeof(uint32_t));

  for (int32_t i = 0, v = 0, e = 0; i < MAX_SPRITE_INSTANCES; i++, v += 4, e += 6) {
    elements[e + 0] = v + 0;
    elements[e + 1] = v + 1;
    elements[e + 2] = v + 2;
    elements[e + 3] = v + 0;
    elements[e + 4] = v + 2;
    elements[e + 5] = v + 3;
  }

  module.elementsBuffer = $(rContext.device, createBufferWithConstMem,
      SDL_GPU_BUFFERUSAGE_INDEX, elements, (Uint32) (numElements * sizeof(uint32_t)));

  free(elements);

  module.instanceBuffer = $(rContext.device, createBuffer, &(SDL_GPUBufferCreateInfo) {
    .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
    .size = sizeof(module.instances),
  });

  module.transferBuffer = $(rContext.device, createTransferBuffer, &(SDL_GPUTransferBufferCreateInfo) {
    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    .size = sizeof(module.instances),
  });

  R_InitSpritePipeline();
}

/**
 * @brief Shuts down the sprite subsystem, releasing the pipeline, sampler and buffers.
 */
void R_ShutdownSprites(void) {

  module.pipeline = release(module.pipeline);
  module.sampler = release(module.sampler);
  module.depthSampler = release(module.depthSampler);
  module.instanceBuffer = release(module.instanceBuffer);
  module.elementsBuffer = release(module.elementsBuffer);
  module.transferBuffer = release(module.transferBuffer);
}

/**
 * @brief Rebuilds sprite pipeline resources.
 */
void R_UpdateSpritePipeline(void) {
  R_ShutdownSprites();
  R_InitSprites();
}
