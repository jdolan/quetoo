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

/*
 * TODO(#864): bring-up mesh program — animated (two-frame lerp) geometry with
 * diffuse material and clustered per-fragment lighting (see mesh_fs / bsp_fs).
 * The two animation frames are supplied as two vertex buffer bindings (the same
 * model buffer at two frame offsets); locations 0..4 are the old frame, 5..6
 * the current frame. Stages, tangents/normal-mapping, shells, and tints are
 * ported in later increments.
 */

#include "uniforms.glsl"

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 4) in vec2 in_diffusemap;

layout (location = 5) in vec3 in_next_position;
layout (location = 6) in vec3 in_next_normal;

/**
 * @brief Per-entity locals.
 */
layout (std140, set = UNIFORM_SET, binding = BINDING_LOCALS) uniform locals_block {
  mat4 model;
  float lerp;
};

layout (location = 0) out vec2 out_diffusemap;
layout (location = 1) out vec3 out_model_position;
layout (location = 2) out vec3 out_model_normal;

invariant gl_Position;

/**
 * @brief
 */
void main(void) {

  const vec4 position = vec4(mix(in_position, in_next_position, lerp), 1.0);
  const vec4 normal = vec4(mix(in_normal, in_next_normal, lerp), 0.0);

  out_diffusemap = in_diffusemap;
  out_model_position = vec3(model * position);
  out_model_normal = normalize(vec3(model * normal));

  gl_Position = projection3D * view * model * position;
}
