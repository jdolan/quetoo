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
	vec3 model;
	vec3 position;
	vec3 normal;
	vec3 smooth_normal;
	vec3 tangent;
	vec3 bitangent;
	vec2 diffusemap;
	vec4 color;
	vec3 ambient;
	float caustics;
	vec4 fog;
} vertex;

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec4 out_bloom;

struct fragment_t {
	vec3 dir;
	float dist;
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	mat3 tbn;
	vec4 diffusemap;
	vec3 normalmap;
	vec4 specularmap;
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
} fragment;

uniform mat4 model;
uniform vec4 color;
uniform vec4 tint_colors[3];

/**
 * @brief
 */
vec4 sample_diffusemap() {
	return texture(texture_material, vec3(vertex.diffusemap, 0));
}

/**
 * @brief
 */
vec3 sample_normalmap() {
	vec3 normalmap = texture(texture_material, vec3(vertex.diffusemap, 1)).xyz * 2.0 - 1.0;
	vec3 roughness = vec3(vec2(material.roughness), 1.0);
	return normalize(fragment.tbn * normalize(normalmap * roughness));
}

/**
 * @brief
 */
float toksvig_gloss(in vec3 normal, in float power) {
	float len_rcp = 1.0 / saturate(length(normal));
	return 1.0 / (1.0 + power * (len_rcp - 1.0));
}

/**
 * @brief
 */
vec4 sample_specularmap() {
	vec4 specularmap;

	specularmap.rgb = texture(texture_material, vec3(vertex.diffusemap, 2)).rgb * material.hardness;

	vec3 roughness = vec3(vec2(material.roughness), 1.0);
	vec3 normalmap0 = (texture(texture_material, vec3(vertex.diffusemap, 1), 0.0).xyz * 2.0 - 1.0) * roughness;
	vec3 normalmap1 = (texture(texture_material, vec3(vertex.diffusemap, 1), 1.0).xyz * 2.0 - 1.0) * roughness;

	float power = pow(1.0 + material.specularity, 4.0);
	specularmap.w = power * min(toksvig_gloss(normalmap0, power), toksvig_gloss(normalmap1, power));

	return specularmap;
}

/**
 * @brief
 */
vec4 sample_tintmap() {
	return texture(texture_material, vec3(vertex.diffusemap, 3));
}

/**
 * @brief
 */
vec4 sample_material_stage() {
	return texture(texture_stage, vertex.diffusemap);
}

/**
 * @brief
 */
float blinn(in vec3 light_dir) {
	return pow(max(0.0, dot(normalize(light_dir + fragment.dir), fragment.normalmap)), fragment.specularmap.w);
}

/**
 * @brief
 */
vec3 blinn_phong(in vec3 diffuse, in vec3 light_dir) {
	return diffuse * fragment.specularmap.rgb * blinn(light_dir);
}

/**
 * @brief
 */
float sample_shadow_cubemap_array(in light_t light, in int index) {

	int array = index / MAX_SHADOW_CUBEMAP_LAYERS;
	int layer = index % MAX_SHADOW_CUBEMAP_LAYERS;

	vec4 shadowmap = vec4(vertex.model - light.model.xyz, layer);
	float bias = length(shadowmap.xyz) / depth_range.y;

	switch (array) {
		case 0:
			return texture(texture_shadow_cubemap_array0, shadowmap, bias);
		case 1:
			return texture(texture_shadow_cubemap_array1, shadowmap, bias);
		case 2:
			return texture(texture_shadow_cubemap_array2, shadowmap, bias);
		case 3:
			return texture(texture_shadow_cubemap_array3, shadowmap, bias);
	}

	return -1.0;
}

/**
 * @brief
 */
void light_and_shadow_dynamic(in light_t light, in int index) {

	vec3 dir = (view * vec4(light.model.xyz, 1.0)).xyz - vertex.position;

	float radius = light.model.w;
	float atten = clamp(1.0 - length(dir) / radius, 0.0, 1.0);

	vec3 diffuse = light.color.rgb * light.color.a;
	switch (int(light.maxs.w)) {
		case LIGHT_ATTEN_LINEAR:
			diffuse *= atten;
			break;
		case LIGHT_ATTEN_INVERSE_SQUARE:
			diffuse *= atten * atten;
			break;
	}

	dir = normalize(dir);

	float lambert = dot(dir, fragment.normalmap);
	if (lambert <= 0.0) {
		return;
	}

	diffuse *= lambert;

	float shadow = sample_shadow_cubemap_array(light, index);

	diffuse *= shadow;

	fragment.diffuse += diffuse;
	fragment.specular += blinn_phong(diffuse, dir);
}

