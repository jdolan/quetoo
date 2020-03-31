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
uniform sampler2D next_texture_diffusemap;

in vertex_data {
	vec3 position;
	vec2 diffusemap;
	vec4 color;
	vec2 next_diffusemap;
	float next_lerp;
} vertex;

out vec4 out_color;

/**
 * @brief
 */
void main(void) {

	vec4 diffuse_color = mix(texture(texture_diffusemap, vertex.diffusemap), texture(next_texture_diffusemap, vertex.next_diffusemap), vertex.next_lerp);

	out_color = vertex.color * diffuse_color;

	float fogginess = fog_factor(vertex.position);

	out_color = out_color * (1.f - fogginess);
	
	out_color.rgb = color_filter(out_color.rgb);

	out_color.a *= soften_sprite();
}
