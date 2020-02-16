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

#define MATRIX4x4_OPENGLORIENTATION true

typedef struct matrix4x4_s {
	float m[4][4];
} mat4_t;

extern const mat4_t matrix4x4_identity;

// functions for manipulating 4x4 matrices

// copy a matrix4x4
void Matrix4x4_Copy (mat4_t *out, const mat4_t *in);
// copy only the rotation portion of a matrix4x4
void Matrix4x4_CopyRotateOnly (mat4_t *out, const mat4_t *in);
// copy only the translate portion of a matrix4x4
void Matrix4x4_CopyTranslateOnly (mat4_t *out, const mat4_t *in);
// multiply two matrix4x4 together, combining their transformations
// (warning: order matters - Concat(a, b, c) != Concat(a, c, b))
void Matrix4x4_Concat (mat4_t *out, const mat4_t *in1, const mat4_t *in2);
// swaps the rows and columns of the matrix
// (is this useful for anything?)
void Matrix4x4_Transpose (mat4_t *out, const mat4_t *in1);
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
// zeros all matrix components, used with Matrix4x4_Accumulate
void Matrix4x4_Clear (mat4_t *out);
// adds a weighted contribution from the supplied matrix, used to blend 3 or
// more matrices with weighting, it is recommended that Matrix4x4_Normalize be
// called afterward (a method known as nlerp rotation, often better for
// animation purposes than slerp)
void Matrix4x4_Accumulate (mat4_t *out, const mat4_t *in, double weight);
// creates a matrix that does the same rotation and translation as the matrix
// provided, but no uniform scaling, does not support scale3 matrices
void Matrix4x4_Normalize (mat4_t *out, const mat4_t *in1);
// creates a matrix with vectors normalized individually (use after
// Matrix4x4_Accumulate)
void Matrix4x4_Normalize3 (mat4_t *out, const mat4_t *in1);
// modifies a matrix to have all vectors and origin reflected across the plane
// to the opposite side (at least if axisscale is -2)
void Matrix4x4_Reflect (mat4_t *out, double normalx, double normaly, double normalz, double dist,
                        double axisscale);

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

// converts a matrix4x4 to a set of 3D vectors for the 3 axial directions, and the translate
void Matrix4x4_ToVectors(const mat4_t *in, float vx[3], float vy[3], float vz[3], float t[3]);
// creates a matrix4x4 from a set of 3D vectors for axial directions, and translate
void Matrix4x4_FromVectors(mat4_t *out, const float vx[3], const float vy[3], const float vz[3], const float t[3]);

// converts a matrix4x4 to a double[16] array in the OpenGL orientation
void Matrix4x4_ToArrayDoubleGL(const mat4_t *in, double out[16]);
// creates a matrix4x4 from a double[16] array in the OpenGL orientation
void Matrix4x4_FromArrayDoubleGL(mat4_t *out, const double in[16]);
// converts a matrix4x4 to a double[16] array in the Direct3D orientation
void Matrix4x4_ToArrayDoubleD3D(const mat4_t *in, double out[16]);
// creates a matrix4x4 from a double[16] array in the Direct3D orientation
void Matrix4x4_FromArrayDoubleD3D(mat4_t *out, const double in[16]);

// converts a matrix4x4 to a float[16] array in the OpenGL orientation
void Matrix4x4_ToArrayFloatGL(const mat4_t *in, float out[16]);
// creates a matrix4x4 from a float[16] array in the OpenGL orientation
void Matrix4x4_FromArrayFloatGL(mat4_t *out, const float in[16]);
// converts a matrix4x4 to a float[16] array in the Direct3D orientation
void Matrix4x4_ToArrayFloatD3D(const mat4_t *in, float out[16]);
// creates a matrix4x4 from a float[16] array in the Direct3D orientation
void Matrix4x4_FromArrayFloatD3D(mat4_t *out, const float in[16]);

