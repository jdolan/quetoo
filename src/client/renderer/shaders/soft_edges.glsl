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

uniform bool soft_particles;
uniform vec2 inv_viewport_size;
uniform vec2 depth_range;
uniform float transition_size;
uniform sampler2D depth_attachment;

/**
 * @brief Reverse depth calculation
 */
float calc_depth(in float z) {
	return (2. * depth_range.x) / (depth_range.y + depth_range.x - z * (depth_range.y - depth_range.x));
}

/**
 * @brief Calculate the soft edge factor for the specified fragment.
 */
float soft_edges() {

	if (!soft_particles) {
		return 1.0;
	}

	return smoothstep(0.0, transition_size, clamp(calc_depth(texture(depth_attachment, gl_FragCoord.xy * inv_viewport_size).r) - calc_depth(gl_FragCoord.z), 0.0, 1.0));
}
