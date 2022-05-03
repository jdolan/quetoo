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
 * @brief
 */
vec3 invert(vec3 color) {
    return vec3(1.0) - color;
}

/**
 * @brief
 */
void main(void) {

    vec3 pixel_color = textureLod(texture_color_attachment, vertex.texcoord, 0).rgb;
    vec3 bloom_color = vec3(0.0);

    for (int i = 1; i <= bloom_lod; i++) {
        // Max makes it look the same regardless of the LOD used.
        bloom_color = max(bloom_color, textureLod(texture_bloom_attachment, vertex.texcoord, i).rgb);
    }

    // This way adds the colors without overexposure artifacts.
    out_color.rgb = invert(invert(pixel_color) * invert(bloom_color));
}
