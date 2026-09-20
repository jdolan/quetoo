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
 * @brief Frees an atlas media asset.
 */
static void R_FreeAtlas(RenderMedia *media) {
  RenderAtlas *atlas = (RenderAtlas *) media;
  Vector *nodes = atlas->atlas->nodes;

  for (size_t i = 0; i < nodes->count; i++) {
    AtlasNode *node = VectorValue(nodes, AtlasNode *, i);

    for (int32_t layer = 0; layer < atlas->atlas->layers; layer++) {
      SDL_DestroySurface(node->surfaces[layer]);
    }
  }

  Atlas_Destroy(atlas->atlas);
}

/**
 * @brief Loads or creates an atlas.
 */
RenderAtlas *R_LoadAtlas(const char *name) {

  RenderAtlas *atlas = (RenderAtlas *) R_FindMedia(name, R_MEDIA_ATLAS);
  if (atlas == NULL) {

    atlas = (RenderAtlas *) R_AllocMedia(name, sizeof(RenderAtlas), R_MEDIA_ATLAS);
    atlas->media.Free = R_FreeAtlas;

    atlas->image = (RenderImage *) R_AllocMedia(va("%s image", atlas->media.name), sizeof(RenderImage), R_MEDIA_IMAGE);
    atlas->image->media.Free = R_FreeImage;

    atlas->image->type = IMG_ATLAS;

    R_RegisterMedia((RenderMedia *) atlas->image);
    R_RegisterMedia((RenderMedia *) atlas);

    R_RegisterDependency((RenderMedia *) atlas, (RenderMedia *) atlas->image);

    atlas->atlas = Atlas_Create(1);
  }

  return atlas;
}

/**
 * @brief Loads an image into an atlas. Recompile the atlas before rendering the returned image.
 */
RenderAtlasImage *R_LoadAtlasImage(RenderAtlas *atlas, const char *name, RenderImageType type) {
  Vector *nodes = atlas->atlas->nodes;

  for (size_t i = 0; i < nodes->count; i++) {
    AtlasNode *node = VectorValue(nodes, AtlasNode *, i);

    RenderAtlasImage *atlasImage = node->data;
    if (!q_strcmp(name, atlasImage->image.media.name)) {
      R_RegisterDependency((RenderMedia *) atlas, (RenderMedia *) atlasImage);
      return atlasImage;
    }
  }

  RenderAtlasImage *atlasImage = (RenderAtlasImage *) R_AllocMedia(name, sizeof(*atlasImage), R_MEDIA_ATLAS_IMAGE);
  assert(atlasImage);

  SDL_Surface *surf = Img_LoadSurface(name);
  if (!surf) {
    Com_Warn("Failed to load atlas image %s\n", name);
    static const int32_t pixels = 0xff0000ff;
    surf = SDL_CreateSurfaceFrom(1, 1, SDL_PIXELFORMAT_RGBA32, (void *) &pixels, sizeof(pixels));
  }

  AtlasNode *node = Atlas_Insert(atlas->atlas, surf);
  assert(node);

  node->data = atlasImage;
  node->w = surf->w;
  node->h = surf->h;

  atlasImage->image.type = type;
  atlasImage->image.width = surf->w;
  atlasImage->image.height = surf->h;

  R_RegisterDependency((RenderMedia *) atlas, (RenderMedia *) atlasImage);

  atlas->dirty = true;

  return atlasImage;
}

/**
 * @brief Updates atlas texture coordinates for a compiled node.
 */
static void R_CompileAtlas_Node(const AtlasNode *node, const RenderAtlas *atlas) {

  RenderAtlasImage *atlasImage = node->data;

  atlasImage->image.texture = atlas->image->texture;

  const float w = atlas->image->width, h = atlas->image->height;
  const float texel = (1.f / atlas->image->width) * .5f;

  atlasImage->texcoords.x = (node->x / w) + texel;
  atlasImage->texcoords.y = (node->y / h) + texel;
  atlasImage->texcoords.z = ((node->x + node->w) / w) - (texel * 2);
  atlasImage->texcoords.w = ((node->y + node->h) / h) - (texel * 2);
}

/**
 * @brief Compiles an atlas texture and updates its images.
 */
void R_CompileAtlas(RenderAtlas *atlas) {
  Vector *nodes = atlas->atlas->nodes;

  if (!atlas->dirty) {
    return;
  }

  R_FreeImage((RenderMedia *) atlas->image);

  int32_t levels = INT32_MAX;

  for (size_t i = 0; i < nodes->count; i++) {
    const AtlasNode *node = VectorValue(nodes, AtlasNode *, i);
    levels = Mini(levels, floorf(log2f(Maxi(node->w, node->h)) + 1));
  }

  atlas->atlas->padding = levels > 1 ? 1 << (levels - 2) : 0;


  atlas->image->width = 0;

  for (int32_t width = 1024; atlas->image->width == 0; width += 512) {

    if (width > rConfig.maxTextureSize) {
      Com_Error(ERROR_DROP, "Atlas exceeds maximum texture size\n");
    }

    SDL_Surface *surf = SDL_CreateSurface(width, width, SDL_PIXELFORMAT_RGBA32);
    SDL_FillSurfaceRect(surf, NULL, 0);

    if (Atlas_Compile(atlas->atlas, 0, surf) == 0) {

      atlas->image->width = width;
      atlas->image->height = width;

      atlas->image->texture = $(rContext.device, createTextureFromSurface, surf, SDL_GPU_TEXTUREUSAGE_SAMPLER, true);

      for (size_t i = 0; i < nodes->count; i++) {
        R_CompileAtlas_Node(VectorValue(nodes, AtlasNode *, i), atlas);
      }
    }

    SDL_DestroySurface(surf);

    atlas->dirty = false;
  }

  R_RegisterDependency((RenderMedia *) atlas, (RenderMedia *) atlas->image);
}
