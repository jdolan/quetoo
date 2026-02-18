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

uniform mat4 model;
uniform int block;

in common_vertex_t vertex;

out vec4 out_color;

common_fragment_t fragment;

/**
 * @brief Calculates the augmented texcoord for parallax occlusion mapping.
 * @see https://learnopengl.com/Advanced-Lighting/Parallax-Mapping
 */
void parallax_occlusion_mapping() {

  fragment.parallax = vertex.diffusemap;

  if (material.parallax == 0.0 || fragment.lod > 4.0) {
    return;
  }

  float num_samples = mix(32.0, 8.0, min(fragment.lod * 0.25, 1.0));

  vec2 texel = 1.0 / textureSize(texture_material, 0).xy;
  vec3 dir = normalize(fragment.view_dir * vertex.tbn);
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
 */
void light_and_shadow(void) {

  // For distant fragments, use simple vertex lighting
  if (fragment.view_dist >= lighting_distance) {
    fragment.ambient = vec3(0.0);
    fragment.diffuse = vertex.lighting;
    fragment.specular = vec3(0.0);
    return;
  }

  // For close fragments, do full per-fragment lighting
  fragment.normal_sample = sample_material_normal(fragment.parallax, vertex.tbn);
  fragment.specular_sample = sample_material_specular(fragment.parallax);

  vec3 sky = textureLod(texture_sky, normalize(vertex.model_normal), 6).rgb;

  fragment.ambient = pow(vec3(1.0) + sky, vec3(2.0)) * ambient * voxel_exposure(vertex.voxel);
  fragment.diffuse = vec3(0.0);
  fragment.specular = vec3(0.0);

  if (editor == 0) {
    fragment_lighting(vertex, fragment);
  } else {
    for (int i = 0; i < MAX_LIGHTS; i++) {
      int index = active_lights[i];
      if (index == -1) {
        break;
      }
      fragment_light(vertex, fragment, index);
    }
  }
}

/**
 * @brief
 */
void main(void) {

  if (wireframe != 0) {
    out_color = vec4(0.8, 0.8, 0.8, 1.0);
    return;
  }

  fragment.view_dir = normalize(-vertex.position);
  fragment.view_dist = length(vertex.position);
  fragment.lod = textureQueryLod(texture_material, vertex.diffusemap).x;

  parallax_occlusion_mapping();

  if ((stage.flags & STAGE_MATERIAL) == STAGE_MATERIAL) {

    fragment.diffuse_sample = sample_material_diffuse(fragment.parallax) * vertex.color;

    if (fragment.diffuse_sample.a < material.alpha_test) {
      discard;
    }

    light_and_shadow();

    fragment.fog = fragment_fog(vertex, fragment);

    out_color = fragment.diffuse_sample;

    out_color.rgb *= (fragment.ambient + fragment.diffuse);
    out_color.rgb += fragment.specular;
    out_color.rgb = mix(out_color.rgb, fragment.fog.rgb, fragment.fog.a);

  } else {

    vec2 st = fragment.parallax;

    if ((stage.flags & STAGE_WARP) == STAGE_WARP) {
      st += texture(texture_warp, st + vec2(ticks * stage.warp.x * 0.000125)).xy * stage.warp.y;
    }

    fragment.diffuse_sample = sample_material_stage(st) * vertex.color;

    out_color = fragment.diffuse_sample;

    //    if ((stage.flags & STAGE_LIGHTMAP) == STAGE_LIGHTMAP) {
    //
    //      fragment_lighting(vertex, fragment);
    //
    //      out_color.rgb *= (fragment.ambient + fragment.diffuse);
    //    }

    if ((stage.flags & STAGE_FOG) == STAGE_FOG) {
      fragment.fog = fragment_fog(vertex, fragment);
      out_color.rgb = mix(out_color.rgb, fragment.fog.rgb, fragment.fog.a);
    }
  }
}

