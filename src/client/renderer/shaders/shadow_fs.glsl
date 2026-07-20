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

layout (location = 0) in vec3 in_position;
layout (location = 1) flat in float in_light_radius;

/**
 * @brief The alpha-test variant samples the base diffuse layer to discard
 * transparent texels (foliage, fences, grates), rather than casting a solid
 * silhouette. There is no separate depth pre-pass for shadows -- this
 * fragment program IS the depth write -- so the discard is load-bearing
 * here, not just an early-Z optimization like in bsp_fs/mesh_fs.
 */
#ifdef ALPHA_TEST
layout (location = 2) in vec2 in_diffusemap;

layout (std140, set = UNIFORM_SET, binding = BINDING_LOCALS) uniform shadow_material_block {
  float alpha_test;
};

layout (set = SAMPLER_SET, binding = 0) uniform sampler2DArray texture_material;
#endif

void main(void) {

#ifdef ALPHA_TEST
  if (texture(texture_material, vec3(in_diffusemap, 0)).a < alpha_test) {
    discard;
  }
#endif

  const float dist = length(in_position) / in_light_radius;
  const float bias = clamp(dist * .08, 1.0 / in_light_radius, 8.0 / in_light_radius);

  gl_FragDepth = min(dist + bias, 1.0);
}
