/*
* Copyright(c) 1997-2001 Id Software, Inc.
* Copyright(c) 2002 The Quakeforge Project.
* Copyright(c) 2006 Quake2World.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or(at your option) any later version.
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

#include "renderer.h"


/*
R_AddCorona
*/
void R_AddCorona(const vec3_t org, float radius, const vec3_t color){
	corona_t *c;

	if(!r_coronas->value)
		return;

	if(r_view.num_coronas == MAX_CORONAS)
		return;

	c = &r_view.coronas[r_view.num_coronas++];

	VectorCopy(org, c->org);
	c->radius = radius;
	VectorCopy(color, c->color);
}


/*
R_DrawCoronas
*/
void R_DrawCoronas(void){
	int i, j, k;
	vec3_t v;

	if(!r_coronas->value)
		return;

	if(!r_view.num_coronas)
		return;

	R_EnableTexture(&texunit_diffuse, false);

	R_EnableColorArray(true);

	R_ResetArrayState();

	R_BlendFunc(GL_ONE, GL_ONE);

	for(k = 0; k < r_view.num_coronas; k++){

		const corona_t *c = &r_view.coronas[k];
		int verts, vertind;

		if(!c->radius)
			continue;

		// use at least 12 verts, more for larger coronas
		verts = 12 + c->radius / 8;

		memcpy(&r_state.color_array[0], c->color, sizeof(vec3_t));
		r_state.color_array[3] = 1.0f;  // set origin color

		// and the corner colors
		memset(&r_state.color_array[4], 0, verts * 2 * sizeof(vec4_t));

		memcpy(&r_state.vertex_array_3d[0], c->org, sizeof(vec3_t));
		vertind = 3;  // and the origin

		for(i = verts; i >= 0; i--){  // now draw the corners
			const float a = i / (float)verts * M_PI * 2;

			for(j = 0; j < 3; j++)
				v[j] = c->org[j] + r_locals.right[j] * (float)cos(a) * c->radius
					+ r_locals.up[j] * (float)sin(a) * c->radius;

			memcpy(&r_state.vertex_array_3d[vertind], v, sizeof(vec3_t));
			vertind += 3;
		}

		glDrawArrays(GL_TRIANGLE_FAN, 0, vertind / 3);
	}

	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_EnableColorArray(false);

	R_EnableTexture(&texunit_diffuse, true);

	glColor4ubv(color_white);
}
