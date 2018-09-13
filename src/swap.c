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
vec_t BigFloat(vec_t f) {
	if (SDL_BYTEORDER == SDL_LIL_ENDIAN) {
		return SwapFloat(f);
	}
	return f;
}

/**
 * @brief
 */
vec_t LittleFloat(vec_t f) {
	if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
		return SwapFloat(f);
	}
	return f;
}
