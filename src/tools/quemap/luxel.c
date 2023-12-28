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
void Luxel_Illuminate(luxel_t *luxel, const lumen_t *lumen) {

	assert(lumen->light);

	const vec3_t color = Vec3_Scale(lumen->light->color, lumen->light->intensity);

	switch (lumen->light->type) {
		case LIGHT_AMBIENT:
			luxel->ambient = Vec3_Fmaf(luxel->ambient, lumen->lumens, color);
			break;

		case LIGHT_SUN:
			luxel->diffuse = Vec3_Fmaf(luxel->diffuse, lumen->lumens, color);
			luxel->direction = Vec3_Fmaf(luxel->direction, lumen->lumens, lumen->direction);
			break;

		case LIGHT_POINT:
		case LIGHT_SPOT:
		case LIGHT_FACE:
			luxel->diffuse = Vec3_Fmaf(luxel->diffuse, lumen->lumens, color);
			luxel->direction = Vec3_Fmaf(luxel->direction, lumen->lumens, lumen->direction);
			if (!g_ptr_array_find(luxel->lights, lumen->light, NULL)) {
				g_ptr_array_add(luxel->lights, (gpointer) lumen->light);
			}
			break;
		
		case LIGHT_PATCH:
			luxel->ambient = Vec3_Fmaf(luxel->ambient, lumen->lumens, color);
			break;
		
		default:
			assert(0);
			break;
	}
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
 * @brief Writes the luxel surface to a 24 bit RGB surface for debugging.
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
