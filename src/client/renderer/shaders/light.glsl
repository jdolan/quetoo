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

/**
 * @file light.glsl
 * @brief Implements shared vertex and fragment lighting helpers for lit materials.
 * @remarks Include after common.glsl, material.glsl, and voxel.glsl.
 */

#include "light_types.glsl"

/**
 * @brief Blends full fragment lighting down to vertex lighting beyond lighting_distance.
 */
#define LIGHTING_LOD_BLEND_DIST 128.0

#if defined(FRAGMENT_SHADER)
/**
 * @brief 2D Poisson disk samples for PCF soft shadows.
 */
const vec2 poisson_disk[16] = vec2[](
  vec2( 0.2770745,  0.6951455),
  vec2(-0.5932785, -0.1203284),
  vec2( 0.4494750,  0.2469098),
  vec2(-0.1460639, -0.5679667),
  vec2( 0.6400498, -0.4071948),
  vec2(-0.3631914,  0.7935778),
  vec2( 0.1248857, -0.8975238),
  vec2(-0.7720318,  0.4438458),
  vec2( 0.8851806,  0.1653373),
  vec2(-0.5238012, -0.7260296),
  vec2( 0.3642682,  0.5968054),
  vec2(-0.8331701, -0.3328346),
  vec2( 0.5527260, -0.6985809),
  vec2(-0.2407123,  0.3153157),
  vec2( 0.7269405, -0.1430640),
  vec2(-0.6444675,  0.6444675)
);

/**
 * @brief Simple pseudo-random function for per-pixel rotation.
 */
float random_angle(vec3 seed) {
  return fract(sin(dot(seed, vec3(12.9898, 78.233, 45.164))) * 43758.5453) * 6.283185;
}

/**
 * @brief Determine cubemap face index and compute face UV from direction vector.
 * @brief Computes the cubemap face and face UV for a light-to-fragment direction.
 */
void cubemap_face_uv(in vec3 dir, out int face, out vec2 face_uv, out float ma) {

  vec3 ad = abs(dir);
  float sc, tc;

  if (ad.x >= ad.y && ad.x >= ad.z) {
    ma = ad.x;
    if (dir.x > 0.0) {
      face = 0; sc = -dir.z; tc = -dir.y;
    } else {
      face = 1; sc = dir.z; tc = -dir.y;
    }
  } else if (ad.y >= ad.x && ad.y >= ad.z) {
    ma = ad.y;
    if (dir.y > 0.0) {
      face = 2; sc = dir.x; tc = dir.z;
    } else {
      face = 3; sc = dir.x; tc = -dir.z;
    }
  } else {
    ma = ad.z;
    if (dir.z > 0.0) {
      face = 4; sc = dir.x; tc = -dir.y;
    } else {
      face = 5; sc = -dir.x; tc = -dir.y;
    }
  }

  face_uv = vec2(sc, tc) / (2.0 * ma) + 0.5;
}

/**
 * @brief Samples the shadow atlas face texture selected by face.
 */
float sample_shadow_face(in int face, in vec3 uvw) {
  if (face == 0) {
    return texture(texture_shadow_atlas_0, uvw);
  } else if (face == 1) {
    return texture(texture_shadow_atlas_1, uvw);
  } else if (face == 2) {
    return texture(texture_shadow_atlas_2, uvw);
  } else if (face == 3) {
    return texture(texture_shadow_atlas_3, uvw);
  } else if (face == 4) {
    return texture(texture_shadow_atlas_4, uvw);
  } else {
    return texture(texture_shadow_atlas_5, uvw);
  }
}

/**
 * @brief Samples the shadow atlas for a light with PCF filtering.
 */
