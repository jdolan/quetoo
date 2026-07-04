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

layout (location = 0) in vec3 in_position;

/**
 * @brief Per-draw locals.
 */
layout (std140, set = UNIFORM_SET, binding = BINDING_LOCALS) uniform locals_block {
  mat4 model;
};

invariant gl_Position;

// Pushes the pre-pass depth this many world units farther from the camera than
// the true surface, so the main pass's LEQUAL test still accepts its own
// (bit-inexact) coplanar depth. Applied in view space, before the nonlinear
// perspective divide, so the bias is meaningful in world units at any distance
// -- unlike a raw depth-buffer offset (SDL_GPU's native depth bias), whose
// effective world-space size varies wildly (and unusably) with depth under a
// far plane this large. See #864: SDL's Metal backend compiles MSL with
// fast-math on (newLibraryWithSource:options:nil), so `invariant gl_Position`
// is not honored and the pre-pass/main-pass depths differ by a few ULPs.
#define DEPTH_PASS_BIAS 1.0

/**
 * @brief Position-only depth pre-pass: writes only the depth attachment, which
 * the main passes reuse for early-Z.
 */
void main(void) {

  mat4 view_model = view * model;

  vec4 view_position = view_model * vec4(in_position, 1.0);
  view_position.z -= DEPTH_PASS_BIAS; // view space looks down -Z; more negative = farther

  gl_Position = projection3D * view_position;
}
