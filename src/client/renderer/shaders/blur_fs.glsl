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

layout (location = 2) out vec4 blur_color;

uniform int level;
uniform float blur;

/**
 * @brief
 */
void main(void) {

	vec3 kernel[9] = vec3[](
		vec3(-1.0, -1.0, 0.077847), vec3(+0.0, -1.0, 0.123317), vec3(+1.0, -1.0, 0.077847),
		vec3(-1.0, +0.0, 0.123317), vec3(+0.0, +0.0, 0.195346), vec3(+1.0, +0.0, 0.123317),
		vec3(-1.0, +1.0, 0.077847), vec3(+0.0, +1.0, 0.123317), vec3(+1.0, +1.0, 0.077847));

	vec2 texel = blur / textureSize(texture_diffusemap, level);

	blur_color = vec4(0.0, 0.0, 0.0, 1.0);

	for (int j = 0; j < 9; j++) {
		vec2 texcoord = clamp(vertex.texcoord + texel * kernel[j].xy, 0.0, 1.0);
		blur_color.rgb += textureLod(texture_diffusemap, texcoord, level).rgb * kernel[j].z;
	}
}
