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

#include "box.h"
#include "vector.h"

/**
 * @brief Sixteen-component single precision 4x4 matrix type.
 */
typedef union {

  /**
   * @brief Flat array accessor.
   */
  float array[16];

  /**
   * @brief Row/Col component accessor.
   */
  float m[4][4];

  /**
   * @brief Row accessors.
   */
  Vec4 rows[4];
} Mat4;

/**
 * @return A `Mat4` with the specified components.
*/
static inline Mat4 __attribute__ ((warn_unused_result)) MakeMat4(const float elements[16]) {
  Mat4 matrix;
  memcpy(matrix.array, elements, sizeof(matrix.array));
  return matrix;
}

/**
 * @return A `Mat4` with the specified rows.
*/
static inline Mat4 __attribute__ ((warn_unused_result)) Mat4_FromRows(const Vec4 row0, const Vec4 row1, const Vec4 row2, const Vec4 row3) {
  return (Mat4) {
    .rows = { row0, row1, row2, row3 }
  };
}

/**
 * @return A `Mat4` with the specified columns.
*/
static inline Mat4 __attribute__ ((warn_unused_result)) Mat4_FromColumns(const Vec4 col0, const Vec4 col1, const Vec4 col2, const Vec4 col3) {
  return (Mat4) { .rows = {
    MakeVec4(col0.x, col1.x, col2.x, col3.x),
    MakeVec4(col0.y, col1.y, col2.y, col3.y),
    MakeVec4(col0.z, col1.z, col2.z, col3.z),
    MakeVec4(col0.w, col1.w, col2.w, col3.w)
  } };
}

/**
 * @return The identity matrix `(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)`.
 */
static inline Mat4 __attribute__ ((warn_unused_result)) Mat4_Identity(void) {
  return MakeMat4((const float []) {
    1.f, 0.f, 0.f, 0.f,
    0.f, 1.f, 0.f, 0.f,
    0.f, 0.f, 1.f, 0.f,
    0.f, 0.f, 0.f, 1.f
  });
}

/**
 * @return The identity matrix `(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)`.
 */
static inline bool __attribute__ ((warn_unused_result)) Mat4_Equal(const Mat4 a, const Mat4 b) {
  return Vec4_Equal(a.rows[0], b.rows[0]) &&
    Vec4_Equal(a.rows[1], b.rows[1]) &&
    Vec4_Equal(a.rows[2], b.rows[2]) &&
    Vec4_Equal(a.rows[3], b.rows[3]);
}

/** 
 * @return The product of `a` and `b`'s matrix concatenation
 */
