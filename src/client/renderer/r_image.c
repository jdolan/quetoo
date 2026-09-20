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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "r_local.h"

/**
 * @brief Fixed cubemap mip level count.
 */
#define IMG_CUBEMAP_LEVELS 8

/**
 * @brief Screenshot types.
 */
typedef enum {
  SCREENSHOT_NONE,
  SCREENSHOT_DEFAULT,
  SCREENSHOT_VIEW,
} RenderScreenshotType;

/**
 * @brief Pending screenshot type.
 */
static RenderScreenshotType rPendingScreenshot;

/**
 * @brief Initializes image output directories.
 */
void R_InitImages(void) {
  Fs_Mkdir("screenshots");
}

/**
 * @brief Copies interior texels to the cubemap face border.
 */
static void R_FixupCubemapFace(SDL_Surface *side) {

  if (!side || side->w < 2 || side->h < 2) {
    return;
  }

  SDL_LockSurface(side);

  const int32_t bpp = SDL_BYTESPERPIXEL(side->format);
  byte *pixels = side->pixels;
  const int32_t pitch = side->pitch;

  memcpy(pixels, pixels + pitch, side->w * bpp);
  memcpy(pixels + (side->h - 1) * pitch, pixels + (side->h - 2) * pitch, side->w * bpp);

  for (int32_t y = 0; y < side->h; y++) {
    byte *row = pixels + y * pitch;
    memcpy(row, row + bpp, bpp);
    memcpy(row + (side->w - 1) * bpp, row + (side->w - 2) * bpp, bpp);
  }

  SDL_UnlockSurface(side);
}

/**
 * @brief Encodes and writes a screenshot surface.
 */
static void R_Screenshot_encode(void *data) {
  char path[MAX_QPATH];
  char date[MAX_QPATH];
  
  Fs_Mkdir("screenshots");

  SDL_Surface *surface = data;
  assert(surface);

  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  strftime(date, sizeof(date), "%Y-%m-%d-%H-%M-%S", tm);
  const int32_t millis = (int32_t) (quetoo.ticks % 1000);

  q_snprintf(path, sizeof(path), "screenshots/%s.%03d", date, millis);

  bool res;
  if (!q_strcmp(r_screenshotFormat->string, "tga")) {
    q_strlcat(path, ".tga", sizeof(path));
    res = Img_WriteTGA(path, surface->pixels, surface->w, surface->h);
  } else if (!q_strcmp(r_screenshotFormat->string, "jpg")) {
    q_strlcat(path, ".jpg", sizeof(path));
    res = Img_WriteJPG(path, surface->pixels, surface->w, surface->h, 95);
  } else {
    q_strlcat(path, ".png", sizeof(path));
    res = Img_WritePNG(path, surface->pixels, surface->w, surface->h);
  }

  if (res) {
    Com_Print("Saved %s\n", path);
  } else {
    Com_Warn("Failed to write %s\n", path);
  }

  SDL_DestroySurface(surface);
}

/**
 * @brief Downloads a texture into a bottom-up BGR24 surface.
 */
static SDL_Surface *R_ReadTexture(const Texture *texture) {

  if (texture->format != SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM &&
      texture->format != SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM) {
    Com_Warn("Unsupported texture format %d for screenshot\n", texture->format);
    return NULL;
  }

  const int32_t red = texture->format == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM ? 0 : 2;
  const int32_t blue = 2 - red;

  const int32_t w = texture->size.w, h = texture->size.h;

  byte *pixels = $(texture, downloadPixels);

  SDL_Surface *surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_BGR24);

  for (int32_t y = 0; y < h; y++) {
    const byte *src = pixels + y * w * 4;
    byte *dst = (byte *) surface->pixels + (h - y - 1) * surface->pitch;

    for (int32_t x = 0; x < w; x++, src += 4, dst += 3) {
      dst[0] = src[blue];
      dst[1] = src[1];
      dst[2] = src[red];
    }
  }

  free(pixels);

  return surface;
}

/**
 * @brief Captures the resolved color buffer if a screenshot is pending.
 */
void R_Screenshot(RenderView *view) {

  if (rPendingScreenshot == SCREENSHOT_NONE) {
    return;
  }

  const Texture *texture = $(rContext.device->framebuffer, resolveColorTexture, 0);

  SDL_Surface *surface = texture ? R_ReadTexture(texture) : NULL;
  if (surface) {
    Thread_Create(R_Screenshot_encode, surface, THREAD_NO_WAIT);
  }

  rPendingScreenshot = SCREENSHOT_NONE;
}

/**
 * @brief Sets the screenshot type for this frame.
 */
void R_Screenshot_f(void) {

  if (!q_strcmp(Cmd_Argv(1), "view")) {
    rPendingScreenshot = SCREENSHOT_VIEW;
  } else {
    rPendingScreenshot = SCREENSHOT_DEFAULT;
  }
}

/**
 * @brief Retains persistent image media.
 */
bool R_RetainImage(RenderMedia *self) {

  switch (((RenderImage *) self)->type) {
    case IMG_PROGRAM:
      return true;
    default:
      return false;
  }
}

