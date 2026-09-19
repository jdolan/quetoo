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

#include "quetoo.h"

/**
 * @brief Two component signed short vector type.
 */
typedef union {

  /**
   * @brief Array accessor.
   */
  int16_t xy[2];

  /**
   * @brief Component accessors.
   */
  struct {
    int16_t x, y;
  };
} Vec2s;

/**
 * @brief Three component signed short vector type.
 */
typedef union {

  /**
   * @brief Array accessor.
   */
  int16_t xyz[3];

  /**
   * @brief Component accessors.
   */
  struct {
    int16_t x, y, z;
  };
} Vec3s;

/**
 * @brief Three component signed integer vector type.
 */
typedef union {

  /**
   * @brief Array accessor.
   */
  int32_t xyz[3];

  /**
   * @brief Component accessors.
   */
  struct {
    int32_t x, y, z;
  };
} Vec3i;

/**
 * @brief Four component signed integer vector type.
 */
typedef union {

  /**
   * @brief Array accessor.
   */
  int32_t xyzw[4];

  /**
   * @brief Component accessors.
   */
  struct {
    int32_t x, y, z, w;
  };
} Vec4i;

/**
 * @brief Two component single precision vector type.
 */
typedef union {

  /**
   * @brief Array accessor.
   */
  float xy[2];

  /**
   * @brief Component accessors.
   */
  struct {
    float x, y;
  };
} Vec2;

/**
 * @brief Three component single precision vector type.
 */
typedef union {

  /**
   * @brief Array accessor.
   */
  float xyz[3];

  /**
   * @brief Component accessors.
   */
  struct {
    float x, y, z;
  };

  /**
   * @brief Swizzle.
   */
  Vec2 xy;
} Vec3;

/**
 * @brief Four component single precision vector type.
 */
typedef union {

  /**
   * @brief Array accessor.
   */
  float xyzw[4];

  /**
   * @brief Component accessors.
   */
  struct {
    float x, y, z, w;
  };

  /**
   * @brief Swizzle.
   */
  Vec3 xyz;

  struct {

    /**
     * @brief Swizzle.
     */
    Vec2 xy;

    /**
     * @brief Swizzle.
     */
    Vec2 zw;
  };
} Vec4;

/**
 * @brief Three component double precision vector type.
 */
typedef union {

  /**
   * @brief Array accessor.
   */
  double xyz[3];

  /**
   * @brief Component accessors.
   */
  struct {
    double x, y, z;
  };
} Vec3d;

#pragma mark - short vectors

/**
 * @return A `Vec2s` with the specified components.
 */
static inline Vec2s __attribute__ ((warn_unused_result)) MakeVec2s(int16_t x, int16_t y) {
  return (Vec2s) {
    .x = x,
    .y = y
  };
}

/**
 * @return The Vector `v` scaled by `scale`.
 */
static inline Vec2s __attribute__ ((warn_unused_result)) Vec2s_Scale(const Vec2s v, float scale) {
  return MakeVec2s((int16_t) v.x * scale, (int16_t) v.y * scale);
}

/**
 * @return The vector `(0, 0)`.
 */
static inline Vec2s __attribute__ ((warn_unused_result)) Vec2s_Zero(void) {
  return MakeVec2s(0, 0);
}

/**
 * @return A `s16vec3_t` with the specified components.
 */
static inline Vec3s __attribute__ ((warn_unused_result)) MakeVec3s(int16_t x, int16_t y, int16_t z) {
  return (Vec3s) {
    .x = x,
    .y = y,
    .z = z
  };
}

/**
 * @return The integer vector `v` cast to `Vec3`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3s_CastVec3(const Vec3s v) {
  return (Vec3) {
    .x = (float) v.x,
    .y = (float) v.y,
    .z = (float) v.z
  };
}

/**
 * @return True if `a` and `b` are equal.
 */
static inline bool __attribute__ ((warn_unused_result)) Vec3s_Equal(const Vec3s a, Vec3s b) {
  return a.x == b.x &&
       a.y == b.y &&
       a.z == b.z;
}

/**
 * @return The vector `(0, 0, 0)`.
 */
static inline Vec3s __attribute__ ((warn_unused_result)) Vec3s_Zero(void) {
  return MakeVec3s(0, 0, 0);
}

#pragma mark - integer vectors

/**
 * @return A `Vec3i` with the specified components.
 */
static inline Vec3i __attribute__ ((warn_unused_result)) MakeVec3i(int32_t x, int32_t y, int32_t z) {
  return (Vec3i) {
    .x = x,
    .y = y,
    .z = z
  };
}

/**
 * @return The sum of `a + b`.
 */
static inline Vec3i __attribute__ ((warn_unused_result)) Vec3i_Add(const Vec3i a, const Vec3i b) {
  return MakeVec3i(a.x + b.x, a.y + b.y, a.z + b.z);
}

/**
 * @return The vector `(0, 0, 0)`.
 */
static inline Vec3i __attribute__ ((warn_unused_result)) Vec3i_Zero(void) {
  return MakeVec3i(0, 0, 0);
}

/**
 * @return A `Vec4i` with the specified components.
 */
static inline Vec4i __attribute__ ((warn_unused_result)) MakeVec4i(int32_t x, int32_t y, int32_t z, int32_t w) {
  return (Vec4i) {
    .x = x,
    .y = y,
    .z = z,
    .w = w
  };
}

/**
 * @return The vector `(0, 0, 0, 0)`.
 */
static inline Vec4i __attribute__ ((warn_unused_result)) Vec4i_Zero(void) {
  return MakeVec4i(0, 0, 0, 0);
}

