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
	vec4_t rows[4];
} mat4_t;

/**
 * @return A `mat4_t` with the specified components.
*/
static inline mat4_t __attribute__ ((warn_unused_result)) Mat4(const float elements[16]) {
	mat4_t matrix;
	memcpy(matrix.array, elements, sizeof(matrix.array));
	return matrix;
}

/**
 * @return A `mat4_t` with the specified rows.
*/
static inline mat4_t __attribute__ ((warn_unused_result)) Mat4_FromRows(const vec4_t row0, const vec4_t row1, const vec4_t row2, const vec4_t row3) {
	return (mat4_t) {
		.rows = { row0, row1, row2, row3 }
	};
}

/**
 * @return A `mat4_t` with the specified columns.
*/
static inline mat4_t __attribute__ ((warn_unused_result)) Mat4_FromColumns(const vec4_t col0, const vec4_t col1, const vec4_t col2, const vec4_t col3) {
	return (mat4_t) { .rows = {
		Vec4(col0.x, col1.x, col2.x, col3.x),
		Vec4(col0.y, col1.y, col2.y, col3.y),
		Vec4(col0.z, col1.z, col2.z, col3.z),
		Vec4(col0.w, col1.w, col2.w, col3.w)
	} };
}

/**
 * @return The identity matrix `(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)`.
 */
static inline mat4_t __attribute__ ((warn_unused_result)) Mat4_Identity(void) {
	return Mat4((const float []) {
		1.f, 0.f, 0.f, 0.f,
		0.f, 1.f, 0.f, 0.f,
		0.f, 0.f, 1.f, 0.f,
		0.f, 0.f, 0.f, 1.f
	});
}

/** 
 * @return The product of `a` and `b`'s matrix concatenation
 */
