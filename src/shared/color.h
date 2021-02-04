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

#pragma once

#include "vector.h"

/**
 * @brief Color constants.
 */
#define color_black			Color3bv(0x000000)
#define color_red			Color3bv(0x0000ff)
#define color_green			Color3bv(0x00ff00)
#define color_yellow		Color3bv(0x00ffff)
#define color_blue			Color3bv(0xff0000)
#define color_magenta		Color3bv(0xff00ff)
#define color_cyan			Color3bv(0xffff00)
#define color_white			Color3bv(0xffffff)
#define color_transparent	Color4bv(0);

#define color_hue_red				0.f
#define color_hue_orange			30.f
#define color_hue_yellow			60.f
#define color_hue_chartreuse_green	90.f
#define color_hue_green				120.f
#define color_hue_spring_green		150.f
#define color_hue_cyan				180.f
#define color_hue_azure				210.f
#define color_hue_blue				240.f
#define color_hue_violet			270.f
#define color_hue_magenta			300.f
#define color_hue_rose				330.f

/**
 * @brief A clamped floating point RGBA color.
 */
typedef union {
	/**
	 * @brief Component accessors.
	 */
	struct {
		float r, g, b, a;
	};

	/**
	 * @brief Array accessor.
	 */
	float rgba[4];
} color_t;

/**
 * @brief A clamped integer RGBA color.
 */
typedef union {

	/**
	 * @brief Component accessors.
	 */
	struct {
		byte r, g, b, a;
	};

	/**
	 * @brief Array accessor.
	 */
	byte bytes[4];

	/**
	 * @brief Integer accessor.
	 */
	int32_t rgba;
} color32_t;

/**
 * @return A color with the specified RGB byte values.
 */
static inline color_t Color3b(byte r, byte g, byte b);

/**
 * @return A color with the specified RGB integer, e.g. `0xbbaadd`.
 */
static inline color_t Color3bv(uint32_t rgb);

/**
 * @return A color with the specified RGB floating point values. Note that the resulting color is clamped.
 */
static inline color_t Color3f(float r, float g, float b);

/**
 * @return A color with the specified RGB vector. Note that the resulting color is clamped.
 */
static inline color_t Color3fv(const vec3_t rgb);

/**
 * @return A color with the specified RGBA bytes.
 */
static inline color_t Color4b(byte r, byte g, byte b, byte a);

/**
 * @return A color with the specified RGBA integer, e.g. `0xdeadbeef`.
 */
static inline color_t Color4bv(uint32_t rgba);

/**
 * @return A color with the specified RGBA floating point values. Note that the resulting color is clamped.
 */
static inline color_t Color4f(float r, float g, float b, float a);

/**
 * @return A color with the specified RGBA vector. Note that the resulting color is clamped.
 */
static inline color_t Color4fv(const vec4_t rgba);

/**
 * @return A color with the specified RGB vector converted from HSV.
 */
static inline color_t ColorHSV(float hue, float saturation, float value);

/**
 * @return A color with the specified RGB vector converted from HSV.
 */
static inline color_t ColorHSVA(float hue, float saturation, float value, float alpha);

/**
 * @return The sum of `a + b`.
 */
static inline color_t Color_Add(const color_t a, const color_t b);

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
static inline color_t Color_Mix(const color_t a, const color_t b, float mix);

/**
 * @return True if the parsing succeeded, false otherwise.
 */
static inline _Bool Color_Parse(const char *s, color_t *color);

/**
 * @return The value of `a * b`.
 */
static inline color_t Color_Multiply(const color_t a, const color_t b);

/**
 * @return The value of `a * b`.
 */
static inline color_t Color_Scale(const color_t a, const float b);

/**
 * @return The difference of `a - b`.
 */
static inline color_t Color_Subtract(const color_t a, const color_t b);

/**
 * @return A hexadecimal string (`rrggbb[aa]`) of the specified color.
 */
static inline const char *Color_Unparse(const color_t color);

/**
 * @return A floating point vector for the specified color;
 */
static inline vec3_t Color_Vec3(const color_t color);

/**
 * @return A floating point vector for the specified color.
 */
static inline vec4_t Color_Vec4(const color_t color);

/**
 * @return A 32-bit integer color for the specified color.
 */
static inline color32_t Color_Color32(const color_t color);

/**
 * @return A color32_t with the specified RGBA components.
 */
static inline color32_t Color32(byte r, byte g, byte b, byte a);

/**
 * @return A floating point color for the specified 32 bit integer color.
 */
static inline color_t Color32_Color(const color32_t c);

#include "color.c"