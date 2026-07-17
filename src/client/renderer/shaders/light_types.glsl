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
 * @file light_types.glsl
 * @brief Declares light_t, shared light helpers, and the BSP and dynamic light storage buffers.
 * @remarks Define BINDING_STORAGE_BSP_LIGHTS and BINDING_STORAGE_DYNAMIC_LIGHTS before including this file.
 */

#define SHADOW_ATLAS_LIGHTS_PER_ROW 32

/**
 * @brief Mirrors the C r_light_uniform_t light record.
 */
struct light_t {
  /**
   * @brief The light origin in model space (xyz) and radius (w).
   */
  vec4 origin;

  /**
   * @brief The light color (xyz) and intensity (w).
   */
  vec4 color;

  /**
   * @brief Stores the shadow atlas tile origin in pixels, or (-1, -1) if the light has no shadow.
   */
  vec2 shadow;
};

/**
 * @brief Returns a light color scaled by intensity, modulate, and saturation.
 */
vec3 light_color(in light_t l) {
  vec3 color = l.color.rgb * l.color.a * modulate;
  float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
  return mix(vec3(luma), color, saturation);
}

/**
 * @brief Tests whether dynamic light j is enabled in the per-draw bitmask.
 */
bool dynamic_light_active(in uvec4 mask[MAX_DYNAMIC_LIGHTS / 128], in int j) {
  return (mask[j >> 7][(j >> 5) & 3] & (1u << (j & 31))) != 0u;
}

/**
 * @brief Declares the clustered static BSP light buffer.
 * @remarks A program opts in by defining BINDING_STORAGE_BSP_LIGHTS before
 * including this file.
 */
layout (std430, set = SAMPLER_SET, binding = BINDING_STORAGE_BSP_LIGHTS) readonly buffer bsp_lights_block {
  int num_bsp_lights;
  light_t bsp_lights[];
};

/**
 * @brief Declares the per-draw dynamic light buffer.
 * @remarks A program opts in by defining BINDING_STORAGE_DYNAMIC_LIGHTS before
 * including this file.
 */
layout (std430, set = SAMPLER_SET, binding = BINDING_STORAGE_DYNAMIC_LIGHTS) readonly buffer dynamic_lights_block {
  int num_dynamic_lights;
  light_t dynamic_lights[];
};
