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
 * @brief Inverse of v.
 */
vec3 invert(vec3 v) {
	return vec3(1.0) - v;
}

/**
 * @brief Clamps to [0.0, 1.0], like in HLSL.
 */
float saturate(float x) {
	return clamp(x, 0.0, 1.0);
}

/**
 * @brief
 */
vec3 hash33(vec3 p) {
	p = fract(p * vec3(.1031,.11369,.13787));
	p += dot(p, p.yxz + 19.19);
	return -1.0 + 2.0 * fract(vec3((p.x + p.y) * p.z, (p.x + p.z) * p.y, (p.y + p.z) * p.x));
}

/**
 * @brief https://www.shadertoy.com/view/4sc3z2
 */
float noise3d(vec3 p) {
	vec3 pi = floor(p);
	vec3 pf = p - pi;

	vec3 w = pf * pf * (3.0 - 2.0 * pf);

	return mix(
		mix(
			mix(dot(pf - vec3(0, 0, 0), hash33(pi + vec3(0, 0, 0))),
				dot(pf - vec3(1, 0, 0), hash33(pi + vec3(1, 0, 0))),
				w.x),
			mix(dot(pf - vec3(0, 0, 1), hash33(pi + vec3(0, 0, 1))),
				dot(pf - vec3(1, 0, 1), hash33(pi + vec3(1, 0, 1))),
				w.x),
			w.z),
		mix(
			mix(dot(pf - vec3(0, 1, 0), hash33(pi + vec3(0, 1, 0))),
				dot(pf - vec3(1, 1, 0), hash33(pi + vec3(1, 1, 0))),
				w.x),
			mix(dot(pf - vec3(0, 1, 1), hash33(pi + vec3(0, 1, 1))),
				dot(pf - vec3(1, 1, 1), hash33(pi + vec3(1, 1, 1))),
				w.x),
			w.z),
		w.y);
}

/**
 * @brief
 */
void caustic_light(in vec3 model, in vec3 color, in vec3 ambient, inout vec3 diffuse) {

	float noise = noise3d(model * .05 + (ticks / 1000.0) * 0.5);

	// make the inner edges stronger, clamp to 0-1

	float thickness = 0.02;
	float glow = 5.0;

	noise = clamp(pow((1.0 - abs(noise)) + thickness, glow), 0.0, 1.0);

	diffuse += clamp((ambient + diffuse) * length(color) * noise * caustics, 0.0, 1.0);
}

/**
 * @brief
 */
float blinn(in vec3 normal, in vec3 light_dir, in vec3 view_dir, in float specularity) {
	return pow(max(0.0, dot(normalize(light_dir + view_dir), normal)), specularity);
}