static inline mat4_t __attribute__ ((warn_unused_result)) Mat4_Concat(const mat4_t a, const mat4_t b) {
	return Mat4_FromColumns(Vec4(
		a.m[0][0] * b.m[0][0] + a.m[1][0] * b.m[0][1] + a.m[2][0] * b.m[0][2] + a.m[3][0] * b.m[0][3],
		a.m[0][0] * b.m[1][0] + a.m[1][0] * b.m[1][1] + a.m[2][0] * b.m[1][2] + a.m[3][0] * b.m[1][3],
		a.m[0][0] * b.m[2][0] + a.m[1][0] * b.m[2][1] + a.m[2][0] * b.m[2][2] + a.m[3][0] * b.m[2][3],
		a.m[0][0] * b.m[3][0] + a.m[1][0] * b.m[3][1] + a.m[2][0] * b.m[3][2] + a.m[3][0] * b.m[3][3]
	), Vec4(
		a.m[0][1] * b.m[0][0] + a.m[1][1] * b.m[0][1] + a.m[2][1] * b.m[0][2] + a.m[3][1] * b.m[0][3],
		a.m[0][1] * b.m[1][0] + a.m[1][1] * b.m[1][1] + a.m[2][1] * b.m[1][2] + a.m[3][1] * b.m[1][3],
		a.m[0][1] * b.m[2][0] + a.m[1][1] * b.m[2][1] + a.m[2][1] * b.m[2][2] + a.m[3][1] * b.m[2][3],
		a.m[0][1] * b.m[3][0] + a.m[1][1] * b.m[3][1] + a.m[2][1] * b.m[3][2] + a.m[3][1] * b.m[3][3]
	), Vec4(
		a.m[0][2] * b.m[0][0] + a.m[1][2] * b.m[0][1] + a.m[2][2] * b.m[0][2] + a.m[3][2] * b.m[0][3],
		a.m[0][2] * b.m[1][0] + a.m[1][2] * b.m[1][1] + a.m[2][2] * b.m[1][2] + a.m[3][2] * b.m[1][3],
		a.m[0][2] * b.m[2][0] + a.m[1][2] * b.m[2][1] + a.m[2][2] * b.m[2][2] + a.m[3][2] * b.m[2][3],
		a.m[0][2] * b.m[3][0] + a.m[1][2] * b.m[3][1] + a.m[2][2] * b.m[3][2] + a.m[3][2] * b.m[3][3]
	), Vec4(
		a.m[0][3] * b.m[0][0] + a.m[1][3] * b.m[0][1] + a.m[2][3] * b.m[0][2] + a.m[3][3] * b.m[0][3],
		a.m[0][3] * b.m[1][0] + a.m[1][3] * b.m[1][1] + a.m[2][3] * b.m[1][2] + a.m[3][3] * b.m[1][3],
		a.m[0][3] * b.m[2][0] + a.m[1][3] * b.m[2][1] + a.m[2][3] * b.m[2][2] + a.m[3][3] * b.m[2][3],
		a.m[0][3] * b.m[3][0] + a.m[1][3] * b.m[3][1] + a.m[2][3] * b.m[3][2] + a.m[3][3] * b.m[3][3]
	));
}

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
static inline mat4_t __attribute__ ((warn_unused_result)) Mat4_Mix(const mat4_t a, const mat4_t b, float mix) {
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
static inline mat4_t __attribute__ ((warn_unused_result)) Mat4_FromFrustum(const float left, const float right, const float bottom, const float top, const float nearval, const float farval) {
	const float rl = 1.f / (right - left);
	const float tb = 1.f / (top - bottom);
	const float nf = 1.f / (nearval - farval);
	const float n2 = (nearval * 2.f);

	return Mat4((const float[]) {
		n2 * rl,				0.f,					0.f,						0.f,
		0.f,					n2 * tb,				0.f,						0.f,
		(right + left) * rl,	(top + bottom) * tb,	(farval + nearval) * nf,	-1.f,
		0.f,					0.f,					(farval * n2) * nf,			0.f
	});
}

/**
 * @return An orthogonal matrix with the specified parameters.
 */
static inline mat4_t __attribute__ ((warn_unused_result)) Mat4_FromOrtho(const float left, const float right, const float bottom, const float top, const float nearval, const float farval) {
	const float lr = 1.f / (left - right);
	const float bt = 1.f / (bottom - top);
	const float nf = 1.f / (nearval - farval);

	return Mat4((const float[]) {
		-2.f * lr,				0.f,					0.f,						0.f,
		0.f,					-2.f * bt,				0.f,						0.f,
		0.f,					0.f,					2.f * nf,					0.f,
		(left + right) * lr,	(top + bottom) * bt,	(farval + nearval) * nf,	1.f
	});
}

/**
 * @return A matrix constructed from the specified routines. Quicker than doing Rotate3 + Translate + Scale separately.
 */
static inline mat4_t __attribute__ ((warn_unused_result)) Mat4_FromRotationTranslationScale(const vec3_t angles, const vec3_t origin, const float scale) {

	if (Vec3_Equal(angles, Vec3_Zero())) {

		return Mat4_FromColumns(
			Vec4(scale, 0.f,   0.f,   origin.x ),
			Vec4(0.f,   scale, 0.f,   origin.y ),
			Vec4(0.f,   0.f,   scale, origin.z ),
			Vec4(0.f,   0.f,   0.f,   1.f )
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

		return Mat4_FromColumns(Vec4( 
				(cp * cy) * scale,
				(sr * sp * cy + cr * -sy) * scale,
				(cr * sp * cy + -sr * -sy) * scale,
				origin.x
			), Vec4(
				(cp * sy) * scale,
				(sr * sp * sy + cr * cy) * scale,
				(cr * sp * sy + -sr * cy) * scale,
				origin.y
			), Vec4(
				(-sp) * scale,
				(sr * cp) * scale,
				(cr * cp) * scale,
				origin.z
			), Vec4(
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

		return Mat4_FromColumns(Vec4(
				(cp * cy) * scale,
				(-sy) * scale,
				(sp * cy) * scale,
				origin.x
			), Vec4(
				(cp * sy) * scale,
				(cy) * scale,
				(sp * sy) * scale,
				origin.y
			), Vec4(
				(-sp) * scale,
				0.f,
				(cp) * scale,
				origin.z
			), Vec4(
				0.f,
				0.f,
				0.f,
				1.f
			));
	} else {
		const float angle = angles.y * (M_PI * 2 / 360);
		const float sy = sinf(angle);
		const float cy = cosf(angle);

		return Mat4_FromColumns(Vec4(
				(cy) * scale,
				(-sy) * scale,
				0.f,
				origin.x
			), Vec4(
				(sy) * scale,
				(cy) * scale,
				0.f,
				origin.y
			), Vec4(
				0.f,
				0.f,
				scale,
				origin.z
			), Vec4(
				0.f,
				0.f,
				0.f,
				1.f
			));
	}
}

/**
 * @return The inverse of the input matrix.
 */
static inline mat4_t __attribute__ ((warn_unused_result)) Mat4_Inverse(const mat4_t a) {
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

	return Mat4((const float[]) {
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
static inline mat4_t __attribute__ ((warn_unused_result)) Mat4_FromTranslation(const vec3_t translate) {
	return Mat4_FromColumns(
		Vec4(1.f,  0.f,  0.f,  translate.x),
		Vec4(0.0f, 1.0f, 0.0f, translate.y),
		Vec4(0.0f, 0.0f, 1.0f, translate.z),
		Vec4(0.0f, 0.0f, 0.0f, 1.0f)
	);
}

/**
 * @return A rotation matrix.
 */
static inline mat4_t __attribute__ ((warn_unused_result)) Mat4_FromRotation(const float degrees, const vec3_t axis) {
	const float radians = -Radians(degrees);
	const float c = cosf(radians);
	const float s = sinf(radians);
	
	return Mat4_FromColumns(Vec4(
		axis.x * axis.x + c * (1.f - axis.x * axis.x),
		axis.x * axis.y * (1.f - c) + axis.z * s,
		axis.z * axis.x * (1.f - c) - axis.y * s,
		0.f
	), Vec4(
		axis.x * axis.y * (1.f - c) - axis.z * s,
		axis.y * axis.y + c * (1.f - axis.y * axis.y),
		axis.y * axis.z * (1.f - c) + axis.x * s,
		0.f
	), Vec4(
		axis.z * axis.x * (1.f - c) + axis.y * s,
		axis.y * axis.z * (1.f - c) - axis.x * s,
		axis.z * axis.z + c * (1.f - axis.z * axis.z),
		0.f
	), Vec4(
		0.f,
		0.f,
		0.f,
		1.f
	));
}

/**
 * @return A scale matrix.
 */
static inline mat4_t __attribute__ ((warn_unused_result)) Mat4_FromScale3(const vec3_t scale) {
	return Mat4((const float []) {
		scale.x, 0.f,     0.f,     0.f,
		0.f,     scale.y, 0.f,     0.f,
		0.f,     0.f,     scale.z, 0.f,
		0.f,     0.f,     0.f,     1.f
	});
}

/**
 * @return A scale matrix.
 */
static inline mat4_t __attribute__ ((warn_unused_result)) Mat4_FromScale(const float scale) {
	return Mat4_FromScale3(Vec3(scale, scale, scale));
}

/**
 * @brief Fetch the three directional vectors and/or translation from this matrix.
 */
static inline void Mat4_Vectors(const mat4_t in, vec3_t *forward, vec3_t *right, vec3_t *up, vec3_t *translation) {

	if (forward) {
		*forward = Vec3(in.m[0][0], in.m[0][1], in.m[0][2]);
	}

	if (right) {
		*right = Vec3(in.m[1][0], in.m[1][1], in.m[1][2]);
	}

	if (up) {
		*up = Vec3(in.m[2][0], in.m[2][1], in.m[2][2]);
	}

	if (translation) {
		*translation = Vec3(in.m[3][0], in.m[3][1], in.m[3][2]);
	}
}

/**
 * @return A matrix defined by the three directional vectors and translation vectors.
 */
static inline mat4_t __attribute__ ((warn_unused_result)) Mat4_FromVectors(const vec3_t forward, const vec3_t right, const vec3_t up, const vec3_t translation) {
	return Mat4_FromColumns(
		Vec4(forward.x, right.x, up.x, translation.x),
		Vec4(forward.y, right.y, up.y, translation.y),
		Vec4(forward.z, right.z, up.z, translation.z),
		Vec4(0.f,       0.f,     0.f,  1.f)
	);
}

/**
 * @return The input vector transformed by the specified matrix.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Mat4_Transform(const mat4_t m, const vec3_t v) {
	return Vec3(
		v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0],
		v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1],
		v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2]
	);
}

/**
 * @return The transformed positive distance plane (A*x+B*y+C*z-D=0).
 */
static inline vec4_t __attribute__ ((warn_unused_result)) Mat4_TransformPlane(const mat4_t in, const vec3_t n, float d) {
	const float scale = sqrtf(in.m[0][0] * in.m[0][0] + in.m[0][1] * in.m[0][1] + in.m[0][2] * in.m[0][2]);
	const float iscale = 1.f / scale;
	const float x = (n.x * in.m[0][0] + n.y * in.m[1][0] + n.z * in.m[2][0]) * iscale;
	const float y = (n.x * in.m[0][1] + n.y * in.m[1][1] + n.z * in.m[2][1]) * iscale;
	const float z = (n.x * in.m[0][2] + n.y * in.m[1][2] + n.z * in.m[2][2]) * iscale;
	
	return Vec4(x, y, z, d * scale + (x * in.m[3][0] + y * in.m[3][1] + z * in.m[3][2]));
}

/**
 * @return The scaling factor of the supplied matrix.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Mat4_ToScale3(const mat4_t m) {
	return Vec3(
		Vec3_Length(Vec3(m.m[1][1], m.m[1][2], m.m[1][3])),
		Vec3_Length(Vec3(m.m[2][1], m.m[2][2], m.m[2][3])),
		Vec3_Length(Vec3(m.m[3][1], m.m[3][2], m.m[3][3]))
	);
}

/**
 * @return The (fast, uniform-scaling only) scale factor of the supplied matrix.
 */
static inline float __attribute__ ((warn_unused_result)) Mat4_ToScale(const mat4_t m) {
	return Vec3_Length(Vec3(m.m[1][1], m.m[1][2], m.m[1][3]));
}

/**
 * @return The result of the input matrix concatenated with a translation matrix.
 */
static inline mat4_t __attribute__ ((warn_unused_result)) Mat4_ConcatTranslation(const mat4_t in, const vec3_t v) {
	return Mat4_Concat(in, Mat4_FromTranslation(v));
}

/**
 * @return The result of the input matrix concatenated with a rotation matrix.
 */
static inline mat4_t __attribute__ ((warn_unused_result)) Mat4_ConcatRotation(const mat4_t in, const float degrees, const vec3_t axis) {
	return Mat4_Concat(in, Mat4_FromRotation(degrees, axis));
}

/**
 * @return The result of the input matrix concatenated with a 3d rotation matrix.
 */
static inline mat4_t __attribute__ ((warn_unused_result)) Mat4_ConcatRotation3(mat4_t in, const vec3_t euler_angles) {
	in = Mat4_ConcatRotation(in, euler_angles.x, Vec3(1.f, 0.f, 0.f));
	in = Mat4_ConcatRotation(in, euler_angles.y, Vec3(0.f, 1.f, 0.f));
	in = Mat4_ConcatRotation(in, euler_angles.z, Vec3(0.f, 0.f, 1.f));
	return in;
}

/**
 * @return The result of the input matrix concatenated with a scale matrix.
 */
static inline mat4_t __attribute__ ((warn_unused_result)) Mat4_ConcatScale(const mat4_t in, const float scale) {
	return Mat4_Concat(in, Mat4_FromScale(scale));
}

/**
 * @return The result of the input matrix concatenated with a 3d scale matrix.
 */
static inline mat4_t __attribute__ ((warn_unused_result)) Mat4_ConcatScale3(const mat4_t in, const vec3_t scale) {
	return Mat4_Concat(in, Mat4_FromScale3(scale));
}
