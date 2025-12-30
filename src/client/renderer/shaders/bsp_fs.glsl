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

in vertex_data {
  vec3 model;
  vec3 position;
  vec3 normal;
  vec3 tangent;
  vec3 bitangent;
  mat3 tbn;
  mat3 inverse_tbn;
  vec2 diffusemap;
  vec3 cubemap;
  vec3 voxel;
  vec4 color;
} vertex;

layout (location = 0) out vec4 out_color;

struct fragment_t {
  vec3 dir;
  float dist;
  float lod;
  vec3 normal;
  vec3 tangent;
  vec3 bitangent;
  vec2 parallax;
  vec4 diffusemap;
  vec3 normalmap;
  vec4 specularmap;
  vec3 ambient;
  vec3 diffuse;
  float caustics;
  vec3 specular;
  vec4 fog;
} fragment;

/**
 * @brief Samples the heightmap at the given texture coordinate.
 */
float sample_heightmap(vec2 texcoord) {
  return textureLod(texture_material, vec3(texcoord, 1), fragment.lod).w;
}

/**
 * @brief Sampels the displacement map at the given texture coordinate and lod.
 */
float sample_displacement(vec2 texcoord) {
  return 1.0 - sample_heightmap(texcoord);
}

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
  vec3 dir = normalize(fragment.dir * vertex.tbn);
  vec2 p = ((dir.xy * texel) / dir.z) * material.parallax * material.parallax;
  vec2 delta = p / num_samples;

  vec2 texcoord = vertex.diffusemap;
  vec2 prev_texcoord = vertex.diffusemap;

  float depth = 0.0;
  float displacement = 0.0;
  float layer = 1.0 / num_samples;

  for (displacement = sample_displacement(texcoord); depth < displacement; depth += layer) {
    prev_texcoord = texcoord;
    texcoord -= delta;
    displacement = sample_displacement(texcoord);
  }

  float a = displacement - depth;
  float b = sample_displacement(prev_texcoord) - depth + layer;

  fragment.parallax = mix(prev_texcoord, texcoord, a / (a - b));
}

/**
 * @brief Returns the shadow scalar for parallax self shadowing.
 * @param light_dir The light direction in view space.
 * @param texel The material texel size.
 * @return The self-shadowing scalar.
 */
float parallax_self_shadow(in vec3 light_dir) {

  // If the light angle is nearly perpendicular, skip self-shadowing
  if (dot(light_dir, normalize(vertex.tbn[2])) > 0.9) {
    return 1.0;
  }

  // LOD-based step count: 16 close, 4 far (bounded for performance)
  int max_steps = int(mix(16.0, 4.0, min(fragment.lod * 0.33, 1.0)));

  // Adaptive step size based on LOD (larger steps at distance)
  float step_scale = mix(1.0, 2.5, min(fragment.lod * 0.5, 1.0));

  vec2 texel = 1.0 / textureSize(texture_material, 0).xy;
  vec3 dir = normalize(vertex.inverse_tbn * light_dir);
  vec3 delta = vec3(dir.xy * texel, max(dir.z * length(texel), .01)) * step_scale;
  vec3 texcoord = vec3(fragment.parallax, sample_heightmap(fragment.parallax));

  float max_height = texcoord.z;
  for (int i = 0; i < max_steps && texcoord.z < 1.0; i++) {
    texcoord += delta;
    max_height = max(max_height, sample_heightmap(texcoord.xy));
  }

  float shadow = 1.0 - (max_height - texcoord.z) * material.shadow;
  return clamp(shadow, 0.0, 1.0);
}

/**
 * @brief
 */
vec4 sample_diffusemap() {
  return texture(texture_material, vec3(fragment.parallax, 0));
}

/**
 * @brief
 */
