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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#version 450

/**
 * @file sprite_fs.glsl
 * @brief Shades sprites and particles with soft-particle fading.
 */

#include "uniforms.glsl"

#define BINDING_SAMPLER_DIFFUSE          0
#define BINDING_SAMPLER_NEXT_DIFFUSE     1
#define BINDING_SAMPLER_DEPTH_ATTACHMENT 2

#include "soften.glsl"

layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_DIFFUSE)      uniform sampler2D textureDiffusemap;
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_NEXT_DIFFUSE) uniform sampler2D textureNextDiffusemap;

layout (location = 0) in vec2 inDiffusemap;
layout (location = 1) in vec2 inNextDiffusemap;
layout (location = 2) in vec3 inColor;
layout (location = 3) in float inLerp;
layout (location = 4) in float inLighting;
layout (location = 5) in vec3 inDiffuse;

layout (location = 0) out vec4 outColor;

/**
 * @brief Shades sprites with optional lighting and soft-particle fading.
 */
void main(void) {

  const vec3 textureColor = mix(
      texture(textureDiffusemap, inDiffusemap).rgb,
      texture(textureNextDiffusemap, inNextDiffusemap).rgb,
      inLerp);

  vec3 color = inColor;
  if (inLighting > 0.0) {
    color = mix(color, color * inDiffuse, inLighting);
  }

  // portals do not have their own depth buffer copy, so don't soften
  float softness = viewType == VIEW_PORTAL ? 1.0 : soften();

  outColor = vec4(textureColor * color * softness, 1.0);
}
