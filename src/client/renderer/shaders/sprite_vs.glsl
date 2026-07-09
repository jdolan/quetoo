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
layout (location = 1) in vec2 in_diffusemap;
layout (location = 2) in vec2 in_next_diffusemap;
layout (location = 3) in vec4 in_color;         // UBYTE4_NORM; .rgb is the sprite color
layout (location = 4) in vec2 in_lerp_lighting;  // UBYTE2_NORM; .x lerp, .y lighting

layout (location = 0) out vec3 out_position;         // world space, for clustered voxel lighting
layout (location = 1) out vec2 out_diffusemap;
layout (location = 2) out vec2 out_next_diffusemap;
layout (location = 3) out vec3 out_color;
layout (location = 4) out float out_lerp;
layout (location = 5) out float out_lighting;        // how much surrounding light the sprite absorbs

invariant gl_Position;

/**
 * @brief Billboard sprite/beam vertices are pre-oriented on the CPU. World
 * position and the per-sprite lighting scalar are forwarded so the fragment
 * shader can modulate the color by clustered voxel lighting (see sprite_fs).
 */
void main(void) {

  out_position = in_position;
  out_diffusemap = in_diffusemap;
  out_next_diffusemap = in_next_diffusemap;
  out_color = in_color.rgb;
  out_lerp = in_lerp_lighting.x;
  out_lighting = in_lerp_lighting.y;

  gl_Position = projection3D * view * vec4(in_position, 1.0);
}
