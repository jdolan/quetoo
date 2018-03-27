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
#include "materials.h"
#include "patches.h"

extern int32_t indirect_bounces;
extern int32_t indirect_bounce;

_Bool Light_PointPVS(const vec3_t org, byte *pvs);
_Bool Light_InPVS(const vec3_t point1, const vec3_t point2);
int32_t Light_PointLeafnum(const vec3_t point);
void Light_Trace(cm_trace_t *trace, const vec3_t start, const vec3_t end, int32_t mask);
