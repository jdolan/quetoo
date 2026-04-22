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
 * @brief Mode 0: extract bright regions from the scene color buffer.
 *
 * The alpha channel of the color attachment encodes a per-pixel bloom flag
 * written by the scene shaders: alpha=1.0 means the pixel was drawn by a
 * material stage (emissive / glowing accent) and should bloom unconditionally;
 * alpha=0.0 means the pixel is ordinary geometry subject to the threshold.
 *
 * The output feeds the first bloom blur ping-pong pass.
 */
void bloom_extract(void) {
  vec4 texel = texture(texture_color_attachment, vertex.texcoord);
  vec3 color = texel.rgb;
  if (texel.a > 0.5) {
    out_color = vec4(color, 1.0);
  } else {
    out_color = vec4(QuadraticThreshold(color, bloom_threshold, bloom_knee), 1.0);
  }
}

/**
 * @brief Mode 1: composite the blurred bloom back onto the scene color.
 *
 * The bloom texture has already been blurred by the separable Gaussian passes
 * in blur_fs.glsl.  We simply add it to the scene color, scaled by the bloom
 * intensity cvar.
 */
void bloom_composite(void) {
  vec3 color = texture(texture_color_attachment, vertex.texcoord).rgb;
  vec3 glow  = texture(texture_bloom_attachment, vertex.texcoord).rgb;
  out_color = vec4(color + glow * bloom, 1.0);
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
