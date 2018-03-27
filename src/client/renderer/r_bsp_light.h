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

#include "r_types.h"

#ifdef __R_LOCAL_H__

typedef struct {
	vec_t diffuse;
	vec3_t dir;
	vec3_t color;
} sun_t;

typedef struct {
	vec3_t ambient;

	vec_t brightness;
	vec_t saturation;
	vec_t contrast;

	GSList *lights;
	GSList *suns;
} r_bsp_light_state_t;

extern r_bsp_light_state_t r_bsp_light_state;

void R_LoadBspLights(r_bsp_model_t *mod);
void R_DrawBspLights(void);
#endif /* __R_LOCAL_H__ */
