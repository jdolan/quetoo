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

uniform vec2 camera_range;
uniform vec2 inv_viewport_size;
uniform float transition_size;

uniform bool soft_particles;

uniform sampler2D texture_diffuse;
uniform sampler2D depth_attachment;

in vertex_data {
	vec3 position;
	vec4 color;
} vertex;

out vec4 out_color;

/**
 * @brief
 */
float calc_depth(in float z) {
	return (2. * camera_range.x) / (camera_range.y + camera_range.x - z * (camera_range.y - camera_range.x));
}

/**
 * @brief
 */
void main(void) {

	out_color = vertex.color * texture(texture_diffuse, gl_PointCoord);

	out_color.rgb = fog(vertex.position, out_color.rgb);

	out_color = ColorFilter(out_color);

	if (soft_particles) {
		vec2 coords = gl_FragCoord.xy * inv_viewport_size;
		float geometryZ = calc_depth(texture(depth_attachment, coords).r);
		float sceneZ = calc_depth(gl_FragCoord.z);
		float a = clamp(geometryZ - sceneZ, 0.0, 1.0);
		float b = smoothstep(0.0, transition_size, a);

		out_color.a *= b;
	}
}

