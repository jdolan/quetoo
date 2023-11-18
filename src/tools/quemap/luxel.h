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
 * @brief Lumens are light source contributions to individual luxels.
 */
typedef struct {
	const light_t *light;
	vec3_t color;
	vec3_t direction;
} lumen_t;

/**
 * @brief Luxels are individual lightmap or lightgrid pixels.
 */
typedef struct {
	int32_t s, t, u;
	vec3_t origin;
	vec3_t normal;
	float dist;
	lumen_t ambient;
	lumen_t diffuse[BSP_LIGHTMAP_CHANNELS];
	vec3_t caustics;
	vec4_t fog;
} luxel_t;

extern void Luxel_Illuminate(luxel_t *luxel, const lumen_t *lumen);
extern SDL_Surface *CreateLuxelSurface(int32_t w, int32_t h, size_t luxel_size, void *luxels);
extern int32_t BlitLuxelSurface(const SDL_Surface *src, SDL_Surface *dest, const SDL_Rect *rect);
extern int32_t WriteLuxelSurface(const SDL_Surface *in, const char *name);

