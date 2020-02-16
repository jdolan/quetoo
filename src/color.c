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

#include "color.h"
#include "swap.h"

/**
 * @brief
 */
color_t Color3b(byte r, byte g, byte b) {
	return Color4b(r, g, b, 255);
}

/**
 * @brief
 */
color_t Color3bv(uint32_t rgb) {
	return Color4bv((rgb << 8) | 0x000000ff);
}

/**
 * @brief
 */
color_t Color3f(float r, float g, float b) {
	return Color4f(r, g, b, 1.f);
}

/**
 * @brief
 */
color_t Color3fv(const vec3_t rgb) {
	return Color3f(rgb.x, rgb.y, rgb.z);
}

/**
 * @brief
 */
color_t Color4b(byte r, byte g, byte b, byte a) {
	return (color_t) {
		.r = r,
		.g = g,
		.b = b,
		.a = a
	};
}

/**
 * @brief
 */
color_t Color4bv(uint32_t rgba) {
	return (color_t) {
		.rgba = BigLong(rgba)
	};
}

/**
 * @brief
 */
color_t Color4f(float r, float g, float b, float a) {

	const float max = Maxf(r, Maxf(g, Maxf(b, a)));
	if (max > 1.f) {
		const float inverse = 1.f / max;
		r *= inverse;
		g *= inverse;
		b *= inverse;
	}

	return (color_t) {
		.r = (byte) Clampf(r, 0.f, 1.f) * 255,
		.g = (byte) Clampf(g, 0.f, 1.f) * 255,
		.b = (byte) Clampf(b, 0.f, 1.f) * 255,
		.a = (byte) Clampf(a, 0.f, 1.f) * 255
	};
}

/**
 * @brief
 */
color_t Color4fv(const vec4_t rgba) {
	return Color4f(rgba.x, rgba.y, rgba.z, rgba.w);
}

/**
 * @brief
 */
vec3_t Color_Vec3(const color_t color) {
	return Vec3_Scale(Vec3(color.r, color.g, color.b), 1.f / 255.f);
}

/**
 * @brief
 */
vec4_t Color_Vec4(const color_t color) {
	return Vec4_Scale(Vec4(color.r, color.g, color.b, color.a), 1.f / 255.f);
}
