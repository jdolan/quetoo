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

#define TEXTURE_DIFFUSEMAP               0
#define TEXTURE_NORMALMAP                1
#define TEXTURE_GLOSSMAP                 2
#define TEXTURE_LIGHTMAP                 3

#define TEXTURE_MASK_DIFFUSEMAP         (1 << TEXTURE_DIFFUSEMAP)
#define TEXTURE_MASK_NORMALMAP          (1 << TEXTURE_NORMALMAP)
#define TEXTURE_MASK_GLOSSMAP           (1 << TEXTURE_GLOSSMAP)
#define TEXTURE_MASK_LIGHTMAP           (1 << TEXTURE_LIGHTMAP)
#define TEXTURE_MASK_ALL                0xff

#define MAX_HARDNESS 16

uniform int textures;

uniform sampler2D texture_diffusemap;
uniform sampler2D texture_normalmap;
uniform sampler2D texture_glossmap;
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
 * @brief Highpasses the heightmap to approximate ambient occlusion.
 */
float gen_cavity(vec4 normalmap) {
	float height_a = normalmap.a;
	float height_b = texture(texture_normalmap, vertex.diffusemap, 4).a;
	float ao = (height_a - height_b) * 0.5 + 0.5;
	return ao * smoothstep(0.8, 1.0, normalmap.z);
}

/**
* @brief For materials without a gloss texture.
*/
float gen_gloss(vec4 diffusemap) {
	float gloss = grayscale(diffusemap.rgb) * 0.875 + 0.125;
	return saturate(pow(gloss * 3.0, 4.0));
}

/**
* @brief For materials without a gloss texture.
*/
float auto_glossmap(vec4 normalmap, vec4 diffusemap) {
	float gloss = gen_gloss(diffusemap) * 0.333 + 0.333;
	float cavity = gen_cavity(normalmap);
	return saturate((gloss + cavity) - 0.333);
}

/**
 * @brief
 */
void main(void) {
	
	float _specular = specular * 100.0; // fudge the numbers to match the old specular model... kinda...
	
	mat3 tbn = mat3(normalize(vertex.tangent), normalize(vertex.bitangent), normalize(vertex.normal));
	
	vec3 light_diffuse = vec3(0.0);
	vec3 light_specular = vec3(0.0);

	vec4 diffusemap;
	if ((textures & TEXTURE_MASK_DIFFUSEMAP) == TEXTURE_MASK_DIFFUSEMAP) {
		diffusemap = texture(texture_diffusemap, vertex.diffusemap) * vertex.color;

		if (diffusemap.a < alpha_threshold) {
			discard;
		}
	} else {
		diffusemap = vertex.color;
	}

	vec4 normalmap;
	float toksvig;
	if ((textures & TEXTURE_MASK_NORMALMAP) == TEXTURE_MASK_NORMALMAP) {
		
		vec4 normalmap_raw_0, normalmap_raw_1;
		normalmap_raw_0 = texture(texture_normalmap, vertex.diffusemap);
		normalmap_raw_0.xyz = normalmap_raw_0.xyz * 2.0 - 1.0;
		normalmap_raw_1 = texture(texture_normalmap, vertex.diffusemap, 1);
		normalmap_raw_1.xyz = normalmap_raw_1.xyz * 2.0 - 1.0;
		
		normalmap = normalize(normalmap_raw_0) * vec4(bump, bump, 1.0, 1.0);
		normalmap.xyz = normalize(normalmap.xyz);
		
		// take the minimum of 2 mip samples to prevent shimmering / crawlies on detailed textures :|
		// sadly this looks like arse on some materials, I think using precomputed textures would fix it
		float toksvig_0 = 1.0 / (1.0 + _specular * ((1.0 / saturate(length(normalmap_raw_0.xyz))) - 1.0));
		float toksvig_1 = 1.0 / (1.0 + _specular * ((1.0 / saturate(length(normalmap_raw_1.xyz))) - 1.0));
		toksvig = min(toksvig_0, toksvig_1);
		
	} else {
		normalmap = vec4(0.0, 0.0, 1.0, 0.5);
		toksvig = 1.0;
	}

	vec3 normal = normalize(tbn * normalmap.xyz);

	float glossmap;
	if ((textures & TEXTURE_MASK_GLOSSMAP) == TEXTURE_MASK_GLOSSMAP) {
		glossmap = texture(texture_glossmap, vertex.diffusemap).r;
	} else {
		glossmap = auto_glossmap(normalmap, diffusemap);
	}

	vec3 stainmap;
	if ((textures & TEXTURE_MASK_LIGHTMAP) == TEXTURE_MASK_LIGHTMAP) {
		vec3 ambient = texture(texture_lightmap, vec3(vertex.lightmap, 0)).rgb;
		vec3 diffuse = texture_bicubic(texture_lightmap, vec3(vertex.lightmap, 1)).rgb;
		vec3 radiosity = texture(texture_lightmap, vec3(vertex.lightmap, 2)).rgb;
		
		vec3 diffuse_dir = texture_bicubic(texture_lightmap, vec3(vertex.lightmap, 3)).xyz;
		diffuse_dir = normalize(diffuse_dir * 2.0 - 1.0);
		diffuse_dir = normalize(tbn * diffuse_dir);

		light_diffuse = diffuse * max(dot(diffuse_dir, normal), 0.0) + ambient + radiosity;
		light_specular = brdf_blinn(normalize(-vertex.position), diffuse_dir, normal, diffuse, glossmap * toksvig, _specular);
		light_specular = min(light_specular * 0.2 * glossmap * hardness, MAX_HARDNESS);

		stainmap = texture_bicubic(texture_lightmap, vec3(vertex.lightmap, 4)).rgb;
	} else {
		light_diffuse = vec3(1.0);
		stainmap = vec3(1.0);
	}

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
