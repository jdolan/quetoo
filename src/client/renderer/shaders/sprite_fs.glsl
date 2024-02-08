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
 
in vertex_data {
	vec3 position;
	vec2 diffusemap;
	vec2 next_diffusemap;
	vec4 color;
	float lerp;
	float softness;
	vec4 fog;
} vertex;

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec4 out_bloom;

/**
 * @brief
 */
void main(void) {

	vec4 texture_color = mix(
		texture(texture_diffusemap, vertex.diffusemap),
		texture(texture_next_diffusemap, vertex.next_diffusemap),
		vertex.lerp);

	out_color = texture_color * vertex.color;

	out_bloom.rgb = max(out_color.rgb * 2.0 * bloom - 1.0, 0.0);
	out_bloom.a = out_color.a;

	out_color.rgb += vertex.fog.rgb * out_color.a;

	float softness = soften(vertex.softness);
	out_color *= softness;
	out_bloom *= softness;
}
