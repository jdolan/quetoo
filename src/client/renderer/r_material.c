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
 * @brief Registers a material's stage media dependencies.
 */
static void R_RegisterMaterial(RenderMedia *self) {
  RenderMaterial *material = (RenderMaterial *) self;

  for (RenderStage *stage = material->stages; stage; stage = stage->next) {
    if (stage->media) {
      R_RegisterDependency(self, stage->media);
    }
  }
}

/**
 * @brief Frees a material's collision material.
 */
static void R_FreeMaterial(RenderMedia *self) {

  Cm_FreeMaterial(((RenderMaterial *) self)->cm);
}

/**
 * @brief Loads an animation for a material stage.
 */
static RenderAnimation *R_LoadStageAnimation(const RenderMaterial *material, RenderStage *stage, int32_t index) {

  const RenderImage *images[stage->cm->animation.numFrames];
  const RenderImage **out = images;

  for (int32_t i = 0; i < stage->cm->animation.numFrames; i++, out++) {

    Asset *frame = &stage->cm->animation.frames[i];
    if (*frame->path) {
      *out = R_LoadImage(frame->path, IMG_MATERIAL);
    } else {
      *out = R_LoadImage("textures/common/notex", IMG_MATERIAL);
      Com_Warn("Failed to resolve frame: %d: %s\n", i, stage->cm->asset.name);
    }
  }

  return R_CreateAnimation(va("%s_%d_animation", material->media.name, index), stage->cm->animation.numFrames, images);
}

/**
 * @brief Appends a stage to a material's stage list.
 */
static void R_AppendStage(RenderMaterial *m, RenderStage *s) {

  if (m->stages == NULL) {
    m->stages = s;
  } else {
    RenderStage *stages = m->stages;
    while (stages->next) {
      stages = stages->next;
    }
    stages->next = s;
  }
}

/**
 * @brief Material surface loading helper.
 */

/**
 * @brief Loads a material surface and rescales it to the requested size.
 */
static SDL_Surface *R_LoadMaterialSurface(int32_t w, int32_t h, const char *path) {

  SDL_Surface *surface = Img_LoadSurface(path);
  if (surface) {
    if (w || h) {
      if (surface->w != w || surface->h != h) {
        Com_Warn("Material asset %s is not %dx%d, resizing..\n", path, w, h);

        SDL_Surface *scaled = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
        SDL_BlitSurfaceScaled(surface, NULL, scaled, NULL, SDL_SCALEMODE_NEAREST);

        SDL_DestroySurface(surface);
        surface = scaled;
      }
    }
  }

  return surface;
}

/**
 * @brief Creates a solid-color material surface.
 */
static SDL_Surface *R_CreateMaterialSurface(int32_t w, int32_t h, Color32 color) {

  SDL_Surface *surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);

  SDL_memset4(surface->pixels, color.rgba, w * h);

  return surface;
}

/**
 * @brief Normalizes height data in a normalmap's alpha channel.
 */
static void R_NormalizeMaterialHeightmap(SDL_Surface *normalmap) {

  const int32_t w = normalmap->w;
  const int32_t h = normalmap->h;

  bool hasHeightmap = false;
  Color32 *pixels = normalmap->pixels;
  for (int32_t i = 0; i < w * h; i++) {
    if (pixels[i].a != 255) {
      hasHeightmap = true;
      break;
    }
  }

  if (!hasHeightmap) {
    return;
  }

  float min = 255.f;
  float max = 0.f;

  for (int32_t i = 0; i < w * h; i++) {
    min = Minf(min, pixels[i].a);
    max = Maxf(max, pixels[i].a);
  }

  const float range = max - min;

  if (range > 0.f) {
    for (int32_t i = 0; i < w * h; i++) {
      pixels[i].a = (pixels[i].a - min) / range * 255.f;
    }
  }
}

/**
 * @brief Derives a grayscale specular map from a diffuse map.
 */
static SDL_Surface *R_CreateSpecularmap(const SDL_Surface *diffusemap) {

  const Color32 *in = diffusemap->pixels;

  SDL_Surface *specularmap = SDL_CreateSurface(diffusemap->w, diffusemap->h, SDL_PIXELFORMAT_RGBA32);
  Color32 *out = specularmap->pixels;

  for (int32_t i = 0; i < diffusemap->w; i++) {
    for (int32_t j = 0; j < diffusemap->h; j++, in++, out++) {
      out->r = out->g = out->b = (byte) (in->r + in->g + in->b) / 3.f;
      out->a = 255;
    }
  }

  return specularmap;
}

