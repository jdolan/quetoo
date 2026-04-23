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

uniform int mode;
uniform float bloom;
uniform float bloom_threshold;

/**
 * @brief Mode 0: extract bright regions from the HDR scene color buffer.
 *
 * Subtracts bloom_threshold from each channel and clamps to zero, so only
 * HDR-range pixels (above the threshold) feed the blur passes.
 */
void bloom_extract(void) {
  out_color = vec4(max(texture(texture_color_attachment, vertex.texcoord).rgb - bloom_threshold, 0.0), 1.0);
}

/**
 * @brief Mode 1: composite blurred bloom onto scene color and clamp to LDR.
 *
 * Adds the blurred bloom (scaled by bloom intensity) to the HDR scene color,
 * then clamps to [0, 1] to produce the final LDR output.  When r_bloom is 0
 * the bloom texture is not bound, glow evaluates to (0, 0, 0), and this
 * reduces to a plain HDR clamp.
 */
void bloom_composite(void) {
  vec3 color = texture(texture_color_attachment, vertex.texcoord).rgb;
  vec3 glow  = texture(texture_bloom_attachment, vertex.texcoord).rgb;
  out_color = vec4(clamp(color + glow * bloom, 0.0, 1.0), 1.0);
}

/**
 * @brief
 */
void main(void) {

  if (mode == 0) {
    bloom_extract();
  } else {
    bloom_composite();
  }
}
