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

#pragma once

#include "vector.h"

typedef struct matrix4x4_s {
	float m[4][4];
} mat4_t;

extern const mat4_t matrix4x4_identity;

// functions for manipulating 4x4 matrices

// copy a matrix4x4
void Matrix4x4_Copy (mat4_t *out, const mat4_t *in);
// multiply two matrix4x4 together, combining their transformations
// (warning: order matters - Concat(a, b, c) != Concat(a, c, b))
void Matrix4x4_Concat (mat4_t *out, const mat4_t *in1, const mat4_t *in2);
// creates a matrix that does the opposite of the matrix provided
// this is a full matrix inverter, it should be able to invert any matrix that
// is possible to invert
// (non-uniform scaling, rotation, shearing, and translation, possibly others)
// warning: this function is SLOW
int32_t Matrix4x4_Invert_Full (mat4_t *out, const mat4_t *in1);
// creates a matrix that does the opposite of the matrix provided
// only supports translate, rotate, scale (not scale3) matrices
void Matrix4x4_Invert_Simple (mat4_t *out, const mat4_t *in1);
// blends between two matrices, used primarily for animation interpolation
// (note: it is recommended to follow this with Matrix4x4_Normalize, a method
//  known as nlerp rotation, often better for animation purposes than slerp)
void Matrix4x4_Interpolate (mat4_t *out, const mat4_t *in1, const mat4_t *in2, double frac);
// creates a matrix that does the same rotation and translation as the matrix
// provided, but no uniform scaling, does not support scale3 matrices
void Matrix4x4_Normalize (mat4_t *out, const mat4_t *in1);

// creates an identity matrix
// (a matrix which does nothing)
void Matrix4x4_CreateIdentity (mat4_t *out);
// creates a translate matrix
// (moves vectors)
void Matrix4x4_CreateTranslate (mat4_t *out, double x, double y, double z);
// creates a rotate matrix
// (rotates vectors)
void Matrix4x4_CreateRotate (mat4_t *out, double angle, double x, double y, double z);
// creates a scaling matrix
// (expands or contracts vectors)
// (warning: do not apply this kind of matrix to direction vectors)
void Matrix4x4_CreateScale (mat4_t *out, double x);
// creates a squishing matrix
// (expands or contracts vectors differently in different axis)
// (warning: this is not reversed by Invert_Simple)
// (warning: do not apply this kind of matrix to direction vectors)
void Matrix4x4_CreateScale3 (mat4_t *out, double x, double y, double z);
// creates a matrix for a quake entity
void Matrix4x4_CreateFromQuakeEntity(mat4_t *out, double x, double y, double z, double pitch, double yaw,
                                     double roll, double scale);

void Matrix4x4_CreateFromEntity(mat4_t *out, const vec3_t origin, const vec3_t angles, float scale);

// creates a matrix4x4 from a float[16] array in the OpenGL orientation
void Matrix4x4_FromArrayFloatGL(mat4_t *out, const float in[16]);

// converts a matrix4x4 to a set of 3D vectors for the 3 axial directions, and the translate
void Matrix4x4_ToVectors(const mat4_t *in, float vx[3], float vy[3], float vz[3], float t[3]);
// creates a matrix4x4 from a set of 3D vectors for axial directions, and translate
void Matrix4x4_FromVectors(mat4_t *out, const float vx[3], const float vy[3], const float vz[3], const float t[3]);

// transforms a 3D vector through a matrix4x4
void Matrix4x4_Transform (const mat4_t *in, const float v[3], float out[3]);
// transforms a positive distance plane (A*x+B*y+C*z-D=0) through a rotation or translation matrix
void Matrix4x4_TransformPositivePlane (const mat4_t *in, float x, float y, float z, float d, float *o);
// transforms a Quake plane through a rotation or translation matrix
void Matrix4x4_TransformQuakePlane(const mat4_t *in, const vec3_t n, float d, vec4_t *out);

// ease of use functions
// immediately applies a Translate to the matrix
void Matrix4x4_ConcatTranslate (mat4_t *out, double x, double y, double z);
// immediately applies a Rotate to the matrix
void Matrix4x4_ConcatRotate (mat4_t *out, double angle, double x, double y, double z);
// immediately applies a Scale to the matrix
void Matrix4x4_ConcatScale (mat4_t *out, double x);
// immediately applies a Scale3 to the matrix
void Matrix4x4_ConcatScale3 (mat4_t *out, double x, double y, double z);

// extracts scaling factor from matrix (only works for uniform scaling)
double Matrix4x4_ScaleFromMatrix (const mat4_t *in);

// scales vectors of a matrix in place and allows you to scale origin as well
void Matrix4x4_Scale (mat4_t *out, double rotatescale, double originscale);

// generate a perspective transformation matrix from bounds
void Matrix4x4_FromFrustum (mat4_t *out, double left, double right, double bottom, double top, double nearval,
                            double farval);
// generate an orthogonal projection matrix from bounds
void Matrix4x4_FromOrtho (mat4_t *out, double left, double right, double bottom, double top, double nearval,
                          double farval);
