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
  vec2 texcoord;
} vertex;

out vec4 out_color;

uniform sampler2D texture_bloom_attachment;
uniform int axis;

/**
 * @brief Two-pass separable Gaussian blur using the bilinear sampling trick.
 *
 * Each pass performs a 9-tap Gaussian kernel with only 5 texture fetches by
 * exploiting hardware bilinear filtering to evaluate two weighted taps per
 * sample.  Running horizontal (axis=0) then vertical (axis=1) produces a
 * full 2D Gaussian blur.
 *
 * @see https://rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/
 */
void main(void) {

  const float offsets[3] = float[](0.0, 1.3846153846, 3.2307692308);
  const float weights[3] = float[](0.2270270270, 0.3162162162, 0.0702702703);

  vec2 texel = 1.0 / textureSize(texture_bloom_attachment, 0);

  out_color = texture(texture_bloom_attachment, vertex.texcoord) * weights[0];

  if (axis == 0) {
    for (int i = 1; i < 3; i++) {
      out_color += texture(texture_bloom_attachment, vertex.texcoord + vec2(texel.x * offsets[i], 0.0)) * weights[i];
      out_color += texture(texture_bloom_attachment, vertex.texcoord - vec2(texel.x * offsets[i], 0.0)) * weights[i];
    }
  } else {
    for (int i = 1; i < 3; i++) {
      out_color += texture(texture_bloom_attachment, vertex.texcoord + vec2(0.0, texel.y * offsets[i])) * weights[i];
      out_color += texture(texture_bloom_attachment, vertex.texcoord - vec2(0.0, texel.y * offsets[i])) * weights[i];
    }
  }

  out_color.a = 1.0;
}
