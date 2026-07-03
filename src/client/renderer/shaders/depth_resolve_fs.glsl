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

layout (location = 0) in vertex_data {
  vec2 texcoord;
} vertex;

layout (set = 2, binding = 0) uniform sampler2DMS texture_depth_ms;

/**
 * @brief Resolves the multisampled scene depth to single-sample by reading sample 0
 * into gl_FragDepth (SDL_gpu has no depth store-op resolve). Sample 0 is sufficient
 * for soft-particle depth comparisons.
 */
void main(void) {
  gl_FragDepth = texelFetch(texture_depth_ms, ivec2(gl_FragCoord.xy), 0).r;
}
