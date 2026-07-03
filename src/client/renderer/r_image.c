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
 * @brief Screenshot types.
 */
typedef enum {
  SCREENSHOT_NONE,
  SCREENSHOT_DEFAULT,
  SCREENSHOT_VIEW,
} r_screenshot_type_t;

/**
 * @brief Image state.
 */
static struct {

  /**
   * @brief The maximum supported texture sampling anisotropy level.
   */
  float max_anisotropy;

  /**
   * @brief The current anisotropy level, clamped to `max_anisotropy`.
   */
  float anisotropy;

  /**
   * @brief If set, take a screenshot at the end of the frame.
   */
  r_screenshot_type_t screenshot;
} r_image_state;


/**
 * @brief Replaces cubemap face border texels with adjacent interior texels.
 * @details This removes 1px edge artifacts present in some source sky cross images and
 * helps prevent seam bleed in mip generation and filtered cubemap sampling.
 */
static void R_FixupCubemapFace(SDL_Surface *side) {

  if (!side || side->w < 2 || side->h < 2) {
    return;
  }

  SDL_LockSurface(side);

  const int32_t bpp = SDL_BYTESPERPIXEL(side->format);
  byte *pixels = side->pixels;
  const int32_t pitch = side->pitch;

  // Top row <- row 1, bottom row <- row h-2.
  memcpy(pixels, pixels + pitch, side->w * bpp);
  memcpy(pixels + (side->h - 1) * pitch, pixels + (side->h - 2) * pitch, side->w * bpp);

  // Left / right columns <- adjacent interior columns.
  for (int32_t y = 0; y < side->h; y++) {
    byte *row = pixels + y * pitch;
    memcpy(row, row + bpp, bpp);
    memcpy(row + (side->w - 1) * bpp, row + (side->w - 2) * bpp, bpp);
  }

  SDL_UnlockSurface(side);
}

/**
 * @brief ThreadRunFunc for `R_Screenshot`.
 */
