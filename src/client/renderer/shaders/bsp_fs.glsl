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

#include "uniforms.glsl"

/**
 * @brief See material.glsl sampler bindings.
 */
#define BINDING_SAMPLER_WARP                 12
#define BINDING_STORAGE_NUM_ACTIVE_SAMPLERS  13
#define BINDING_UNIFORMS_MATERIAL            2

/**
 * @brief BSP surfaces support parallax occlusion mapping and self-shadowing
 * from their heightmaps; mesh entities do not (see mesh_fs.glsl).
 */
#define PARALLAX_SELF_SHADOW

#include "common.glsl"
#include "material.glsl"
#include "voxel.glsl"

layout (std140, set = UNIFORM_SET, binding = BINDING_LOCALS) uniform bsp_locals_block {
  uvec4 active_dynamic_lights[MAX_DYNAMIC_LIGHTS / 128];
};

#include "light.glsl"

/**
 * @brief Warp texture for STAGE_WARP liquid surfaces.
 */
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_WARP) uniform sampler2D texture_warp;

layout (location = 0) in common_vertex_t vertex;

layout (location = 0) out vec4 out_color;
layout (location = 1) out float out_depth;

common_fragment_t fragment;

#define PARALLAX_SAMPLES_PER_TEXEL 6.0
#define PARALLAX_MIN_SAMPLES 24.0
#define PARALLAX_MAX_SAMPLES 96.0

/**
 * @brief Applies parallax occlusion mapping to the fragment texcoord.
 */
void parallax_occlusion_mapping(in common_vertex_t vertex, inout common_fragment_t fragment) {

  fragment.parallax = vertex.diffusemap;

  if (material.parallax == 0.0 || fragment.texture_lod > 2.0 ||
      fragment.view_dist >= lighting_distance + LIGHTING_LOD_BLEND_DIST) {
    return;
  }

  vec3 dir = normalize(fragment.view_dir * mat3(vertex.tangent, vertex.bitangent, vertex.normal));

  vec2 texel = 1.0 / textureSize(texture_material, 0).xy;
  vec2 p = ((dir.xy * texel) / max(dir.z, 0.1)) * material.parallax * material.parallax;

  float sweep = length(p / texel) / exp2(fragment.texture_lod);
  float budget = mix(PARALLAX_MAX_SAMPLES, PARALLAX_MIN_SAMPLES, fragment.texture_lod_normalized);
  float num_samples = clamp(ceil(sweep * PARALLAX_SAMPLES_PER_TEXEL), 1.0, budget);

  vec2 delta = p / num_samples;

  vec2 texcoord = vertex.diffusemap;
  vec2 prev_texcoord = vertex.diffusemap;

  float depth = 0.0;
  float layer = 1.0 / num_samples;
  float displacement = sample_material_displacement(texcoord, fragment.texture_lod);

  for (int i = 0; i < int(num_samples) && depth < displacement; i++) {
    depth += layer;
    prev_texcoord = texcoord;
    texcoord -= delta;
    displacement = sample_material_displacement(texcoord, fragment.texture_lod);
  }

  float a = displacement - depth;
  float b = sample_material_displacement(prev_texcoord, fragment.texture_lod) - depth + layer;

  fragment.parallax = mix(prev_texcoord, texcoord, a / (a - b));
}

/**
 * @brief Shades BSP base surfaces and material stages.
 */
void main(void) {

  out_depth = gl_FragCoord.z;

  fragment.view_dir = normalize(-vertex.position);
  fragment.view_dist = length(vertex.position);
  fragment.texture_lod = textureQueryLod(texture_material, vertex.diffusemap).x;
  int num_mipmap_levels = textureQueryLevels(texture_material);
  fragment.texture_lod_normalized = clamp(fragment.texture_lod / float(num_mipmap_levels - 1), 0.0, 1.0);

  parallax_occlusion_mapping(vertex, fragment);

  if (material.flags == STAGE_NONE) {

    fragment.diffuse_sample = sample_material_diffuse(fragment.parallax);

#ifdef ALPHA_TEST
    if ((material.surface & SURF_ALPHA_TEST) == SURF_ALPHA_TEST) {
      if (fragment.diffuse_sample.a < material.alpha_test) {
        discard;
      }
    }
#endif

    out_color = fragment.diffuse_sample;

    out_color *= vertex.color;

    fragment_lighting_lod(vertex, fragment);

    out_color.rgb *= (fragment.ambient + fragment.diffuse);
    out_color.rgb += fragment.specular;

  } else {

    vec2 st = fragment.parallax;

    if ((material.flags & STAGE_WARP) == STAGE_WARP) {
      st += (texture(texture_warp, st + vec2(ticks * material.warp.x * 0.000125)).xy - 0.5) * material.warp.y;
    }

    fragment.diffuse_sample = sample_material_stage(st) * vertex.color;

    out_color = fragment.diffuse_sample;

    if ((material.flags & STAGE_LIGHTING) == STAGE_LIGHTING) {
      fragment_lighting_lod(vertex, fragment);
      out_color.rgb *= mix(vec3(1.0), fragment.ambient + fragment.diffuse, material.lighting);
      out_color.rgb += fragment.specular * material.lighting;
    }

    if ((material.flags & STAGE_EMISSIVE) == STAGE_EMISSIVE) {
      out_color.rgb += fragment.diffuse_sample.rgb * material.emissive;
    }
  }
}
