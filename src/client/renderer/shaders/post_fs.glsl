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

#version 450

/*
 * Self-contained post program: the HDR scene color and the bloom texture at
 * fragment sampler slots 0, 1 (set 2), and the per-pass locals at fragment
 * uniform slot 0 (set 3).
 */

layout (set = 2, binding = 0) uniform sampler2D texture_color_attachment;
layout (set = 2, binding = 1) uniform sampler2D texture_bloom_attachment;

layout (location = 0) in vertex_data {
  vec2 texcoord;
} vertex;

layout (location = 0) out vec4 out_color;

/**
 * @brief Per-pass locals.
 */
layout (std140, set = 3, binding = 0) uniform locals_block {
  int post_stage;
  float bloom;
  float bloom_threshold;
};

/**
 * @brief Post-processing stage selector, mirroring the r_post_stage_t C enum.
 */
const int R_POST_BLOOM_EXTRACT = 0;
const int R_POST_BLOOM_BLUR_X  = 1;
const int R_POST_BLOOM_BLUR_Y  = 2;
const int R_POST_TONEMAP       = 3;

/**
 * @brief Extracts bright HDR regions for the bloom passes.
 */
void bloom_extract(void) {
  out_color = vec4(max(texture(texture_color_attachment, vertex.texcoord).rgb - bloom_threshold, 0.0), 1.0);
}

/**
 * @brief Applies one axis of the separable bloom blur.
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
 * @brief Combines scene color and bloom and clamps the result to LDR.
 */
void tonemap(void) {
  vec3 color = texture(texture_color_attachment, vertex.texcoord).rgb;
  vec3 glow  = texture(texture_bloom_attachment, vertex.texcoord).rgb;
  color = color + glow * bloom;

  out_color = vec4(clamp(color, 0.0, 1.0), 1.0);
}

/**
 * @brief Runs the selected post-processing stage.
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
