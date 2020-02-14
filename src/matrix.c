/*
 * Copyright(c) 2010 DarkPlaces.
 * Copyright(c) 2010 Quetoo.
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

#include "matrix.h"

const mat4_t matrix4x4_identity = {
	{
		{1, 0, 0, 0},
		{0, 1, 0, 0},
		{0, 0, 1, 0},
		{0, 0, 0, 1}
	}
};

void Matrix4x4_Copy (mat4_t *out, const mat4_t *in) {
	*out = *in;
}

void Matrix4x4_CopyRotateOnly (mat4_t *out, const mat4_t *in) {
	out->m[0][0] = in->m[0][0];
	out->m[0][1] = in->m[0][1];
	out->m[0][2] = in->m[0][2];
	out->m[0][3] = 0.0f;
	out->m[1][0] = in->m[1][0];
	out->m[1][1] = in->m[1][1];
	out->m[1][2] = in->m[1][2];
	out->m[1][3] = 0.0f;
	out->m[2][0] = in->m[2][0];
	out->m[2][1] = in->m[2][1];
	out->m[2][2] = in->m[2][2];
	out->m[2][3] = 0.0f;
	out->m[3][0] = 0.0f;
	out->m[3][1] = 0.0f;
	out->m[3][2] = 0.0f;
	out->m[3][3] = 1.0f;
}

void Matrix4x4_CopyTranslateOnly (mat4_t *out, const mat4_t *in) {
#if MATRIX4x4_OPENGLORIENTATION
	out->m[0][0] = 1.0f;
	out->m[1][0] = 0.0f;
	out->m[2][0] = 0.0f;
	out->m[3][0] = in->m[0][3];
	out->m[0][1] = 0.0f;
	out->m[1][1] = 1.0f;
	out->m[2][1] = 0.0f;
	out->m[3][1] = in->m[1][3];
	out->m[0][2] = 0.0f;
	out->m[1][2] = 0.0f;
	out->m[2][2] = 1.0f;
	out->m[3][2] = in->m[2][3];
	out->m[0][3] = 0.0f;
	out->m[1][3] = 0.0f;
	out->m[2][3] = 0.0f;
	out->m[3][3] = 1.0f;
#else
	out->m[0][0] = 1.0f;
	out->m[0][1] = 0.0f;
	out->m[0][2] = 0.0f;
	out->m[0][3] = in->m[0][3];
	out->m[1][0] = 0.0f;
	out->m[1][1] = 1.0f;
	out->m[1][2] = 0.0f;
	out->m[1][3] = in->m[1][3];
	out->m[2][0] = 0.0f;
	out->m[2][1] = 0.0f;
	out->m[2][2] = 1.0f;
	out->m[2][3] = in->m[2][3];
	out->m[3][0] = 0.0f;
	out->m[3][1] = 0.0f;
	out->m[3][2] = 0.0f;
	out->m[3][3] = 1.0f;
#endif
}

void Matrix4x4_Concat (mat4_t *out, const mat4_t *in1, const mat4_t *in2) {
	// In the case where Concat uses the output as input,
	// handle it with a quick stop'n'swap.
	// Ideally I think a Matrix4x4_Multiply(out, in) would
	// be a better option though, to operate as out *= in.
	if (in1 == out) {
		static mat4_t in1_temp;
		Matrix4x4_Copy(&in1_temp, in1);
		in1 = &in1_temp;
	}
	if (in2 == out) {
		static mat4_t in2_temp;
		Matrix4x4_Copy(&in2_temp, in2);
		in2 = &in2_temp;
	}

#if MATRIX4x4_OPENGLORIENTATION
	out->m[0][0] = in1->m[0][0] * in2->m[0][0] + in1->m[1][0] * in2->m[0][1] + in1->m[2][0] * in2->m[0][2] + in1->m[3][0] *
	               in2->m[0][3];
	out->m[1][0] = in1->m[0][0] * in2->m[1][0] + in1->m[1][0] * in2->m[1][1] + in1->m[2][0] * in2->m[1][2] + in1->m[3][0] *
	               in2->m[1][3];
	out->m[2][0] = in1->m[0][0] * in2->m[2][0] + in1->m[1][0] * in2->m[2][1] + in1->m[2][0] * in2->m[2][2] + in1->m[3][0] *
	               in2->m[2][3];
	out->m[3][0] = in1->m[0][0] * in2->m[3][0] + in1->m[1][0] * in2->m[3][1] + in1->m[2][0] * in2->m[3][2] + in1->m[3][0] *
	               in2->m[3][3];
	out->m[0][1] = in1->m[0][1] * in2->m[0][0] + in1->m[1][1] * in2->m[0][1] + in1->m[2][1] * in2->m[0][2] + in1->m[3][1] *
	               in2->m[0][3];
	out->m[1][1] = in1->m[0][1] * in2->m[1][0] + in1->m[1][1] * in2->m[1][1] + in1->m[2][1] * in2->m[1][2] + in1->m[3][1] *
	               in2->m[1][3];
	out->m[2][1] = in1->m[0][1] * in2->m[2][0] + in1->m[1][1] * in2->m[2][1] + in1->m[2][1] * in2->m[2][2] + in1->m[3][1] *
	               in2->m[2][3];
	out->m[3][1] = in1->m[0][1] * in2->m[3][0] + in1->m[1][1] * in2->m[3][1] + in1->m[2][1] * in2->m[3][2] + in1->m[3][1] *
	               in2->m[3][3];
	out->m[0][2] = in1->m[0][2] * in2->m[0][0] + in1->m[1][2] * in2->m[0][1] + in1->m[2][2] * in2->m[0][2] + in1->m[3][2] *
	               in2->m[0][3];
	out->m[1][2] = in1->m[0][2] * in2->m[1][0] + in1->m[1][2] * in2->m[1][1] + in1->m[2][2] * in2->m[1][2] + in1->m[3][2] *
	               in2->m[1][3];
	out->m[2][2] = in1->m[0][2] * in2->m[2][0] + in1->m[1][2] * in2->m[2][1] + in1->m[2][2] * in2->m[2][2] + in1->m[3][2] *
	               in2->m[2][3];
	out->m[3][2] = in1->m[0][2] * in2->m[3][0] + in1->m[1][2] * in2->m[3][1] + in1->m[2][2] * in2->m[3][2] + in1->m[3][2] *
	               in2->m[3][3];
	out->m[0][3] = in1->m[0][3] * in2->m[0][0] + in1->m[1][3] * in2->m[0][1] + in1->m[2][3] * in2->m[0][2] + in1->m[3][3] *
	               in2->m[0][3];
	out->m[1][3] = in1->m[0][3] * in2->m[1][0] + in1->m[1][3] * in2->m[1][1] + in1->m[2][3] * in2->m[1][2] + in1->m[3][3] *
	               in2->m[1][3];
	out->m[2][3] = in1->m[0][3] * in2->m[2][0] + in1->m[1][3] * in2->m[2][1] + in1->m[2][3] * in2->m[2][2] + in1->m[3][3] *
	               in2->m[2][3];
	out->m[3][3] = in1->m[0][3] * in2->m[3][0] + in1->m[1][3] * in2->m[3][1] + in1->m[2][3] * in2->m[3][2] + in1->m[3][3] *
	               in2->m[3][3];
#else
	out->m[0][0] = in1->m[0][0] * in2->m[0][0] + in1->m[0][1] * in2->m[1][0] + in1->m[0][2] * in2->m[2][0] + in1->m[0][3] *
	               in2->m[3][0];
	out->m[0][1] = in1->m[0][0] * in2->m[0][1] + in1->m[0][1] * in2->m[1][1] + in1->m[0][2] * in2->m[2][1] + in1->m[0][3] *
	               in2->m[3][1];
	out->m[0][2] = in1->m[0][0] * in2->m[0][2] + in1->m[0][1] * in2->m[1][2] + in1->m[0][2] * in2->m[2][2] + in1->m[0][3] *
	               in2->m[3][2];
	out->m[0][3] = in1->m[0][0] * in2->m[0][3] + in1->m[0][1] * in2->m[1][3] + in1->m[0][2] * in2->m[2][3] + in1->m[0][3] *
	               in2->m[3][3];
	out->m[1][0] = in1->m[1][0] * in2->m[0][0] + in1->m[1][1] * in2->m[1][0] + in1->m[1][2] * in2->m[2][0] + in1->m[1][3] *
	               in2->m[3][0];
	out->m[1][1] = in1->m[1][0] * in2->m[0][1] + in1->m[1][1] * in2->m[1][1] + in1->m[1][2] * in2->m[2][1] + in1->m[1][3] *
	               in2->m[3][1];
	out->m[1][2] = in1->m[1][0] * in2->m[0][2] + in1->m[1][1] * in2->m[1][2] + in1->m[1][2] * in2->m[2][2] + in1->m[1][3] *
	               in2->m[3][2];
	out->m[1][3] = in1->m[1][0] * in2->m[0][3] + in1->m[1][1] * in2->m[1][3] + in1->m[1][2] * in2->m[2][3] + in1->m[1][3] *
	               in2->m[3][3];
	out->m[2][0] = in1->m[2][0] * in2->m[0][0] + in1->m[2][1] * in2->m[1][0] + in1->m[2][2] * in2->m[2][0] + in1->m[2][3] *
	               in2->m[3][0];
	out->m[2][1] = in1->m[2][0] * in2->m[0][1] + in1->m[2][1] * in2->m[1][1] + in1->m[2][2] * in2->m[2][1] + in1->m[2][3] *
	               in2->m[3][1];
	out->m[2][2] = in1->m[2][0] * in2->m[0][2] + in1->m[2][1] * in2->m[1][2] + in1->m[2][2] * in2->m[2][2] + in1->m[2][3] *
	               in2->m[3][2];
	out->m[2][3] = in1->m[2][0] * in2->m[0][3] + in1->m[2][1] * in2->m[1][3] + in1->m[2][2] * in2->m[2][3] + in1->m[2][3] *
	               in2->m[3][3];
	out->m[3][0] = in1->m[3][0] * in2->m[0][0] + in1->m[3][1] * in2->m[1][0] + in1->m[3][2] * in2->m[2][0] + in1->m[3][3] *
	               in2->m[3][0];
	out->m[3][1] = in1->m[3][0] * in2->m[0][1] + in1->m[3][1] * in2->m[1][1] + in1->m[3][2] * in2->m[2][1] + in1->m[3][3] *
	               in2->m[3][1];
	out->m[3][2] = in1->m[3][0] * in2->m[0][2] + in1->m[3][1] * in2->m[1][2] + in1->m[3][2] * in2->m[2][2] + in1->m[3][3] *
	               in2->m[3][2];
	out->m[3][3] = in1->m[3][0] * in2->m[0][3] + in1->m[3][1] * in2->m[1][3] + in1->m[3][2] * in2->m[2][3] + in1->m[3][3] *
	               in2->m[3][3];
#endif
}

void Matrix4x4_Transpose (mat4_t *out, const mat4_t *in1) {
	if (in1 == out) {
		static mat4_t in1_temp;
		Matrix4x4_Copy(&in1_temp, in1);
		in1 = &in1_temp;
	}

	out->m[0][0] = in1->m[0][0];
	out->m[0][1] = in1->m[1][0];
	out->m[0][2] = in1->m[2][0];
	out->m[0][3] = in1->m[3][0];
	out->m[1][0] = in1->m[0][1];
	out->m[1][1] = in1->m[1][1];
	out->m[1][2] = in1->m[2][1];
	out->m[1][3] = in1->m[3][1];
	out->m[2][0] = in1->m[0][2];
	out->m[2][1] = in1->m[1][2];
	out->m[2][2] = in1->m[2][2];
	out->m[2][3] = in1->m[3][2];
	out->m[3][0] = in1->m[0][3];
	out->m[3][1] = in1->m[1][3];
	out->m[3][2] = in1->m[2][3];
	out->m[3][3] = in1->m[3][3];
}

#if 0
// Adapted from code contributed to Mesa by David Moore (Mesa 7.6 under SGI Free License B - which is MIT/X11-type)
// added helper for common subexpression elimination by eihrul, and other optimizations by div0
int32_t Matrix4x4_Invert_Full (mat4_t *out, const mat4_t *in1) {
	float det;

	// note: orientation does not matter, as transpose(invert(transpose(m))) == invert(m), proof:
	//   transpose(invert(transpose(m))) * m
	// = transpose(invert(transpose(m))) * transpose(transpose(m))
	// = transpose(transpose(m) * invert(transpose(m)))
	// = transpose(identity)
	// = identity

	// this seems to help gcc's common subexpression elimination, and also makes the code look nicer
	float   m00 = in1->m[0][0], m01 = in1->m[0][1], m02 = in1->m[0][2], m03 = in1->m[0][3],
	        m10 = in1->m[1][0], m11 = in1->m[1][1], m12 = in1->m[1][2], m13 = in1->m[1][3],
	        m20 = in1->m[2][0], m21 = in1->m[2][1], m22 = in1->m[2][2], m23 = in1->m[2][3],
	        m30 = in1->m[3][0], m31 = in1->m[3][1], m32 = in1->m[3][2], m33 = in1->m[3][3];

	// calculate the adjoint
	out->m[0][0] =  (m11 * (m22 * m33 - m23 * m32) - m21 * (m12 * m33 - m13 * m32) + m31 * (m12 * m23 - m13 * m22));
	out->m[0][1] = -(m01 * (m22 * m33 - m23 * m32) - m21 * (m02 * m33 - m03 * m32) + m31 * (m02 * m23 - m03 * m22));
	out->m[0][2] =  (m01 * (m12 * m33 - m13 * m32) - m11 * (m02 * m33 - m03 * m32) + m31 * (m02 * m13 - m03 * m12));
	out->m[0][3] = -(m01 * (m12 * m23 - m13 * m22) - m11 * (m02 * m23 - m03 * m22) + m21 * (m02 * m13 - m03 * m12));
	out->m[1][0] = -(m10 * (m22 * m33 - m23 * m32) - m20 * (m12 * m33 - m13 * m32) + m30 * (m12 * m23 - m13 * m22));
	out->m[1][1] =  (m00 * (m22 * m33 - m23 * m32) - m20 * (m02 * m33 - m03 * m32) + m30 * (m02 * m23 - m03 * m22));
	out->m[1][2] = -(m00 * (m12 * m33 - m13 * m32) - m10 * (m02 * m33 - m03 * m32) + m30 * (m02 * m13 - m03 * m12));
	out->m[1][3] =  (m00 * (m12 * m23 - m13 * m22) - m10 * (m02 * m23 - m03 * m22) + m20 * (m02 * m13 - m03 * m12));
	out->m[2][0] =  (m10 * (m21 * m33 - m23 * m31) - m20 * (m11 * m33 - m13 * m31) + m30 * (m11 * m23 - m13 * m21));
	out->m[2][1] = -(m00 * (m21 * m33 - m23 * m31) - m20 * (m01 * m33 - m03 * m31) + m30 * (m01 * m23 - m03 * m21));
	out->m[2][2] =  (m00 * (m11 * m33 - m13 * m31) - m10 * (m01 * m33 - m03 * m31) + m30 * (m01 * m13 - m03 * m11));
	out->m[2][3] = -(m00 * (m11 * m23 - m13 * m21) - m10 * (m01 * m23 - m03 * m21) + m20 * (m01 * m13 - m03 * m11));
	out->m[3][0] = -(m10 * (m21 * m32 - m22 * m31) - m20 * (m11 * m32 - m12 * m31) + m30 * (m11 * m22 - m12 * m21));
	out->m[3][1] =  (m00 * (m21 * m32 - m22 * m31) - m20 * (m01 * m32 - m02 * m31) + m30 * (m01 * m22 - m02 * m21));
	out->m[3][2] = -(m00 * (m11 * m32 - m12 * m31) - m10 * (m01 * m32 - m02 * m31) + m30 * (m01 * m12 - m02 * m11));
	out->m[3][3] =  (m00 * (m11 * m22 - m12 * m21) - m10 * (m01 * m22 - m02 * m21) + m20 * (m01 * m12 - m02 * m11));

	// calculate the determinant (as inverse == 1/det * adjoint, adjoint * m == identity * det, so this calculates the det)
	det = m00 * out->m[0][0] + m10 * out->m[0][1] + m20 * out->m[0][2] + m30 * out->m[0][3];
	if (det == 0.0f) {
		return 0;
	}

	// multiplications are faster than divisions, usually
	det = 1.0f / det;

	// manually unrolled loop to multiply all matrix elements by 1/det
	out->m[0][0] *= det;
	out->m[0][1] *= det;
	out->m[0][2] *= det;
	out->m[0][3] *= det;
	out->m[1][0] *= det;
	out->m[1][1] *= det;
	out->m[1][2] *= det;
	out->m[1][3] *= det;
	out->m[2][0] *= det;
	out->m[2][1] *= det;
	out->m[2][2] *= det;
	out->m[2][3] *= det;
	out->m[3][0] *= det;
	out->m[3][1] *= det;
	out->m[3][2] *= det;
	out->m[3][3] *= det;

	return 1;
}
#elif 1
// Adapted from code contributed to Mesa by David Moore (Mesa 7.6 under SGI Free License B - which is MIT/X11-type)
int32_t Matrix4x4_Invert_Full (mat4_t *out, const mat4_t *in1) {
	mat4_t temp;
	double det;
	int32_t i, j;

#if MATRIX4x4_OPENGLORIENTATION
	temp.m[0][0] =  in1->m[1][1] * in1->m[2][2] * in1->m[3][3] - in1->m[1][1] * in1->m[2][3] * in1->m[3][2] - in1->m[2][1] *
	                in1->m[1][2] * in1->m[3][3] + in1->m[2][1] * in1->m[1][3] * in1->m[3][2] + in1->m[3][1] * in1->m[1][2] * in1->m[2][3] -
	                in1->m[3][1] * in1->m[1][3] * in1->m[2][2];
	temp.m[1][0] = -in1->m[1][0] * in1->m[2][2] * in1->m[3][3] + in1->m[1][0] * in1->m[2][3] * in1->m[3][2] + in1->m[2][0] *
	               in1->m[1][2] * in1->m[3][3] - in1->m[2][0] * in1->m[1][3] * in1->m[3][2] - in1->m[3][0] * in1->m[1][2] * in1->m[2][3] +
	               in1->m[3][0] * in1->m[1][3] * in1->m[2][2];
	temp.m[2][0] =  in1->m[1][0] * in1->m[2][1] * in1->m[3][3] - in1->m[1][0] * in1->m[2][3] * in1->m[3][1] - in1->m[2][0] *
	                in1->m[1][1] * in1->m[3][3] + in1->m[2][0] * in1->m[1][3] * in1->m[3][1] + in1->m[3][0] * in1->m[1][1] * in1->m[2][3] -
	                in1->m[3][0] * in1->m[1][3] * in1->m[2][1];
	temp.m[3][0] = -in1->m[1][0] * in1->m[2][1] * in1->m[3][2] + in1->m[1][0] * in1->m[2][2] * in1->m[3][1] + in1->m[2][0] *
	               in1->m[1][1] * in1->m[3][2] - in1->m[2][0] * in1->m[1][2] * in1->m[3][1] - in1->m[3][0] * in1->m[1][1] * in1->m[2][2] +
	               in1->m[3][0] * in1->m[1][2] * in1->m[2][1];
	temp.m[0][1] = -in1->m[0][1] * in1->m[2][2] * in1->m[3][3] + in1->m[0][1] * in1->m[2][3] * in1->m[3][2] + in1->m[2][1] *
	               in1->m[0][2] * in1->m[3][3] - in1->m[2][1] * in1->m[0][3] * in1->m[3][2] - in1->m[3][1] * in1->m[0][2] * in1->m[2][3] +
	               in1->m[3][1] * in1->m[0][3] * in1->m[2][2];
	temp.m[1][1] =  in1->m[0][0] * in1->m[2][2] * in1->m[3][3] - in1->m[0][0] * in1->m[2][3] * in1->m[3][2] - in1->m[2][0] *
	                in1->m[0][2] * in1->m[3][3] + in1->m[2][0] * in1->m[0][3] * in1->m[3][2] + in1->m[3][0] * in1->m[0][2] * in1->m[2][3] -
	                in1->m[3][0] * in1->m[0][3] * in1->m[2][2];
	temp.m[2][1] = -in1->m[0][0] * in1->m[2][1] * in1->m[3][3] + in1->m[0][0] * in1->m[2][3] * in1->m[3][1] + in1->m[2][0] *
	               in1->m[0][1] * in1->m[3][3] - in1->m[2][0] * in1->m[0][3] * in1->m[3][1] - in1->m[3][0] * in1->m[0][1] * in1->m[2][3] +
	               in1->m[3][0] * in1->m[0][3] * in1->m[2][1];
	temp.m[3][1] =  in1->m[0][0] * in1->m[2][1] * in1->m[3][2] - in1->m[0][0] * in1->m[2][2] * in1->m[3][1] - in1->m[2][0] *
	                in1->m[0][1] * in1->m[3][2] + in1->m[2][0] * in1->m[0][2] * in1->m[3][1] + in1->m[3][0] * in1->m[0][1] * in1->m[2][2] -
	                in1->m[3][0] * in1->m[0][2] * in1->m[2][1];
	temp.m[0][2] =  in1->m[0][1] * in1->m[1][2] * in1->m[3][3] - in1->m[0][1] * in1->m[1][3] * in1->m[3][2] - in1->m[1][1] *
	                in1->m[0][2] * in1->m[3][3] + in1->m[1][1] * in1->m[0][3] * in1->m[3][2] + in1->m[3][1] * in1->m[0][2] * in1->m[1][3] -
	                in1->m[3][1] * in1->m[0][3] * in1->m[1][2];
	temp.m[1][2] = -in1->m[0][0] * in1->m[1][2] * in1->m[3][3] + in1->m[0][0] * in1->m[1][3] * in1->m[3][2] + in1->m[1][0] *
	               in1->m[0][2] * in1->m[3][3] - in1->m[1][0] * in1->m[0][3] * in1->m[3][2] - in1->m[3][0] * in1->m[0][2] * in1->m[1][3] +
	               in1->m[3][0] * in1->m[0][3] * in1->m[1][2];
	temp.m[2][2] =  in1->m[0][0] * in1->m[1][1] * in1->m[3][3] - in1->m[0][0] * in1->m[1][3] * in1->m[3][1] - in1->m[1][0] *
	                in1->m[0][1] * in1->m[3][3] + in1->m[1][0] * in1->m[0][3] * in1->m[3][1] + in1->m[3][0] * in1->m[0][1] * in1->m[1][3] -
	                in1->m[3][0] * in1->m[0][3] * in1->m[1][1];
	temp.m[3][2] = -in1->m[0][0] * in1->m[1][1] * in1->m[3][2] + in1->m[0][0] * in1->m[1][2] * in1->m[3][1] + in1->m[1][0] *
	               in1->m[0][1] * in1->m[3][2] - in1->m[1][0] * in1->m[0][2] * in1->m[3][1] - in1->m[3][0] * in1->m[0][1] * in1->m[1][2] +
	               in1->m[3][0] * in1->m[0][2] * in1->m[1][1];
	temp.m[0][3] = -in1->m[0][1] * in1->m[1][2] * in1->m[2][3] + in1->m[0][1] * in1->m[1][3] * in1->m[2][2] + in1->m[1][1] *
	               in1->m[0][2] * in1->m[2][3] - in1->m[1][1] * in1->m[0][3] * in1->m[2][2] - in1->m[2][1] * in1->m[0][2] * in1->m[1][3] +
	               in1->m[2][1] * in1->m[0][3] * in1->m[1][2];
	temp.m[1][3] =  in1->m[0][0] * in1->m[1][2] * in1->m[2][3] - in1->m[0][0] * in1->m[1][3] * in1->m[2][2] - in1->m[1][0] *
	                in1->m[0][2] * in1->m[2][3] + in1->m[1][0] * in1->m[0][3] * in1->m[2][2] + in1->m[2][0] * in1->m[0][2] * in1->m[1][3] -
	                in1->m[2][0] * in1->m[0][3] * in1->m[1][2];
	temp.m[2][3] = -in1->m[0][0] * in1->m[1][1] * in1->m[2][3] + in1->m[0][0] * in1->m[1][3] * in1->m[2][1] + in1->m[1][0] *
	               in1->m[0][1] * in1->m[2][3] - in1->m[1][0] * in1->m[0][3] * in1->m[2][1] - in1->m[2][0] * in1->m[0][1] * in1->m[1][3] +
	               in1->m[2][0] * in1->m[0][3] * in1->m[1][1];
	temp.m[3][3] =  in1->m[0][0] * in1->m[1][1] * in1->m[2][2] - in1->m[0][0] * in1->m[1][2] * in1->m[2][1] - in1->m[1][0] *
	                in1->m[0][1] * in1->m[2][2] + in1->m[1][0] * in1->m[0][2] * in1->m[2][1] + in1->m[2][0] * in1->m[0][1] * in1->m[1][2] -
	                in1->m[2][0] * in1->m[0][2] * in1->m[1][1];
#else
	temp.m[0][0] =  in1->m[1][1] * in1->m[2][2] * in1->m[3][3] - in1->m[1][1] * in1->m[3][2] * in1->m[2][3] - in1->m[1][2] *
	                in1->m[2][1] * in1->m[3][3] + in1->m[1][2] * in1->m[3][1] * in1->m[2][3] + in1->m[1][3] * in1->m[2][1] * in1->m[3][2] -
	                in1->m[1][3] * in1->m[3][1] * in1->m[2][2];
	temp.m[0][1] = -in1->m[0][1] * in1->m[2][2] * in1->m[3][3] + in1->m[0][1] * in1->m[3][2] * in1->m[2][3] + in1->m[0][2] *
	               in1->m[2][1] * in1->m[3][3] - in1->m[0][2] * in1->m[3][1] * in1->m[2][3] - in1->m[0][3] * in1->m[2][1] * in1->m[3][2] +
	               in1->m[0][3] * in1->m[3][1] * in1->m[2][2];
	temp.m[0][2] =  in1->m[0][1] * in1->m[1][2] * in1->m[3][3] - in1->m[0][1] * in1->m[3][2] * in1->m[1][3] - in1->m[0][2] *
	                in1->m[1][1] * in1->m[3][3] + in1->m[0][2] * in1->m[3][1] * in1->m[1][3] + in1->m[0][3] * in1->m[1][1] * in1->m[3][2] -
	                in1->m[0][3] * in1->m[3][1] * in1->m[1][2];
	temp.m[0][3] = -in1->m[0][1] * in1->m[1][2] * in1->m[2][3] + in1->m[0][1] * in1->m[2][2] * in1->m[1][3] + in1->m[0][2] *
	               in1->m[1][1] * in1->m[2][3] - in1->m[0][2] * in1->m[2][1] * in1->m[1][3] - in1->m[0][3] * in1->m[1][1] * in1->m[2][2] +
	               in1->m[0][3] * in1->m[2][1] * in1->m[1][2];
	temp.m[1][0] = -in1->m[1][0] * in1->m[2][2] * in1->m[3][3] + in1->m[1][0] * in1->m[3][2] * in1->m[2][3] + in1->m[1][2] *
	               in1->m[2][0] * in1->m[3][3] - in1->m[1][2] * in1->m[3][0] * in1->m[2][3] - in1->m[1][3] * in1->m[2][0] * in1->m[3][2] +
	               in1->m[1][3] * in1->m[3][0] * in1->m[2][2];
	temp.m[1][1] =  in1->m[0][0] * in1->m[2][2] * in1->m[3][3] - in1->m[0][0] * in1->m[3][2] * in1->m[2][3] - in1->m[0][2] *
	                in1->m[2][0] * in1->m[3][3] + in1->m[0][2] * in1->m[3][0] * in1->m[2][3] + in1->m[0][3] * in1->m[2][0] * in1->m[3][2] -
	                in1->m[0][3] * in1->m[3][0] * in1->m[2][2];
	temp.m[1][2] = -in1->m[0][0] * in1->m[1][2] * in1->m[3][3] + in1->m[0][0] * in1->m[3][2] * in1->m[1][3] + in1->m[0][2] *
	               in1->m[1][0] * in1->m[3][3] - in1->m[0][2] * in1->m[3][0] * in1->m[1][3] - in1->m[0][3] * in1->m[1][0] * in1->m[3][2] +
	               in1->m[0][3] * in1->m[3][0] * in1->m[1][2];
	temp.m[1][3] =  in1->m[0][0] * in1->m[1][2] * in1->m[2][3] - in1->m[0][0] * in1->m[2][2] * in1->m[1][3] - in1->m[0][2] *
	                in1->m[1][0] * in1->m[2][3] + in1->m[0][2] * in1->m[2][0] * in1->m[1][3] + in1->m[0][3] * in1->m[1][0] * in1->m[2][2] -
	                in1->m[0][3] * in1->m[2][0] * in1->m[1][2];
	temp.m[2][0] =  in1->m[1][0] * in1->m[2][1] * in1->m[3][3] - in1->m[1][0] * in1->m[3][1] * in1->m[2][3] - in1->m[1][1] *
	                in1->m[2][0] * in1->m[3][3] + in1->m[1][1] * in1->m[3][0] * in1->m[2][3] + in1->m[1][3] * in1->m[2][0] * in1->m[3][1] -
	                in1->m[1][3] * in1->m[3][0] * in1->m[2][1];
	temp.m[2][1] = -in1->m[0][0] * in1->m[2][1] * in1->m[3][3] + in1->m[0][0] * in1->m[3][1] * in1->m[2][3] + in1->m[0][1] *
	               in1->m[2][0] * in1->m[3][3] - in1->m[0][1] * in1->m[3][0] * in1->m[2][3] - in1->m[0][3] * in1->m[2][0] * in1->m[3][1] +
	               in1->m[0][3] * in1->m[3][0] * in1->m[2][1];
	temp.m[2][2] =  in1->m[0][0] * in1->m[1][1] * in1->m[3][3] - in1->m[0][0] * in1->m[3][1] * in1->m[1][3] - in1->m[0][1] *
	                in1->m[1][0] * in1->m[3][3] + in1->m[0][1] * in1->m[3][0] * in1->m[1][3] + in1->m[0][3] * in1->m[1][0] * in1->m[3][1] -
	                in1->m[0][3] * in1->m[3][0] * in1->m[1][1];
	temp.m[2][3] = -in1->m[0][0] * in1->m[1][1] * in1->m[2][3] + in1->m[0][0] * in1->m[2][1] * in1->m[1][3] + in1->m[0][1] *
	               in1->m[1][0] * in1->m[2][3] - in1->m[0][1] * in1->m[2][0] * in1->m[1][3] - in1->m[0][3] * in1->m[1][0] * in1->m[2][1] +
	               in1->m[0][3] * in1->m[2][0] * in1->m[1][1];
	temp.m[3][0] = -in1->m[1][0] * in1->m[2][1] * in1->m[3][2] + in1->m[1][0] * in1->m[3][1] * in1->m[2][2] + in1->m[1][1] *
	               in1->m[2][0] * in1->m[3][2] - in1->m[1][1] * in1->m[3][0] * in1->m[2][2] - in1->m[1][2] * in1->m[2][0] * in1->m[3][1] +
	               in1->m[1][2] * in1->m[3][0] * in1->m[2][1];
	temp.m[3][1] =  in1->m[0][0] * in1->m[2][1] * in1->m[3][2] - in1->m[0][0] * in1->m[3][1] * in1->m[2][2] - in1->m[0][1] *
	                in1->m[2][0] * in1->m[3][2] + in1->m[0][1] * in1->m[3][0] * in1->m[2][2] + in1->m[0][2] * in1->m[2][0] * in1->m[3][1] -
	                in1->m[0][2] * in1->m[3][0] * in1->m[2][1];
	temp.m[3][2] = -in1->m[0][0] * in1->m[1][1] * in1->m[3][2] + in1->m[0][0] * in1->m[3][1] * in1->m[1][2] + in1->m[0][1] *
	               in1->m[1][0] * in1->m[3][2] - in1->m[0][1] * in1->m[3][0] * in1->m[1][2] - in1->m[0][2] * in1->m[1][0] * in1->m[3][1] +
	               in1->m[0][2] * in1->m[3][0] * in1->m[1][1];
	temp.m[3][3] =  in1->m[0][0] * in1->m[1][1] * in1->m[2][2] - in1->m[0][0] * in1->m[2][1] * in1->m[1][2] - in1->m[0][1] *
	                in1->m[1][0] * in1->m[2][2] + in1->m[0][1] * in1->m[2][0] * in1->m[1][2] + in1->m[0][2] * in1->m[1][0] * in1->m[2][1] -
	                in1->m[0][2] * in1->m[2][0] * in1->m[1][1];
#endif

	det = in1->m[0][0] * temp.m[0][0] + in1->m[1][0] * temp.m[0][1] + in1->m[2][0] * temp.m[0][2] + in1->m[3][0] *
	      temp.m[0][3];
	if (det == 0.0f) {
		return 0;
	}

	det = 1.0f / det;

	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++) {
			out->m[i][j] = temp.m[i][j] * det;
		}

	return 1;
}
#else
int32_t Matrix4x4_Invert_Full (mat4_t *out, const mat4_t *in1) {
	double	*temp;
	double	*r[4];
	double	rtemp[4][8];
	double	m[4];
	double	s;

	r[0]	= rtemp[0];
	r[1]	= rtemp[1];
	r[2]	= rtemp[2];
	r[3]	= rtemp[3];

#if MATRIX4x4_OPENGLORIENTATION
	r[0][0]	= in1->m[0][0];
	r[0][1]	= in1->m[1][0];
	r[0][2]	= in1->m[2][0];
	r[0][3]	= in1->m[3][0];
	r[0][4]	= 1.0;
	r[0][5]	=				r[0][6]	=				r[0][7]	= 0.0;

	r[1][0]	= in1->m[0][1];
	r[1][1]	= in1->m[1][1];
	r[1][2]	= in1->m[2][1];
	r[1][3]	= in1->m[3][1];
	r[1][5]	= 1.0;
	r[1][4]	=				r[1][6]	=				r[1][7]	= 0.0;

	r[2][0]	= in1->m[0][2];
	r[2][1]	= in1->m[1][2];
	r[2][2]	= in1->m[2][2];
	r[2][3]	= in1->m[3][2];
	r[2][6]	= 1.0;
	r[2][4]	=				r[2][5]	=				r[2][7]	= 0.0;

	r[3][0]	= in1->m[0][3];
	r[3][1]	= in1->m[1][3];
	r[3][2]	= in1->m[2][3];
	r[3][3]	= in1->m[3][3];
	r[3][7]	= 1.0;
	r[3][4]	=				r[3][5]	=				r[3][6]	= 0.0;
#else
	r[0][0]	= in1->m[0][0];
	r[0][1]	= in1->m[0][1];
	r[0][2]	= in1->m[0][2];
	r[0][3]	= in1->m[0][3];
	r[0][4]	= 1.0;
	r[0][5]	=				r[0][6]	=				r[0][7]	= 0.0;

	r[1][0]	= in1->m[1][0];
	r[1][1]	= in1->m[1][1];
	r[1][2]	= in1->m[1][2];
	r[1][3]	= in1->m[1][3];
	r[1][5]	= 1.0;
	r[1][4]	=				r[1][6]	=				r[1][7]	= 0.0;

	r[2][0]	= in1->m[2][0];
	r[2][1]	= in1->m[2][1];
	r[2][2]	= in1->m[2][2];
	r[2][3]	= in1->m[2][3];
	r[2][6]	= 1.0;
	r[2][4]	=				r[2][5]	=				r[2][7]	= 0.0;

	r[3][0]	= in1->m[3][0];
	r[3][1]	= in1->m[3][1];
	r[3][2]	= in1->m[3][2];
	r[3][3]	= in1->m[3][3];
	r[3][7]	= 1.0;
	r[3][4]	=				r[3][5]	=				r[3][6]	= 0.0;
#endif

	if (fabs (r[3][0]) > fabs (r[2][0])) {
		temp = r[3];
		r[3] = r[2];
		r[2] = temp;
	}
	if (fabs (r[2][0]) > fabs (r[1][0])) {
		temp = r[2];
		r[2] = r[1];
		r[1] = temp;
	}
	if (fabs (r[1][0]) > fabs (r[0][0])) {
		temp = r[1];
		r[1] = r[0];
		r[0] = temp;
	}

	if (r[0][0]) {
		m[1]	= r[1][0] / r[0][0];
		m[2]	= r[2][0] / r[0][0];
		m[3]	= r[3][0] / r[0][0];

		s	= r[0][1];
		r[1][1] -= m[1] * s;
		r[2][1] -= m[2] * s;
		r[3][1] -= m[3] * s;
		s	= r[0][2];
		r[1][2] -= m[1] * s;
		r[2][2] -= m[2] * s;
		r[3][2] -= m[3] * s;
		s	= r[0][3];
		r[1][3] -= m[1] * s;
		r[2][3] -= m[2] * s;
		r[3][3] -= m[3] * s;

		s	= r[0][4];
		if (s) {
			r[1][4] -= m[1] * s;
			r[2][4] -= m[2] * s;
			r[3][4] -= m[3] * s;
		}
		s	= r[0][5];
		if (s) {
			r[1][5] -= m[1] * s;
			r[2][5] -= m[2] * s;
			r[3][5] -= m[3] * s;
		}
		s	= r[0][6];
		if (s) {
			r[1][6] -= m[1] * s;
			r[2][6] -= m[2] * s;
			r[3][6] -= m[3] * s;
		}
		s	= r[0][7];
		if (s) {
			r[1][7] -= m[1] * s;
			r[2][7] -= m[2] * s;
			r[3][7] -= m[3] * s;
		}

		if (fabs (r[3][1]) > fabs (r[2][1])) {
			temp = r[3];
			r[3] = r[2];
			r[2] = temp;
		}
		if (fabs (r[2][1]) > fabs (r[1][1])) {
			temp = r[2];
			r[2] = r[1];
			r[1] = temp;
		}

		if (r[1][1]) {
			m[2]		= r[2][1] / r[1][1];
			m[3]		= r[3][1] / r[1][1];
			r[2][2]	-= m[2] * r[1][2];
			r[3][2]	-= m[3] * r[1][2];
			r[2][3]	-= m[2] * r[1][3];
			r[3][3]	-= m[3] * r[1][3];

			s	= r[1][4];
			if (s) {
				r[2][4] -= m[2] * s;
				r[3][4] -= m[3] * s;
			}
			s	= r[1][5];
			if (s) {
				r[2][5] -= m[2] * s;
				r[3][5] -= m[3] * s;
			}
			s	= r[1][6];
			if (s) {
				r[2][6] -= m[2] * s;
				r[3][6] -= m[3] * s;
			}
			s	= r[1][7];
			if (s) {
				r[2][7] -= m[2] * s;
				r[3][7] -= m[3] * s;
			}

			if (fabs (r[3][2]) > fabs (r[2][2])) {
				temp = r[3];
				r[3] = r[2];
				r[2] = temp;
			}

			if (r[2][2]) {
				m[3]		= r[3][2] / r[2][2];
				r[3][3]	-= m[3] * r[2][3];
				r[3][4]	-= m[3] * r[2][4];
				r[3][5]	-= m[3] * r[2][5];
				r[3][6]	-= m[3] * r[2][6];
				r[3][7]	-= m[3] * r[2][7];

				if (r[3][3]) {
					s			= 1.0 / r[3][3];
					r[3][4]	*= s;
					r[3][5]	*= s;
					r[3][6]	*= s;
					r[3][7]	*= s;

					m[2]		= r[2][3];
					s			= 1.0 / r[2][2];
					r[2][4]	= s * (r[2][4] - r[3][4] * m[2]);
					r[2][5]	= s * (r[2][5] - r[3][5] * m[2]);
					r[2][6]	= s * (r[2][6] - r[3][6] * m[2]);
					r[2][7]	= s * (r[2][7] - r[3][7] * m[2]);

					m[1]		= r[1][3];
					r[1][4]	-= r[3][4] * m[1], r[1][5] -= r[3][5] * m[1];
					r[1][6]	-= r[3][6] * m[1], r[1][7] -= r[3][7] * m[1];

					m[0]		= r[0][3];
					r[0][4]	-= r[3][4] * m[0], r[0][5] -= r[3][5] * m[0];
					r[0][6]	-= r[3][6] * m[0], r[0][7] -= r[3][7] * m[0];

					m[1]		= r[1][2];
					s			= 1.0 / r[1][1];
					r[1][4]	= s * (r[1][4] - r[2][4] * m[1]), r[1][5] = s * (r[1][5] - r[2][5] * m[1]);
					r[1][6]	= s * (r[1][6] - r[2][6] * m[1]), r[1][7] = s * (r[1][7] - r[2][7] * m[1]);

					m[0]		= r[0][2];
					r[0][4]	-= r[2][4] * m[0], r[0][5] -= r[2][5] * m[0];
					r[0][6]	-= r[2][6] * m[0], r[0][7] -= r[2][7] * m[0];

					m[0]		= r[0][1];
					s			= 1.0 / r[0][0];
					r[0][4]	= s * (r[0][4] - r[1][4] * m[0]), r[0][5] = s * (r[0][5] - r[1][5] * m[0]);
					r[0][6]	= s * (r[0][6] - r[1][6] * m[0]), r[0][7] = s * (r[0][7] - r[1][7] * m[0]);

#if MATRIX4x4_OPENGLORIENTATION
					out->m[0][0]	= r[0][4];
					out->m[0][1]	= r[1][4];
					out->m[0][2]	= r[2][4];
					out->m[0][3]	= r[3][4];
					out->m[1][0]	= r[0][5];
					out->m[1][1]	= r[1][5];
					out->m[1][2]	= r[2][5];
					out->m[1][3]	= r[3][5];
					out->m[2][0]	= r[0][6];
					out->m[2][1]	= r[1][6];
					out->m[2][2]	= r[2][6];
					out->m[2][3]	= r[3][6];
					out->m[3][0]	= r[0][7];
					out->m[3][1]	= r[1][7];
					out->m[3][2]	= r[2][7];
					out->m[3][3]	= r[3][7];
#else
					out->m[0][0]	= r[0][4];
					out->m[0][1]	= r[0][5];
					out->m[0][2]	= r[0][6];
					out->m[0][3]	= r[0][7];
					out->m[1][0]	= r[1][4];
					out->m[1][1]	= r[1][5];
					out->m[1][2]	= r[1][6];
					out->m[1][3]	= r[1][7];
					out->m[2][0]	= r[2][4];
					out->m[2][1]	= r[2][5];
					out->m[2][2]	= r[2][6];
					out->m[2][3]	= r[2][7];
					out->m[3][0]	= r[3][4];
					out->m[3][1]	= r[3][5];
					out->m[3][2]	= r[3][6];
					out->m[3][3]	= r[3][7];
#endif

					return 1;
				}
			}
		}
	}

	return 0;
}
#endif

void Matrix4x4_Invert_Simple (mat4_t *out, const mat4_t *in1) {
	// we only support uniform scaling, so assume the first row is enough
	// (note the lack of sqrt here, because we're trying to undo the scaling,
	// this means multiplying by the inverse scale twice - squaring it, which
	// makes the sqrt a waste of time)
#if 1
	double scale = 1.0 / (in1->m[0][0] * in1->m[0][0] + in1->m[0][1] * in1->m[0][1] + in1->m[0][2] * in1->m[0][2]);
#else
	double scale = 3.0 / sqrt
	               (in1->m[0][0] * in1->m[0][0] + in1->m[0][1] * in1->m[0][1] + in1->m[0][2] * in1->m[0][2]
	                + in1->m[1][0] * in1->m[1][0] + in1->m[1][1] * in1->m[1][1] + in1->m[1][2] * in1->m[1][2]
	                + in1->m[2][0] * in1->m[2][0] + in1->m[2][1] * in1->m[2][1] + in1->m[2][2] * in1->m[2][2]);
	scale *= scale;
#endif

	// invert the rotation by transposing and multiplying by the squared
	// recipricol of the input matrix scale as described above
	out->m[0][0] = in1->m[0][0] * scale;
	out->m[0][1] = in1->m[1][0] * scale;
	out->m[0][2] = in1->m[2][0] * scale;
	out->m[1][0] = in1->m[0][1] * scale;
	out->m[1][1] = in1->m[1][1] * scale;
	out->m[1][2] = in1->m[2][1] * scale;
	out->m[2][0] = in1->m[0][2] * scale;
	out->m[2][1] = in1->m[1][2] * scale;
	out->m[2][2] = in1->m[2][2] * scale;

#if MATRIX4x4_OPENGLORIENTATION
	// invert the translate
	out->m[3][0] = -(in1->m[3][0] * out->m[0][0] + in1->m[3][1] * out->m[1][0] + in1->m[3][2] * out->m[2][0]);
	out->m[3][1] = -(in1->m[3][0] * out->m[0][1] + in1->m[3][1] * out->m[1][1] + in1->m[3][2] * out->m[2][1]);
	out->m[3][2] = -(in1->m[3][0] * out->m[0][2] + in1->m[3][1] * out->m[1][2] + in1->m[3][2] * out->m[2][2]);

	// don't know if there's anything worth doing here
	out->m[0][3] = 0;
	out->m[1][3] = 0;
	out->m[2][3] = 0;
	out->m[3][3] = 1;
#else
	// invert the translate
	out->m[0][3] = -(in1->m[0][3] * out->m[0][0] + in1->m[1][3] * out->m[0][1] + in1->m[2][3] * out->m[0][2]);
	out->m[1][3] = -(in1->m[0][3] * out->m[1][0] + in1->m[1][3] * out->m[1][1] + in1->m[2][3] * out->m[1][2]);
	out->m[2][3] = -(in1->m[0][3] * out->m[2][0] + in1->m[1][3] * out->m[2][1] + in1->m[2][3] * out->m[2][2]);

	// don't know if there's anything worth doing here
	out->m[3][0] = 0;
	out->m[3][1] = 0;
	out->m[3][2] = 0;
	out->m[3][3] = 1;
#endif
}

void Matrix4x4_Interpolate (mat4_t *out, const mat4_t *in1, const mat4_t *in2, double frac) {
	int32_t i, j;
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++) {
			out->m[i][j] = in1->m[i][j] + frac * (in2->m[i][j] - in1->m[i][j]);
		}
}

void Matrix4x4_Clear (mat4_t *out) {
	int32_t i, j;
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++) {
			out->m[i][j] = 0;
		}
}

void Matrix4x4_Accumulate (mat4_t *out, const mat4_t *in, double weight) {
	int32_t i, j;
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++) {
			out->m[i][j] += in->m[i][j] * weight;
		}
}

void Matrix4x4_Normalize (mat4_t *out, const mat4_t *in1) {
	// scale rotation matrix vectors to a length of 1
	// note: this is only designed to undo uniform scaling
	double scale = 1.0 / sqrt(in1->m[0][0] * in1->m[0][0] + in1->m[0][1] * in1->m[0][1] + in1->m[0][2] * in1->m[0][2]);
	*out = *in1;
	Matrix4x4_Scale(out, scale, 1);
}

void Matrix4x4_Normalize3 (mat4_t *out, const mat4_t *in1) {
	int32_t i;
	double scale;
	// scale each rotation matrix vector to a length of 1
	// intended for use after Matrix4x4_Interpolate or Matrix4x4_Accumulate
	*out = *in1;
	for (i = 0; i < 3; i++) {
#if MATRIX4x4_OPENGLORIENTATION
		scale = sqrt(in1->m[i][0] * in1->m[i][0] + in1->m[i][1] * in1->m[i][1] + in1->m[i][2] * in1->m[i][2]);
		if (scale) {
			scale = 1.0 / scale;
		}
		out->m[i][0] *= scale;
		out->m[i][1] *= scale;
		out->m[i][2] *= scale;
#else
		scale = sqrt(in1->m[0][i] * in1->m[0][i] + in1->m[1][i] * in1->m[1][i] + in1->m[2][i] * in1->m[2][i]);
		if (scale) {
			scale = 1.0 / scale;
		}
		out->m[0][i] *= scale;
		out->m[1][i] *= scale;
		out->m[2][i] *= scale;
#endif
	}
}

void Matrix4x4_Reflect (mat4_t *out, double normalx, double normaly, double normalz, double dist,
                        double axisscale) {
	int32_t i;
	double d;
	double p[4], p2[4];
	p[0] = normalx;
	p[1] = normaly;
	p[2] = normalz;
	p[3] = -dist;
	p2[0] = p[0] * axisscale;
	p2[1] = p[1] * axisscale;
	p2[2] = p[2] * axisscale;
	p2[3] = 0;
	for (i = 0; i < 4; i++) {
#if MATRIX4x4_OPENGLORIENTATION
		d = out->m[i][0] * p[0] + out->m[i][1] * p[1] + out->m[i][2] * p[2] + out->m[i][3] * p[3];
		out->m[i][0] += p2[0] * d;
		out->m[i][1] += p2[1] * d;
		out->m[i][2] += p2[2] * d;
#else
		d = out->m[0][i] * p[0] + out->m[1][i] * p[1] + out->m[2][i] * p[2] + out->m[3][i] * p[3];
		out->m[0][i] += p2[0] * d;
		out->m[1][i] += p2[1] * d;
		out->m[2][i] += p2[2] * d;
#endif
	}
}

void Matrix4x4_CreateIdentity (mat4_t *out) {
	out->m[0][0] = 1.0f;
	out->m[0][1] = 0.0f;
	out->m[0][2] = 0.0f;
	out->m[0][3] = 0.0f;
	out->m[1][0] = 0.0f;
	out->m[1][1] = 1.0f;
	out->m[1][2] = 0.0f;
	out->m[1][3] = 0.0f;
	out->m[2][0] = 0.0f;
	out->m[2][1] = 0.0f;
	out->m[2][2] = 1.0f;
	out->m[2][3] = 0.0f;
	out->m[3][0] = 0.0f;
	out->m[3][1] = 0.0f;
	out->m[3][2] = 0.0f;
	out->m[3][3] = 1.0f;
}

void Matrix4x4_CreateTranslate (mat4_t *out, double x, double y, double z) {
#if MATRIX4x4_OPENGLORIENTATION
	out->m[0][0] = 1.0f;
	out->m[1][0] = 0.0f;
	out->m[2][0] = 0.0f;
	out->m[3][0] = x;
	out->m[0][1] = 0.0f;
	out->m[1][1] = 1.0f;
	out->m[2][1] = 0.0f;
	out->m[3][1] = y;
	out->m[0][2] = 0.0f;
	out->m[1][2] = 0.0f;
	out->m[2][2] = 1.0f;
	out->m[3][2] = z;
	out->m[0][3] = 0.0f;
	out->m[1][3] = 0.0f;
	out->m[2][3] = 0.0f;
	out->m[3][3] = 1.0f;
#else
	out->m[0][0] = 1.0f;
	out->m[0][1] = 0.0f;
	out->m[0][2] = 0.0f;
	out->m[0][3] = x;
	out->m[1][0] = 0.0f;
	out->m[1][1] = 1.0f;
	out->m[1][2] = 0.0f;
	out->m[1][3] = y;
	out->m[2][0] = 0.0f;
	out->m[2][1] = 0.0f;
	out->m[2][2] = 1.0f;
	out->m[2][3] = z;
	out->m[3][0] = 0.0f;
	out->m[3][1] = 0.0f;
	out->m[3][2] = 0.0f;
	out->m[3][3] = 1.0f;
#endif
}

void Matrix4x4_CreateRotate (mat4_t *out, double angle, double x, double y, double z) {
	double len, c, s;

	len = x * x + y * y + z * z;
	if (len != 0.0f) {
		len = 1.0f / sqrt(len);
	}
	x *= len;
	y *= len;
	z *= len;

	angle *= (-M_PI / 180.0);
	c = cos(angle);
	s = sin(angle);

#if MATRIX4x4_OPENGLORIENTATION
	out->m[0][0] = x * x + c * (1 - x * x);
	out->m[1][0] = x * y * (1 - c) + z * s;
	out->m[2][0] = z * x * (1 - c) - y * s;
	out->m[3][0] = 0.0f;
	out->m[0][1] = x * y * (1 - c) - z * s;
	out->m[1][1] = y * y + c * (1 - y * y);
	out->m[2][1] = y * z * (1 - c) + x * s;
	out->m[3][1] = 0.0f;
	out->m[0][2] = z * x * (1 - c) + y * s;
	out->m[1][2] = y * z * (1 - c) - x * s;
	out->m[2][2] = z * z + c * (1 - z * z);
	out->m[3][2] = 0.0f;
	out->m[0][3] = 0.0f;
	out->m[1][3] = 0.0f;
	out->m[2][3] = 0.0f;
	out->m[3][3] = 1.0f;
#else
	out->m[0][0] = x * x + c * (1 - x * x);
	out->m[0][1] = x * y * (1 - c) + z * s;
	out->m[0][2] = z * x * (1 - c) - y * s;
	out->m[0][3] = 0.0f;
	out->m[1][0] = x * y * (1 - c) - z * s;
	out->m[1][1] = y * y + c * (1 - y * y);
	out->m[1][2] = y * z * (1 - c) + x * s;
	out->m[1][3] = 0.0f;
	out->m[2][0] = z * x * (1 - c) + y * s;
	out->m[2][1] = y * z * (1 - c) - x * s;
	out->m[2][2] = z * z + c * (1 - z * z);
	out->m[2][3] = 0.0f;
	out->m[3][0] = 0.0f;
	out->m[3][1] = 0.0f;
	out->m[3][2] = 0.0f;
	out->m[3][3] = 1.0f;
#endif
}

void Matrix4x4_CreateScale (mat4_t *out, double x) {
	out->m[0][0] = x;
	out->m[0][1] = 0.0f;
	out->m[0][2] = 0.0f;
	out->m[0][3] = 0.0f;
	out->m[1][0] = 0.0f;
	out->m[1][1] = x;
	out->m[1][2] = 0.0f;
	out->m[1][3] = 0.0f;
	out->m[2][0] = 0.0f;
	out->m[2][1] = 0.0f;
	out->m[2][2] = x;
	out->m[2][3] = 0.0f;
	out->m[3][0] = 0.0f;
	out->m[3][1] = 0.0f;
	out->m[3][2] = 0.0f;
	out->m[3][3] = 1.0f;
}

void Matrix4x4_CreateScale3 (mat4_t *out, double x, double y, double z) {
	out->m[0][0] = x;
	out->m[0][1] = 0.0f;
	out->m[0][2] = 0.0f;
	out->m[0][3] = 0.0f;
	out->m[1][0] = 0.0f;
	out->m[1][1] = y;
	out->m[1][2] = 0.0f;
	out->m[1][3] = 0.0f;
	out->m[2][0] = 0.0f;
	out->m[2][1] = 0.0f;
	out->m[2][2] = z;
	out->m[2][3] = 0.0f;
	out->m[3][0] = 0.0f;
	out->m[3][1] = 0.0f;
	out->m[3][2] = 0.0f;
	out->m[3][3] = 1.0f;
}

void Matrix4x4_CreateFromQuakeEntity(mat4_t *out, double x, double y, double z, double pitch, double yaw,
                                     double roll, double scale) {
	double angle, sr, sp, sy, cr, cp, cy;

	if (roll) {
		angle = yaw * (M_PI * 2 / 360);
		sy = sin(angle);
		cy = cos(angle);
		angle = pitch * (M_PI * 2 / 360);
		sp = sin(angle);
		cp = cos(angle);
		angle = roll * (M_PI * 2 / 360);
		sr = sin(angle);
		cr = cos(angle);
#if MATRIX4x4_OPENGLORIENTATION
		out->m[0][0] = (cp * cy) * scale;
		out->m[1][0] = (sr * sp * cy + cr * -sy) * scale;
		out->m[2][0] = (cr * sp * cy + -sr * -sy) * scale;
		out->m[3][0] = x;
		out->m[0][1] = (cp * sy) * scale;
		out->m[1][1] = (sr * sp * sy + cr * cy) * scale;
		out->m[2][1] = (cr * sp * sy + -sr * cy) * scale;
		out->m[3][1] = y;
		out->m[0][2] = (-sp) * scale;
		out->m[1][2] = (sr * cp) * scale;
		out->m[2][2] = (cr * cp) * scale;
		out->m[3][2] = z;
		out->m[0][3] = 0;
		out->m[1][3] = 0;
		out->m[2][3] = 0;
		out->m[3][3] = 1;
#else
		out->m[0][0] = (cp * cy) * scale;
		out->m[0][1] = (sr * sp * cy + cr * -sy) * scale;
		out->m[0][2] = (cr * sp * cy + -sr * -sy) * scale;
		out->m[0][3] = x;
		out->m[1][0] = (cp * sy) * scale;
		out->m[1][1] = (sr * sp * sy + cr * cy) * scale;
		out->m[1][2] = (cr * sp * sy + -sr * cy) * scale;
		out->m[1][3] = y;
		out->m[2][0] = (-sp) * scale;
		out->m[2][1] = (sr * cp) * scale;
		out->m[2][2] = (cr * cp) * scale;
		out->m[2][3] = z;
		out->m[3][0] = 0;
		out->m[3][1] = 0;
		out->m[3][2] = 0;
		out->m[3][3] = 1;
#endif
	} else if (pitch) {
		angle = yaw * (M_PI * 2 / 360);
		sy = sin(angle);
		cy = cos(angle);
		angle = pitch * (M_PI * 2 / 360);
		sp = sin(angle);
		cp = cos(angle);
#if MATRIX4x4_OPENGLORIENTATION
		out->m[0][0] = (cp * cy) * scale;
		out->m[1][0] = (-sy) * scale;
		out->m[2][0] = (sp * cy) * scale;
		out->m[3][0] = x;
		out->m[0][1] = (cp * sy) * scale;
		out->m[1][1] = (cy) * scale;
		out->m[2][1] = (sp * sy) * scale;
		out->m[3][1] = y;
		out->m[0][2] = (-sp) * scale;
		out->m[1][2] = 0;
		out->m[2][2] = (cp) * scale;
		out->m[3][2] = z;
		out->m[0][3] = 0;
		out->m[1][3] = 0;
		out->m[2][3] = 0;
		out->m[3][3] = 1;
#else
		out->m[0][0] = (cp * cy) * scale;
		out->m[0][1] = (-sy) * scale;
		out->m[0][2] = (sp * cy) * scale;
		out->m[0][3] = x;
		out->m[1][0] = (cp * sy) * scale;
		out->m[1][1] = (cy) * scale;
		out->m[1][2] = (sp * sy) * scale;
		out->m[1][3] = y;
		out->m[2][0] = (-sp) * scale;
		out->m[2][1] = 0;
		out->m[2][2] = (cp) * scale;
		out->m[2][3] = z;
		out->m[3][0] = 0;
		out->m[3][1] = 0;
		out->m[3][2] = 0;
		out->m[3][3] = 1;
#endif
	} else if (yaw) {
		angle = yaw * (M_PI * 2 / 360);
		sy = sin(angle);
		cy = cos(angle);
#if MATRIX4x4_OPENGLORIENTATION
		out->m[0][0] = (cy) * scale;
		out->m[1][0] = (-sy) * scale;
		out->m[2][0] = 0;
		out->m[3][0] = x;
		out->m[0][1] = (sy) * scale;
		out->m[1][1] = (cy) * scale;
		out->m[2][1] = 0;
		out->m[3][1] = y;
		out->m[0][2] = 0;
		out->m[1][2] = 0;
		out->m[2][2] = scale;
		out->m[3][2] = z;
		out->m[0][3] = 0;
		out->m[1][3] = 0;
		out->m[2][3] = 0;
		out->m[3][3] = 1;
#else
		out->m[0][0] = (cy) * scale;
		out->m[0][1] = (-sy) * scale;
		out->m[0][2] = 0;
		out->m[0][3] = x;
		out->m[1][0] = (sy) * scale;
		out->m[1][1] = (cy) * scale;
		out->m[1][2] = 0;
		out->m[1][3] = y;
		out->m[2][0] = 0;
		out->m[2][1] = 0;
		out->m[2][2] = scale;
		out->m[2][3] = z;
		out->m[3][0] = 0;
		out->m[3][1] = 0;
		out->m[3][2] = 0;
		out->m[3][3] = 1;
#endif
	} else {
#if MATRIX4x4_OPENGLORIENTATION
		out->m[0][0] = scale;
		out->m[1][0] = 0;
		out->m[2][0] = 0;
		out->m[3][0] = x;
		out->m[0][1] = 0;
		out->m[1][1] = scale;
		out->m[2][1] = 0;
		out->m[3][1] = y;
		out->m[0][2] = 0;
		out->m[1][2] = 0;
		out->m[2][2] = scale;
		out->m[3][2] = z;
		out->m[0][3] = 0;
		out->m[1][3] = 0;
		out->m[2][3] = 0;
		out->m[3][3] = 1;
#else
		out->m[0][0] = scale;
		out->m[0][1] = 0;
		out->m[0][2] = 0;
		out->m[0][3] = x;
		out->m[1][0] = 0;
		out->m[1][1] = scale;
		out->m[1][2] = 0;
		out->m[1][3] = y;
		out->m[2][0] = 0;
		out->m[2][1] = 0;
		out->m[2][2] = scale;
		out->m[2][3] = z;
		out->m[3][0] = 0;
		out->m[3][1] = 0;
		out->m[3][2] = 0;
		out->m[3][3] = 1;
#endif
	}
}

void Matrix4x4_CreateFromEntity(mat4_t *out, const vec3_t origin, const vec3_t angles, float scale) {
	return Matrix4x4_CreateFromQuakeEntity(out, origin.x, origin.y, origin.z,
										        angles.x, angles.y, angles.z,
										        scale);
}

void Matrix4x4_ToVectors(const mat4_t *in, float vx[3], float vy[3], float vz[3], float t[3]) {
#if MATRIX4x4_OPENGLORIENTATION
	if (vx) {
		vx[0] = in->m[0][0];
		vx[1] = in->m[0][1];
		vx[2] = in->m[0][2];
	}
	if (vy) {
		vy[0] = in->m[1][0];
		vy[1] = in->m[1][1];
		vy[2] = in->m[1][2];
	}
	if (vz) {
		vz[0] = in->m[2][0];
		vz[1] = in->m[2][1];
		vz[2] = in->m[2][2];
	}
	if (t) {
		t [0] = in->m[3][0];
		t [1] = in->m[3][1];
		t [2] = in->m[3][2];
	}
#else
	if (vx) {
		vx[0] = in->m[0][0];
		vx[1] = in->m[1][0];
		vx[2] = in->m[2][0];
	}
	if (vy) {
		vy[0] = in->m[0][1];
		vy[1] = in->m[1][1];
		vy[2] = in->m[2][1];
	}
	if (vz) {
		vz[0] = in->m[0][2];
		vz[1] = in->m[1][2];
		vz[2] = in->m[2][2];
	}
	if (t) {
		t [0] = in->m[0][3];
		t [1] = in->m[1][3];
		t [2] = in->m[2][3];
	}
#endif
}

void Matrix4x4_FromVectors(mat4_t *out, const float vx[3], const float vy[3], const float vz[3],
                           const float t[3]) {
#if MATRIX4x4_OPENGLORIENTATION
	out->m[0][0] = vx[0];
	out->m[1][0] = vy[0];
	out->m[2][0] = vz[0];
	out->m[3][0] = t[0];
	out->m[0][1] = vx[1];
	out->m[1][1] = vy[1];
	out->m[2][1] = vz[1];
	out->m[3][1] = t[1];
	out->m[0][2] = vx[2];
	out->m[1][2] = vy[2];
	out->m[2][2] = vz[2];
	out->m[3][2] = t[2];
	out->m[0][3] = 0.0f;
	out->m[1][3] = 0.0f;
	out->m[2][3] = 0.0f;
	out->m[3][3] = 1.0f;
#else
	out->m[0][0] = vx[0];
	out->m[0][1] = vy[0];
	out->m[0][2] = vz[0];
	out->m[0][3] = t[0];
	out->m[1][0] = vx[1];
	out->m[1][1] = vy[1];
	out->m[1][2] = vz[1];
	out->m[1][3] = t[1];
	out->m[2][0] = vx[2];
	out->m[2][1] = vy[2];
	out->m[2][2] = vz[2];
	out->m[2][3] = t[2];
	out->m[3][0] = 0.0f;
	out->m[3][1] = 0.0f;
	out->m[3][2] = 0.0f;
	out->m[3][3] = 1.0f;
#endif
}

void Matrix4x4_ToArrayDoubleGL(const mat4_t *in, double out[16]) {
#if MATRIX4x4_OPENGLORIENTATION
	out[ 0] = in->m[0][0];
	out[ 1] = in->m[0][1];
	out[ 2] = in->m[0][2];
	out[ 3] = in->m[0][3];
	out[ 4] = in->m[1][0];
	out[ 5] = in->m[1][1];
	out[ 6] = in->m[1][2];
	out[ 7] = in->m[1][3];
	out[ 8] = in->m[2][0];
	out[ 9] = in->m[2][1];
	out[10] = in->m[2][2];
	out[11] = in->m[2][3];
	out[12] = in->m[3][0];
	out[13] = in->m[3][1];
	out[14] = in->m[3][2];
	out[15] = in->m[3][3];
#else
	out[ 0] = in->m[0][0];
	out[ 1] = in->m[1][0];
	out[ 2] = in->m[2][0];
	out[ 3] = in->m[3][0];
	out[ 4] = in->m[0][1];
	out[ 5] = in->m[1][1];
	out[ 6] = in->m[2][1];
	out[ 7] = in->m[3][1];
	out[ 8] = in->m[0][2];
	out[ 9] = in->m[1][2];
	out[10] = in->m[2][2];
	out[11] = in->m[3][2];
	out[12] = in->m[0][3];
	out[13] = in->m[1][3];
	out[14] = in->m[2][3];
	out[15] = in->m[3][3];
#endif
}

void Matrix4x4_FromArrayDoubleGL (mat4_t *out, const double in[16]) {
#if MATRIX4x4_OPENGLORIENTATION
	out->m[0][0] = in[0];
	out->m[0][1] = in[1];
	out->m[0][2] = in[2];
	out->m[0][3] = in[3];
	out->m[1][0] = in[4];
	out->m[1][1] = in[5];
	out->m[1][2] = in[6];
	out->m[1][3] = in[7];
	out->m[2][0] = in[8];
	out->m[2][1] = in[9];
	out->m[2][2] = in[10];
	out->m[2][3] = in[11];
	out->m[3][0] = in[12];
	out->m[3][1] = in[13];
	out->m[3][2] = in[14];
	out->m[3][3] = in[15];
#else
	out->m[0][0] = in[0];
	out->m[1][0] = in[1];
	out->m[2][0] = in[2];
	out->m[3][0] = in[3];
	out->m[0][1] = in[4];
	out->m[1][1] = in[5];
	out->m[2][1] = in[6];
	out->m[3][1] = in[7];
	out->m[0][2] = in[8];
	out->m[1][2] = in[9];
	out->m[2][2] = in[10];
	out->m[3][2] = in[11];
	out->m[0][3] = in[12];
	out->m[1][3] = in[13];
	out->m[2][3] = in[14];
	out->m[3][3] = in[15];
#endif
}

void Matrix4x4_ToArrayDoubleD3D(const mat4_t *in, double out[16]) {
#if MATRIX4x4_OPENGLORIENTATION
	out[ 0] = in->m[0][0];
	out[ 1] = in->m[1][0];
	out[ 2] = in->m[2][0];
	out[ 3] = in->m[3][0];
	out[ 4] = in->m[0][1];
	out[ 5] = in->m[1][1];
	out[ 6] = in->m[2][1];
	out[ 7] = in->m[3][1];
	out[ 8] = in->m[0][2];
	out[ 9] = in->m[1][2];
	out[10] = in->m[2][2];
	out[11] = in->m[3][2];
	out[12] = in->m[0][3];
	out[13] = in->m[1][3];
	out[14] = in->m[2][3];
	out[15] = in->m[3][3];
#else
	out[ 0] = in->m[0][0];
	out[ 1] = in->m[0][1];
	out[ 2] = in->m[0][2];
	out[ 3] = in->m[0][3];
	out[ 4] = in->m[1][0];
	out[ 5] = in->m[1][1];
	out[ 6] = in->m[1][2];
	out[ 7] = in->m[1][3];
	out[ 8] = in->m[2][0];
	out[ 9] = in->m[2][1];
	out[10] = in->m[2][2];
	out[11] = in->m[2][3];
	out[12] = in->m[3][0];
	out[13] = in->m[3][1];
	out[14] = in->m[3][2];
	out[15] = in->m[3][3];
#endif
}

void Matrix4x4_FromArrayDoubleD3D (mat4_t *out, const double in[16]) {
#if MATRIX4x4_OPENGLORIENTATION
	out->m[0][0] = in[0];
	out->m[1][0] = in[1];
	out->m[2][0] = in[2];
	out->m[3][0] = in[3];
	out->m[0][1] = in[4];
	out->m[1][1] = in[5];
	out->m[2][1] = in[6];
	out->m[3][1] = in[7];
	out->m[0][2] = in[8];
	out->m[1][2] = in[9];
	out->m[2][2] = in[10];
	out->m[3][2] = in[11];
	out->m[0][3] = in[12];
	out->m[1][3] = in[13];
	out->m[2][3] = in[14];
	out->m[3][3] = in[15];
#else
	out->m[0][0] = in[0];
	out->m[0][1] = in[1];
	out->m[0][2] = in[2];
	out->m[0][3] = in[3];
	out->m[1][0] = in[4];
	out->m[1][1] = in[5];
	out->m[1][2] = in[6];
	out->m[1][3] = in[7];
	out->m[2][0] = in[8];
	out->m[2][1] = in[9];
	out->m[2][2] = in[10];
	out->m[2][3] = in[11];
	out->m[3][0] = in[12];
	out->m[3][1] = in[13];
	out->m[3][2] = in[14];
	out->m[3][3] = in[15];
#endif
}

void Matrix4x4_ToArrayFloatGL(const mat4_t *in, float out[16]) {
#if MATRIX4x4_OPENGLORIENTATION
	out[ 0] = in->m[0][0];
	out[ 1] = in->m[0][1];
	out[ 2] = in->m[0][2];
	out[ 3] = in->m[0][3];
	out[ 4] = in->m[1][0];
	out[ 5] = in->m[1][1];
	out[ 6] = in->m[1][2];
	out[ 7] = in->m[1][3];
	out[ 8] = in->m[2][0];
	out[ 9] = in->m[2][1];
	out[10] = in->m[2][2];
	out[11] = in->m[2][3];
	out[12] = in->m[3][0];
	out[13] = in->m[3][1];
	out[14] = in->m[3][2];
	out[15] = in->m[3][3];
#else
	out[ 0] = in->m[0][0];
	out[ 1] = in->m[1][0];
	out[ 2] = in->m[2][0];
	out[ 3] = in->m[3][0];
	out[ 4] = in->m[0][1];
	out[ 5] = in->m[1][1];
	out[ 6] = in->m[2][1];
	out[ 7] = in->m[3][1];
	out[ 8] = in->m[0][2];
	out[ 9] = in->m[1][2];
	out[10] = in->m[2][2];
	out[11] = in->m[3][2];
	out[12] = in->m[0][3];
	out[13] = in->m[1][3];
	out[14] = in->m[2][3];
	out[15] = in->m[3][3];
#endif
}

void Matrix4x4_FromArrayFloatGL (mat4_t *out, const float in[16]) {
#if MATRIX4x4_OPENGLORIENTATION
	out->m[0][0] = in[0];
	out->m[0][1] = in[1];
	out->m[0][2] = in[2];
	out->m[0][3] = in[3];
	out->m[1][0] = in[4];
	out->m[1][1] = in[5];
	out->m[1][2] = in[6];
	out->m[1][3] = in[7];
	out->m[2][0] = in[8];
	out->m[2][1] = in[9];
	out->m[2][2] = in[10];
	out->m[2][3] = in[11];
	out->m[3][0] = in[12];
	out->m[3][1] = in[13];
	out->m[3][2] = in[14];
	out->m[3][3] = in[15];
#else
	out->m[0][0] = in[0];
	out->m[1][0] = in[1];
	out->m[2][0] = in[2];
	out->m[3][0] = in[3];
	out->m[0][1] = in[4];
	out->m[1][1] = in[5];
	out->m[2][1] = in[6];
	out->m[3][1] = in[7];
	out->m[0][2] = in[8];
	out->m[1][2] = in[9];
	out->m[2][2] = in[10];
	out->m[3][2] = in[11];
	out->m[0][3] = in[12];
	out->m[1][3] = in[13];
	out->m[2][3] = in[14];
	out->m[3][3] = in[15];
#endif
}

void Matrix4x4_ToArrayFloatD3D(const mat4_t *in, float out[16]) {
#if MATRIX4x4_OPENGLORIENTATION
	out[ 0] = in->m[0][0];
	out[ 1] = in->m[1][0];
	out[ 2] = in->m[2][0];
	out[ 3] = in->m[3][0];
	out[ 4] = in->m[0][1];
	out[ 5] = in->m[1][1];
	out[ 6] = in->m[2][1];
	out[ 7] = in->m[3][1];
	out[ 8] = in->m[0][2];
	out[ 9] = in->m[1][2];
	out[10] = in->m[2][2];
	out[11] = in->m[3][2];
	out[12] = in->m[0][3];
	out[13] = in->m[1][3];
	out[14] = in->m[2][3];
	out[15] = in->m[3][3];
#else
	out[ 0] = in->m[0][0];
	out[ 1] = in->m[0][1];
	out[ 2] = in->m[0][2];
	out[ 3] = in->m[0][3];
	out[ 4] = in->m[1][0];
	out[ 5] = in->m[1][1];
	out[ 6] = in->m[1][2];
	out[ 7] = in->m[1][3];
	out[ 8] = in->m[2][0];
	out[ 9] = in->m[2][1];
	out[10] = in->m[2][2];
	out[11] = in->m[2][3];
	out[12] = in->m[3][0];
	out[13] = in->m[3][1];
	out[14] = in->m[3][2];
	out[15] = in->m[3][3];
#endif
}

void Matrix4x4_FromArrayFloatD3D (mat4_t *out, const float in[16]) {
#if MATRIX4x4_OPENGLORIENTATION
	out->m[0][0] = in[0];
	out->m[1][0] = in[1];
	out->m[2][0] = in[2];
	out->m[3][0] = in[3];
	out->m[0][1] = in[4];
	out->m[1][1] = in[5];
	out->m[2][1] = in[6];
	out->m[3][1] = in[7];
	out->m[0][2] = in[8];
	out->m[1][2] = in[9];
	out->m[2][2] = in[10];
	out->m[3][2] = in[11];
	out->m[0][3] = in[12];
	out->m[1][3] = in[13];
	out->m[2][3] = in[14];
	out->m[3][3] = in[15];
#else
	out->m[0][0] = in[0];
	out->m[0][1] = in[1];
	out->m[0][2] = in[2];
	out->m[0][3] = in[3];
	out->m[1][0] = in[4];
	out->m[1][1] = in[5];
	out->m[1][2] = in[6];
	out->m[1][3] = in[7];
	out->m[2][0] = in[8];
	out->m[2][1] = in[9];
	out->m[2][2] = in[10];
	out->m[2][3] = in[11];
	out->m[3][0] = in[12];
	out->m[3][1] = in[13];
	out->m[3][2] = in[14];
	out->m[3][3] = in[15];
#endif
}

void Matrix4x4_ToArray12FloatGL(const mat4_t *in, float out[12]) {
#if MATRIX4x4_OPENGLORIENTATION
	out[ 0] = in->m[0][0];
	out[ 1] = in->m[0][1];
	out[ 2] = in->m[0][2];
	out[ 3] = in->m[1][0];
	out[ 4] = in->m[1][1];
	out[ 5] = in->m[1][2];
	out[ 6] = in->m[2][0];
	out[ 7] = in->m[2][1];
	out[ 8] = in->m[2][2];
	out[ 9] = in->m[3][0];
	out[10] = in->m[3][1];
	out[11] = in->m[3][2];
#else
	out[ 0] = in->m[0][0];
	out[ 1] = in->m[1][0];
	out[ 2] = in->m[2][0];
	out[ 3] = in->m[0][1];
	out[ 4] = in->m[1][1];
	out[ 5] = in->m[2][1];
	out[ 6] = in->m[0][2];
	out[ 7] = in->m[1][2];
	out[ 8] = in->m[2][2];
	out[ 9] = in->m[0][3];
	out[10] = in->m[1][3];
	out[11] = in->m[2][3];
#endif
}

void Matrix4x4_FromArray12FloatGL(mat4_t *out, const float in[12]) {
#if MATRIX4x4_OPENGLORIENTATION
	out->m[0][0] = in[0];
	out->m[0][1] = in[1];
	out->m[0][2] = in[2];
	out->m[0][3] = 0;
	out->m[1][0] = in[3];
	out->m[1][1] = in[4];
	out->m[1][2] = in[5];
	out->m[1][3] = 0;
	out->m[2][0] = in[6];
	out->m[2][1] = in[7];
	out->m[2][2] = in[8];
	out->m[2][3] = 0;
	out->m[3][0] = in[9];
	out->m[3][1] = in[10];
	out->m[3][2] = in[11];
	out->m[3][3] = 1;
#else
	out->m[0][0] = in[0];
	out->m[1][0] = in[1];
	out->m[2][0] = in[2];
	out->m[3][0] = 0;
	out->m[0][1] = in[3];
	out->m[1][1] = in[4];
	out->m[2][1] = in[5];
	out->m[3][1] = 0;
	out->m[0][2] = in[6];
	out->m[1][2] = in[7];
	out->m[2][2] = in[8];
	out->m[3][2] = 0;
	out->m[0][3] = in[9];
	out->m[1][3] = in[10];
	out->m[2][3] = in[11];
	out->m[3][3] = 1;
#endif
}

void Matrix4x4_ToArray12FloatD3D(const mat4_t *in, float out[12]) {
#if MATRIX4x4_OPENGLORIENTATION
	out[ 0] = in->m[0][0];
	out[ 1] = in->m[1][0];
	out[ 2] = in->m[2][0];
	out[ 3] = in->m[3][0];
	out[ 4] = in->m[0][1];
	out[ 5] = in->m[1][1];
	out[ 6] = in->m[2][1];
	out[ 7] = in->m[3][1];
	out[ 8] = in->m[0][2];
	out[ 9] = in->m[1][2];
	out[10] = in->m[2][2];
	out[11] = in->m[3][2];
#else
	out[ 0] = in->m[0][0];
	out[ 1] = in->m[0][1];
	out[ 2] = in->m[0][2];
	out[ 3] = in->m[0][3];
	out[ 4] = in->m[1][0];
	out[ 5] = in->m[1][1];
	out[ 6] = in->m[1][2];
	out[ 7] = in->m[1][3];
	out[ 8] = in->m[2][0];
	out[ 9] = in->m[2][1];
	out[10] = in->m[2][2];
	out[11] = in->m[2][3];
#endif
}

void Matrix4x4_FromArray12FloatD3D(mat4_t *out, const float in[12]) {
#if MATRIX4x4_OPENGLORIENTATION
	out->m[0][0] = in[0];
	out->m[1][0] = in[1];
	out->m[2][0] = in[2];
	out->m[3][0] = in[3];
	out->m[0][1] = in[4];
	out->m[1][1] = in[5];
	out->m[2][1] = in[6];
	out->m[3][1] = in[7];
	out->m[0][2] = in[8];
	out->m[1][2] = in[9];
	out->m[2][2] = in[10];
	out->m[3][2] = in[11];
	out->m[0][3] = 0;
	out->m[1][3] = 0;
	out->m[2][3] = 0;
	out->m[3][3] = 1;
#else
	out->m[0][0] = in[0];
	out->m[0][1] = in[1];
	out->m[0][2] = in[2];
	out->m[0][3] = in[3];
	out->m[1][0] = in[4];
	out->m[1][1] = in[5];
	out->m[1][2] = in[6];
	out->m[1][3] = in[7];
	out->m[2][0] = in[8];
	out->m[2][1] = in[9];
	out->m[2][2] = in[10];
	out->m[2][3] = in[11];
	out->m[3][0] = 0;
	out->m[3][1] = 0;
	out->m[3][2] = 0;
	out->m[3][3] = 1;
#endif
}

void Matrix4x4_FromOriginQuat(mat4_t *m, double ox, double oy, double oz, double x, double y, double z, double w) {
#if MATRIX4x4_OPENGLORIENTATION
	m->m[0][0] = 1 - 2 * (y * y + z * z);
	m->m[1][0] =  2 * (x * y - z * w);
	m->m[2][0] =  2 * (x * z + y * w);
	m->m[3][0] = ox;
	m->m[0][1] =  2 * (x * y + z * w);
	m->m[1][1] = 1 - 2 * (x * x + z * z);
	m->m[2][1] =  2 * (y * z - x * w);
	m->m[3][1] = oy;
	m->m[0][2] =  2 * (x * z - y * w);
	m->m[1][2] =  2 * (y * z + x * w);
	m->m[2][2] = 1 - 2 * (x * x + y * y);
	m->m[3][2] = oz;
	m->m[0][3] =  0          ;
	m->m[1][3] =  0          ;
	m->m[2][3] =  0          ;
	m->m[3][3] = 1;
#else
	m->m[0][0] = 1 - 2 * (y * y + z * z);
	m->m[0][1] =  2 * (x * y - z * w);
	m->m[0][2] =  2 * (x * z + y * w);
	m->m[0][3] = ox;
	m->m[1][0] =  2 * (x * y + z * w);
	m->m[1][1] = 1 - 2 * (x * x + z * z);
	m->m[1][2] =  2 * (y * z - x * w);
	m->m[1][3] = oy;
	m->m[2][0] =  2 * (x * z - y * w);
	m->m[2][1] =  2 * (y * z + x * w);
	m->m[2][2] = 1 - 2 * (x * x + y * y);
	m->m[2][3] = oz;
	m->m[3][0] =  0          ;
	m->m[3][1] =  0          ;
	m->m[3][2] =  0          ;
	m->m[3][3] = 1;
#endif
}

// see http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/index.htm
void Matrix4x4_ToOrigin3Quat4Float(const mat4_t *m, float *origin, float *quat) {
	float s;
	quat[3] = sqrt(1.0f + m->m[0][0] + m->m[1][1] + m->m[2][2]) * 0.5f;
	s = 0.25f / quat[3];
#if MATRIX4x4_OPENGLORIENTATION
	origin[0] = m->m[3][0];
	origin[1] = m->m[3][1];
	origin[2] = m->m[3][2];
	quat[0] = (m->m[1][2] - m->m[2][1]) * s;
	quat[1] = (m->m[2][0] - m->m[0][2]) * s;
	quat[2] = (m->m[0][1] - m->m[1][0]) * s;
#else
	origin[0] = m->m[0][3];
	origin[1] = m->m[1][3];
	origin[2] = m->m[2][3];
	quat[0] = (m->m[2][1] - m->m[1][2]) * s;
	quat[1] = (m->m[0][2] - m->m[2][0]) * s;
	quat[2] = (m->m[1][0] - m->m[0][1]) * s;
#endif
}

// LordHavoc: I got this code from:
//http://www.doom3world.org/phpbb2/viewtopic.php?t=2884
void Matrix4x4_FromDoom3Joint(mat4_t *m, double ox, double oy, double oz, double x, double y, double z) {
	double w = 1.0f - (x * x + y * y + z * z);
	w = w > 0.0f ? -sqrt(w) : 0.0f;
#if MATRIX4x4_OPENGLORIENTATION
	m->m[0][0] = 1 - 2 * (y * y + z * z);
	m->m[1][0] =  2 * (x * y - z * w);
	m->m[2][0] =  2 * (x * z + y * w);
	m->m[3][0] = ox;
	m->m[0][1] =  2 * (x * y + z * w);
	m->m[1][1] = 1 - 2 * (x * x + z * z);
	m->m[2][1] =  2 * (y * z - x * w);
	m->m[3][1] = oy;
	m->m[0][2] =  2 * (x * z - y * w);
	m->m[1][2] =  2 * (y * z + x * w);
	m->m[2][2] = 1 - 2 * (x * x + y * y);
	m->m[3][2] = oz;
	m->m[0][3] =  0          ;
	m->m[1][3] =  0          ;
	m->m[2][3] =  0          ;
	m->m[3][3] = 1;
#else
	m->m[0][0] = 1 - 2 * (y * y + z * z);
	m->m[0][1] =  2 * (x * y - z * w);
	m->m[0][2] =  2 * (x * z + y * w);
	m->m[0][3] = ox;
	m->m[1][0] =  2 * (x * y + z * w);
	m->m[1][1] = 1 - 2 * (x * x + z * z);
	m->m[1][2] =  2 * (y * z - x * w);
	m->m[1][3] = oy;
	m->m[2][0] =  2 * (x * z - y * w);
	m->m[2][1] =  2 * (y * z + x * w);
	m->m[2][2] = 1 - 2 * (x * x + y * y);
	m->m[2][3] = oz;
	m->m[3][0] =  0          ;
	m->m[3][1] =  0          ;
	m->m[3][2] =  0          ;
	m->m[3][3] = 1;
#endif
}

void Matrix4x4_FromBonePose6s(mat4_t *m, float originscale, const int16_t *pose6s) {
	float origin[3];
	float quat[4];
	origin[0] = pose6s[0] * originscale;
	origin[1] = pose6s[1] * originscale;
	origin[2] = pose6s[2] * originscale;
	quat[0] = pose6s[3] * (1.0f / 32767.0f);
	quat[1] = pose6s[4] * (1.0f / 32767.0f);
	quat[2] = pose6s[5] * (1.0f / 32767.0f);
	quat[3] = 1.0f - (quat[0] * quat[0] + quat[1] * quat[1] + quat[2] * quat[2]);
	quat[3] = quat[3] > 0.0f ? -sqrt(quat[3]) : 0.0f;
	Matrix4x4_FromOriginQuat(m, origin[0], origin[1], origin[2], quat[0], quat[1], quat[2], quat[3]);
}

void Matrix4x4_ToBonePose6s(const mat4_t *m, float origininvscale, int16_t *pose6s) {
	float origin[3];
	float quat[4];
	float s;
	Matrix4x4_ToOrigin3Quat4Float(m, origin, quat);
	// normalize quaternion so that it is unit length
	s = quat[0] * quat[0] + quat[1] * quat[1] + quat[2] * quat[2] + quat[3] * quat[3];
	if (s) {
		s = 1.0f / sqrt(s);
		quat[0] *= s;
		quat[1] *= s;
		quat[2] *= s;
		quat[3] *= s;
	}
	// use a negative scale on the quat because the above function produces a
	// positive quat[3] and canonical quaternions have negative quat[3]
	pose6s[0] = origin[0] * origininvscale;
	pose6s[1] = origin[1] * origininvscale;
	pose6s[2] = origin[2] * origininvscale;
	pose6s[3] = quat[0] * -32767.0f;
	pose6s[4] = quat[1] * -32767.0f;
	pose6s[5] = quat[2] * -32767.0f;
}

void Matrix4x4_Blend (mat4_t *out, const mat4_t *in1, const mat4_t *in2, double blend) {
	double iblend = 1 - blend;
	out->m[0][0] = in1->m[0][0] * iblend + in2->m[0][0] * blend;
	out->m[0][1] = in1->m[0][1] * iblend + in2->m[0][1] * blend;
	out->m[0][2] = in1->m[0][2] * iblend + in2->m[0][2] * blend;
	out->m[0][3] = in1->m[0][3] * iblend + in2->m[0][3] * blend;
	out->m[1][0] = in1->m[1][0] * iblend + in2->m[1][0] * blend;
	out->m[1][1] = in1->m[1][1] * iblend + in2->m[1][1] * blend;
	out->m[1][2] = in1->m[1][2] * iblend + in2->m[1][2] * blend;
	out->m[1][3] = in1->m[1][3] * iblend + in2->m[1][3] * blend;
	out->m[2][0] = in1->m[2][0] * iblend + in2->m[2][0] * blend;
	out->m[2][1] = in1->m[2][1] * iblend + in2->m[2][1] * blend;
	out->m[2][2] = in1->m[2][2] * iblend + in2->m[2][2] * blend;
	out->m[2][3] = in1->m[2][3] * iblend + in2->m[2][3] * blend;
	out->m[3][0] = in1->m[3][0] * iblend + in2->m[3][0] * blend;
	out->m[3][1] = in1->m[3][1] * iblend + in2->m[3][1] * blend;
	out->m[3][2] = in1->m[3][2] * iblend + in2->m[3][2] * blend;
	out->m[3][3] = in1->m[3][3] * iblend + in2->m[3][3] * blend;
}

void Matrix4x4_Transform2 (const mat4_t *in, const float v[2], float out[2]) {
	const float v0 = v[0];
	const float v1 = v[1];

#if MATRIX4x4_OPENGLORIENTATION
	out[0] = v0 * in->m[0][0] + v1 * in->m[1][0] + in->m[3][0];
	out[1] = v0 * in->m[0][1] + v1 * in->m[1][1] + in->m[3][1];
#else
	out[0] = v0 * in->m[0][0] + v1 * in->m[0][1] + in->m[0][3];
	out[1] = v0 * in->m[1][0] + v1 * in->m[1][1] + in->m[1][3];
#endif
}

void Matrix4x4_Transform (const mat4_t *in, const float v[3], float out[3]) {
#if MATRIX4x4_OPENGLORIENTATION
	out[0] = v[0] * in->m[0][0] + v[1] * in->m[1][0] + v[2] * in->m[2][0] + in->m[3][0];
	out[1] = v[0] * in->m[0][1] + v[1] * in->m[1][1] + v[2] * in->m[2][1] + in->m[3][1];
	out[2] = v[0] * in->m[0][2] + v[1] * in->m[1][2] + v[2] * in->m[2][2] + in->m[3][2];
#else
	out[0] = v[0] * in->m[0][0] + v[1] * in->m[0][1] + v[2] * in->m[0][2] + in->m[0][3];
	out[1] = v[0] * in->m[1][0] + v[1] * in->m[1][1] + v[2] * in->m[1][2] + in->m[1][3];
	out[2] = v[0] * in->m[2][0] + v[1] * in->m[2][1] + v[2] * in->m[2][2] + in->m[2][3];
#endif
}

void Matrix4x4_Transform4 (const mat4_t *in, const float v[4], float out[4]) {
#if MATRIX4x4_OPENGLORIENTATION
	out[0] = v[0] * in->m[0][0] + v[1] * in->m[1][0] + v[2] * in->m[2][0] + in->m[3][0];
	out[1] = v[0] * in->m[0][1] + v[1] * in->m[1][1] + v[2] * in->m[2][1] + in->m[3][1];
	out[2] = v[0] * in->m[0][2] + v[1] * in->m[1][2] + v[2] * in->m[2][2] + in->m[3][2];
	out[3] = v[0] * in->m[0][3] + v[1] * in->m[1][3] + v[2] * in->m[2][3] + v[3] * in->m[3][3];
#else
	out[0] = v[0] * in->m[0][0] + v[1] * in->m[0][1] + v[2] * in->m[0][2] + v[3] * in->m[0][3];
	out[1] = v[0] * in->m[1][0] + v[1] * in->m[1][1] + v[2] * in->m[1][2] + v[3] * in->m[1][3];
	out[2] = v[0] * in->m[2][0] + v[1] * in->m[2][1] + v[2] * in->m[2][2] + v[3] * in->m[2][3];
	out[3] = v[0] * in->m[3][0] + v[1] * in->m[3][1] + v[2] * in->m[3][2] + v[3] * in->m[3][3];
#endif
}

void Matrix4x4_Transform3x3 (const mat4_t *in, const float v[3], float out[3]) {
#if MATRIX4x4_OPENGLORIENTATION
	out[0] = v[0] * in->m[0][0] + v[1] * in->m[1][0] + v[2] * in->m[2][0];
	out[1] = v[0] * in->m[0][1] + v[1] * in->m[1][1] + v[2] * in->m[2][1];
	out[2] = v[0] * in->m[0][2] + v[1] * in->m[1][2] + v[2] * in->m[2][2];
#else
	out[0] = v[0] * in->m[0][0] + v[1] * in->m[0][1] + v[2] * in->m[0][2];
	out[1] = v[0] * in->m[1][0] + v[1] * in->m[1][1] + v[2] * in->m[1][2];
	out[2] = v[0] * in->m[2][0] + v[1] * in->m[2][1] + v[2] * in->m[2][2];
#endif
}

void Matrix4x4_TransformPositivePlane(const mat4_t *in, float x, float y, float z, float d, float *o) {
	float scale = sqrt(in->m[0][0] * in->m[0][0] + in->m[0][1] * in->m[0][1] + in->m[0][2] * in->m[0][2]);
	float iscale = 1.0f / scale;
#if MATRIX4x4_OPENGLORIENTATION
	o[0] = (x * in->m[0][0] + y * in->m[1][0] + z * in->m[2][0]) * iscale;
	o[1] = (x * in->m[0][1] + y * in->m[1][1] + z * in->m[2][1]) * iscale;
	o[2] = (x * in->m[0][2] + y * in->m[1][2] + z * in->m[2][2]) * iscale;
	o[3] = d * scale + (o[0] * in->m[3][0] + o[1] * in->m[3][1] + o[2] * in->m[3][2]);
#else
	o[0] = (x * in->m[0][0] + y * in->m[0][1] + z * in->m[0][2]) * iscale;
	o[1] = (x * in->m[1][0] + y * in->m[1][1] + z * in->m[1][2]) * iscale;
	o[2] = (x * in->m[2][0] + y * in->m[2][1] + z * in->m[2][2]) * iscale;
	o[3] = d * scale + (o[0] * in->m[0][3] + o[1] * in->m[1][3] + o[2] * in->m[2][3]);
#endif
}

void Matrix4x4_TransformStandardPlane(const mat4_t *in, float x, float y, float z, float d, float *o) {
	float scale = sqrt(in->m[0][0] * in->m[0][0] + in->m[0][1] * in->m[0][1] + in->m[0][2] * in->m[0][2]);
	float iscale = 1.0f / scale;
#if MATRIX4x4_OPENGLORIENTATION
	o[0] = (x * in->m[0][0] + y * in->m[1][0] + z * in->m[2][0]) * iscale;
	o[1] = (x * in->m[0][1] + y * in->m[1][1] + z * in->m[2][1]) * iscale;
	o[2] = (x * in->m[0][2] + y * in->m[1][2] + z * in->m[2][2]) * iscale;
	o[3] = d * scale - (o[0] * in->m[3][0] + o[1] * in->m[3][1] + o[2] * in->m[3][2]);
#else
	o[0] = (x * in->m[0][0] + y * in->m[0][1] + z * in->m[0][2]) * iscale;
	o[1] = (x * in->m[1][0] + y * in->m[1][1] + z * in->m[1][2]) * iscale;
	o[2] = (x * in->m[2][0] + y * in->m[2][1] + z * in->m[2][2]) * iscale;
	o[3] = d * scale - (o[0] * in->m[0][3] + o[1] * in->m[1][3] + o[2] * in->m[2][3]);
#endif
}

void Matrix4x4_TransformQuakePlane(const mat4_t *in, const vec3_t n, float d, vec4_t *out) {
	Matrix4x4_TransformPositivePlane(in, n.x, n.y, n.z, d, out->xyzw);
}

/*
void Matrix4x4_SimpleUntransform (const mat4_t *in, const float v[3], float out[3])
{
	double t[3];
#if MATRIX4x4_OPENGLORIENTATION
	t[0] = v[0] - in->m[3][0];
	t[1] = v[1] - in->m[3][1];
	t[2] = v[2] - in->m[3][2];
	out[0] = t[0] * in->m[0][0] + t[1] * in->m[0][1] + t[2] * in->m[0][2];
	out[1] = t[0] * in->m[1][0] + t[1] * in->m[1][1] + t[2] * in->m[1][2];
	out[2] = t[0] * in->m[2][0] + t[1] * in->m[2][1] + t[2] * in->m[2][2];
#else
	t[0] = v[0] - in->m[0][3];
	t[1] = v[1] - in->m[1][3];
	t[2] = v[2] - in->m[2][3];
	out[0] = t[0] * in->m[0][0] + t[1] * in->m[1][0] + t[2] * in->m[2][0];
	out[1] = t[0] * in->m[0][1] + t[1] * in->m[1][1] + t[2] * in->m[2][1];
	out[2] = t[0] * in->m[0][2] + t[1] * in->m[1][2] + t[2] * in->m[2][2];
#endif
}
*/

