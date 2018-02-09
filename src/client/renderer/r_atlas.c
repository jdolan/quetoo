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
#include "r_gl.h"

static cvar_t *r_atlas;

/**
 * @brief Free event listener for atlases.
 */
static void R_FreeAtlas(r_media_t *media) {
	r_atlas_t *atlas = (r_atlas_t *) media;

	if (atlas->image.texnum) {
		glDeleteTextures(1, &atlas->image.texnum);
	}

	g_array_unref(atlas->images);

	g_hash_table_destroy(atlas->hash);
}

/**
 * @brief Creates a blank state for an atlas and returns it.
 */
r_atlas_t *R_CreateAtlas(const char *name) {
	r_atlas_t *atlas = (r_atlas_t *) R_AllocMedia(name, sizeof(r_atlas_t), MEDIA_ATLAS);

	atlas->image.media.Free = R_FreeAtlas;
	atlas->image.type = IT_ATLAS_MAP;
	g_strlcpy(atlas->image.media.name, name, sizeof(atlas->image.media.name));

	if (r_atlas->value) {
		atlas->images = g_array_new(false, true, sizeof(r_atlas_image_t));
		atlas->hash = g_hash_table_new(g_direct_hash, g_direct_equal);
	}

	return atlas;
}

/**
 * @brief Add an image to the list of images for this atlas.
 */
void R_AddImageToAtlas(r_atlas_t *atlas, const r_image_t *image) {

	if (!r_atlas->value) {
		return;
	}

	// ignore duplicates
	if (g_hash_table_contains(atlas->hash, image)) {
		return;
	}

	// add to array
	g_array_append_vals(atlas->images, &(const r_atlas_image_t) {
		.input_image = image,
		.atlas = atlas
	}, 1);

	// add blank entry to hash, it's filled in in upload stage
	g_hash_table_insert(atlas->hash, (gpointer) image, NULL);

	// might as well register it as a dependent
	R_RegisterDependency((r_media_t *) atlas, (r_media_t *) image);

	Com_Debug(DEBUG_RENDERER, "Atlas %s -> %s\n", atlas->image.media.name, image->media.name);
}

/**
 * @brief Resolve an atlas image from an atlas and image.
 */
const r_atlas_image_t *R_GetAtlasImageFromAtlas(const r_atlas_t *atlas, const r_image_t *image) {

	if (!r_atlas->value) {
		return (r_atlas_image_t *) image;
	}

	return (const r_atlas_image_t *) g_hash_table_lookup(atlas->hash, image);
}

/**
 * @brief See if we have enough space for n number of nodes.
 */
static void R_AtlasPacker_Reserve(r_atlas_packer_t *packer, const uint32_t new_nodes) {

	// make sure we have at least n new entries
	if (packer->num_alloc_nodes <= packer->num_nodes + new_nodes) {

		packer->num_alloc_nodes = (packer->num_nodes + new_nodes) * 2;
		packer->nodes = Mem_Realloc(packer->nodes, sizeof(r_atlas_packer_node_t) * packer->num_alloc_nodes);
	}
}

/**
 * @brief Initialize an r_atlas_packer_t structure. If the packer is already
 * created, clears the packer back to an initial state.
 */
void R_AtlasPacker_InitPacker(r_atlas_packer_t *packer, const uint16_t max_width, const uint16_t max_height,
                              const uint16_t root_width, const uint16_t root_height, const uint32_t initial_size) {

	packer->max_width = max_width;
	packer->max_height = max_height;

	if (packer->nodes == NULL) {

		R_AtlasPacker_Reserve(packer, initial_size ? : 1);

	} else {

		packer->num_nodes = 0;
	}

	packer->nodes[0] = (const r_atlas_packer_node_t) {
		.width = root_width,
		 .height = root_height,
		  .right = -1,
		   .down = -1
	};

	packer->num_nodes++;
	packer->root = 0;
}

/**
 * @brief Free data created by R_AtlasPacker_InitPacker
 */
void R_AtlasPacker_FreePacker(r_atlas_packer_t *packer) {

	Mem_Free(packer->nodes);
	packer->nodes = NULL;
}

/**
 * @brief Finds a free node that is big enough to hold us.
 */
r_atlas_packer_node_t *R_AtlasPacker_FindNode(r_atlas_packer_t *packer, const uint32_t root, const uint16_t width,
                                        const uint16_t height) {

	uint32_t node_queue[packer->num_nodes];
	uint32_t node_index = 1;

	node_queue[0] = root;

	do {

		uint32_t node_id = node_queue[--node_index];
		r_atlas_packer_node_t *node = &packer->nodes[node_id];

		// TODO: this line is still hot. It's better without the boolean,
		// but it's weird that this line causes the worst.
		if (node->down != -1u) {

			node_queue[node_index + 0] = node->down;
			node_queue[node_index + 1] = node->right;
			node_index += 2;
		} else if (width <= node->width && height <= node->height &&
		           (node->x + width) <= packer->max_width && (node->y + height) <= packer->max_height) {

			return node;
		}
	} while (node_index != 0);

	return NULL;
}