static inline Mat4 __attribute__ ((warn_unused_result)) Mat4_Concat(const Mat4 a, const Mat4 b) {
  return Mat4_FromColumns(MakeVec4(
    a.m[0][0] * b.m[0][0] + a.m[1][0] * b.m[0][1] + a.m[2][0] * b.m[0][2] + a.m[3][0] * b.m[0][3],
    a.m[0][0] * b.m[1][0] + a.m[1][0] * b.m[1][1] + a.m[2][0] * b.m[1][2] + a.m[3][0] * b.m[1][3],
    a.m[0][0] * b.m[2][0] + a.m[1][0] * b.m[2][1] + a.m[2][0] * b.m[2][2] + a.m[3][0] * b.m[2][3],
    a.m[0][0] * b.m[3][0] + a.m[1][0] * b.m[3][1] + a.m[2][0] * b.m[3][2] + a.m[3][0] * b.m[3][3]
  ), MakeVec4(
    a.m[0][1] * b.m[0][0] + a.m[1][1] * b.m[0][1] + a.m[2][1] * b.m[0][2] + a.m[3][1] * b.m[0][3],
    a.m[0][1] * b.m[1][0] + a.m[1][1] * b.m[1][1] + a.m[2][1] * b.m[1][2] + a.m[3][1] * b.m[1][3],
    a.m[0][1] * b.m[2][0] + a.m[1][1] * b.m[2][1] + a.m[2][1] * b.m[2][2] + a.m[3][1] * b.m[2][3],
    a.m[0][1] * b.m[3][0] + a.m[1][1] * b.m[3][1] + a.m[2][1] * b.m[3][2] + a.m[3][1] * b.m[3][3]
  ), MakeVec4(
    a.m[0][2] * b.m[0][0] + a.m[1][2] * b.m[0][1] + a.m[2][2] * b.m[0][2] + a.m[3][2] * b.m[0][3],
    a.m[0][2] * b.m[1][0] + a.m[1][2] * b.m[1][1] + a.m[2][2] * b.m[1][2] + a.m[3][2] * b.m[1][3],
    a.m[0][2] * b.m[2][0] + a.m[1][2] * b.m[2][1] + a.m[2][2] * b.m[2][2] + a.m[3][2] * b.m[2][3],
    a.m[0][2] * b.m[3][0] + a.m[1][2] * b.m[3][1] + a.m[2][2] * b.m[3][2] + a.m[3][2] * b.m[3][3]
  ), MakeVec4(
    a.m[0][3] * b.m[0][0] + a.m[1][3] * b.m[0][1] + a.m[2][3] * b.m[0][2] + a.m[3][3] * b.m[0][3],
    a.m[0][3] * b.m[1][0] + a.m[1][3] * b.m[1][1] + a.m[2][3] * b.m[1][2] + a.m[3][3] * b.m[1][3],
    a.m[0][3] * b.m[2][0] + a.m[1][3] * b.m[2][1] + a.m[2][3] * b.m[2][2] + a.m[3][3] * b.m[2][3],
    a.m[0][3] * b.m[3][0] + a.m[1][3] * b.m[3][1] + a.m[2][3] * b.m[3][2] + a.m[3][3] * b.m[3][3]
  ));
}

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
static inline Mat4 __attribute__ ((warn_unused_result)) Mat4_Mix(const Mat4 a, const Mat4 b, float mix) {
  return Mat4_FromRows(
    Vec4_Mix(a.rows[0], b.rows[0], mix),
    Vec4_Mix(a.rows[1], b.rows[1], mix),
    Vec4_Mix(a.rows[2], b.rows[2], mix),
    Vec4_Mix(a.rows[3], b.rows[3], mix)
  );
}

/**
 * @return A perspective matrix with the specified parameters.
 */
static inline Mat4 __attribute__ ((warn_unused_result)) Mat4_FromFrustum(const float left, const float right, const float bottom, const float top, const float nearval, const float farval) {
  const float rl = 1.f / (right - left);
  const float tb = 1.f / (top - bottom);
  const float nf = 1.f / (nearval - farval);
  const float n2 = (nearval * 2.f);

  return MakeMat4((const float[]) {
    n2 * rl,        0.f,          0.f,            0.f,
    0.f,          n2 * tb,        0.f,            0.f,
    (right + left) * rl,  (top + bottom) * tb,  (farval + nearval) * nf,  -1.f,
    0.f,          0.f,          (farval * n2) * nf,      0.f
  });
}

/**
 * @return An orthogonal matrix with the specified parameters.
 */
static inline Mat4 __attribute__ ((warn_unused_result)) Mat4_FromOrtho(const float left, const float right, const float bottom, const float top, const float nearval, const float farval) {
  const float lr = 1.f / (left - right);
  const float bt = 1.f / (bottom - top);
  const float nf = 1.f / (nearval - farval);

  return MakeMat4((const float[]) {
    -2.f * lr,        0.f,          0.f,            0.f,
    0.f,          -2.f * bt,        0.f,            0.f,
    0.f,          0.f,          2.f * nf,          0.f,
    (left + right) * lr,  (top + bottom) * bt,  (farval + nearval) * nf,  1.f
  });
}

/**
 * @return A matrix constructed from the specified routines. Quicker than doing Rotate3 + Translate + Scale separately.
 */
