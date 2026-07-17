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

#define MATERIAL_TINTS

#include "uniforms.glsl"

#define BINDING_STORAGE_NUM_ACTIVE_SAMPLERS 12
#define BINDING_UNIFORMS_MATERIAL           2

#include "common.glsl"
#include "material.glsl"
#include "voxel.glsl"

layout (std140, set = UNIFORM_SET, binding = BINDING_LOCALS) uniform mesh_locals_block {
  uvec4 active_dynamic_lights[MAX_DYNAMIC_LIGHTS / 128];
};

#include "light.glsl"

layout (location = 0) in common_vertex_t vertex;

layout (location = 0) out vec4 out_color;

layout (location = 1) out float out_depth;

common_fragment_t fragment;

/**
 * @brief Computes per-fragment mesh lighting with a vertex-lighting fallback.
 */
void mesh_fragment_lighting(in common_vertex_t vertex, inout common_fragment_t fragment) {

  if (view_type == VIEW_PLAYER_MODEL) {
    fragment.ambient = vertex.ambient;
    fragment.diffuse = vertex.diffuse;
    fragment.specular = vec3(0.0);
    return;
  }

  fragment_lighting_lod(vertex, fragment);
}

/**
 * @brief Shades either the base mesh material pass or a material stage overlay.
 */
void main(void) {

  out_depth = gl_FragCoord.z;

  fragment.view_dir = normalize(-vertex.position);
  fragment.view_dist = length(vertex.position);

  fragment.parallax = vertex.diffusemap;

  if (material.flags == STAGE_NONE) {

    fragment.diffuse_sample = sample_material_diffuse(fragment.parallax);

#ifdef ALPHA_TEST
    if ((material.surface & SURF_ALPHA_TEST) == SURF_ALPHA_TEST) {
      if (fragment.diffuse_sample.a < material.alpha_test) {
        discard;
      }
    }
#endif

    vec4 tintmap = sample_material_tint(fragment.parallax);
    fragment.diffuse_sample.rgb *= 1.0 - tintmap.a;
    fragment.diffuse_sample.rgb += (material.tint_colors[0] * tintmap.r).rgb * tintmap.a;
    fragment.diffuse_sample.rgb += (material.tint_colors[1] * tintmap.g).rgb * tintmap.a;
    fragment.diffuse_sample.rgb += (material.tint_colors[2] * tintmap.b).rgb * tintmap.a;

    out_color = fragment.diffuse_sample * vertex.color;

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
