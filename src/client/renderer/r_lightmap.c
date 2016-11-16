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

/*
 * In video memory, lightmaps are chunked into NxN RGB blocks. In the BSP,
 * they are a contiguous lump. During the loading process, we use floating
 * point to provide precision.
 */

typedef struct {
	r_bsp_model_t *bsp;
	GSList *blocks;
} r_lightmap_state_t;

static r_lightmap_state_t r_lightmap_state;

/**
 * @brief The source of my misery for about 3 weeks.
 */
static void R_FreeLightmap(r_media_t *media) {
	glDeleteTextures(1, &((r_image_t *) media)->texnum);
}

/**
 * @brief Allocates a new lightmap (or deluxemap) image handle.
 */
static r_image_t *R_AllocLightmap_(r_image_type_t type, const r_pixel_t width, const r_pixel_t height) {
	static uint32_t count;
	char name[MAX_QPATH];

	const char *base = (type == IT_LIGHTMAP ? "lightmap" : "deluxemap");
	g_snprintf(name, sizeof(name), "%s %u", base, count++);

	r_image_t *image = (r_image_t *) R_AllocMedia(name, sizeof(r_image_t), MEDIA_IMAGE);

	image->media.Free = R_FreeLightmap;

	image->type = type;
	image->width = width;
	image->height = height;

	return image;
}

#define R_AllocLightmap(w, h) R_AllocLightmap_(IT_LIGHTMAP, w, h)
#define R_AllocDeluxemap(w, h) R_AllocLightmap_(IT_DELUXEMAP, w, h)

/**
 * @brief
 */
static void R_BuildDefaultLightmap(r_bsp_model_t *bsp, r_bsp_surface_t *surf, byte *sout,
                                   byte *dout, size_t stride) {

	const r_pixel_t smax = surf->st_extents[0];
	const r_pixel_t tmax = surf->st_extents[1];

	stride -= (smax * 3);

	for (r_pixel_t i = 0; i < tmax; i++, sout += stride, dout += stride) {
		for (r_pixel_t j = 0; j < smax; j++) {

			sout[0] = 255;
			sout[1] = 255;
			sout[2] = 255;

			sout += 3;

			dout[0] = 127;
			dout[1] = 127;
			dout[2] = 255;

			dout += 3;
		}
	}
}

/**
 * @brief Apply brightness, saturation and contrast to the lightmap.
 */
static void R_FilterLightmap(r_pixel_t width, r_pixel_t height, byte *lightmap) {
	static r_image_t image = { .type = IT_LIGHTMAP };

	image.width = width;
	image.height = height;

	R_FilterImage(&image, GL_RGB, lightmap);
}

/**
 * @brief Consume raw lightmap and deluxemap RGB/XYZ data from the surface samples,
 * writing processed lightmap and deluxemap RGB to the specified destinations.
 *
 * @param in The beginning of the surface lightmap [and deluxemap] data.
 * @param sout The destination for processed lightmap data.
 * @param dout The destination for processed deluxemap data.
 */
static void R_BuildLightmap(const r_bsp_model_t *bsp, const r_bsp_surface_t *surf, const byte *in,
                            byte *lout, byte *dout, size_t stride) {

	const r_pixel_t smax = surf->st_extents[0];
	const r_pixel_t tmax = surf->st_extents[1];

	const size_t size = smax * tmax;
	stride -= (smax * 3);

	byte *lightmap = (byte *) Mem_TagMalloc(size * 3, MEM_TAG_RENDERER);
	byte *lm = lightmap;

	byte *deluxemap = (byte *) Mem_TagMalloc(size * 3, MEM_TAG_RENDERER);
	byte *dm = deluxemap;

	// convert the raw lightmap samples to RGBA for softening
	for (size_t i = 0; i < size; i++) {
		*lm++ = *in++;
		*lm++ = *in++;
		*lm++ = *in++;

		// read in directional samples for per-pixel lighting as well
		if (bsp->version == BSP_VERSION_QUETOO) {
			*dm++ = *in++;
			*dm++ = *in++;
			*dm++ = *in++;
		} else {
			*dm++ = 127;
			*dm++ = 127;
			*dm++ = 255;
		}
	}

	// apply modulate, contrast, saturation, etc..
	R_FilterLightmap(smax, tmax, lightmap);

	// the lightmap is uploaded to the card via the strided block

	lm = lightmap;
	dm = deluxemap;

	for (r_pixel_t t = 0; t < tmax; t++, lout += stride, dout += stride) {
		for (r_pixel_t s = 0; s < smax; s++) {

			// copy the lightmap and deluxemap to the strided block
			*lout++ = *lm++;
			*lout++ = *lm++;
			*lout++ = *lm++;

			*dout++ = *dm++;
			*dout++ = *dm++;
			*dout++ = *dm++;
		}
	}

	Mem_Free(lightmap);
	Mem_Free(deluxemap);
}

/**
 * @brief
 */
