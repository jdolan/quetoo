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

#ifndef _COMMON_GLSL_
#define _COMMON_GLSL_

/**
 * @brief Returns the maximum component of a vec2.
 */
float hmax(vec2 v) {
  return max(v.x, v.y);
}

/**
 * @brief Returns the maximum component of a vec3.
 */
float hmax(vec3 v) {
  return max(max(v.x, v.y), v.z);
}

/**
 * @brief Returns the maximum component of a vec4.
 */
float hmax(vec4 v) {
  return max(max(v.x, v.y), max(v.z, v.w));
}

/**
 * @brief Returns the minimum component of a vec2.
 */
float hmin(vec2 v) {
  return min(v.x, v.y);
}

/**
 * @brief Returns the minimum component of a vec3.
 */
float hmin(vec3 v) {
  return min(min(v.x, v.y), v.z);
}

/**
 * @brief Returns the minimum component of a vec4.
 */
float hmin(vec4 v) {
  return min(min(v.x, v.y), min(v.z, v.w));
}

/**
 * @brief Returns 1.0 - v.
 */
vec3 invert(vec3 v) {
  return vec3(1.0) - v;
}

/**
 * @brief Clamps x to [0.0, 1.0].
 */
float saturate(float x) {
  return clamp(x, 0.0, 1.0);
}

/**
 * @brief Shared vertex shader data.
 */
struct common_vertex_t {
  vec3 model_position;
  vec3 model_normal;
  vec3 position;
  vec3 normal;
  vec3 tangent;
  vec3 bitangent;
  vec2 diffusemap;
  vec3 voxel;
  vec4 color;
  vec3 ambient;
  vec3 diffuse;
  float caustics;
};

/**
 * @brief Shared fragment shader data.
 */
struct common_fragment_t {
  vec3 view_dir;
  float view_dist;
  float texture_lod;
  vec3 normal;
  vec3 tangent;
  vec3 bitangent;
  mat3 tbn;
  vec2 parallax;
  vec4 diffuse_sample;
  vec3 normal_sample;
  vec4 specular_sample;
  vec3 ambient;
  vec3 diffuse;
  vec3 specular;
  float caustics;
  vec2 shadow_sin_cos;
};

/**
 * @brief Maps t from [a, b] to [0, 1].
 */
float linearstep(float a, float b, float t) {
  return clamp((t - a) / (b - a), 0.0, 1.0);
}

/**
 * @brief Returns a 3D hash vector.
 */
vec3 hash33(vec3 p) {
  p = fract(p * vec3(.1031,.11369,.13787));
  p += dot(p, p.yxz + 19.19);
  return -1.0 + 2.0 * fract(vec3((p.x + p.y) * p.z, (p.x + p.z) * p.y, (p.y + p.z) * p.x));
}

/**
 * @brief Returns 3D value noise.
 */
float noise3d(vec3 p) {
  vec3 pi = floor(p);
  vec3 pf = p - pi;

  vec3 w = pf * pf * (3.0 - 2.0 * pf);

  return mix(
    mix(
      mix(dot(pf - vec3(0, 0, 0), hash33(pi + vec3(0, 0, 0))),
        dot(pf - vec3(1, 0, 0), hash33(pi + vec3(1, 0, 0))),
        w.x),
      mix(dot(pf - vec3(0, 0, 1), hash33(pi + vec3(0, 0, 1))),
        dot(pf - vec3(1, 0, 1), hash33(pi + vec3(1, 0, 1))),
        w.x),
      w.z),
    mix(
      mix(dot(pf - vec3(0, 1, 0), hash33(pi + vec3(0, 1, 0))),
        dot(pf - vec3(1, 1, 0), hash33(pi + vec3(1, 1, 0))),
        w.x),
      mix(dot(pf - vec3(0, 1, 1), hash33(pi + vec3(0, 1, 1))),
        dot(pf - vec3(1, 1, 1), hash33(pi + vec3(1, 1, 1))),
        w.x),
      w.z),
    w.y);
}

/**
 * @brief Builds a look-at matrix.
 */
mat4 lookAt(vec3 eye, vec3 pos, vec3 up) {

  vec3 Z = normalize(eye - pos);
  vec3 X = normalize(cross(up, Z));
  vec3 Y = normalize(cross(Z, X));

  return mat4(
    vec4(X.x, Y.x, Z.x, 0.0),
    vec4(X.y, Y.y, Z.y, 0.0),
    vec4(X.z, Y.z, Z.z, 0.0),
    vec4(-dot(X, eye), -dot(Y, eye), -dot(Z, eye), 1.0)
  );
}

/**
 * @brief Returns a Toksvig-adjusted gloss factor.
 */
float toksvig_gloss(in vec3 normal, in float power) {
  float len_rcp = 1.0 / saturate(length(normal));
  return 1.0 / (1.0 + power * (len_rcp - 1.0));
}

#endif // _COMMON_GLSL_
