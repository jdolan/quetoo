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

#define VOXEL_CAUSTICS_OCCLUSION
#define LIGHT_SKY

#include "uniforms.glsl"

#define BINDING_SAMPLER_MATERIAL             0
#define BINDING_SAMPLER_SHADOW_ATLAS_0       1
#define BINDING_SAMPLER_SHADOW_ATLAS_1       2
#define BINDING_SAMPLER_SHADOW_ATLAS_2       3
#define BINDING_SAMPLER_SHADOW_ATLAS_3       4
#define BINDING_SAMPLER_SHADOW_ATLAS_4       5
#define BINDING_SAMPLER_SHADOW_ATLAS_5       6
#define BINDING_SAMPLER_VOXEL_CAUSTICS       7
#define BINDING_SAMPLER_VOXEL_OCCLUSION      8
#define BINDING_SAMPLER_SKY_AMBIENT          9
#define BINDING_SAMPLER_STAGE               10
#define BINDING_SAMPLER_STAGE_NEXT          11
#define BINDING_SAMPLER_WARP                12
#define BINDING_STORAGE_BSP_LIGHTS           13
#define BINDING_STORAGE_DYNAMIC_LIGHTS       14
#define BINDING_STORAGE_VOXEL_LIGHT_DATA     15
#define BINDING_STORAGE_VOXEL_LIGHT_INDICES  16
#define BINDING_UNIFORMS_MATERIAL            2

#include "common.glsl"
#include "material.glsl"
#include "voxel.glsl"

layout (std140, set = UNIFORM_SET, binding = BINDING_LOCALS) uniform bsp_locals_block {
  uvec4 active_dynamic_lights[MAX_DYNAMIC_LIGHTS / 128]; // 128 bits (4 x uint32) per uvec4
};

#include "light.glsl"

/**
 * @brief The procedural warp noise texture, for STAGE_WARP liquid surfaces.
 * @remarks BSP-only: unlike the other samplers above, this is not declared in
 * material.glsl since mesh materials never set STAGE_WARP (matching main).
 */
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_WARP) uniform sampler2D texture_warp;

layout (location = 0) in common_vertex_t vertex;

layout (location = 0) out vec4 out_color;

// A float depth copy for the sprite pass to sample (soft particles); see r_framebuffer.c.
layout (location = 1) out float out_depth;

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
 * @brief Calculate lighting and shadows for BSP with distance-based LOD.
 * @details Handles BSP-specific ambient (sky + voxel exposure) and editor mode.
 * Full per-fragment lighting blends out to the vertex shader's cheap
 * per-vertex lighting over [lighting_distance, lighting_distance +
 * LIGHTING_LOD_BLEND_DIST]; fully distant fragments take the vertex lighting
 * alone.
 */
void bsp_fragment_lighting(in common_vertex_t vertex, inout common_fragment_t fragment) {

  const float lod = clamp((fragment.view_dist - lighting_distance) / LIGHTING_LOD_BLEND_DIST, 0.0, 1.0);

  if (lod >= 1.0) {
    fragment.ambient = vertex.ambient;
    fragment.diffuse = vertex.diffuse;
    fragment.specular = vec3(0.0);
    return;
  }

  if ((material.flags & STAGE_LIGHTING_FLAT) == STAGE_LIGHTING_FLAT) {
    fragment.normal_sample = normalize(vertex.normal);
    fragment.specular_sample = vec4(fragment.diffuse_sample.rgb, pow(1.0 + material.specularity, 4.0));
  } else {
    fragment.normal_sample = sample_material_normal(fragment.parallax, mat3(vertex.tangent, vertex.bitangent, vertex.normal));
    fragment.specular_sample = sample_material_specular(fragment.parallax);
  }

  // Precompute per-pixel Poisson rotation for shadow PCF
  float angle = random_angle(vertex.model_position);
  fragment.shadow_sin_cos = vec2(sin(angle), cos(angle));

  fragment_lighting(vertex, fragment);

  fragment.ambient = mix(fragment.ambient, vertex.ambient, lod);
  fragment.diffuse = mix(fragment.diffuse, vertex.diffuse, lod);
  fragment.specular *= 1.0 - lod;
}

/**
 * @brief Opaque diffuse surfaces (material.flags == STAGE_NONE) get clustered +
 * dynamic lighting; material stages sample their own texture and are
 * optionally lit and/or emissive, blended over the base surface. One shader,
 * one pipeline: which branch runs is a runtime uniform, not a compile-time
 * variant, so a stage draw never requires a pipeline swap.
 * @details The alpha-test `discard` below is instead a *compile-time* variant
 * (`ALPHA_TEST`, see Makefile.am's `bsp_fs_alpha_test`), because a `discard`
 * anywhere in a fragment shader's source forces late (post-shader) depth
 * testing for every draw using that shader/pipeline, even ones that never
 * reach it. Material stages (the `else` branch) never discard, and most
 * opaque surfaces aren't alpha-tested, so those are compiled and drawn with
 * this file's default (discard-free) variant, preserving early-Z / Hi-Z for
 * them; only `SURF_ALPHA_TEST` surfaces pay for late-Z, via their own
 * dedicated pipeline (r_bsp_draw.pipeline_alpha_test).
 */
void main(void) {

  out_depth = gl_FragCoord.z;

  fragment.view_dir = normalize(-vertex.position);
  fragment.view_dist = length(vertex.position);
  fragment.lod = textureQueryLod(texture_material, vertex.diffusemap).x;

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

    bsp_fragment_lighting(vertex, fragment);

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
      bsp_fragment_lighting(vertex, fragment);
      out_color.rgb *= mix(vec3(1.0), fragment.ambient + fragment.diffuse, material.lighting);
      out_color.rgb += fragment.specular * material.lighting;
    }

    if ((material.flags & STAGE_EMISSIVE) == STAGE_EMISSIVE) {
      out_color.rgb += fragment.diffuse_sample.rgb * material.emissive;
    }
  }
}
