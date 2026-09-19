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
 * @brief Frees an animation media asset.
 */
static void R_FreeAnimation(RenderMedia *media) {

  RenderAnimation *animation = (RenderAnimation *) media;

  Mem_Free(animation->frames);
}

/**
 * @brief Creates an animation from the specified image frames.
 */
RenderAnimation *R_CreateAnimation(const char *name, int32_t numImages, const RenderImage **images) {

  RenderAnimation *animation = (RenderAnimation *) R_AllocMedia(name, sizeof(RenderAnimation), R_MEDIA_ANIMATION);

  animation->media.Free = R_FreeAnimation;
  animation->numFrames = numImages;
  animation->frames = Mem_TagMalloc(sizeof(RenderImage *) * numImages, MEM_TAG_RENDERER);
  memcpy(animation->frames, images, sizeof(RenderImage *) * numImages);

  for (int32_t i = 0; i < numImages; i++) {
    R_RegisterDependency((RenderMedia *) animation, (RenderMedia *) images[i]);
  }

  return animation;
}

/**
 * @brief Resolves an animation frame for the specified time.
 */
const RenderImage *R_ResolveAnimation(const RenderAnimation *animation, float time, int32_t offset) {
  const int32_t frame = (int32_t) (animation->numFrames * time);
  return animation->frames[Mini(Maxi(frame + offset, 0), animation->numFrames - 1)];
}