float sample_shadow_atlas(in light_t light, in common_vertex_t v, in common_fragment_t f, in float atten) {

  if (light.shadow.x < 0.0) {
    return 1.0;
  }

  vec2 texture_size = vec2(textureSize(texture_shadow_atlas_0, 0).xy);
  float tile_px = texture_size.x / float(SHADOW_ATLAS_LIGHTS_PER_ROW);
  vec2 tile_origin = light.shadow.xy / texture_size;
  float tile_uv = tile_px / texture_size.x;

  vec3 light_to_frag = v.model_position - light.origin.xyz;
  float dist_to_light = length(light_to_frag);

  float light_size = light.origin.w * 3.0;
  float filter_radius = light_size * (dist_to_light / light.origin.w) * 0.005;

  // Offset the receiver along its normal to combat acne. Every PCF tap
  // below is compared against a single reference depth computed here, so
  // the offset must cover the worst-case depth variation across the
  // *entire* filter footprint (filter_radius), not just one texel --
  // otherwise grazing/curved surfaces alias against their neighboring taps.
  // Note: v.normal is view-space; v.model_normal is the world/model-space
  // normal that actually matches v.model_position and light.origin here.
  float n_dot_l = max(dot(v.model_normal, normalize(-light_to_frag)), 0.0);
  vec3 offset_position = v.model_position + v.model_normal * filter_radius * (1.0 - n_dot_l);

  light_to_frag = offset_position - light.origin.xyz;
  dist_to_light = length(light_to_frag);
  float current_depth = dist_to_light / light.origin.w;

  int face;
  vec2 fuv;
  float ma;
  cubemap_face_uv(light_to_frag, face, fuv, ma);

  fuv.y = 1.0 - fuv.y;

  vec2 half_texel = 0.5 / texture_size;
  vec2 tile_min = tile_origin + half_texel;
  vec2 tile_max = tile_origin + vec2(tile_uv) - half_texel;

  float filter_uv = filter_radius / (2.0 * max(ma, 0.001));

  float importance = atten * clamp(1.0 - f.view_dist / 2048.0, 0.0, 1.0);
  int num_samples = importance > 0.3 ? 8 : (importance > 0.1 ? 4 : 2);

  float s = f.shadow_sin_cos.x;
  float c = f.shadow_sin_cos.y;

  float shadow = 0.0;

  for (int i = 0; i < num_samples; i++) {
    vec2 rotated = vec2(c * poisson_disk[i].x - s * poisson_disk[i].y,
                        s * poisson_disk[i].x + c * poisson_disk[i].y);

    vec2 sample_fuv = fuv + rotated * filter_uv;

    vec2 atlas_uv = tile_origin + sample_fuv * vec2(tile_uv);

    atlas_uv = clamp(atlas_uv, tile_min, tile_max);

    shadow += sample_shadow_face(face, vec3(atlas_uv, current_depth));
  }

  return shadow / float(num_samples);
}

/**
 * @brief Evaluates the Blinn specular term.
 */
float blinn(in vec3 light_dir, in common_fragment_t f) {
  return pow(max(0.0, dot(normalize(light_dir + f.view_dir), f.normal_sample)), f.specular_sample.w);
}

/**
 * @brief Evaluates the Blinn-Phong specular contribution for a light.
 */
vec3 blinn_phong(in vec3 light_color, in vec3 light_dir, in common_fragment_t f) {
  return light_color * f.specular_sample.rgb * blinn(light_dir, f);
}
#endif

/**
 * @brief Computes ambient lighting from the sky cubemap and voxel data.
 */
vec3 ambient_light(in common_vertex_t v) {

  float occlusion = voxel_occlusion(v.voxel);
  float exposure = voxel_exposure(v.voxel);

  vec3 sky = textureLod(texture_sky, normalize(v.model_normal), 6).rgb;
  return pow(vec3(2.0) + sky, vec3(2.0)) * exposure * (1.0 - occlusion * ambient_occlusion) * ambient;
}

/**
 * @brief Computes unshadowed diffuse vertex lighting from one light.
 */
