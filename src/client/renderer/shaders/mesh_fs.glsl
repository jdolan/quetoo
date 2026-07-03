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
 * The mesh fragment program: full per-fragment material lighting (normal maps,
 * Blinn-Phong specular, parallax occlusion, shadows, clustered voxel + dynamic
 * lights), sharing the BSP fragment stack and its descriptor layout (see bsp_fs).
 * TODO(#864): material stages, shells, and tints are ported in later increments;
 * parallax_occlusion_mapping is duplicated from bsp_fs pending a shared home.
 */

#define UNIFORMS_NO_SAMPLERS
#define UNIFORMS_LIGHT_CULL
#define VOXEL_CAUSTICS_OCCLUSION
#define LIGHT_SKY
#include "uniforms.glsl"
#include "common.glsl"
#include "material.glsl"
#include "voxel.glsl"
#include "light.glsl"

layout (location = 0) in common_vertex_t vertex;

layout (location = 0) out vec4 out_color;

/**
 * @brief Per-entity tint colors for player-skin colorization, blended in via the
 * material tint map (layer 3).
 */
layout (std140, set = UNIFORM_SET, binding = BINDING_UNIFORMS_TINTS) uniform tint_block {
  vec4 tint_colors[3];
};

common_fragment_t fragment;

/**
 * @brief Calculates the augmented texcoord for parallax occlusion mapping.
 * @see https://learnopengl.com/Advanced-Lighting/Parallax-Mapping
 */
void parallax_occlusion_mapping(in common_vertex_t vertex, inout common_fragment_t fragment) {

  fragment.parallax = vertex.diffusemap;

  if (material.parallax == 0.0 || fragment.lod > 4.0) {
    return;
  }

  float num_samples = mix(32.0, 8.0, min(fragment.lod * 0.25, 1.0));

  vec2 texel = 1.0 / textureSize(texture_material, 0).xy;
  vec3 dir = normalize(fragment.view_dir * mat3(vertex.tangent, vertex.bitangent, vertex.normal));
  dir.z = max(dir.z, 0.1);
  vec2 p = ((dir.xy * texel) / dir.z) * material.parallax * material.parallax;
  vec2 delta = p / num_samples;

  vec2 texcoord = vertex.diffusemap;
  vec2 prev_texcoord = vertex.diffusemap;

  float depth = 0.0;
  float layer = 1.0 / num_samples;
  float displacement = sample_material_displacement(texcoord, fragment.lod);

  for (int i = 0; i < int(num_samples) && depth < displacement; i++) {
    depth += layer;
    prev_texcoord = texcoord;
    texcoord -= delta;
    displacement = sample_material_displacement(texcoord, fragment.lod);
  }

  float a = displacement - depth;
  float b = sample_material_displacement(prev_texcoord, fragment.lod) - depth + layer;

  fragment.parallax = mix(prev_texcoord, texcoord, a / (a - b));
}

/**
 * @brief Animated mesh: diffuse material with full per-fragment lighting.
 */
void main(void) {

  fragment.view_dir = normalize(-vertex.position);
  fragment.view_dist = length(vertex.position);
  fragment.lod = textureQueryLod(texture_material, vertex.diffusemap).x;

  parallax_occlusion_mapping(vertex, fragment);

  fragment.diffuse_sample = sample_material_diffuse(fragment.parallax);

  if ((material.surface & SURF_ALPHA_TEST) == SURF_ALPHA_TEST) {
    if (fragment.diffuse_sample.a < material.alpha_test) {
      discard;
    }
  }

  // Player-skin tint: blend the entity tint colors in by the material tint map.
  vec4 tintmap = sample_material_tint(fragment.parallax);
  fragment.diffuse_sample.rgb *= 1.0 - tintmap.a;
  fragment.diffuse_sample.rgb += (tint_colors[0] * tintmap.r).rgb * tintmap.a;
  fragment.diffuse_sample.rgb += (tint_colors[1] * tintmap.g).rgb * tintmap.a;
  fragment.diffuse_sample.rgb += (tint_colors[2] * tintmap.b).rgb * tintmap.a;

  out_color = fragment.diffuse_sample * vertex.color;

  fragment.normal_sample = sample_material_normal(fragment.parallax, mat3(vertex.tangent, vertex.bitangent, vertex.normal));
  fragment.specular_sample = sample_material_specular(fragment.parallax);

  // Per-pixel Poisson rotation for shadow PCF.
  float angle = random_angle(vertex.model_position);
  fragment.shadow_sin_cos = vec2(sin(angle), cos(angle));

  fragment_lighting(vertex, fragment);

  out_color.rgb *= (fragment.ambient + fragment.diffuse);
  out_color.rgb += fragment.specular;
}