// FIXME: optimize
void Matrix4x4_ConcatTranslate (mat4_t *out, double x, double y, double z) {
	mat4_t base, temp;
	base = *out;
	Matrix4x4_CreateTranslate(&temp, x, y, z);
	Matrix4x4_Concat(out, &base, &temp);
}

// FIXME: optimize
void Matrix4x4_ConcatRotate (mat4_t *out, double angle, double x, double y, double z) {
	mat4_t base, temp;
	base = *out;
	Matrix4x4_CreateRotate(&temp, angle, x, y, z);
	Matrix4x4_Concat(out, &base, &temp);
}

// FIXME: optimize
void Matrix4x4_ConcatScale (mat4_t *out, double x) {
	mat4_t base, temp;
	base = *out;
	Matrix4x4_CreateScale(&temp, x);
	Matrix4x4_Concat(out, &base, &temp);
}

// FIXME: optimize
void Matrix4x4_ConcatScale3 (mat4_t *out, double x, double y, double z) {
	mat4_t base, temp;
	base = *out;
	Matrix4x4_CreateScale3(&temp, x, y, z);
	Matrix4x4_Concat(out, &base, &temp);
}

void Matrix4x4_OriginFromMatrix (const mat4_t *in, float *out) {
#if MATRIX4x4_OPENGLORIENTATION
	out[0] = in->m[3][0];
	out[1] = in->m[3][1];
	out[2] = in->m[3][2];
#else
	out[0] = in->m[0][3];
	out[1] = in->m[1][3];
	out[2] = in->m[2][3];
#endif
}

