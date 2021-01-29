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
 * @brief Shim for OpenGL 4.5's fwidth.
 */
vec2 fwidth(vec2 p) {
	return abs(dFdx(p)) + abs(dFdy(p));
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
 * @brief Bicubic texture filtering.
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
 * @brief Groups postprocessing operations
 */
vec3 postprocessing(vec3 color)
{
	color = tonemap(color);
	color = color_filter(color.rgb);
	color = dither(color.rgb);
	return color;
}
