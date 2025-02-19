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

#include "luxel.h"
#include "qlight.h"

/**
 * @brief
 */
void IlluminateLuxel(luxel_t *luxel, const lumen_t *lumen) {

	assert(lumen->light);

	const vec3_t color = Vec3_Scale(lumen->light->color, lumen->light->intensity);

	switch (lumen->light->type) {
		case LIGHT_AMBIENT:
		case LIGHT_INDIRECT:
			luxel->ambient = Vec3_Fmaf(luxel->ambient, lumen->lumens, color);
			break;

		case LIGHT_SUN:
		case LIGHT_POINT:
		case LIGHT_SPOT:
		case LIGHT_BRUSH_SIDE: {
			luxel->diffuse = Vec3_Fmaf(luxel->diffuse, lumen->lumens, color);

			lumen_t *out;
			size_t i;

			for (i = 0, out = luxel->lumens; i < lengthof(luxel->lumens); i++, out++) {
				if (out->light == lumen->light) {
					out->lumens += lumen->lumens;
					break;
				}

				if (out->light == NULL) {
					*out = *lumen;
					break;
				}
			}

			if (i == lengthof(luxel->lumens)) {
				Mon_SendPoint(MON_WARN, luxel->origin, "MAX_LUXEL_LUMENS");
			}
		}

			break;
		
		default:
			break;
	}

	// append the luxel origin to the light's visible bounds
	lumen->light->bounds = Box3_Append(lumen->light->bounds, luxel->origin);
}

/**
 * @brief
 */
static int32_t LumenCompare(const void *a, const void *b) {
	const lumen_t *x = a;
	const lumen_t *y = b;

	return (int32_t) (1000.f * (y->lumens - x->lumens));
}

/**
 * @brief Resolve the block-level light index for the given luxel and lumen.
 * @details If the light is not yet present in the block, append it. The returned index is
 * guaranteed to fit into an 8 bit value for texture encoding.
 */
static int32_t BlockLightForLumen(const luxel_t *luxel, const lumen_t *lumen) {

	if (lumen->light) {

		const int32_t index = LightIndex(lumen->light);
		assert(index >= 0);
		assert(lumen->light->type > LIGHT_AMBIENT);
		assert(lumen->light->type < LIGHT_INDIRECT);

		bsp_block_t *block = luxel->block;

		for (int32_t i = 1; i < block->num_lights; i++) {
			if (block->lights[i] == index) {
				return i;
			}
		}

		if (block->num_lights == BSP_MAX_BLOCK_LIGHTS) {
			Mon_SendPoint(MON_ERROR, luxel->origin, "BSP_MAX_BLOCK_LIGHTS\n");
		}

		block->lights[block->num_lights++] = index;
		return block->num_lights - 1;
	} else {
		return 0;
	}
}

/**
 * @brief Sort the lumens and populate luxel->lights with block-level indexes.
 */
void FinalizeLuxel(luxel_t *luxel) {

	qsort(luxel->lumens, lengthof(luxel->lumens), sizeof(lumen_t), LumenCompare);

	const lumen_t *lumen = luxel->lumens;
	for (size_t i = 0; i < lengthof(luxel->lumens) && lumen->light; i++, lumen++) {

		if (i < 4) {
			luxel->lights.bytes[i] = (uint8_t) BlockLightForLumen(luxel, lumen);
		} else {
			const vec3_t color = Vec3_Scale(lumen->light->color, lumen->light->intensity);
			luxel->ambient = Vec3_Fmaf(luxel->ambient, lumen->lumens, color);
		}
	}

	luxel->caustics = Vec3_Clampf(luxel->caustics, 0.f, 1.f);
}

/**
 * @brief Create an `SDL_Surface` with the given luxel data.
 */
SDL_Surface *CreateLuxelSurface(int32_t w, int32_t h, size_t luxel_size, void *luxels) {

	SDL_Surface *surface = SDL_malloc(sizeof(SDL_Surface));

	surface->flags = SDL_DONTFREE;
	surface->w = w;
	surface->h = h;
	surface->userdata = (void *) luxel_size;
	surface->pixels = luxels;

	return surface;
}

/**
 * @brief Blits the `src` to the given `rect` in `dest`.
 */
int32_t BlitLuxelSurface(const SDL_Surface *src, SDL_Surface *dest, const SDL_Rect *rect) {

	assert(src);
	assert(src->pixels);
	
	assert(dest);
	assert(dest->pixels);

	assert(src->userdata == dest->userdata);

	assert(rect);

	assert(src->w == rect->w);
	assert(src->h == rect->h);

	const size_t luxel_size = (size_t) src->userdata;

	const byte *in = src->pixels;
	byte *out = dest->pixels;

	out += rect->y * dest->w * luxel_size + rect->x * luxel_size;

	for (int32_t x = 0; x < src->w; x++) {
		for (int32_t y = 0; y < src->h; y++) {

			const byte *in_luxel = in + y * src->w * luxel_size + x * luxel_size;
			byte *out_luxel = out + y * dest->w * luxel_size + x * luxel_size;

			memcpy(out_luxel, in_luxel, luxel_size);
		}
	}

	return 0;
}

/**
 * @brief Writes the luxel surface to a temporary file for debugging.
 */
int32_t WriteLuxelSurface(const SDL_Surface *in, const char *name) {

	assert(in);
	assert(in->pixels);

	SDL_Surface *out;

	const size_t luxel_size = (size_t) in->userdata;
	switch (luxel_size) {

		case sizeof(vec3_t): {
			const vec3_t *in_luxel = (vec3_t *) in->pixels;

			out = SDL_CreateRGBSurfaceWithFormat(0, in->w, in->h, 24, SDL_PIXELFORMAT_RGB24);
			color24_t *out_luxel = (color24_t *) out->pixels;

			for (int32_t x = 0; x < in->w; x++) {
				for (int32_t y = 0; y < in->h; y++) {
					*out_luxel++ = Color_Color24(Color3fv(*in_luxel++));
				}
			}
		}
			break;

		case sizeof(color24_t): {
			const color24_t *in_luxel = (color24_t *) in->pixels;

			out = SDL_CreateRGBSurfaceWithFormat(0, in->w, in->h, 24, SDL_PIXELFORMAT_RGB24);
			color24_t *out_luxel = (color24_t *) out->pixels;

			for (int32_t x = 0; x < in->w; x++) {
				for (int32_t y = 0; y < in->h; y++) {
					*out_luxel++ = *in_luxel++;
				}
			}
		}
			break;

		case sizeof(color32_t): {
			const color32_t *in_luxel = (color32_t *) in->pixels;

			out = SDL_CreateRGBSurfaceWithFormat(0, in->w, in->h, 32, SDL_PIXELFORMAT_RGBA32);
			color32_t *out_luxel = (color32_t *) out->pixels;

			for (int32_t x = 0; x < in->w; x++) {
				for (int32_t y = 0; y < in->h; y++) {
					*out_luxel++ = *in_luxel++;
				}
			}
		}
			break;
	}

	const int32_t err = IMG_SavePNG(out, name);

	SDL_FreeSurface(out);

	return err;
}
