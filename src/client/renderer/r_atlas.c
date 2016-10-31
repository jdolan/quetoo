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
 * @brief Retain event listener for atlases.
 */
static _Bool R_RetainAtlas(r_media_t *self) {
	
	return false;
}

/**
 * @brief Free event listener for atlases.
 */
static void R_FreeAtlas(r_media_t *media) {
	r_atlas_t *atlas = (r_atlas_t *) media;

	if (atlas->image.texnum)
		glDeleteTextures(1, &atlas->image.texnum);

	g_array_unref(atlas->images);

	g_hash_table_destroy(atlas->hash);
}

/**
 * @brief Creates a blank state for an atlas and returns it.
 */
r_atlas_t *R_CreateAtlas(const char *name) {

	r_atlas_t *atlas = (r_atlas_t *) R_AllocMedia(name, sizeof(r_atlas_t));

	atlas->image.media.Retain = R_RetainAtlas;
	atlas->image.media.Free = R_FreeAtlas;
	atlas->image.type = IT_ATLAS_MAP;
	g_strlcpy(atlas->image.media.name, name, sizeof(atlas->image.media.name));

	atlas->images = g_array_new(false, true, sizeof(r_atlas_image_t));

	atlas->hash = g_hash_table_new(g_direct_hash, g_direct_equal);

	return atlas;
}

/**
 * @brief Add an image to the list of images for this atlas.
 */
void R_AddImageToAtlas(r_atlas_t *atlas, const r_image_t *image) {

	// ignore duplicates
	if (g_hash_table_contains(atlas->hash, image))
		return;

	// add to list
	g_array_append_val(atlas->images, (const r_atlas_image_t){
		.input_image = image
	});

	// add blank entry to hash, it's filled in in upload stage
	g_hash_table_insert(atlas->hash, (gpointer) image, NULL);

	// might as well register it as a dependent
	R_RegisterDependency((r_media_t *) atlas, (r_media_t *) image);

	Com_Debug("Atlas %s -> %s\n", atlas->image.media.name, image->media.name);
}

/**
 * @brief Resolve an atlas image from an atlas and image.
 */
const r_atlas_image_t *R_GetAtlasImageFromAtlas(const r_atlas_t *atlas, const r_image_t *image) {

	return (const r_atlas_image_t *) g_hash_table_lookup(atlas->hash, image);
}

// RGBA color masks
#define RMASK 0x000000ff
#define GMASK 0x0000ff00
#define BMASK 0x00ff0000
#define AMASK 0xff000000

/**
 * @brief Stitches the images together into an atlas.
 */
void R_StitchAtlas(r_atlas_t *atlas) {

	uint16_t width = 0, height = 0;
	uint16_t min_size = USHRT_MAX;
	uint32_t time_start = SDL_GetTicks();

	// make the image if we need to
	if (!atlas->image.texnum) {

		glGenTextures(1, &(atlas->image.texnum));
		R_BindTexture(atlas->image.texnum);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_image_state.filter_min);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_image_state.filter_mag);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_image_state.anisotropy);
	}
	else
		R_BindTexture(atlas->image.texnum);
	
	// FIXME: quite possibly the worst stitch algorithm known to man.
	// stitch and set up some initial data
	for (uint16_t i = 0; i < atlas->images->len; i++) {
		r_atlas_image_t *image = &g_array_index(atlas->images, r_atlas_image_t, i);

		image->position[0] = width;
		image->position[1] = 0;

		width += image->input_image->width;
		height = MAX(height, image->input_image->height);

		min_size = MIN(min_size, MIN(image->input_image->width, image->input_image->height));
		
		image->image.width = image->input_image->width;
		image->image.height = image->input_image->height;
		image->image.type = IT_ATLAS_IMAGE;
		image->image.texnum = atlas->image.texnum;

		g_hash_table_replace(atlas->hash, (gpointer) image->input_image, (gpointer) image);
	}

	// see how many mips we need to make
	uint16_t num_mips = 1;

	while (min_size > 1) {
		min_size = floor(min_size / 2.0);
		num_mips++;
	}

	// set up num mipmaps
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, num_mips - 1);

	// make all of the mips as we go
	for (uint16_t i = 0; i < num_mips; i++) {
		const r_pixel_t mip_width = width >> i;
		const r_pixel_t mip_height = height >> i;

		SDL_Surface *mip_surface = SDL_CreateRGBSurface(0, mip_width, mip_height, 32, RMASK, GMASK, BMASK, AMASK);

		if (!mip_surface)
			Com_Error(ERR_DROP, "Unable to allocate memory for atlas");

		// pop in all of the textures
		for (uint16_t j = 0, w = 0; j < atlas->images->len; j++) {
			const r_atlas_image_t *image = &g_array_index(atlas->images, r_atlas_image_t, j);
			SDL_Surface *image_surface;
			GLint image_mip_width;
			GLint image_mip_height;

			R_BindTexture(image->input_image->texnum);
			
			// this has to be done, otherwise mipmaps for the image specified may not have been generated yet.
			// these actually force it to generate the mip.
			glGetTexLevelParameteriv(GL_TEXTURE_2D, i, GL_TEXTURE_WIDTH, &image_mip_width);
			glGetTexLevelParameteriv(GL_TEXTURE_2D, i, GL_TEXTURE_HEIGHT, &image_mip_height);

			image_surface = SDL_CreateRGBSurface(0, image_mip_width, image_mip_height, 32, RMASK, GMASK, BMASK, AMASK);

			SDL_SetSurfaceBlendMode(image_surface, SDL_BLENDMODE_NONE);

			SDL_LockSurface(image_surface);

			glGetTexImage(GL_TEXTURE_2D, i, GL_RGBA, GL_UNSIGNED_BYTE, image_surface->pixels);

			SDL_UnlockSurface(image_surface);

			SDL_Rect dst = {
				.x = w,
				.y = 0,
				.w = image_mip_width,
				.h = image_mip_height
			};

			SDL_BlitSurface(image_surface, NULL, mip_surface, &dst);

			SDL_FreeSurface(image_surface);

			w += image_mip_width;
		}

		SDL_LockSurface(mip_surface);

		R_BindTexture(atlas->image.texnum);

		glTexImage2D(GL_TEXTURE_2D, i, GL_RGBA, mip_width, mip_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, mip_surface->pixels);

		SDL_UnlockSurface(mip_surface);

		SDL_FreeSurface(mip_surface);
	}
	
	// check for errors
	R_GetError(atlas->image.media.name);

	// setup texcoords
	for (uint16_t i = 0, j = 0; i < atlas->images->len; i++) {
		r_atlas_image_t *image = &g_array_index(atlas->images, r_atlas_image_t, i);

		Vector4Set(image->texcoords, 
			(float)j / (float)width,
			0.0,
			(float)(j + image->input_image->width) / (float)width,
			(float)image->input_image->height / (float)height);

		j += image->input_image->width;
	}

	// register if we aren't already
	R_RegisterMedia((r_media_t *) atlas);

	uint32_t time = SDL_GetTicks() - time_start;
	Com_Debug("Atlas %s compiled in %u ms", atlas->image.media.name, time);
}