vec3 sample_normalmap() {
  vec3 normalmap = texture(texture_material, vec3(fragment.parallax, 1)).xyz * 2.0 - 1.0;
  vec3 roughness = vec3(vec2(material.roughness), 1.0);
  return normalize(vertex.tbn * normalize(normalmap * roughness));
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
  specularmap.rgb = texture(texture_material, vec3(fragment.parallax, 2)).rgb * material.hardness;

  vec3 roughness = vec3(vec2(material.roughness), 1.0);
  vec3 normalmap0 = (texture(texture_material, vec3(fragment.parallax, 1), 0.0).xyz * 2.0 - 1.0) * roughness;
  vec3 normalmap1 = (texture(texture_material, vec3(fragment.parallax, 1), 1.0).xyz * 2.0 - 1.0) * roughness;

  float power = pow(1.0 + material.specularity, 4.0);
  specularmap.w = power * min(toksvig_gloss(normalmap0, power), toksvig_gloss(normalmap1, power));

  return specularmap;
}

/**
 * @brief
 */
vec4 sample_material_stage(in vec2 texcoord) {
  return texture(texture_stage, texcoord);
}

/**
 * @brief
 */
float sample_voxel_caustics() {
  return texture(texture_voxel_diffuse, vertex.voxel).a;
}

/**
 * @brief
 */
