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

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_next_position;

uniform mat4 model;
uniform mat4 light_view[6];
uniform int light_index;
uniform int face_index;
uniform float lerp;

out vec4 position;

void main() {
  position = model * vec4(mix(in_position, in_next_position, lerp), 1.0);
  position = position - vec4(lights[light_index].origin.xyz, 0.0);
  position = light_view[face_index] * position;
  gl_Position = light_projection * position;
}
