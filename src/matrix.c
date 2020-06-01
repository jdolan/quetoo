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

void Matrix4x4_FromArrayFloatGL (mat4_t *out, const float in[16]) {

	memcpy(out, in, sizeof(mat4_t));
}

void Matrix4x4_Copy (mat4_t *out, const mat4_t *in) {
	*out = *in;
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
}

// Adapted from code contributed to Mesa by David Moore (Mesa 7.6 under SGI Free License B - which is MIT/X11-type)
int32_t Matrix4x4_Invert_Full (mat4_t *out, const mat4_t *in1) {
	mat4_t temp;
	double det;
	int32_t i, j;

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

	// invert the translate
	out->m[3][0] = -(in1->m[3][0] * out->m[0][0] + in1->m[3][1] * out->m[1][0] + in1->m[3][2] * out->m[2][0]);
	out->m[3][1] = -(in1->m[3][0] * out->m[0][1] + in1->m[3][1] * out->m[1][1] + in1->m[3][2] * out->m[2][1]);
	out->m[3][2] = -(in1->m[3][0] * out->m[0][2] + in1->m[3][1] * out->m[1][2] + in1->m[3][2] * out->m[2][2]);

	// don't know if there's anything worth doing here
	out->m[0][3] = 0;
	out->m[1][3] = 0;
	out->m[2][3] = 0;
	out->m[3][3] = 1;
}

void Matrix4x4_Interpolate (mat4_t *out, const mat4_t *in1, const mat4_t *in2, double frac) {
	int32_t i, j;
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++) {
			out->m[i][j] = in1->m[i][j] + frac * (in2->m[i][j] - in1->m[i][j]);
		}
}

void Matrix4x4_Normalize (mat4_t *out, const mat4_t *in1) {
	// scale rotation matrix vectors to a length of 1
	// note: this is only designed to undo uniform scaling
	double scale = 1.0 / sqrt(in1->m[0][0] * in1->m[0][0] + in1->m[0][1] * in1->m[0][1] + in1->m[0][2] * in1->m[0][2]);
	*out = *in1;
	Matrix4x4_Scale(out, scale, 1);
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
	} else if (pitch) {
		angle = yaw * (M_PI * 2 / 360);
		sy = sin(angle);
		cy = cos(angle);
		angle = pitch * (M_PI * 2 / 360);
		sp = sin(angle);
		cp = cos(angle);
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
	} else if (yaw) {
		angle = yaw * (M_PI * 2 / 360);
		sy = sin(angle);
		cy = cos(angle);
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
	} else {
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
	}
}

void Matrix4x4_CreateFromEntity(mat4_t *out, const vec3_t origin, const vec3_t angles, float scale) {
	Matrix4x4_CreateFromQuakeEntity(out, origin.x, origin.y, origin.z,
										 angles.x, angles.y, angles.z,
										 scale);
}

void Matrix4x4_ToVectors(const mat4_t *in, float vx[3], float vy[3], float vz[3], float t[3]) {
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
}

void Matrix4x4_FromVectors(mat4_t *out, const float vx[3], const float vy[3], const float vz[3],
                           const float t[3]) {
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
}

void Matrix4x4_Transform (const mat4_t *in, const float v[3], float out[3]) {
	out[0] = v[0] * in->m[0][0] + v[1] * in->m[1][0] + v[2] * in->m[2][0] + in->m[3][0];
	out[1] = v[0] * in->m[0][1] + v[1] * in->m[1][1] + v[2] * in->m[2][1] + in->m[3][1];
	out[2] = v[0] * in->m[0][2] + v[1] * in->m[1][2] + v[2] * in->m[2][2] + in->m[3][2];
}

void Matrix4x4_TransformPositivePlane(const mat4_t *in, float x, float y, float z, float d, float *o) {
	float scale = sqrt(in->m[0][0] * in->m[0][0] + in->m[0][1] * in->m[0][1] + in->m[0][2] * in->m[0][2]);
	float iscale = 1.0f / scale;
	o[0] = (x * in->m[0][0] + y * in->m[1][0] + z * in->m[2][0]) * iscale;
	o[1] = (x * in->m[0][1] + y * in->m[1][1] + z * in->m[2][1]) * iscale;
	o[2] = (x * in->m[0][2] + y * in->m[1][2] + z * in->m[2][2]) * iscale;
	o[3] = d * scale + (o[0] * in->m[3][0] + o[1] * in->m[3][1] + o[2] * in->m[3][2]);
}

void Matrix4x4_TransformQuakePlane(const mat4_t *in, const vec3_t n, float d, vec4_t *out) {
	Matrix4x4_TransformPositivePlane(in, n.x, n.y, n.z, d, out->xyzw);
}

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

double Matrix4x4_ScaleFromMatrix (const mat4_t *in) {
	// we only support uniform scaling, so assume the first row is enough
	return sqrt(in->m[0][0] * in->m[0][0] + in->m[0][1] * in->m[0][1] + in->m[0][2] * in->m[0][2]);
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
	out->m[3][0] *= originscale;
	out->m[3][1] *= originscale;
	out->m[3][2] *= originscale;
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