/**
 * @brief Resolves the media for a material's stages.
 */
static void R_ResolveMaterialStages(RenderMaterial *material) {
  int32_t numStages = 0;

  const CmMaterial *cm = material->cm;
  for (const CmStage *cs = cm->stages; cs; cs = cs->next, numStages++) {

    RenderStage *stage = (RenderStage *) Mem_LinkMalloc(sizeof(RenderStage), material);
    stage->cm = cs;
    stage->flags = cs->flags;

    if (cm->surface & SURF_PORTAL) {
      if (!q_strcmp(cs->asset.name, cm->diffusemap.name)) {
        stage->flags |= STAGE_PORTAL;
      }
    }

    if (*stage->cm->asset.path) {
      if (stage->cm->flags & STAGE_ANIMATION) {
        stage->media = (RenderMedia *) R_LoadStageAnimation(material, stage, numStages);
      } else {
        stage->media = (RenderMedia *) R_LoadImage(stage->cm->asset.path, IMG_MATERIAL);
      }

      assert(stage->media);

      R_RegisterDependency((RenderMedia *) material, stage->media);
    }

    R_AppendStage(material, stage);
  }

  Com_Debug(DEBUG_RENDERER, "Resolved material %s with %d stages\n", material->cm->name, numStages);
}

/**
 * @brief Creates a renderer material from a collision material.
 */
static RenderMaterial *R_ResolveMaterial(CmMaterial *cm) {
  char key[MAX_QPATH];

  Cm_MaterialPath(cm->name, key, sizeof(key), cm->context);

  RenderMaterial *material = (RenderMaterial *) R_AllocMedia(key, sizeof(RenderMaterial), R_MEDIA_MATERIAL);
  material->cm = cm;

  material->media.Register = R_RegisterMaterial;
  material->media.Free = R_FreeMaterial;

  R_RegisterMedia((RenderMedia *) material);

  material->texture = (RenderImage *) R_AllocMedia(va("%s_texture", material->media.name), sizeof(RenderImage), R_MEDIA_IMAGE);
  material->texture->type = IMG_MATERIAL;
  material->texture->media.Free = R_FreeImage;

  R_RegisterDependency((RenderMedia *) material, (RenderMedia *) material->texture);

  Cm_ResolveMaterial(cm);
  
  SDL_Surface *diffusemap = NULL;
  if (*cm->diffusemap.path) {
    if ((diffusemap = Img_LoadSurface(cm->diffusemap.path))) {
      Com_Debug(DEBUG_RENDERER, "Loaded diffusemap %s for %s\n", cm->diffusemap.path, cm->basename);
    } else {
      if (cm->context == ASSET_CONTEXT_PLAYERS) {
        Com_Debug(DEBUG_RENDERER, "Failed to load diffusemap %s for %s\n", cm->diffusemap.path, cm->basename);
      } else {
        Com_Warn("Failed to load diffusemap %s for %s\n", cm->diffusemap.path, cm->basename);
      }
      diffusemap = Img_LoadSurface("textures/common/notex");
    }
  } else {
    if (cm->context == ASSET_CONTEXT_PLAYERS) {
      // third-party player models frequently omit skins for decorative or FX-only surfaces
      Com_Debug(DEBUG_RENDERER, "Failed to load diffusemap for %s\n", cm->basename);
    } else {
      Com_Warn("Failed to load diffusemap for %s\n", cm->basename);
    }
    diffusemap = Img_LoadSurface("textures/common/notex");
  }

  const int32_t w = material->texture->width = diffusemap->w;
  const int32_t h = material->texture->height = diffusemap->h;

  const size_t layerSize = w * h * 4;

  switch (cm->context) {
    case ASSET_CONTEXT_TEXTURES:
    case ASSET_CONTEXT_MODELS:
    case ASSET_CONTEXT_PLAYERS: {

      if (cm->context == ASSET_CONTEXT_MODELS
          || cm->context == ASSET_CONTEXT_PLAYERS) {
        cm->shadow = 0.f;
      }

      SDL_Surface *normalmap = NULL;
      if (*cm->normalmap.path) {
        normalmap = R_LoadMaterialSurface(w, h, cm->normalmap.path);
        if (normalmap == NULL) {
          Com_Warn("Failed to load normalmap %s for %s\n", cm->normalmap.path, cm->basename);
          normalmap = R_CreateMaterialSurface(w, h, MakeColor32(127, 127, 255, 255));
        }
      } else {
        normalmap = R_CreateMaterialSurface(w, h, MakeColor32(127, 127, 255, 255));
      }

      R_NormalizeMaterialHeightmap(normalmap);

      SDL_Surface *specularmap = NULL;
      if (*cm->specularmap.path) {
        specularmap = R_LoadMaterialSurface(w, h, cm->specularmap.path);
        if (specularmap == NULL) {
          Com_Warn("Failed to load specularmap %s for %s\n", cm->specularmap.path, cm->basename);
          specularmap = R_CreateSpecularmap(diffusemap);
        }
      } else {
        specularmap = R_CreateSpecularmap(diffusemap);
      }
      
      SDL_Surface *tintmap = NULL;
      if (*cm->tintmap.path) {
        tintmap = R_LoadMaterialSurface(w, h, cm->tintmap.path);
        if (tintmap == NULL) {
          Com_Warn("Failed to load tintmap %s for %s\n", cm->tintmap.path, cm->basename);
          tintmap = R_CreateMaterialSurface(diffusemap->w, diffusemap->h, MakeColor32(0, 0, 0, 0));
        }
      } else {
        tintmap = R_CreateMaterialSurface(diffusemap->w, diffusemap->h, MakeColor32(0, 0, 0, 0));
      }

      material->texture->depth = 4;

      byte *data = malloc(layerSize * material->texture->depth);

      memcpy(data + 0 * layerSize, diffusemap->pixels, layerSize);
      memcpy(data + 1 * layerSize, normalmap->pixels, layerSize);
      memcpy(data + 2 * layerSize, specularmap->pixels, layerSize);
      memcpy(data + 3 * layerSize, tintmap->pixels, layerSize);

      const int32_t levels = (int32_t) floorf(log2f((float) Mini(w, h))) + 1;

      material->texture->texture = $(rContext.device, createTexture, &(SDL_GPUTextureCreateInfo) {
        .type = SDL_GPU_TEXTURETYPE_2D_ARRAY,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .width = w,
        .height = h,
        .layer_count_or_depth = material->texture->depth,
        .num_levels = levels,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
      }, data);

      free(data);

      CommandBuffer *commands = $(rContext.device, acquireCommandBuffer);
      $(commands, generateMipmaps, material->texture->texture->texture);
      $(commands, submit);
      release(commands);

      SDL_DestroySurface(normalmap);
      SDL_DestroySurface(specularmap);
      SDL_DestroySurface(tintmap);
    }
      break;

    default:
      material->texture->texture = $(rContext.device, createTexture, &(SDL_GPUTextureCreateInfo) {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .width = w,
        .height = h,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
      }, diffusemap->pixels);
      break;
  }

  $(material->texture->texture, setName, material->texture->media.name);

  material->color = Img_Color(diffusemap);

  SDL_DestroySurface(diffusemap);

  R_ResolveMaterialStages(material);

  return material;
}

