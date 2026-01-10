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

in vertex_data {
  vec3 model_position;
  vec3 position;
  vec3 normal;
  vec3 smooth_normal;
  vec3 tangent;
  vec3 bitangent;
  vec2 diffusemap;
  vec4 color;
  vec3 ambient;
  float caustics;
  vec4 fog;
} vertex;

layout (location = 0) out vec4 out_color;

struct fragment_t {
  vec3 dir;
  float dist;
  vec3 normal;
  vec3 tangent;
  vec3 bitangent;
  mat3 tbn;
  vec4 diffusemap;
  vec3 normalmap;
  vec4 specularmap;
  vec3 ambient;
  vec3 diffuse;
  vec3 specular;
} fragment;

uniform mat4 model;
uniform vec4 color;
uniform vec4 tint_colors[3];

/**
 * @brief
 */
vec4 sample_diffusemap() {
  return texture(texture_material, vec3(vertex.diffusemap, 0));
}

/**
 * @brief
 */
vec3 sample_normalmap() {
  vec3 normalmap = texture(texture_material, vec3(vertex.diffusemap, 1)).xyz * 2.0 - 1.0;
  vec3 roughness = vec3(vec2(material.roughness), 1.0);
  return normalize(fragment.tbn * (normalmap * roughness));
}

/**
 * @brief
 */
float toksvig_gloss(in vec3 normal, in float power) {
  float len_rcp = 1.0 / saturate(length(normal));
  return 1.0 / (1.0 + power * (len_rcp - 1.0));
}

/**
 * @brief
 */
vec4 sample_specularmap() {
  vec4 specularmap;

  specularmap.rgb = texture(texture_material, vec3(vertex.diffusemap, 2)).rgb * material.hardness;

  vec3 roughness = vec3(vec2(material.roughness), 1.0);
  vec3 normalmap0 = (texture(texture_material, vec3(vertex.diffusemap, 1), 0.0).xyz * 2.0 - 1.0) * roughness;
  vec3 normalmap1 = (texture(texture_material, vec3(vertex.diffusemap, 1), 1.0).xyz * 2.0 - 1.0) * roughness;

  float power = pow(1.0 + material.specularity, 4.0);
  specularmap.w = power * min(toksvig_gloss(normalmap0, power), toksvig_gloss(normalmap1, power));

  return specularmap;
}

/**
 * @brief
 */
vec4 sample_tintmap() {
  return texture(texture_material, vec3(vertex.diffusemap, 3));
}

/**
 * @brief
 */
vec4 sample_material_stage() {
  return texture(texture_stage, vertex.diffusemap);
}

/**
 * @brief
 */
float blinn(in vec3 light_dir) {
  return pow(max(0.0, dot(normalize(light_dir + fragment.dir), fragment.normalmap)), fragment.specularmap.w);
}

/**
 * @brief
 */
vec3 blinn_phong(in vec3 light_color, in vec3 light_dir) {
  return light_color * fragment.specularmap.rgb * blinn(light_dir);
}

/**
 * @brief Poisson disk samples for PCF soft shadows
 */
const vec3 poisson_disk[16] = vec3[](
                                     vec3( 0.2770745,  0.6951455,  0.6638531),
                                     vec3(-0.5932785, -0.1203284,  0.7954771),
                                     vec3( 0.4494750,  0.2469098, -0.8414080),
                                     vec3(-0.1460639, -0.5679667, -0.8098297),
                                     vec3( 0.6400498, -0.4071948,  0.6527679),
                                     vec3(-0.3631914,  0.7935778, -0.4889667),
                                     vec3( 0.1248857, -0.8975238,  0.4223446),
                                     vec3(-0.7720318,  0.4438458,  0.4568679),
                                     vec3( 0.8851806,  0.1653373, -0.4349109),
                                     vec3(-0.5238012, -0.7260296,  0.4437121),
                                     vec3( 0.3642682,  0.5968054,  0.7145538),
                                     vec3(-0.8331701, -0.3328346, -0.4437121),
                                     vec3( 0.5527260, -0.6985809, -0.4568679),
                                     vec3(-0.2407123,  0.3153157,  0.9181205),
                                     vec3( 0.7269405, -0.1430640,  0.6718765),
                                     vec3(-0.6444675,  0.6444675, -0.4076138)
                                     );

/**
 * @brief Simple pseudo-random function for per-pixel rotation
 */
float random_angle(vec3 seed) {
  return fract(sin(dot(seed, vec3(12.9898, 78.233, 45.164))) * 43758.5453) * 6.283185;
}