// converts a matrix4x4 to a float[12] array in the OpenGL orientation
void Matrix4x4_ToArray12FloatGL(const mat4_t *in, float out[12]);
// creates a matrix4x4 from a float[12] array in the OpenGL orientation
void Matrix4x4_FromArray12FloatGL(mat4_t *out, const float in[12]);
// converts a matrix4x4 to a float[12] array in the Direct3D orientation
void Matrix4x4_ToArray12FloatD3D(const mat4_t *in, float out[12]);
// creates a matrix4x4 from a float[12] array in the Direct3D orientation
void Matrix4x4_FromArray12FloatD3D(mat4_t *out, const float in[12]);

// creates a matrix4x4 from an origin and quaternion (used mostly with skeletal model formats such as PSK)
void Matrix4x4_FromOriginQuat(mat4_t *m, double ox, double oy, double oz, double x, double y, double z, double w);
// creates an origin and quaternion from a mat4_t, quat[3] is always positive
void Matrix4x4_ToOrigin3Quat4Float(const mat4_t *m, float *origin, float *quat);
// creates a matrix4x4 from an origin and canonical unit-length quaternion (used mostly with skeletal model formats such as MD5)
void Matrix4x4_FromDoom3Joint(mat4_t *m, double ox, double oy, double oz, double x, double y, double z);

// creates a mat4_t from an origin and canonical unit-length quaternion in short[6] normalized format
void Matrix4x4_FromBonePose6s(mat4_t *m, float originscale, const int16_t *pose6s);
// creates a short[6] representation from normalized mat4_t
void Matrix4x4_ToBonePose6s(const mat4_t *m, float origininvscale, int16_t *pose6s);

// blends two matrices together, at a given percentage (blend controls percentage of in2)
void Matrix4x4_Blend (mat4_t *out, const mat4_t *in1, const mat4_t *in2, double blend);

// transforms a 2D vector through a matrix4x4
void Matrix4x4_Transform2 (const mat4_t *in, const float v[2], float out[2]);

// transforms a 3D vector through a matrix4x4
void Matrix4x4_Transform (const mat4_t *in, const float v[3], float out[3]);
// transforms a 4D vector through a matrix4x4
// (warning: if you don't know why you would need this, you don't need it)
// (warning: the 4th component of the vector should be 1.0)
void Matrix4x4_Transform4 (const mat4_t *in, const float v[4], float out[4]);
// reverse transforms a 3D vector through a matrix4x4, at least for *simple*
// cases (rotation and translation *ONLY*), this attempts to undo the results
// of Transform
//void Matrix4x4_SimpleUntransform (const mat4_t *in, const float v[3], float out[3]);
// transforms a direction vector through the rotation part of a matrix
void Matrix4x4_Transform3x3 (const mat4_t *in, const float v[3], float out[3]);
// transforms a positive distance plane (A*x+B*y+C*z-D=0) through a rotation or translation matrix
void Matrix4x4_TransformPositivePlane (const mat4_t *in, float x, float y, float z, float d, float *o);
// transforms a standard plane (A*x+B*y+C*z+D=0) through a rotation or translation matrix
void Matrix4x4_TransformStandardPlane (const mat4_t *in, float x, float y, float z, float d, float *o);
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

// extracts origin vector (translate) from matrix
void Matrix4x4_OriginFromMatrix (const mat4_t *in, float *out);
// extracts scaling factor from matrix (only works for uniform scaling)
double Matrix4x4_ScaleFromMatrix (const mat4_t *in);

// replaces origin vector (translate) in matrix
void Matrix4x4_SetOrigin (mat4_t *out, double x, double y, double z);
// moves origin vector (translate) in matrix by a simple translate
void Matrix4x4_AdjustOrigin (mat4_t *out, double x, double y, double z);
// scales vectors of a matrix in place and allows you to scale origin as well
void Matrix4x4_Scale (mat4_t *out, double rotatescale, double originscale);
// ensures each element of the 3x3 rotation matrix is facing in the + direction
void Matrix4x4_Abs (mat4_t *out);

// generate a perspective transformation matrix from bounds
void Matrix4x4_FromFrustum (mat4_t *out, double left, double right, double bottom, double top, double nearval,
                            double farval);
// generate an orthogonal projection matrix from bounds
void Matrix4x4_FromOrtho (mat4_t *out, double left, double right, double bottom, double top, double nearval,
                          double farval);
