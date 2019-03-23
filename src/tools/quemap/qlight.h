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
#include "lightmap.h"
#include "material.h"
#include "patch.h"
#include "quemap.h"

extern _Bool antialias;
extern _Bool indirect;

extern vec_t brightness;
extern vec_t saturation;
extern vec_t contrast;

extern int16_t luxel_size;
extern int16_t patch_size;

extern int32_t indirect_bounces;
extern int32_t indirect_bounce;

int32_t Light_PointContents(const vec3_t p);
void Light_Trace(cm_trace_t *trace, const vec3_t start, const vec3_t end, int32_t mask);
int32_t Light_PVS(const lightmap_t *lm, byte *pvs);

int32_t LIGHT_Main(void);
