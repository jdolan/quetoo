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
uniform sampler2D texture_stage;
uniform sampler2DArrayShadow texture_shadowmap;
uniform samplerCubeArrayShadow texture_shadowmap_cube;

uniform material_t material;
uniform stage_t stage;

uniform vec4 tint_colors[3];

in vertex_data {
	vec3 model;
	vec3 position;
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	vec2 diffusemap;
	vec4 color;
	vec3 ambient;
	vec3 diffuse;
	vec3 direction;
	vec3 caustics;
	vec4 fog;

	flat int active_lights[MAX_LIGHT_UNIFORMS_ACTIVE];
	flat int num_active_lights;
} vertex;

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec4 out_bloom;

struct fragment_t {
	vec3 view_dir;
	vec4 diffusemap;
	vec3 normalmap;
	vec4 specularmap;
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
	vec3 caustics;
} fragment;

/**
 * @brief
 */
void tint_fragment() {

	vec4 tintmap = texture(texture_material, vec3(vertex.diffusemap, 3));

	fragment.diffusemap.rgb *= 1.0 - tintmap.a;
	
	for (int i = 0; i < 3; i++) {
		fragment.diffusemap.rgb += (tint_colors[i] * tintmap[i]).rgb * tintmap.a;
	}
}

/**
 * @brief
 */
void main(void) {

	if ((stage.flags & STAGE_MATERIAL) == STAGE_MATERIAL) {

		fragment.diffusemap = texture(texture_material, vec3(vertex.diffusemap, 0));
		fragment.diffusemap *= vertex.color;

		if (fragment.diffusemap.a < material.alpha_test) {
			discard;
		}

		tint_fragment();

		mat3 tbn = mat3(normalize(vertex.tangent), normalize(vertex.bitangent), normalize(vertex.normal));

		fragment.normalmap = texture(texture_material, vec3(vertex.diffusemap, 1)).xyz;
		fragment.normalmap = normalize(tbn * normalize((fragment.normalmap * 2.0 - 1.0) * vec3(vec2(material.roughness), 1.0)));

		fragment.specularmap.xyz = texture(texture_material, vec3(vertex.diffusemap, 2)).rgb * material.hardness;
		fragment.specularmap.w = toksvig(texture_material, vec3(vertex.diffusemap, 1), material.roughness, material.specularity);

		vec3 view_dir = normalize(-vertex.position);
		vec3 direction = normalize(vertex.direction);

		fragment.ambient = vertex.ambient * modulate * max(0.0, dot(vertex.normal, fragment.normalmap));
		fragment.diffuse = vertex.diffuse * modulate * max(0.0, dot(direction, fragment.normalmap));

		fragment.specular = vec3(0.0);

		fragment.specular += fragment.diffuse * fragment.specularmap.xyz * blinn(fragment.normalmap, direction, view_dir, fragment.specularmap.w);
		fragment.specular += fragment.ambient * fragment.specularmap.xyz * blinn(fragment.normalmap, vertex.normal, view_dir, fragment.specularmap.w);

		caustic_light(vertex.model, vertex.caustics, fragment.ambient, fragment.diffuse);

		out_color = fragment.diffusemap;

		out_color.rgb = clamp(out_color.rgb * (fragment.ambient + fragment.diffuse), 0.0, 1.0);
		out_color.rgb = clamp(out_color.rgb + fragment.specular, 0.0, 1.0);

		out_bloom.rgb = clamp(out_color.rgb * material.bloom - 1.0, 0.0, 1.0);
		out_bloom.a = out_color.a;

		out_color.rgb += vertex.fog.rgb * out_color.a;

		if (lightmaps == 1) {
			out_color.rgb = modulate * (vertex.ambient + vertex.diffuse);
		} else if (lightmaps == 2) {
			out_color.rgb = inverse(tbn) * vertex.direction;
		} else if (lightmaps == 3) {
			out_color.rgb = fragment.ambient + fragment.diffuse;
		} else {
			out_color = postprocess(out_color);
		}

	} else {

		vec4 effect = texture(texture_stage, vertex.diffusemap);
		effect *= vertex.color;

		out_bloom.rgb = clamp(effect.rgb * material.bloom - 1.0, 0.0, 1.0);
		out_bloom.a = effect.a;

		if ((stage.flags & STAGE_FOG) == STAGE_FOG) {
			effect.rgb += vertex.fog.rgb * effect.a;
		}

		out_color = effect;

		out_color = postprocess(out_color);
	}
}