static inline Mat4 __attribute__ ((warn_unused_result)) Mat4_FromRotationTranslationScale(const Vec3 angles, const Vec3 origin, const float scale) {

  if (Vec3_Equal(angles, Vec3_Zero())) {

    return Mat4_FromColumns(
      MakeVec4(scale, 0.f,   0.f,   origin.x ),
      MakeVec4(0.f,   scale, 0.f,   origin.y ),
      MakeVec4(0.f,   0.f,   scale, origin.z ),
      MakeVec4(0.f,   0.f,   0.f,   1.f )
    );
  } else if (angles.z) {
    float angle = angles.y * (M_PI * 2 / 360);
    const float sy = sinf(angle);
    const float cy = cosf(angle);
    angle = angles.x * (M_PI * 2 / 360);
    const float sp = sinf(angle);
    const float cp = cosf(angle);
    angle = angles.z * (M_PI * 2 / 360);
    const float sr = sinf(angle);
    const float cr = cosf(angle);

    return Mat4_FromColumns(MakeVec4( 
        (cp * cy) * scale,
        (sr * sp * cy + cr * -sy) * scale,
        (cr * sp * cy + -sr * -sy) * scale,
        origin.x
      ), MakeVec4(
        (cp * sy) * scale,
        (sr * sp * sy + cr * cy) * scale,
        (cr * sp * sy + -sr * cy) * scale,
        origin.y
      ), MakeVec4(
        (-sp) * scale,
        (sr * cp) * scale,
        (cr * cp) * scale,
        origin.z
      ), MakeVec4(
        0.f,
        0.f,
        0.f,
        1.f
      ));
  } else if (angles.x) {
    float angle = angles.y * (M_PI * 2 / 360);
    const float sy = sinf(angle);
    const float cy = cosf(angle);
    angle = angles.x * (M_PI * 2 / 360);
    const float sp = sinf(angle);
    const float cp = cosf(angle);

    return Mat4_FromColumns(MakeVec4(
        (cp * cy) * scale,
        (-sy) * scale,
        (sp * cy) * scale,
        origin.x
      ), MakeVec4(
        (cp * sy) * scale,
        (cy) * scale,
        (sp * sy) * scale,
        origin.y
      ), MakeVec4(
        (-sp) * scale,
        0.f,
        (cp) * scale,
        origin.z
      ), MakeVec4(
        0.f,
        0.f,
        0.f,
        1.f
      ));
  } else {
    const float angle = angles.y * (M_PI * 2 / 360);
    const float sy = sinf(angle);
    const float cy = cosf(angle);

    return Mat4_FromColumns(MakeVec4(
        (cy) * scale,
        (-sy) * scale,
        0.f,
        origin.x
      ), MakeVec4(
        (sy) * scale,
        (cy) * scale,
        0.f,
        origin.y
      ), MakeVec4(
        0.f,
        0.f,
        scale,
        origin.z
      ), MakeVec4(
        0.f,
        0.f,
        0.f,
        1.f
      ));
  }
}

/**
 * @return A view matrix with the specified eye, center and up vector.
 */
static inline Mat4 __attribute__ ((warn_unused_result)) Mat4_LookAt(const Vec3 eye, const Vec3 pos, const Vec3 up) {

  Vec3 z = Vec3_Direction(eye, pos);
  Vec3 x = Vec3_Normalize(Vec3_Cross(up, z));
  Vec3 y = Vec3_Normalize(Vec3_Cross(z, x));

  Mat4 m;
  m.m[0][0] = x.x;
  m.m[1][0] = x.y;
  m.m[2][0] = x.z;
  m.m[3][0] = -Vec3_Dot(x, eye);
  m.m[0][1] = y.x;
  m.m[1][1] = y.y;
  m.m[2][1] = y.z;
  m.m[3][1] = -Vec3_Dot(y, eye);
  m.m[0][2] = z.x;
  m.m[1][2] = z.y;
  m.m[2][2] = z.z;
  m.m[3][2] = -Vec3_Dot(z, eye);
  m.m[0][3] = 0.f;
  m.m[1][3] = 0.f;
  m.m[2][3] = 0.f;
  m.m[3][3] = 1.f;

  return m;
}

