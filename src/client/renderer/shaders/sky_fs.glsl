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

in vec3 cubemap_coord;
in vec3 view_direction;

out vec4 out_color;

common_fragment_t fragment;

/**
 * @brief Convert normalized direction to equirectangular UV coordinates.
 */
vec2 direction_to_equirectangular(vec3 direction) {
  vec3 normalized = normalize(direction);
  
  float lon = atan(normalized.y, normalized.x);
  float lat = asin(normalized.z);

  return vec2((lon + PI) / (2.0 * PI), (lat + PI / 2.0) / PI);
}

/**
 * @brief Apply stage transforms (rotation, scroll, scale) to UV coordinates.
 */
vec2 transform_stage_uv(vec2 uv) {
  vec2 center = uv - 0.5;
  
  if ((stage.flags & STAGE_ROTATE) == STAGE_ROTATE) {
    float cos_a = cos(stage.rotate * ticks * 0.001);
    float sin_a = sin(stage.rotate * ticks * 0.001);
    center = mat2(cos_a, -sin_a, sin_a, cos_a) * center;
  }
  
  uv = center + 0.5;
  
  uv += stage.scroll * ticks * 0.001;
  
  if ((stage.flags & (STAGE_SCALE_S | STAGE_SCALE_T)) > 0) {
    center = uv - 0.5;
    center /= stage.scale;
    uv = center + 0.5;
  }
  
  return uv;
}

/**
 * @brief
 */
void main(void) {

  if ((stage.flags & STAGE_MATERIAL) == STAGE_MATERIAL) {
    fragment.view_dist = length(vertex.position);
    fragment.diffuse_sample = texture(texture_sky, normalize(cubemap_coord));

    fragment_fog(vertex, fragment);

    out_color = fragment.diffuse_sample;
    out_color.rgb = mix(out_color.rgb, fragment.fog.rgb, fragment.fog.a);

  } else {

    vec2 st = direction_to_equirectangular(view_direction);
    
    st = transform_stage_uv(st);
    
    fragment.diffuse_sample = sample_material_stage(st);

    out_color = fragment.diffuse_sample * vertex.color;

    if ((stage.flags & STAGE_FOG) == STAGE_FOG) {

      fragment_fog(vertex, fragment);

      out_color.rgb = mix(out_color.rgb, fragment.fog.rgb, fragment.fog.a * stage.fog);
    }
  }
}

