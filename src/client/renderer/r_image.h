/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#ifndef __R_IMAGE_H__
#define __R_IMAGE_H__

#include "r_types.h"

extern r_image_t *r_null_image;

#define NUM_ENVMAP_IMAGES 3
extern r_image_t *r_envmap_images[NUM_ENVMAP_IMAGES];

#define NUM_FLARE_IMAGES 3
extern r_image_t *r_flare_images[NUM_FLARE_IMAGES];

extern r_image_t *r_warp_image;

r_image_t *R_LoadImage(const char *name, r_image_type_t type);

#ifdef __R_LOCAL_H__

#define MAX_GL_TEXTURES		1024
extern r_image_t r_images[MAX_GL_TEXTURES];
extern uint16_t r_num_images;

#define MAX_GL_LIGHTMAPS 	256
#define TEXNUM_LIGHTMAPS 	MAX_GL_TEXTURES

#define MAX_GL_DELUXEMAPS	256
#define TEXNUM_DELUXEMAPS	(TEXNUM_LIGHTMAPS + MAX_GL_LIGHTMAPS)

#define BACK_PLANE_EPSILON	0.01

void R_SoftenTexture(byte *in, int32_t width, int32_t height, r_image_type_t type);
void R_FilterTexture(byte *in, int32_t width, int32_t height, vec3_t color, r_image_type_t type);
r_image_t *R_UploadImage(const char *name, byte *data, int32_t width, int32_t height, r_image_type_t type);
void R_TextureMode(const char *mode);
void R_ListImages_f(void);
void R_Screenshot_f(void);
void R_InitImages(void);
void R_FreeImage(r_image_t *image);
void R_FreeImages(void);
void R_ShutdownImages(void);

#endif /* __R_LOCAL_H__ */

#endif /* __R_IMAGE_H__ */
