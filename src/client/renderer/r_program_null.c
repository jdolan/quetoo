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
typedef struct {
	r_sampler2d_t sampler0;

	r_uniform_fog_t fog;

	r_uniform_matrix4fv_t projection_mat;
	r_uniform_matrix4fv_t modelview_mat;
	r_uniform_matrix4fv_t texture_mat;

	r_uniform4fv_t current_color;
} r_null_program_t;

static r_null_program_t r_null_program;

/**
 * @brief
 */
void R_PreLink_null(const r_program_t *program) {
	
	R_BindAttributeLocation(program, "POSITION", R_ARRAY_VERTEX);
	R_BindAttributeLocation(program, "COLOR", R_ARRAY_COLOR);
	R_BindAttributeLocation(program, "TEXCOORD", R_ARRAY_TEX_DIFFUSE);
}

/**
 * @brief
 */
void R_InitProgram_null(r_program_t *program) {

	r_null_program_t *p = &r_null_program;

	R_ProgramVariable(&program->attributes[R_ARRAY_VERTEX], R_ATTRIBUTE, "POSITION");
	R_ProgramVariable(&program->attributes[R_ARRAY_COLOR], R_ATTRIBUTE, "COLOR");
	R_ProgramVariable(&program->attributes[R_ARRAY_TEX_DIFFUSE], R_ATTRIBUTE, "TEXCOORD");
	
	R_ProgramVariable(&p->sampler0, R_SAMPLER_2D, "SAMPLER0");

	R_ProgramVariable(&p->fog.start, R_UNIFORM_FLOAT, "FOG.START");
	R_ProgramVariable(&p->fog.end, R_UNIFORM_FLOAT, "FOG.END");
	R_ProgramVariable(&p->fog.color, R_UNIFORM_VEC3, "FOG.COLOR");
	R_ProgramVariable(&p->fog.density, R_UNIFORM_FLOAT, "FOG.DENSITY");

	R_ProgramVariable(&p->projection_mat, R_UNIFORM_MAT4, "PROJECTION_MAT");
	R_ProgramVariable(&p->modelview_mat, R_UNIFORM_MAT4, "MODELVIEW_MAT");
	R_ProgramVariable(&p->texture_mat, R_UNIFORM_MAT4, "TEXTURE_MAT");

	R_ProgramVariable(&p->current_color, R_UNIFORM_VEC4, "GLOBAL_COLOR");

	R_ProgramParameter1i(&p->sampler0, 0);

	R_ProgramParameter1f(&p->fog.density, 0.0);

	const vec4_t white = { 1.0, 1.0, 1.0, 1.0 };
	R_ProgramParameter4fv(&p->current_color, white);
}

/**
 * @brief
 */
void R_UseFog_null(const r_fog_parameters_t *fog) {

	r_null_program_t *p = &r_null_program;

	if (fog && fog->density)
	{
		R_ProgramParameter1f(&p->fog.density, fog->density);
		R_ProgramParameter1f(&p->fog.start, fog->start);
		R_ProgramParameter1f(&p->fog.end, fog->end);
		R_ProgramParameter3fv(&p->fog.color, fog->color);
	}
	else
		R_ProgramParameter1f(&p->fog.density, 0.0);
}

/**
 * @brief
 */
void R_UseMatrices_null(const matrix4x4_t *matrices) {

	r_null_program_t *p = &r_null_program;

	R_ProgramParameterMatrix4fv(&p->projection_mat, (const GLfloat *) matrices[R_MATRIX_PROJECTION].m);
	R_ProgramParameterMatrix4fv(&p->modelview_mat, (const GLfloat *) matrices[R_MATRIX_MODELVIEW].m);
	R_ProgramParameterMatrix4fv(&p->texture_mat, (const GLfloat *) matrices[R_MATRIX_TEXTURE].m);
}

/**
 * @brief
 */
void R_UseCurrentColor_null(const vec4_t color) {

	r_null_program_t *p = &r_null_program;
	const vec4_t white = { 1.0, 1.0, 1.0, 1.0 };

	if (color && r_state.color_array_enabled)
		R_ProgramParameter4fv(&p->current_color, color);
	else
		R_ProgramParameter4fv(&p->current_color, white);
}
