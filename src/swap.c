/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#include <SDL/SDL.h>

/*
 * @brief
 */
static int16_t SwapShort(int16_t l) {

	const byte b1 = l & 255;
	const byte b2 = (l >> 8) & 255;

	return (b1 << 8) + b2;
}

/*
 * @brief
 */
static int32_t SwapLong(int32_t l) {

	const byte b1 = l & 255;
	const byte b2 = (l >> 8) & 255;
	const byte b3 = (l >> 16) & 255;
	const byte b4 = (l >> 24) & 255;

	return ((int32_t) b1 << 24) + ((int32_t) b2 << 16) + ((int32_t) b3 << 8) + b4;
}

/*
 * @brief
 */
static vec_t SwapFloat(vec_t f) {

	union {
		vec_t f;
		byte b[4];
	} dat1, dat2;

	dat1.f = f;

	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];

	return dat2.f;
}

/*
 * @brief
 */
int16_t BigShort(int16_t s) {
	if (SDL_BYTEORDER == SDL_LIL_ENDIAN)
		return SwapShort(s);
	return s;
}

/*
 * @brief
 */
int16_t LittleShort(int16_t s) {
	if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
		return SwapShort(s);
	return s;
}

/*
 * @brief
 */
int32_t BigLong(int32_t l) {
	if (SDL_BYTEORDER == SDL_LIL_ENDIAN)
		return SwapLong(l);
	return l;
}

/*
 * @brief
 */
int32_t LittleLong(int32_t l) {
	if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
		return SwapLong(l);
	return l;
}

/*
 * @brief
 */
vec_t BigFloat(vec_t f) {
	if (SDL_BYTEORDER == SDL_LIL_ENDIAN)
		return SwapFloat(f);
	return f;
}

/*
 * @brief
 */
vec_t LittleFloat(vec_t f) {
	if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
		return SwapFloat(f);
	return f;
}
