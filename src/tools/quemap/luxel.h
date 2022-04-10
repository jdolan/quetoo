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
	vec3_t color;
	vec3_t direction;
	int32_t light_id;
	light_type_t light_type;
	int32_t indirect_bounce;
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
	vec3_t caustics;
	vec4_t fog;
	GArray *lumens;
} luxel_t;

extern void Luxel_LightLumen(const light_t *light, luxel_t *luxel, const vec3_t color, const vec3_t direction);
extern void Luxel_SortLumens(luxel_t *luxel);
extern void Luxel_FreeLumens(luxel_t *luxel);
