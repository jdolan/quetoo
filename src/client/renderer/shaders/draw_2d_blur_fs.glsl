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

uniform sampler2D texture_diffusemap;

uniform vec2 resolution;
uniform vec2 direction;

in vertex_data {
	vec2 diffusemap;
} vertex;

out vec4 out_color;

vec4 blur5(sampler2D image, vec2 uv, vec2 resolution, vec2 direction)
{
	vec4 color = vec4(0.0);
	vec2 off1 = vec2(1.3333333333333333) * direction;
	color += texture(image, uv) * 0.29411764705882354;
	color += texture(image, uv + (off1 / resolution)) * 0.35294117647058826;
	color += texture(image, uv - (off1 / resolution)) * 0.35294117647058826;
	return color; 
}

vec4 blur13(sampler2D image, vec2 uv, vec2 resolution, vec2 direction)
{
  vec4 color = vec4(0.0);
  vec2 off1 = vec2(1.411764705882353) * direction;
  vec2 off2 = vec2(3.2941176470588234) * direction;
  vec2 off3 = vec2(5.176470588235294) * direction;
  color += texture(image, uv) * 0.1964825501511404;
  color += texture(image, uv + (off1 / resolution)) * 0.2969069646728344;
  color += texture(image, uv - (off1 / resolution)) * 0.2969069646728344;
  color += texture(image, uv + (off2 / resolution)) * 0.09447039785044732;
  color += texture(image, uv - (off2 / resolution)) * 0.09447039785044732;
  color += texture(image, uv + (off3 / resolution)) * 0.010381362401148057;
  color += texture(image, uv - (off3 / resolution)) * 0.010381362401148057;
  return color;
}

/**
 * @brief
 */
void main(void) {

	out_color = blur13(texture_diffusemap, vertex.diffusemap, resolution,
					   direction * (1.0 + calc_depth(texture(texture_depth_stencil_attachment, gl_FragCoord.xy * inv_viewport_size).r)) * 2.f);
}
