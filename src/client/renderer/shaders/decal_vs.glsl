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

#version 450

#include "uniforms.glsl"

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_texcoord;
layout (location = 3) in vec4 in_color;      // UBYTE4_NORM
layout (location = 4) in uint in_time;
layout (location = 5) in uint in_lifetime;

/**
 * @brief Per-model locals (the inline model matrix; identity for worldspawn).
 */
layout (std140, set = UNIFORM_SET, binding = BINDING_LOCALS) uniform locals_block {
  mat4 model;
};

layout (location = 0) out vec3 out_model_position;
layout (location = 1) out vec3 out_model_normal;
layout (location = 2) out vec2 out_texcoord;
layout (location = 3) out vec4 out_color;
layout (location = 4) flat out uint out_time;
layout (location = 5) flat out uint out_lifetime;

invariant gl_Position;

/**
 * @brief Decal vertex shader: the decal geometry is pre-clipped to surfaces on
 * the CPU; here it is transformed and its model-space position/normal forwarded
 * for clustered voxel lighting in the fragment stage.
 */
void main(void) {

  const vec4 position = vec4(in_position, 1.0);

  out_model_position = vec3(model * position);
  out_model_normal = normalize(vec3(model * vec4(in_normal, 0.0)));
  out_texcoord = in_texcoord;
  out_color = in_color;
  out_time = in_time;
  out_lifetime = in_lifetime;

  gl_Position = projection3D * view * model * position;
}