#pragma mark - single precision

/**
 * @brief Wraps an angle in degrees to the range [0, 360).
 */
static inline float __attribute__ ((warn_unused_result)) AngleMod(float a) {
  a = fmodf(a, 360.f);

  if (a < 0) {
    return a + (((int32_t)(a / 360.f) + 1) * 360.f);
  }

  return a;
}

/**
 * @return The minimum of `a` and `b`.
 */
static inline float __attribute__ ((warn_unused_result)) Minf(float a, float b) {
  return a < b ? a : b;
}

/**
 * @return The minimum of `a` and `b`.
 */
static inline int32_t __attribute__ ((warn_unused_result)) Mini(int32_t a, int32_t b) {
  return a < b ? a : b;
}

/**
 * @return The minimum of `a` and `b`.
 */
static inline uint64_t __attribute__ ((warn_unused_result)) Minui64(uint64_t a, uint64_t b) {
  return a < b ? a : b;
}

/**
 * @return The minimum of `a` and `b`.
 */
static inline size_t __attribute__ ((warn_unused_result)) Minz(size_t a, size_t b) {
  return a < b ? a : b;
}

/**
 * @return The maximum of `a` and `b`.
 */
static inline float __attribute__ ((warn_unused_result)) Maxf(float a, float b) {
  return a > b ? a : b;
}

/**
 * @return The maximum of `a` and `b`.
 */
static inline int32_t __attribute__ ((warn_unused_result)) Maxi(int32_t a, int32_t b) {
  return a > b ? a : b;
}

/**
 * @return The maximum of `a` and `b`.
 */
static inline int64_t __attribute__ ((warn_unused_result)) Maxui64(uint64_t a, uint64_t b) {
  return a > b ? a : b;
}

/**
 * @return The maximum of `a` and `b`.
 */
static inline size_t __attribute__ ((warn_unused_result)) Maxz(size_t a, size_t b) {
  return a > b ? a : b;
}

/**
 * @return The value `f`, clamped to the specified `min` and `max`.
 */
static inline float __attribute__ ((warn_unused_result)) Clampf(float f, float min, float max) {
  return Minf(Maxf(f, min), max);
}

/**
 * @return The value `f`, clamped to 0.0 and 1.0.
 */
static inline float __attribute__ ((warn_unused_result)) Clampf01(float f) {
  return Minf(Maxf(f, 0.f), 1.f);
}

#define DegreesScalar ((float) (180.0 / M_PI))
#define RadiansScalar ((float) (M_PI / 180.0))

/**
 * @return Radians in degrees.
 */
static inline float __attribute__ ((warn_unused_result)) Degrees(float radians) {
  return radians * DegreesScalar;
}

/**
 * @return True if `fabsf(a - b) <= epsilon`.
 */
static inline bool __attribute__ ((warn_unused_result)) EqualEpsilonf(float a, float b, float epsilon) {
  return (isinf(a) && isinf(b)) || fabsf(a - b) <= epsilon;
}

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
static inline float __attribute__ ((warn_unused_result)) Mixf(float a, float b, float mix) {
  return a * (1.f - mix) + b * mix;
}

/**
 * @return Degrees in radians.
 */
static inline float __attribute__ ((warn_unused_result)) Radians(float degrees) {
  return degrees * RadiansScalar;
}

/**
 * @brief Returns the smallest absolute angular difference between two angles in degrees.
 */
static inline float __attribute__ ((warn_unused_result)) SmallestAngleBetween(const float x, const float y) {
  return Minf(360.f - fabsf(x - y), fabsf(x - y));
}

/**
 * @return The angle `theta` circularly clamped.
 */
static inline float __attribute__ ((warn_unused_result)) ClampEuler(float theta) {
  while (theta >= 360.f) {
    theta -= 360.f;
  }
  while (theta < 0.f) {
    theta += 360.f;
  }
  return theta;
}


/**
 * @return A thread-local pseudo-random 32-bit value (xorshift64 state).
 */
static inline uint32_t Randomu(void) {
  static _Thread_local uint64_t state;
  if (!state) {
    state = (uint64_t) time(NULL) ^ (uint64_t) (uintptr_t) &state;
    if (!state) { state = 1; }
  }
  state ^= state << 13;
  state ^= state >> 7;
  state ^= state << 17;
  return (uint32_t) state;
}

/**
 * @return A psuedo random single precision value in [0.0, 1.0).
 */
static inline float __attribute__ ((warn_unused_result)) Randomf(void) {
  return (float) Randomu() * 0x1p-32f;
}

/**
 * @return A psuedo random boolean.
 */
static inline bool __attribute__ ((warn_unused_result)) Randomb(void) {
  return !!(Randomu() & 1);
}

/**
 * @return A psuedo random number between `begin` and `end`.
 */
static inline float __attribute__ ((warn_unused_result)) RandomRangef(float begin, float end) {
  return begin + (end - begin) * Randomf();
}

/**
 * @return A psuedo random integral value between `INT_MIN` and `INT_MAX`.
 */
static inline int32_t __attribute__ ((warn_unused_result)) Randomi(void) {
  return (int32_t) Randomu();
}

/**
 * @return A psuedo random number between `begin` and `end`.
 */
static inline int32_t __attribute__ ((warn_unused_result)) RandomRangei(int32_t begin, int32_t end) {
  if (end <= begin) { return begin; }
  return begin + (int32_t) (Randomu() % (uint32_t) ((int64_t) end - begin));
}

/**
 * @return A psuedo random number between `begin` and `end`.
 */
