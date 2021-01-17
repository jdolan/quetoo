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
layout (location = 5) in float in_softness;
layout (location = 6) in float in_lighting;

uniform sampler3D texture_lightgrid_ambient;
uniform sampler3D texture_lightgrid_diffuse;
uniform sampler3D texture_lightgrid_fog;

out vertex_data {
	vec3 position;
	vec2 diffusemap;
	vec2 next_diffusemap;
	vec4 color;
	float lerp;
	float softness;
	float lighting;
	vec3 ambient;
	vec3 diffuse;
	vec4 fog;
} vertex;

/**
 * @brief
 */
void main(void) {

	vec4 position = vec4(in_position, 1.0);

	vertex.position = vec3(view * position);
	vertex.diffusemap = in_diffusemap;
	vertex.next_diffusemap = in_next_diffusemap;
	vertex.color = in_color;
	vertex.lerp = in_lerp;
	vertex.softness = in_softness;
	vertex.lighting = in_lighting;

	vec3 lightgrid_uvw = lightgrid_uvw(in_position);

	vertex.ambient = texture(texture_lightgrid_ambient, lightgrid_uvw).rgb;
	vertex.diffuse = texture(texture_lightgrid_diffuse, lightgrid_uvw).rgb;

	vertex.fog = vec4(0.0, 0.0, 0.0, 1.0);
	lightgrid_fog(vertex.fog, texture_lightgrid_fog, vertex.position, lightgrid_uvw);

	gl_Position = projection3D * view * position;
}