/**
 * @return The inverse of the input matrix.
 */
static inline Mat4 __attribute__ ((warn_unused_result)) Mat4_Inverse(const Mat4 a) {
  const float b00 = a.m[0][0] * a.m[1][1] - a.m[0][1] * a.m[1][0];
  const float b01 = a.m[0][0] * a.m[1][2] - a.m[0][2] * a.m[1][0];
  const float b02 = a.m[0][0] * a.m[1][3] - a.m[0][3] * a.m[1][0];
  const float b03 = a.m[0][1] * a.m[1][2] - a.m[0][2] * a.m[1][1];
  const float b04 = a.m[0][1] * a.m[1][3] - a.m[0][3] * a.m[1][1];
  const float b05 = a.m[0][2] * a.m[1][3] - a.m[0][3] * a.m[1][2];
  const float b06 = a.m[2][0] * a.m[3][1] - a.m[2][1] * a.m[3][0];
  const float b07 = a.m[2][0] * a.m[3][2] - a.m[2][2] * a.m[3][0];
  const float b08 = a.m[2][0] * a.m[3][3] - a.m[2][3] * a.m[3][0];
  const float b09 = a.m[2][1] * a.m[3][2] - a.m[2][2] * a.m[3][1];
  const float b10 = a.m[2][1] * a.m[3][3] - a.m[2][3] * a.m[3][1];
  const float b11 = a.m[2][2] * a.m[3][3] - a.m[2][3] * a.m[3][2];

  float det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;

  if (!det) {
    return Mat4_Identity();
  }

  det = 1.f / det;

  return MakeMat4((const float[]) {
    (a.m[1][1] * b11 - a.m[1][2] * b10 + a.m[1][3] * b09) * det,
    (a.m[0][2] * b10 - a.m[0][1] * b11 - a.m[0][3] * b09) * det,
    (a.m[3][1] * b05 - a.m[3][2] * b04 + a.m[3][3] * b03) * det,
    (a.m[2][2] * b04 - a.m[2][1] * b05 - a.m[2][3] * b03) * det,
    (a.m[1][2] * b08 - a.m[1][0] * b11 - a.m[1][3] * b07) * det,
    (a.m[0][0] * b11 - a.m[0][2] * b08 + a.m[0][3] * b07) * det,
    (a.m[3][2] * b02 - a.m[3][0] * b05 - a.m[3][3] * b01) * det,
    (a.m[2][0] * b05 - a.m[2][2] * b02 + a.m[2][3] * b01) * det,
    (a.m[1][0] * b10 - a.m[1][1] * b08 + a.m[1][3] * b06) * det,
    (a.m[0][1] * b08 - a.m[0][0] * b10 - a.m[0][3] * b06) * det,
    (a.m[3][0] * b04 - a.m[3][1] * b02 + a.m[3][3] * b00) * det,
    (a.m[2][1] * b02 - a.m[2][0] * b04 - a.m[2][3] * b00) * det,
    (a.m[1][1] * b07 - a.m[1][0] * b09 - a.m[1][2] * b06) * det,
    (a.m[0][0] * b09 - a.m[0][1] * b07 + a.m[0][2] * b06) * det,
    (a.m[3][1] * b01 - a.m[3][0] * b03 - a.m[3][2] * b00) * det,
    (a.m[2][0] * b03 - a.m[2][1] * b01 + a.m[2][2] * b00) * det
  });
}

/**
 * @return A translation matrix.
 */
static inline Mat4 __attribute__ ((warn_unused_result)) Mat4_FromTranslation(const Vec3 translate) {
  return Mat4_FromColumns(
    MakeVec4(1.f,  0.f,  0.f,  translate.x),
    MakeVec4(0.0f, 1.0f, 0.0f, translate.y),
    MakeVec4(0.0f, 0.0f, 1.0f, translate.z),
    MakeVec4(0.0f, 0.0f, 0.0f, 1.0f)
  );
}

