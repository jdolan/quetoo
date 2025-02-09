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

#pragma once

#include "light.h"

/**
 * @brief The maximum number of diffuse light sources per luxel.
 */
#define MAX_LUXEL_LUMENS 32

/**
 * @brief Lumens are light source contributions to individual luxels.
 */
typedef struct lumen_s {
	/**
	 * @brief The light source.
	 */
	light_t *light;

	/**
	 * @brief The light directional vector.
	 */
	vec3_t direction;

	/**
	 * @brief The amount of light that reached the luxel.
	 */
	float lumens;
} lumen_t;

/**
 * @brief Luxels are individual lightmap or lightgrid pixels.
 */
typedef struct {
	int32_t s, t, u;
	vec3_t origin;
	vec3_t normal;
	vec3_t ambient;
	vec3_t diffuse;
	vec3_t direction;
	lumen_t lumens[MAX_LUXEL_LUMENS];
	vec3_t caustics;
	vec4_t fog;
} luxel_t;

extern void IlluminateLuxel(luxel_t *luxel, const lumen_t *lumen);
extern void FinalizeLuxel(luxel_t *luxel);
extern SDL_Surface *CreateLuxelSurface(int32_t w, int32_t h, size_t luxel_size, void *luxels);
extern uint16_t LightSourceForLumen(const lumen_t *lumen);
extern int32_t BlitLuxelSurface(const SDL_Surface *src, SDL_Surface *dest, const SDL_Rect *rect);
extern int32_t WriteLuxelSurface(const SDL_Surface *in, const char *name);