/**
 * @brief Split a packer node into two, assigning the first to the image.
 */
r_atlas_packer_node_t *R_AtlasPacker_SplitNode(r_atlas_packer_t *packer, r_atlas_packer_node_t *node, const uint16_t width,
        const uint16_t height) {
	const uintptr_t index = (uintptr_t) (node - packer->nodes);

	node->down = packer->num_nodes;
	node->right = packer->num_nodes + 1;

	const r_atlas_packer_node_t down_node = (const r_atlas_packer_node_t) {
		.width = node->width,
		 .height = node->height - height,
		  .x = node->x,
		   .y = node->y + height,
		    .right = -1,
		     .down = -1
	};

	const r_atlas_packer_node_t right_node = (const r_atlas_packer_node_t) {
		.width = node->width - width,
		 .height = height,
		  .x = node->x + width,
		   .y = node->y,
		    .right = -1,
		     .down = -1
	};

	R_AtlasPacker_Reserve(packer, 2);

	// do not use "node" after this point! it may be changed

	packer->nodes[packer->num_nodes] = down_node;
	packer->nodes[packer->num_nodes + 1] = right_node;

	packer->num_nodes += 2;

	return &packer->nodes[index];
}

#define GROW_RIGHT true
#define GROW_DOWN false

/**
 * @brief Grow the packer in the specified direction.
 */
static r_atlas_packer_node_t *R_AtlasPacker_Grow(r_atlas_packer_t *packer, const uint16_t width, const uint16_t height,
        const _Bool grow_direction) {
	const uint32_t new_root_id = packer->num_nodes;
	const uint32_t new_connect_id = packer->num_nodes + 1;
	const r_atlas_packer_node_t *old_root = &packer->nodes[packer->root];

	R_AtlasPacker_Reserve(packer, 2);

	if (grow_direction == GROW_RIGHT) {

		packer->nodes[new_root_id] = (const r_atlas_packer_node_t) {
			.width = old_root->width + width,
			 .height = old_root->height,
			  .down = packer->root,
			   .right = new_connect_id
		};

		packer->nodes[new_connect_id] = (const r_atlas_packer_node_t) {
			.width = width,
			 .height = old_root->height,
			  .x = old_root->width,
			   .y = 0,
			    .right = -1,
			     .down = -1
		};
	} else {

		packer->nodes[new_root_id] = (const r_atlas_packer_node_t) {
			.width = old_root->width,
			 .height = old_root->height + height,
			  .right = packer->root,
			   .down = new_connect_id
		};

		packer->nodes[new_connect_id] = (const r_atlas_packer_node_t) {
			.width = old_root->width,
			 .height = height,
			  .x = 0,
			   .y = old_root->height,
			    .right = -1,
			     .down = -1
		};
	}

	packer->num_nodes += 2;
	packer->root = new_root_id;

	r_atlas_packer_node_t *node = R_AtlasPacker_FindNode(packer, packer->root, width, height);

	if (node != NULL) {
		return R_AtlasPacker_SplitNode(packer, node, width, height);
	}

	return NULL;
}

/**
 * @brief Checks to see where the packer should grow into next. This keeps the atlas square.
 */
r_atlas_packer_node_t *R_AtlasPacker_GrowNode(r_atlas_packer_t *packer, const uint16_t width, const uint16_t height) {
	const r_atlas_packer_node_t *root = &packer->nodes[packer->root];

	const _Bool canGrowDown = (width <= root->width);
	const _Bool canGrowRight = (height <= root->height);

	const _Bool shouldGrowRight = canGrowRight &&
	                              (root->height >= (root->width +
	                                      width)); // attempt to keep square-ish by growing right when height is much greater than width
	const _Bool shouldGrowDown = canGrowDown &&
	                             (root->width >= (root->height +
	                                     height));  // attempt to keep square-ish by growing down  when width  is much greater than height

	if (shouldGrowRight) {
		return R_AtlasPacker_Grow(packer, width, height, GROW_RIGHT);
	} else if (shouldGrowDown) {
		return R_AtlasPacker_Grow(packer, width, height, GROW_DOWN);
	} else if (canGrowRight) {
		return R_AtlasPacker_Grow(packer, width, height, GROW_RIGHT);
	} else if (canGrowDown) {
		return R_AtlasPacker_Grow(packer, width, height, GROW_DOWN);
	}

	return NULL;
}

