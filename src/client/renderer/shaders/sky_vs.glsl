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

uniform mat4 projection;
uniform mat4 model_view;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec2 in_diffuse;

out vertex_data {
	vec2 diffuse;
} vertex;

/**
 * @brief
 */
void main(void) {

	gl_Position = projection * model_view * vec4(in_position, 1.0);

	vertex.diffuse = in_diffuse;
}

