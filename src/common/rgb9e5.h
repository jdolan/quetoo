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

/*
 * This code is adapted from https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_shared_exponent.txt
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL3/SDL_endian.h>

#define RGB9E5_EXPONENT_BITS          5
#define RGB9E5_MANTISSA_BITS          9
#define RGB9E5_EXP_BIAS               15
#define RGB9E5_MAX_VALID_BIASED_EXP   31

#define MAX_RGB9E5_EXP               (RGB9E5_MAX_VALID_BIASED_EXP - RGB9E5_EXP_BIAS)
#define RGB9E5_MANTISSA_VALUES       (1<<RGB9E5_MANTISSA_BITS)
#define MAX_RGB9E5_MANTISSA          (RGB9E5_MANTISSA_VALUES-1)
#define MAX_RGB9E5                   (((float)MAX_RGB9E5_MANTISSA)/RGB9E5_MANTISSA_VALUES * (1<<MAX_RGB9E5_EXP))
#define EPSILON_RGB9E5               ((1.0/RGB9E5_MANTISSA_VALUES) / (1<<RGB9E5_EXP_BIAS))

typedef struct {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
  unsigned int negative:1;
  unsigned int biasedexponent:8;
  unsigned int mantissa:23;
#else
  unsigned int mantissa:23;
  unsigned int biasedexponent:8;
  unsigned int negative:1;
#endif
} BitsOfIEEE754;

typedef union {
  unsigned int raw;
  float value;
  BitsOfIEEE754 field;
} float754;

typedef struct {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
  unsigned int biasedexponent:RGB9E5_EXPONENT_BITS;
  unsigned int b:RGB9E5_MANTISSA_BITS;
  unsigned int g:RGB9E5_MANTISSA_BITS;
  unsigned int r:RGB9E5_MANTISSA_BITS;
#else
  unsigned int r:RGB9E5_MANTISSA_BITS;
  unsigned int g:RGB9E5_MANTISSA_BITS;
  unsigned int b:RGB9E5_MANTISSA_BITS;
  unsigned int biasedexponent:RGB9E5_EXPONENT_BITS;
#endif
} BitsOfRGB9E5;

typedef union {
  unsigned int raw;
  BitsOfRGB9E5 field;
} rgb9e5;

rgb9e5 float3_to_rgb9e5(const float rgb[3]);

void rgb9e5_to_float3(rgb9e5 v, float retval[3]);
