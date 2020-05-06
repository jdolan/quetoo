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

uniform float alpha_threshold;

uniform float modulate;

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
 * @brief
 */
void main(void) {

	if ((stage.flags & STAGE_MATERIAL) == STAGE_MATERIAL) {

		float _specularity = material.specularity * 100.0;

		vec4 diffusemap = texture(texture_material, vec3(vertex.diffusemap, 0));
		vec4 normalmap = texture(texture_material, vec3(vertex.diffusemap, 1));
		vec4 glossmap = texture(texture_material, vec3(vertex.diffusemap, 2));

		diffusemap *= vertex.color;

		if (diffusemap.a < alpha_threshold) {
			discard;
		}

		mat3 tbn = mat3(normalize(vertex.tangent), normalize(vertex.bitangent), normalize(vertex.normal));
		vec3 normal = normalize(tbn * ((normalmap.xyz * 2.0 - 1.0) * vec3(material.roughness, material.roughness, 1.0)));

		vec3 ambient = texture(texture_lightmap, vec3(vertex.lightmap, 0)).rgb;
		vec3 diffuse = texture(texture_lightmap, vec3(vertex.lightmap, 1)).rgb;
		vec3 direction = texture(texture_lightmap, vec3(vertex.lightmap, 2)).xyz;

		direction = normalize(tbn * (direction * 2.0 - 1.0));

		vec3 light_diffuse = ambient + diffuse * max(0.0, dot(direction, normal));
		vec3 light_specular = brdf_blinn(normalize(-vertex.position), direction, normal, diffuse, glossmap.a, _specularity);
		light_specular = min(light_specular * 0.2 * glossmap.xyz * material.hardness, MAX_HARDNESS);

		vec3 stainmap = texture_bicubic(texture_lightmap, vec3(vertex.lightmap, 4)).rgb;

		dynamic_light(vertex.position, normal, 64, light_diffuse, light_specular);

		out_color = diffusemap;
		out_color *= vec4(stainmap, 1.0);

		out_color.rgb = clamp(out_color.rgb * light_diffuse  * modulate, 0.0, 32.0);
		out_color.rgb = clamp(out_color.rgb + light_specular * modulate, 0.0, 32.0);

	} else {
		vec2 texcoord = vertex.diffusemap;

		if ((stage.flags & STAGE_WARP) == STAGE_WARP) {
			texcoord += texture(texture_warp, texcoord + vec2(stage.ticks * stage.warp.x * 0.000125)).xy * stage.warp.y;
		}

		vec4 effect = texture(texture_stage, texcoord);

		effect *= vertex.color;

		if ((stage.flags & STAGE_LIGHTMAP) == STAGE_LIGHTMAP) {
			vec3 ambient = texture(texture_lightmap, vec3(vertex.lightmap, 0)).rgb;
			vec3 diffuse = texture(texture_lightmap, vec3(vertex.lightmap, 1)).rgb;

			effect.rgb *= (ambient + diffuse) * modulate;
		}

		out_color = effect;
	}

	// postprocessing
	
	out_color.rgb = tonemap(out_color.rgb);
	
	out_color.rgb = fog(vertex.position, out_color.rgb);
	
	out_color.rgb = color_filter(out_color.rgb);
	
	out_color.rgb = dither(out_color.rgb);
}
