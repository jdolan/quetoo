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

#define SKY_DISTANCE (MAX_WORLD_COORD * 2)

#define SKY_ST_MIN	(128) // offset to prevent ST wrapping
#define SKY_ST_MAX	(UINT16_MAX - SKY_ST_MIN)

typedef struct {
	vec3_t position;
	u16vec2_t texcoord;
} r_sky_interleave_vertex_t;

static r_buffer_layout_t r_sky_layout_buffer[] = {
	{ .attribute = R_ARRAY_POSITION, .type = R_ATTRIB_FLOAT, .count = 3, .size = sizeof(vec3_t) },
	{ .attribute = R_ARRAY_DIFFUSE, .type = R_ATTRIB_UNSIGNED_SHORT, .count = 2, .size = sizeof(u16vec2_t), .offset = offsetof(r_sky_interleave_vertex_t, texcoord), .normalized = true },
	{ .attribute = -1 }
};

// sky structure
typedef struct {
	r_image_t *images[6];
	r_buffer_t quick_vert_buffer;
	r_buffer_t quick_elem_buffer;
} r_sky_t;

static r_sky_t r_sky;

/**
 * @brief
 */
void R_DrawSkyBox(void) {
	matrix4x4_t modelview;

	R_GetMatrix(R_MATRIX_MODELVIEW, &modelview);
	
	R_UnbindAttributeBuffers();

	R_BindAttributeInterleaveBuffer(&r_sky.quick_vert_buffer, R_ARRAY_MASK_ALL);

	R_BindAttributeBuffer(R_ARRAY_ELEMENTS, &r_sky.quick_elem_buffer);

	R_PushMatrix(R_MATRIX_MODELVIEW);

	Matrix4x4_ConcatTranslate(&modelview, r_view.origin[0], r_view.origin[1], r_view.origin[2]);

	R_SetMatrix(R_MATRIX_MODELVIEW, &modelview);

	R_UseProgram(program_null);

	R_EnableFog(true);

	r_state.active_fog_parameters.end = FOG_END * 8.0;
	r_state.active_program->UseFog(&r_state.active_fog_parameters);
	
	R_BindDiffuseTexture(r_sky.images[4]->texnum);
	R_DrawArrays(GL_TRIANGLES, 0, 6);

	R_BindDiffuseTexture(r_sky.images[5]->texnum);
	R_DrawArrays(GL_TRIANGLES, 6, 6);

	R_BindDiffuseTexture(r_sky.images[0]->texnum);
	R_DrawArrays(GL_TRIANGLES, 12, 6);

	R_BindDiffuseTexture(r_sky.images[2]->texnum);
	R_DrawArrays(GL_TRIANGLES, 18, 6);

	R_BindDiffuseTexture(r_sky.images[1]->texnum);
	R_DrawArrays(GL_TRIANGLES, 24, 6);

	R_BindDiffuseTexture(r_sky.images[3]->texnum);
	R_DrawArrays(GL_TRIANGLES, 30, 6);

	r_state.active_fog_parameters.end = FOG_END;
	r_state.active_program->UseFog(&r_state.active_fog_parameters);

	R_EnableFog(false);

	R_PopMatrix(R_MATRIX_MODELVIEW);

	R_UnbindAttributeBuffers();
}

/**
 * @brief
 */
