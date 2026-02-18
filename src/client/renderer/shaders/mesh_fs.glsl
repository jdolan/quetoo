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

in common_vertex_t vertex;

layout (location = 0) out vec4 out_color;

common_fragment_t fragment;

uniform mat4 model;
uniform vec4 color;
uniform vec4 tint_colors[3];

/**
 * @brief
 */
void light_and_shadow_light(in int index) {

  light_t light = lights[index];

  vec3 dir = light.origin.xyz - vertex.model_position;

  float radius = light.origin.w;
  float dist = length(dir);
  float atten = clamp(1.0 - dist / radius, 0.0, 1.0);
  if (atten <= 0.0) {
	  return;
  }

  vec3 color = light.color.rgb * light.color.a * atten * modulate;

  dir = normalize(view * vec4(dir, 0.0)).xyz;

  float lambert = max(0.0, dot(dir, fragment.normal_sample));
  
  float shadow = sample_shadow_cubemap_array(light, index, vertex, fragment);

  fragment.diffuse += color * lambert * shadow;
  fragment.specular += blinn_phong(color * shadow, dir, fragment);
}

/**
 * @brief
 */
void light_and_shadow_caustics() {

  if (vertex.caustics == 0.0) {
	  return;
  }

  float noise = noise3d(vertex.model_position * .05 + (ticks / 1000.0) * 0.5);

  // make the inner edges stronger, clamp to 0-1

  float thickness = 0.02;
  float glow = 5.0;

  noise = clamp(pow((1.0 - abs(noise)) + thickness, glow), 0.0, 1.0);

  vec3 light = fragment.ambient + fragment.diffuse;
  fragment.diffuse += max(vec3(0.0), light * vertex.caustics * noise);
}

/**
 * @brief
 */
/**
 * @brief Calculate lighting and shadows with distance-based LOD.
 * @details For distant fragments (>= lighting_distance), uses pre-calculated vertex
 * lighting (ambient + diffuse, no shadows or specular). For close fragments, performs
 * full per-fragment lighting with shadows, specular, and caustics.
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
  fragment.normal_sample = sample_material_normal(vertex.diffusemap, fragment.tbn);
  fragment.specular_sample = sample_material_specular(vertex.diffusemap);

  fragment.ambient = vertex.ambient;
  fragment.diffuse = vec3(0.0);
  fragment.specular = vec3(0.0);;

  ivec3 voxel = voxel_xyz(vertex.model_position);
  ivec2 data = voxel_light_data(voxel);

  for (int i = 0; i < data.y; i++) {
    int index = voxel_light_index(data.x + i);
    light_and_shadow_light(index);
  }

  for (int i = 0; i < MAX_DYNAMIC_LIGHTS; i++) {
    int index = active_lights[i];
    if (index == -1) {
      break;
    }

    light_and_shadow_light(index);
  }

  light_and_shadow_caustics();
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
  fragment.tbn = mat3(normalize(vertex.tangent), normalize(vertex.bitangent), normalize(vertex.normal));

  if ((stage.flags & STAGE_MATERIAL) == STAGE_MATERIAL) {

	  fragment.diffuse_sample = sample_material_diffuse(vertex.diffusemap) * vertex.color;

	  if (fragment.diffuse_sample.a < material.alpha_test) {
  	  discard;
	  }

	  vec4 tintmap = sample_material_tint(vertex.diffusemap);
	  fragment.diffuse_sample.rgb *= 1.0 - tintmap.a;
	  fragment.diffuse_sample.rgb += (tint_colors[0] * tintmap.r).rgb * tintmap.a;
	  fragment.diffuse_sample.rgb += (tint_colors[1] * tintmap.g).rgb * tintmap.a;
	  fragment.diffuse_sample.rgb += (tint_colors[2] * tintmap.b).rgb * tintmap.a;

	  light_and_shadow();

	  out_color = fragment.diffuse_sample;

	  out_color.rgb = max(out_color.rgb * (fragment.ambient + fragment.diffuse), 0.0);
	  out_color.rgb = max(out_color.rgb + fragment.specular, 0.0);
	  out_color.rgb = mix(out_color.rgb, vertex.fog.rgb, vertex.fog.a);

  } else {

	  fragment.diffuse_sample = sample_material_stage(vertex.diffusemap) * vertex.color * color;

	  out_color = fragment.diffuse_sample;

//	  if ((stage.flags & STAGE_LIGHTMAP) == STAGE_LIGHTMAP) {
//
//  	  fragment.ambient = vertex.ambient * max(0.0, dot(fragment.normal, fragment.normal_sample));
//
//  	  light_and_shadow(); // FIXME ambient?
//
//  	  out_color.rgb *= (fragment.ambient + fragment.diffuse);
//	  }

	  if ((stage.flags & STAGE_FOG) == STAGE_FOG) {
  	  out_color.rgb = mix(out_color.rgb, vertex.fog.rgb, vertex.fog.a);
	  }
  }
}
