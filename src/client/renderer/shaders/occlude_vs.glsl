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

// The constant unit-cube corner (VERTEX rate, shared by every instance): one
// of the 8 corners in {0, 1}^3, ordered to match Box3_ToPoints (bit 0 = X,
// bit 1 = Y, bit 2 = Z; unset = mins, set = maxs).
layout (location = 0) in vec3 in_corner;

// The per-instance box bounds (INSTANCE rate), already in world space.
layout (location = 1) in vec3 in_mins;
layout (location = 2) in vec3 in_maxs;

invariant gl_Position;

/**
 * @brief Draws an occlusion query box as an instance of the shared unit cube,
 * scaled and offset to the instance's world-space bounds. This lets a single
 * query draw an arbitrary number of boxes (e.g. one per voxel a light
 * touches) in a single instanced draw call, without baking per-query vertex
 * data: only a compact (mins, maxs) pair is needed per box, and the boxes are
 * already in world space, so no model matrix is required here.
 */
void main(void) {

  const vec3 position = mix(in_mins, in_maxs, in_corner);

  gl_Position = projection3D * view * vec4(position, 1.0);
}
