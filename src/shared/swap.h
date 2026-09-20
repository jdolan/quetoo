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

#include "matrix.h"
#include "vector.h"

int16_t LittleShort(int16_t s);
int32_t BigLong(int32_t l);
int32_t LittleLong(int32_t l);
float LittleFloat(float f);
Mat4 LittleMat4(const Mat4 m);
Vec3s LittleVec3s(const Vec3s v);
Vec3i LittleVec3i(const Vec3i v);
Vec2 LittleVec2(const Vec2 v);
Vec3 LittleVec3(const Vec3 v);
Vec4 LittleVec4(const Vec4 v);
Box3 LittleBounds(const Box3 v);
