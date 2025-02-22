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

#include "rgb9e5.h"

#define RGB9E5_EXPONENT_BITS          5
#define RGB9E5_MANTISSA_BITS          9
#define RGB9E5_EXP_BIAS               15
#define RGB9E5_MAX_VALID_BIASED_EXP   31

#define MAX_RGB9E5_EXP               (RGB9E5_MAX_VALID_BIASED_EXP - RGB9E5_EXP_BIAS)
#define RGB9E5_MANTISSA_VALUES       (1<<RGB9E5_MANTISSA_BITS)
#define MAX_RGB9E5_MANTISSA          (RGB9E5_MANTISSA_VALUES-1)
#define MAX_RGB9E5                   (((float)MAX_RGB9E5_MANTISSA)/RGB9E5_MANTISSA_VALUES * (1<<MAX_RGB9E5_EXP))
#define EPSILON_RGB9E5               ((1.0/RGB9E5_MANTISSA_VALUES) / (1<<RGB9E5_EXP_BIAS))

static float ClampRange_for_rgb9e5(float x)
{
  if (x > 0.0) {
	if (x >= MAX_RGB9E5) {
	  return MAX_RGB9E5;
	} else {
	  return x;
	}
  } else {
	/* NaN gets here too since comparisons with NaN always fail! */
	return 0.0;
  }
}

static float MaxOf3(float x, float y, float z)
{
  if (x > y) {
	if (x > z) {
	  return x;
	} else {
	  return z;
	}
  } else {
	if (y > z) {
	  return y;
	} else {
	  return z;
	}
  }
}

/* Ok, FloorLog2 is not correct for the denorm and zero values, but we
   are going to do a max of this value with the minimum rgb9e5 exponent
   that will hide these problem cases. */
static int FloorLog2(float x)
{
  float754 f;

  f.value = x;
  return (f.field.biasedexponent - 127);
}

static int Max(int x, int y)
{
  if (x > y) {
	return x;
  } else {
	return y;
  }
}

rgb9e5 float3_to_rgb9e5(const float rgb[3])
{
  rgb9e5 retval;
  float maxrgb, maxm;
  int rm, gm, bm;
  float rc, gc, bc;
  int exp_shared;
  double denom;

  rc = ClampRange_for_rgb9e5(rgb[0]);
  gc = ClampRange_for_rgb9e5(rgb[1]);
  bc = ClampRange_for_rgb9e5(rgb[2]);

  maxrgb = MaxOf3(rc, gc, bc);
  exp_shared = Max(-RGB9E5_EXP_BIAS-1, FloorLog2(maxrgb)) + 1 + RGB9E5_EXP_BIAS;
  assert(exp_shared <= RGB9E5_MAX_VALID_BIASED_EXP);
  assert(exp_shared >= 0);
  /* This pow function could be replaced by a table. */
  denom = pow(2, exp_shared - RGB9E5_EXP_BIAS - RGB9E5_MANTISSA_BITS);

  maxm = (int) floor(maxrgb / denom + 0.5);
  if (maxm == MAX_RGB9E5_MANTISSA+1) {
	denom *= 2;
	exp_shared += 1;
	assert(exp_shared <= RGB9E5_MAX_VALID_BIASED_EXP);
  } else {
	assert(maxm <= MAX_RGB9E5_MANTISSA);
  }

  rm = (int) floor(rc / denom + 0.5);
  gm = (int) floor(gc / denom + 0.5);
  bm = (int) floor(bc / denom + 0.5);

  assert(rm <= MAX_RGB9E5_MANTISSA);
  assert(gm <= MAX_RGB9E5_MANTISSA);
  assert(bm <= MAX_RGB9E5_MANTISSA);
  assert(rm >= 0);
  assert(gm >= 0);
  assert(bm >= 0);

  retval.field.r = rm;
  retval.field.g = gm;
  retval.field.b = bm;
  retval.field.biasedexponent = exp_shared;

  return retval;
}

void rgb9e5_to_float3(rgb9e5 v, float retval[3])
{
  int exponent = v.field.biasedexponent - RGB9E5_EXP_BIAS - RGB9E5_MANTISSA_BITS;
  float scale = (float) pow(2, exponent);

  retval[0] = v.field.r * scale;
  retval[1] = v.field.g * scale;
  retval[2] = v.field.b * scale;
}