double Matrix4x4_ScaleFromMatrix (const mat4_t *in) {
	// we only support uniform scaling, so assume the first row is enough
	return sqrt(in->m[0][0] * in->m[0][0] + in->m[0][1] * in->m[0][1] + in->m[0][2] * in->m[0][2]);
}

void Matrix4x4_SetOrigin (mat4_t *out, double x, double y, double z) {
#if MATRIX4x4_OPENGLORIENTATION
	out->m[3][0] = x;
	out->m[3][1] = y;
	out->m[3][2] = z;
#else
	out->m[0][3] = x;
	out->m[1][3] = y;
	out->m[2][3] = z;
#endif
}

void Matrix4x4_AdjustOrigin (mat4_t *out, double x, double y, double z) {
#if MATRIX4x4_OPENGLORIENTATION
	out->m[3][0] += x;
	out->m[3][1] += y;
	out->m[3][2] += z;
#else
	out->m[0][3] += x;
	out->m[1][3] += y;
	out->m[2][3] += z;
#endif
}

void Matrix4x4_Scale (mat4_t *out, double rotatescale, double originscale) {
	out->m[0][0] *= rotatescale;
	out->m[0][1] *= rotatescale;
	out->m[0][2] *= rotatescale;
	out->m[1][0] *= rotatescale;
	out->m[1][1] *= rotatescale;
	out->m[1][2] *= rotatescale;
	out->m[2][0] *= rotatescale;
	out->m[2][1] *= rotatescale;
	out->m[2][2] *= rotatescale;
#if MATRIX4x4_OPENGLORIENTATION
	out->m[3][0] *= originscale;
	out->m[3][1] *= originscale;
	out->m[3][2] *= originscale;
#else
	out->m[0][3] *= originscale;
	out->m[1][3] *= originscale;
	out->m[2][3] *= originscale;
#endif
}

