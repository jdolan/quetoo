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

/**
 * @brief Color and directional information are loaded into layered, atlased textures.
 */
#define MAX_LIGHTMAP_LAYERS 2

typedef struct {
	r_bsp_model_t *bsp;
	GSList *blocks;
	file_t *cache_file;
} r_lightmap_state_t;

#define LIGHTMAP_CACHE_MAGIC 0xbeef

typedef struct {
	int32_t magic;
	int64_t size;
	int64_t time;
	uint32_t num_packers;
} r_lightmap_cache_header_t;

typedef struct {
	uint32_t width, height; // final lm size
	uint32_t count; // # of surfs this packer is responsible for
} r_lightmap_cache_packer_header_t;

static r_lightmap_state_t r_lightmap_state;

/**
 * @brief Pushes lightmap to list of blocks to be processed
 */
void R_CreateBspSurfaceLightmap(const r_bsp_model_t *bsp, r_bsp_surface_t *surf, const byte *data) {

	if (!(surf->flags & R_SURF_LIGHTMAP)) {
		return;
	}

	r_lightmap_t *lm = &surf->lightmap;

	lm->data = data;

	vec_t dist;
	if (surf->flags & R_SURF_PLANE_BACK) {
		dist = -surf->plane->dist;
	} else {
		dist = surf->plane->dist;
	}

	vec3_t s, t;

	VectorNormalize2(surf->texinfo->vecs[0], s);
	VectorNormalize2(surf->texinfo->vecs[1], t);

	VectorScale(s, 1.0 / bsp->luxel_size, s);
	VectorScale(t, 1.0 / bsp->luxel_size, t);

	Matrix4x4_FromArrayFloatGL(&lm->matrix, (const vec_t[]) {
		s[0], t[0], surf->normal[0], 0.0,
		s[1], t[1], surf->normal[1], 0.0,
		s[2], t[2], surf->normal[2], 0.0,
		0.0,  0.0,  -dist,           1.0
	});

	Matrix4x4_Invert_Full(&lm->inverse_matrix, &lm->matrix);

	lm->lm_mins[0] = lm->lm_mins[1] = FLT_MAX;
	lm->lm_maxs[0] = lm->lm_maxs[1] = -FLT_MAX;

	for (int32_t i = 0; i < surf->num_face_edges; i++) {
		const int32_t e = bsp->file->face_edges[surf->first_face_edge + i];
		const bsp_vertex_t *v;

		if (e >= 0) {
			v = bsp->file->vertexes + bsp->file->edges[e].v[0];
		} else {
			v = bsp->file->vertexes + bsp->file->edges[-e].v[1];
		}

		vec3_t st;
		Matrix4x4_Transform(&lm->matrix, v->point, st);

		for (int32_t j = 0; j < 2; j++) {

			if (st[j] < lm->lm_mins[j]) {
				lm->lm_mins[j] = st[j];
			}
			if (st[j] > lm->lm_maxs[j]) {
				lm->lm_maxs[j] = st[j];
			}
		}
	}

	lm->w = floorf(lm->lm_maxs[0] - lm->lm_mins[0]) + 2;
	lm->h = floorf(lm->lm_maxs[1] - lm->lm_mins[1]) + 2;

	r_lightmap_state.blocks = g_slist_prepend(r_lightmap_state.blocks, surf);
}

/**
 * @brief Allocates a `r_lightmap_media_t` with the given dimensions.
 */
