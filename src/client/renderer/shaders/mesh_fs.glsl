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

uniform mat4 view;

uniform sampler2DArray texture_material;

uniform sampler3D texture_lightgrid_ambient;
uniform sampler3D texture_lightgrid_diffuse;
uniform sampler3D texture_lightgrid_direction;

uniform vec4 color;
uniform float alpha_threshold;

uniform float modulate;

uniform float bump;
uniform float parallax;
uniform float hardness;
uniform float specular;

uniform vec4 caustics;

in vertex_data {
	vec3 position;
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	vec2 diffusemap;
	vec3 lightgrid;
} vertex;

out vec4 out_color;

/**
 * @brief
 */
void main(void) {

	vec4 diffusemap = texture(texture_material, vec3(vertex.diffusemap, 0));
	vec4 normalmap = texture(texture_material, vec3(vertex.diffusemap, 1));
	vec4 glossmap = texture(texture_material, vec3(vertex.diffusemap, 2));

	diffusemap *= color;

	if (diffusemap.a < alpha_threshold) {
		discard;
	}

	mat3 tbn = mat3(normalize(vertex.tangent), normalize(vertex.bitangent), normalize(vertex.normal));
	vec3 normal = normalize(tbn * ((normalmap.xyz * 2.0 - 1.0) * vec3(bump, bump, 1.0)));

	vec3 ambient = texture(texture_lightgrid_ambient, vertex.lightgrid).rgb;
	vec3 diffuse = texture(texture_lightgrid_diffuse, vertex.lightgrid).rgb;
	vec3 direction = texture(texture_lightgrid_direction, vertex.lightgrid).xyz;
	
	direction = normalize((view * vec4(direction * 2.0 - 1.0, 1.0)).xyz);

	vec3 lightgrid = ambient + diffuse * max(0.0, dot(normal, direction));

	out_color = diffusemap;

	vec3 light_diffuse = lightgrid;
	vec3 light_specular = vec3(0.0);
	
	dynamic_light(vertex.position, normal, 64, light_diffuse, light_specular);

	out_color.rgb = clamp(out_color.rgb * light_diffuse  * modulate, 0.0, 32.0);
	out_color.rgb = clamp(out_color.rgb + light_specular * modulate, 0.0, 32.0);

	// postprocessing
	
	out_color.rgb = tonemap(out_color.rgb);

	out_color.rgb = fog(vertex.position, out_color.rgb);

	out_color.rgb = color_filter(out_color.rgb);

	out_color.rgb = dither(out_color.rgb);
}

