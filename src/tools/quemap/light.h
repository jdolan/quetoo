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

#include "shared.h"
#include "bspfile.h"

typedef enum {
	LIGHT_SURFACE,
	LIGHT_POINT,
	LIGHT_SPOT,
} light_type_t;

typedef struct light_s {
	struct light_s *next;
	light_type_t type;

	vec_t radius;
	vec3_t origin;
	vec3_t color;
	vec3_t normal; // spotlight and sun direction
	vec_t cone; // spotlight cone, in radians
} light_t;

/**
 * @brief Light sources are dynamically allocated, but hashed into bins by PVS cluster. Each light
 * in this array is a linked list of all lights in that cluster.
 */
extern light_t *lights[MAX_BSP_LEAFS];

typedef struct {
	vec3_t origin;
	vec3_t color;
	vec3_t normal;
	vec_t light;
} sun_t;

/**
 * @brief Map authors can define any number of suns (directional light sources) that will
 * illuminate their map globally.
 */
extern sun_t suns[MAX_BSP_ENTITIES];
extern size_t num_suns;

void BuildDirectLights(void);
void BuildIndirectLights(void);
