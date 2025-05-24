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

uniform mat4 model;

in vertex_data {
	vec3 model;
	vec3 position;
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	vec2 diffusemap;
	vec2 lightmap;
	vec3 lightgrid;
	vec4 color;
} vertex;

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec4 out_bloom;

struct fragment_t {
	vec3 dir;
	float dist;
	float lod;
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	mat3 tbn;
	mat3 inverse_tbn;
	vec2 parallax;
	vec4 diffusemap;
	vec3 normalmap;
	vec4 specularmap;
	vec3 ambient;
	vec3 diffuse;
	vec3 direction;
	float caustics;
	vec3 specular;
	vec4 fog;
	vec3 stains;
} fragment;

/**
 * @brief Samples the heightmap at the given texture coordinate.
 */
float sample_heightmap(vec2 texcoord) {
	return textureLod(texture_material, vec3(texcoord, 1), fragment.lod).w;
}

/**
 * @brief Sampels the displacement map at the given texture coordinate and lod.
 */
float sample_displacement(vec2 texcoord) {
	return 1.0 - sample_heightmap(texcoord);
}

/**
 * @brief Calculates the augmented texcoord for parallax occlusion mapping.
 * @see https://learnopengl.com/Advanced-Lighting/Parallax-Mapping
 */
void parallax_occlusion_mapping() {

	fragment.parallax = vertex.diffusemap;

	if (material.parallax == 0.0) {
		return;
	}

	float num_samples = 96.0 / max(1.0, fragment.lod * fragment.lod);

	vec2 texel = 1.0 / textureSize(texture_material, 0).xy;
	vec3 dir = normalize(fragment.dir * fragment.tbn);
	vec2 p = ((dir.xy * texel) / dir.z) * material.parallax * material.parallax;
	vec2 delta = p / num_samples;

	vec2 texcoord = vertex.diffusemap;
	vec2 prev_texcoord = vertex.diffusemap;

	float depth = 0.0;
	float displacement = 0.0;
	float layer = 1.0 / num_samples;

	for (displacement = sample_displacement(texcoord); depth < displacement; depth += layer) {
		prev_texcoord = texcoord;
		texcoord -= delta;
		displacement = sample_displacement(texcoord);
	}

	float a = displacement - depth;
	float b = sample_displacement(prev_texcoord) - depth + layer;

	fragment.parallax = mix(prev_texcoord, texcoord, a / (a - b));
}

/**
 * @brief Returns the shadow scalar for parallax self shadowing.
 * @param light_dir The light direction in view space.
 * @return The self-shadowing scalar.
 */
float parallax_self_shadow(in vec3 light_dir) {

	if (developer == 2) {
		
		if (material.parallax == 0.0) {
			return 1.0;
		}

		vec2 texel = 1.0 / textureSize(texture_material, 0).xy;
		vec3 dir = normalize(fragment.inverse_tbn * light_dir);
		vec3 delta = vec3(dir.xy * texel, max(dir.z * length(texel), .01));
		vec3 texcoord = vec3(fragment.parallax, sample_heightmap(fragment.parallax));

		do {
			texcoord += delta;
			float sample_height = sample_heightmap(texcoord.xy);
			if (sample_height > texcoord.z * 1.05) {
				return distance(fragment.parallax, texcoord.xy);
			}
		} while (texcoord.z < 1.0);
	}

	return 1.0;
}

/**
 * @brief
 */
vec4 sample_diffusemap() {
	return texture(texture_material, vec3(fragment.parallax, 0));
}

/**
 * @brief
 */
vec3 sample_normalmap() {
	vec3 normalmap = texture(texture_material, vec3(fragment.parallax, 1)).xyz * 2.0 - 1.0;
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
	specularmap.rgb = texture(texture_material, vec3(fragment.parallax, 2)).rgb * material.hardness;

	vec3 roughness = vec3(vec2(material.roughness), 1.0);
	vec3 normalmap0 = (texture(texture_material, vec3(fragment.parallax, 1), 0.0).xyz * 2.0 - 1.0) * roughness;
	vec3 normalmap1 = (texture(texture_material, vec3(fragment.parallax, 1), 1.0).xyz * 2.0 - 1.0) * roughness;

	float power = pow(1.0 + material.specularity, 4.0);
	specularmap.w = power * min(toksvig_gloss(normalmap0, power), toksvig_gloss(normalmap1, power));

	return specularmap;
}

/**
 * @brief
 */
vec4 sample_material_stage(in vec2 texcoord) {
	return texture(texture_stage, texcoord);
}

/**
 * @brief
 */
vec3 sample_lightmap_stains() {
	return texture(texture_lightmap_stains, vertex.lightmap).rgb * stains;
}

/**
 * @brief
 */
vec3 sample_lightgrid_ambient() {
	return texture(texture_lightgrid_ambient, vertex.lightgrid).rgb * modulate;
}

/**
 * @brief
 */
float sample_lightgrid_caustics() {
	return texture(texture_lightgrid_direction, vertex.lightgrid).a;
}

/**
 * @brief
 */
