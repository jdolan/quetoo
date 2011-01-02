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

#include "images.h"
#include "r_material.h"

typedef enum {
	it_chars,
	it_effect,
	it_world,
	it_lightmap,
	it_deluxemap,
	it_normalmap,
	it_material,
	it_sky,
	it_skin,
	it_pic
} r_image_type_t;

typedef struct r_image_s {
	char name[MAX_QPATH];  // game path, excluding extension
	r_image_type_t type;
	int width, height;  // source image
	int upload_width, upload_height;  // after power of two
	GLuint texnum;  // gl texture binding
	r_material_t material;  // material definition
	vec3_t color;  // average color
	struct r_image_s *normalmap;  // normalmap texture
} r_image_t;

#define MAX_GL_TEXTURES		1024
extern r_image_t r_images[MAX_GL_TEXTURES];
extern int r_num_images;

#define MAX_GL_LIGHTMAPS 	256
#define TEXNUM_LIGHTMAPS 	MAX_GL_TEXTURES

#define MAX_GL_DELUXEMAPS	256
#define TEXNUM_DELUXEMAPS	(TEXNUM_LIGHTMAPS + MAX_GL_LIGHTMAPS)

#define BACKFACE_EPSILON	0.01

extern r_image_t *r_notexture;
extern r_image_t *r_particletexture;
extern r_image_t *r_explosiontexture;
extern r_image_t *r_teleporttexture;
extern r_image_t *r_smoketexture;
extern r_image_t *r_bubbletexture;
extern r_image_t *r_raintexture;
extern r_image_t *r_snowtexture;
extern r_image_t *r_beamtexture;
extern r_image_t *r_burntexture;
extern r_image_t *r_shadowtexture;
extern r_image_t *r_bloodtexture;
extern r_image_t *r_lightningtexture;
extern r_image_t *r_railtrailtexture;
extern r_image_t *r_flametexture;
extern r_image_t *r_sparktexture;

#define NUM_BULLETTEXTURES 3
extern r_image_t *r_bullettextures[NUM_BULLETTEXTURES];

#define NUM_ENVMAPTEXTURES 3
extern r_image_t *r_envmaptextures[NUM_ENVMAPTEXTURES];

#define NUM_FLARETEXTURES 3
extern r_image_t *r_flaretextures[NUM_FLARETEXTURES];

extern r_image_t *r_warptexture;

// r_image.c
void R_SoftenTexture(byte *in, int width, int height, r_image_type_t type);
void R_FilterTexture(byte *in, int width, int height, vec3_t color, r_image_type_t type);
r_image_t *R_UploadImage(const char *name, void *data, int width, int height, r_image_type_t type);
r_image_t *R_LoadImage(const char *name, r_image_type_t type);
void R_TextureMode(const char *mode);
void R_ListImages_f(void);
void R_Screenshot_f(void);
void R_InitImages(void);
void R_FreeImage(r_image_t *image);
void R_FreeImages(void);
void R_ShutdownImages(void);

#endif /* __R_IMAGE_H__ */
