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
 * @brief The light_t type and its minimal helpers, with no dependency on
 * common.glsl/material.glsl/voxel.glsl. Shared by light.glsl (bsp/mesh's full
 * shadowed/dynamic lighting) as well as lightweight consumers that only need
 * light_t itself -- decal_fs.glsl (clustered voxel BSP light only, no shadows
 * or dynamic tail) and the sprite program (distance-only attenuation, no
 * Lambert term) -- without pulling in light.glsl's heavier dependencies.
 * @remarks Include after uniforms.glsl.
 */

#define SHADOW_ATLAS_LIGHTS_PER_ROW 32

/**
 * @brief The light struct, mirroring the C `r_light_uniform_t`.
 * @remarks This struct is vec4 aligned.
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
   * @brief This light's shadow atlas tile origin, in pixels, or (-1, -1) if
   * this light casts no shadow. The tile is square and the same in all six
   * face textures; its size is derived from
   * textureSize() / SHADOW_ATLAS_LIGHTS_PER_ROW rather than carried here.
   */
  vec2 shadow;
};

/**
 * @brief Returns the modulated, intensity-scaled, and saturated color for a light.
 * @param l The light.
 * @return The color scaled by intensity, modulate, and saturation.
 */
vec3 light_color(in light_t l) {
  vec3 color = l.color.rgb * l.color.a * modulate;
  float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
  return mix(vec3(luma), color, saturation);
}

/**
 * @brief Tests whether dynamic light `j` is flagged active in `mask`.
 * @param mask A per-draw active-dynamic-lights bitmask (e.g. `bsp_locals_block`'s
 * `active_dynamic_lights`): 256 bits packed as uvec4[2], since std140 forbids a
 * tightly-packed uint[] array (each scalar array element would still consume a
 * full 16-byte slot, wasting 4x the space/bandwidth of the equivalent uvec4[]).
 * @param j The dynamic light's index into the `dynamic_lights` storage buffer.
 * @return true if dynamic light `j` is flagged active, false otherwise.
 */
bool dynamic_light_active(in uvec4 mask[MAX_DYNAMIC_LIGHTS / 128], in int j) {
  return (mask[j >> 7][(j >> 5) & 3] & (1u << (j & 31))) != 0u;
}
