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

in decal_data {
  flat uint time;
  flat uint lifetime;
} decal;

uniform mat4 model;
uniform int block;

layout (location = 0) out vec4 out_color;

/**
 * @brief Decal fragment shader.
 */
void main(void) {

  vec4 diffuse = texture(texture_diffusemap, vertex.diffusemap);

  // Apply vertex lighting
  out_color = diffuse * vertex.color;
  out_color.rgb *= vertex.lighting;

  if (decal.lifetime > 0u) {
    float age = float(uint(ticks) - decal.time);
    float fade = 1.0 - clamp(age / decal.lifetime, 0.0, 1.0);
    out_color.a *= fade;
  }

  // Use vertex fog (decals are always simple geometry)
  out_color.rgb = mix(out_color.rgb, vertex.fog.rgb, vertex.fog.a);
}