static r_lightmap_media_t *R_AllocLightmapMedia(r_pixel_t width, r_pixel_t height) {
	static uint32_t count;
	char name[MAX_QPATH];

	g_snprintf(name, sizeof(name), "lightmap media %u", count);

	r_lightmap_media_t *media = (r_lightmap_media_t *) R_AllocMedia(name, sizeof(r_lightmap_media_t), MEDIA_LIGHTMAP);

	g_snprintf(name, sizeof(name), "lightmap %u", count);

	media->lightmaps = (r_image_t *) R_AllocMedia(name, sizeof(r_image_t), MEDIA_IMAGE);
	media->lightmaps->media.Free = R_FreeImage;

	media->lightmaps->type = IT_LIGHTMAP;
	media->lightmaps->width = width;
	media->lightmaps->height = height;
	media->lightmaps->layers = MAX_LIGHTMAP_LAYERS;

	R_RegisterDependency((r_media_t *) media, (r_media_t *) media->lightmaps);

	if (r_stainmaps->value) {

		g_snprintf(name, sizeof(name), "stainmap %u", count);

		media->stainmaps = (r_image_t *) R_AllocMedia(name, sizeof(r_image_t), MEDIA_IMAGE);
		media->stainmaps->media.Free = R_FreeImage;
		media->stainmaps->type = IT_STAINMAP;
		media->stainmaps->width = width;
		media->stainmaps->height = height;

		R_RegisterDependency((r_media_t *) media, (r_media_t *) media->stainmaps);

		g_strlcat(name, " framebuffer", sizeof(name));

		media->framebuffer = R_CreateFramebuffer(name);

		Matrix4x4_FromOrtho(&media->projection, 0.0, width, height, 0.0, -1.0, 1.0);
	}

	count++;
	return media;
}

/**
 * @brief Copy the contiguous lightmap and deluxemap samples to the strided, layered lightmap.
 */
static void R_BuildLightmap(const r_bsp_model_t *bsp, const r_bsp_surface_t *surf, const byte *in,
                            byte *out, size_t stride, size_t layer_size) {

	for (r_pixel_t t = 0; t < surf->lightmap.h; t++, out += stride) {
		for (r_pixel_t s = 0; s < surf->lightmap.w; s++) {

			const ptrdiff_t offset = (t * surf->lightmap.w + s) * 3;

			byte *lm = out + offset;
			byte *dm = out + offset + layer_size;

			if (in) {
				*lm++ = *in++;
				*lm++ = *in++;
				*lm++ = *in++;

				*dm++ = *in++;
				*dm++ = *in++;
				*dm++ = *in++;
			} else {
				*lm++ = 255;
				*lm++ = 255;
				*lm++ = 255;

				*dm++ = 127;
				*dm++ = 127;
				*dm++ = 255;
			}
		}
	}
}

/**
 * @brief Uploads sorted lightmaps from start (inclusive) to end (exclusive).
 */
static void R_UploadLightmaps(const r_bsp_model_t *bsp, const r_atlas_packer_t *packer,
							  uint32_t width, uint32_t height, GSList *start, GSList *end) {

	// write to cache
	if (r_lightmap_state.cache_file) {

		Fs_Write(r_lightmap_state.cache_file, &(const r_lightmap_cache_packer_header_t) {
			.width = width,
			.height = height,
			.count = (end == NULL) ? g_slist_length(start) : g_slist_position(start, end)
		}, sizeof(r_lightmap_cache_packer_header_t), 1);

		// serialize packer
		R_AtlasPacker_Serialize(packer, r_lightmap_state.cache_file);
	}

	// allocate the lightmap media
	r_lightmap_media_t *media = R_AllocLightmapMedia(width, height);

	// temp buffers
	const size_t layer_size = width * height * 3;
	byte *buffer = Mem_Malloc(layer_size * MAX_LIGHTMAP_LAYERS);

	do {
		r_bsp_surface_t *surf = start->data;

		const size_t offset = (surf->lightmap.t * width + surf->lightmap.s) * 3;
		const size_t stride = (width - surf->lightmap.w) * 3;

		byte *out = buffer + offset;

		R_BuildLightmap(bsp, surf, surf->lightmap.data, out, stride, layer_size);

		surf->lightmap.media = media;

		if (r_lightmap_state.cache_file) {
			Fs_Write(r_lightmap_state.cache_file, &surf->lightmap.s, sizeof(surf->lightmap.s), 1);
			Fs_Write(r_lightmap_state.cache_file, &surf->lightmap.t, sizeof(surf->lightmap.t), 1);
		}

		start = start->next;
	} while (start != end);

	R_UploadImage(media->lightmaps, GL_RGB8, buffer);

	Mem_Free(buffer);

	if (r_stainmaps->value) {

		R_UploadImage(media->stainmaps, GL_RGBA8, NULL);

		R_AttachFramebufferImage(media->framebuffer, media->stainmaps);

		if (!R_FramebufferReady(media->framebuffer)) {
			Com_Warn("Unable to allocate a stainmap framebuffer");
			media->framebuffer = NULL;
		}

		R_BindFramebuffer(FRAMEBUFFER_DEFAULT);
	}
}

