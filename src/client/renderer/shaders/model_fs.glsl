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

#define MODEL_INVALID    0
#define MODEL_BSP        1
#define MODEL_BSP_INLINE 2
#define MODEL_MESH       3

uniform sampler2DArray texture_material;
uniform sampler2D texture_stage;
uniform sampler2D texture_warp;
uniform sampler2D texture_lightmap_ambient;
uniform sampler2D texture_lightmap_diffuse;
uniform sampler2D texture_lightmap_direction;
uniform sampler2D texture_lightmap_caustics;
uniform sampler2D texture_lightmap_stains;
uniform sampler3D texture_lightgrid_ambient;
uniform sampler3D texture_lightgrid_diffuse;
uniform sampler3D texture_lightgrid_direction;
uniform sampler3D texture_lightgrid_caustics;
uniform sampler3D texture_lightgrid_fog;
uniform sampler2DArrayShadow texture_shadowmap;
uniform samplerCubeArrayShadow texture_shadowmap_cube;

uniform int model_type;
uniform mat4 model;

uniform material_t material;
uniform stage_t stage;

in geometry_data {
	vec3 model;
	vec3 position;
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	vec2 diffusemap;
	vec2 lightmap;
	vec3 lightgrid;
	vec4 color;

	flat int active_lights[MAX_LIGHT_UNIFORMS_ACTIVE];
	flat int num_active_lights;
} vertex;

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec4 out_bloom;

struct fragment_t {
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	mat3 tbn;
	vec3 dir;
	vec4 diffusemap;
	vec3 normalmap;
	vec4 specularmap;
	vec3 ambient;
	vec3 diffuse;
	vec3 direction;
	vec3 specular;
	vec3 caustics;
	vec4 fog;
} fragment;

/**
 * @brief
 */
