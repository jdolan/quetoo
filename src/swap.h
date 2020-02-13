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

#include "vector.h"

int16_t BigShort(int16_t s);
int16_t LittleShort(int16_t s);
int32_t BigLong(int32_t l);
int32_t LittleLong(int32_t l);
float BigFloat(float f);
float LittleFloat(float f);
vec3s_t LittleVec3s(const vec3s_t v);
vec3i_t LittleVec3i(const vec3i_t v);
vec2_t LittleVec2(const vec2_t v);
vec3_t LittleVec3(const vec3_t v);
vec4_t LittleVec4(const vec4_t v);
