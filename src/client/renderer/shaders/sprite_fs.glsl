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

#define UNIFORMS_NO_SAMPLERS
#include "uniforms.glsl"

layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_DIFFUSE)      uniform sampler2D texture_diffusemap;
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_NEXT_DIFFUSE) uniform sampler2D texture_next_diffusemap;

layout (location = 0) in vec2 in_diffusemap;
layout (location = 1) in vec2 in_next_diffusemap;
layout (location = 2) in vec3 in_color;
layout (location = 3) in float in_lerp;

layout (location = 0) out vec4 out_color;

/**
 * @brief
 * @remarks TODO(#864): soft particles (depth-attachment fade) are deferred.
 */
void main(void) {

  const vec3 texture_color = mix(
      texture(texture_diffusemap, in_diffusemap).rgb,
      texture(texture_next_diffusemap, in_next_diffusemap).rgb,
      in_lerp);

  out_color = vec4(texture_color * in_color, 1.0);
}
