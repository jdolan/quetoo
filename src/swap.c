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
 * SwapShort
 */
static short SwapShort(short l) {

	const byte b1 = l & 255;
	const byte b2 = (l >> 8) & 255;

	return (b1 << 8) + b2;
}

/*
 * SwapLong
 */
static int SwapLong(int l) {

	const byte b1 = l & 255;
	const byte b2 = (l >> 8) & 255;
	const byte b3 = (l >> 16) & 255;
	const byte b4 = (l >> 24) & 255;

	return ((int) b1 << 24) + ((int) b2 << 16) + ((int) b3 << 8) + b4;
}

/*
 * SwapFloat
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

/*
 * BigShort
 */
short BigShort(short s) {
	if (SDL_BYTEORDER == SDL_LIL_ENDIAN)
		return SwapShort(s);
	return s;
}

/*
 * LittleShort
 */
short LittleShort(short s) {
	if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
		return SwapShort(s);
	return s;
}

/*
 * BigLong
 */
int BigLong(int l) {
	if (SDL_BYTEORDER == SDL_LIL_ENDIAN)
		return SwapLong(l);
	return l;
}

/*
 * LittleLong
 */
int LittleLong(int l) {
	if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
		return SwapLong(l);
	return l;
}

/*
 * BigFloat
 */
float BigFloat(float f) {
	if (SDL_BYTEORDER == SDL_LIL_ENDIAN)
		return SwapFloat(f);
	return f;
}

/*
 * LittleFloat
 */
float LittleFloat(float f) {
	if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
		return SwapFloat(f);
	return f;
}
