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
 * Point-light shadow depth pass, one cube face per draw. light_projection and
 * depth_range come from the shared globals; the caller pushes the current face's
 * light_view and the light origin per light. Locations 0/1 are the old and
 * current animation frames (identical for static geometry). The light-relative
 * position is passed through as a linear varying; the fragment shader computes
 * the true radial distance-from-light per-fragment (see shadow_fs.glsl).
 */

#include "uniforms.glsl"

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_next_position;

layout (std140, set = UNIFORM_SET, binding = BINDING_LOCALS) uniform locals_block {
  mat4 model;
  mat4 light_view;      // light_view matrix for the current cube face
  vec4 light_origin;    // xyz = light origin (world space)
  float lerp;
};

layout (location = 0) out vec3 out_position;

invariant gl_Position;

/**
 * @brief
 */
void main(void) {

  const vec3 position = vec3(model * vec4(mix(in_position, in_next_position, lerp), 1.0)) - light_origin.xyz;

  out_position = position;

  gl_Position = light_projection * light_view * vec4(position, 1.0);
}