void R_InitSky(void) {
	const r_sky_interleave_vertex_t quick_sky_verts[] = {
		// +z (top)
		{ .position = { -SKY_DISTANCE, -SKY_DISTANCE, SKY_DISTANCE },
		  .texcoord = { SKY_ST_MAX, SKY_ST_MIN } },
		{ .position = { SKY_DISTANCE, -SKY_DISTANCE, SKY_DISTANCE },
		  .texcoord = { SKY_ST_MAX, SKY_ST_MAX } },
		{ .position = { SKY_DISTANCE, SKY_DISTANCE, SKY_DISTANCE },
		  .texcoord = { SKY_ST_MIN, SKY_ST_MAX } },
		{ .position = { -SKY_DISTANCE, SKY_DISTANCE, SKY_DISTANCE },
		  .texcoord = { SKY_ST_MIN, SKY_ST_MIN } },
		
		// -z (bottom)
		{ .position = { -SKY_DISTANCE, SKY_DISTANCE, -SKY_DISTANCE },
		  .texcoord = { SKY_ST_MIN, SKY_ST_MAX } },
		{ .position = { SKY_DISTANCE, SKY_DISTANCE, -SKY_DISTANCE },
		  .texcoord = { SKY_ST_MIN, SKY_ST_MIN } },
		{ .position = { SKY_DISTANCE, -SKY_DISTANCE, -SKY_DISTANCE },
		  .texcoord = { SKY_ST_MAX, SKY_ST_MIN } },
		{ .position = { -SKY_DISTANCE, -SKY_DISTANCE, -SKY_DISTANCE },
		  .texcoord = { SKY_ST_MAX, SKY_ST_MAX } },
		
		// +x (right)
		{ .position = { SKY_DISTANCE, -SKY_DISTANCE, SKY_DISTANCE },
		  .texcoord = { SKY_ST_MAX, SKY_ST_MIN } },
		{ .position = { SKY_DISTANCE, -SKY_DISTANCE, -SKY_DISTANCE },
		  .texcoord = { SKY_ST_MAX, SKY_ST_MAX } },
		{ .position = { SKY_DISTANCE, SKY_DISTANCE, -SKY_DISTANCE },
		  .texcoord = { SKY_ST_MIN, SKY_ST_MAX } },
		{ .position = { SKY_DISTANCE, SKY_DISTANCE, SKY_DISTANCE },
		  .texcoord = { SKY_ST_MIN, SKY_ST_MIN } },
		
		// -x (left)
		{ .position = { -SKY_DISTANCE, SKY_DISTANCE, SKY_DISTANCE },
		  .texcoord = { SKY_ST_MAX, SKY_ST_MIN } },
		{ .position = { -SKY_DISTANCE, SKY_DISTANCE, -SKY_DISTANCE },
		  .texcoord = { SKY_ST_MAX, SKY_ST_MAX } },
		{ .position = { -SKY_DISTANCE, -SKY_DISTANCE, -SKY_DISTANCE },
		  .texcoord = { SKY_ST_MIN, SKY_ST_MAX } },
		{ .position = { -SKY_DISTANCE, -SKY_DISTANCE, SKY_DISTANCE },
		  .texcoord = { SKY_ST_MIN, SKY_ST_MIN } },
		
		// +y (back)
		{ .position = { SKY_DISTANCE, SKY_DISTANCE, SKY_DISTANCE },
		  .texcoord = { SKY_ST_MAX, SKY_ST_MIN } },
		{ .position = { SKY_DISTANCE, SKY_DISTANCE, -SKY_DISTANCE },
		  .texcoord = { SKY_ST_MAX, SKY_ST_MAX } },
		{ .position = { -SKY_DISTANCE, SKY_DISTANCE, -SKY_DISTANCE },
		  .texcoord = { SKY_ST_MIN, SKY_ST_MAX } },
		{ .position = { -SKY_DISTANCE, SKY_DISTANCE, SKY_DISTANCE },
		  .texcoord = { SKY_ST_MIN, SKY_ST_MIN } },
		
		// -y (front)
		{ .position = { -SKY_DISTANCE, -SKY_DISTANCE, SKY_DISTANCE },
		  .texcoord = { SKY_ST_MAX, SKY_ST_MIN } },
		{ .position = { -SKY_DISTANCE, -SKY_DISTANCE, -SKY_DISTANCE },
		  .texcoord = { SKY_ST_MAX, SKY_ST_MAX } },
		{ .position = { SKY_DISTANCE, -SKY_DISTANCE, -SKY_DISTANCE },
		  .texcoord = { SKY_ST_MIN, SKY_ST_MAX } },
		{ .position = { SKY_DISTANCE, -SKY_DISTANCE, SKY_DISTANCE },
		  .texcoord = { SKY_ST_MIN, SKY_ST_MIN } },
	};

	R_CreateInterleaveBuffer(&r_sky.quick_vert_buffer, sizeof(r_sky_interleave_vertex_t), r_sky_layout_buffer, GL_STATIC_DRAW,
	                         sizeof(quick_sky_verts), quick_sky_verts);

#define TRIANGLE_ELEMENTS_FROM(start) \
		start + 0, start + 1, start + 2, \
		start + 0, start + 2, start + 3

	const byte quick_sky_elements[] = {
		TRIANGLE_ELEMENTS_FROM(0),
		TRIANGLE_ELEMENTS_FROM(4),
		TRIANGLE_ELEMENTS_FROM(8),
		TRIANGLE_ELEMENTS_FROM(12),
		TRIANGLE_ELEMENTS_FROM(16),
		TRIANGLE_ELEMENTS_FROM(20)
	};

	R_CreateElementBuffer(&r_sky.quick_elem_buffer, R_ATTRIB_UNSIGNED_BYTE, GL_STATIC_DRAW, sizeof(quick_sky_elements), quick_sky_elements);
}

/**
 * @brief
 */
void R_ShutdownSky(void) {
	
	R_DestroyBuffer(&r_sky.quick_vert_buffer);

	R_DestroyBuffer(&r_sky.quick_elem_buffer);
}

/**
 * @brief Sets the sky to the specified environment map.
 */
void R_SetSky(const char *name) {
	const char *suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };
	uint32_t i;

	for (i = 0; i < lengthof(suf); i++) {
		char path[MAX_QPATH];

		g_snprintf(path, sizeof(path), "env/%s%s", name, suf[i]);
		r_sky.images[i] = R_LoadImage(path, IT_SKY);

		if (r_sky.images[i]->type == IT_NULL) { // try unit1_
			if (g_strcmp0(name, "unit1_")) {
				R_SetSky("unit1_");
				return;
			}
		}
	}
}

/**
 * @brief
 */
void R_Sky_f(void) {

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <basename>\n", Cmd_Argv(0));
		return;
	}

	R_SetSky(Cmd_Argv(1));
}
