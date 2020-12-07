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

uniform sampler3D texture_lightgrid_fog;

in vertex_data {
	vec3 position;
	vec3 lightgrid;
} vertex;

out vec4 out_color;

/**
 * @brief
 */
void main(void) {

	float alpha = (0.6 - soften());

	if (alpha <= 0.0) {
		discard;
	}

	out_color = vec4(0.0, 0.0, 0.0, alpha);
	
	lightgrid_fog(out_color, texture_lightgrid_fog, vertex.position, vertex.lightgrid);
}