/**
 * @return A rotation matrix.
 */
static inline Mat4 __attribute__ ((warn_unused_result)) Mat4_FromRotation(const float degrees, const Vec3 axis) {
  const float radians = -Radians(degrees);
  const float c = cosf(radians);
  const float s = sinf(radians);
  
  return Mat4_FromColumns(MakeVec4(
    axis.x * axis.x + c * (1.f - axis.x * axis.x),
    axis.x * axis.y * (1.f - c) + axis.z * s,
    axis.z * axis.x * (1.f - c) - axis.y * s,
    0.f
  ), MakeVec4(
    axis.x * axis.y * (1.f - c) - axis.z * s,
    axis.y * axis.y + c * (1.f - axis.y * axis.y),
    axis.y * axis.z * (1.f - c) + axis.x * s,
    0.f
  ), MakeVec4(
    axis.z * axis.x * (1.f - c) + axis.y * s,
    axis.y * axis.z * (1.f - c) - axis.x * s,
    axis.z * axis.z + c * (1.f - axis.z * axis.z),
    0.f
  ), MakeVec4(
    0.f,
    0.f,
    0.f,
    1.f
  ));
}

/**
 * @return A scale matrix.
 */
static inline Mat4 __attribute__ ((warn_unused_result)) Mat4_FromScale3(const Vec3 scale) {
  return MakeMat4((const float []) {
    scale.x, 0.f,     0.f,     0.f,
    0.f,     scale.y, 0.f,     0.f,
    0.f,     0.f,     scale.z, 0.f,
    0.f,     0.f,     0.f,     1.f
  });
}

/**
 * @return A scale matrix.
 */
static inline Mat4 __attribute__ ((warn_unused_result)) Mat4_FromScale(const float scale) {
  return Mat4_FromScale3(MakeVec3(scale, scale, scale));
}

/**
 * @brief Fetch the three directional vectors and/or translation from this matrix.
 */
static inline void Mat4_Vectors(const Mat4 in, Vec3 *forward, Vec3 *right, Vec3 *up, Vec3 *translation) {

  if (forward) {
    *forward = MakeVec3(in.m[0][0], in.m[0][1], in.m[0][2]);
  }

  if (right) {
    *right = MakeVec3(in.m[1][0], in.m[1][1], in.m[1][2]);
  }

  if (up) {
    *up = MakeVec3(in.m[2][0], in.m[2][1], in.m[2][2]);
  }

  if (translation) {
    *translation = MakeVec3(in.m[3][0], in.m[3][1], in.m[3][2]);
  }
}

/**
 * @return A matrix defined by the three directional vectors and translation vectors.
 */
static inline Mat4 __attribute__ ((warn_unused_result)) Mat4_FromVectors(const Vec3 forward, const Vec3 right, const Vec3 up, const Vec3 translation) {
  return Mat4_FromColumns(
    MakeVec4(forward.x, right.x, up.x, translation.x),
    MakeVec4(forward.y, right.y, up.y, translation.y),
    MakeVec4(forward.z, right.z, up.z, translation.z),
    MakeVec4(0.f,       0.f,     0.f,  1.f)
  );
}

/**
 * @return The input vector transformed by the specified matrix.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Mat4_Transform(const Mat4 m, const Vec3 v) {
  return MakeVec3(
    v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0],
    v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1],
    v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2]
  );
}

/**
 * @return The input direction rotated by the specified matrix, ignoring its translation.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Mat4_RotateVector(const Mat4 m, const Vec3 v) {
  return MakeVec3(
    v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0],
    v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1],
    v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2]
  );
}

/**
 * @return The transformed positive distance plane (A*x+B*y+C*z-D=0).
 */
