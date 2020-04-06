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

#define TEXTURE_MATERIAL                 0
#define TEXTURE_LIGHTMAP                 1

#define MAX_HARDNESS 16

uniform sampler2DArray texture_material;
uniform sampler2DArray texture_lightmap;

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
	vec2 lightmap;
	vec4 color;
} vertex;

out vec4 out_color;

/**
 * @brief
 */
void main(void) {
	
	float _specular = specular * 100.0; // fudge the numbers to match the old specular model... kinda...

	vec4 diffusemap = texture(texture_material, vec3(vertex.diffusemap, 0));
	vec4 normalmap = texture(texture_material, vec3(vertex.diffusemap, 1));
	vec4 glossmap = texture(texture_material, vec3(vertex.diffusemap, 2));

	diffusemap *= vertex.color;

	if (diffusemap.a < alpha_threshold) {
		discard;
	}

	mat3 tbn = mat3(normalize(vertex.tangent), normalize(vertex.bitangent), normalize(vertex.normal));
	vec3 normal = normalize(tbn * ((normalmap.xyz * 2.0 - 1.0) * vec3(bump, bump, 1.0)));

	vec3 ambient = texture(texture_lightmap, vec3(vertex.lightmap, 0)).rgb;
	vec3 diffuse = texture(texture_lightmap, vec3(vertex.lightmap, 1)).rgb;
	vec3 radiosity = texture(texture_lightmap, vec3(vertex.lightmap, 2)).rgb;
	vec3 diffuse_dir = texture(texture_lightmap, vec3(vertex.lightmap, 3)).xyz;

	diffuse_dir = normalize(tbn * (diffuse_dir * 2.0 - 1.0));

	vec3 light_diffuse = diffuse * max(dot(diffuse_dir, normal), 0.0) + ambient + radiosity;
	vec3 light_specular = brdf_blinn(normalize(-vertex.position), diffuse_dir, normal, diffuse, glossmap.a, _specular);
	light_specular = min(light_specular * 0.2 * glossmap.xyz * hardness, MAX_HARDNESS);

	vec3 stainmap = texture_bicubic(texture_lightmap, vec3(vertex.lightmap, 4)).rgb;

	dynamic_light(vertex.position, normal, 64, light_diffuse, light_specular);
	
	out_color = diffusemap;
	out_color *= vec4(stainmap, 1.0);

	out_color.rgb = clamp(out_color.rgb * light_diffuse  * modulate, 0.0, 32.0);
	out_color.rgb = clamp(out_color.rgb + light_specular * modulate, 0.0, 32.0);
	
	// postprocessing
	
	out_color.rgb = tonemap(out_color.rgb);
	
	out_color.rgb = fog(vertex.position, out_color.rgb);
	
	out_color.rgb = color_filter(out_color.rgb);
	
	out_color.rgb = dither(out_color.rgb);

	// out_color.rgb = texture_bicubic(texture_lightmap, vec3(vertex.lightmap, 3)).xyz;
	// out_color.rgb = (out_color.rgb + 1) * 0.5;

	// out_color.rgb = (normal + 1) * 0.5;
	// out_color.rgb = vec3(gen_cavity(normalmap));
	// out_color.rgb = vec3(gen_gloss(diffusemap));
	// out_color.rgb = vec3(min(auto_glossmap(normalmap, diffusemap), toksvig));
	

	// float power = saturate(glossmap.r * hardness);
	// float rlen = 1.0 / saturate(length(normalmap.xyz * 2.0 - 1.0));
	// float gloss = 1.0 / (power * (rlen - 1.0) + 1.0);
	// out_color.rgb = vec3(length(normalmap.xyz * 2.0 - 1.0) * 0.5 + 0.5);
	
	// out_color.rgb = vec3(light_specular);
}
