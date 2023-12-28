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

uniform sampler2D texture_color_attachment;
uniform sampler2D texture_bloom_attachment;

in vertex_data {
	vec2 texcoord;
} vertex;

out vec4 out_color;

/**
 * @brief Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
 */
vec3 tonemap_color(in vec3 color) {
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;

	return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

/**
 * @brief
 */
void main(void) {

	out_color = texture(texture_color_attachment, vertex.texcoord);

	if (bloom > 0.0) {
		out_color += texture(texture_bloom_attachment, vertex.texcoord);
	}

	if (tonemap > 0.0) {
		out_color.rgb = tonemap_color(out_color.rgb);
	}

	out_color.rgb = pow(out_color.rgb, vec3(1.0 / gamma));
}