vec3 vertex_light(in common_vertex_t v, in light_t light) {

  vec3 light_dir = light.origin.xyz - v.model_position;
  float dist = length(light_dir);
  float radius = light.origin.w;
  float atten = clamp(1.0 - dist / radius, 0.0, 1.0);

  if (atten <= 0.0) {
    return vec3(0.0);
  }

  light_dir = normalize(light_dir);
  float lambert = dot(v.model_normal, light_dir);
  lambert = bool(material.surface & (SURF_MASK_BLEND | SURF_LIQUID)) ? abs(lambert) : max(0.0, lambert);
  return light_color(light) * atten * lambert;
}

/**
 * @brief Caches the vertex caustics strength.
 */
void vertex_caustics(inout common_vertex_t v) {
  v.caustics = length(voxel_caustics(v.voxel));
}

/**
 * @brief Accumulates the vertex lighting fallback for a draw.
 */
void vertex_lighting(inout common_vertex_t v) {

  v.ambient = ambient_light(v);
  v.diffuse = vec3(0.0);

  if (editor == 0) {
    ivec3 voxel_coord = voxel_xyz(v.model_position);
    ivec2 data = voxel_light_data(voxel_coord);

    for (int i = 0; i < data.y; i++) {
      int index = voxel_light_index(data.x + i);
      v.diffuse += vertex_light(v, bsp_lights[index]);
    }
  }

  for (int j = 0; j < num_dynamic_lights; j++) {
    if (dynamic_light_active(active_dynamic_lights, j)) {
      v.diffuse += vertex_light(v, dynamic_lights[j]);
    }
  }

  vertex_caustics(v);
}

#if defined(FRAGMENT_SHADER)
/**
 * @brief Applies animated caustics to the fragment diffuse lighting.
 */
void fragment_caustics(in common_vertex_t v, inout common_fragment_t f) {

  vec3 caustics_sample = voxel_caustics(v.voxel);

  float caustics_strength = length(caustics_sample);
  if (caustics_strength == 0.0) {
    return;
  }

  vec3 caustics_dir = normalize(mat3(view) * caustics_sample);
  float facing = dot(v.normal, caustics_dir);
  float backface = facing < -0.25 ? 0.25 : 1.0;
  f.caustics = caustics_strength * backface;

  if (f.caustics == 0.0) {
    return;
  }

  float noise = noise3d(v.model_position * .05 + (ticks / 1000.0) * 0.5);

  float thickness = 0.02;
  float glow = 5.0;

  noise = clamp(pow((1.0 - abs(noise)) + thickness, glow), 0.0, 1.0);

  vec3 light = f.ambient + f.diffuse;
  f.diffuse += max(vec3(0.0), light * f.caustics * noise);
}

/**
 * @brief Raymarches parallax self-shadowing along the light direction.
 */
#if defined(PARALLAX_SELF_SHADOW)
float parallax_self_shadow(in vec3 light_dir, in common_vertex_t v, in common_fragment_t f) {

  int max_steps = int(mix(12.0, 2.0, min(f.texture_lod * 0.5, 1.0)));

  float step_scale = mix(1.0, 4.0, min(f.texture_lod * 0.5, 1.0));

  vec2 texel = 1.0 / textureSize(texture_material, 0).xy;
  vec3 dir = normalize(vec3(dot(light_dir, v.tangent), dot(light_dir, v.bitangent), dot(light_dir, v.normal)));
  vec3 delta = vec3(dir.xy * texel, max(dir.z * length(texel), .01)) * step_scale;
  vec3 texcoord = vec3(f.parallax, sample_material_heightmap(f.parallax, f.texture_lod));

  float max_height = texcoord.z;
  for (int i = 0; i < max_steps && texcoord.z < 1.0 && max_height < 1.0; i++) {
    texcoord += delta;
    max_height = max(max_height, sample_material_heightmap(texcoord.xy, f.texture_lod));
  }

  float shadow = 1.0 - (max_height - texcoord.z) * material.shadow;
  return clamp(shadow, 0.0, 1.0);
}
#endif