vec4 sample_voxel_fog() {

  vec4 fog = vec4(0.0);

  float samples = clamp(fragment.dist / BSP_VOXEL_SIZE, 1.0, fog_samples);

  for (float i = 0; i < samples; i++) {

    vec3 xyz = mix(vertex.model, view[0].xyz, i / samples);
    vec3 uvw = mix(vertex.voxel, voxels.view_coordinate.xyz, i / samples);

    fog += texture(texture_voxel_fog, uvw) * vec4(vec3(1.0), fog_density) * min(1.0, samples - i);
    if (fog.a >= 1.0) {
      break;
    }
  }

  if (hmax(fog.rgb) > 1.0) {
    fog.rgb /= hmax(fog.rgb);
  }

  return clamp(fog, 0.0, 1.0);
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
 * @brief
 */
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
 * @brief Rotate a 3D vector around an arbitrary axis
 */
vec3 rotate_around_axis(vec3 v, vec3 axis, float angle) {
  axis = normalize(axis);
  float s = sin(angle);
  float c = cos(angle);
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

  int array = index / MAX_SHADOW_CUBEMAP_LAYERS;
  int layer = index % MAX_SHADOW_CUBEMAP_LAYERS;

  vec3 light_to_frag = vertex.model - light.origin.xyz;
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
  float angle = random_angle(vertex.model);
  vec3 rotation_axis = normalize(light_to_frag);

  float shadow = 0.0;

  for (int i = 0; i < num_samples; i++) {
    // Rotate Poisson sample to eliminate banding patterns
    vec3 rotated_offset = rotate_around_axis(poisson_disk[i], rotation_axis, angle);
    vec3 sample_dir = light_to_frag + rotated_offset * filter_radius;
    vec4 shadowmap = vec4(sample_dir, layer);

    switch (array) {
      case 0:
        shadow += texture(texture_shadow_cubemap_array0, shadowmap, current_depth);
        break;
      case 1:
        shadow += texture(texture_shadow_cubemap_array1, shadowmap, current_depth);
        break;
      case 2:
        shadow += texture(texture_shadow_cubemap_array2, shadowmap, current_depth);
        break;
      case 3:
        shadow += texture(texture_shadow_cubemap_array3, shadowmap, current_depth);
        break;
    }
  }

  return shadow / float(num_samples);
}

/**
 * @brief
 */
void light_and_shadow_light(in light_t light, in int index) {

  vec3 dir = light.origin.xyz - vertex.model;

  float radius = light.origin.w;
  float atten = clamp(1.0 - length(dir) / radius, 0.0, 1.0);
  if (atten <= 0.0) {
    return;
  }

  vec3 color = light.color.rgb * light.color.a * atten * modulate;

  float shadow = sample_shadow_cubemap_array(light, index);

  if (fragment.lod < 4.0 && material.shadow > 0.0) {
    shadow *= parallax_self_shadow(dir);
  }

  if (shadow <= 0.0) {
    return;
  }

  dir = normalize(view * vec4(dir, 0.0)).xyz;
  float lambert = max(0.0, dot(dir, fragment.normalmap));

  fragment.diffuse += color * lambert * shadow;
  fragment.specular += blinn_phong(color * shadow, dir);
}

/**
 * @brief
 */
void light_and_shadow_caustics() {

  fragment.caustics = sample_voxel_caustics();

  if (fragment.caustics == 0.0) {
    return;
  }

  float noise = noise3d(vertex.model * .05 + (ticks / 1000.0) * 0.5);

  // make the inner edges stronger, clamp to 0-1

  float thickness = 0.02;
  float glow = 5.0;

  noise = clamp(pow((1.0 - abs(noise)) + thickness, glow), 0.0, 1.0);

  vec3 light = fragment.ambient + fragment.diffuse;
  fragment.diffuse += max(vec3(0.0), light * fragment.caustics * noise);
}

/**
 * @brief
 */
void light_and_shadow(void) {

  fragment.normalmap = sample_normalmap();
  fragment.specularmap = sample_specularmap();

  vec3 sky = textureLod(texture_sky, normalize(vertex.cubemap), 6).rgb;

  fragment.ambient = pow(vec3(1.0) + sky, vec3(2.0)) * ambient;
  fragment.diffuse = vec3(0.0);
  fragment.specular = vec3(0.0);

  ivec3 voxel = voxel_xyz(vertex.model);
  ivec2 data = voxel_light_data(voxel);

  for (int i = 0; i < data.y; i++) {
    int light_index = voxel_light_index(data.x + i);

    light_t light = lights[light_index];
    light_and_shadow_light(light, light_index);
  }

  for (int i = 0; i < MAX_DYNAMIC_LIGHTS; i++) {
    int index = dynamic_lights[i];
    if (index == -1) {
      break;
    }

    light_t light = lights[index];

    float dist = distance(light.origin.xyz, vertex.model);
    if (dist < light.origin.w) {
      light_and_shadow_light(light, index);
    }
  }

  light_and_shadow_caustics();
}

/**
 * @brief
 */
void main(void) {

  fragment.dir = normalize(-vertex.position);
  fragment.dist = length(vertex.position);
  fragment.lod = textureQueryLod(texture_material, vertex.diffusemap).x;
  fragment.normal = normalize(vertex.normal);
  fragment.tangent = normalize(vertex.tangent);
  fragment.bitangent = normalize(vertex.bitangent);
  fragment.parallax = vertex.diffusemap;

  parallax_occlusion_mapping();

  if ((stage.flags & STAGE_MATERIAL) == STAGE_MATERIAL) {

    fragment.diffusemap = sample_diffusemap() * vertex.color;

    if (fragment.diffusemap.a < material.alpha_test) {
      discard;
    }

    light_and_shadow();

    fragment.fog = sample_voxel_fog();

    out_color = fragment.diffusemap;

    out_color.rgb *= (fragment.ambient + fragment.diffuse);
    out_color.rgb += fragment.specular;
    out_color.rgb = mix(out_color.rgb, fragment.fog.rgb, fragment.fog.a);

  } else {

    vec2 st = fragment.parallax;

    if ((stage.flags & STAGE_WARP) == STAGE_WARP) {
      st += texture(texture_warp, st + vec2(ticks * stage.warp.x * 0.000125)).xy * stage.warp.y;
    }

    fragment.diffusemap = sample_material_stage(st) * vertex.color;

    out_color = fragment.diffusemap;

    //    if ((stage.flags & STAGE_LIGHTMAP) == STAGE_LIGHTMAP) {
    //
    //      light_and_shadow();
    //
    //      out_color.rgb *= (fragment.ambient + fragment.diffuse);
    //    }

    if ((stage.flags & STAGE_FOG) == STAGE_FOG) {
      fragment.fog = sample_voxel_fog();
      out_color.rgb = mix(out_color.rgb, fragment.fog.rgb, fragment.fog.a);
    }
  }
}