/**
 * @brief Populates per-draw material uniforms.
 */
void R_MaterialUniforms(const RenderMaterial *material, int32_t surface, RenderMaterialUniforms *out) {

  const CmMaterial *cm = material->cm;

  memset(out, 0, sizeof(*out));

  out->surface = surface;
  out->alphaTest = cm->alphaTest * r_alphaTest->value;
  out->roughness = cm->roughness * r_roughness->value;
  out->hardness = cm->hardness * r_hardness->value;
  out->specularity = cm->specularity * r_specularity->value;
  out->parallax = cm->parallax * r_parallax->value;
  out->shadow = cm->shadow * r_parallaxShadow->value;
}

/**
 * @brief Returns a stable drift value for a draw or entity stage.
 */
static float R_StageDriftHash(const void *a, const void *b) {
  uint32_t h = (uint32_t) ((uintptr_t) a >> 4) ^ (uint32_t) ((uintptr_t) b >> 4);
  h ^= h >> 16;
  h *= 0x7feb352dU;
  h ^= h >> 15;
  h *= 0x846ca68bU;
  h ^= h >> 16;
  return h / (float) UINT32_MAX;
}

/**
 * @brief Populates stage uniforms and resolves stage textures.
 */
bool R_StageUniforms(const RenderView *view, const RenderEntity *entity, const RenderBspDrawElements *draw, const RenderStage *stage,
                     RenderMaterialUniforms *out, SDL_GPUTexture **texture, SDL_GPUTexture **textureNext) {

  const CmStage *cm = stage->cm;

  out->lerp = 0.f;

  out->flags = stage->flags;
  out->color = cm->color.vec4;
  out->stOrigin = draw ? draw->stOrigin : Vec2_Zero();
  out->stretch = MakeVec2(cm->stretch.amplitude, cm->stretch.hz);
  out->scroll = MakeVec2(cm->scroll.s, cm->scroll.t);
  out->scale = MakeVec2(cm->scale.s, cm->scale.t);
  out->terrain = MakeVec2(cm->terrain.floor, cm->terrain.ceil);
  out->warp = MakeVec2(cm->warp.hz, cm->warp.amplitude);
  out->pulse = cm->pulse.hz;
  out->drift = cm->pulse.drift * R_StageDriftHash(entity ? (const void *) entity : (const void *) draw, stage);
  out->rotate = cm->rotate.hz;
  out->dirtmap = cm->dirtmap.intensity;
  out->lighting = cm->lighting.intensity;
  out->emissive = cm->emissive;
  out->shell = cm->shell.radius;

  *texture = NULL;
  *textureNext = NULL;

  if (stage->media == NULL) {
    return false;
  }

  switch (stage->media->type) {
    case R_MEDIA_IMAGE:
    case R_MEDIA_ATLAS_IMAGE: {
      const RenderImage *image = (const RenderImage *) stage->media;
      if (image->texture) {
        *texture = image->texture->texture;
        *textureNext = image->texture->texture;
      }
    }
      break;

    case R_MEDIA_ANIMATION: {
      const RenderAnimation *animation = (const RenderAnimation *) stage->media;
      if (animation->numFrames == 0) {
        return false;
      }

      int32_t frame;
      float lerp = 0.f;

      if (cm->animation.fps == 0.f && entity != NULL) {
        frame = entity->frame;
        if (cm->flags & STAGE_ANIM_LERP) {
          lerp = entity->lerp;
        }
      } else {
        const float drift = cm->animation.drift * R_StageDriftHash(entity ? (const void *) entity : (const void *) draw, stage);
        const float frameF = (view->ticks / 1000.f + drift) * cm->animation.fps;
        frame = (int32_t) frameF;
        if (cm->flags & STAGE_ANIM_LERP) {
          lerp = frameF - floorf(frameF);
        }
      }

      const RenderImage *cur = animation->frames[((frame % animation->numFrames) + animation->numFrames) % animation->numFrames];
      *texture = cur->texture ? cur->texture->texture : NULL;

      if (cm->flags & STAGE_ANIM_LERP) {
        const RenderImage *next = animation->frames[(((frame + 1) % animation->numFrames) + animation->numFrames) % animation->numFrames];
        *textureNext = next->texture ? next->texture->texture : NULL;
        out->lerp = lerp;
      } else {
        *textureNext = *texture;
      }
    }
      break;

    default:
      return false;
  }

  return *texture != NULL && *textureNext != NULL;
}