gint R_InsertBlock_CompareFunc(gconstpointer  a,
                                   gconstpointer  b) {

	const r_bsp_surface_t *ai = (const r_bsp_surface_t *) a;
	const r_bsp_surface_t *bi = (const r_bsp_surface_t *) b;

	return bi->st_extents[1] - ai->st_extents[1];
}

/**
 * @brief Pushes lightmap to list of blocks to be processed
 */
void R_CreateBspSurfaceLightmap(r_bsp_model_t *bsp, r_bsp_surface_t *surf, const byte *data) {

	if (!(surf->flags & R_SURF_LIGHTMAP)) {
		return;
	}

	surf->lightmap_input = data;
	r_lightmap_state.blocks = g_slist_prepend(r_lightmap_state.blocks, surf);
}

/**
 * @brief Uploads sorted lightmaps from start to (end - 1) and 
 * puts them in the new maps sized to width/height
 */
static void R_UploadPackedLightmaps(uint32_t width, uint32_t height, r_bsp_model_t *bsp, GSList *start, GSList *end) {

	// edge case, no blocks left
	if (!width || !height || !start) {
		return;
	}

	// allocate the image
	r_image_t *lightmap = R_AllocLightmap(width, height);
	r_image_t *deluxemap = R_AllocDeluxemap(width, height);

	// temp buffers
	byte *sample_buffer = Mem_TagMalloc(width * height * 3, MEM_TAG_RENDERER);
	byte *direction_buffer = Mem_TagMalloc(width * height * 3, MEM_TAG_RENDERER);

	do {
		r_bsp_surface_t *surf = (r_bsp_surface_t *) start->data;

		const size_t stride = width * 3;
		const size_t lightmap_offset = (surf->lightmap_t * width + surf->lightmap_s) * 3;

		byte *sout = sample_buffer + lightmap_offset;
		byte *dout = direction_buffer + lightmap_offset;

		if (surf->lightmap_input) {
			R_BuildLightmap(bsp, surf, surf->lightmap_input, sout, dout, stride);
		} else {
			R_BuildDefaultLightmap(bsp, surf, sout, dout, stride);
		}
		
		surf->lightmap = lightmap;
		surf->deluxemap = deluxemap;

		start = start->next;
	} while (start != end);

	// upload!
	R_UploadImage(lightmap, GL_RGB, sample_buffer);
	R_UploadImage(deluxemap, GL_RGB, direction_buffer);

	// free
	Mem_Free(sample_buffer);
	Mem_Free(direction_buffer);
}

/**
 * @brief
 */
void R_EndBspSurfaceLightmaps(r_bsp_model_t *bsp) {

	// sort all the lightmap blocks
	r_lightmap_state.blocks = g_slist_sort(r_lightmap_state.blocks, R_InsertBlock_CompareFunc);

	// make packers and start packin!
	r_bsp_surface_t *surf = (r_bsp_surface_t *) r_lightmap_state.blocks->data;

	r_packer_t packer;
	memset(&packer, 0, sizeof(packer));

	R_AtlasPacker_InitPacker(&packer, r_config.max_texture_size, r_config.max_texture_size, surf->st_extents[0], surf->st_extents[1], bsp->num_surfaces / 2);

	GSList *start = r_lightmap_state.blocks;

	r_pixel_t current_width = 0, current_height = 0;

	for (GSList *list = r_lightmap_state.blocks; ; list = list->next) {

		if (list == NULL) {
			// Upload
			R_UploadPackedLightmaps(current_width, current_height, bsp, start, list);
			break;
		}

		surf = (r_bsp_surface_t *) list->data;

		r_packer_node_t *node;
		
		do {
			node = R_AtlasPacker_FindNode(&packer, &g_array_index(packer.nodes, r_packer_node_t, packer.root), surf->st_extents[0], surf->st_extents[1]);

			if (node != NULL) {
				node = R_AtlasPacker_SplitNode(&packer, node, surf->st_extents[0], surf->st_extents[1]);
			} else {
				node = R_AtlasPacker_GrowNode(&packer, surf->st_extents[0], surf->st_extents[1]);
			}

			// can't fit any more, so upload and initialize a new packer
			if (node == NULL) {

				// Upload
				R_UploadPackedLightmaps(current_width, current_height, bsp, start, list);

				// reinitialize packer
				R_AtlasPacker_InitPacker(&packer, r_config.max_texture_size, r_config.max_texture_size, surf->st_extents[0], surf->st_extents[1], 0);

				// new start position
				start = list;

				// reset accumulators
				current_width = current_height = 0;
			} else {
				r_pixel_t w = node->x + surf->st_extents[0],
					      h = node->y + surf->st_extents[1];
				
				w = NearestMultiple(w, 4);
				h = NearestMultiple(h, 4);

				// fit good, update current sizes
				current_width = MAX(current_width, w);
				current_height = MAX(current_height, h);
				
				// update surface parameters
				surf->lightmap_s = node->x;
				surf->lightmap_t = node->y;
			}
		} while (node == NULL);
	}

	// free stuff
	R_AtlasPacker_FreePacker(&packer);

	g_slist_free(r_lightmap_state.blocks);
	r_lightmap_state.blocks = NULL;
}