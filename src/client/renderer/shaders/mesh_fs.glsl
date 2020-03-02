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

#define TEXTURE_DIFFUSEMAP               0
#define TEXTURE_NORMALMAP                1
#define TEXTURE_GLOSSMAP                 2
#define TEXTURE_LIGHTGRID                3
#define TEXTURE_LIGHTGRID_AMBIENT        3
#define TEXTURE_LIGHTGRID_DIFFUSE        4
#define TEXTURE_LIGHTGRID_RADIOSITY      5
#define TEXTURE_LIGHTGRID_DIFFUSE_DIR    6

#define TEXTURE_MASK_DIFFUSEMAP         (1 << TEXTURE_DIFFUSEMAP)
#define TEXTURE_MASK_NORMALMAP          (1 << TEXTURE_NORMALMAP)
#define TEXTURE_MASK_GLOSSMAP           (1 << TEXTURE_GLOSSMAP)
#define TEXTURE_MASK_LIGHTGRID          (1 << TEXTURE_LIGHTGRID)
#define TEXTURE_MASK_ALL                0xff

uniform mat4 view;

uniform int textures;

uniform sampler2D texture_diffusemap;
uniform sampler2D texture_normalmap;
uniform sampler2D texture_glossmap;

uniform sampler3D texture_lightgrid_ambient;
uniform sampler3D texture_lightgrid_diffuse;
uniform sampler3D texture_lightgrid_radiosity;
uniform sampler3D texture_lightgrid_diffuse_dir;

uniform vec4 color;
uniform float alpha_threshold;

uniform float modulate;

uniform float bump;
uniform float parallax;
uniform float hardness;
uniform float specular;

uniform vec4 caustics;

in vertex_data {
	vec3 position;
	vec3 normal;
	vec2 diffusemap;
	vec3 lightgrid;
} vertex;

out vec4 out_color;

/**
 * @brief
 */
void main(void) {

	vec4 diffusemap;
	if ((textures & TEXTURE_MASK_DIFFUSEMAP) == TEXTURE_MASK_DIFFUSEMAP) {
		diffusemap = texture(texture_diffusemap, vertex.diffusemap) * color;

		if (diffusemap.a < alpha_threshold) {
			discard;
		}
	} else {
		diffusemap = color;
	}

	vec4 normalmap;
	if ((textures & TEXTURE_MASK_NORMALMAP) == TEXTURE_MASK_NORMALMAP) {
		normalmap = texture(texture_normalmap, vertex.diffusemap);
		normalmap.xyz = normalize(normalmap.xyz);
		normalmap.xy = (normalmap.xy * 2.0 - 1.0) * bump;
		normalmap.xyz = normalize(normalmap.xyz);
	} else {
		normalmap = vec4(0.0, 0.0, 1.0, 0.5);
	}

	vec3 glossmap;
	if ((textures & TEXTURE_MASK_GLOSSMAP) == TEXTURE_MASK_GLOSSMAP) {
		glossmap = texture(texture_glossmap, vertex.diffusemap).rgb * specular;
	} else {
		glossmap = vec3(0.5) * specular;
	}

	vec3 ambient, diffuse, radiosity, diffuse_dir;
	if ((textures & TEXTURE_MASK_LIGHTGRID) == TEXTURE_MASK_LIGHTGRID) {
		ambient = texture(texture_lightgrid_ambient, vertex.lightgrid).rgb * modulate;
		diffuse = texture(texture_lightgrid_diffuse, vertex.lightgrid).rgb * modulate;
		radiosity = texture(texture_lightgrid_radiosity, vertex.lightgrid).rgb * modulate;

		diffuse_dir = texture(texture_lightgrid_diffuse_dir, vertex.lightgrid).xyz;
		diffuse_dir = normalize((view * vec4(diffuse_dir * 2.0 - 1.0, 1.0)).xyz);
	} else {
		ambient = vec3(1.0) * modulate;
		diffuse = vec3(0.0) * modulate;
		radiosity = vec3(0.0) * modulate;
		diffuse_dir = vertex.normal;
	}

	out_color = diffusemap;

	mat3 tbn = cotangent_frame(normalize(vertex.normal), normalize(-vertex.position), vertex.diffusemap);

	vec3 normal = normalize(tbn * normalmap.xyz);

	out_color.rgb *= ambient +
					 radiosity +
					 directional_light(vertex.position, normal, diffuse_dir, diffuse, specular) +
	                 dynamic_light(vertex.position, normal, specular);

	// postprocessing
	
	out_color.rgb = tonemap(out_color.rgb);

	out_color.rgb = fog(vertex.position, out_color.rgb);

	out_color.rgb = color_filter(out_color.rgb);

	out_color.rgb = dither(out_color.rgb);
}