/**
 * @brief
 */
void light_and_shadow_caustics() {

	if (vertex.caustics == 0.0) {
		return;
	}

	float noise = noise3d(vertex.model * .05 + (ticks / 1000.0) * 0.5);

	// make the inner edges stronger, clamp to 0-1

	float thickness = 0.02;
	float glow = 5.0;

	noise = clamp(pow((1.0 - abs(noise)) + thickness, glow), 0.0, 1.0);

	vec3 light = fragment.ambient + fragment.diffuse;
	fragment.diffuse += max(vec3(0.0), light * vertex.caustics * noise);
}

/**
 * @brief
 */
void light_and_shadow(void) {

	fragment.normalmap = sample_normalmap();
	fragment.specularmap = sample_specularmap();

	fragment.ambient = vertex.ambient * max(0.0, dot(fragment.normal, fragment.normalmap));
	fragment.specular = blinn_phong(fragment.ambient, fragment.normal);

	fragment.diffuse = vec3(0);

	for (int i = 0; i < MAX_LIGHTS; i++) {
		
		int index = active_lights[i];
		if (index == 0) {
			break;
		}

		light_t light = lights[index];

		if (box_contains(light.mins.xyz, light.maxs.xyz, vertex.model)) {
			light_and_shadow_dynamic(light, index);
		}
	}

	light_and_shadow_caustics();
}

/**
 * @brief
 */
void main(void) {

	fragment.dir = normalize(-vertex.position);
	fragment.dist = length(vertex.position);
	fragment.normal = normalize(vertex.normal);
	fragment.tangent = normalize(vertex.tangent);
	fragment.bitangent = normalize(vertex.bitangent);
	fragment.tbn = mat3(fragment.tangent, fragment.bitangent, fragment.normal);

	if ((stage.flags & STAGE_MATERIAL) == STAGE_MATERIAL) {

		fragment.diffusemap = sample_diffusemap() * vertex.color;

		if (fragment.diffusemap.a < material.alpha_test) {
			discard;
		}

		vec4 tintmap = sample_tintmap();
		fragment.diffusemap.rgb *= 1.0 - tintmap.a;
		fragment.diffusemap.rgb += (tint_colors[0] * tintmap.r).rgb * tintmap.a;
		fragment.diffusemap.rgb += (tint_colors[1] * tintmap.g).rgb * tintmap.a;
		fragment.diffusemap.rgb += (tint_colors[2] * tintmap.b).rgb * tintmap.a;

		light_and_shadow();

		out_color = fragment.diffusemap;

		out_color.rgb = max(out_color.rgb * (fragment.ambient + fragment.diffuse), 0.0);
		out_color.rgb = max(out_color.rgb + fragment.specular, 0.0);
		out_color.rgb = mix(out_color.rgb, vertex.fog.rgb, vertex.fog.a);

		out_bloom.rgb = max(out_color.rgb * material.bloom - 1.0, 0.0);
		out_bloom.a = out_color.a;

	} else {

		fragment.diffusemap = sample_material_stage() * vertex.color * color;

		out_color = fragment.diffusemap;

		if ((stage.flags & STAGE_LIGHTMAP) == STAGE_LIGHTMAP) {

			fragment.ambient = vertex.ambient * max(0.0, dot(fragment.normal, fragment.normalmap));

			light_and_shadow(); // FIXME ambient?

			out_color.rgb *= (fragment.ambient + fragment.diffuse);
		}

		if ((stage.flags & STAGE_FOG) == STAGE_FOG) {
			out_color.rgb = mix(out_color.rgb, vertex.fog.rgb, vertex.fog.a);
		}

		out_bloom.rgb = max(out_color.rgb * material.bloom - 1.0, 0.0);
		out_bloom.a = out_color.a;
	}
}