static inline uint32_t __attribute__ ((warn_unused_result)) RandomRangeu(uint32_t begin, uint32_t end) {
  if (end <= begin) { return begin; }
  return begin + Randomu() % (end - begin);
}

/**
 * @brief Returns a random number between 0 and 2 pi.
 */
static inline float __attribute__ ((warn_unused_result)) RandomRadian(void) {
  return Randomf() * (float) (M_PI * 2.0);
}

/**
 * @return The sign of the specified float.
 */
static inline int32_t __attribute__ ((warn_unused_result)) SignOf(float f) {
  return (f > 0.f) - (f < 0.f);
}

/**
 * @return The Hermite interpolation of `f`.
 */
static inline float __attribute__ ((warn_unused_result)) Smoothf(float f, float min, float max) {
  const float s = Clampf01((f - min) / (max - min));
  return s * s;
}

#pragma mark - Vec2

/**
 * @return A `Vec2` with the specified components.
 */
static inline Vec2 __attribute__ ((warn_unused_result)) MakeVec2(float x, float y) {
  return (Vec2) {
    .x = x + 0.f,
    .y = y + 0.f
  };
}

/**
 * @return The vector sum of `a + b`.
 */
static inline Vec2 __attribute__ ((warn_unused_result)) Vec2_Add(const Vec2 a, const Vec2 b) {
  return MakeVec2(a.x + b.x, a.y + b.y);
}

/**
 * @return The difference of `a - b`.
 */
static inline Vec2 __attribute__ ((warn_unused_result)) Vec2_Subtract(const Vec2 a, const Vec2 b) {
  return MakeVec2(a.x - b.x, a.y - b.y);
}

/**
 * @return The dot product of `a · b`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec2_Dot(const Vec2 a, const Vec2 b) {
  return a.x * b.x + a.y * b.y;
}

/**
 * @return The squared length (magnitude) of `v`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec2_LengthSquared(const Vec2 v) {
  return Vec2_Dot(v, v);
}

/**
 * @return The length (magnitude) of `v`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec2_Length(const Vec2 v) {
  return sqrtf(Vec2_LengthSquared(v));
}

/**
 * @return The length squared of the vector `a - b`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec2_DistanceSquared(const Vec2 a, const Vec2 b) {
  return Vec2_LengthSquared(Vec2_Subtract(a, b));
}

/**
 * @return The length of the vector `a - b`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec2_Distance(const Vec2 a, const Vec2 b) {
  return Vec2_Length(Vec2_Subtract(a, b));
}

/**
 * @return True if `a` and `b` are equal, using the specified epsilon.
 */
static inline bool __attribute__ ((warn_unused_result)) Vec2_EqualEpsilon(const Vec2 a, const Vec2 b, float epsilon) {
  return EqualEpsilonf(a.x, b.x, epsilon) &&
       EqualEpsilonf(a.y, b.y, epsilon);
}

/**
 * @return True if `a` and `b` are equal.
 */
static inline bool __attribute__ ((warn_unused_result)) Vec2_Equal(const Vec2 a, const Vec2 b) {
  return Vec2_EqualEpsilon(a, b, __FLT_EPSILON__);
}

/**
 * @return A vector containing the max components of `a` and `b`.
 */
static inline Vec2 __attribute__ ((warn_unused_result)) Vec2_Maxf(const Vec2 a, const Vec2 b) {
  return MakeVec2(Maxf(a.x, b.x), Maxf(a.y, b.y));
}

/**
 * @return The vector `(-FLT_MAX, -FLT_MAX)`.
 */
static inline Vec2 __attribute__ ((warn_unused_result)) Vec2_Maxs(void) {
  return MakeVec2(-FLT_MAX, -FLT_MAX);
}

/**
 * @return A vector containing the min components of `a` and `b`.
 */
static inline Vec2 __attribute__ ((warn_unused_result)) Vec2_Minf(const Vec2 a, const Vec2 b) {
  return MakeVec2(Minf(a.x, b.x), Minf(a.y, b.y));
}

/**
 * @return The vector `(FLT_MAX, FLT_MAX)`.
 */
static inline Vec2 __attribute__ ((warn_unused_result)) Vec2_Mins(void) {
  return MakeVec2(FLT_MAX, FLT_MAX);
}

/**
 * @return The vector `v` scaled by `scale`.
 */
static inline Vec2 __attribute__ ((warn_unused_result)) Vec2_Scale(const Vec2 v, float scale) {
  return MakeVec2(v.x * scale, v.y * scale);
}

/**
 * @return The vector `v` + (`add` * `multiply`).
 */
static inline Vec2 __attribute__ ((warn_unused_result)) Vec2_Fmaf(const Vec2 v, float multiply, const Vec2 add) {
  return MakeVec2(fmaf(add.x, multiply, v.x), fmaf(add.y, multiply, v.y));
}

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
static inline Vec2 __attribute__ ((warn_unused_result)) Vec2_Mix(const Vec2 a, const Vec2 b, float mix) {
  return Vec2_Add(Vec2_Scale(a, 1.f - mix), Vec2_Scale(b, mix));
}

/**
 * @return The zero vector.
 */
static inline Vec2 __attribute__ ((warn_unused_result)) Vec2_Zero(void) {
  return MakeVec2(0.f, 0.f);
}

#pragma mark - Vec3

