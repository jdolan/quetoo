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

// these are the variables defined in the GLSL shader
typedef struct r_warp_program_s {
	r_uniform1f_t offset;

	r_sampler2d_t sampler0;
	r_sampler2d_t sampler1;

	r_uniform_fog_t fog;

	r_uniform_matrix4fv_t projection_mat;
	r_uniform_matrix4fv_t modelview_mat;

	r_uniform4fv_t current_color;
} r_warp_program_t;

static r_warp_program_t r_warp_program;

/**
 * @brief
 */
void R_PreLink_warp(const r_program_t *program) {

	R_BindAttributeLocation(program, "POSITION", R_ARRAY_VERTEX);
	R_BindAttributeLocation(program, "TEXCOORD", R_ARRAY_TEX_DIFFUSE);
}

/**
 * @brief
 */
void R_InitProgram_warp(r_program_t *program) {
	r_warp_program_t *p = &r_warp_program;

	R_ProgramVariable(&program->attributes[R_ARRAY_VERTEX], R_ATTRIBUTE, "POSITION");
	R_ProgramVariable(&program->attributes[R_ARRAY_TEX_DIFFUSE], R_ATTRIBUTE, "TEXCOORD");

	R_ProgramVariable(&p->offset, R_UNIFORM_FLOAT, "OFFSET");

	R_ProgramVariable(&p->sampler0, R_SAMPLER_2D, "SAMPLER0");
	R_ProgramVariable(&p->sampler1, R_SAMPLER_2D, "SAMPLER1");

	R_ProgramVariable(&p->current_color, R_UNIFORM_VEC4, "GLOBAL_COLOR");

	R_ProgramVariable(&p->fog.start, R_UNIFORM_FLOAT, "FOG.START");
	R_ProgramVariable(&p->fog.end, R_UNIFORM_FLOAT, "FOG.END");
	R_ProgramVariable(&p->fog.color, R_UNIFORM_VEC3, "FOG.COLOR");
	R_ProgramVariable(&p->fog.density, R_UNIFORM_FLOAT, "FOG.DENSITY");

	R_ProgramParameter1f(&p->offset, 0.0);

	R_ProgramParameter1i(&p->sampler0, 0);
	R_ProgramParameter1i(&p->sampler1, 1);

	R_ProgramParameter1f(&p->fog.density, 0.0);

	R_ProgramVariable(&p->projection_mat, R_UNIFORM_MAT4, "PROJECTION_MAT");
	R_ProgramVariable(&p->modelview_mat, R_UNIFORM_MAT4, "MODELVIEW_MAT");

	const vec4_t white = { 1.0, 1.0, 1.0, 1.0 };
	R_ProgramParameter4fv(&p->current_color, white);
}

/**
 * @brief
 */
void R_UseProgram_warp(void) {

	r_warp_program_t *p = &r_warp_program;

	R_ProgramParameter1f(&p->offset, r_view.time * 0.000125);
}

/**
 * @brief
 */
void R_UseFog_warp(const r_fog_parameters_t *fog) {

	r_warp_program_t *p = &r_warp_program;

	if (fog && fog->density) {
		R_ProgramParameter1f(&p->fog.density, fog->density);
		R_ProgramParameter1f(&p->fog.start, fog->start);
		R_ProgramParameter1f(&p->fog.end, fog->end);
		R_ProgramParameter3fv(&p->fog.color, fog->color);
	} else {
		R_ProgramParameter1f(&p->fog.density, 0.0);
	}
}

/**
 * @brief
 */
void R_UseMatrices_warp(const matrix4x4_t *matrices) {

	r_warp_program_t *p = &r_warp_program;

	R_ProgramParameterMatrix4fv(&p->projection_mat, (const GLfloat *) matrices[R_MATRIX_PROJECTION].m);
	R_ProgramParameterMatrix4fv(&p->modelview_mat, (const GLfloat *) matrices[R_MATRIX_MODELVIEW].m);
}

/**
 * @brief
 */
void R_UseCurrentColor_warp(const vec4_t color) {

	r_warp_program_t *p = &r_warp_program;
	const vec4_t white = { 1.0, 1.0, 1.0, 1.0 };

	if (color) {
		R_ProgramParameter4fv(&p->current_color, color);
	} else {
		R_ProgramParameter4fv(&p->current_color, white);
	}
}
