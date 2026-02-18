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
  vec3 position;
  vec2 texcoord;
  vec4 color;
  flat uint time;
  flat uint lifetime;
} vertex;

uniform mat4 model;
uniform int block;

layout (location = 0) out vec4 out_color;

/**
 * @brief Samples voxel fog
 */
vec4 sample_voxel_fog() {
  
  vec4 fog = vec4(0.0);

  float samples = clamp(length(vertex.position) / BSP_VOXEL_SIZE, 1.0, fog_samples);

  for (float i = 0; i < samples; i++) {

    vec3 xyz = mix((model * vec4(vertex.position, 1.0)).xyz, view[0].xyz, i / samples);
    vec3 uvw = mix(voxel_uvw(xyz), voxels.view_coordinate.xyz, i / samples);

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
 * @brief Decal fragment shader.
 */
void main(void) {

  vec4 diffuse = texture(texture_diffusemap, vertex.texcoord);

  out_color = diffuse * vertex.color;

  if (vertex.lifetime > 0u) {
    float age = float(uint(ticks) - vertex.time);
    float fade = 1.0 - clamp(age / vertex.lifetime, 0.0, 1.0);
    out_color.a *= fade;
  }

  vec4 fog = sample_voxel_fog();
  out_color.rgb = mix(out_color.rgb, fog.rgb, fog.a);
}
