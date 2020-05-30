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

uniform mat4 view;
uniform vec2 inv_viewport_size;
uniform sampler2D depth_stencil_attachment;

in vertex_data {
	vec3 position;
	float dist_to_ground;
} vertex;

out vec4 out_color;

vec2 depth_range = vec2(1.0, 11586.0);
float transition_size = .0016f;

/**
 * @brief Reverse depth calculation.
 */
float calc_depth(in float z) {
	return (2. * depth_range.x) / (depth_range.y + depth_range.x - z * (depth_range.y - depth_range.x));
}

/**
 * @brief Calculate the soft edge factor for the current fragment.
 */
float soften() {
	return smoothstep(0.0, transition_size, clamp(calc_depth(texture(depth_stencil_attachment, gl_FragCoord.xy * inv_viewport_size).r) - calc_depth(gl_FragCoord.z), 0.0, 1.0));
}

/**
 * @brief
 */
void main(void) {

	vec3 shadow_base = vec3(0, 0, 0);
	float alpha = 1.0 - vertex.dist_to_ground;

	if (alpha <= 0.0) {
		discard;
	}

	vec4 effect = vec4(shadow_base, alpha);

	out_color = effect;

	// postprocessing
	
	//out_color.rgb = tonemap(out_color.rgb);

	out_color.rgb = fog(vertex.position, out_color.rgb);

	out_color.a *= 1.0 - soften();

	//out_color.rgb = color_filter(out_color.rgb);

	//out_color.rgb = dither(out_color.rgb);
}
