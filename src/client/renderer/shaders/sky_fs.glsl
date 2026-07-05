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

#include "uniforms.glsl"

// Sky's own binding map (fragment stage): a dense family shared with no other
// pipeline. Sky has no base diffuse texture array, so texture_material (the
// only sampler material.glsl would otherwise declare) is compiled out via
// MATERIAL_NO_DIFFUSE, keeping the family dense at {sky, stage, stage_next}.
#define BINDING_SAMPLER_SKY        0
#define BINDING_SAMPLER_STAGE      1
#define BINDING_SAMPLER_STAGE_NEXT 2
#define BINDING_UNIFORMS_MATERIAL  1

#define MATERIAL_NO_DIFFUSE
#include "common.glsl"
#include "material.glsl"

layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_SKY) uniform samplerCube texture_sky;

layout (location = 0) in vec3 cubemap_coord;
layout (location = 1) in vec4 stage_color;

layout (location = 0) out vec4 out_color;

/**
 * @brief Converts a normalized direction to an azimuthal equidistant
 * projection: center = straight up, edge = horizon. Avoids pole singularities
 * by placing them at the texture edge.
 */
vec2 direction_to_azimuthal_equidistant(in vec3 direction) {

  float theta = acos(clamp(direction.z, -1.0, 1.0)); // angle from +Z (up)
  float phi = atan(direction.y, direction.x); // azimuth

  float r = theta / PI; // radial distance (0 at center, 1 at edge)

  return 0.5 + r * vec2(cos(phi), sin(phi)) * 0.5;
}

/**
 * @brief Applies the stage's rotate/scroll/scale to the azimuthal-projected
 * texcoord. Distinct from material.glsl's stage_vertex, which transforms a
 * per-vertex diffusemap texcoord that sky surfaces don't have.
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
 * @brief Windows into the sky cubemap (material.flags == STAGE_NONE), or draws
 * an azimuthally-projected material stage (moving clouds, moons, ...) layered
 * over it. One shader, one pipeline: which branch runs is a runtime uniform,
 * matching the BSP/mesh material-stage pattern.
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
