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

#version 450

/**
 * @file sky_fs.glsl
 * @brief Samples the sky cubemap or an azimuthal sky stage.
 */

#include "uniforms.glsl"

#define BINDING_UNIFORMS_MATERIAL  1

#include "common.glsl"
#include "material.glsl"

layout (location = 0) in vec3 cubemap_coord;
layout (location = 1) in vec4 stage_color;

layout (location = 0) out vec4 out_color;

/**
 * @brief Projects a direction into azimuthal equidistant UV space.
 */
vec2 direction_to_azimuthal_equidistant(in vec3 direction) {

  float theta = acos(clamp(direction.z, -1.0, 1.0));
  float phi = atan(direction.y, direction.x);

  float r = theta / PI;

  return 0.5 + r * vec2(cos(phi), sin(phi)) * 0.5;
}

/**
 * @brief Applies the material stage transform to sky UVs.
 */
vec2 transform_stage_uv(in vec2 uv) {

  if ((material.flags & STAGE_ROTATE) == STAGE_ROTATE) {
    vec2 center = uv - 0.5;
    float theta = ticks * 0.001 * material.rotate * TWO_PI;
    center = mat2(cos(theta), -sin(theta), sin(theta), cos(theta)) * center;
    uv = center + 0.5;
  }

  if ((material.flags & STAGE_SCROLL_S) == STAGE_SCROLL_S) {
    uv.s += material.scroll.s * ticks * 0.001;
  }

  if ((material.flags & STAGE_SCROLL_T) == STAGE_SCROLL_T) {
    uv.t += material.scroll.t * ticks * 0.001;
  }

  if ((material.flags & (STAGE_SCALE_S | STAGE_SCALE_T)) != 0) {
    vec2 center = uv - 0.5;
    center /= vec2(
      (material.flags & STAGE_SCALE_S) == STAGE_SCALE_S ? material.scale.s : 1.0,
      (material.flags & STAGE_SCALE_T) == STAGE_SCALE_T ? material.scale.t : 1.0
    );
    uv = center + 0.5;
  }

  return uv;
}

/**
 * @brief Shades sky surfaces from the cubemap or the active material stage.
 */
void main(void) {

  if (material.flags == STAGE_NONE) {

    out_color = texture(texture_sky, normalize(cubemap_coord));

  } else {

    vec2 st = direction_to_azimuthal_equidistant(normalize(cubemap_coord));
    st = transform_stage_uv(st);

    out_color = sample_material_stage(st) * stage_color;
  }
}