typedef struct {
	uint16_t width, height;
	uint16_t num_mips;
} r_atlas_params_t;

#define NearestPowerOfTwo(x) ({ int32_t power = 1; while (power < x) { power *= 2; } power; })

/**
 * @brief Stitches the atlas, returning atlas parameters.
 */
static void R_StitchAtlas(r_atlas_t *atlas, r_atlas_params_t *params) {
	uint16_t min_size = USHRT_MAX;
	params->width = params->height = 0;

	// setup base packer parameters
	r_atlas_packer_t packer;
	memset(&packer, 0, sizeof(packer));

	r_atlas_image_t *image = (r_atlas_image_t *) atlas->images->data;
	R_AtlasPacker_InitPacker(&packer, r_config.max_texture_size, r_config.max_texture_size, image->input_image->width,
	                         image->input_image->height, atlas->images->len);

	// stitch!
	for (uint16_t i = 0; i < atlas->images->len; i++, image++) {
		r_atlas_packer_node_t *node = R_AtlasPacker_FindNode(&packer, packer.root, image->input_image->width,
		                        image->input_image->height);

		if (node != NULL) {
			node = R_AtlasPacker_SplitNode(&packer, node, image->input_image->width, image->input_image->height);
		} else {
			node = R_AtlasPacker_GrowNode(&packer, image->input_image->width, image->input_image->height);
		}

		params->width = Max(node->x + image->input_image->width, params->width);
		params->height = Max(node->y + image->input_image->height, params->height);

		min_size = Min(min_size, Min(image->input_image->width, image->input_image->height));

		image->position[0] = node->x;
		image->position[1] = node->y;
		image->image.width = image->input_image->width;
		image->image.height = image->input_image->height;
		image->image.type = IT_ATLAS_IMAGE;
		image->image.texnum = atlas->image.texnum;

		// replace with new atlas image ptr
		g_hash_table_replace(atlas->hash, (gpointer) image->input_image, (gpointer) image);
	}
	
	params->width = NearestPowerOfTwo(params->width);
	params->height = NearestPowerOfTwo(params->height);

	// free packer
	R_AtlasPacker_FreePacker(&packer);

	// see how many mips we need to make
	params->num_mips = 1;

	while (min_size > 1) {
		min_size = floor(min_size / 2.0);
		params->num_mips++;
	}
}

/**
 * @brief Generate mipmap levels for the specified atlas.
 */