/**
 * @return A `Vec3` with the specified components.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) MakeVec3(float x, float y, float z) {
  return (Vec3) {
    .x = x + 0.f,
    .y = y + 0.f,
    .z = z + 0.f
  };
}

/**
 * @return A `Vec3` from the specified bytes.
 * @see Vec3_Bytes
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3bv(const byte *bytes) {
  return MakeVec3(
    (bytes[0] / 255.f) * 2.f - 1.f,
    (bytes[1] / 255.f) * 2.f - 1.f,
    (bytes[2] / 255.f) * 2.f - 1.f
  );
}

/**
 * @return A `Vec3` comprised of the specified `Vec2` and `z`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec2_ToVec3(const Vec2 v, float z) {
  return MakeVec3(v.x, v.y, z);
}

/**
 * @return The vector `(0.f, 0.f, 0.f)`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Zero(void) {
  return MakeVec3(0.f, 0.f, 0.f);
}

/**
 * @return The vector `v` cast to `Vec3`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3i_CastVec3(const Vec3i v) {
  return MakeVec3((float) v.x, (float) v.y, (float) v.z);
}

/**
 * @return The vector sum of `a + b`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Add(const Vec3 a, const Vec3 b) {
  return MakeVec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

/**
 * @return The difference of `a - b`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Subtract(const Vec3 a, const Vec3 b) {
  return MakeVec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

/**
 * @return The vector `v` cast to `Vec3d`.
 */
static inline Vec3d __attribute__ ((warn_unused_result)) Vec3_CastVec3d(const Vec3 v) {
  return (Vec3d) {
    .x = (double) v.x,
    .y = (double) v.y,
    .z = (double) v.z
  };
}

/**
 * @return The vector `v` cast to `s16vec3_t`.
 */
static inline Vec3s __attribute__ ((warn_unused_result)) Vec3_CastVec3s(const Vec3 v) {
  return (Vec3s) {
    .x = (int16_t) v.x,
    .y = (int16_t) v.y,
    .z = (int16_t) v.z
  };
}

/**
 * @return The vector `v` cast to `s32vec3_t`.
 */
static inline Vec3i __attribute__ ((warn_unused_result)) Vec3_CastVec3i(const Vec3 v) {
  return (Vec3i) {
    .x = (int32_t) v.x,
    .y = (int32_t) v.y,
    .z = (int32_t) v.z
  };
}

/**
 * @brief A vector containing the components of `v`, rounded to the nearest higher integer.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Ceilf(const Vec3 v) {
  return MakeVec3(ceilf(v.x),
        ceilf(v.y),
        ceilf(v.z));
}

/**
 * @return The specified Euler angles circularly clamped to `0.f - 360.f`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_ClampEuler(const Vec3 euler) {
  return MakeVec3(ClampEuler(euler.x),
        ClampEuler(euler.y),
        ClampEuler(euler.z));
}

/**
 * @return The cross product of `a ✕ b`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Cross(const Vec3 a, const Vec3 b) {
  return MakeVec3(a.y * b.z - a.z * b.y,
         a.z * b.x - a.x * b.z,
         a.x * b.y - a.y * b.x);
}

/**
 * @return The vector `v` scaled by `scale`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Scale(const Vec3 v, float scale) {
  return MakeVec3(v.x * scale, v.y * scale, v.z * scale);
}

/**
 * @return The negated vector `v`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Negate(const Vec3 v) {
  return Vec3_Scale(v, -1.f);
}

/**
 * @return The dot product of `a · b`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec3_Dot(const Vec3 a, const Vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

/**
 * @return The squared length (magnitude) of `v`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec3_LengthSquared(const Vec3 v) {
  return Vec3_Dot(v, v);
}

/**
 * @return The length (magnitude) of `v`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec3_Length(const Vec3 v) {
  return sqrtf(Vec3_LengthSquared(v));
}

/**
 * @return The normalized vector `v`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_NormalizeLength(const Vec3 v, float *length) {
  *length = Vec3_Length(v);
  if (*length > 0.f) {
    return Vec3_Scale(v, 1.f / *length);
  } else {
    return Vec3_Zero();
  }
}

/**
 * @return The normalized vector `v`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Normalize(const Vec3 v) {
  float length;
  return Vec3_NormalizeLength(v, &length);
}

/**
 * @return The length of `a - b` as well as the normalized directional vector.
 */
static inline float __attribute__ ((warn_unused_result)) Vec3_DistanceDir(const Vec3 a, const Vec3 b, Vec3 *dir) {
  float length;

  *dir = Vec3_NormalizeLength(Vec3_Subtract(a, b), &length);

  return length;
}

/**
 * @return The direction vector between points a and b.
 */
static inline Vec3 __attribute__((warn_unused_result)) Vec3_Direction(const Vec3 a, const Vec3 b) {
  return Vec3_Normalize(Vec3_Subtract(a, b));
}

/**
 * @return The squared length of the vector `a - b`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec3_DistanceSquared(const Vec3 a, const Vec3 b) {
  return Vec3_LengthSquared(Vec3_Subtract(a, b));
}

/**
 * @return The length of the vector `a - b`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec3_Distance(const Vec3 a, const Vec3 b) {
  return Vec3_Length(Vec3_Subtract(a, b));
}

/**
 * @return The quotient of `a / b`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Divide(const Vec3 a, const Vec3 b) {
  return MakeVec3(a.x / b.x, a.y / b.y, a.z / b.z);
}

/**
 * @return The up vector `(0.f, 0.f, 1.f)`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Up(void) {
  return MakeVec3(0.f, 0.f, 1.f);
}

/**
 * @return The down vector `(0.f, 0.f, -1.f)`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Down(void) {
  return Vec3_Negate(Vec3_Up());
}

/**
 * @return True if `a` and `b` are equal using the specified epsilon.
 */
