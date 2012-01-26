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
 * In video memory, lightmaps are chunked into NxN RGB blocks.  In the bsp,
 * they are a contiguous lump.  During the loading process, we use floating
 * point to provide precision.
 */

r_lightmaps_t r_lightmaps;

/*
 * R_UploadLightmapBlock
 */
static void R_UploadLightmapBlock() {

	if (r_lightmaps.lightmap_texnum == MAX_GL_LIGHTMAPS) {
		Com_Warn("R_UploadLightmapBlock: MAX_GL_LIGHTMAPS reached.\n");
		return;
	}

	R_BindTexture(r_lightmaps.lightmap_texnum);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, r_lightmaps.size, r_lightmaps.size,
			0, GL_RGB, GL_UNSIGNED_BYTE, r_lightmaps.sample_buffer);

	r_lightmaps.lightmap_texnum++;

	if (r_load_model->version == BSP_VERSION_) { // upload deluxe block as well

		if (r_lightmaps.deluxemap_texnum == MAX_GL_DELUXEMAPS) {
			Com_Warn("R_UploadLightmapBlock: MAX_GL_DELUXEMAPS reached.\n");
			return;
		}

		R_BindTexture(r_lightmaps.deluxemap_texnum);

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, r_lightmaps.size,
				r_lightmaps.size, 0, GL_RGB, GL_UNSIGNED_BYTE,
				r_lightmaps.direction_buffer);

		r_lightmaps.deluxemap_texnum++;
	}

	// clear the allocation block and buffers
	memset(r_lightmaps.allocated, 0, r_lightmaps.size * 3);
	memset(r_lightmaps.sample_buffer, 0, r_lightmaps.size * r_lightmaps.size * 3);
	memset(r_lightmaps.direction_buffer, 0, r_lightmaps.size * r_lightmaps.size * 3);
}

/*
 * R_AllocLightmapBlock
 */
