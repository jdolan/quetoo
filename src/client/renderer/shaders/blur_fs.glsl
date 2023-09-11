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

uniform sampler2D texture_diffusemap;

in vertex_data {
	vec2 texcoord;
} vertex;

out vec4 out_color;

uniform int kernel;
uniform int level;
uniform bool horizontal;

/**
 * @brief
 * @see https://learnopengl.com/Advanced-Lighting/Bloom
 */
void main(void) {

	vec2 offset = 2.0 / textureSize(texture_diffusemap, level);

	out_color = vec4(0.0, 0.0, 0.0, 1.0);

	// https://www.quora.com/Given-a-number-n-what-is-the-sum-of-all-integers-less-than-n
	float frac = 1.0 / kernel * (kernel + 1.0) / 2.0;

	for (int i = kernel; i >= 0; i--) {

		float weight = i * frac;

		if (horizontal) {
			out_color.rgb += textureLod(texture_diffusemap, vertex.texcoord + vec2(offset.x * i, 0.0), level).rgb * weight;
			out_color.rgb += textureLod(texture_diffusemap, vertex.texcoord - vec2(offset.x * i, 0.0), level).rgb * weight;
		} else {
			out_color.rgb += textureLod(texture_diffusemap, vertex.texcoord + vec2(0.0, offset.y * i), level).rgb * weight;
			out_color.rgb += textureLod(texture_diffusemap, vertex.texcoord - vec2(0.0, offset.y * i), level).rgb * weight;
		}
	}
}