static inline bool __attribute__ ((warn_unused_result)) Vec3_EqualEpsilon(const Vec3 a, const Vec3 b, float epsilon) {
  return EqualEpsilonf(a.x, b.x, epsilon) &&
       EqualEpsilonf(a.y, b.y, epsilon) &&
       EqualEpsilonf(a.z, b.z, epsilon);
}

/**
 * @return True if `a` and `b` are equal.
 */
static inline bool __attribute__ ((warn_unused_result)) Vec3_Equal(const Vec3 a, const Vec3 b) {
  return Vec3_EqualEpsilon(a, b, __FLT_EPSILON__);
}

/**
 * @return The euler angles, in radians, for the directional vector `dir`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Euler(const Vec3 dir) {
  float pitch, yaw;

  if (dir.y == 0.f && dir.x == 0.f) {
    yaw = 0.f;
    if (dir.z > 0.f) {
      pitch = 90.f;
    } else {
      pitch = 270.f;
    }
  } else {
    if (dir.x) {
      yaw = Degrees(atan2f(dir.y, dir.x));
    } else if (dir.y > 0.f) {
      yaw = 90.f;
    } else {
      yaw = 270.f;
    }

    if (yaw < 0.f) {
      yaw += 360.f;
    }

    const float forward = sqrtf(dir.x * dir.x + dir.y * dir.y);
    pitch = Degrees(atan2f(dir.z, forward));

    if (pitch < 0.f) {
      pitch += 360.f;
    }
  }

  return MakeVec3(-pitch, yaw, 0);
}

/**
 * @return A vector containing the absolute values of `v`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Fabsf(const Vec3 v) {
  return MakeVec3(fabsf(v.x), fabsf(v.y), fabsf(v.z));
}

/**
 * @brief A vector containing the components of `v`, rounded to the nearest lower integer.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Floorf(const Vec3 v) {
  return MakeVec3(floorf(v.x),
        floorf(v.y),
        floorf(v.z));
}

/**
 * @return The vector `v` + (`add` * `multiply`).
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Fmaf(const Vec3 v, float multiply, const Vec3 add) {
  return MakeVec3(fmaf(add.x, multiply, v.x), fmaf(add.y, multiply, v.y), fmaf(add.z, multiply, v.z));
}

/**
 * @return The vector containing the floating point modulo of `a / b`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Fmodf(const Vec3 a, const Vec3 b) {
  return MakeVec3(fmodf(a.x, b.x), fmodf(a.y, b.y), fmodf(a.z, b.z));
}

/**
 * @return A vector containing the max component of `v`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec3_Hmaxf(const Vec3 v) {
  return Maxf(Maxf(v.x, v.y), v.z);
}

/**
 * @return A vector containing the min component of `v`.
 */
static inline float __attribute__ ((warn_unused_result)) Vec3_Hminf(const Vec3 v) {
  return Minf(Minf(v.x, v.y), v.z);
}

