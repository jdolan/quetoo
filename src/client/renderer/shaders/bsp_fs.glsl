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

#define MAX_HARDNESS 16

#define STAGE_TEXTURE      (1 << 0)
#define STAGE_BLEND        (1 << 2)
#define STAGE_COLOR        (1 << 3)
#define STAGE_PULSE        (1 << 4)
#define STAGE_STRETCH      (1 << 5)
#define STAGE_ROTATE       (1 << 6)
#define STAGE_SCROLL_S     (1 << 7)
#define STAGE_SCROLL_T     (1 << 8)
#define STAGE_SCALE_S      (1 << 9)
#define STAGE_SCALE_T      (1 << 10)
#define STAGE_TERRAIN      (1 << 11)
#define STAGE_ANIM         (1 << 12)
#define STAGE_LIGHTMAP     (1 << 13)
#define STAGE_DIRTMAP      (1 << 14)
#define STAGE_ENVMAP       (1 << 15)
#define STAGE_FLARE        (1 << 16)
#define STAGE_FOG          (1 << 17)

uniform sampler2DArray texture_material;
uniform sampler2DArray texture_lightmap;
uniform sampler2D texture_stage;
uniform sampler2D texture_warp;

uniform float alpha_threshold;

uniform float modulate;

uniform float bump;
uniform float parallax;
uniform float hardness;
uniform float specular;
uniform float warp;

uniform int stage;
uniform vec4 color;
uniform float pulse;
uniform vec2 st_origin;
uniform vec2 stretch;
uniform float rotate;
uniform vec2 scroll;
uniform vec2 scale;
uniform vec2 terrain;
uniform vec2 dirtmap;

uniform int ticks;

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

	if (stage == 0) {
		float _specular = specular * 100.0; // fudge the numbers to match the old specular model... kinda...

		vec2 vertex_diffusemap = vertex.diffusemap;
		if (warp != 0.0) {
			vertex_diffusemap += texture(texture_warp, vertex.diffusemap + vec2(ticks * 0.000125)).xy * warp;
		}

		vec4 diffusemap = texture(texture_material, vec3(vertex_diffusemap, 0));
		vec4 normalmap = texture(texture_material, vec3(vertex_diffusemap, 1));
		vec4 glossmap = texture(texture_material, vec3(vertex_diffusemap, 2));

		diffusemap *= vertex.color;

		if (diffusemap.a < alpha_threshold) {
			discard;
		}

		mat3 tbn = mat3(normalize(vertex.tangent), normalize(vertex.bitangent), normalize(vertex.normal));
		vec3 normal = normalize(tbn * ((normalmap.xyz * 2.0 - 1.0) * vec3(bump, bump, 1.0)));

		vec3 ambient = texture(texture_lightmap, vec3(vertex.lightmap, 0)).rgb;
		vec3 diffuse = texture(texture_lightmap, vec3(vertex.lightmap, 1)).rgb;
		vec3 direction = texture(texture_lightmap, vec3(vertex.lightmap, 2)).xyz;

		direction = normalize(tbn * (direction * 2.0 - 1.0));

		vec3 light_diffuse = ambient + diffuse * max(0.0, dot(direction, normal));
		vec3 light_specular = brdf_blinn(normalize(-vertex.position), direction, normal, diffuse, glossmap.a, _specular);
		light_specular = min(light_specular * 0.2 * glossmap.xyz * hardness, MAX_HARDNESS);

		vec3 stainmap = texture_bicubic(texture_lightmap, vec3(vertex.lightmap, 4)).rgb;

		dynamic_light(vertex.position, normal, 64, light_diffuse, light_specular);

		out_color = diffusemap;
		out_color *= vec4(stainmap, 1.0);

		out_color.rgb = clamp(out_color.rgb * light_diffuse  * modulate, 0.0, 32.0);
		out_color.rgb = clamp(out_color.rgb + light_specular * modulate, 0.0, 32.0);
	} else {

		vec2 texcoord = vertex.diffusemap;

		if ((stage & STAGE_ENVMAP) == STAGE_ENVMAP) {
			texcoord = normalize(vertex.position).xy;
		}

		if ((stage & STAGE_STRETCH) == STAGE_STRETCH) {
			float hz = (sin(ticks * stretch.x * 0.00628f) + 1.0) / 2.0;
			float amp = 1.5 - hz * stretch.y;

			texcoord = texcoord - st_origin;
			texcoord = mat2(amp, 0, 0, amp) * texcoord;
			texcoord = texcoord + st_origin;
		}

		if ((stage & STAGE_ROTATE) == STAGE_ROTATE) {
			float theta = ticks * rotate * 0.36;

			texcoord = texcoord - st_origin;
			texcoord = mat2(cos(theta), -sin(theta), sin(theta),  cos(theta)) * texcoord;
			texcoord = texcoord + st_origin;
		}

		if ((stage & STAGE_SCROLL_S) == STAGE_SCROLL_S) {
			texcoord.s += scroll.s * ticks / 1000.0;
		}

		if ((stage & STAGE_SCROLL_T) == STAGE_SCROLL_T) {
			texcoord.t += scroll.t * ticks / 1000.0;
		}

		vec4 effect = texture(texture_stage, texcoord);

		if ((stage & STAGE_COLOR) == STAGE_COLOR) {
			effect *= color;
		}
		
		if ((stage & STAGE_PULSE) == STAGE_PULSE) {
			effect.a *= (sin(pulse * ticks * 0.00628) + 1.0) / 2.0;
		}

		effect *= vertex.color;

		if ((stage & STAGE_LIGHTMAP) == STAGE_LIGHTMAP) {
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
