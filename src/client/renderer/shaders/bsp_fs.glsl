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

uniform sampler2DArray texture_material;
uniform sampler2DArray texture_lightmap;
uniform sampler2D texture_stage;
uniform sampler2D texture_warp;
uniform sampler3D texture_lightgrid_ambient;
uniform sampler3D texture_lightgrid_diffuse;
uniform sampler3D texture_lightgrid_direction;
uniform sampler3D texture_lightgrid_caustics;
uniform sampler3D texture_lightgrid_fog;
uniform samplerCubeArrayShadow texture_shadowmap;

uniform mat4 model;

uniform int entity;
uniform int bicubic;

uniform material_t material;
uniform stage_t stage;

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
	flat ivec4 lights;
} vertex;

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec4 out_bloom;

struct fragment_t {
	vec3 view_dir;
	vec4 diffusemap;
	vec3 normalmap;
	vec4 specularmap;
	vec3 lightmap;
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
	vec3 caustics;
} fragment;

/**
 * @brief Samples the lightmap and stainmap with either bilinear or bicubic sampling.
 */
vec4 sample_lightmap(int index) {
	if (bicubic > 0) {
		return texture_bicubic(texture_lightmap, vec3(vertex.lightmap, index));
	} else {
		return texture(texture_lightmap, vec3(vertex.lightmap, index));
	}
}

/**
 * @brief
 */
vec3 blinn_phong(in vec3 diffuse, in vec3 light_dir) {
	return diffuse * fragment.specularmap.rgb * blinn(fragment.normalmap, light_dir, fragment.view_dir, fragment.specularmap.w);
}

/**
 * @brief
 */
void dynamic_light(void) {

	for (int i = 0; i < num_lights; i++) {

		vec3 diffuse = lights[i].color.rgb;

		float radius = lights[i].origin.w;
		if (radius <= 0.0) {
			continue;
		}

		diffuse *= radius;

		float intensity = lights[i].color.w;
		if (intensity <= 0.0) {
			continue;
		}

		diffuse *= intensity;

		vec3 light_pos = (view * vec4(lights[i].origin.xyz, 1.0)).xyz;
		float atten = 1.0 - distance(light_pos, vertex.position) / radius;
		if (atten <= 0.0) {
			continue;
		}

		diffuse *= atten * atten;

		vec3 light_dir = normalize(light_pos - vertex.position);
		float lambert = dot(light_dir, fragment.normalmap);
		if (lambert <= 0.0) {
			continue;
		}

		diffuse *= lambert;

		vec3 shadow_dir = vertex.model - lights[i].origin.xyz;
		float shadow = texture(texture_shadowmap, vec4(shadow_dir, i), length(shadow_dir) / depth_range.y);
		if (shadow <= 0.0) {
			continue;
		}

		diffuse *= shadow;

		fragment.diffuse += diffuse;
		fragment.specular += blinn_phong(diffuse, light_dir);
	}
}

/**
 * @brief
 */