static inline Vec4 __attribute__ ((warn_unused_result)) Mat4_TransformPlane(const Mat4 in, const Vec3 n, float d) {
  const float scale = sqrtf(in.m[0][0] * in.m[0][0] + in.m[0][1] * in.m[0][1] + in.m[0][2] * in.m[0][2]);
  const float iscale = 1.f / scale;
  const float x = (n.x * in.m[0][0] + n.y * in.m[1][0] + n.z * in.m[2][0]) * iscale;
  const float y = (n.x * in.m[0][1] + n.y * in.m[1][1] + n.z * in.m[2][1]) * iscale;
  const float z = (n.x * in.m[0][2] + n.y * in.m[1][2] + n.z * in.m[2][2]) * iscale;
  
  return MakeVec4(x, y, z, d * scale + (x * in.m[3][0] + y * in.m[3][1] + z * in.m[3][2]));
}

/**
 * @return The scaling factor of the supplied matrix.
 */
static inline Vec3 __attribute__ ((warn_unused_result)) Mat4_ToScale3(const Mat4 m) {
  return MakeVec3(
    Vec3_Length(MakeVec3(m.m[0][0], m.m[1][0], m.m[2][0])),
    Vec3_Length(MakeVec3(m.m[0][1], m.m[1][1], m.m[2][1])),
    Vec3_Length(MakeVec3(m.m[0][2], m.m[1][2], m.m[2][2]))
  );
}

/**
 * @return The (fast, uniform-scaling only) scale factor of the supplied matrix.
 */
static inline float __attribute__ ((warn_unused_result)) Mat4_ToScale(const Mat4 m) {
  return Vec3_Length(MakeVec3(m.m[0][0], m.m[1][0], m.m[2][0]));
}

/**
 * @return The result of the input matrix concatenated with a translation matrix.
 */
static inline Mat4 __attribute__ ((warn_unused_result)) Mat4_ConcatTranslation(const Mat4 in, const Vec3 v) {
  return Mat4_Concat(in, Mat4_FromTranslation(v));
}

/**
 * @return The result of the input matrix concatenated with a rotation matrix.
 */
static inline Mat4 __attribute__ ((warn_unused_result)) Mat4_ConcatRotation(const Mat4 in, const float degrees, const Vec3 axis) {
  return Mat4_Concat(in, Mat4_FromRotation(degrees, axis));
}

/**
 * @return The result of the input matrix concatenated with a 3d rotation matrix.
 */
static inline Mat4 __attribute__ ((warn_unused_result)) Mat4_ConcatRotation3(Mat4 in, const Vec3 eulerAngles) {
  in = Mat4_ConcatRotation(in, eulerAngles.x, MakeVec3(1.f, 0.f, 0.f));
  in = Mat4_ConcatRotation(in, eulerAngles.y, MakeVec3(0.f, 1.f, 0.f));
  in = Mat4_ConcatRotation(in, eulerAngles.z, MakeVec3(0.f, 0.f, 1.f));
  return in;
}

/**
 * @return The result of the input matrix concatenated with a scale matrix.
 */
static inline Mat4 __attribute__ ((warn_unused_result)) Mat4_ConcatScale(const Mat4 in, const float scale) {
  return Mat4_Concat(in, Mat4_FromScale(scale));
}

/**
 * @return The result of the input matrix concatenated with a 3d scale matrix.
 */
static inline Mat4 __attribute__ ((warn_unused_result)) Mat4_ConcatScale3(const Mat4 in, const Vec3 scale) {
  return Mat4_Concat(in, Mat4_FromScale3(scale));
}

/**
 * @return A new bounding box that contains all eight points of the input `bounds`
 * being transformed by `m`.
*/
static inline Box3 Mat4_TransformBounds(const Mat4 m, const Box3 bounds) {
  
  Vec3 points[8];
  Box3_ToPoints(bounds, points);

  Box3 b = Box3_Null();

  for (size_t i = 0; i < lengthof(points); i++) {
    b = Box3_Append(b, Mat4_Transform(m, points[i]));
  }

  return b;
}
