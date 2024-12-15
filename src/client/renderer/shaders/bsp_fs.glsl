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

#define MODEL_BSP        1
#define MODEL_BSP_INLINE 2

uniform int model_type;
uniform mat4 model;

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
	vec2 parallax;
	vec4 diffusemap;
	vec3 normalmap;
	vec4 specularmap;
	vec3 ambient;
	vec3 diffuse;
	vec3 direction;
	vec3 specular;
	vec3 caustics;
	vec4 fog;
	vec3 stains;
} fragment;

/**
 * @brief
 */
float sample_heightmap(vec2 texcoord) {
	return texture(texture_material, vec3(texcoord, 1)).w;
}

/**
 * @brief
 */
float sample_displacement(vec2 texcoord) {
	return 1.0 - sample_heightmap(texcoord);
}

#define PARALLAX_SAMPLES 16

/**
 * @brief Calculates the augmented texcoord for parallax occlusion mapping.
 * @see https://learnopengl.com/Advanced-Lighting/Parallax-Mapping
 */
void parallax_occlusion_mapping() {

	if (material.parallax == 0.0) {
		return;
	}

	vec3 dir = normalize(fragment.dir * fragment.tbn);
	vec2 p = dir.xy / dir.z * material.parallax * .01;
	vec2 delta = p / PARALLAX_SAMPLES;

	vec2 texcoord = vertex.diffusemap;
	vec2 prev_texcoord = vertex.diffusemap;

	float depth = 0.0;
	float displacement = 0.0;
	float layer = 1.0 / PARALLAX_SAMPLES;

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
void light_and_shadow_caustics() {

	if (fragment.caustics == vec3(0.0)) {
		return;
	}

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
vec3 sample_lightgrid_diffuse() {
	return texture(texture_lightgrid_diffuse, vertex.lightgrid).rgb * modulate;
}

/**
 * @brief
 */
vec3 sample_lightgrid_direction() {
	vec3 direction = texture(texture_lightgrid_direction, vertex.lightgrid).xyz * 2.0 - 1.0;
	return vec3(view * vec4(normalize(direction), 0.0));
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

	float samples = clamp(length(vertex.position) / BSP_LIGHTGRID_LUXEL_SIZE, 1.0, fog_samples);

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
	float atten = 1.0 - distance(light_pos, vertex.position) / (radius + size * 0.5);
	if (atten <= 0.0) {
		return;
	}

	vec3 light_dir = normalize(light_pos - vertex.position);
	float lambert = dot(light_dir, fragment.normalmap);
	if (lambert <= 0.0) {
		return;
	}

	float shadow = sample_shadowmap_cube(light, index);
	float shadow_atten = (1.0 - shadow) * lambert * atten;

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
	float atten = 1.0 - distance(light_pos, vertex.position) / (radius + size * 0.5);
	if (atten <= 0.0) {
		return;
	}

	vec3 light_dir = normalize(light_pos - vertex.position);
	float lambert = dot(light_dir, fragment.normalmap);
	if (lambert <= 0.0) {
		return;
	}

	float shadow = sample_shadowmap_cube(light, index);
	float shadow_atten = (1.0 - shadow) * lambert * atten;

	fragment.diffuse -= fragment.diffuse * shadow_atten;
	fragment.specular -= fragment.specular * shadow_atten;
}

/**
 * @brief
 */
void light_and_shadow_brush_side(in light_t light, in int index) {

	float radius = light.model.w;
	if (radius <= 0.0) {
		return;
	}

	float size = light.mins.w;

	vec3 light_pos = light.position.xyz;
	float atten = 1.0 - distance(light_pos, vertex.position) / (radius + size * 0.5);

	if (atten <= 0.0) {
		return;
	}

	vec3 light_dir = normalize(light_pos - vertex.position);
	float lambert = dot(light_dir, fragment.normalmap);
	if (lambert <= 0.0) {
		return;
	}

	float shadow = sample_shadowmap_cube(light, index);
	float shadow_atten = (1.0 - shadow) * lambert * atten;

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

	float atten = 1.0 - distance(light_pos, vertex.position) / (radius + size * 0.5);
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
	float shadow_atten = (1.0 - shadow) * lambert * atten;

	diffuse *= shadow;

	fragment.diffuse += diffuse;
	fragment.specular += blinn_phong(diffuse, light_dir);
}

/**
 * @brief
 */
void light_and_shadow(void) {

	for (int i = 0; i < vertex.num_active_lights; i++) {

		int index = vertex.active_lights[i];

		light_t light = lights[index];

		int type = int(light.position.w);
		switch (type) {
			case LIGHT_SUN:
				light_and_shadow_sun(light, index);
				break;
			case LIGHT_POINT:
				light_and_shadow_point(light, index);
				break;
			case LIGHT_SPOT:
				light_and_shadow_spot(light, index);
				break;
			case LIGHT_BRUSH_SIDE:
				light_and_shadow_brush_side(light, index);
				break;
			case LIGHT_DYNAMIC:
				light_and_shadow_dynamic(light, index);
				break;
			default:
				break;
		}
	}

	light_and_shadow_caustics();
}

/**
 * @brief
 */
void main(void) {

	fragment.dir = normalize(-vertex.position);
	fragment.normal = normalize(vertex.normal);
	fragment.tangent = normalize(vertex.tangent);
	fragment.bitangent = normalize(vertex.bitangent);
	fragment.tbn = mat3(fragment.tangent, fragment.bitangent, fragment.normal);
	fragment.parallax = vertex.diffusemap;
	fragment.specular = vec3(0);

	parallax_occlusion_mapping();

	if ((stage.flags & STAGE_MATERIAL) == STAGE_MATERIAL) {

		fragment.diffusemap = sample_diffusemap() * vertex.color;

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

		fragment.stains = sample_lightmap_stains();
		fragment.fog = sample_lightgrid_fog();

		fragment.ambient *= max(0.0, dot(fragment.normal, fragment.normalmap));
		fragment.diffuse *= max(0.0, dot(fragment.direction, fragment.normalmap));
		fragment.specular += blinn_phong(fragment.diffuse, fragment.direction);
		fragment.specular += blinn_phong(fragment.ambient, fragment.normal);

		light_and_shadow();

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

			fragment.stains = sample_lightmap_stains();

			fragment.diffuse *= max(0.0, dot(fragment.diffuse, fragment.direction));

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
		out_color.rgb = fragment.normalmap;
	} else if (lightmaps == 7) {
		out_color.rgb = vec3(sample_heightmap(vertex.diffusemap));
	} else if (lightmaps == 8) {
		out_color = vec4(fragment.stains, 1.0);
	} else if (lightmaps == 9) {
		out_color.rgb = normalize(fragment.tbn * fragment.dir);
	}
}