/**
 * @brief Accumulates diffuse, specular, and shadowing from one light.
 */
void fragment_light(in common_vertex_t v, inout common_fragment_t f, in light_t light) {

  vec3 dir = light.origin.xyz - v.model_position;
  float dist = length(dir);
  float radius = light.origin.w;
  float atten = clamp(1.0 - dist / radius, 0.0, 1.0);
  if (atten <= 0.0) {
    return;
  }

  dir = normalize(view * vec4(dir, 0.0)).xyz;

  bool is_blend = bool(material.surface & SURF_MASK_BLEND);
  bool is_liquid = bool(material.surface & SURF_LIQUID);
  bool is_stage = bool(material.flags != STAGE_NONE);

  float lambert = dot(dir, f.normal_sample);
  lambert = is_blend || is_liquid || is_stage ? abs(lambert) : max(0.0, lambert);

  if (atten * lambert <= 0.0) {
    return;
  }

  vec3 color = light_color(light) * atten;

  float shadow = sample_shadow_atlas(light, v, f, atten);

#if defined(PARALLAX_SELF_SHADOW)
  if (!is_stage && material.shadow > 0.0 && f.texture_lod < 2.0) {
    shadow *= parallax_self_shadow(dir, v, f);
  }
#endif

  if (shadow <= 0.0) {
    return;
  }

  f.diffuse += color * lambert * shadow;
  f.specular += blinn_phong(color * shadow, dir, f);
}

/**
 * @brief Accumulates full fragment lighting for the active BSP and dynamic lights.
 */
void fragment_lighting(in common_vertex_t v, inout common_fragment_t f) {

  f.ambient = ambient_light(v);
  f.diffuse = vec3(0.0);
  f.specular = vec3(0.0);

  if (editor == 0) {
    ivec3 voxel_coord = voxel_xyz(v.model_position);
    ivec2 data = voxel_light_data(voxel_coord);

    for (int i = 0; i < data.y; i++) {
      int index = voxel_light_index(data.x + i);
      fragment_light(v, f, bsp_lights[index]);
    }
  }

  for (int j = 0; j < num_dynamic_lights; j++) {
    if (dynamic_light_active(active_dynamic_lights, j)) {
      fragment_light(v, f, dynamic_lights[j]);
    }
  }

  fragment_caustics(v, f);
}

/**
 * @brief Computes full fragment lighting, blending down to vertex lighting as
 * fragment.view_dist approaches lighting_distance.
 */
void fragment_lighting_lod(in common_vertex_t v, inout common_fragment_t f) {

  const float lighting_lod = clamp((f.view_dist - lighting_distance) / LIGHTING_LOD_BLEND_DIST, 0.0, 1.0);

  if (lighting_lod >= 1.0) {
    f.ambient = v.ambient;
    f.diffuse = v.diffuse;
    f.specular = vec3(0.0);
    return;
  }

  if ((material.flags & STAGE_LIGHTING_FLAT) == STAGE_LIGHTING_FLAT) {
    f.normal_sample = normalize(v.normal);
    f.specular_sample = vec4(f.diffuse_sample.rgb, pow(1.0 + material.specularity, 4.0));
  } else {
    f.normal_sample = sample_material_normal(f.parallax, mat3(v.tangent, v.bitangent, v.normal));
    f.specular_sample = sample_material_specular(f.parallax);
  }

  float angle = random_angle(v.model_position);
  f.shadow_sin_cos = vec2(sin(angle), cos(angle));

  fragment_lighting(v, f);

  f.ambient = mix(f.ambient, v.ambient, lighting_lod);
  f.diffuse = mix(f.diffuse, v.diffuse, lighting_lod);
  f.specular *= 1.0 - lighting_lod;
}
#endif
