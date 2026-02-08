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
  vec2 texcoord;
  vec4 color;
  flat uint time;
  flat uint lifetime;
} vertex;

layout (location = 0) out vec4 out_color;

/**
 * @brief Decal fragment shader.
 */
void main(void) {

  vec4 diffuse = texture(texture_diffusemap, vertex.texcoord);

  out_color = diffuse * vertex.color;
  
  // Fade out over the last 20% of lifetime
  if (vertex.lifetime > 0u) {
    float age = float(uint(ticks) - vertex.time);
    float fade_start = float(vertex.lifetime) * 0.8;
    float fade_duration = float(vertex.lifetime) * 0.2;
    
    if (age > fade_start) {
      float fade = 1.0 - clamp((age - fade_start) / fade_duration, 0.0, 1.0);
      out_color.a *= fade;
    }
  }
}
