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

#version 330

layout (location = 0) in vec4 in_position;
layout (location = 1) in vec4 in_color;

uniform mat4 projection;
uniform mat4 view;

uniform float pixels_per_radian;
uniform vec2 depth_range;

out vertex_data {
	vec3 position;
	vec4 color;
} vertex;

/**
 * @brief
 */
void main(void) {

	gl_Position = projection * view * vec4(in_position.xyz, 1.0);

	vertex.position = vec3(view * vec4(in_position.xyz, 1.0));

	float depth = clamp(gl_Position.z / depth_range.y, 0.0, 1.0);

	gl_PointSize = pixels_per_radian * in_position.w  / depth;

	vertex.color = in_color;
}
