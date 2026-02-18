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
  float layer = 1.0 / num_samples;
  float displacement = sample_displacement(texcoord);

  for (int i = 0; i < int(num_samples) && depth < displacement; i++) {
    depth += layer;
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
  return normalize(vertex.tbn * (normalmap * roughness));
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
 * @brief Calculates caustics based on voxel contents.
 */
/**
 * @brief Samples the pre-calculated caustics intensity from the voxel texture.
 */
float sample_voxel_caustics() {
  return voxel_caustics(vertex.voxel) * caustics;
}

/**
 * @brief Sample volumetric fog with distance-based LOD.
 * @details For distant fragments (>= fog_distance), uses pre-calculated vertex fog
 * for performance. For close fragments, performs full per-fragment raymarching
 * for high-quality dynamic lighting effects (e.g., rocket lighting fog).
 */
vec4 sample_voxel_fog() {

  // For distant fragments, use interpolated vertex fog (much cheaper)
  if (fragment.dist >= fog_distance) {
    return vertex.fog;
  }

  // For close fragments, do full per-fragment raymarching
  vec4 fog = vec4(0.0);

  float samples = clamp(fragment.dist / BSP_VOXEL_SIZE, 1.0, fog_samples);

  for (float i = 0; i < samples; i++) {

    vec3 xyz = mix(vertex.model_position, view[0].xyz, i / samples);
    vec3 uvw = mix(vertex.voxel, voxels.view_coordinate.xyz, i / samples);

    float fog_density_sample = voxel_fog_density(uvw);
    
    if (fog_density_sample > 0.0) {
      vec3 fog_lighting = light_fog(xyz);
      fog += vec4(fog_lighting, fog_density_sample * fog_density) * min(1.0, samples - i);
    }
    
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
void light_and_shadow_light(in int index) {

  light_t light = lights[index];

  vec3 dir = light.origin.xyz - vertex.model_position;
  float dist = length(dir);
  float radius = light.origin.w;
  float atten = clamp(1.0 - dist / radius, 0.0, 1.0);
  if (atten <= 0.0) {
    return;
  }

  dir = normalize(view * vec4(dir, 0.0)).xyz;

  vec3 color = light.color.rgb * light.color.a * atten * modulate;

  common_fragment_t f;
  f.view_dir = fragment.dir;
  f.view_dist = fragment.dist;
  f.normal_sample = fragment.normalmap;
  f.specular_sample = fragment.specularmap;
  
  float shadow = sample_shadow_cubemap_array(light, index, vertex, f);

  if (fragment.lod < 4.0 && material.shadow > 0.0) {
    shadow *= parallax_self_shadow(dir);
  }

  if (shadow <= 0.0) {
    return;
  }

  float lambert = max(0.0, dot(dir, fragment.normalmap));

  fragment.diffuse += color * lambert * shadow;
  fragment.specular += blinn_phong(color * shadow, dir, f);
}

/**
 * @brief
 */
void light_and_shadow_caustics() {

  fragment.caustics = sample_voxel_caustics();

  if (fragment.caustics == 0.0) {
    return;
  }

  float noise = noise3d(vertex.model_position * .05 + (ticks / 1000.0) * 0.5);

  // make the inner edges stronger, clamp to 0-1

  float thickness = 0.02;
  float glow = 5.0;

  noise = clamp(pow((1.0 - abs(noise)) + thickness, glow), 0.0, 1.0);

  vec3 light = fragment.ambient + fragment.diffuse;
  fragment.diffuse += max(vec3(0.0), light * fragment.caustics * noise);
}

/**
 * @brief Calculate lighting and shadows with distance-based LOD.
 * @details For distant fragments (>= lighting_distance), uses pre-calculated vertex
 * lighting (ambient + diffuse, no shadows or specular). For close fragments, performs
 * full per-fragment lighting with shadows, specular, and caustics.
 */
void light_and_shadow(void) {

  // For distant fragments, use simple vertex lighting
  if (fragment.dist >= lighting_distance) {
    fragment.ambient = vec3(0.0);
    fragment.diffuse = vertex.lighting;
    fragment.specular = vec3(0.0);
    return;
  }

  // For close fragments, do full per-fragment lighting
  fragment.normalmap = sample_normalmap();
  fragment.specularmap = sample_specularmap();

  vec3 sky = textureLod(texture_sky, normalize(vertex.model_normal), 6).rgb;

  fragment.ambient = pow(vec3(1.0) + sky, vec3(2.0)) * ambient * voxel_exposure(vertex.voxel);
  fragment.diffuse = vec3(0.0);
  fragment.specular = vec3(0.0);

  if (editor == 0) {
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

  } else {
    for (int i = 0; i < MAX_LIGHTS; i++) {
      int index = active_lights[i];
      if (index == -1) {
        break;
      }

      light_and_shadow_light(index);
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

  fragment.dir = normalize(-vertex.position);
  fragment.dist = length(vertex.position);
  fragment.lod = textureQueryLod(texture_material, vertex.diffusemap).x;

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

