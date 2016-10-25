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

	R_EnableColorArray(true);

	R_ResetArrayState();

	R_BlendFunc(GL_SRC_ALPHA, GL_ONE);

	R_UseProgram(r_state.corona_program);

	uint16_t num_verts = 0;
	uint16_t num_elements = 0;

	const vec2_t texcoords[] = {
		{ 0.0, 0.0 },
		{ 1.0, 0.0 },
		{ 1.0, 1.0 },
		{ 0.0, 1.0 }
	};

	for (uint16_t i = 0; i < r_view.num_coronas; i++) {

		const r_corona_t *c = &r_view.coronas[i];
		const vec_t f = c->radius + (c->radius * c->flicker * sin(0.09 * r_view.time));

		// copy texcoords
		memcpy(&texunit_diffuse.texcoord_array[num_verts * 2], texcoords, sizeof(texcoords));

		// set colors
		memcpy(&r_state.color_array[num_verts * 4], c->color, sizeof(vec3_t));
		r_state.color_array[(num_verts * 4) + 3] = r_coronas->value; // set origin color

		// and the corner colors
		for (uint16_t j = 4; j < 16; j++) {

			r_state.color_array[(num_verts * 4) + j] = r_state.color_array[(num_verts * 4) + (j % 4)];
		}

		vec3_t v;

		// top-left
		VectorMA(c->origin, -f, r_view.right, v);
		VectorMA(v, -f, r_view.up, &r_state.vertex_array[(num_verts * 3) + 0]);
		
		// top-right
		VectorMA(c->origin, f, r_view.right, v);
		VectorMA(v, -f, r_view.up, &r_state.vertex_array[(num_verts * 3) + 3]);
		
		// bottom-right
		VectorMA(c->origin, f, r_view.right, v);
		VectorMA(v, f, r_view.up, &r_state.vertex_array[(num_verts * 3) + 6]);

		// bottom-left
		VectorMA(c->origin, -f, r_view.right, v);
		VectorMA(v, f, r_view.up, &r_state.vertex_array[(num_verts * 3) + 9]);

		// indices
		r_state.indice_array[num_elements + 0] = num_verts + 0;
		r_state.indice_array[num_elements + 1] = num_verts + 1;
		r_state.indice_array[num_elements + 2] = num_verts + 2;

		r_state.indice_array[num_elements + 3] = num_verts + 0;
		r_state.indice_array[num_elements + 4] = num_verts + 2;
		r_state.indice_array[num_elements + 5] = num_verts + 3;

		num_verts += 4;
		num_elements += 6;
	}

	R_UploadToBuffer(&r_state.buffer_vertex_array, 0, num_verts * sizeof(vec3_t), r_state.vertex_array);
	R_UploadToBuffer(&r_state.buffer_color_array, 0, num_verts * sizeof(vec4_t), r_state.color_array);
	R_UploadToBuffer(&texunit_diffuse.buffer_texcoord_array, 0, num_verts * sizeof(vec2_t), texunit_diffuse.texcoord_array);
	R_UploadToBuffer(&r_state.buffer_element_array, 0, num_elements * sizeof(GLuint), r_state.indice_array);

	R_BindArray(R_ARRAY_ELEMENTS, &r_state.buffer_element_array);

	R_DrawArrays(GL_TRIANGLES, 0, num_elements);

	R_BindArray(R_ARRAY_ELEMENTS, NULL);
	
	R_EnableColorArray(false);

	R_UseProgram(r_state.null_program);

	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