vec4 sample_lightgrid_fog() {

	vec4 fog = vec4(0.0);

	float samples = clamp(fragment.dist / BSP_LIGHTGRID_LUXEL_SIZE, 1.0, fog_samples);

	for (float i = 0; i < samples; i++) {

		vec3 xyz = mix(vertex.model, view[0].xyz, i / samples);
		vec3 uvw = mix(vertex.lightgrid, lightgrid.view_coordinate.xyz, i / samples);

		fog += texture(texture_lightgrid_fog, uvw) * vec4(vec3(1.0), fog_density) * min(1.0, samples - i);
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

	return 1.0;
}

/**
 * @brief
 */
void light_and_shadow_dynamic(in light_t light, in int index) {

	vec3 dir = (view * vec4(light.model.xyz, 1.0)).xyz - vertex.position;

	float radius = light.model.w;
	float atten = clamp(1.0 - length(dir) / radius, 0.0, 1.0);

	vec3 diffuse = light.color.rgb * light.color.a * modulate;
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
	if (shadow == 1.0) {
		shadow = parallax_self_shadow(dir);
	}

	diffuse *= shadow;

	fragment.diffuse += diffuse;
	fragment.specular += blinn_phong(diffuse, dir);
}

/**
 * @brief
 */
void light_and_shadow_caustics() {

	fragment.caustics = sample_lightgrid_caustics();

	if (fragment.caustics == 0.0) {
		return;
	}

	float noise = noise3d(vertex.model * .05 + (ticks / 1000.0) * 0.5);

	// make the inner edges stronger, clamp to 0-1

	float thickness = 0.02;
	float glow = 5.0;

	noise = clamp(pow((1.0 - abs(noise)) + thickness, glow), 0.0, 1.0);

	vec3 light = fragment.ambient + fragment.diffuse;
	fragment.diffuse += max(vec3(0.0), light * fragment.caustics * noise);
}

/**
 * @brief
 */
void light_and_shadow(void) {

	fragment.normalmap = sample_normalmap();
	fragment.specularmap = sample_specularmap();

	fragment.ambient = sample_lightgrid_ambient() * max(0.0, dot(fragment.normal, fragment.normalmap));
	fragment.specular = blinn_phong(fragment.ambient, fragment.normalmap);

	fragment.diffuse = vec3(0);

	for (int index = 0; index < num_lights; index++) {

		uint bits = active_lights[index / 32];
		uint bit = index % 32;

		if ((bits & (1u << bit)) == 0) {
			continue;
		}

		light_and_shadow_dynamic(lights[index], index);
	}

	light_and_shadow_caustics();
}

/**
 * @brief
 */
void main(void) {

	fragment.dir = normalize(-vertex.position);
	fragment.dist = length(vertex.position);
	fragment.lod = textureQueryLod(texture_material, vertex.diffusemap).x;
	fragment.normal = normalize(vertex.normal);
	fragment.tangent = normalize(vertex.tangent);
	fragment.bitangent = normalize(vertex.bitangent);
	fragment.tbn = mat3(fragment.tangent, fragment.bitangent, fragment.normal);
	fragment.inverse_tbn = inverse(fragment.tbn);
	fragment.parallax = vertex.diffusemap;

	parallax_occlusion_mapping();

	if ((stage.flags & STAGE_MATERIAL) == STAGE_MATERIAL) {

		fragment.diffusemap = sample_diffusemap() * vertex.color;

		if (fragment.diffusemap.a < material.alpha_test) {
			discard;
		}

		light_and_shadow();

		fragment.stains = sample_lightmap_stains();
		fragment.fog = sample_lightgrid_fog();

		out_color = fragment.diffusemap;

		out_color.rgb *= (fragment.ambient + fragment.diffuse);
		out_color.rgb += fragment.specular;
		out_color.rgb *= fragment.stains;
		out_color.rgb = mix(out_color.rgb, fragment.fog.rgb, fragment.fog.a);

		out_bloom.rgb = max(out_color.rgb * material.bloom - 1.0, 0.0);
		out_bloom.a = out_color.a;

	} else {

		vec2 st = fragment.parallax;

		if ((stage.flags & STAGE_WARP) == STAGE_WARP) {
			st += texture(texture_warp, st + vec2(ticks * stage.warp.x * 0.000125)).xy * stage.warp.y;
		}

		fragment.diffusemap = sample_material_stage(st) * vertex.color;

		out_color = fragment.diffusemap;

		if ((stage.flags & STAGE_LIGHTMAP) == STAGE_LIGHTMAP) {

			light_and_shadow();

			out_color.rgb *= (fragment.ambient + fragment.diffuse);
			out_color.rgb *= fragment.stains;
		}

		if ((stage.flags & STAGE_FOG) == STAGE_FOG) {
			fragment.fog = sample_lightgrid_fog();
			out_color.rgb = mix(out_color.rgb, fragment.fog.rgb, fragment.fog.a);
		}

		out_bloom.rgb = max(out_color.rgb * material.bloom - 1.0, 0.0);
		out_bloom.a = out_color.a;
	}
}