static void R_Screenshot_encode(void *data) {
  char path[MAX_QPATH];
  char date[MAX_QPATH];

  SDL_Surface *surface = data;
  assert(surface);

  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  strftime(date, sizeof(date), "%Y-%m-%d-%H-%M-%S", tm);
  const int32_t millis = (int32_t) (quetoo.ticks % 1000);

  q_snprintf(path, sizeof(path), "screenshots/%s.%03d", date, millis);

  bool res;
  if (!q_strcmp(r_screenshot_format->string, "tga")) {
    q_strlcat(path, ".tga", sizeof(path));
    res = Img_WriteTGA(path, surface->pixels, surface->w, surface->h);
  } else if (!q_strcmp(r_screenshot_format->string, "jpg")) {
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
 * @brief Captures a screenshot, if requested, writing it to the user's directory.
 */
void R_Screenshot(r_view_t *view) {

  // TODO(#864): screenshots not yet ported to SDL_gpu.
  r_image_state.screenshot = SCREENSHOT_NONE;
}

/**
* @brief Sets the screenshot type to take this frame.
*/
void R_Screenshot_f(void) {

  if (!q_strcmp(Cmd_Argv(1), "view")) {
    r_image_state.screenshot = SCREENSHOT_VIEW;
  } else {
    r_image_state.screenshot = SCREENSHOT_DEFAULT;
  }
}

/**
 * @brief Creates the base image state for the image.
 */
void R_SetupImage(r_image_t *image) {

  // TODO(#864): retired; GPU textures are created in R_LoadImage via ObjectivelyGPU.
}

/**
 * @brief Uploads the given pixel data to the specified image and target.
 * @param image The image.
 * @param target The upload target, which may be different from the image's bind target.
 * @param data The pixel data.
 */
void R_UploadImageTarget(r_image_t *image, uint32_t target, const void *data) {

  // TODO(#864): retired during SDL_gpu port (2D via createTextureFromSurface in R_LoadImage).
}

/**
 * @brief Uploads the given pixel data to the specified image.
 * @param image The image.
 * @param data The pixel data.
 */
void R_UploadImage(r_image_t *image, const void *data) {
  R_UploadImageTarget(image, 0, data);
}


/**
 * @brief Retain event listener for images.
 */
bool R_RetainImage(r_media_t *self) {

  switch (((r_image_t *) self)->type) {
    case IMG_PROGRAM:
    case IMG_FONT:
    case IMG_UI:
      return true;
    default:
      return false;
  }
}

/**
 * @brief Free event listener for images.
 */
void R_FreeImage(r_media_t *media) {

  r_image_t *image = (r_image_t *) media;

  image->texture = release(image->texture);
}

/**
 * @brief Loads the image by the specified name.
 */
r_image_t *R_LoadImage(const char *name, r_image_type_t type) {
  char key[MAX_QPATH];
  r_image_t *image;

  if (!name || !name[0]) {
    Com_Error(ERROR_DROP, "NULL name\n");
  }

  StripExtension(name, key);

  image = (r_image_t *) R_FindMedia(key, R_MEDIA_IMAGE);
  if (image) {
    return image;
  }

  image = (r_image_t *) R_FindMedia(key, R_MEDIA_ATLAS_IMAGE);
  if (image) {
    return image;
  }

  SDL_Surface *surface = Img_LoadSurface(name);

  if (!surface) {
    Com_Debug(DEBUG_RENDERER, "Couldn't load %s\n", name);
    return NULL;
  }

  image = (r_image_t *) R_AllocMedia(key, sizeof(r_image_t), R_MEDIA_IMAGE);

  image->media.Retain = R_RetainImage;
  image->media.Free = R_FreeImage;

  image->type = type;

  if (type == IMG_CUBEMAP) {

    image->width = surface->w / 4;
    image->height = surface->h / 3;

    // right left front back up down
    const vec2s_t offsets[] = {
      Vec2s(2, 1),
      Vec2s(0, 1),
      Vec2s(3, 1),
      Vec2s(1, 1),
      Vec2s(1, 0),
      Vec2s(1, 2)
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

    // Assemble the six faces, layer-major, into a single RGBA buffer and upload
    // them as one cube texture (SDL_gpu uploads all layers in one copy).
    const size_t face_size = image->width * image->height * 4;
    byte *data = malloc(face_size * 6);

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
        memcpy(data + i * face_size + y * image->width * 4,
               (const byte *) rgba->pixels + y * rgba->pitch,
               image->width * 4);
      }
      SDL_DestroySurface(rgba);

      SDL_DestroySurface(side);
    }

    image->texture = $(r_context.device, createTexture, &(SDL_GPUTextureCreateInfo) {
      .type = SDL_GPU_TEXTURETYPE_CUBE,
      .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
      .width = image->width,
      .height = image->height,
      .layer_count_or_depth = 6,
      .num_levels = 1,
      .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
    }, data);

    free(data);
  } else {
    image->width = surface->w;
    image->height = surface->h;

    image->texture = $(r_context.device, createTextureFromSurface, surface, SDL_GPU_TEXTUREUSAGE_SAMPLER);
  }
    
  R_RegisterMedia((r_media_t *) image);

  SDL_DestroySurface(surface);

  return image;
}

/**
 * @brief Dump the image to the specified output file.
 */
static void R_DumpImage(const r_image_t *image, const char *output, bool mipmap, bool raw) {
}

/**
 * @brief `R_MediaEnumerator` for `R_DumpImages_f`.
 */
static void R_DumpImages_enumerator(const r_media_t *media, void *data) {

  if (media->type == R_MEDIA_IMAGE) {
    const r_image_t *image = (const r_image_t *) media;
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

/**
 * @brief Initializes the images facilities
 */
void R_InitImages(void) {

  memset(&r_image_state, 0, sizeof(r_image_state));

  Fs_Mkdir("screenshots");
}
