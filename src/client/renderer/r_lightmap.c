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
 * In video memory, lightmaps are chunked into NxN RGB blocks. In the bsp,
 * they are a contiguous lump. During the loading process, we use floating
 * point to provide precision.
 */

r_lightmaps_t r_lightmaps;

/*
 * @brief
 */
static void R_UploadLightmapBlock(r_bsp_model_t *bsp) {
	static uint32_t count;

	r_image_t *lightmap = Z_TagMalloc(sizeof(r_image_t), Z_TAG_RENDERER);

	snprintf(lightmap->name, sizeof(lightmap->name), "lightmap %d", count);
	lightmap->type = IT_LIGHTMAP;

	lightmap->width = lightmap->height = r_lightmaps.block_size;
	lightmap->texnum = r_lightmaps.lightmap_texnum;

	R_UploadImage(lightmap, GL_RGB, r_lightmaps.sample_buffer);

	if (bsp->version == BSP_VERSION_Q2W) { // upload deluxe block as well

		r_image_t *deluxemap = Z_TagMalloc(sizeof(r_image_t), Z_TAG_RENDERER);

		snprintf(deluxemap->name, sizeof(deluxemap->name), "deluxemap %d", count);
		deluxemap->type = IT_DELUXEMAP;

		deluxemap->width = deluxemap->height = r_lightmaps.block_size;
		deluxemap->texnum = r_lightmaps.deluxemap_texnum;

		R_UploadImage(deluxemap, GL_RGB, r_lightmaps.direction_buffer);
	}

	count++;

	// clear the allocation block and buffers
	memset(r_lightmaps.allocated, 0, r_lightmaps.block_size * sizeof(r_pixel_t));
	memset(r_lightmaps.sample_buffer, 0, r_lightmaps.block_size * r_lightmaps.block_size * 3);
	memset(r_lightmaps.direction_buffer, 0, r_lightmaps.block_size * r_lightmaps.block_size * 3);
}

/*
 * @brief
 */
