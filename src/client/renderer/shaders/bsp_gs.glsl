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

in vertex_data {
	vec3 model;
	vec3 position;
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	vec2 diffusemap;
	vec2 lightmap;
	vec3 lightgrid;
	vec4 color;
} in_vertex[];

out geometry_data {
	vec3 model;
	vec3 position;
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	vec2 diffusemap;
	vec2 lightmap;
	vec3 lightgrid;
	vec4 color;

	flat int active_lights[MAX_LIGHT_UNIFORMS_ACTIVE];
	flat int num_active_lights;
} out_vertex;

/**
 * @brief
 */
void main(void) {

	int active_lights[MAX_LIGHT_UNIFORMS_ACTIVE];
	int num_active_lights = 0;

	vec3 a = in_vertex[0].model;
	vec3 b = in_vertex[1].model;
	vec3 c = in_vertex[2].model;

	for (int i = 0; i < num_lights && num_active_lights < MAX_LIGHT_UNIFORMS_ACTIVE; i++) {

		light_t light = lights[i];

		if (!triangle_intersects(a, b, c, light.mins.xyz, light.maxs.xyz)) {
			continue;
		}

		int type = int(light.position.w);

		if (type == LIGHT_AMBIENT || type == LIGHT_SUN) {
			if (dot(-light.normal.xyz, in_vertex[0].normal) < 0.0) {
				continue;
			}
		}

		active_lights[num_active_lights++] = i;
	}

	for (int i = 0; i < 3; i++) {

		out_vertex.model = in_vertex[i].model;
		out_vertex.position = in_vertex[i].position;
		out_vertex.normal = in_vertex[i].normal;
		out_vertex.tangent = in_vertex[i].tangent;
		out_vertex.bitangent = in_vertex[i].bitangent;
		out_vertex.diffusemap = in_vertex[i].diffusemap;
		out_vertex.lightmap = in_vertex[i].lightmap;
		out_vertex.lightgrid = in_vertex[i].lightgrid;
		out_vertex.color = in_vertex[i].color;

		out_vertex.active_lights = active_lights;
		out_vertex.num_active_lights = num_active_lights;

//		if (out_vertex.num_active_lights == MAX_LIGHT_UNIFORMS_ACTIVE) {
//			out_vertex.color = vec4(1, 0, 0, 1);
//		}

		gl_Position = gl_in[i].gl_Position;

		EmitVertex();
	}

	EndPrimitive();
}

