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

	switch (lumen->light->type) {
		case LIGHT_SUN:
		case LIGHT_POINT:
		case LIGHT_SPOT:
		case LIGHT_PATCH: {

			lumen_t *diffuse = luxel->diffuse;
			for (int32_t i = 0; i < BSP_LIGHTMAP_CHANNELS; i++, diffuse++) {

				if (diffuse->light == NULL) {
					*diffuse = *lumen;
					return;
				}

				if (diffuse->light == lumen->light) {
					diffuse->color = Vec3_Add(diffuse->color, lumen->color);
					diffuse->direction = Vec3_Add(diffuse->direction, lumen->direction);
					return;
				}

				if (Vec3_LengthSquared(lumen->color) > Vec3_LengthSquared(diffuse->color)) {

					const lumen_t last = luxel->diffuse[BSP_LIGHTMAP_CHANNELS - 1];

					for (int32_t j = BSP_LIGHTMAP_CHANNELS - 1; j > i; j--) {
						luxel->diffuse[j] = luxel->diffuse[j - 1];
					}

					*diffuse = *lumen;

					if (last.light) {
						Luxel_Illuminate(luxel, &last);
					}

					return;
				}
			}

			vec3_t c = lumen->color;
			vec3_t d = lumen->direction;

			diffuse = luxel->diffuse;
			for (int32_t j = 0; j < BSP_LIGHTMAP_CHANNELS; j++, diffuse++) {

				const vec3_t a = Vec3_Normalize(lumen->direction);
				const vec3_t b = Vec3_Normalize(diffuse->direction);

				const float dot = Clampf(Vec3_Dot(a, b), 0.f, 1.f);

				diffuse->color = Vec3_Fmaf(diffuse->color, dot, c);
				diffuse->direction = Vec3_Fmaf(diffuse->direction, dot, d);

				c = Vec3_Fmaf(c, -dot, c);
				d = Vec3_Fmaf(d, -dot, d);
			}

			luxel->ambient.color = Vec3_Add(luxel->ambient.color, c);
		}

			break;
		default:
			luxel->ambient.color = Vec3_Add(luxel->ambient.color, lumen->color);
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
