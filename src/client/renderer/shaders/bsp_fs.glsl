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
uniform sampler3D texture_lightgrid_direct;
uniform sampler3D texture_lightgrid_direct_dir;
uniform sampler3D texture_lightgrid_indirect;
uniform sampler3D texture_lightgrid_indirect_dir;
uniform sampler3D texture_lightgrid_caustics;
uniform sampler3D texture_lightgrid_fog;

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
} vertex;

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec4 out_bloom;

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
void main(void) {

	mat3 tbn = mat3(normalize(vertex.tangent), normalize(vertex.bitangent), normalize(vertex.normal));

	vec3 viewdir = normalize(-vertex.position);

	vec2 texcoord_material = vertex.diffusemap;
	vec2 texcoord_lightmap = vertex.lightmap;

	if ((stage.flags & STAGE_MATERIAL) == STAGE_MATERIAL) {

		vec4 diffusemap = texture(texture_material, vec3(texcoord_material, 0));
		vec4 normalmap = texture(texture_material, vec3(texcoord_material, 1));
		vec4 normalmap_mipofs1 = texture(texture_material, vec3(texcoord_material, 1), 1);
		vec4 glossmap = texture(texture_material, vec3(texcoord_material, 2));

		vec3 normalmap_scaled = (normalmap.xyz * 2.0 - 1.0) * vec3(material.roughness, material.roughness, 1.0);
		vec3 normalmap_mipofs1_scaled = (normalmap_mipofs1.xyz * 2.0 - 1.0) * vec3(material.roughness, material.roughness, 1.0);

		diffusemap *= vertex.color;

		if (diffusemap.a < material.alpha_test) {
			discard;
		}

		vec3 normal = normalize(tbn * normalmap_scaled);

		vec3 ambient = sample_lightmap(0).rgb;
		vec3 direct = sample_lightmap(1).rgb;
		vec3 direct_dir = sample_lightmap(2).xyz;
		vec3 indirect = sample_lightmap(3).rgb;
		vec3 indirect_dir = sample_lightmap(4).xyz;
		vec3 caustics = sample_lightmap(5).rgb;

		if (entity > 0) {
			ambient = mix(ambient, texture(texture_lightgrid_ambient, vertex.lightgrid).rgb, .666);

			direct = mix(direct, texture(texture_lightgrid_direct, vertex.lightgrid).rgb, .666);
			vec3 lg_direct_dir = texture(texture_lightgrid_direct_dir, vertex.lightgrid).xyz;
			lg_direct_dir = normalize((view * vec4(lg_direct_dir * 2.0 - 1.0, 0.0)).xyz);
			direct *= max(0.0, dot(normal, lg_direct_dir));
			direct_dir = normalize(mix(direct_dir, lg_direct_dir, .666));

			indirect = mix(indirect, texture(texture_lightgrid_indirect, vertex.lightgrid).rgb, .666);
			vec3 lg_indirect_dir = texture(texture_lightgrid_indirect_dir, vertex.lightgrid).xyz;
			lg_indirect_dir = normalize((view * vec4(lg_indirect_dir * 2.0 - 1.0, 0.0)).xyz);
			indirect *= max(0.0, dot(normal, lg_indirect_dir));
			indirect_dir = normalize(mix(indirect_dir, lg_indirect_dir, .666));

			caustics = texture(texture_lightgrid_caustics, vertex.lightgrid).rgb;
		}

		vec3 direct_dir_ts = normalize(tbn * (direct_dir * 2.0 - 1.0));
		vec3 indirect_dir_ts = normalize(tbn * (indirect_dir * 2.0 - 1.0));

		float bump_shade_direct = (dot(direct_dir_ts, normal) - dot(direct_dir,   vertex.normal)) * 0.5 + 0.5;
		float bump_shade_indirect = (dot(indirect_dir_ts, normal) - dot(indirect_dir, vertex.normal)) * 0.5 + 0.5;
		// The * 2.0 is because the bump shade neutral value is 0.5.
		vec3 diffuse = ambient + (direct * 2.0 * bump_shade_direct) + (indirect * 2.0 * bump_shade_indirect);

		// * 100.0 = eyeballed magic value for old content
		float power = material.specularity * 100.0;

		float gloss = min(
			toksvig(normalmap_scaled.xyz, power),
			toksvig(normalmap_mipofs1_scaled.xyz, power));

		float specular_direct = blinn(saturate(dot(normalize(viewdir + direct_dir_ts), normal)), gloss * glossmap.a, power);
		float specular_indirect = blinn(saturate(dot(normalize(viewdir + indirect_dir_ts), normal)), gloss * glossmap.a, power * 0.05);

		vec3 specular_light = (direct * specular_direct) + (indirect * specular_indirect);

		// * 0.2 = eyeballed magic value for old content
		specular_light = min(specular_light * 0.2 * glossmap.xyz * material.hardness, MAX_HARDNESS);

		vec3 stainmap = sample_lightmap(6).rgb;

		caustic_light(vertex.model, caustics, diffuse);

		dynamic_light(vertex.position, normal, 64.0, diffuse, specular_light);

		out_color = diffusemap;
		out_color.rgb *= stainmap;

		out_color.rgb = clamp(out_color.rgb * diffuse  * modulate, 0.0, 32.0);
		out_color.rgb = clamp(out_color.rgb + specular_light * modulate, 0.0, 32.0);

		out_bloom.rgb = clamp(out_color.rgb * out_color.rgb * material.bloom - 1.0, 0.0, 1.0);
		out_bloom.a = out_color.a;

		lightgrid_fog(out_color, texture_lightgrid_fog, vertex.position, vertex.lightgrid);
		global_fog(out_color, vertex.position);

		int val = 0;
		if (lightmaps == ++val) {
			out_color.rgb = diffuse * 0.5 + ambient * 0.5;
		} else if (lightmaps == ++val) {
			out_color.rgb = diffuse;
		} else if (lightmaps == ++val) {
			out_color.rgb = ambient;
		} else if (lightmaps == ++val) {
			out_color.rgb = direct_dir;
		} else if (lightmaps == ++val) {
			out_color.rgb = vec3(bump_shade_direct);
		} else if (lightmaps == ++val) {
			out_color.rgb = vec3(bump_shade_indirect);
		} else if (lightmaps == ++val) {
			out_color.rgb = (vec3(bump_shade_direct) + vec3(bump_shade_indirect)) * 0.5;
		} else if (lightmaps == ++val) {
			out_color.rgb = diffuse * specular_direct;
		} else if (lightmaps == ++val) {
			out_color.rgb = ambient * specular_indirect;
		} else if (lightmaps == ++val) {
			out_color.rgb = specular_light;
		} else if (lightmaps == ++val) {
			out_color.rgb = vec3(gloss);
		} else if (lightmaps == ++val) {
			out_color = diffusemap;
		} else if (lightmaps == ++val) {
			out_color.rgb = stainmap.rgb;
		} else {
			// postprocessing
			out_color = postprocess(out_color);
		}

		//out_color.rgb = caustics
		//out_color.rgb = texture(texture_lightgrid_direct, vertex.lightgrid).rgb;
		//out_color.rgb = diffuse + ambient;
		//out_color.rgb = sample_lightmap(1).xyz;
	} else {

		if ((stage.flags & STAGE_WARP) == STAGE_WARP) {
			texcoord_material += texture(texture_warp, texcoord_material + vec2(ticks * stage.warp.x * 0.000125)).xy * stage.warp.y;
		}

		vec4 effect = texture(texture_stage, texcoord_material);

		effect *= vertex.color;

		if ((stage.flags & STAGE_LIGHTMAP) == STAGE_LIGHTMAP) {
			vec3 ambient = sample_lightmap(0).rgb;
			vec3 diffuse = sample_lightmap(1).rgb;

			effect.rgb *= (ambient + diffuse) * modulate;
		}

		if ((stage.flags & STAGE_FOG) == STAGE_FOG) {
			lightgrid_fog(effect, texture_lightgrid_fog, vertex.position, vertex.lightgrid);
			global_fog(effect, vertex.position);
		}

		out_color = effect;

		// postprocessing
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

	#if 0
	// draw flat lightmaps
	out_color.rgb = sample_lightmap(0).rgb + sample_lightmap(1).rgb;
	#endif
}
