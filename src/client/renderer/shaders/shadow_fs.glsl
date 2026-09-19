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
 * @file shadow_fs.glsl
 * @brief Writes biased radial depth for point-light shadow mapping.
 */

#include "uniforms.glsl"

layout (location = 0) in vec3 inPosition;
layout (location = 1) flat in float inLightRadius;

/**
 * @brief The alpha-test variant samples the base diffuse layer to discard
 * transparent texels (foliage, fences, grates), rather than casting a solid
 * silhouette. There is no separate depth pre-pass for shadows -- this
 * fragment program IS the depth write -- so the discard is load-bearing
 * here, not just an early-Z optimization like in bsp_fs/mesh_fs.
 */
#ifdef ALPHA_TEST
layout (location = 2) in vec2 inDiffusemap;

layout (std140, set = UNIFORM_SET, binding = BINDING_LOCALS) uniform shadowMaterialBlock {
  float alphaTest;
};

layout (set = SAMPLER_SET, binding = 0) uniform sampler2DArray textureMaterial;
#endif

void main(void) {

#ifdef ALPHA_TEST
  if (texture(textureMaterial, vec3(inDiffusemap, 0)).a < alphaTest) {
    discard;
  }
#endif

  const float dist = length(inPosition) / inLightRadius;
  const float bias = clamp(dist * .08, 1.0 / inLightRadius, 8.0 / inLightRadius);

  gl_FragDepth = min(dist + bias, 1.0);
}