/**
 * @brief Attempt to load, parse and upload the surface lightmap cache.
 * @returns false if the cache does not exist or is out of date.
 */
static _Bool R_LoadBspSurfaceLightmapCache(const r_bsp_model_t *bsp) {
	char filename[MAX_QPATH];

	if (!r_lightmap_cache->integer) {
		return false;
	}

	StripExtension(bsp->cm->name, filename);
	g_strlcat(filename, ".lmc", sizeof(filename));

	if (!Fs_Exists(filename)) {
		return false;
	}

	file_t *file = Fs_OpenRead(filename);
	if (!file) {
		return false;
	}

	r_lightmap_cache_header_t header;
	if (!Fs_Read(file, &header, sizeof(header), 1)) {
		Fs_Close(file);
		return false;
	}

	if (header.magic != LIGHTMAP_CACHE_MAGIC ||
		header.size != bsp->cm->size ||
		header.time != bsp->cm->mod_time) {

		Fs_Close(file);
		return false;
	}

	r_atlas_packer_t packer;
	memset(&packer, 0, sizeof(packer));

	GSList *start = r_lightmap_state.blocks, *list = start;

	for (uint32_t i = 0; i < header.num_packers; i++) {
		r_lightmap_cache_packer_header_t packer_header;

		if (!Fs_Read(file, &packer_header, sizeof(packer_header), 1)) {
			R_AtlasPacker_FreePacker(&packer);
			Fs_Close(file);
			return false;
		}

		if (!R_AtlasPacker_Unserialize(file, &packer)) {
			R_AtlasPacker_FreePacker(&packer);
			Fs_Close(file);
			return false;
		}

		// read in the surf x/y positions
		for (uint32_t s = 0; s < packer_header.count; s++) {

			r_bsp_surface_t *surf = (r_bsp_surface_t *) list->data;

			if (!Fs_Read(file, &surf->lightmap.s, sizeof(surf->lightmap.s), 1) ||
				!Fs_Read(file, &surf->lightmap.t, sizeof(surf->lightmap.t), 1)) {
				R_AtlasPacker_FreePacker(&packer);
				Fs_Close(file);
				return false;
			}

			if (surf->lightmap.s == -1 || surf->lightmap.t == -1) {
				R_AtlasPacker_FreePacker(&packer);
				Fs_Close(file);
				return false;
			}

			list = list->next;
		}

		R_UploadLightmaps(bsp, NULL, packer_header.width, packer_header.height, start, list);

		// reset for next round
		start = list;

		R_AtlasPacker_FreePacker(&packer);
	}

	Fs_Close(file);

	g_slist_free(r_lightmap_state.blocks);
	r_lightmap_state.blocks = NULL;

	return true;
}

/**
 * @brief Sort surfaces by height descending, and then by their index. This is a stable sort, so
 * that for the same BSP, the surface ordering is always the same. This enables the lightmap cache
 * to work reliably.
 */
static gint R_InsertBlock_CompareFunc(gconstpointer a, gconstpointer b) {

	const r_bsp_surface_t *ai = (const r_bsp_surface_t *) a;
	const r_bsp_surface_t *bi = (const r_bsp_surface_t *) b;

	int32_t result = bi->lightmap.h - ai->lightmap.h;

	if (result) {
		return result;
	}

	return bi->index - ai->index;
}

/**
 * @brief
 */