/**
 * @return A vector containing the max components of `a` and `b`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Maxf(const Vec3 a, const Vec3 b) {
  return MakeVec3(Maxf(a.x, b.x), Maxf(a.y, b.y), Maxf(a.z, b.z));
}

/**
 * @return The vector `(-FLT_MAX, -FLT_MAX, -FLT_MAX)`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Maxs(void) {
  return MakeVec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
}

/**
 * @return A vector containing the min components of `a` and `b`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Minf(const Vec3 a, const Vec3 b) {
  return MakeVec3(Minf(a.x, b.x), Minf(a.y, b.y), Minf(a.z, b.z));
}

/**
 * @return The vector `(FLT_MAX, FLT_MAX, FLT_MAX)`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Mins(void) {
  return MakeVec3(FLT_MAX, FLT_MAX, FLT_MAX);
}

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Mix(const Vec3 a, const Vec3 b, float mix) {
  return Vec3_Add(Vec3_Scale(a, 1.f - mix), Vec3_Scale(b, mix));
}

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_MixEuler(const Vec3 a, const Vec3 b, float mix) {

  Vec3 _a = a;
  Vec3 _b = b;

  for (size_t i = 0; i < 3; i++) {
    if (_b.xyz[i] - _a.xyz[i] >= 180.f) {
      _a.xyz[i] += 360.f;
    } else if (_b.xyz[i] - _a.xyz[i] <= -180.f) {
      _b.xyz[i] += 360.f;
    }
  }

  return Vec3_Mix(_a, _b, mix);
}

/**
 * @return The product `a * b`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Multiply(const Vec3 a, const Vec3 b) {
  return MakeVec3(a.x * b.x, a.y * b.y, a.z * b.z);
}

/**
 * @return The vector `(1.f, 1.f, 1.f)`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_One(void) {
  return MakeVec3(1.f, 1.f, 1.f);
}

/**
 * @return The linear interpolation of `a` and `b` using the specified fractions.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Mix3(const Vec3 a, const Vec3 b, const Vec3 mix) {
  return Vec3_Add(Vec3_Multiply(a, Vec3_Subtract(Vec3_One(), mix)), Vec3_Multiply(b, mix));
}

/**
 * @return The vector `a` raised the exponent `exp`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Pow(const Vec3 a, float exp) {
  return MakeVec3(powf(a.x, exp), powf(a.y, exp), powf(a.z, exp));
}

/**
 * @return The vector `degrees` in radians.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Radians(const Vec3 degrees) {
  return Vec3_Scale(degrees, RadiansScalar);
}

/**
 * @return A vector with random values between the respective ranges.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_RandomRanges(
        float x_begin, float x_end,
        float y_begin, float y_end,
        float z_begin, float z_end) {
  return MakeVec3(RandomRangef(x_begin, x_end),
        RandomRangef(y_begin, y_end),
        RandomRangef(z_begin, z_end));
}

/**
 * @return A vector with random values between `begin` and `end`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_RandomRange(float begin, float end) {
  return Vec3_RandomRanges(begin, end, begin, end, begin, end);
}

/**
 * @return A vector with random values between `0` and `1`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Random(void) {
  return Vec3_RandomRange(0.f, 1.f);
}

/**
 * @return Returns a random vector (positive or negative) with a length of 1.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_RandomDir(void) {
  return Vec3_Normalize(Vec3_RandomRange(-1.f, 1.f));
}

/**
 * @return Takes a vector and randomizes its direction where a `random` of 0 yields the original vector and 1 yields a completely random direction.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_RandomizeDir(const Vec3 dir, const float randomization) {
  float length;
  Vec3 direction = Vec3_NormalizeLength(dir, &length);
  Vec3 result = Vec3_Mix(direction, Vec3_RandomDir(), Clampf(randomization, 0.f, 1.f));
  return Vec3_Scale(Vec3_Normalize(result), length);
}

/**
 * @return The vector `v` rounded to the nearest integer values.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Roundf(const Vec3 v) {
  return MakeVec3(roundf(v.x), roundf(v.y), roundf(v.z));
}

/**
 * @return The vector `v` quantized to the given step.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Quantize(const Vec3 v, float step) {
  return Vec3_Scale(Vec3_Roundf(Vec3_Scale(v, 1.f / step)), step);
}

/**
 * @return
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Clamp(const Vec3 v, Vec3 min, Vec3 max) {
  return MakeVec3(
    Clampf(v.x, min.x, max.x),
    Clampf(v.y, min.y, max.y),
    Clampf(v.z, min.z, max.z));
}

/**
 * @return
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Clampf(const Vec3 v, float min, float max) {
  return MakeVec3(
    Clampf(v.x, min, max),
    Clampf(v.y, min, max),
    Clampf(v.z, min, max));
}

/**
 * @return
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Clampf01(const Vec3 v) {
  return MakeVec3(
    Clampf(v.x, 0.0, 1.0),
    Clampf(v.y, 0.0, 1.0),
    Clampf(v.z, 0.0, 1.0));
}

/**
 * @return The vector `a` reflected by the vector `b`.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3_Reflect(const Vec3 a, const Vec3 b) {
  return Vec3_Add(a, Vec3_Scale(b, -2.f * Vec3_Dot(a, b)));
}

/**
 * @return The tangent and bitangent vectors for the given normal and texture directional vectors.
 */
static inline void Vec3_Tangents(const Vec3 normal, const Vec3 sdir, const Vec3 tdir, Vec3 *tangent, Vec3 *bitangent) {
  const Vec3 t = sdir;
  const Vec3 b = tdir;
  const Vec3 n = normal;

  *tangent = Vec3_Normalize(Vec3_Subtract(t, Vec3_Scale(n, Vec3_Dot(t, n))));
  *bitangent = Vec3_Normalize(Vec3_Cross(n, *tangent));

  if (Vec3_Dot(*bitangent, b) < 0.f) {
    *bitangent = Vec3_Negate(*bitangent);
  }
}

/**
 * @brief Computes the sine and cosine of `rad` simultaneously.
 */
static inline void SinCosf(const float rad, float *s, float *c) {
  *s = sinf(rad);
  *c = cosf(rad);
}

/**
 * @brief Encodes the normalized directional vector `v` into a `Vec2s`.
 * @details This is useful for encoding directional vectors in textures, where 32 bits formats
 * are widely supported. The Z component of the vector can be regenerated.
 */
static inline Vec2s Vec3_Vec2s(const Vec3 v) {
  return (Vec2s) {
    .x = v.x * INT16_MAX,
    .y = v.y * INT16_MAX
  };
}

/**
 * @return The forward, right and up vectors for the euler angles in radians.
 */
static inline void Vec3_Vectors(const Vec3 euler, Vec3 *forward, Vec3 *right, Vec3 *up) {
  float sr, sp, sy, cr, cp, cy;

  SinCosf(Radians(euler.x), &sp, &cp);
  SinCosf(Radians(euler.y), &sy, &cy);
  SinCosf(Radians(euler.z), &sr, &cr);

  if (forward) {
    forward->x = cp * cy;
    forward->y = cp * sy;
    forward->z = -sp;
  }

  if (right) {
    right->x = (-1.f * sr * sp * cy + -1.f * cr * -sy);
    right->y = (-1.f * sr * sp * sy + -1.f * cr * cy);
    right->z =  -1.f * sr * cp;
  }

  if (up) {
    up->x = (cr * sp * cy + -sr * -sy);
    up->y = (cr * sp * sy + -sr * cy);
    up->z = cr * cp;
  }
}

/**
 * @return The `xy` swizzle of `v`.
 */
static inline Vec2 __attribute__ ((warn_unused_result)) Vec3_XY(const Vec3 v) {
  return MakeVec2(v.x, v.y);
}

#pragma mark - Vec4

/**
 * @return A `Vec4` with the specified components.
 */
static inline Vec4 __attribute__ ((warn_unused_result)) MakeVec4(float x, float y, float z, float w) {
  return (Vec4) { .x = x + 0.f, .y = y + 0.f, .z = z + 0.f, .w = w + 0.f};
}