static void R_GenerateAtlasMips(r_atlas_t *atlas, r_atlas_params_t *params) {
	
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	
	const vec_t texel_w = 1.0 / params->width;
	const vec_t texel_h = 1.0 / params->height;

	for (uint16_t i = 0; i < params->num_mips; i++) {
		const uint16_t mip_scale = 1 << i;

		const uint16_t mip_width = params->width / mip_scale;
		const uint16_t mip_height = params->height / mip_scale;

		R_BindDiffuseTexture(atlas->image.texnum);

		// set the default to all black transparent
		GLubyte *pixels = Mem_Malloc(mip_width * mip_height * 4);

		memset(pixels, 0, mip_width * mip_height * 4);

		// make initial space
		glTexImage2D(GL_TEXTURE_2D, i, GL_RGBA, mip_width, mip_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

		// pop in all of the textures
		for (uint16_t j = 0; j < atlas->images->len; j++) {
			r_atlas_image_t *image = &g_array_index(atlas->images, r_atlas_image_t, j);

			// pull the mip pixels from the original image
			GLint image_mip_width;
			GLint image_mip_height;

			R_BindDiffuseTexture(image->input_image->texnum);

			glGetTexLevelParameteriv(GL_TEXTURE_2D, i, GL_TEXTURE_WIDTH, &image_mip_width);
			glGetTexLevelParameteriv(GL_TEXTURE_2D, i, GL_TEXTURE_HEIGHT, &image_mip_height);

			GLubyte *subimage_pixels = Mem_Malloc(image_mip_width * image_mip_height * 4);

			glGetTexImage(GL_TEXTURE_2D, i, GL_RGBA, GL_UNSIGNED_BYTE, subimage_pixels);

			// push them into the atlas
			R_BindDiffuseTexture(atlas->image.texnum);

			const uint16_t subimage_x = image->position[0] / mip_scale;
			const uint16_t subimage_y = image->position[1] / mip_scale;

			glTexSubImage2D(GL_TEXTURE_2D, i, subimage_x, subimage_y, image_mip_width, image_mip_height, GL_RGBA, GL_UNSIGNED_BYTE,
			                subimage_pixels);

			// free scratch
			Mem_Free(subimage_pixels);

			// if we're setting up mip 0, set up texcoords
			if (i == 0) {
				Vector4Set(image->texcoords,
				           (image->position[0] / (vec_t) params->width) + texel_w,
				           (image->position[1] / (vec_t) params->height) + texel_h,
				           ((image->position[0] + image->input_image->width) / (vec_t) params->width) - texel_w,
				           ((image->position[1] + image->input_image->height) / (vec_t) params->height) - texel_h);
			}

			R_GetError(NULL);
		}

		Mem_Free(pixels);
	}
}

/**
 * @brief Comparison function for atlas images. This keeps the larger ones running first.
 */
static int R_AtlasImage_Compare(gconstpointer a, gconstpointer b) {
	const r_atlas_image_t *ai = (const r_atlas_image_t *) a;
	const r_atlas_image_t *bi = (const r_atlas_image_t *) b;
	int cmp;

	if ((cmp = Max(bi->input_image->width, bi->input_image->height) - Max(ai->input_image->width,
	           ai->input_image->height)) ||
	        (cmp = Min(bi->input_image->width, bi->input_image->height) - Min(ai->input_image->width, ai->input_image->height)) ||
	        (cmp = bi->input_image->height - ai->input_image->height) ||
	        (cmp = bi->input_image->width - ai->input_image->width)) {
		return cmp;
	}

	return 0;
}

/**
 * @brief Compiles the specified atlas.
 */
void R_CompileAtlas(r_atlas_t *atlas) {

	if (!r_atlas->value) {
		return;
	}

	// sort images, to ensure best stitching
	g_array_sort(atlas->images, R_AtlasImage_Compare);

	r_atlas_params_t params;
	uint32_t time_start = SDL_GetTicks();

	// make the image if we need to
	if (!atlas->image.texnum) {

		glGenTextures(1, &(atlas->image.texnum));
		R_BindDiffuseTexture(atlas->image.texnum);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_image_state.filter_min);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_image_state.filter_mag);

		if (r_image_state.anisotropy) {
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_image_state.anisotropy);
		}
	} else {
		R_BindDiffuseTexture(atlas->image.texnum);
	}

	// stitch
	R_StitchAtlas(atlas, &params);

	// set width/height
	atlas->image.width = params.width;
	atlas->image.height = params.height;

	// set up num mipmaps
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, params.num_mips - 1);

	// run generation
	R_GenerateAtlasMips(atlas, &params);

	// check for errors
	R_GetError(atlas->image.media.name);

	// register if we aren't already
	R_RegisterMedia((r_media_t *) atlas);

	uint32_t time = SDL_GetTicks() - time_start;
	Com_Debug(DEBUG_RENDERER, "Atlas %s compiled in %u ms", atlas->image.media.name, time);
}

/**
 * @brief Serialize a packer to a file.
 */
void R_AtlasPacker_Serialize(const r_atlas_packer_t *packer, file_t *file) {
	
	Fs_Write(file, &packer->num_nodes, sizeof(packer->num_nodes), 1);
	Fs_Write(file, &packer->root, sizeof(packer->root), 1);
	Fs_Write(file, packer->nodes, sizeof(r_atlas_packer_node_t), packer->num_nodes);
}

/**
 * @brief Unserialize packer from a file into the specified packer.
 * @returns bool if packer in file was invalid.
 */
_Bool R_AtlasPacker_Unserialize(file_t *file, r_atlas_packer_t *packer) {

	// read the header
	if (!Fs_Read(file, &packer->num_nodes, sizeof(packer->num_nodes), 1) ||
		!Fs_Read(file, &packer->root, sizeof(packer->root), 1)) {
		return false;
	}
	
	packer->num_nodes = (uint32_t) LittleLong(packer->num_nodes);
	packer->root = (uint32_t) LittleLong(packer->root);

	// check for malformed packer
	if (packer->root >= packer->num_nodes) {
		return false;
	}

	// see if the file even has enough room for this packer
	if ((packer->num_nodes * sizeof(r_atlas_packer_node_t)) > (size_t) (Fs_FileLength(file) - Fs_Tell(file))) {
		return false;
	}

	// good to go!
	packer->nodes = Mem_Malloc(sizeof(r_atlas_packer_node_t) * packer->num_nodes);

	// fatal somehow
	if (Fs_Read(file, packer->nodes, sizeof(r_atlas_packer_node_t), packer->num_nodes) != packer->num_nodes) {
		R_AtlasPacker_FreePacker(packer);
		return false;
	}

	// done!
	return true;
}

/**
 * @brief Initialize atlas subsystem.
 */
void R_InitAtlas(void) {

	r_atlas = Cvar_Add("r_atlas", "1", CVAR_ARCHIVE | CVAR_R_MEDIA,
	                   "Controls whether to enable atlasing of common images or not.");
}
