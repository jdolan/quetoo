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

/**
 * @file shadow_vs.glsl
 * @brief Transforms shadow caster vertices into the current point-light cube face.
 */

#include "uniforms.glsl"

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNextPosition;
#ifdef ALPHA_TEST
layout (location = 2) in vec2 inDiffusemap;
#endif

/**
 * @brief Per-draw shadow transform and light origin.
 */
layout (std140, set = UNIFORM_SET, binding = BINDING_LOCALS) uniform localsBlock {
  mat4 model;
  mat4 lightView;
  vec4 lightOrigin;
  float lerp;
};

layout (location = 0) out vec3 outPosition;
layout (location = 1) flat out float outLightRadius;
#ifdef ALPHA_TEST
layout (location = 2) out vec2 outDiffusemap;
#endif

invariant gl_Position;

/**
 * @brief Outputs light-relative positions for shadow depth rendering.
 */
void main(void) {

  const vec3 position = vec3(model * vec4(mix(inPosition, inNextPosition, lerp), 1.0)) - lightOrigin.xyz;

  outPosition = position;
  outLightRadius = lightOrigin.w;
#ifdef ALPHA_TEST
  outDiffusemap = inDiffusemap;
#endif

  gl_Position = lightProjection * lightView * vec4(position, 1.0);
}