/**
 * @brief Rotate a 3D vector around a normalized axis with precomputed sin/cos
 */
vec3 rotate_around_axis(vec3 v, vec3 axis, float s, float c) {
  float oc = 1.0 - c;

  return vec3(
    (oc * axis.x * axis.x + c) * v.x + (oc * axis.x * axis.y - axis.z * s) * v.y + (oc * axis.z * axis.x + axis.y * s) * v.z,
    (oc * axis.x * axis.y + axis.z * s) * v.x + (oc * axis.y * axis.y + c) * v.y + (oc * axis.y * axis.z - axis.x * s) * v.z,
    (oc * axis.z * axis.x - axis.y * s) * v.x + (oc * axis.y * axis.z + axis.x * s) * v.y + (oc * axis.z * axis.z + c) * v.z
  );
}

/**
 * @brief Soft shadow sampling with PCF and Poisson disk
 */
float sample_shadow_cubemap_array(in light_t light, in int index) {

  vec3 light_to_frag = vertex.model_position - light.origin.xyz;
  float current_depth = length(light_to_frag) / depth_range.y;

  // Estimate penumbra size based on light radius (treat as light size)
  float light_size = light.origin.w * 3.0; // 3x radius term used for penumbra heuristic
  float dist_to_light = length(light_to_frag);
  float penumbra = light_size * (dist_to_light / light.origin.w);
  float filter_radius = penumbra * 0.005;  // Scale to texel units

  // Distance-based sample count (close = more samples, far = fewer)
  float view_dist = length(vertex.position);
  int num_samples = view_dist < 500.0 ? 16 : (view_dist < 1000.0 ? 9 : 4);

  // Per-pixel rotation to eliminate banding
  vec3 rotation_axis = normalize(light_to_frag);
  float angle = random_angle(vertex.model_position);
  float s = sin(angle);
  float c = cos(angle);

  float shadow = 0.0;

  for (int i = 0; i < num_samples; i++) {
    // Rotate Poisson sample to eliminate banding patterns
    vec3 rotated_offset = rotate_around_axis(poisson_disk[i], rotation_axis, s, c);
    vec3 sample_dir = light_to_frag + rotated_offset * filter_radius;
    vec4 shadowmap = vec4(sample_dir, index);

    shadow += texture(texture_shadow_cubemap_array, shadowmap, current_depth);
  }

  return shadow / float(num_samples);
}

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

  float lambert = max(0.0, dot(dir, fragment.normalmap));
  float shadow = sample_shadow_cubemap_array(light, index);

  fragment.diffuse += color * lambert * shadow;
  fragment.specular += blinn_phong(color * shadow, dir);
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
void light_and_shadow(void) {

  fragment.normalmap = sample_normalmap();
  fragment.specularmap = sample_specularmap();

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
    int index = dynamic_lights[i];
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

  fragment.dir = normalize(-vertex.position);
  fragment.dist = length(vertex.position);
  fragment.tbn = mat3(normalize(vertex.tangent), normalize(vertex.bitangent), normalize(vertex.normal));

  if ((stage.flags & STAGE_MATERIAL) == STAGE_MATERIAL) {

	  fragment.diffusemap = sample_diffusemap() * vertex.color;

	  if (fragment.diffusemap.a < material.alpha_test) {
  	  discard;
	  }

	  vec4 tintmap = sample_tintmap();
	  fragment.diffusemap.rgb *= 1.0 - tintmap.a;
	  fragment.diffusemap.rgb += (tint_colors[0] * tintmap.r).rgb * tintmap.a;
	  fragment.diffusemap.rgb += (tint_colors[1] * tintmap.g).rgb * tintmap.a;
	  fragment.diffusemap.rgb += (tint_colors[2] * tintmap.b).rgb * tintmap.a;

	  light_and_shadow();

	  out_color = fragment.diffusemap;

	  out_color.rgb = max(out_color.rgb * (fragment.ambient + fragment.diffuse), 0.0);
	  out_color.rgb = max(out_color.rgb + fragment.specular, 0.0);
	  out_color.rgb = mix(out_color.rgb, vertex.fog.rgb, vertex.fog.a);

  } else {

	  fragment.diffusemap = sample_material_stage() * vertex.color * color;

	  out_color = fragment.diffusemap;

//	  if ((stage.flags & STAGE_LIGHTMAP) == STAGE_LIGHTMAP) {
//
//  	  fragment.ambient = vertex.ambient * max(0.0, dot(fragment.normal, fragment.normalmap));
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
