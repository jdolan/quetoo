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
layout (location = 1) in vec2 in_diffusemap;
layout (location = 2) in vec2 in_next_diffusemap;
layout (location = 3) in vec4 in_color;
layout (location = 4) in float in_lerp;
layout (location = 5) in int in_blend_depth;

uniform mat4 projection;
uniform mat4 view;

uniform lightgrid_t lightgrid;

out vertex_data {
	vec3 position;
	vec2 diffusemap;
	vec2 next_diffusemap;
	vec3 lightgrid;
	vec4 color;
	float lerp;
	int blend_depth;
} vertex;

/**
 * @brief
 */
void main(void) {

	vec4 position = vec4(in_position, 1.0);

	vertex.position = vec3(view * vec4(in_position.xyz, 1.0));
	vertex.diffusemap = in_diffusemap;
	vertex.next_diffusemap = in_next_diffusemap;
	vertex.lightgrid = lightgrid_vertex(lightgrid, in_position);
	vertex.color = in_color;
	vertex.lerp = in_lerp;
	vertex.blend_depth = in_blend_depth;

	gl_Position = projection * view * position;
}
