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

const float WEIGHT[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

/**
 * @brief
 */
void main(void) {

	out_color = texture(texture_color_attachment, vertex.texcoord);

	// FIXME: Make the blur kernel relative to screen size, like 2% or something?

	vec2 texture_size = textureSize(texture_bloom_attachment, 0);
	vec2 texel_size = 1.0 / texture_size;

	vec2 kernel = 4.0 * texel_size;

	vec3 bloom = texture(texture_bloom_attachment, vertex.texcoord).rgb * WEIGHT[0];

	for (int i = 1; i < 5; i++) {
		bloom += texture(texture_bloom_attachment, vertex.texcoord + vec2(kernel.x * i, 0.0)).rgb * WEIGHT[i];
		bloom += texture(texture_bloom_attachment, vertex.texcoord - vec2(kernel.x * i, 0.0)).rgb * WEIGHT[i];
	}

	for (int i = 1; i < 5; i++) {
		bloom += texture(texture_bloom_attachment, vertex.texcoord + vec2(0.0, kernel.y * i)).rgb * WEIGHT[i];
		bloom += texture(texture_bloom_attachment, vertex.texcoord - vec2(0.0, kernel.y * i)).rgb * WEIGHT[i];
	}

	out_color.rgb += bloom;
}
