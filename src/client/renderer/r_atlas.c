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
 * @brief Free event listener for atlases.
 */
static void R_FreeAtlas(r_media_t *media) {
	r_atlas_t *atlas = (r_atlas_t *) media;

	for (guint i = 0; i < atlas->atlas->nodes->len; i++) {
		atlas_node_t *node = g_ptr_array_index(atlas->atlas->nodes, i);

		for (int32_t layer = 0; layer < atlas->atlas->layers; layer++) {
			SDL_FreeSurface(node->surfaces[layer]);
		}
	}

	Atlas_Destroy(atlas->atlas);
}

/**
 * @brief Loads an existing atlas from memory, or creates a new one.
 */
r_atlas_t *R_LoadAtlas(const char *name) {

	r_atlas_t *atlas = (r_atlas_t *) R_FindMedia(name);
	if (atlas == NULL) {

		atlas = (r_atlas_t *) R_AllocMedia(name, sizeof(r_atlas_t), MEDIA_ATLAS);
		atlas->media.Free = R_FreeAtlas;

		atlas->image = (r_image_t *) R_AllocMedia(va("%s image", atlas->media.name), sizeof(r_image_t), MEDIA_IMAGE);
		atlas->image->media.Free = R_FreeImage;

		atlas->image->type = IT_ATLAS;

		R_RegisterMedia((r_media_t *) atlas->image);
		R_RegisterMedia((r_media_t *) atlas);

		R_RegisterDependency((r_media_t *) atlas, (r_media_t *) atlas->image);

		atlas->atlas = Atlas_Create(1);
	}

	return atlas;
}

/**
 * @brief Loads the named image through the specified atlas. The returned r_atlas_image_t is
 * not available for rendering until the atlas is recompiled.
 */
r_atlas_image_t *R_LoadAtlasImage(r_atlas_t *atlas, const char *name, r_image_type_t type) {
	const int32_t pixels = 0xff0000ff;

	for (guint i = 0; i < atlas->atlas->nodes->len; i++) {
		atlas_node_t *node = g_ptr_array_index(atlas->atlas->nodes, i);

		r_atlas_image_t *atlas_image = node->data;
		if (!strcmp(name, atlas_image->image.media.name)) {
			R_RegisterMedia((r_media_t *) atlas_image);
			return atlas_image;
		}
	}

	r_atlas_image_t *atlas_image = (r_atlas_image_t *) R_AllocMedia(name, sizeof(*atlas_image), MEDIA_ATLAS_IMAGE);
	assert(atlas_image);

	SDL_Surface *surf = Img_LoadSurface(name);
	if (!surf) {
		Com_Warn("Failed to load atlas image %s\n", name);

		surf = SDL_CreateRGBSurfaceWithFormatFrom((void *) &pixels, 1, 1, 32, 4, SDL_PIXELFORMAT_RGBA32);
	}

	atlas_node_t *node = Atlas_Insert(atlas->atlas, surf);
	assert(node);

	node->data = atlas_image;

	atlas_image->image.type = type;
	atlas_image->image.width = surf->w;
	atlas_image->image.height = surf->h;

	R_RegisterMedia((r_media_t *) atlas_image);

	atlas->dirty = true;

	return atlas_image;
}

/**
 * @brief GFunc for atlas node compilation.
 */
static void R_CompileAtlas_Node(gpointer data, gpointer user_data) {

	const atlas_node_t *node = data;
	const r_atlas_t *atlas = user_data;

	r_atlas_image_t *atlas_image = node->data;

	atlas_image->image.texnum = atlas->image->texnum;

	const float w = atlas->image->width, h = atlas->image->height;

	atlas_image->texcoords.x = node->x / w;
	atlas_image->texcoords.y = node->y / h;
	atlas_image->texcoords.z = (node->x + atlas_image->image.width) / w;
	atlas_image->texcoords.w = (node->y + atlas_image->image.height) / h;
}

/**
 * @brief Compiles the specified atlas.
 */
void R_CompileAtlas(r_atlas_t *atlas) {

	if (!atlas->dirty) {
		return;
	}

	R_FreeImage((r_media_t *) atlas->image);

	atlas->image->width = 0;

	// calculate number of mip levels; basically we just take the smallest possible
	// mip size. larger textures get kinda shafted...
	GLsizei levels = INT32_MAX;

	for (int32_t i = 0; i < atlas->atlas->nodes->len; i++) {
		const atlas_node_t *node = g_ptr_array_index(atlas->atlas->nodes, i);
		const r_atlas_image_t *atlas_image = node->data;

		levels = MIN(levels, floorf(log2f(MAX(atlas_image->image.width, atlas_image->image.height)) + 1));
	}

	for (int32_t width = 2048; atlas->image->width == 0; width += 1024) {

		if (width > r_config.max_texture_size) {
			Com_Error(ERROR_DROP, "Atlas exceeds GL_MAX_TEXTURE_SIZE\n");
		}

		SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, width, width, 32, SDL_PIXELFORMAT_RGBA32);
		if (Atlas_Compile(atlas->atlas, 0, surf) == 0) {

			atlas->image->width = width;
			atlas->image->height = width;

			R_SetupImage(atlas->image, GL_TEXTURE_2D, GL_RGBA, levels, GL_UNSIGNED_BYTE, NULL);

			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, surf->w, surf->h, GL_RGBA, GL_UNSIGNED_BYTE, surf->pixels);
			
			for (GLsizei i = 1; i < levels; i++) {
				SDL_Surface *mip_surf = SDL_CreateRGBSurfaceWithFormat(0, width >> i, width >> i, 32, SDL_PIXELFORMAT_RGBA32);

				for (int32_t l = 0; l < atlas->atlas->nodes->len; l++) {
					const atlas_node_t *node = g_ptr_array_index(atlas->atlas->nodes, l);
					const r_atlas_image_t *atlas_image = node->data;

					SDL_BlitScaled(surf, &(const SDL_Rect) {
						.x = node->x, .y = node->y, .w = atlas_image->image.width, .h = atlas_image->image.height
					}, mip_surf, &(SDL_Rect) {
						.x = node->x >> i, .y = node->y >> i, .w = atlas_image->image.width >> i, .h = atlas_image->image.height >> i
					});
				}

				glTexSubImage2D(GL_TEXTURE_2D, i, 0, 0, mip_surf->w, mip_surf->h, GL_RGBA, GL_UNSIGNED_BYTE, mip_surf->pixels);

				R_GetError("");

				SDL_FreeSurface(mip_surf);
			}

			g_ptr_array_foreach(atlas->atlas->nodes, R_CompileAtlas_Node, atlas);
		}

		SDL_FreeSurface(surf);

		atlas->dirty = false;
	}

	R_RegisterDependency((r_media_t *) atlas, (r_media_t *) atlas->image);
}
