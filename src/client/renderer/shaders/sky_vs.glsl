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
 * @file sky_vs.glsl
 * @brief Outputs sky cubemap coordinates and stage color.
 */

#include "uniforms.glsl"

#define BINDING_UNIFORMS_MATERIAL 1

#include "common.glsl"
#include "material.glsl"

layout (location = 0) in vec3 in_position;

layout (location = 0) out vec3 cubemap_coord;
layout (location = 1) out vec4 stage_color;

invariant gl_Position;

/**
 * @brief Transforms sky vertices and prepares the stage color.
 */
void main(void) {

  vec4 position = vec4(in_position, 1.0);

  cubemap_coord = vec3(sky_projection * position);

  stage_color = vec4(1.0);
  if ((material.flags & STAGE_COLOR) == STAGE_COLOR) {
    stage_color = material.color;
  }
  if ((material.flags & STAGE_PULSE) == STAGE_PULSE) {
    stage_color.a *= (sin((ticks * .001 + material.drift) * material.pulse * PI) + 1.0) * .5;
  }

  gl_Position = projection3D * view * position;
}