static boolean_t R_AllocLightmapBlock(r_pixel_t w, r_pixel_t h, unsigned int *x, unsigned int *y) {
	size_t i, j;
	unsigned int best;

	best = r_lightmaps.size;

	for (i = 0; i < r_lightmaps.size - w; i++) {
		unsigned int best2 = 0;

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

	if (best + h > r_lightmaps.size)
		return false;

	for (i = 0; i < w; i++)
		r_lightmaps.allocated[*x + i] = best + h;

	return true;
}

/*
 * R_BuildDefaultLightmap
 */
static void R_BuildDefaultLightmap(r_bsp_surface_t *surf, byte *sout,
		byte *dout, int stride) {
	int i, j;

	const int smax = (surf->st_extents[0] / r_load_model->lightmap_scale) + 1;
	const int tmax = (surf->st_extents[1] / r_load_model->lightmap_scale) + 1;

	stride -= (smax * 3);

	for (i = 0; i < tmax; i++, sout += stride, dout += stride) {
		for (j = 0; j < smax; j++) {

			sout[0] = 255;
			sout[1] = 255;
			sout[2] = 255;

			sout += 3;

			if (r_load_model->version == BSP_VERSION_) {
				dout[0] = 127;
				dout[1] = 127;
				dout[2] = 255;

				dout += 3;
			}
		}
	}

	VectorSet(surf->color, 1.0, 1.0, 1.0);
	surf->color[3] = 1.0;
}

/*
 * R_BuildLightmap
 *
 * Consume raw lightmap and deluxemap RGB/XYZ data from the surface samples,
 * writing processed lightmap and deluxemap RGBA to the specified destinations.
 */
static void R_BuildLightmap(r_bsp_surface_t *surf, byte *sout, byte *dout,
		int stride) {
	int i, j;
	byte *lightmap, *lm, *l, *deluxemap, *dm;

	const int smax = (surf->st_extents[0] / r_load_model->lightmap_scale) + 1;
	const int tmax = (surf->st_extents[1] / r_load_model->lightmap_scale) + 1;

	const int size = smax * tmax;
	stride -= (smax * 3);

	lightmap = (byte *) Z_Malloc(size * 3);
	lm = lightmap;

	deluxemap = dm = NULL;

	if (r_load_model->version == BSP_VERSION_) {
		deluxemap = (byte *) Z_Malloc(size * 3);
		dm = deluxemap;
	}

	// convert the raw lightmap samples to RGBA for softening
	for (i = j = 0; i < size; i++, lm += 3, dm += 3) {
		lm[0] = surf->samples[j++];
		lm[1] = surf->samples[j++];
		lm[2] = surf->samples[j++];

		// read in directional samples for deluxe mapping as well
		if (r_load_model->version == BSP_VERSION_) {
			dm[0] = surf->samples[j++];
			dm[1] = surf->samples[j++];
			dm[2] = surf->samples[j++];
		}
	}

	// apply modulate, contrast, resolve average surface color, etc..
	R_FilterTexture(lightmap, smax, tmax, surf->color, it_lightmap);

	if (surf->texinfo->flags & (SURF_BLEND_33 | SURF_ALPHA_TEST))
		surf->color[3] = 0.25;
	else if (surf->texinfo->flags & SURF_BLEND_66)
		surf->color[3] = 0.50;
	else
		surf->color[3] = 1.0;

	// soften it if it's sufficiently large
	if (r_soften->value && size > 128) {
		for (i = 0; i < r_soften->value; i++) {
			R_SoftenTexture(lightmap, smax, tmax, it_lightmap);

			if (r_load_model->version == BSP_VERSION_)
				R_SoftenTexture(deluxemap, smax, tmax, it_deluxemap);
		}
	}

	// the final lightmap is uploaded to the card via the strided lightmap
	// block, and also cached on the surface for fast point lighting lookups

	surf->lightmap = (byte *) R_HunkAlloc(size * 3);
	l = surf->lightmap;

	lm = lightmap;

	if (r_load_model->version == BSP_VERSION_)
		dm = deluxemap;

	for (i = 0; i < tmax; i++, sout += stride, dout += stride) {
		for (j = 0; j < smax; j++) {

			// copy the lightmap to the strided block
			sout[0] = lm[0];
			sout[1] = lm[1];
			sout[2] = lm[2];
			sout += 3;

			// and to the surface, discarding alpha
			l[0] = lm[0];
			l[1] = lm[1];
			l[2] = lm[2];
			l += 3;

			lm += 3;

			// lastly copy the deluxemap to the strided block
			if (r_load_model->version == BSP_VERSION_) {
				dout[0] = dm[0];
				dout[1] = dm[1];
				dout[2] = dm[2];
				dout += 3;

				dm += 3;
			}
		}
	}

	Z_Free(lightmap);

	if (r_load_model->version == BSP_VERSION_)
		Z_Free(deluxemap);
}

/*
 * R_CreateSurfaceLightmap
 */
void R_CreateSurfaceLightmap(r_bsp_surface_t *surf) {
	unsigned int smax, tmax, stride;
	byte *samples, *directions;

	if (!(surf->flags & R_SURF_LIGHTMAP))
		return;

	smax = (surf->st_extents[0] / r_load_model->lightmap_scale) + 1;
	tmax = (surf->st_extents[1] / r_load_model->lightmap_scale) + 1;

	if (!R_AllocLightmapBlock(smax, tmax, &surf->light_s, &surf->light_t)) {

		R_UploadLightmapBlock(); // upload the last block

		if (!R_AllocLightmapBlock(smax, tmax, &surf->light_s, &surf->light_t)) {
			Com_Error(ERR_DROP,
					"R_CreateSurfaceLightmap: Consecutive calls to "
						"R_AllocLightmapBlock(%d,%d) failed.", smax, tmax);
		}
	}

	surf->lightmap_texnum = r_lightmaps.lightmap_texnum;
	surf->deluxemap_texnum = r_lightmaps.deluxemap_texnum;

	samples = r_lightmaps.sample_buffer;
	samples += (surf->light_t * r_lightmaps.size + surf->light_s) * 3;

	directions = r_lightmaps.direction_buffer;
	directions += (surf->light_t * r_lightmaps.size + surf->light_s) * 3;

	stride = r_lightmaps.size * 3;

	if (surf->samples)
		R_BuildLightmap(surf, samples, directions, stride);
	else
		R_BuildDefaultLightmap(surf, samples, directions, stride);
}

/*
 * R_BeginBuildingLightmaps
 */
void R_BeginBuildingLightmaps(void) {
	int max;

	// users can tune lightmap size for their card
	r_lightmaps.size = r_lightmap_block_size->integer;

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max);

	// but clamp it to the card's capability to avoid errors
	if (r_lightmaps.size < 256)
		r_lightmaps.size = 256;

	if (r_lightmaps.size > (size_t) max)
		r_lightmaps.size = (size_t) max;

	r_lightmaps.allocated = (unsigned *) R_HunkAlloc(
			r_lightmaps.size * sizeof(unsigned));

	r_lightmaps.sample_buffer = (byte *) R_HunkAlloc(
			r_lightmaps.size * r_lightmaps.size * sizeof(unsigned int));

	r_lightmaps.direction_buffer = (byte *) R_HunkAlloc(
			r_lightmaps.size * r_lightmaps.size * sizeof(unsigned int));

	r_lightmaps.lightmap_texnum = TEXNUM_LIGHTMAPS;
	r_lightmaps.deluxemap_texnum = TEXNUM_DELUXEMAPS;
}

/*
 * R_EndBuildingLightmaps
 */
void R_EndBuildingLightmaps(void) {

	// upload the pending lightmap block
	R_UploadLightmapBlock();
}
