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

uniform vec2 viewport;

out vertex_data {
	vec4 color;
} vertex;

/**
 * @brief
 */
void main(void) {

	gl_Position = projection * view * vec4(in_position.xyz, 1.0);

	vec4 view_position = view * vec4(in_position.xyz, 1.0);
	vec4 projected_size = projection * vec4(in_position.w, in_position.w, view_position.z, view_position.w);
	vec2 size = viewport * projected_size.xy / projected_size.w;

	gl_PointSize = size.x + size.y;

	vertex.color = in_color;
}
