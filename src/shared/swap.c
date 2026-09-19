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

#include <SDL3/SDL_endian.h>

/**
 * @brief Byte-swaps a 32-bit float by reversing its byte representation.
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
 * @brief Converts a 16-bit integer to big-endian byte order.
 */
int16_t BigShort(int16_t s) {
  return SDL_Swap16BE(s);
}

/**
 * @brief Converts a 16-bit integer to little-endian byte order.
 */
int16_t LittleShort(int16_t s) {
  return SDL_Swap16LE(s);
}

/**
 * @brief Converts a 32-bit integer to big-endian byte order.
 */
int32_t BigLong(int32_t l) {
  return SDL_Swap32BE(l);
}

/**
 * @brief Converts a 32-bit integer to little-endian byte order.
 */
int32_t LittleLong(int32_t l) {
  return SDL_Swap32LE(l);
}

/**
 * @brief Converts a float to big-endian byte order.
 */
float BigFloat(float f) {
  if (SDL_BYTEORDER == SDL_LIL_ENDIAN) {
    return SwapFloat(f);
  }
  return f;
}

/**
 * @brief Converts a float to little-endian byte order.
 */
float LittleFloat(float f) {
  if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
    return SwapFloat(f);
  }
  return f + 0.f;
}

/**
 * @brief Converts all elements of a `Mat4` to little-endian byte order.
 */
Mat4 LittleMat4(const Mat4 m) {
  Mat4 out = m;
  for (int32_t i = 0; i < 4; i++) {
    for (int32_t j = 0; j < 4; j++) {
      out.m[i][j] = LittleFloat(out.m[i][j]);
    }
  }
  return out;
}

/**
 * @brief Converts all components of a `Vec3s` to little-endian byte order.
 */
Vec3s LittleVec3s(const Vec3s v) {
  return MakeVec3s(LittleShort(v.x),
         LittleShort(v.y),
         LittleShort(v.z));
}

/**
 * @brief Converts all components of a `Vec3i` to little-endian byte order.
 */
Vec3i LittleVec3i(const Vec3i v) {
  return MakeVec3i(LittleLong(v.x),
         LittleLong(v.y),
         LittleLong(v.z));
}

/**
 * @brief Converts all components of a `Vec2` to little-endian byte order.
 */
Vec2 LittleVec2(const Vec2 v) {
  return MakeVec2(LittleFloat(v.x),
        LittleFloat(v.y));
}

/**
 * @brief Converts all components of a `Vec3` to little-endian byte order.
 */
Vec3 LittleVec3(const Vec3 v) {
  return MakeVec3(LittleFloat(v.x),
        LittleFloat(v.y),
        LittleFloat(v.z));
}

/**
 * @brief Converts all components of a `Vec4` to little-endian byte order.
 */
Vec4 LittleVec4(const Vec4 v) {
  return MakeVec4(LittleFloat(v.x),
        LittleFloat(v.y),
        LittleFloat(v.z),
        LittleFloat(v.w));
}

/**
 * @brief Converts both min and max vectors of a `Box3` to little-endian byte order.
 */
Box3 LittleBounds(const Box3 b) {
  return MakeBox3(LittleVec3(b.mins),
          LittleVec3(b.maxs));
}
