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

#include "fog.h"
#include "light.h"
#include "lightgrid.h"
#include "lightmap.h"
#include "material.h"
#include "patch.h"
#include "quemap.h"
#include "writebsp.h"

extern _Bool antialias;

extern float brightness;
extern float saturation;
extern float contrast;

extern int32_t luxel_size;
extern int32_t patch_size;

extern float ambient_intensity;
extern float sun_intensity;
extern float light_intensity;
extern float patch_intensity;
extern float indirect_intensity;

extern float caustics;

int32_t Light_PointContents(const vec3_t p, int32_t head_node);
cm_trace_t Light_Trace(const vec3_t start, const vec3_t end, int32_t head_node, int32_t mask);

int32_t LIGHT_Main(void);