/**
 * @brief Frees an image texture.
 */
void R_FreeImage(RenderMedia *media) {

  RenderImage *image = (RenderImage *) media;

  image->texture = release(image->texture);
}

/**
 * @brief Loads an image by name.
 */
RenderImage *R_LoadImage(const char *name, RenderImageType type) {
  char key[MAX_QPATH];
  RenderImage *image;

  if (!name || !name[0]) {
    Com_Error(ERROR_DROP, "NULL name\n");
  }

  StripExtension(name, key);

  image = (RenderImage *) R_FindMedia(key, R_MEDIA_IMAGE);
  if (image) {
    return image;
  }

  image = (RenderImage *) R_FindMedia(key, R_MEDIA_ATLAS_IMAGE);
  if (image) {
    return image;
  }

  SDL_Surface *surface = Img_LoadSurface(name);

  if (!surface) {
    Com_Debug(DEBUG_RENDERER, "Couldn't load %s\n", name);
    return NULL;
  }

  image = (RenderImage *) R_AllocMedia(key, sizeof(RenderImage), R_MEDIA_IMAGE);

  image->media.Retain = R_RetainImage;
  image->media.Free = R_FreeImage;

  image->type = type;

  if (type == IMG_CUBEMAP) {

    image->width = surface->w / 4;
    image->height = surface->h / 3;

    const Vec2s offsets[] = {
      MakeVec2s(2, 1),
      MakeVec2s(0, 1),
      MakeVec2s(3, 1),
      MakeVec2s(1, 1),
      MakeVec2s(1, 0),
      MakeVec2s(1, 2)
    };

    const int32_t rotations[] = {
      1,
      3,
      2,
      0,
      0,
      2
    };

    image->depth = 6;

    const size_t faceSize = image->width * image->height * 4;
    byte *data = malloc(faceSize * 6);

    for (size_t i = 0; i < 6; i++) {

      SDL_Surface *side = SDL_CreateSurface(image->width, image->height, SDL_PIXELFORMAT_RGB24);

      SDL_BlitSurface(surface, &(const SDL_Rect) {
        .x = image->width * offsets[i].x,
        .y = image->height * offsets[i].y,
        .w = image->width,
        .h = image->height
      }, side, &(SDL_Rect) {
        .x = 0,
        .y = 0,
        .w = image->width,
        .h = image->height
      });

      if (rotations[i]) {
        SDL_Surface *rotated = Img_RotateSurface(side, rotations[i]);

        if (rotated != side) {
          SDL_DestroySurface(side);
          side = rotated;
        }
      }

      R_FixupCubemapFace(side);

      SDL_Surface *rgba = SDL_ConvertSurface(side, SDL_PIXELFORMAT_RGBA32);
      for (int32_t y = 0; y < image->height; y++) {
        memcpy(data + i * faceSize + y * image->width * 4,
               (const byte *) rgba->pixels + y * rgba->pitch,
               image->width * 4);
      }
      SDL_DestroySurface(rgba);

      SDL_DestroySurface(side);
    }

    const int32_t levels = Mini(IMG_CUBEMAP_LEVELS,
                                (int32_t) floorf(log2f((float) Mini(image->width, image->height))) + 1);

    image->texture = $(rContext.device, createTexture, &(SDL_GPUTextureCreateInfo) {
      .type = SDL_GPU_TEXTURETYPE_CUBE,
      .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
      .width = image->width,
      .height = image->height,
      .layer_count_or_depth = 6,
      .num_levels = levels,
      .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
    }, data);

    free(data);

    CommandBuffer *commands = $(rContext.device, acquireCommandBuffer);
    $(commands, generateMipmaps, image->texture->texture);
    $(commands, submit);
    release(commands);
  } else {
    image->width = surface->w;
    image->height = surface->h;

    image->texture = $(rContext.device, createTextureFromSurface, surface, SDL_GPU_TEXTUREUSAGE_SAMPLER, true);
  }

  $(image->texture, setName, image->media.name);

  R_RegisterMedia((RenderMedia *) image);

  SDL_DestroySurface(surface);

  return image;
}

/**
 * @brief Dump the image to the specified output file.
 */
static void R_DumpImage(const RenderImage *image, const char *output, bool mipmap, bool raw) {
}

/**
 * @brief Enumerates loaded images for dumping.
 */
static void R_DumpImages_enumerator(const RenderMedia *media, void *data) {

  if (media->type == R_MEDIA_IMAGE) {
    const RenderImage *image = (const RenderImage *) media;
    char path[MAX_OS_PATH];

    q_snprintf(path, sizeof(path), "imgdmp/%s", image->media.name);

    R_DumpImage(image, path, true, false);
  }
}

/**
 * @brief Console command to dump all loaded images to disk.
 */
void R_DumpImages_f(void) {

  Com_Print("Dumping media... ");

  Fs_Mkdir("imgdmp");

  R_EnumerateMedia(R_DumpImages_enumerator, NULL);
}
