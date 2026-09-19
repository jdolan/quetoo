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

layout (set = 2, binding = 0) uniform sampler2D textureColorAttachment;
layout (set = 2, binding = 1) uniform sampler2D textureBloomAttachment;

layout (location = 0) in vertexData {
  vec2 texcoord;
} vertex;

layout (location = 0) out vec4 outColor;

/**
 * @brief Per-pass locals.
 */
layout (std140, set = 3, binding = 0) uniform localsBlock {
  int postStage;
  float bloom;
  float bloomThreshold;
};

/**
 * @brief Post-processing stage selector, mirroring the RenderPostStage C enum.
 */
const int R_POST_BLOOM_EXTRACT = 0;
const int R_POST_BLOOM_BLUR_X  = 1;
const int R_POST_BLOOM_BLUR_Y  = 2;
const int R_POST_TONEMAP       = 3;

/**
 * @brief Extracts bright HDR regions for the bloom passes.
 */
void bloomExtract(void) {
  outColor = vec4(max(texture(textureColorAttachment, vertex.texcoord).rgb - bloomThreshold, 0.0), 1.0);
}

/**
 * @brief Applies one axis of the separable bloom blur.
 */
void bloomBlur(void) {

  const float offsets[3] = float[](0.0, 1.3846153846, 3.2307692308);
  const float weights[3] = float[](0.2270270270, 0.3162162162, 0.0702702703);

  vec2 texel = 1.0 / textureSize(textureBloomAttachment, 0);

  outColor = texture(textureBloomAttachment, vertex.texcoord) * weights[0];

  if (postStage == R_POST_BLOOM_BLUR_X) {
    for (int i = 1; i < 3; i++) {
      outColor += texture(textureBloomAttachment, vertex.texcoord + vec2(texel.x * offsets[i], 0.0)) * weights[i];
      outColor += texture(textureBloomAttachment, vertex.texcoord - vec2(texel.x * offsets[i], 0.0)) * weights[i];
    }
  } else {
    for (int i = 1; i < 3; i++) {
      outColor += texture(textureBloomAttachment, vertex.texcoord + vec2(0.0, texel.y * offsets[i])) * weights[i];
      outColor += texture(textureBloomAttachment, vertex.texcoord - vec2(0.0, texel.y * offsets[i])) * weights[i];
    }
  }

  outColor.a = 1.0;
}

/**
 * @brief Combines scene color and bloom and clamps the result to LDR.
 */
void tonemap(void) {
  vec3 color = texture(textureColorAttachment, vertex.texcoord).rgb;
  vec3 glow  = texture(textureBloomAttachment, vertex.texcoord).rgb;
  color = color + glow * bloom;

  outColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}

/**
 * @brief Runs the selected post-processing stage.
 */
void main(void) {

  if (postStage == R_POST_BLOOM_EXTRACT) {
    bloomExtract();
  } else if (postStage == R_POST_BLOOM_BLUR_X || postStage == R_POST_BLOOM_BLUR_Y) {
    bloomBlur();
  } else {
    tonemap();
  }
}