vec4 sample_diffusemap() {
	return pow(texture(texture_material, vec3(vertex.diffusemap, 0)), vec4(vec3(gamma), 1.0));
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
 * @brief Toksvig normalmap antialiasing.
 */
float toksvig(in sampler2DArray sampler, in vec3 texcoord, in float roughness, in float specularity) {

	float power = pow(1.0 + specularity, 4.0);

	vec3 normalmap0 = (texture(sampler, texcoord, 0).xyz * 2.0 - 1.0) * vec3(vec2(roughness), 1.0);
	vec3 normalmap1 = (texture(sampler, texcoord, 1).xyz * 2.0 - 1.0) * vec3(vec2(roughness), 1.0);

	return power * min(toksvig_gloss(normalmap0, power), toksvig_gloss(normalmap1, power));
}

/**
 * @brief
 */
vec4 sample_specularmap() {
	vec4 specularmap;
	specularmap.rgb = texture(texture_material, vec3(vertex.diffusemap, 2)).rgb * material.hardness;
	specularmap.w = toksvig(texture_material, vec3(vertex.diffusemap, 1), material.roughness, material.specularity);
	return specularmap;
}

/**
 * @brief
 */
vec3 sample_lightmap_ambient() {
	return texture(texture_lightmap_ambient, vertex.lightmap).rgb * modulate;
}

/**
 * @brief
 */
vec3 sample_lightmap_diffuse() {
	return texture(texture_lightmap_diffuse, vertex.lightmap).rgb * modulate;
}

/**
* @brief
*/
vec3 sample_lightmap_direction() {
	vec3 direction = texture(texture_lightmap_direction, vertex.lightmap).xyz * 2.0 - 1.0;
	return normalize(fragment.tbn * normalize(direction));
}

/**
 * @brief
 */
float blinn(in vec3 normal, in vec3 light_dir, in vec3 view_dir, in float specularity) {
	return pow(max(0.0, dot(normalize(light_dir + view_dir), normal)), specularity);
}

/**
 * @brief
 */
vec3 blinn_phong(in vec3 diffuse, in vec3 light_dir) {
	return diffuse * fragment.specularmap.rgb * blinn(fragment.normalmap, light_dir, fragment.dir, fragment.specularmap.w);
}

/**
 * @brief
 */
void caustic_light() {

	float noise = noise3d(vertex.model * .05 + (ticks / 1000.0) * 0.5);

	// make the inner edges stronger, clamp to 0-1

	float thickness = 0.02;
	float glow = 5.0;

	noise = clamp(pow((1.0 - abs(noise)) + thickness, glow), 0.0, 1.0);

	vec3 light = fragment.ambient + fragment.diffuse;
	fragment.diffuse += max(vec3(0.0), light * length(fragment.caustics) * noise);
}

/**
 * @brief
 */
vec3 sample_lightmap_caustics() {
	return texture(texture_lightmap_caustics, vertex.lightmap).rgb * caustics;
}

/**
 * @brief FIXME: Make these alpha blended
 */
vec3 sample_lightmap_stains() {
	return texture(texture_lightmap_stains, vertex.lightmap).rgb;
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
vec3 sample_lightgrid_diffuse() {
	return texture(texture_lightgrid_diffuse, vertex.lightgrid).rgb * modulate;
}

/**
 * @brief
 */
vec3 sample_lightgrid_direction() {
	vec3 direction = texture(texture_lightgrid_direction, vertex.lightgrid).xyz;
	return normalize((view * model * vec4(normalize(direction), 0.0)).xyz);
}

/**
 * @brief
 */
vec3 sample_lightgrid_caustics() {
	return texture(texture_lightgrid_caustics, vertex.lightgrid).rgb;
}

/**
 * @brief
 */
vec4 sample_lightgrid_fog() {

	vec4 fog = vec4(0.0);

	if (fog_density > 0.0) {

		int samples = int(clamp(length(vertex.position) / BSP_LIGHTGRID_LUXEL_SIZE, 1, fog_samples));

		for (int i = 0; i < samples; i++) {
			vec3 xyz = mix(vertex.model, view[0].xyz, float(i) / float(samples));
			vec3 uvw = mix(vertex.lightgrid, lightgrid.view_coordinate.xyz, float(i) / float(samples));
			//uvw += noise3d(vertex.model) * 2.0 * lightgrid.luxel_size.xyz;

			vec3 noise = vec3(0);//noise3d(sin(xyz * .05 + ticks * .00125)) * lightgrid.luxel_size.xyz;

			fog += texture(texture_lightgrid_fog, uvw + noise) * vec4(vec3(1.0), fog_density);
			if (fog.a >= 1.0) {
				break;
			}
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
float sample_shadowmap(in light_t light, in int index) {

	if (shadows == 0) {
		return 1.0;
	}

	vec4 position = vec4(light.model.xyz - vertex.model, 1.0);
	vec4 projected = light_projection * position;
	vec2 shadowmap = (projected.xy / projected.w) * 0.5 + 0.5;

	return texture(texture_shadowmap, vec4(shadowmap, index, length(position.xyz) / depth_range.y));
}

/**
 * @brief
 */
float sample_shadowmap_cube(in light_t light, in int index) {

	if (shadows == 0) {
		return 1.0;
	}

	vec4 shadowmap = vec4(vertex.model - light.model.xyz, index);
	return texture(texture_shadowmap_cube, shadowmap, length(shadowmap.xyz) / depth_range.y);
}

/**
 * @brief
 */
void light_and_shadow_ambient(in light_t light, in int index) {

	float radius = light.model.w;
	if (radius <= 0.0) {
		return;
	}

	float size = light.mins.w;

	vec3 light_pos = light.position.xyz;
	float atten = 1.0 - distance(light_pos, vertex.position) / (radius + size);
	if (atten <= 0.0) {
		return;
	}

	float lambert = dot(-light.normal.xyz, fragment.normalmap);
	if (lambert <= 0.0) {
		return;
	}

	float shadow = sample_shadowmap(light, index);
	float shadow_atten = (1.0 - shadow) * sqrt(atten) * lambert;

	fragment.ambient -= fragment.ambient * shadow_atten;
	fragment.specular -= fragment.specular * shadow_atten;
}

/**
 * @brief
 */
void light_and_shadow_sun(in light_t light, in int index) {

	// TODO
}

/**
 * @brief
 */
void light_and_shadow_point(in light_t light, in int index) {

	float radius = light.model.w;
	if (radius <= 0.0) {
		return;
	}

	float size = light.mins.w;

	vec3 light_pos = light.position.xyz;
	float atten = 1.0 - distance(light_pos, vertex.position) / (radius + size);
	if (atten <= 0.0) {
		return;
	}

	vec3 light_dir = normalize(light_pos - vertex.position);
	float lambert = dot(light_dir, fragment.normalmap);
	if (lambert <= 0.0) {
		return;
	}

	float shadow = sample_shadowmap_cube(light, index);
	float shadow_atten = (1.0 - shadow) * lambert * atten /** atten*/;

	fragment.diffuse -= fragment.diffuse * shadow_atten;
	fragment.specular -= fragment.specular * shadow_atten;
}

/**
 * @brief
 */
void light_and_shadow_spot(in light_t light, in int index) {

	float radius = light.model.w;
	if (radius <= 0.0) {
		return;
	}

	float size = light.mins.w;

	vec3 light_pos = light.position.xyz;
	float atten = 1.0 - distance(light_pos, vertex.position) / (radius + size);
	if (atten <= 0.0) {
		return;
	}

	vec3 light_dir = normalize(light_pos - vertex.position);
	float lambert = dot(light_dir, fragment.normalmap);
	if (lambert <= 0.0) {
		return;
	}

	float shadow = sample_shadowmap_cube(light, index);
	float shadow_atten = (1.0 - shadow) * lambert * atten /** atten*/;

	fragment.diffuse -= fragment.diffuse * shadow_atten;
	fragment.specular -= fragment.specular * shadow_atten;
}

/**
 * @brief
 */
void light_and_shadow_face(in light_t light, in int index) {

	float radius = light.model.w;
	if (radius <= 0.0) {
		return;
	}

	float size = light.mins.w;

	vec3 light_pos = light.position.xyz;
	float atten = 1.0 - distance(light_pos, vertex.position) / (radius + size);
	if (atten <= 0.0) {
		return;
	}

	vec3 light_dir = normalize(light_pos - vertex.position);
	float lambert = dot(light_dir, fragment.normalmap);
	if (lambert <= 0.0) {
		return;
	}

	float shadow = sample_shadowmap_cube(light, index);
	float shadow_atten = (1.0 - shadow) * lambert * atten /** atten*/;

	fragment.diffuse -= fragment.diffuse * shadow_atten;
	fragment.specular -= fragment.specular * shadow_atten;
}

/**
 * @brief
 */
void light_and_shadow_dynamic(in light_t light, in int index) {

	vec3 diffuse = light.color.rgb;
	if (length(diffuse) <= 0.0) {
		return;
	}

	float radius = light.model.w;
	if (radius <= 0.0) {
		return;
	}

	float size = light.mins.w;

	diffuse *= radius;

	float intensity = light.color.w;
	if (intensity <= 0.0) {
		return;
	}

	diffuse *= intensity;

	vec3 light_pos = light.position.xyz;

	float atten = 1.0 - distance(light_pos, vertex.position) / (radius + size);
	if (atten <= 0.0) {
		return;
	}

	diffuse *= atten * atten;

	vec3 light_dir = normalize(light_pos - vertex.position);
	float lambert = dot(light_dir, fragment.normalmap);
	if (lambert <= 0.0) {
		return;
	}

	diffuse *= lambert;

	float shadow = sample_shadowmap_cube(light, index);
	float shadow_atten = (1.0 - shadow) * lambert * atten * atten;

	diffuse *= shadow;

	fragment.diffuse += diffuse;
	fragment.specular += blinn_phong(diffuse, light_dir);
}

/**
 * @brief
 */
void light_and_shadow(void) {

	caustic_light();

	for (int i = 0; i < vertex.num_active_lights; i++) {

		int index = vertex.active_lights[i];

		light_t light = lights[index];

		int type = int(light.position.w);
		switch (type) {
			case LIGHT_AMBIENT:
				light_and_shadow_ambient(light, index);
				break;
			case LIGHT_SUN:
				light_and_shadow_sun(light, index);
				break;
			case LIGHT_POINT:
				light_and_shadow_point(light, index);
				break;
			case LIGHT_SPOT:
				light_and_shadow_spot(light, index);
				break;
			case LIGHT_FACE:
				light_and_shadow_face(light, index);
				break;
			case LIGHT_DYNAMIC:
				light_and_shadow_dynamic(light, index);
				break;
			default:
				break;
		}
	}
}

/**
 * @brief
 */
void main(void) {

	fragment.normal = normalize(vertex.normal);
	fragment.tangent = normalize(vertex.tangent);
	fragment.bitangent = normalize(vertex.bitangent);
	fragment.tbn = mat3(fragment.tangent, fragment.bitangent, fragment.normal);
	fragment.dir = normalize(-vertex.position);

	if ((stage.flags & STAGE_MATERIAL) == STAGE_MATERIAL) {

		fragment.diffusemap = sample_diffusemap();
		fragment.diffusemap *= vertex.color;

		if (fragment.diffusemap.a < material.alpha_test) {
			discard;
		}

		fragment.normalmap = sample_normalmap();
		fragment.specularmap = sample_specularmap();

		if (model_type == MODEL_BSP) {
			fragment.ambient = sample_lightmap_ambient();
			fragment.diffuse = sample_lightmap_diffuse();
			fragment.direction = sample_lightmap_direction();
			fragment.caustics = sample_lightmap_caustics();
		} else {
			fragment.ambient = sample_lightgrid_ambient();
			fragment.diffuse = sample_lightgrid_diffuse();
			fragment.direction = sample_lightgrid_direction();
			fragment.caustics = sample_lightgrid_caustics();
		}

		fragment.ambient *= max(0.0, dot(fragment.normal, fragment.normalmap));
		fragment.diffuse *= max(0.0, dot(fragment.direction, fragment.normalmap));
		fragment.specular += blinn_phong(fragment.diffuse, fragment.direction);
		fragment.specular += blinn_phong(fragment.ambient, fragment.normal);

		light_and_shadow();

		out_color = fragment.diffusemap;

		vec3 stainmap = sample_lightmap_stains();

		out_color.rgb = max(out_color.rgb * (fragment.ambient + fragment.diffuse) * stainmap, 0.0);
		out_color.rgb = max(out_color.rgb + fragment.specular * stainmap, 0.0);

		//lightgrid_fog(out_color, texture_lightgrid_fog, vertex.position, vertex.lightgrid);
		fragment.fog = sample_lightgrid_fog();
		out_color.rgb = out_color.rgb * (1.0 - fragment.fog.a) + fragment.fog.rgb * fragment.fog.a;

		float exposure = lightgrid.view_coordinate.w;
		out_color.rgb += out_color.rgb / exposure;

		out_bloom.rgb = max(out_color.rgb * material.bloom - 1.0, 0.0);
		out_bloom.a = out_color.a;

		if (lightmaps == 1) {
			out_color.rgb = fragment.ambient;
		} else if (lightmaps == 2) {
			out_color.rgb = fragment.diffuse;
		} else if (lightmaps == 3) {
			out_color.rgb = fragment.specular;
		} else if (lightmaps == 4) {
			out_color.rgb = fragment.ambient + fragment.diffuse + fragment.specular;
		} else if (lightmaps == 5) {
			out_color.rgb = texture(texture_lightmap_direction, vertex.lightmap).xyz * 2.0 - 1.0;
		} else if (lightmaps == 6) {
			out_color.rgb = sample_normalmap();
		}

	} else {

		vec2 st = vertex.diffusemap;

		if ((stage.flags & STAGE_WARP) == STAGE_WARP) {
			st += texture(texture_warp, st + vec2(ticks * stage.warp.x * 0.000125)).xy * stage.warp.y;
		}

		vec4 effect = pow(texture(texture_stage, st), vec4(vec3(gamma), 1.0));
		effect *= vertex.color;

		if ((stage.flags & STAGE_LIGHTMAP) == STAGE_LIGHTMAP) {
			vec3 ambient = sample_lightmap_ambient();
			vec3 diffuse = sample_lightmap_diffuse();
			vec3 direction = sample_lightmap_direction();
			diffuse *= max(0.0, dot(diffuse, direction));

			effect.rgb *= (ambient + diffuse);
		}

		out_bloom.rgb = max(effect.rgb * material.bloom - 1.0, 0.0);
		out_bloom.a = effect.a;

		if ((stage.flags & STAGE_FOG) == STAGE_FOG) {
			lightgrid_fog(effect, texture_lightgrid_fog, vertex.position, vertex.lightgrid);
		}

		out_color = effect;
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

