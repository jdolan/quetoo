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
 * @brief Register event listener for materials.
 */
static void R_RegisterMaterial(r_media_t *self) {
  r_material_t *material = (r_material_t *) self;

  for (r_stage_t *stage = material->stages; stage; stage = stage->next) {
    if (stage->media) {
      R_RegisterDependency(self, stage->media);
    }
  }
}

/**
 * @brief Free event listener for materials.
 */
static void R_FreeMaterial(r_media_t *self) {

  Cm_FreeMaterial(((r_material_t *) self)->cm);
}

/**
 * @return The number of frames resolved, or -1 on error.
 */
static r_animation_t *R_LoadStageAnimation(const cm_material_t *material, r_stage_t *stage) {

  const r_image_t *images[stage->cm->animation.num_frames];
  const r_image_t **out = images;

  for (int32_t i = 0; i < stage->cm->animation.num_frames; i++, out++) {

    cm_asset_t *frame = &stage->cm->animation.frames[i];
    if (*frame->path) {
      *out = R_LoadImage(frame->path, IMG_MATERIAL);
    } else {
      *out = R_LoadImage("textures/common/notex", IMG_MATERIAL);
      Com_Warn("Failed to resolve frame: %d: %s\n", i, stage->cm->asset.name);
    }
  }

  return R_CreateAnimation(va("%s_%s_animation", material->name, stage->cm->asset.name), stage->cm->animation.num_frames, images);
}

/**
 * @brief Appends a stage to the end of the material's stage list.
 */
static void R_AppendStage(r_material_t *m, r_stage_t *s) {

  if (m->stages == NULL) {
    m->stages = s;
  } else {
    r_stage_t *stages = m->stages;
    while (stages->next) {
      stages = stages->next;
    }
    stages->next = s;
  }
}

/**
 * @brief Normalizes material names to their context, returning the unique media key for subsequent
 * material lookups.
 */

/**
 * @brief Loads the material asset, ensuring it is the specified dimensions for texture layering.
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
 * @brief Creates a solid-color `SDL_Surface` of the given dimensions.
 */
static SDL_Surface *R_CreateMaterialSurface(int32_t w, int32_t h, color32_t color) {

  SDL_Surface *surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);

  SDL_memset4(surface->pixels, color.rgba, w * h);

  return surface;
}

/**
 * @brief Normalizes the heightmap data in the normalmap's alpha channel to
 * maximize the range for parallax occlusion mapping.
 * @details Heightmap data is expected to be pre-baked into the normalmap alpha
 * channel by the offline `pack_heightmaps` tool.
 */
