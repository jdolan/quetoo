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
	s16vec3_t position;
	u16vec2_t texcoord;
} r_sky_interleave_vertex_t;

static r_buffer_layout_t r_sky_buffer_layout[] = {
	{ .attribute = R_ATTRIB_POSITION, .type = R_TYPE_SHORT, .count = 3 },
	{ .attribute = R_ATTRIB_DIFFUSE, .type = R_TYPE_UNSIGNED_SHORT, .count = 2, .normalized = true },
	{ .attribute = -1 }
};

// sky structure
typedef struct {
	r_image_t *images[6];
	r_buffer_t quick_vert_buffer;
} r_sky_t;

static r_sky_t r_sky;

/**
 * @brief
 */
void R_DrawSkyBox(void) {
	matrix4x4_t modelview;

	R_GetMatrix(R_MATRIX_MODELVIEW, &modelview);

	R_BindAttributeInterleaveBuffer(&r_sky.quick_vert_buffer, R_ATTRIB_MASK_ALL);

	R_PushMatrix(R_MATRIX_MODELVIEW);

	Matrix4x4_ConcatTranslate(&modelview, r_view.origin[0], r_view.origin[1], r_view.origin[2]);

	R_SetMatrix(R_MATRIX_MODELVIEW, &modelview);

	R_EnableFog(true);

	r_state.active_fog_parameters.end = FOG_END * 8.0;
	r_state.active_program->UseFog(&r_state.active_fog_parameters);

	R_BindDiffuseTexture(r_sky.images[4]->texnum);
	R_DrawArrays(GL_TRIANGLE_FAN, 0, 4);

	R_BindDiffuseTexture(r_sky.images[5]->texnum);
	R_DrawArrays(GL_TRIANGLE_FAN, 4, 4);

	R_BindDiffuseTexture(r_sky.images[0]->texnum);
	R_DrawArrays(GL_TRIANGLE_FAN, 8, 4);

	R_BindDiffuseTexture(r_sky.images[2]->texnum);
	R_DrawArrays(GL_TRIANGLE_FAN, 12, 4);

	R_BindDiffuseTexture(r_sky.images[1]->texnum);
	R_DrawArrays(GL_TRIANGLE_FAN, 16, 4);

	R_BindDiffuseTexture(r_sky.images[3]->texnum);
	R_DrawArrays(GL_TRIANGLE_FAN, 20, 4);

	r_state.active_fog_parameters.end = FOG_END;
	r_state.active_program->UseFog(&r_state.active_fog_parameters);

	R_EnableFog(false);

	R_PopMatrix(R_MATRIX_MODELVIEW);

	R_UnbindAttributeBuffers();
}

// 4 verts for 6 sides
#define MAX_SKY_VERTS	4 * 6

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

	R_CreateInterleaveBuffer(&r_sky.quick_vert_buffer, &(const r_create_interleave_t) {
		.struct_size = sizeof(r_sky_interleave_vertex_t),
		.layout = r_sky_buffer_layout,
		.hint = GL_STATIC_DRAW,
		.size = sizeof(r_sky_interleave_vertex_t) * MAX_SKY_VERTS,
		.data = NULL
	});

	R_UploadToSubBuffer(&r_sky.quick_vert_buffer, 0, sizeof(quick_sky_verts), quick_sky_verts, false);
}

/**
 * @brief
 */
void R_ShutdownSky(void) {

	R_DestroyBuffer(&r_sky.quick_vert_buffer);
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
