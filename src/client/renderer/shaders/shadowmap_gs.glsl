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
layout (triangle_strip, max_vertices=18) out;

uniform mat4 cubemap_view[6];
uniform mat4 cubemap_projection;

out vec4 frag_pos;

void main() {

    for (int i = 0; i < 6; i++) {

		gl_Layer = i;

        for (int j = 0; j < 3; j++) {

			frag_pos = gl_in[j].gl_Position;

            gl_Position = cubemap_projection * cubemap_view[i] * frag_pos;

            EmitVertex();
        }

        EndPrimitive();
    }
}  
