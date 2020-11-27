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

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

uniform int blend_depth;

in vertex_data {
	vec3 position;
	vec2 diffusemap;
	vec2 next_diffusemap;
	vec3 lightgrid;
	vec4 color;
	float lerp;
	int blend_depth;
} in_vertex[];

out vertex_data {
	vec3 position;
	vec2 diffusemap;
	vec2 next_diffusemap;
	vec3 lightgrid;
	vec4 color;
	float lerp;
} out_vertex;

/**
 * @brief
 */
void main() {

	if (blend_depth == in_vertex[0].blend_depth) {

		for (int i = 0; i < 3; i++) {
			gl_Position = gl_in[i].gl_Position;
			out_vertex.position = in_vertex[i].position;
			out_vertex.diffusemap = in_vertex[i].diffusemap;
			out_vertex.next_diffusemap = in_vertex[i].next_diffusemap;
			out_vertex.lightgrid = in_vertex[i].lightgrid;
			out_vertex.color = in_vertex[i].color;
			out_vertex.lerp = in_vertex[i].lerp;

			EmitVertex();
		}
	}

    EndPrimitive();
}
