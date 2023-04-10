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

uniform bool horizontal;

const float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

/**
 * @brief
 * @see https://learnopengl.com/Advanced-Lighting/Bloom
 */
void main(void) {

	vec2 offset = 2.0 / textureSize(texture_diffusemap, 0);
	vec3 in_color = texture(texture_diffusemap, vertex.texcoord).rgb * weight[0];

	if (horizontal) {
		for (int i = 1; i < 5; i++) {
			in_color += texture(texture_diffusemap, vertex.texcoord + vec2(offset.x * i, 0.0)).rgb * weight[i];
			in_color += texture(texture_diffusemap, vertex.texcoord - vec2(offset.x * i, 0.0)).rgb * weight[i];
		}
	} else {
		for (int i = 1; i < 5; i++) {
			in_color += texture(texture_diffusemap, vertex.texcoord + vec2(0.0, offset.y * i)).rgb * weight[i];
			in_color += texture(texture_diffusemap, vertex.texcoord - vec2(0.0, offset.y * i)).rgb * weight[i];
		}
	}

	out_color = vec4(in_color, 1.0);
}