void main(void) {

	fragment.view_dir = normalize(-vertex.position);

	if ((stage.flags & STAGE_MATERIAL) == STAGE_MATERIAL) {

		fragment.diffusemap = texture(texture_material, vec3(vertex.diffusemap, 0));
		fragment.diffusemap *= vertex.color;

		if (fragment.diffusemap.a < material.alpha_test) {
			discard;
		}

		mat3 tbn = mat3(normalize(vertex.tangent), normalize(vertex.bitangent), normalize(vertex.normal));

		fragment.normalmap = texture(texture_material, vec3(vertex.diffusemap, 1)).xyz;
		fragment.normalmap = normalize(tbn * normalize((fragment.normalmap * 2.0 - 1.0) * vec3(vec2(material.roughness), 1.0)));

		fragment.specularmap.xyz = texture(texture_material, vec3(vertex.diffusemap, 2)).rgb * material.hardness;
		fragment.specularmap.w = toksvig(texture_material, vec3(vertex.diffusemap, 1), material.roughness, material.specularity);

		fragment.ambient = vec3(0.0), fragment.diffuse = vec3(0.0), fragment.caustics = vec3(0.0), fragment.specular = vec3(0.0);
		if (entity == 0) {
			fragment.ambient = sample_lightmap(0).rgb;
			fragment.ambient *= modulate * max(0.0, dot(vertex.normal, fragment.normalmap));

			vec3 diffuse0 = sample_lightmap(1).rgb;
			vec3 direction0 = normalize(tbn * normalize(sample_lightmap(2).xyz * 2.0 - 1.0));
			diffuse0 *= modulate * max(0.0, dot(direction0, fragment.normalmap));

			vec3 diffuse1 = sample_lightmap(3).rgb;
			vec3 direction1 = normalize(tbn * normalize(sample_lightmap(4).xyz * 2.0 - 1.0));
			diffuse1 *= modulate * max(0.0, dot(direction1, fragment.normalmap));

			fragment.diffuse += diffuse0 + diffuse1;

			fragment.specular += blinn_phong(diffuse0, direction0);
			fragment.specular += blinn_phong(diffuse1, direction1);
			fragment.specular += blinn_phong(fragment.ambient, vertex.normal);

			fragment.caustics = sample_lightmap(5).rgb;
		} else {
			fragment.ambient = texture(texture_lightgrid_ambient, vertex.lightgrid).rgb;
			fragment.ambient *= modulate * max(0.0, dot(vertex.normal, fragment.normalmap));

			fragment.diffuse = texture(texture_lightgrid_diffuse, vertex.lightgrid).rgb;
			vec3 direction = texture(texture_lightgrid_direction, vertex.lightgrid).xyz;
			direction = normalize((view * model * vec4(normalize(direction * 2.0 - 1.0), 0.0)).xyz);
			fragment.diffuse *= modulate * max(0.0, dot(direction, fragment.normalmap));

			fragment.specular += blinn_phong(fragment.diffuse, direction);
			fragment.specular += blinn_phong(fragment.ambient, vertex.normal);

			fragment.caustics = texture(texture_lightgrid_caustics, vertex.lightgrid).rgb;
		}

		caustic_light(vertex.model, fragment.caustics, fragment.ambient, fragment.diffuse);

		dynamic_light();

		out_color = fragment.diffusemap;

		vec3 stainmap = sample_lightmap(6).rgb;

		out_color.rgb = clamp(out_color.rgb * (fragment.ambient + fragment.diffuse) * stainmap, 0.0, 1.0);
		out_color.rgb = clamp(out_color.rgb + fragment.specular * stainmap, 0.0, 1.0);

		out_bloom.rgb = clamp(out_color.rgb * material.bloom - 1.0, 0.0, 1.0);
		out_bloom.a = out_color.a;

		lightgrid_fog(out_color, texture_lightgrid_fog, vertex.position, vertex.lightgrid);
		global_fog(out_color, vertex.position);

		if (lightmaps == 1) {
			out_color.rgb = modulate * (sample_lightmap(0).rgb + sample_lightmap(1).rgb + sample_lightmap(3).rgb);
		} else if (lightmaps == 2) {
			out_color.rgb = normalize(sample_lightmap(2).xyz + sample_lightmap(4).xyz);
		} else if (lightmaps == 3) {
			out_color.rgb = fragment.ambient + fragment.diffuse;
		} else {
			out_color = postprocess(out_color);
		}

	} else {

		vec2 st = vertex.diffusemap;

		if ((stage.flags & STAGE_WARP) == STAGE_WARP) {
			st += texture(texture_warp, st + vec2(ticks * stage.warp.x * 0.000125)).xy * stage.warp.y;
		}

		vec4 effect = texture(texture_stage, st);

		effect *= vertex.color;

		if ((stage.flags & STAGE_LIGHTMAP) == STAGE_LIGHTMAP) {
			vec3 ambient = sample_lightmap(0).rgb;
			vec3 diffuse = sample_lightmap(1).rgb;

			effect.rgb *= (ambient + diffuse) * modulate;
		}

		out_bloom.rgb = clamp(effect.rgb * material.bloom - 1.0, 0.0, 1.0);
		out_bloom.a = effect.a;

		if ((stage.flags & STAGE_FOG) == STAGE_FOG) {
			lightgrid_fog(effect, texture_lightgrid_fog, vertex.position, vertex.lightgrid);
			global_fog(effect, vertex.position);
		}

		out_color = effect;

		out_color = postprocess(out_color);
	}

	// debugging

	#if 0
	// draw lightgrid texel borders
	vec4 raster = lightgrid_raster(vertex.lightgrid.xyz, length(vertex.position));
	out_color.rgb = mix(out_color.rgb, raster.rgb, raster.a * 0.5);
	#endif

	#if 0
	// draw vertex tangents
	out_color.rgb = (vertex.tangent.xyz + 1) * 0.5;
	#endif
}
