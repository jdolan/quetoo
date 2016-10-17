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

#include "r_local.h"

/**
 * @brief
 */
void R_AddCorona(const r_corona_t *c) {

	if (!r_coronas->value)
		return;

	if (r_view.num_coronas >= MAX_CORONAS)
		return;

	if (c->radius < 1.0)
		return;

	r_view.coronas[r_view.num_coronas++] = *c;
}

/**
 * @brief
 */
void R_DrawCoronas(void) {

	if (!r_coronas->value)
		return;

	if (!r_view.num_coronas)
		return;

	R_EnableTexture(&texunit_diffuse, false);

	R_EnableColorArray(true);

	R_ResetArrayState();

	R_BlendFunc(GL_SRC_ALPHA, GL_ONE);

	for (uint16_t i = 0; i < r_view.num_coronas; i++) {

		const r_corona_t *c = &r_view.coronas[i];
		const vec_t f = c->radius * c->flicker * sin(0.09 * r_view.time);

		// use at least 12 verts, more for larger coronas
		const uint16_t num_verts = Clamp(c->radius / 8.0, 12, 64);

		memcpy(&r_state.color_array[0], c->color, sizeof(vec3_t));
		r_state.color_array[3] = r_coronas->value; // set origin color

		// and the corner colors
		memset(&r_state.color_array[4], 0, num_verts * 2 * sizeof(vec4_t));

		memcpy(&r_state.vertex_array[0], c->origin, sizeof(vec3_t));
		uint32_t vert_index = 3; // and the origin

		for (uint16_t j = 0; j <= num_verts; j++) { // now draw the corners
			const vec_t a = j / (vec_t) num_verts * M_PI * 2;

			vec3_t v;
			VectorCopy(c->origin, v);

			VectorMA(v, cos(a) * (c->radius + f), r_view.right, v);
			VectorMA(v, sin(a) * (c->radius + f), r_view.up, v);

			memcpy(&r_state.vertex_array[vert_index], v, sizeof(vec3_t));
			vert_index += 3;
		}

		R_UploadToBuffer(&r_state.buffer_vertex_array, 0, vert_index * sizeof(float), r_state.vertex_array);
		R_UploadToBuffer(&r_state.buffer_color_array, 0, num_verts * sizeof(vec4_t), r_state.color_array);

		R_DrawArrays(GL_TRIANGLE_FAN, 0, vert_index / 3);
	}

	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_EnableColorArray(false);

	R_EnableTexture(&texunit_diffuse, true);

	R_Color(NULL);
}
