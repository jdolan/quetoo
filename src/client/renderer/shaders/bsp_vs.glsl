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
layout (location = 5) in vec2 in_lightmap;
layout (location = 6) in vec4 in_color;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

uniform stage_t stage;

uniform int ticks;

out vertex_data {
	vec3 position;
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	vec2 diffusemap;
	vec2 lightmap;
	vec4 color;
} vertex;

/**
 * @brief
 */
void main(void) {

	gl_Position = projection * view * model * vec4(in_position, 1.0);

	vertex.position = (view * model * vec4(in_position, 1.0)).xyz;
	vertex.normal = normalize(vec3(view * model * vec4(in_normal, 0.0)).xyz);
	vertex.tangent = normalize(vec3(view * model * vec4(in_tangent, 0.0)).xyz);
	vertex.bitangent = normalize(vec3(view * model * vec4(in_bitangent, 0.0)).xyz);

	vertex.diffusemap = in_diffusemap;
	vertex.lightmap = in_lightmap;
	vertex.color = in_color;

	{
		if ((stage.flags & STAGE_COLOR) == STAGE_COLOR) {
			vertex.color *= stage.color;
		}

		if ((stage.flags & STAGE_PULSE) == STAGE_PULSE) {
			vertex.color.a *= (sin(stage.pulse * ticks * 0.00628) + 1.0) / 2.0;
		}

		if ((stage.flags & STAGE_STRETCH) == STAGE_STRETCH) {
			float hz = (sin(ticks * stage.stretch.x * 0.00628f) + 1.0) / 2.0;
			float amp = 1.5 - hz * stage.stretch.y;

			vertex.diffusemap = vertex.diffusemap - stage.st_origin;
			vertex.diffusemap = mat2(amp, 0, 0, amp) * vertex.diffusemap;
			vertex.diffusemap = vertex.diffusemap + stage.st_origin;
		}

		if ((stage.flags & STAGE_ROTATE) == STAGE_ROTATE) {
			float theta = ticks * stage.rotate * 0.36;

			vertex.diffusemap = vertex.diffusemap - stage.st_origin;
			vertex.diffusemap = mat2(cos(theta), -sin(theta), sin(theta),  cos(theta)) * vertex.diffusemap;
			vertex.diffusemap = vertex.diffusemap + stage.st_origin;
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

		if ((stage.flags & STAGE_TERRAIN) == STAGE_TERRAIN) {
			vertex.color.a *= clamp((in_position.z - stage.terrain.x) / (stage.terrain.y - stage.terrain.x), 0.0, 1.0);
		}

		if ((stage.flags & STAGE_DIRTMAP) == STAGE_DIRTMAP) {
			vertex.color.a *= DIRTMAP[int(abs(in_position.x + in_position.y)) % DIRTMAP.length()] * stage.dirtmap;
		}
	}
}
