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

layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out;

uniform int shadow_index;
uniform mat4 shadow_view[6];
uniform mat4 shadow_projection;

out vec4 position;

void main() {

	light_t light = lights[shadow_index];

	int type = int(light.position.w);

	if (type == LIGHT_AMBIENT || type == LIGHT_SUN) {
		gl_Layer = shadow_index;

		for (int j = 0; j < 3; j++) {

			position = shadow_view[0] * gl_in[j].gl_Position;

			gl_Position = shadow_projection * position;

			EmitVertex();
		}

		EndPrimitive();
	} else {
		for (int i = 0; i < 6; i++) {
			gl_Layer = shadow_index * 6 + i;

			for (int j = 0; j < 3; j++) {

				position = shadow_view[i] * gl_in[j].gl_Position;

				gl_Position = shadow_projection * position;

				EmitVertex();
			}

			EndPrimitive();
		}
	}
}