/**
 * @return A `Vec4` from the encoded normalized bytes.
 */
static inline Vec4 __attribute__ ((warn_unused_result)) Vec4bv(const uint32_t xyzw) {

  union {
    struct {
      byte x, y, z, w;
    };
    uint32_t integer;
  } in;

  in.integer = xyzw;

  return MakeVec4(
    ((float) in.x / 255.f) * 2.f - 1.f,
    ((float) in.y / 255.f) * 2.f - 1.f,
    ((float) in.z / 255.f) * 2.f - 1.f,
    ((float) in.w / 255.f) * 2.f - 1.f);
}

/**
 * @return A `Vec4` comprised of the specified `Vec3` and `w`.
 */
static inline Vec4 __attribute__ ((warn_unused_result)) Vec3_ToVec4(const Vec3 v, float w) {
  return MakeVec4(v.x, v.y, v.z, w);
}

/**
 * @return The sub of `a + b`.
 */
static inline Vec4 __attribute__ ((warn_unused_result)) Vec4_Add(const Vec4 a, const Vec4 b) {
  return MakeVec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

/**
 * @return The difference of `a - b`.
 */
static inline Vec4 __attribute__ ((warn_unused_result)) Vec4_Subtract(const Vec4 a, const Vec4 b) {
  return MakeVec4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

/**
 * @return The sub of `a + b`.
 */
static inline Vec4 __attribute__ ((warn_unused_result)) Vec4_Multiply(const Vec4 a, const Vec4 b) {
  return MakeVec4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}

/**
 * @return The negated vector `v`.
 */
static inline Vec4 __attribute__ ((warn_unused_result)) Vec4_Negate(const Vec4 v) {
  return MakeVec4(-v.x, -v.y, -v.z, -v.w);
}

/**
 * @return True if `a` and `b` are equal using the specified epsilon.
 */
static inline bool __attribute__ ((warn_unused_result)) Vec4_EqualEpsilon(const Vec4 a, const Vec4 b, float epsilon) {
  return fabsf(a.x - b.x) <= epsilon &&
       fabsf(a.y - b.y) <= epsilon &&
       fabsf(a.z - b.z) <= epsilon &&
       fabsf(a.w - b.w) <= epsilon;
}

/**
 * @return True if `a` and `b` are equal.
 */
static inline bool __attribute__ ((warn_unused_result)) Vec4_Equal(const Vec4 a, const Vec4 b) {
  return Vec4_EqualEpsilon(a, b, __FLT_EPSILON__);
}

/**
 * @return The vector `v` scaled by `scale`.
 */
static inline Vec4 __attribute__ ((warn_unused_result)) Vec4_Scale(const Vec4 v, float scale) {
  return MakeVec4(v.x * scale, v.y * scale, v.z * scale, v.w * scale);
}

/**
 * @return The vector `v` + (`add` * `multiply`).
 */
static inline Vec4 __attribute__ ((warn_unused_result)) Vec4_Fmaf(const Vec4 v, float multiply, const Vec4 add) {
  return MakeVec4(fmaf(add.x, multiply, v.x), fmaf(add.y, multiply, v.y), fmaf(add.z, multiply, v.z), fmaf(add.w, multiply, v.w));
}

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
static inline Vec4 __attribute__ ((warn_unused_result)) Vec4_Mix(const Vec4 a, const Vec4 b, float mix) {
  return Vec4_Add(Vec4_Scale(a, 1.f - mix), Vec4_Scale(b, mix));
}

/**
 * @return The vector `(1.f, 1f., 1.f)`.
 */
static inline Vec4 __attribute__ ((warn_unused_result)) Vec4_One(void) {
  return MakeVec4(1.f, 1.f, 1.f, 1.f);
}

/**
 * @return The vector `a` raised tht exponent `exp`.
 */
static inline Vec4 __attribute__ ((warn_unused_result)) Vec4_Pow(const Vec4 a, float exp) {
  return MakeVec4(powf(a.x, exp), powf(a.y, exp), powf(a.z, exp), powf(a.w, exp));
}

/**
 * @return The vector `a` raised tht exponent `exp`.
 */
static inline Vec4 __attribute__ ((warn_unused_result)) Vec4_Pow3(const Vec4 a, const Vec3 exp) {
  return MakeVec4(powf(a.x, exp.x), powf(a.y, exp.y), powf(a.z, exp.z), a.w);
}

/**
 * @return A vector with random values between `begin` and `end`.
 */
static inline Vec4 __attribute__ ((warn_unused_result)) Vec4_RandomRange(float begin, float end) {
  return MakeVec4(RandomRangef(begin, end),
        RandomRangef(begin, end),
        RandomRangef(begin, end),
        RandomRangef(begin, end));
}

/**
 * @return A vevtor with random values between `0` and `1`.
 */
static inline Vec4 __attribute__ ((warn_unused_result)) Vec4_Random(void) {
  return Vec4_RandomRange(0.f, 1.f);
}

/**
 * @return A byte encoded representation of the normalized vector `v`.
 * @details Floating point -1.0 to 1.0 are packed to bytes, where -1.0 -> 0 and 1.0 -> 255.
 */
static inline uint32_t __attribute__ ((warn_unused_result)) Vec4_Bytes(const Vec4 v) {

  union {
    struct {
      byte x, y, z, w;
    };
    uint32_t integer;
  } out;

  out.x = (byte) Clampf((v.x + 1.f) * 0.5f * 255.f, 0.f, 255.f);
  out.y = (byte) Clampf((v.y + 1.f) * 0.5f * 255.f, 0.f, 255.f);
  out.z = (byte) Clampf((v.z + 1.f) * 0.5f * 255.f, 0.f, 255.f);
  out.w = (byte) Clampf((v.w + 1.f) * 0.5f * 255.f, 0.f, 255.f);

  return out.integer;
}

/**
 * @return A byte encoded representation of the normalized vector `v`.
 */
static inline int32_t __attribute__ ((warn_unused_result)) Vec3_Bytes(const Vec3 v) {
  return Vec4_Bytes(Vec3_ToVec4(v, 1.f));
}

/**
 * @return The vector `(0.f, 0.f, 0.f, 0.f)`.
 */
static inline Vec4 __attribute__ ((warn_unused_result)) Vec4_Zero(void) {
  return MakeVec4(0.f, 0.f, 0.f, 0.f);
}

#pragma mark - double precision

/**
 * @return True if `fabs(a - b) <= epsilon`.
 */
static inline bool __attribute__ ((warn_unused_result)) EqualEpsilon(double a, double b, double epsilon) {
  return fabs(a - b) <= epsilon;
}

#pragma mark - Vec3d

/**
 * @return A `Vec3d` with the specified components.
 */
static inline Vec3d __attribute__ ((warn_unused_result)) MakeVec3d(double x, double y, double z) {
  return (Vec3d) {
    .x = x + 0.0,
    .y = y + 0.0,
    .z = z + 0.0
  };
}

/**
 * @return The vector `v` cast to single precision.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Vec3d_CastVec3(const Vec3d v) {
  return MakeVec3(v.x, v.y, v.z);
}

/**
 * @return The vector sum of `a + b`.
 */
static inline Vec3d __attribute__ ((warn_unused_result)) Vec3d_Add(const Vec3d a, const Vec3d b) {
  return MakeVec3d(a.x + b.x, a.y + b.y, a.z + b.z);
}

/**
 * @return The cross product of `a ✕ b`.
 */
static inline Vec3d __attribute__ ((warn_unused_result)) Vec3d_Cross(const Vec3d a, const Vec3d b) {
  return MakeVec3d(a.y * b.z - a.z * b.y,
         a.z * b.x - a.x * b.z,
         a.x * b.y - a.y * b.x);
}

/**
 * @return The difference of `a - b`.
 */
static inline Vec3d __attribute__ ((warn_unused_result)) Vec3d_Subtract(const Vec3d a, const Vec3d b) {
  return MakeVec3d(a.x - b.x, a.y - b.y, a.z - b.z);
}

/**
 * @return The dot product of `a · b`.
 */
static inline double __attribute__ ((warn_unused_result)) Vec3d_Dot(const Vec3d a, const Vec3d b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

/**
 * @return The squared length (magnitude) of `v`.
 */
static inline double __attribute__ ((warn_unused_result)) Vec3d_LengthSquared(const Vec3d v) {
  return Vec3d_Dot(v, v);
}

/**
 * @return The length (magnitude) of `v`.
 */
static inline double __attribute__ ((warn_unused_result)) Vec3d_Length(const Vec3d v) {
  return sqrt(Vec3d_LengthSquared(v));
}

/**
 * @return The squared length of the vector `a - b`.
 */
static inline double __attribute__ ((warn_unused_result)) Vec3d_DistanceSquared(const Vec3d a, const Vec3d b) {
  return Vec3d_LengthSquared(Vec3d_Subtract(a, b));
}

/**
 * @return The length of the vector `a - b`.
 */
static inline double __attribute__ ((warn_unused_result)) Vec3d_Distance(const Vec3d a, const Vec3d b) {
  return Vec3d_Length(Vec3d_Subtract(a, b));
}

/**
 * @return True if `a` and `b` are equal using the specified epsilon.
 */
static inline bool __attribute__ ((warn_unused_result)) Vec3d_EqualEpsilon(const Vec3d a, const Vec3d b, double epsilon) {
  return EqualEpsilon(a.x, b.x, epsilon) &&
       EqualEpsilon(a.y, b.y, epsilon) &&
       EqualEpsilon(a.z, b.z, epsilon);
}

/**
 * @return True if `a` and `b` are equal.
 */
static inline bool __attribute__ ((warn_unused_result)) Vec3d_Equal(const Vec3d a, const Vec3d b) {
  return Vec3d_EqualEpsilon(a, b, __DBL_EPSILON__);
}

/**
 * @return The vector `v` + (`add` * `multiply`).
 */
static inline Vec3d __attribute__ ((warn_unused_result)) Vec3d_Fma(const Vec3d v, double multiply, const Vec3d add) {
  return MakeVec3d(fma(add.x, multiply, v.x), fma(add.y, multiply, v.y), fma(add.z, multiply, v.z));
}

/**
 * @return The vector `v` scaled by `scale`.
 */
static inline Vec3d __attribute__ ((warn_unused_result)) Vec3d_Scale(const Vec3d v, double scale) {
  return MakeVec3d(v.x * scale, v.y * scale, v.z * scale);
}

/**
 * @return The vector `(0., 0., 0.)`.
 */
static inline Vec3d __attribute__ ((warn_unused_result)) Vec3d_Zero(void) {
  return MakeVec3d(0., 0., 0.);
}

/**
 * @return The normalized vector `v`.
 */
static inline Vec3d __attribute__ ((warn_unused_result)) Vec3d_Normalize(const Vec3d v) {
  const double length = Vec3d_Length(v);
  if (length) {
    return Vec3d_Scale(v, 1.0 / length);
  } else {
    return Vec3d_Zero();
  }
}
