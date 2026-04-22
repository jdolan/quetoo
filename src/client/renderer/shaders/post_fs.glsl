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
uniform float bloom_knee;

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
 * @brief ACES filmic tonemapping (Krzysztof Narkowicz approximation).
 *
 * Maps HDR scene values to the LDR display range [0, 1] with a pleasing
 * filmic S-curve: highlights compress gracefully instead of hard-clipping,
 * and shadows/mids retain contrast.
 *
 * @see https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
 */
vec3 ACESTonemap(vec3 x) {
  const float a = 2.51;
  const float b = 0.03;
  const float c = 2.43;
  const float d = 0.59;
  const float e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

/**
 * @brief Mode 0: extract bright regions from the HDR scene color buffer.
 *
 * The color attachment is GL_R11F_G11F_B10F, so values from additive
 * material stages can exceed 1.0 and bloom naturally.  A soft-knee
 * quadratic threshold avoids hard-cutoff banding.
 *
 * The output feeds the first bloom blur ping-pong pass.
 */
void bloom_extract(void) {
  vec3 color = texture(texture_color_attachment, vertex.texcoord).rgb;
  out_color = vec4(QuadraticThreshold(color, bloom_threshold, bloom_knee), 1.0);
}

/**
 * @brief Mode 1: composite blurred bloom onto scene color, then tonemap.
 *
 * Adds the blurred bloom (scaled by bloom intensity) to the HDR scene
 * color, then applies ACES filmic tonemapping to produce the final LDR
 * output written to the RGBA8 post attachment.
 */
void bloom_composite(void) {
  vec3 color = texture(texture_color_attachment, vertex.texcoord).rgb;
  vec3 glow  = texture(texture_bloom_attachment, vertex.texcoord).rgb;
  out_color = vec4(ACESTonemap(color + glow * bloom), 1.0);
}

/**
 * @brief Mode 2: passthrough — clamp HDR to LDR with no tonemapping or bloom.
 *
 * Used when r_post is disabled.  R_DrawPost must still run to write the
 * HDR color attachment into the RGBA8 post attachment (the display cannot
 * receive the float format directly), but this mode preserves the exact
 * pre-tonemapped appearance so disabling r_post has no visual side-effects.
 */
void bloom_passthrough(void) {
  out_color = vec4(clamp(texture(texture_color_attachment, vertex.texcoord).rgb, 0.0, 1.0), 1.0);
}

/**
 * @brief
 */
void main(void) {

  if (mode == 0) {
    bloom_extract();
  } else if (mode == 1) {
    bloom_composite();
  } else {
    bloom_passthrough();
  }
}
