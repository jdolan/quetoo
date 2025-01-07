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

#include "swap.h"

#include <SDL_endian.h>

/**
 * @brief
 */
static float SwapFloat(float f) {

	union {
		float f;
		byte b[4];
	} dat1, dat2;

	dat1.f = f + 0.f;

	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];

	return dat2.f;
}

/**
 * @brief
 */
int16_t BigShort(int16_t s) {
	return SDL_SwapBE16(s);
}

/**
 * @brief
 */
int16_t LittleShort(int16_t s) {
	return SDL_SwapLE16(s);
}

/**
 * @brief
 */
int32_t BigLong(int32_t l) {
	return SDL_SwapBE32(l);
}

/**
 * @brief
 */
int32_t LittleLong(int32_t l) {
	return SDL_SwapLE32(l);
}

/**
 * @brief
 */
float BigFloat(float f) {
	if (SDL_BYTEORDER == SDL_LIL_ENDIAN) {
		return SwapFloat(f);
	}
	return f;
}

/**
 * @brief
 */
float LittleFloat(float f) {
	if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
		return SwapFloat(f);
	}
	return f + 0.f;
}

/**
 * @brief
 */
mat4_t LittleMat4(const mat4_t m) {
	mat4_t out = m;
	for (int32_t i = 0; i < 4; i++) {
		for (int32_t j = 0; j < 4; j++) {
			out.m[i][j] = LittleFloat(out.m[i][j]);
		}
	}
	return out;
}

/**
 * @brief
 */
vec3s_t LittleVec3s(const vec3s_t v) {
	return Vec3s(LittleShort(v.x),
				 LittleShort(v.y),
				 LittleShort(v.z));
}

/**
 * @brief
 */
vec3i_t LittleVec3i(const vec3i_t v) {
	return Vec3i(LittleLong(v.x),
				 LittleLong(v.y),
				 LittleLong(v.z));
}

/**
 * @brief
 */
vec2_t LittleVec2(const vec2_t v) {
	return Vec2(LittleFloat(v.x),
				LittleFloat(v.y));
}

/**
 * @brief
 */
vec3_t LittleVec3(const vec3_t v) {
	return Vec3(LittleFloat(v.x),
				LittleFloat(v.y),
				LittleFloat(v.z));
}

/**
 * @brief
 */
vec4_t LittleVec4(const vec4_t v) {
	return Vec4(LittleFloat(v.x),
				LittleFloat(v.y),
				LittleFloat(v.z),
				LittleFloat(v.w));
}

/**
 * @brief
 */
box3_t LittleBounds(const box3_t b) {
	return Box3(LittleVec3(b.mins),
				  LittleVec3(b.maxs));
}
