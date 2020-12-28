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

/**
 * @brief Horizontal maximum, returns the maximum component of a vector.
 */
float hmax(vec2 v) {
	return max(v.x, v.y);
}

/**
 * @brief Horizontal maximum, returns the maximum component of a vector.
 */
float hmax(vec3 v) {
	return max(max(v.x, v.y), v.z);
}

/**
 * @brief Horizontal maximum, returns the maximum component of a vector.
 */
float hmax(vec4 v) {
	return max(max(v.x, v.y), max(v.z, v.w));
}

/**
 * @brief Horizontal minimum, returns the minimum component of a vector.
 */
float hmin(vec2 v) {
	return min(v.x, v.y);
}

/**
 * @brief Horizontal minimum, returns the minimum component of a vector.
 */
float hmin(vec3 v) {
	return min(min(v.x, v.y), v.z);
}

/**
 * @brief Horizontal minimum, returns the minimum component of a vector.
 */
float hmin(vec4 v) {
	return min(min(v.x, v.y), min(v.z, v.w));
}

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
 * @brief Brightness, contrast, saturation and gamma.
 */
vec3 color_filter(vec3 color) {

	vec3 luminance = vec3(0.2125, 0.7154, 0.0721);
	vec3 bias = vec3(0.5);

	vec3 scaled = mix(vec3(1.0), color, gamma) * brightness;

	color = mix(bias, mix(vec3(dot(luminance, scaled)), scaled, saturation), contrast);

	return color;
}

/**
 * @brief Phong BRDF for specular highlights.
 */
vec3 brdf_phong(vec3 view_dir, vec3 light_dir, vec3 normal,
	vec3 light_color, float specular_intensity, float specular_exponent) {

	vec3 reflection = reflect(light_dir, normal);
	float r_dot_v = max(-dot(view_dir, reflection), 0.0);
	return light_color * specular_intensity * pow(r_dot_v, 16.0 * specular_exponent);
}

/**
 * @brief Blinn-Phong BRDF for specular highlights.
 */
vec3 brdf_blinn(vec3 view_dir, vec3 light_dir, vec3 normal,
	vec3 light_color, float glossiness, float specular_exponent) {

	vec3 half_angle = normalize(light_dir + view_dir);
	float n_dot_h = max(dot(normal, half_angle), 0.0);
	float p = specular_exponent * glossiness;
	float gloss = pow(n_dot_h, p) * (p + 2.0) / 8.0; // energy preserving
	return light_color * max(gloss, 0.001);
}

/**
 * @brief Lambert BRDF for diffuse lighting.
 */
vec3 brdf_lambert(vec3 light_dir, vec3 normal, vec3 light_color) {
	return light_color * max(dot(light_dir, normal), 0.0);
}

/**
 * @brief Half-Lambert BRDF for diffuse lighting.
 */
vec3 brdf_halflambert(vec3 light_dir, vec3 normal, vec3 light_color) {
	return light_color * (1.0 - (dot(normal, light_dir) * 0.5 + 0.5));
}

/**
 * @brief Prevents surfaces from becoming overexposed by lights (looks bad).
 */
vec3 tonemap(vec3 color) {
	#if 1
	color *= exp(color);
	color /= color + 0.825;
	return color;
	#else
	color = color * color;
	return sqrt(color / (color + 0.75));
	#endif
}

/**
 * @brief Converts uniform distribution into triangle-shaped distribution. Used for dithering.
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
 * @brief Converts uniform distribution into triangle-shaped distribution for vec3. Used for dithering.
 */
vec3 remap_triangular_3(vec3 c) {
    return vec3(remap_triangular(c.r), remap_triangular(c.g), remap_triangular(c.b));
}

/**
 * @brief Applies dithering before quantizing to 8-bit values to remove color banding.
 */
vec3 dither(vec3 color) {

	// The function is adapted from slide 49 of Alex Vlachos's
	// GDC 2015 talk: "Advanced VR Rendering".
	// http://alex.vlachos.com/graphics/Alex_Vlachos_Advanced_VR_Rendering_GDC2015.pdf
	// original shadertoy implementation by Zavie:
	// https://www.shadertoy.com/view/4dcSRX
	// modification with triangular distribution by Hornet (loopit.dk):
	// https://www.shadertoy.com/view/Md3XRf

	// FIXME: replace gl_FragCoord (deprecated) with a varying

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
	return saturate3(color + pattern);
}

