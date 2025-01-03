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
#include "swap.h"

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

	/**
	 * @brief Vec3 accessor.
	 */
	vec3_t vec3;

	/**
	 * @brief Vec4 accessor.
	 */
	vec4_t vec4;
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
 * @brief A clamped integer RGB color.
 */
typedef union {

	/**
	 * @brief Component accessors.
	 */
	struct {
		byte r, g, b;
	};

	/**
	 * @brief Array accessor.
	 */
	byte bytes[3];
} color24_t;

/**
 * @return A color with the specified RGBA bytes.
 */
static inline color_t __attribute__ ((warn_unused_result)) Color4b(byte r, byte g, byte b, byte a) {
	return (color_t) {
		.r = r / 255.f,
		.g = g / 255.f,
		.b = b / 255.f,
		.a = a / 255.f
	};
}

/**
 * @return A color with the specified RGB byte values.
 */
static inline color_t __attribute__ ((warn_unused_result)) Color3b(byte r, byte g, byte b) {
	return Color4b(r, g, b, 255);
}

/**
 * @return A color with the specified RGBA integer, e.g. `0xdeadbeef`.
 */
static inline color_t __attribute__ ((warn_unused_result)) Color4bv(uint32_t rgba) {
	const color32_t c = {
		.rgba = LittleLong(rgba)
	};

	return Color4b(c.r, c.g, c.b, c.a);
}

/**
 * @return A color with the specified RGB integer, e.g. `0xbbaadd`.
 */
static inline color_t __attribute__ ((warn_unused_result)) Color3bv(uint32_t rgb) {
	return Color4bv(rgb | 0xff000000);
}

/**
 * @return A color with the specified unclamped RGBA floating point values. Note that the resulting color is clamped.
 */
static inline color_t __attribute__ ((warn_unused_result)) Color4f(float r, float g, float b, float a) {
	return (color_t) {
		.r = Maxf(0.f, r),
		.g = Maxf(0.f, g),
		.b = Maxf(0.f, b),
		.a = Clampf(a, 0.f, 1.f)
	};
}

/**
 * @return A color with the specified RGB floating point values. Note that the resulting color is clamped.
 */
static inline color_t __attribute__ ((warn_unused_result)) Color3f(float r, float g, float b) {
	return Color4f(r, g, b, 1.f);
}

/**
 * @return A color with the specified RGB vector. Note that the resulting color is clamped.
 */
static inline color_t __attribute__ ((warn_unused_result)) Color3fv(const vec3_t rgb) {
	return Color3f(rgb.x, rgb.y, rgb.z);
}

/**
 * @return A color with the specified RGBA vector. Note that the resulting color is clamped.
 */
static inline color_t __attribute__ ((warn_unused_result)) Color4fv(const vec4_t rgba) {
	return Color4f(rgba.x, rgba.y, rgba.z, rgba.w);
}

/**
 * @brief Fills `len` of `out` with RGBA values from the floating point color (like memset).
 */
static inline void Color_Fill(color_t *out, const color_t c, size_t len) {

	for (size_t i = 0; i < len; i++) {
		*out++ = c;
	}
}

/**
 * @return A color with the specified RGB vector converted from HSV.
 */
static inline color_t __attribute__ ((warn_unused_result)) ColorHSV(float hue, float saturation, float value) {

	value = Clampf01(value);

    if (saturation <= 0.0f) {
		return Color3f(value, value, value);
    }

	saturation = Minf(saturation, 1.f);

	hue = ClampEuler(hue) / 60.f;
	color_t color = { .a = 1.f };

	const float h = fabsf(hue);
	const int i = (int) h;
	const float f = h - i;
	const float p = value * (1.0f - saturation);
	const float q = value * (1.0f - (saturation * f));
	const float t = value * (1.0f - (saturation * (1.0f - f)));

	switch (i) {
		case 0:
			color.r = value;
			color.g = t;
			color.b = p;
			break;
		case 1:
			color.r = q;
			color.g = value;
			color.b = p;
			break;
		case 2:
			color.r = p;
			color.g = value;
			color.b = t;
			break;
		case 3:
			color.r = p;
			color.g = q;
			color.b = value;
			break;
		case 4:
			color.r = t;
			color.g = p;
			color.b = value;
			break;
		default:
			color.r = value;
			color.g = p;
			color.b = q;
			break;
	}

	return color;
}

