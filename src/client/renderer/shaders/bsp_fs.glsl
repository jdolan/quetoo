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

#define TEXTURE_DIFFUSE                  0
#define TEXTURE_NORMALMAP                1
#define TEXTURE_GLOSSMAP                 2
#define TEXTURE_LIGHTMAP                 3
#define TEXTURE_STAINMAP                 4

#define TEXTURE_MASK_DIFFUSE            (1 << TEXTURE_DIFFUSE)
#define TEXTURE_MASK_NORMALMAP          (1 << TEXTURE_NORMALMAP)
#define TEXTURE_MASK_GLOSSMAP           (1 << TEXTURE_GLOSSMAP)
#define TEXTURE_MASK_LIGHTMAP           (1 << TEXTURE_LIGHTMAP)
#define TEXTURE_MASK_STAINMAP           (1 << TEXTURE_STAINMAP)
#define TEXTURE_MASK_ALL                0xff

uniform int textures;

uniform sampler2D texture_diffuse;
uniform sampler2D texture_normalmap;
uniform sampler2D texture_glossmap;
uniform sampler2DArray texture_lightmap;

uniform float alpha_threshold;

uniform float modulate;

uniform float bump;
uniform float parallax;
uniform float hardness;
uniform float specular;

uniform vec3 fog_parameters;
uniform vec3 fog_color;

uniform vec4 caustics;

in vertex_data {
	vec3 position;
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	vec2 diffuse;
	vec2 lightmap;
	vec4 color;
	vec3 eye;
} vertex;

out vec4 out_color;

vec3 light_diffuse;
vec3 light_specular;

/**
* @brief Clamps to [0.0, 1.0], like in HLSL.
*/
float saturate(float x) {
	return clamp(x, 0.0, 1.0);
}

/**
* @brief Clamps vec3 components to [0.0, 1.0], like in HLSL.
*/
vec3 saturate3(vec3 v) {
	v.x = saturate(v.x);
	v.y = saturate(v.y);
	v.z = saturate(v.z);
	return v;
}

/**
 * @brief Adds fog on top of the scene.
 */
void apply_fog(inout vec4 scene_color) {
	float near  = fog_parameters.x;
	float far   = fog_parameters.y;
	float scale = fog_parameters.z;
	
	float strength;
	strength = (length(vertex.position) - near) / (far - near);
	strength = clamp(strength * scale, 0.0, 1.0);
	
	scene_color.rgb = mix(scene_color.rgb, fog_color, strength);
}

/**
 * @brief Prevents surfaces from becoming overexposed by lights (looks bad).
 */
void apply_tonemap(inout vec4 color) {
	// clamp to fudge factor to avoid precision issues
	color.rgb *= exp(color.rgb);
	color.rgb /= color.rgb + 0.825;
}

/**
 * Converts uniform distribution into triangle-shaped distribution.
 */
float remap_triangular(float v) {
	
    float original = v * 2.0 - 1.0;
    v = original / sqrt(abs(original));
    v = max(-1.0, v);
    v = v - sign(original) + 0.5;

    return v;

    // result is range [-0.5,1.5] which is useful for actual dithering.
    // convert to [0,1] for output
    // return (v + 0.5f) * 0.5f;
}

/**
 * Converts uniform distribution into triangle-shaped distribution for vec3.
 */
vec3 remap_triangular_3(vec3 c) {
    return vec3(remap_triangular(c.r), remap_triangular(c.g), remap_triangular(c.b));
}

/**
 * Applies dithering before quantizing to 8-bit values to remove color banding.
 */
void apply_dither(inout vec4 color) {

	// The function is adapted from slide 49 of Alex Vlachos's
	// GDC 2015 talk: "Advanced VR Rendering".
	// http://alex.vlachos.com/graphics/Alex_Vlachos_Advanced_VR_Rendering_GDC2015.pdf
	// original shadertoy implementation by Zavie:
	// https://www.shadertoy.com/view/4dcSRX
	// modification with triangular distribution by Hornet (loopit.dk):
	// https://www.shadertoy.com/view/Md3XRf

	vec3 pattern;
	// generate dithering pattern
	pattern = vec3(dot(vec2(131.0, 312.0), gl_FragCoord.xy));
    pattern = fract(pattern / vec3(103.0, 71.0, 97.0));
	// remap distribution for smoother results
	pattern = remap_triangular_3(pattern);
	// scale the magnitude to be the distance between two 8-bit colors
	pattern /= 255.0;
	// apply the pattern, causing some fractional color values to be
	// rounded up and others down, thus removing banding artifacts.
	color.rgb = saturate3(color.rgb + pattern);
}

/**
 * @brief
 */
void main(void) {

	// fetch textures
	
	vec4 diffuse;
	if ((textures & TEXTURE_MASK_DIFFUSE) == TEXTURE_MASK_DIFFUSE) {
		diffuse = texture(texture_diffuse, vertex.diffuse) * vertex.color;

		if (diffuse.a < alpha_threshold) {
			discard;
		}
	} else {
		diffuse = vertex.color;
	}

	vec4 normalmap;
	if ((textures & TEXTURE_MASK_NORMALMAP) == TEXTURE_MASK_NORMALMAP) {
		normalmap = texture(texture_normalmap, vertex.diffuse);
		normalmap.xyz = normalize(normalmap.xyz);
		normalmap.xy = (normalmap.xy * 2.0 - 1.0) * bump;
		normalmap.xyz = normalize(normalmap.xyz);
	} else {
		normalmap = vec4(0.0, 0.0, 1.0, 0.5);
	}

	vec4 glossmap;
	if ((textures & TEXTURE_MASK_GLOSSMAP) == TEXTURE_MASK_GLOSSMAP) {
		glossmap = texture(texture_glossmap, vertex.diffuse);
	} else {
		glossmap = vec4(1.0);
	}

	vec3 lightmap;
	vec3 diffuse_dir;
	if ((textures & TEXTURE_MASK_LIGHTMAP) == TEXTURE_MASK_LIGHTMAP) {
		vec3 ambient = texture(texture_lightmap, vec3(vertex.lightmap, 0)).rgb * modulate;
		vec3 diffuse = texture(texture_lightmap, vec3(vertex.lightmap, 1)).rgb * modulate;
		vec3 radiosity = texture(texture_lightmap, vec3(vertex.lightmap, 2)).rgb * modulate;
		
		diffuse_dir = texture(texture_lightmap, vec3(vertex.lightmap, 3)).xyz;
		diffuse_dir = normalize(diffuse_dir * 2.0 - 1.0);

		lightmap = ambient + diffuse + radiosity;
	} else {
		lightmap = vec3(1.0);
		diffuse_dir = vec3(0.0, 0.0, 1.0);
	}

	vec4 stainmap;
	if ((textures & TEXTURE_MASK_STAINMAP) == TEXTURE_MASK_STAINMAP) {
		stainmap = texture(texture_lightmap, vec3(vertex.lightmap, 2));
	} else {
		stainmap = vec4(0.0);
	}

	light_diffuse = lightmap.rgb;
	light_specular = vec3(0.0);
	
	dynamic_light(vertex.position, vertex.normal, 64, light_diffuse, light_specular);
	
	out_color.rgb = diffuse.rgb;
	out_color.rgb = clamp(out_color.rgb * light_diffuse, 0.0, 32.0);
	out_color.rgb = clamp(out_color.rgb + light_specular, 0.0, 32.0);
	
	apply_tonemap(out_color);
	
	// tonemapping changes fog color, so do it afterwards for now.
	apply_fog(out_color);
	
	out_color = ColorFilter(out_color);
	
	apply_dither(out_color);
}