/**
 * @brief
 */
vec3 pow3(vec3 v, float exponent) {
	v.x = pow(v.x, exponent);
	v.y = pow(v.y, exponent);
	v.z = pow(v.z, exponent);
	return v;
}

/**
 * @brief
 */
vec4 cubic(float v) {
	vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
	vec4 s = n * n * n;
	float x = s.x;
	float y = s.y - 4.0 * s.x;
	float z = s.z - 4.0 * s.y + 6.0 * s.x;
	float w = 6.0 - x - y - z;
	return vec4(x, y, z, w) * (1.0/6.0);
}

/**
 * @brief
 */
vec4 texture_bicubic(sampler2DArray sampler, vec3 coords) {

	// source: http://www.java-gaming.org/index.php?topic=35123.0

	vec2 texCoords = coords.xy;
	float layer = coords.z;

	vec2 texSize = textureSize(sampler, 0).xy;
	vec2 invTexSize = 1.0 / texSize;

	texCoords = texCoords * texSize - 0.5;

	vec2 fxy = fract(texCoords);
	texCoords -= fxy;

	vec4 xcubic = cubic(fxy.x);
	vec4 ycubic = cubic(fxy.y);

	vec4 c = texCoords.xxyy + vec2(-0.5, +1.5).xyxy;

	vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);
	vec4 offset = c + vec4(xcubic.yw, ycubic.yw) / s;

	offset *= invTexSize.xxyy;

	vec4 sample0 = texture(sampler, vec3(offset.xz, layer));
	vec4 sample1 = texture(sampler, vec3(offset.yz, layer));
	vec4 sample2 = texture(sampler, vec3(offset.xw, layer));
	vec4 sample3 = texture(sampler, vec3(offset.yw, layer));

	float sx = s.x / (s.x + s.y);
	float sy = s.z / (s.z + s.w);

	return mix(
	   mix(sample3, sample2, sx), mix(sample1, sample0, sx)
	, sy);
}

/**
* @brief
*/
float grayscale(vec3 color) {
	return dot(color, vec3(0.299, 0.587, 0.114));
}

/**
 * @brief Clamp value t to range [a,b] and map [a,b] to [0,1].
 */
float linearstep(float a, float b, float t) {
	return clamp((t - a) / (b - a), 0.0, 1.0);
}

/**
 * @brief Get (approximate) mipmap level.
 */
float mipmap_level(vec2 uv) {
    vec2  dx = dFdx(uv);
    vec2  dy = dFdy(uv);
    float delta_max_sqr = max(dot(dx, dx), dot(dy, dy));

    return 0.5 * log2(delta_max_sqr); // == log2(sqrt(delta_max_sqr));
}

/**
 * @brief
 */
void dynamic_light(in int lights_mask, in vec3 position, in vec3 normal, in float specular_exponent,
				   inout vec3 diffuse_light, inout vec3 specular_light) {

	for (int i = 0; i < MAX_LIGHTS; i++) {

		if ((lights_mask & (1 << i)) == 0) {
			continue;
		}

		float radius = lights[i].origin.w;
		if (radius == 0.0) {
			continue;
		}

		float intensity = lights[i].color.w;
		if (intensity == 0.0) {
			continue;
		}

		float dist = distance(lights[i].origin.xyz, position);
		if (dist < radius) {

			vec3 light_dir = normalize(lights[i].origin.xyz - position);
			float angle_atten = dot(light_dir, normal);
			if (angle_atten > 0.0) {

				float dist_atten;
				dist_atten = 1.0 - dist / radius;
				dist_atten *= dist_atten; // for looks, not for correctness

				float attenuation = dist_atten * angle_atten;

				vec3 view_dir = normalize(-position);
				vec3 half_dir = normalize(light_dir + view_dir);

				float specular_base = max(dot(half_dir, normal), 0.0);
				float specular = pow(specular_base, specular_exponent);

				vec3 color = lights[i].color.rgb * intensity;

				diffuse_light += attenuation * radius * color;
				specular_light += attenuation * attenuation * radius * specular * color;
			}
		}
	}
}