void Matrix4x4_Abs (mat4_t *out) {
	out->m[0][0] = fabs(out->m[0][0]);
	out->m[0][1] = fabs(out->m[0][1]);
	out->m[0][2] = fabs(out->m[0][2]);
	out->m[1][0] = fabs(out->m[1][0]);
	out->m[1][1] = fabs(out->m[1][1]);
	out->m[1][2] = fabs(out->m[1][2]);
	out->m[2][0] = fabs(out->m[2][0]);
	out->m[2][1] = fabs(out->m[2][1]);
	out->m[2][2] = fabs(out->m[2][2]);
}

// dutifully stolen from https://github.com/toji/gl-matrix/blob/master/src/gl-matrix/mat4.js
void Matrix4x4_FromFrustum (mat4_t *out, double left, double right, double bottom, double top, double nearval,
                            double farval) {
	double rl = 1.0 / (right - left);
	double tb = 1.0 / (top - bottom);
	double nf = 1.0 / (nearval - farval);
	double n2 = (nearval * 2.0);

	out->m[0][0] = n2 * rl;
	out->m[0][1] = 0.0;
	out->m[0][2] = 0.0;
	out->m[0][3] = 0.0;
	out->m[1][0] = 0.0;
	out->m[1][1] = n2 * tb;
	out->m[1][2] = 0.0;
	out->m[1][3] = 0.0;
	out->m[2][0] = (right + left) * rl;
	out->m[2][1] = (top + bottom) * tb;
	out->m[2][2] = (farval + nearval) * nf;
	out->m[2][3] = -1.0;
	out->m[3][0] = 0.0;
	out->m[3][1] = 0.0;
	out->m[3][2] = (farval * n2) * nf;
	out->m[3][3] = 0.0;
}

// dutifully stolen from https://github.com/toji/gl-matrix/blob/master/src/gl-matrix/mat4.js
void Matrix4x4_FromOrtho (mat4_t *out, double left, double right, double bottom, double top, double nearval,
                          double farval) {
	double lr = 1.0 / (left - right);
	double bt = 1.0 / (bottom - top);
	double nf = 1.0 / (nearval - farval);

	out->m[0][0] = -2.0 * lr;
	out->m[0][1] = 0.0;
	out->m[0][2] = 0.0;
	out->m[0][3] = 0.0;
	out->m[1][0] = 0.0;
	out->m[1][1] = -2.0 * bt;
	out->m[1][2] = 0.0;
	out->m[1][3] = 0.0;
	out->m[2][0] = 0.0;
	out->m[2][1] = 0.0;
	out->m[2][2] = 2.0 * nf;
	out->m[2][3] = 0.0;
	out->m[3][0] = (left + right) * lr;
	out->m[3][1] = (top + bottom) * bt;
	out->m[3][2] = (farval + nearval) * nf;
	out->m[3][3] = 1.0;
}
