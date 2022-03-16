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

uniform material_t material;
uniform stage_t stage;

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
} vertex;

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec4 out_bloom;

uniform vec4 tint_colors[3];

/**
 * @brief
 */
vec3 tint_fragment(vec3 diffuse, vec4 tintmap) {
	diffuse.rgb *= 1.0 - tintmap.a;
	
	for (int i = 0; i < 3; i++) {
		diffuse.rgb += (tint_colors[i] * tintmap[i]).rgb * tintmap.a;
	}

	return diffuse.rgb;
}

/**
 * @brief
 */
void main(void) {

	mat3 tbn = mat3(normalize(vertex.tangent), normalize(vertex.bitangent), normalize(vertex.normal));

	vec3 view_dir = normalize(-vertex.position);

	if ((stage.flags & STAGE_MATERIAL) == STAGE_MATERIAL) {

		vec4 diffusemap = texture(texture_material, vec3(vertex.diffusemap, 0));
		diffusemap *= vertex.color;

		if (diffusemap.a < material.alpha_test) {
			discard;
		}

		vec3 normalmap = texture(texture_material, vec3(vertex.diffusemap, 1)).xyz;
		vec4 specularmap = texture(texture_material, vec3(vertex.diffusemap, 2));

		vec4 tintmap = texture(texture_material, vec3(vertex.diffusemap, 3));
		diffusemap.rgb = tint_fragment(diffusemap.rgb, tintmap);

		vec3 roughness = vec3(material.roughness, material.roughness, 1.0);
		vec3 hardness = specularmap.rgb * material.hardness;

		normalmap = normalize(tbn * (normalize(normalmap * 2.0 - 1.0) * roughness));
		vec3 direction = normalize(vertex.direction);

		vec3 ambient = vertex.ambient * max(0.0, dot(vertex.normal, normalmap));
		vec3 diffuse = vertex.diffuse * max(0.0, dot(direction, normalmap));

		float specularity = pow(material.specularity * (hmax(specularmap.rgb) + 1.0), 4.0);
		vec3 specular = diffuse * hardness * pow(max(0.0, dot(reflect(-direction, normalmap), view_dir)), specularity);
		specular += ambient * hardness * pow(max(0.0, dot(reflect(-vertex.normal, normalmap), view_dir)), specularity);

		caustic_light(vertex.model, vertex.caustics, diffuse);

		dynamic_light(vertex.position, normalmap, specularity, diffuse, specular);

		out_color = diffusemap;

		out_color.rgb = clamp(out_color.rgb * (ambient + diffuse) * modulate, 0.0, 1.0);
		out_color.rgb = clamp(out_color.rgb + specular * modulate, 0.0, 1.0);

		out_color.rgb += vertex.fog.rgb * out_color.a;

		out_bloom.rgb = clamp(out_color.rgb * material.bloom - 1.0, 0.0, 1.0);
		out_bloom.a = out_color.a;

		if (lightmaps == 1) {
			out_color.rgb = vertex.ambient + vertex.diffuse;
		} else {
			out_color = postprocess(out_color);
		}
		
	} else {

		// effect
		vec4 effect = texture(texture_stage, vertex.diffusemap);
		effect *= vertex.color;

		if ((stage.flags & STAGE_FOG) == STAGE_FOG) {
			effect.rgb += vertex.fog.rgb * effect.a;
		}

		out_color = effect;

		out_color = postprocess(out_color);
	}
}
