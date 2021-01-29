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

uniform sampler2D texture_depth_stencil_attachment;

#define TRANSITION_SIZE .0016

/**
 * @brief Reverse depth calculation.
 */
float calc_depth(in float z) {
	return (2. * depth_range.x) / (depth_range.y + depth_range.x - z * (depth_range.y - depth_range.x));
}

/**
 * @brief Calculate the soft edge factor for the current fragment.
 */
float soften(in float scalar) {

	if (scalar == 0) {
		return 1.f;
	}

	vec4 depth_sample = texture(texture_depth_stencil_attachment, gl_FragCoord.xy / viewport.zw);
	float softness = smoothstep(0.0, TRANSITION_SIZE * abs(scalar), clamp(calc_depth(depth_sample.r) - calc_depth(gl_FragCoord.z), 0.0, 1.0));

	if (scalar < 0) {
		softness = 1.0 - softness;
	}

	return softness;
}
