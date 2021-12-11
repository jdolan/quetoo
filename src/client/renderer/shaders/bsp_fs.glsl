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

uniform int entity;
uniform float alpha_threshold;

uniform int bicubic;
uniform int parallax_samples;

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
 * @brief Returns texture coordinates offset via parallax occlusion mapping.
 */
vec2 parallax(sampler2DArray sampler, vec3 uv, vec3 viewdir, float dist, float scale) {

	// TODO: figure out how dFdx/dFdy can be used to improve quality around silhouettes
	// TODO: store the heightmap as a BC4 texture to remove texture sampling bottleneck?
	// TODO: unrolling to 4 iterations per loop supposedly more performant?

	// TODO: do this outside the fragment shader
	#if 1
	float tex_lod; {

		// this shader eats through texture bandwidth like crazy,
		// so limit the size of the sampled texture by limiting
		// the mipmap sampled from.

		// this means lod bias or anisotropy is disregarded which is
		// actually good for parallax since it makes no discernable
		// difference and anisotropy makes it a lot slower

		// 2^7 = 128*128 largest texture size.
		const float max_mipsize = 7;

		vec2 tex_size = textureSize(sampler, 0).xy;
		float mipcount = log2(max(tex_size.x, tex_size.y));

		float min_mip = mipcount - max_mipsize;
		float cur_mip = mipmap_level(uv.xy);

		tex_lod = max(cur_mip, min_mip);
	}
	#else
	float tex_lod = 0;
	#endif

	// TODO: do this outside the fragment shader
	float samplecount = parallax_samples;
	#if 1
	{
		float min_samples = 1;

		#if 1 // fade out at a distance.
		float dist = 1.0 - linearstep(200.0, 500.0, dist);
		samplecount = mix(min_samples, samplecount, dist);
		scale *= dist;
		#endif

		#if 0 // do less work on surfaces orthogonal to the view -- might give subtle peeling artifacts.
		float orthogonality = saturate(dot(viewdir, vec3(0.0, 0.0, 1.0)));
		samplecount = mix(min_samples, samplecount, 1.0 - orthogonality * orthogonality);
		#endif
	}
	#endif

	// record keeping
	vec2  prev_coord;
	vec2  curr_coord = uv.xy - (viewdir.xy * scale * 0.5); // middle-out parallax
	float prev_surface_height;
	float curr_surface_height = textureLod(sampler, uv, tex_lod).a;
	float prev_ray_height;
	float curr_ray_height = 0.0;

	// height offset per step
	float height_delta = 1.0 / samplecount;
	// uv offset per step
	vec2 coord_delta = viewdir.xy * scale / samplecount;

	// raymarch in small steps until we intersect the surface
	while (curr_ray_height <= curr_surface_height) {
		prev_coord = curr_coord;
		curr_coord += coord_delta;
		prev_surface_height = curr_surface_height;
		curr_surface_height = textureLod(sampler, vec3(curr_coord, uv.z), tex_lod).a;
		prev_ray_height = curr_ray_height;
		curr_ray_height += height_delta;
	}

	// take last two heights and lerp between them
	float a = curr_surface_height - curr_ray_height;
	float b = prev_surface_height - prev_ray_height + height_delta;
	vec2 result = mix(curr_coord, prev_coord, a / (a - b));

	return result;
}

/**
 * @brief
 */
void main(void) {

	mat3 tbn = mat3(normalize(vertex.tangent), normalize(vertex.bitangent), normalize(vertex.normal));

	float fragdist = length(vertex.position);

	vec3 viewdir = normalize(-vertex.position);

	vec2 texcoord_material = vertex.diffusemap;
	vec2 texcoord_lightmap = vertex.lightmap;

	if ((stage.flags & STAGE_MATERIAL) == STAGE_MATERIAL) {

		texcoord_material = parallax(texture_material, vec3(texcoord_material, 1.0), viewdir * tbn, fragdist, material.parallax * 0.04);

		vec4 diffusemap = texture(texture_material, vec3(texcoord_material, 0));
		vec4 normalmap = texture(texture_material, vec3(texcoord_material, 1));
		vec4 glossmap = texture(texture_material, vec3(texcoord_material, 2));

		diffusemap *= vertex.color;

		if (diffusemap.a < alpha_threshold) {
			discard;
		}

		vec3 normal = normalize(tbn * ((normalmap.xyz * 2.0 - 1.0) * vec3(material.roughness, material.roughness, 1.0)));

		vec3 ambient = sample_lightmap(0).rgb;
		vec3 diffuse = sample_lightmap(1).rgb;
		vec3 direction = sample_lightmap(2).xyz;
		vec3 caustic = sample_lightmap(3).rgb;

		if (entity > 0) {
			ambient = mix(ambient, texture(texture_lightgrid_ambient, vertex.lightgrid).rgb, .666);
			diffuse = mix(diffuse, texture(texture_lightgrid_diffuse, vertex.lightgrid).rgb, .666);
			direction = mix(direction, texture(texture_lightgrid_direction, vertex.lightgrid).xyz, .666);
			caustic = mix(caustic, texture(texture_lightgrid_caustics, vertex.lightgrid).rgb, .666);
			direction = normalize(direction);
		}

		direction = normalize(tbn * (direction * 2.0 - 1.0));

		float bump_shading = (dot(direction, normal) - dot(direction, vertex.normal)) * 0.5 + 0.5;
		vec3 diffuse_light = ambient + diffuse * 2.0 * bump_shading;

		vec3 specular_light = brdf_blinn(viewdir, direction, normal, diffuse, glossmap.a, material.specularity * 100.0);
		specular_light = min(specular_light * 0.2 * glossmap.xyz * material.hardness, MAX_HARDNESS);

		vec3 stainmap = sample_lightmap(4).rgb;

		caustic_light(vertex.model, caustic, diffuse_light);

		dynamic_light(vertex.position, normal, 64.0, diffuse_light, specular_light);

		out_color = diffusemap;
		out_color *= vec4(stainmap, 1.0);

		out_color.rgb = clamp(out_color.rgb * diffuse_light  * modulate, 0.0, 32.0);
		out_color.rgb = clamp(out_color.rgb + specular_light * modulate, 0.0, 32.0);

		lightgrid_fog(out_color, texture_lightgrid_fog, vertex.position, vertex.lightgrid);

		//out_color.rgb = caustic;
		//out_color.rgb = texture(texture_lightgrid_diffuse, vertex.lightgrid).rgb;

		out_bloom.rgb = clamp(out_color.rgb * out_color.rgb * material.bloom - 1.0, 0.0, 1.0);
		out_bloom.a = out_color.a;
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

		out_color = effect;
	}

	// postprocessing

	out_color = postprocess(out_color);

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
