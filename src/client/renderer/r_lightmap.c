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

#include "r_local.h"

/*
 * In video memory, lightmaps are chunked into NxN RGB blocks. In the BSP,
 * they are a contiguous lump. During the loading process, we use floating
 * point to provide precision.
 */

typedef struct {
	r_image_t *lightmap;
	r_image_t *deluxemap;

	r_pixel_t block_size;  // lightmap block size (NxN)
	r_pixel_t *allocated;  // block availability

	byte *sample_buffer;  // RGB buffers for uploading
	byte *direction_buffer;
} r_lightmap_state_t;

static r_lightmap_state_t r_lightmap_state;

/*
 * @brief
 */
static void R_UploadLightmapBlock(r_bsp_model_t *bsp) {
	static uint32_t count;

	r_image_t *lm = r_lightmap_state.lightmap;

	g_snprintf(lm->media.name, sizeof(lm->media.name), "lightmap %d", count);
	lm->type = IT_LIGHTMAP;
	lm->width = lm->height = r_lightmap_state.block_size;

	R_UploadImage(lm, GL_RGB, r_lightmap_state.sample_buffer);

	if (bsp->version == BSP_VERSION_Q2W) { // upload deluxe block as well

		r_image_t *dm = r_lightmap_state.deluxemap;

		g_snprintf(dm->media.name, sizeof(dm->media.name), "deluxemap %d", count);
		dm->type = IT_DELUXEMAP;
		dm->width = dm->height = r_lightmap_state.block_size;

		R_UploadImage(dm, GL_RGB, r_lightmap_state.direction_buffer);
	}

	count++;

	// clear the allocation block and buffers
	memset(r_lightmap_state.allocated, 0, r_lightmap_state.block_size * sizeof(r_pixel_t));
	memset(r_lightmap_state.sample_buffer, 0, r_lightmap_state.block_size * r_lightmap_state.block_size * 3);
	memset(r_lightmap_state.direction_buffer, 0, r_lightmap_state.block_size * r_lightmap_state.block_size * 3);
}

/*
 * @brief
 */
