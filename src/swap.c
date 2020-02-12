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

	dat1.f = f;

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
	return f;
}

/**
 * @brief
 */
s16vec2_t LittleShortVector2(const s16vec2_t v) {
	return s16vec2(LittleShort(v.x),
				   LittleShort(v.y));
}

/**
 * @brief
 */
s16vec3_t LittleShortVector3(const s16vec3_t v) {
	return s16vec3(LittleShort(v.x),
				   LittleShort(v.y),
				   LittleShort(v.z));
}

/**
 * @brief
 */
s32vec3_t LittleLongVector3(const s32vec3_t v) {
	return s32vec3(LittleLong(v.x),
				   LittleLong(v.y),
				   LittleLong(v.z));
}

/**
 * @brief
 */
vec2_t LittleVector2(const vec2_t v) {
	return vec2(LittleFloat(v.x),
				LittleFloat(v.y));
}

/**
 * @brief
 */
vec3_t LittleVector3(const vec3_t v) {
	return vec3(LittleFloat(v.x),
				LittleFloat(v.y),
				LittleFloat(v.z));
}

/**
 * @brief
 */
vec4_t LittleVector4(const vec4_t v) {
	return vec4(LittleFloat(v.x),
				LittleFloat(v.y),
				LittleFloat(v.z),
				LittleFloat(v.w));
}
