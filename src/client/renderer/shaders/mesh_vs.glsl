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
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_tangent;
layout (location = 3) in vec3 in_bitangent;
layout (location = 4) in vec2 in_diffusemap;

layout (location = 5) in vec3 in_next_position;
layout (location = 6) in vec3 in_next_normal;
layout (location = 7) in vec3 in_next_tangent;
layout (location = 8) in vec3 in_next_bitangent;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

uniform float lerp;

uniform vec3 lightgrid_mins;
uniform vec3 lightgrid_maxs;

uniform vec4 color;

uniform stage_t stage;
uniform int ticks;

out vertex_data {
	vec3 position;
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	vec2 diffusemap;
	vec3 lightgrid;
	vec4 color;
} vertex;

/**
 * @brief
 */
void main(void) {

	vec4 lerp_position = vec4(mix(in_position, in_next_position, lerp), 1.0);
	vec4 lerp_normal = vec4(mix(in_normal, in_next_normal, lerp), 0.0);
	vec4 lerp_tangent = vec4(mix(in_tangent, in_next_tangent, lerp), 0.0);
	vec4 lerp_bitangent = vec4(mix(in_bitangent, in_next_bitangent, lerp), 0.0);

	gl_Position = projection * view * model * lerp_position;

	vertex.position = vec3(view * model * lerp_position);
	vertex.normal = normalize(vec3(view * model * lerp_normal));
	vertex.tangent = normalize(vec3(view * model * lerp_tangent));
	vertex.bitangent = normalize(vec3(view * model * lerp_bitangent));
	vertex.diffusemap = in_diffusemap;
	vertex.color = color;

	vec3 world_position = vec3(model * lerp_position);
	vertex.lightgrid = (world_position - lightgrid_mins) / (lightgrid_maxs - lightgrid_mins);

	{
		if ((stage.flags & STAGE_COLOR) == STAGE_COLOR) {
			vertex.color *= stage.color;
		}

		if ((stage.flags & STAGE_PULSE) == STAGE_PULSE) {
			vertex.color.a *= (sin(stage.pulse * ticks * 0.00628) + 1.0) / 2.0;
		}

		if ((stage.flags & STAGE_SCROLL_S) == STAGE_SCROLL_S) {
			vertex.diffusemap.s += stage.scroll.s * ticks / 1000.0;
		}

		if ((stage.flags & STAGE_SCROLL_T) == STAGE_SCROLL_T) {
			vertex.diffusemap.t += stage.scroll.t * ticks / 1000.0;
		}

		if ((stage.flags & STAGE_SCALE_S) == STAGE_SCALE_S) {
			vertex.diffusemap.s *= stage.scale.s;
		}

		if ((stage.flags & STAGE_SCALE_T) == STAGE_SCALE_T) {
			vertex.diffusemap.t *= stage.scale.t;
		}

		if ((stage.flags & STAGE_ENVMAP) == STAGE_ENVMAP) {
			vertex.diffusemap = normalize(vertex.position).xy;
		}
	}
}