/**
 * @return A color with the specified RGB vector converted from HSV.
 */
static inline color_t __attribute__ ((warn_unused_result)) ColorHSVA(float hue, float saturation, float value, float alpha) {
	color_t color = ColorHSV(hue, saturation, value);
	color.a = Clampf01(alpha);
	return color;
}

/**
 * @return An HSV vector of the specified color.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Color_HSV(const color_t color) {

	const float cmax = Maxf(Maxf(color.r, color.g), color.b);
	const float cmin = Minf(Minf(color.r, color.g), color.b);

	const float delta = cmax - cmin;

	float h;
	if (cmax == 0.f && cmin == 0.f) {
		h = 0;
	} else if (cmax == color.r) {
		h = (60.f * (color.g - color.b) / delta + 360.f);
	} else if (cmax == color.g) {
		h = (60.f * (color.b - color.r) / delta + 120.f);
	} else {
		h = (60.f * (color.r - color.g) / delta + 240.f);
	}

	float s;
	if (cmax == 0.f) {
		s = 0.f;
	} else {
		s = (delta / cmax) * 100.f;
	}

	const float v = cmax * 100.f;

	return Vec3((int32_t) h % 360, s, v);
}

/**
 * @return An HSV vector of the specified color.
 */
static inline vec4_t __attribute__ ((warn_unused_result)) Color_HSVA(const color_t c) {
	return Vec3_ToVec4(Color_HSV(c), c.a);
}

/**
 * @return The sum of `a + b`.
 */
static inline color_t __attribute__ ((warn_unused_result)) Color_Add(const color_t a, const color_t b) {
	return Color4fv(Vec4_Add(a.vec4, b.vec4));
}

/**
 * @return The difference of `a - b`.
 */
static inline color_t __attribute__ ((warn_unused_result)) Color_Subtract(const color_t a, const color_t b) {
	return Color4fv(Vec4_Subtract(a.vec4, b.vec4));
}

/**
 * @return The value of `a * b`.
 */
static inline color_t __attribute__ ((warn_unused_result)) Color_Multiply(const color_t a, const color_t b) {
	return Color4fv(Vec4_Multiply(a.vec4, b.vec4));
}

/**
 * @return The color `c` normalized to the range `0.f - 1.f`. Hue is preserved.
 */
static inline color_t __attribute__ ((warn_unused_result)) Color_Normalize(const color_t c) {
	const float max = Vec3_Hmaxf(c.vec3);
	if (max > 1.f) {
		return Color4f(c.r / max, c.g / max, c.b / max, c.a);
	} else {
		return c;
	}
}

/**
 * @return The value of `a * b`.
 */
static inline color_t __attribute__ ((warn_unused_result)) Color_Scale(const color_t a, const float b) {
	return Color4fv(Vec4_Scale(a.vec4, b));
}

/**
 * @return The linear interpolation of `a` and `b` using the specified fraction.
 */
static inline color_t __attribute__ ((warn_unused_result)) Color_Mix(const color_t a, const color_t b, float mix) {
	return Color4fv(Vec4_Mix(a.vec4, b.vec4, mix));
}

/**
 * @return True if the parsing succeeded, false otherwise.
 */
static inline bool __attribute__ ((warn_unused_result)) Color_Parse(const char *s, color_t *color) {

	const size_t length = strlen(s);
	if (length != 6 && length != 8) {
		return false;
	}

	char buffer[9];
	g_strlcpy(buffer, s, sizeof(buffer));

	if (length == 6) {
		g_strlcat(buffer, "ff", sizeof(buffer));
	}

	uint32_t rgba;
	if (sscanf(buffer, "%x", &rgba) != 1) {
		return false;
	}

	*color = Color4bv(BigLong(rgba));
	return true;
}

/**
 * @return A `color32_t` with the specified RGBA components.
 */
static inline color32_t __attribute__ ((warn_unused_result)) Color32(byte r, byte g, byte b, byte a) {
	return (color32_t) {
		.r = r,
		.b = b,
		.g = g,
		.a = a
	};
}

/**
 * @return A `color32_t` with the specified RGBA integer.
 */
static inline color32_t __attribute__ ((warn_unused_result)) Color32i(int32_t rgba) {
	return (color32_t) {
		.rgba = rgba
	};
}

/**
 * @return A clamped 32-bit color for the specified floating point color.
 */