static void R_NormalizeMaterialHeightmap(SDL_Surface *normalmap) {

  const int32_t w = normalmap->w;
  const int32_t h = normalmap->h;

  bool has_heightmap = false;
  color32_t *pixels = normalmap->pixels;
  for (int32_t i = 0; i < w * h; i++) {
    if (pixels[i].a != 255) {
      has_heightmap = true;
      break;
    }
  }

  if (!has_heightmap) {
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
 * @brief Derives a greyscale specularmap surface from the given diffusemap.
 */
static SDL_Surface *R_CreateSpecularmap(const SDL_Surface *diffusemap) {

  const color32_t *in = diffusemap->pixels;

  SDL_Surface *specularmap = SDL_CreateSurface(diffusemap->w, diffusemap->h, SDL_PIXELFORMAT_RGBA32);
  color32_t *out = specularmap->pixels;

  for (int32_t i = 0; i < diffusemap->w; i++) {
    for (int32_t j = 0; j < diffusemap->h; j++, in++, out++) {
      out->r = out->g = out->b = (byte) (in->r + in->g + in->b) / 3.f;
      out->a = 255;
    }
  }

  return specularmap;
}

/**
 * @brief Resolves all asset references in the specified render material's stages
 */
static void R_ResolveMaterialStages(r_material_t *material) {
  int32_t num_stages = 0;

  const cm_material_t *cm = material->cm;
  for (const cm_stage_t *cs = cm->stages; cs; cs = cs->next, num_stages++) {

    r_stage_t *stage = (r_stage_t *) Mem_LinkMalloc(sizeof(r_stage_t), material);
    stage->cm = cs;

    if (*stage->cm->asset.path) {
      if (stage->cm->flags & STAGE_ANIMATION) {
        stage->media = (r_media_t *) R_LoadStageAnimation(cm, stage);
      } else {
        stage->media = (r_media_t *) R_LoadImage(stage->cm->asset.path, IMG_MATERIAL);
      }

      assert(stage->media);

      R_RegisterDependency((r_media_t *) material, stage->media);
    }

    R_AppendStage(material, stage);
  }

  Com_Debug(DEBUG_RENDERER, "Resolved material %s with %d stages\n", material->cm->name, num_stages);
}

/**
 * @brief Resolves all asset references in the specified collision material, yielding a usable
 * renderer material.
 */
static r_material_t *R_ResolveMaterial(cm_material_t *cm) {
  char key[MAX_QPATH];

  Cm_MaterialPath(cm->name, key, sizeof(key), cm->context);

  r_material_t *material = (r_material_t *) R_AllocMedia(key, sizeof(r_material_t), R_MEDIA_MATERIAL);
  material->cm = cm;

  material->media.Register = R_RegisterMaterial;
  material->media.Free = R_FreeMaterial;

  R_RegisterMedia((r_media_t *) material);

  material->texture = (r_image_t *) R_AllocMedia(va("%s_texture", material->cm->basename), sizeof(r_image_t), R_MEDIA_IMAGE);
  material->texture->type = IMG_MATERIAL;

  R_RegisterDependency((r_media_t *) material, (r_media_t *) material->texture);

  Cm_ResolveMaterial(cm);
  
  SDL_Surface *diffusemap = NULL;
  if (*cm->diffusemap.path) {
    if ((diffusemap = Img_LoadSurface(cm->diffusemap.path))) {
      Com_Debug(DEBUG_RENDERER, "Loaded diffusemap %s for %s\n", cm->diffusemap.path, cm->basename);
    } else {
      Com_Warn("Failed to load diffusemap %s for %s\n", cm->diffusemap.path, cm->basename);
      diffusemap = Img_LoadSurface("textures/common/notex");
    }
  } else {
    Com_Warn("Failed to load diffusemap for %s\n", cm->basename);
    diffusemap = Img_LoadSurface("textures/common/notex");
  }

  const int32_t w = material->texture->width = diffusemap->w;
  const int32_t h = material->texture->height = diffusemap->h;

  const size_t layer_size = w * h * 4;

  switch (cm->context) {
    case ASSET_CONTEXT_TEXTURES:
    case ASSET_CONTEXT_MODELS:
    case ASSET_CONTEXT_PLAYERS: {

      SDL_Surface *normalmap = NULL;
      if (*cm->normalmap.path) {
        normalmap = R_LoadMaterialSurface(w, h, cm->normalmap.path);
        if (normalmap == NULL) {
          Com_Warn("Failed to load normalmap %s for %s\n", cm->normalmap.path, cm->basename);
          normalmap = R_CreateMaterialSurface(w, h, Color32(127, 127, 255, 255));
        }
      } else {
        normalmap = R_CreateMaterialSurface(w, h, Color32(127, 127, 255, 255));
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
          tintmap = R_CreateMaterialSurface(diffusemap->w, diffusemap->h, Color32(0, 0, 0, 0));
        }
      } else {
        tintmap = R_CreateMaterialSurface(diffusemap->w, diffusemap->h, Color32(0, 0, 0, 0));
      }

      material->texture->depth = 4;

      byte *data = malloc(layer_size * material->texture->depth);

      memcpy(data + 0 * layer_size, diffusemap->pixels, layer_size);
      memcpy(data + 1 * layer_size, normalmap->pixels, layer_size);
      memcpy(data + 2 * layer_size, specularmap->pixels, layer_size);
      memcpy(data + 3 * layer_size, tintmap->pixels, layer_size);

      // TODO(#864): level 0 only; material mip generation deferred.
      material->texture->texture = $(r_context.device, createTexture, &(SDL_GPUTextureCreateInfo) {
        .type = SDL_GPU_TEXTURETYPE_2D_ARRAY,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .width = w,
        .height = h,
        .layer_count_or_depth = material->texture->depth,
        .num_levels = 1,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
      }, data);

      free(data);

      SDL_DestroySurface(normalmap);
      SDL_DestroySurface(specularmap);
      SDL_DestroySurface(tintmap);
    }
      break;

    default:
      material->texture->texture = $(r_context.device, createTexture, &(SDL_GPUTextureCreateInfo) {
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

  material->color = Img_Color(diffusemap);
  
  SDL_DestroySurface(diffusemap);

  R_ResolveMaterialStages(material);

  return material;
}

/**
 * @brief Finds an existing `r_material_t` from the specified diffusemap.
 */
/**
 * @brief Fills `out` with the per-draw material parameters for `material` and the
 * draw's `surface` flags, pre-multiplied by their r_* cvars. Mirrors the GL
 * `glUniform` material uploads.
 */
void R_MaterialUniforms(const r_material_t *material, int32_t surface, r_material_uniforms_t *out) {

  const cm_material_t *cm = material->cm;

  out->surface = surface;
  out->alpha_test = cm->alpha_test * r_alpha_test->value;
  out->roughness = cm->roughness * r_roughness->value;
  out->hardness = cm->hardness * r_hardness->value;
  out->specularity = cm->specularity * r_specularity->value;
  out->parallax = cm->parallax * r_parallax->value;
  out->shadow = cm->shadow * r_parallax_shadow->value;
  out->_pad = 0.f;
}

/**
 * @brief A stable per-(draw|entity, stage) pseudo-random value in [0, 1], used to
 * de-synchronize pulse and animation timing across surfaces sharing a material.
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
 * @brief Fills `out` with the per-stage parameters for `stage` on `draw` (and
 * `entity`, or NULL for the world), and resolves the stage's current and next
 * animation-frame textures. Returns true if the stage has drawable media.
 * @remarks TODO(#864): R_MEDIA_MATERIAL stage media (material-swap stages) is
 * deferred; such stages are skipped.
 */
bool R_StageUniforms(const r_view_t *view, const r_entity_t *entity,
                     const r_bsp_draw_elements_t *draw, const r_stage_t *stage,
                     r_stage_uniforms_t *out, SDL_GPUTexture **texture, SDL_GPUTexture **texture_next) {

  const cm_stage_t *cm = stage->cm;

  memset(out, 0, sizeof(*out));

  out->flags = cm->flags;
  out->color = cm->color.vec4;
  out->st_origin = draw ? draw->st_origin : Vec2_Zero(); // mesh stages have no draw elements
  out->stretch = Vec2(cm->stretch.amplitude, cm->stretch.hz);
  out->scroll = Vec2(cm->scroll.s, cm->scroll.t);
  out->scale = Vec2(cm->scale.s, cm->scale.t);
  out->terrain = Vec2(cm->terrain.floor, cm->terrain.ceil);
  out->warp = Vec2(cm->warp.hz, cm->warp.amplitude);
  out->pulse = cm->pulse.hz;
  out->drift = cm->pulse.drift * R_StageDriftHash(entity ? (const void *) entity : (const void *) draw, stage);
  out->rotate = cm->rotate.hz;
  out->dirtmap = cm->dirtmap.intensity;
  out->lighting = cm->lighting.intensity;
  out->emissive = cm->emissive;
  out->shell = cm->shell.radius;

  *texture = NULL;
  *texture_next = NULL;

  if (stage->media == NULL) {
    return false;
  }

  switch (stage->media->type) {
    case R_MEDIA_IMAGE:
    case R_MEDIA_ATLAS_IMAGE: {
      const r_image_t *image = (const r_image_t *) stage->media;
      if (image->texture) {
        *texture = image->texture->texture;
        *texture_next = image->texture->texture;
      }
    }
      break;

    case R_MEDIA_ANIMATION: {
      const r_animation_t *animation = (const r_animation_t *) stage->media;
      if (animation->num_frames == 0) {
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
        const float frame_f = (view->ticks / 1000.f + drift) * cm->animation.fps;
        frame = (int32_t) frame_f;
        if (cm->flags & STAGE_ANIM_LERP) {
          lerp = frame_f - floorf(frame_f);
        }
      }

      const r_image_t *cur = animation->frames[((frame % animation->num_frames) + animation->num_frames) % animation->num_frames];
      *texture = cur->texture ? cur->texture->texture : NULL;

      if (cm->flags & STAGE_ANIM_LERP) {
        const r_image_t *next = animation->frames[(((frame + 1) % animation->num_frames) + animation->num_frames) % animation->num_frames];
        *texture_next = next->texture ? next->texture->texture : NULL;
        out->lerp = lerp;
      } else {
        *texture_next = *texture;
      }
    }
      break;

    default:
      return false;
  }

  return *texture != NULL && *texture_next != NULL;
}

r_material_t *R_FindMaterial(const char *name, cm_asset_context_t context) {
  char key[MAX_QPATH];
  char basename[MAX_QPATH];
  
  StripExtension(name, basename);
  Cm_MaterialPath(basename, key, sizeof(key), context);

  return (r_material_t *) R_FindMedia(key, R_MEDIA_MATERIAL);
}

/**
 * @brief Loads the `r_material_t` with the specified asset name and context.
 */
r_material_t *R_LoadMaterial(const char *name, cm_asset_context_t context) {

  r_material_t *material = R_FindMaterial(name, context);
  if (material == NULL) {

    cm_material_t *cm = Cm_LoadMaterial(name, context);

    material = R_ResolveMaterial(cm);
  }

  return material;
}

/**
 * @brief `R_EnumerateMedia` callback that saves dirty materials.
 */
static void R_SaveMaterials_enumerator(const r_media_t *media, void *data) {

  if (media->type == R_MEDIA_MATERIAL) {
    r_material_t *material = (r_material_t *) media;
    if (material->cm->dirty) {
      if (Cm_SaveMaterial(material->cm)) {
        material->cm->dirty = false;
        (*(int32_t *) data)++;
      }
    }
  }
}

/**
 * @brief Saves all dirty materials to disk and clears their dirty flags.
 */
void R_SaveMaterials_f(void) {

  int32_t count = 0;

  R_EnumerateMedia(R_SaveMaterials_enumerator, &count);

  Com_Print("Saved %d material(s)\n", count);
}
