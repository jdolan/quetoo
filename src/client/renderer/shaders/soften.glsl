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

/**
 * @file soften.glsl
 * @brief Provides soft-particle depth fading helpers.
 * @remarks Include after uniforms.glsl.
 */

#define TRANSITION_SIZE .0016

layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_DEPTH_ATTACHMENT) uniform sampler2D texture_depth_attachment;

/**
 * @brief Converts clip-space depth to normalized linear depth.
 */
float calc_depth(in float z) {
  return (2. * depth_range.x) / (depth_range.y + depth_range.x - z * (depth_range.y - depth_range.x));
}

/**
 * @brief Computes soft-particle fade against the scene depth attachment.
 */
float soften(void) {

  vec4 depth_sample = texture(texture_depth_attachment, gl_FragCoord.xy / vec2(viewport.zw));
  return smoothstep(0.0, TRANSITION_SIZE, clamp(calc_depth(depth_sample.r) - calc_depth(gl_FragCoord.z), 0.0, 1.0));
}