static bool R_AllocLightmapBlock(r_pixel_t w, r_pixel_t h, r_pixel_t *x, r_pixel_t *y) {
	r_pixel_t i, j;
	r_pixel_t best;

	best = r_lightmaps.block_size;

	for (i = 0; i < r_lightmaps.block_size - w; i++) {
		r_pixel_t best2 = 0;

		for (j = 0; j < w; j++) {
			if (r_lightmaps.allocated[i + j] >= best)
				break;
			if (r_lightmaps.allocated[i + j] > best2)
				best2 = r_lightmaps.allocated[i + j];
		}
		if (j == w) { // this is a valid spot
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > r_lightmaps.block_size)
		return false;

	for (i = 0; i < w; i++)
		r_lightmaps.allocated[*x + i] = best + h;

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
 * @brief Consume raw lightmap and deluxemap RGB/XYZ data from the surface samples,
 * writing processed lightmap and deluxemap RGBA to the specified destinations.
 */
static void R_BuildLightmap(r_bsp_model_t *bsp, r_bsp_surface_t *surf, byte *sout, byte *dout, size_t stride) {
	size_t i, j;
	byte *lightmap, *lm, *deluxemap, *dm;

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
	for (i = j = 0; i < size; i++, lm += 3, dm += 3) {
		lm[0] = surf->samples[j++];
		lm[1] = surf->samples[j++];
		lm[2] = surf->samples[j++];

		// read in directional samples for deluxe mapping as well
		if (bsp->version == BSP_VERSION_Q2W) {
			dm[0] = surf->samples[j++];
			dm[1] = surf->samples[j++];
			dm[2] = surf->samples[j++];
		}
	}

	// apply modulate, contrast, resolve average surface color, etc..
	R_FilterTexture(lightmap, smax, tmax, NULL, IT_LIGHTMAP);

	// soften it if it's sufficiently large
	if (r_soften->value && size > 128) {
		for (i = 0; i < r_soften->value; i++) {
			R_SoftenTexture(lightmap, smax, tmax, IT_LIGHTMAP);

			if (bsp->version == BSP_VERSION_Q2W)
				R_SoftenTexture(deluxemap, smax, tmax, IT_DELUXEMAP);
		}
	}

	// the lightmap is uploaded to the card via the strided block

	lm = lightmap;

	if (bsp->version == BSP_VERSION_Q2W)
		dm = deluxemap;

	r_pixel_t s, t;
	for (t = 0; t < tmax; t++, sout += stride, dout += stride) {
		for (s = 0; s < smax; s++) {

			// copy the lightmap to the strided block
			sout[0] = lm[0];
			sout[1] = lm[1];
			sout[2] = lm[2];
			sout += 3;

			lm += 3;

			// and the deluxemap for maps which include it
			if (bsp->version == BSP_VERSION_Q2W) {
				dout[0] = dm[0];
				dout[1] = dm[1];
				dout[2] = dm[2];
				dout += 3;

				dm += 3;
			}
		}
	}

	Z_Free(lightmap);

	if (bsp->version == BSP_VERSION_Q2W)
		Z_Free(deluxemap);
}

/*
 * @brief
 */
void R_CreateSurfaceLightmap(r_bsp_model_t *bsp, r_bsp_surface_t *surf) {

	if (!(surf->flags & R_SURF_LIGHTMAP))
		return;

	const r_pixel_t smax = (surf->st_extents[0] / bsp->lightmap_scale) + 1;
	const r_pixel_t tmax = (surf->st_extents[1] / bsp->lightmap_scale) + 1;

	if (!R_AllocLightmapBlock(smax, tmax, &surf->light_s, &surf->light_t)) {

		R_UploadLightmapBlock(bsp); // upload the last block

		glGenTextures(1, &r_lightmaps.lightmap_texnum);

		if (bsp->version == BSP_VERSION_Q2W) {
			glGenTextures(1, &r_lightmaps.deluxemap_texnum);
		}

		if (!R_AllocLightmapBlock(smax, tmax, &surf->light_s, &surf->light_t)) {
			Com_Error(ERR_DROP, "R_CreateSurfaceLightmap: Consecutive calls to "
				"R_AllocLightmapBlock(%d,%d) failed.", smax, tmax);
		}
	}

	surf->lightmap_texnum = r_lightmaps.lightmap_texnum;
	surf->deluxemap_texnum = r_lightmaps.deluxemap_texnum;

	byte *samples = r_lightmaps.sample_buffer;
	samples += (surf->light_t * r_lightmaps.block_size + surf->light_s) * 3;

	byte *directions = r_lightmaps.direction_buffer;
	directions += (surf->light_t * r_lightmaps.block_size + surf->light_s) * 3;

	const size_t stride = r_lightmaps.block_size * 3;

	if (surf->samples)
		R_BuildLightmap(bsp, surf, samples, directions, stride);
	else
		R_BuildDefaultLightmap(bsp, surf, samples, directions, stride);
}

/*
 * @brief
 */
void R_BeginBuildingLightmaps(r_bsp_model_t *bsp) {
	int32_t max;

	// users can tune lightmap size for their card
	r_lightmaps.block_size = r_lightmap_block_size->integer;

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max);

	// but clamp it to the card's capability to avoid errors
	r_lightmaps.block_size = Clamp(r_lightmaps.block_size, 256, (r_pixel_t) max);

	const r_pixel_t bs = r_lightmaps.block_size;

	r_lightmaps.allocated = Z_TagMalloc(bs * sizeof(r_pixel_t), Z_TAG_RENDERER);

	r_lightmaps.sample_buffer = Z_TagMalloc(bs * bs * sizeof(uint32_t), Z_TAG_RENDERER);
	r_lightmaps.direction_buffer = Z_TagMalloc(bs * bs * sizeof(uint32_t), Z_TAG_RENDERER);

	// generate the initial texture for lightmap data
	glGenTextures(1, &r_lightmaps.lightmap_texnum);

	// and, if applicable, deluxemaps too
	if (bsp->version == BSP_VERSION_Q2W) {
		glGenTextures(1, &r_lightmaps.deluxemap_texnum);
	}
}

/*
 * @brief
 */
void R_EndBuildingLightmaps(r_bsp_model_t *bsp) {

	// upload the pending lightmap block
	R_UploadLightmapBlock(bsp);

	Z_Free(r_lightmaps.allocated);

	Z_Free(r_lightmaps.sample_buffer);
	Z_Free(r_lightmaps.direction_buffer);
}
