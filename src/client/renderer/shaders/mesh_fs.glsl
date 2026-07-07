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
 * Blinn-Phong specular, shadows, clustered voxel + dynamic lights), sharing the
 * BSP fragment stack and its descriptor layout (see bsp_fs). Unlike BSP surfaces,
 * mesh entity normal maps never carry a heightmap, so there is no parallax
 * occlusion mapping or parallax self-shadowing here -- fragment.parallax is
 * always just the plain diffusemap texcoord. Material stages and the EF_SHELL
 * overlay share this shader with the base pass via a runtime branch on
 * material.flags, exactly like bsp_fs -- see main() below.
 */

#define UNIFORMS_LIGHT_CULL
#define VOXEL_CAUSTICS_OCCLUSION
#define LIGHT_SKY
#define MATERIAL_TINTS
#include "uniforms.glsl"

/*
 * The mesh program's own binding map (fragment stage), mirroring bsp_fs's
 * shape exactly -- material.glsl's material_block additionally carries the
 * per-entity tint colors here (MATERIAL_TINTS, defined above), since they're
 * material-related too; active_lights stays in the generic light_cull_block
 * (uniforms.glsl, BINDING_LOCALS) it has nothing to do with tints or stages.
 * The vertex stage's own map is defined in mesh_vs.glsl.
 */
#define BINDING_SAMPLER_MATERIAL         0
#define BINDING_SAMPLER_VOXEL_LIGHT_DATA 1
#define BINDING_SAMPLER_SHADOW_ATLAS_0   2 // one sampler2DShadow per cube face
#define BINDING_SAMPLER_SHADOW_ATLAS_1   3
#define BINDING_SAMPLER_SHADOW_ATLAS_2   4
#define BINDING_SAMPLER_SHADOW_ATLAS_3   5
#define BINDING_SAMPLER_SHADOW_ATLAS_4   6
#define BINDING_SAMPLER_SHADOW_ATLAS_5   7
#define BINDING_SAMPLER_VOXEL_CAUSTICS   8
#define BINDING_SAMPLER_VOXEL_OCCLUSION  9
#define BINDING_SAMPLER_SKY_AMBIENT      10
#define BINDING_SAMPLER_STAGE            11
#define BINDING_SAMPLER_STAGE_NEXT       12
#define BINDING_STORAGE_LIGHTS              13
#define BINDING_STORAGE_VOXEL_LIGHT_INDICES 14
#define BINDING_UNIFORMS_MATERIAL        2

#include "common.glsl"
#include "material.glsl"
#include "voxel.glsl"
#include "light.glsl"

layout (location = 0) in common_vertex_t vertex;

layout (location = 0) out vec4 out_color;

// A float depth copy for the sprite pass to sample (soft particles); see r_framebuffer.c.
layout (location = 1) out float out_depth;

common_fragment_t fragment;

/**
 * @brief Samples the material normal/specular maps and computes per-fragment
 * lighting, with distance-based LOD: distant fragments (and the player-model
 * preview, which has no BSP voxel data) reuse the vertex shader's cheap
 * per-vertex ambient+diffuse instead.
 */
void mesh_fragment_lighting(in common_vertex_t vertex, inout common_fragment_t fragment) {

  if (fragment.view_dist >= lighting_distance || view_type == VIEW_PLAYER_MODEL) {
    fragment.ambient = vertex.ambient;
    fragment.diffuse = vertex.diffuse;
    fragment.specular = vec3(0.0);
    fragment.caustics = 0.0;
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
}

/**
 * @brief Animated mesh: diffuse material with full per-fragment lighting and
 * player-skin tinting (material.flags == STAGE_NONE), or a blended material
 * stage / shell overlay. One shader, one pipeline: see bsp_fs's main() for why.
 */
void main(void) {

  out_depth = gl_FragCoord.z;

  fragment.view_dir = normalize(-vertex.position);
  fragment.view_dist = length(vertex.position);
  fragment.lod = textureQueryLod(texture_material, vertex.diffusemap).x;

  // Mesh entity normal maps never carry a heightmap, so there is no parallax
  // occlusion mapping to apply here (see bsp_fs for that).
  fragment.parallax = vertex.diffusemap;

  if (material.flags == STAGE_NONE) {

    fragment.diffuse_sample = sample_material_diffuse(fragment.parallax);

    if ((material.surface & SURF_ALPHA_TEST) == SURF_ALPHA_TEST) {
      if (fragment.diffuse_sample.a < material.alpha_test) {
        discard;
      }
    }

    // Player-skin tint: blend the entity tint colors in by the material tint map.
    vec4 tintmap = sample_material_tint(fragment.parallax);
    fragment.diffuse_sample.rgb *= 1.0 - tintmap.a;
    fragment.diffuse_sample.rgb += (material.tint_colors[0] * tintmap.r).rgb * tintmap.a;
    fragment.diffuse_sample.rgb += (material.tint_colors[1] * tintmap.g).rgb * tintmap.a;
    fragment.diffuse_sample.rgb += (material.tint_colors[2] * tintmap.b).rgb * tintmap.a;

    out_color = fragment.diffuse_sample * vertex.color;

    // The player-model preview (menu) has no world, so no voxel/shadow data to
    // sample; light it with a flat, neutral ambient instead of full per-fragment
    // lighting. Matches the GL renderer's VIEW_PLAYER_MODEL shortcut.
    if (view_type == VIEW_PLAYER_MODEL) {
      fragment.ambient = vec3(0.666);
      fragment.diffuse = vec3(0.0);
      fragment.specular = vec3(0.0);
    } else {
      mesh_fragment_lighting(vertex, fragment);
    }

    out_color.rgb *= (fragment.ambient + fragment.diffuse);
    out_color.rgb += fragment.specular;

  } else {

    // Material stage / shell pass: sample the stage texture, optionally lit
    // and/or emissive, blended over the base surface.
    fragment.diffuse_sample = sample_material_stage(fragment.parallax) * vertex.color;

    out_color = fragment.diffuse_sample;

    if ((material.flags & STAGE_LIGHTING) == STAGE_LIGHTING) {
      mesh_fragment_lighting(vertex, fragment);
      out_color.rgb *= mix(vec3(1.0), fragment.ambient + fragment.diffuse, material.lighting);
      out_color.rgb += fragment.specular * material.lighting;
    }

    if ((material.flags & STAGE_EMISSIVE) == STAGE_EMISSIVE) {
      out_color.rgb += fragment.diffuse_sample.rgb * material.emissive;
    }
  }
}
