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

layout (location = 0) in vec3 in_corner;

layout (location = 1) in vec3 in_mins;
layout (location = 2) in vec3 in_maxs;

invariant gl_Position;

/**
 * @brief Expands each instanced unit-cube corner into an occlusion query box vertex.
 */
void main(void) {

  const vec3 position = mix(in_mins, in_maxs, in_corner);

  gl_Position = projection3D * view * vec4(position, 1.0);
}