void R_LoadBspSurfaceLightmaps(const r_bsp_model_t *bsp) {

	// sort all the lightmap blocks
	r_lightmap_state.blocks = g_slist_sort(r_lightmap_state.blocks, R_InsertBlock_CompareFunc);

	// check if the cache file exists
	if (R_LoadBspSurfaceLightmapCache(bsp)) {
		return;
	}

	// if the cache fails, then build a new packed lightmap atlas
	r_bsp_surface_t *surf = (r_bsp_surface_t *) r_lightmap_state.blocks->data;

	r_atlas_packer_t packer;
	memset(&packer, 0, sizeof(packer));

	R_AtlasPacker_InitPacker(&packer,
							 Min(r_config.max_texture_size, USHRT_MAX),
							 Min(r_config.max_texture_size, USHRT_MAX),
							 surf->lightmap.w,
	                         surf->lightmap.h,
							 bsp->num_surfaces / 2);

	GSList *start = r_lightmap_state.blocks;

	uint32_t current_width = 0, current_height = 0, num_packers = 1, num_surfs = 0;
	r_lightmap_state.cache_file = NULL;

	if (r_lightmap_cache->integer) {
		char filename[MAX_QPATH];

		StripExtension(bsp->cm->name, filename);
		g_strlcat(filename, ".lmc", sizeof(filename));

		r_lightmap_state.cache_file = Fs_OpenWrite(filename);

		if (r_lightmap_state.cache_file) {

			// write initial header; num_packers will be substituted in later on.
			// use -1 so that if this doesn't finish for some reason, it'll be picked up
			// by the read function as invalid.
			Fs_Write(r_lightmap_state.cache_file, &(const r_lightmap_cache_header_t) {
				.magic = LIGHTMAP_CACHE_MAGIC,
				.size = bsp->cm->size,
				.time = bsp->cm->mod_time,
				.num_packers = (uint32_t) -1
			}, sizeof(r_lightmap_cache_header_t), 1);
		}
	}

	for (GSList *list = r_lightmap_state.blocks; ; list = list->next) {

		if (list == NULL) { // upload
			R_UploadLightmaps(bsp, &packer, current_width, current_height, start, list);
			break;
		}

		surf = (r_bsp_surface_t *) list->data;

		r_atlas_packer_node_t *node;

		do {
			node = R_AtlasPacker_FindNode(&packer, packer.root, surf->lightmap.w, surf->lightmap.h);

			if (node != NULL) {
				node = R_AtlasPacker_SplitNode(&packer, node, surf->lightmap.w, surf->lightmap.h);
			} else {
				node = R_AtlasPacker_GrowNode(&packer, surf->lightmap.w, surf->lightmap.h);
			}

			// can't fit any more, so upload and initialize a new packer
			if (node == NULL) {

				// Upload
				R_UploadLightmaps(bsp, &packer, current_width, current_height, start, list);

				// reinitialize packer
				R_AtlasPacker_InitPacker(&packer,
										 Min(r_config.max_texture_size, USHRT_MAX),
										 Min(r_config.max_texture_size, USHRT_MAX),
				                         surf->lightmap.w,
										 surf->lightmap.h,
										 bsp->num_surfaces / 2);

				num_packers++;

				// new start position
				start = list;

				// reset accumulators
				current_width = current_height = num_surfs = 0;
			} else {
				uint32_t w = node->x + surf->lightmap.w,
				         h = node->y + surf->lightmap.h;

				w = NearestMultiple(w, 4);
				h = NearestMultiple(h, 4);

				// fit good, update current sizes
				current_width = Max(current_width, w);
				current_height = Max(current_height, h);

				// update surface parameters
				surf->lightmap.s = node->x;
				surf->lightmap.t = node->y;

				num_surfs++;
			}
		} while (node == NULL);
	}

	// free stuff
	R_AtlasPacker_FreePacker(&packer);

	g_slist_free(r_lightmap_state.blocks);
	r_lightmap_state.blocks = NULL;

	if (r_lightmap_state.cache_file) {

		// write final packer #
		Fs_Seek(r_lightmap_state.cache_file, offsetof(r_lightmap_cache_header_t, num_packers));
		Fs_Write(r_lightmap_state.cache_file, &num_packers, sizeof(num_packers), 1);

		Fs_Close(r_lightmap_state.cache_file);
		r_lightmap_state.cache_file = NULL;
	}
}
