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
 * @brief Quadratic soft-knee threshold for smooth bloom extraction.
 *
 * Pixels dimmer than (threshold - knee) contribute nothing.
 * Pixels between (threshold - knee) and (threshold + knee) are ramped up
 * smoothly via a quadratic curve.  Pixels above (threshold + knee) pass
 * through at full brightness minus the threshold.
 *
 * This avoids the hard-cutoff banding visible when thresholding at a fixed
 * luminance value.
 *
 * @see "Next Generation Post Processing in Call of Duty: Advanced Warfare"
 *      Jimenez et al., SIGGRAPH 2014.
 */
vec3 QuadraticThreshold(vec3 color, float threshold, float knee) {
  float brightness = max(max(color.r, color.g), color.b);
  float rq = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
  rq = (rq * rq) / (4.0 * knee + 0.00001);
  return color * max(rq, brightness - threshold) / max(brightness, 0.00001);
}

/**
 * @brief Mode 0: extract bright regions from the HDR scene color buffer.
 */
void bloom_extract(void) {
  vec3 color = texture(texture_color_attachment, vertex.texcoord).rgb;
  out_color = vec4(QuadraticThreshold(color, bloom_threshold, 1.0), 1.0);
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
