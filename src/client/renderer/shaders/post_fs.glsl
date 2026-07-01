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

uniform int post_stage;
uniform float bloom;
uniform float bloom_threshold;

/**
 * @brief Post-processing stage selector, mirroring the r_post_stage_t C enum.
 */
const int R_POST_BLOOM_EXTRACT = 0;
const int R_POST_BLOOM_BLUR_X  = 1;
const int R_POST_BLOOM_BLUR_Y  = 2;
const int R_POST_TONEMAP       = 3;

/**
 * @brief Extract bright regions from the HDR scene color buffer.
 *
 * Subtracts bloom_threshold from each channel and clamps to zero, so only
 * HDR-range pixels (above the threshold) feed the blur passes.
 */
void bloom_extract(void) {
  out_color = vec4(max(texture(texture_color_attachment, vertex.texcoord).rgb - bloom_threshold, 0.0), 1.0);
}

/**
 * @brief One axis of the separable Gaussian bloom blur.
 *
 * Two-pass (X then Y) Gaussian blur using the bilinear sampling trick.
 * Each pass performs a 9-tap kernel with only 5 texture fetches by exploiting
 * hardware bilinear filtering to evaluate two weighted taps per sample.
 *
 * @see https://rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/
 */
void bloom_blur(void) {

  const float offsets[3] = float[](0.0, 1.3846153846, 3.2307692308);
  const float weights[3] = float[](0.2270270270, 0.3162162162, 0.0702702703);

  vec2 texel = 1.0 / textureSize(texture_bloom_attachment, 0);

  out_color = texture(texture_bloom_attachment, vertex.texcoord) * weights[0];

  if (post_stage == R_POST_BLOOM_BLUR_X) {
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

/**
 * @brief Hashes a screen coordinate to a pseudo-random float in [0, 1).
 * @see https://www.shadertoy.com/view/4djSRW (Dave Hoskins, hash12)
 */
float hash12(in vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

/**
 * @brief Tonemap and color clamp to LDR.
 *
 * Adds the blurred bloom (scaled by bloom intensity) to the HDR scene color,
 * then clamps to [0, 1] to produce the final LDR output written to post_attachment.
 * When r_bloom is 0 the bloom texture is not bound, glow evaluates to (0, 0, 0),
 * and this reduces to a plain HDR clamp.
 *
 * A triangular (TPDF) dither of +/- 1 LSB is added before the 8-bit write to break
 * up banding in smooth, low-contrast gradients (e.g. dark falloffs). The noise is
 * purely spatial so it does not shimmer on static imagery.
 */
void tonemap(void) {
  vec3 color = texture(texture_color_attachment, vertex.texcoord).rgb;
  vec3 glow  = texture(texture_bloom_attachment, vertex.texcoord).rgb;
  color = color + glow * bloom;

  color = clamp(color, 0.0, 1.0);

  // TPDF dither: sum of two independent uniform samples spans [-1, 1] LSB
  float r0 = hash12(gl_FragCoord.xy);
  float r1 = hash12(gl_FragCoord.xy + 11.0);
  color += (r0 + r1 - 1.0) / 255.0;

  out_color = vec4(color, 1.0);
}

/**
 * @brief
 */
void main(void) {

  if (post_stage == R_POST_BLOOM_EXTRACT) {
    bloom_extract();
  } else if (post_stage == R_POST_BLOOM_BLUR_X || post_stage == R_POST_BLOOM_BLUR_Y) {
    bloom_blur();
  } else {
    tonemap();
  }
}