static bool R_AllocLightmapBlock(r_pixel_t w, r_pixel_t h, r_pixel_t *x, r_pixel_t *y) {
	r_pixel_t i, j;
	r_pixel_t best;

	best = r_lightmap_state.block_size;

	for (i = 0; i < r_lightmap_state.block_size - w; i++) {
		r_pixel_t best2 = 0;

		for (j = 0; j < w; j++) {
			if (r_lightmap_state.allocated[i + j] >= best)
				break;
			if (r_lightmap_state.allocated[i + j] > best2)
				best2 = r_lightmap_state.allocated[i + j];
		}
		if (j == w) { // this is a valid spot
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > r_lightmap_state.block_size)
		return false;

	for (i = 0; i < w; i++)
		r_lightmap_state.allocated[*x + i] = best + h;

	return true;
}

/*
 * @brief
 */
static void R_BuildDefaultLightmap(r_bsp_model_t *bsp, r_bsp_surface_t *surf, byte *sout,
		byte *dout, size_t stride) {
	int32_t i, j;

	const r_pixel_t smax = (surf->st_extents[0] / bsp->lightmap_scale) + 1;
	const r_pixel_t tmax = (surf->st_extents[1] / bsp->lightmap_scale) + 1;

	stride -= (smax * 3);

	for (i = 0; i < tmax; i++, sout += stride, dout += stride) {
		for (j = 0; j < smax; j++) {

			sout[0] = 255;
			sout[1] = 255;
			sout[2] = 255;

			sout += 3;

			if (bsp->version == BSP_VERSION_Q2W) {
				dout[0] = 127;
				dout[1] = 127;
				dout[2] = 255;

				dout += 3;
			}
		}
	}
}

/*
 * @brief Apply brightness, saturation and contrast to the lightmap.
 */
static void R_FilterLightmap(r_pixel_t width, r_pixel_t height, byte *lightmap) {
	r_image_t image;

	memset(&image, 0, sizeof(image));

	image.type = IT_LIGHTMAP;

	image.width = width;
	image.height = height;

	R_FilterImage(&image, GL_RGB, lightmap);
}

/*
 * @brief Consume raw lightmap and deluxemap RGB/XYZ data from the surface samples,
 * writing processed lightmap and deluxemap RGB to the specified destinations.
 *
 * @param in The beginning of the surface lightmap [and deluxemap] data.
 * @param sout The destination for processed lightmap data.
 * @param dout The destination for processed deluxemap data.
 */
static void R_BuildLightmap(const r_bsp_model_t *bsp, const r_bsp_surface_t *surf, const byte *in,
		byte *lout, byte *dout, size_t stride) {
	byte *lightmap, *lm, *deluxemap, *dm;
	size_t i;

	const r_pixel_t smax = (surf->st_extents[0] / bsp->lightmap_scale) + 1;
	const r_pixel_t tmax = (surf->st_extents[1] / bsp->lightmap_scale) + 1;

	const size_t size = smax * tmax;
	stride -= (smax * 3);

	lightmap = (byte *) Z_TagMalloc(size * 3, Z_TAG_RENDERER);
	lm = lightmap;

	deluxemap = dm = NULL;

	if (bsp->version == BSP_VERSION_Q2W) {
		deluxemap = (byte *) Z_TagMalloc(size * 3, Z_TAG_RENDERER);
		dm = deluxemap;
	}

	// convert the raw lightmap samples to RGBA for softening
	for (i = 0; i < size; i++) {
		*lm++ = *in++;
		*lm++ = *in++;
		*lm++ = *in++;

		// read in directional samples for per-pixel lighting as well
		if (bsp->version == BSP_VERSION_Q2W) {
			*dm++ = *in++;
			*dm++ = *in++;
			*dm++ = *in++;
		}
	}

	// apply modulate, contrast, saturation, etc..
	R_FilterLightmap(smax, tmax, lightmap);

	// the lightmap is uploaded to the card via the strided block

	lm = lightmap;

	if (bsp->version == BSP_VERSION_Q2W) {
		dm = deluxemap;
	}

	r_pixel_t s, t;
	for (t = 0; t < tmax; t++, lout += stride, dout += stride) {
		for (s = 0; s < smax; s++) {

			// copy the lightmap to the strided block
			*lout++ = *lm++;
			*lout++ = *lm++;
			*lout++ = *lm++;

			// and the deluxemap for maps which include it
			if (bsp->version == BSP_VERSION_Q2W) {
				*dout++ = *dm++;
				*dout++ = *dm++;
				*dout++ = *dm++;
			}
		}
	}

	Z_Free(lightmap);

	if (bsp->version == BSP_VERSION_Q2W)
		Z_Free(deluxemap);
}

/*
 * @brief Creates the lightmap and deluxemap for the specified surface. The GL
 * textures to which the lightmap and deluxemap are written is stored on the
 * surface.
 *
 * @param data If NULL, a default lightmap and deluxemap will be generated.
 */
void R_CreateBspSurfaceLightmap(r_bsp_model_t *bsp, r_bsp_surface_t *surf, const byte *data) {

	if (!(surf->flags & R_SURF_LIGHTMAP))
		return;

	const r_pixel_t smax = (surf->st_extents[0] / bsp->lightmap_scale) + 1;
	const r_pixel_t tmax = (surf->st_extents[1] / bsp->lightmap_scale) + 1;

	if (!R_AllocLightmapBlock(smax, tmax, &surf->light_s, &surf->light_t)) {

		R_UploadLightmapBlock(bsp); // upload the last block

		r_lightmap_state.lightmap = Z_TagMalloc(sizeof(r_image_t), Z_TAG_RENDERER);

		if (bsp->version == BSP_VERSION_Q2W) {
			r_lightmap_state.deluxemap = Z_TagMalloc(sizeof(r_image_t), Z_TAG_RENDERER);
		}

		if (!R_AllocLightmapBlock(smax, tmax, &surf->light_s, &surf->light_t)) {
			Com_Error(ERR_DROP, "R_CreateSurfaceLightmap: Consecutive calls to "
				"R_AllocLightmapBlock(%d,%d) failed.", smax, tmax);
		}
	}

	surf->lightmap = r_lightmap_state.lightmap;
	surf->deluxemap = r_lightmap_state.deluxemap;

	byte *sout = r_lightmap_state.sample_buffer;
	sout += (surf->light_t * r_lightmap_state.block_size + surf->light_s) * 3;

	byte *dout = r_lightmap_state.direction_buffer;
	dout += (surf->light_t * r_lightmap_state.block_size + surf->light_s) * 3;

	const size_t stride = r_lightmap_state.block_size * 3;

	if (data)
		R_BuildLightmap(bsp, surf, data, sout, dout, stride);
	else
		R_BuildDefaultLightmap(bsp, surf, sout, dout, stride);
}

/*
 * @brief
 */
void R_BeginBspSurfaceLightmaps(r_bsp_model_t *bsp) {
	int32_t max;

	// users can tune lightmap size for their card
	r_lightmap_state.block_size = r_lightmap_block_size->integer;

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max);

	// but clamp it to the card's capability to avoid errors
	r_lightmap_state.block_size = Clamp(r_lightmap_state.block_size, 256, (r_pixel_t) max);

	const r_pixel_t bs = r_lightmap_state.block_size;

	r_lightmap_state.allocated = Z_TagMalloc(bs * sizeof(r_pixel_t), Z_TAG_RENDERER);

	r_lightmap_state.sample_buffer = Z_TagMalloc(bs * bs * sizeof(uint32_t), Z_TAG_RENDERER);
	r_lightmap_state.direction_buffer = Z_TagMalloc(bs * bs * sizeof(uint32_t), Z_TAG_RENDERER);

	// generate the initial texture for lightmap data
	r_lightmap_state.lightmap = Z_TagMalloc(sizeof(r_image_t), Z_TAG_RENDERER);

	// and, if applicable, deluxemaps too
	if (bsp->version == BSP_VERSION_Q2W) {
		r_lightmap_state.deluxemap = Z_TagMalloc(sizeof(r_image_t), Z_TAG_RENDERER);
	}
}

/*
 * @brief
 */
void R_EndBspSurfaceLightmaps(r_bsp_model_t *bsp) {

	// upload the pending lightmap block
	R_UploadLightmapBlock(bsp);

	Z_Free(r_lightmap_state.allocated);

	Z_Free(r_lightmap_state.sample_buffer);
	Z_Free(r_lightmap_state.direction_buffer);
}