static inline color32_t __attribute__ ((warn_unused_result)) Color_Color32(const color_t c) {

	vec3_t rgb = Vec3(c.r, c.g, c.b);
	const float max = Vec3_Hmaxf(rgb);
	if (max > 1.f) {
		rgb = Vec3_Scale(rgb, 1.f / max);
	}
	
	return Color32(Clampf(rgb.x, 0.f, 1.f) * 255.f,
				   Clampf(rgb.y, 0.f, 1.f) * 255.f,
				   Clampf(rgb.z, 0.f, 1.f) * 255.f,
				   Clampf(c.a, 0.f, 1.f) * 255.f);
}

/**
 * @return A `color24_t` with the specified RGB components.
 */
static inline color24_t __attribute__ ((warn_unused_result)) Color24(byte r, byte g, byte b) {
	return (color24_t) {
		.r = r,
		.g = g,
		.b = b
	};
}

/**
 * @return A `color24_t` with the specified RGB integer.
 */
static inline color24_t __attribute__ ((warn_unused_result)) Color24i(int32_t rgb) {
	union {
		int32_t rgb;
		color24_t c24;
	} out;

	out.rgb = rgb;
	return out.c24;
}

/**
 * @return A 24-bit color for the specified floating point color.
 */
static inline color24_t __attribute__ ((warn_unused_result)) Color_Color24(const color_t c) {

	vec3_t rgb = Vec3(c.r, c.g, c.b);
	const float max = Vec3_Hmaxf(rgb);
	if (max > 1.f) {
		rgb = Vec3_Scale(rgb, 1.f / max);
	}

	return Color24(Clampf(rgb.x, 0.f, 1.f) * 255.f,
				   Clampf(rgb.y, 0.f, 1.f) * 255.f,
				   Clampf(rgb.z, 0.f, 1.f) * 255.f);
}

/**
 * @return A hexadecimal string (`rrggbb[aa]`) of the specified color.
 */
static inline const char * __attribute__ ((warn_unused_result)) Color_Unparse(const color_t color) {
	const color32_t c = Color_Color32(color);

	static char buffer[12];
	g_snprintf(buffer, sizeof(buffer), "%02x%02x%02x%02x", c.r, c.g, c.b, c.a);

	return buffer;
}

/**
 * @return A floating point color for the specified 32 bit integer color.
 */
static inline color_t __attribute__ ((warn_unused_result)) Color32_Color(const color32_t c) {
	return Color4bv(c.rgba);
}

/**
 * @return A normalized (0.0 - 1.0) `vec3_t` for the specified 32 bit integer color.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Color32_Vec3(const color32_t c) {
	return Color4bv(c.rgba).vec3;
}

/**
 * @return A normalized (0.0 - 1.0) `vec4_t` for the specified 32 bit integer color.
 */
static inline vec4_t __attribute__ ((warn_unused_result)) Color32_Vec4(const color32_t c) {
	return Color4bv(c.rgba).vec4;
}

/**
 * @brief Fills `len` of `out` with RGBA values from the 32 bit color (like memset).
 */
static inline void Color32_Fill(color32_t *out, const color32_t c, size_t len) {
	for (size_t i = 0; i < len; i++, out++) {
		*out = c;
	}
}

/**
 * @return A 24 bit color for the specified 32 bit color (RGB swizzle).
 */
static inline color24_t __attribute__ ((warn_unused_result)) Color32_Color24(const color32_t c) {
	return Color24(c.r, c.g, c.b);
}

/**
 * @return The decoded directional vector from this `color32_t`'s RGB components.
 */
static inline vec3_t __attribute__ ((warn_unused_result)) Color32_Direction(const color32_t c) {
	return Vec3_Normalize(Vec3_Subtract(Vec3_Scale(Color32_Color(c).vec3, 2.f), Vec3_One()));
}

/**
 * @rretur A floating point color for the specified 24 bit integer color.
 */
static inline color_t Color24_Color(const color24_t c) {
	return Color3f(c.r / 255.f, 
				   c.g / 255.f,
				   c.b / 255.f);
}

/**
 * @brief Fills `len` of `out` with the RGB values from the 24 bit color (like memset).
 */
static inline void Color24_Fill(color24_t *out, const color24_t c, size_t len) {
	for (size_t i = 0; i < len; i++, out++) {
		*out = c;
	}
}
