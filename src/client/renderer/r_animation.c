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
 * @brief Free event listener for animations.
 */
static void R_FreeAnimation(r_media_t *media) {

	r_animation_t *animation = (r_animation_t *) media;

	Mem_Free(animation->frames);
}

/**
 * @brief Load animation image with the specified base name and number of images.
 */
r_animation_t *R_CreateAnimation(const char *name, int32_t num_images, const r_image_t **images) {

	r_animation_t *animation = (r_animation_t *) R_AllocMedia(name, sizeof(r_animation_t), MEDIA_ANIMATION);

	animation->media.Free = R_FreeAnimation;
	animation->num_frames = num_images;
	animation->frames = Mem_TagMalloc(sizeof(r_image_t *) * num_images, MEM_TAG_RENDERER);
	memcpy(animation->frames, images, sizeof(r_image_t *) * num_images);

	for (int32_t i = 0; i < num_images; i++) {
		R_RegisterDependency((r_media_t *) animation, (r_media_t *) images[i]);
	}

	return animation;
}

/**
 * @brief Resolve animation image for time parameter
 */
const r_image_t *R_ResolveAnimation(const r_animation_t *animation, float time, int32_t offset) {

	return animation->frames[MIN(animation->num_frames - 1, (int32_t) ((animation->num_frames * time) + offset))];
}
