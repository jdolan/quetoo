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
uniform sampler2D texture_next_diffusemap;

uniform sampler3D texture_lightgrid_fog;

in vertex_data {
	vec3 position;
	vec2 diffusemap;
	vec2 next_diffusemap;
	vec4 color;
	float lerp;
	float softness;
	float lighting;
	vec3 ambient;
	vec3 diffuse;
	vec4 fog;
} vertex;

out vec4 out_color;

/**
 * @brief
 */
void main(void) {

	vec4 diffuse_color = mix(texture(texture_diffusemap, vertex.diffusemap), texture(texture_next_diffusemap, vertex.next_diffusemap), vertex.lerp);

	vec3 light_diffuse = vertex.ambient + vertex.diffuse;
	vec3 light_specular = vec3(0.0);

	dynamic_light(vertex.position, vec3(0.0, 0.0, -1.0), 64.0, light_diffuse, light_specular);

	out_color = vertex.color * diffuse_color;

	vec3 lighting_color = out_color.rgb;
	
	lighting_color = clamp(lighting_color * (light_diffuse * modulate), 0.0, 32.0);
	lighting_color = clamp(lighting_color + (light_specular * modulate), 0.0, 32.0);

	out_color.rgb = mix(out_color.rgb, lighting_color.rgb, vertex.lighting);

	//out_color.rgb = mix(out_color.rgb, vertex.fog.rgb, vertex.fog.a * diffuse_color.a);

	out_color.rgb = color_filter(out_color.rgb);

	out_color *= soften(vertex.softness);
}