/**
 * @brief Finds an existing material for the specified name and context.
 */
RenderMaterial *R_FindMaterial(const char *name, AssetContext context) {
  char key[MAX_QPATH];
  char basename[MAX_QPATH];
  
  StripExtension(name, basename);
  Cm_MaterialPath(basename, key, sizeof(key), context);

  return (RenderMaterial *) R_FindMedia(key, R_MEDIA_MATERIAL);
}

/**
 * @brief Loads a material for the specified asset name and context.
 */
RenderMaterial *R_LoadMaterial(const char *name, AssetContext context) {

  if (name == NULL || *name == '\0') {
    Com_Warn("Empty material name\n");
    return NULL;
  }

  RenderMaterial *material = R_FindMaterial(name, context);
  if (material == NULL) {

    CmMaterial *cm = Cm_LoadMaterial(name, context);

    material = R_ResolveMaterial(cm);
  }

  assert(material->cm);

  return material;
}

/**
 * @brief Saves one dirty material during enumeration.
 */
static void R_SaveMaterials_enumerator(const RenderMedia *media, void *data) {

  if (media->type == R_MEDIA_MATERIAL) {
    RenderMaterial *material = (RenderMaterial *) media;
    if (material->cm->dirty) {
      if (Cm_SaveMaterial(material->cm)) {
        material->cm->dirty = false;
        (*(int32_t *) data)++;
      }
    }
  }
}

/**
 * @brief Saves all dirty materials.
 */
void R_SaveMaterials_f(void) {

  int32_t count = 0;

  R_EnumerateMedia(R_SaveMaterials_enumerator, &count);

  Com_Print("Saved %d material(s)\n", count);
}
