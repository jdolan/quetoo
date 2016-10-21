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
typedef struct r_shell_program_s {
	r_uniform1f_t offset;
	r_uniform1f_t time_fraction;

	r_sampler2d_t sampler0;
	r_sampler2d_t sampler1;

	r_uniform_matrix4fv_t projection_mat;
	r_uniform_matrix4fv_t modelview_mat;

	r_uniform4fv_t current_color;
} r_shell_program_t;

static r_shell_program_t r_shell_program;

/**
 * @brief
 */
void R_PreLink_shell(const r_program_t *program) {
	
	R_BindAttributeLocation(program, "POSITION", R_ARRAY_VERTEX);
	R_BindAttributeLocation(program, "TEXCOORD", R_ARRAY_TEX_DIFFUSE);
	R_BindAttributeLocation(program, "NEXT_POSITION", R_ARRAY_NEXT_VERTEX);
}

/**
 * @brief
 */
void R_InitProgram_shell(r_program_t *program) {
	r_shell_program_t *p = &r_shell_program;

	R_ProgramVariable(&program->attributes[R_ARRAY_VERTEX], R_ATTRIBUTE, "POSITION");
	R_ProgramVariable(&program->attributes[R_ARRAY_TEX_DIFFUSE], R_ATTRIBUTE, "TEXCOORD");
	R_ProgramVariable(&program->attributes[R_ARRAY_NEXT_VERTEX], R_ATTRIBUTE, "NEXT_POSITION");

	R_ProgramVariable(&p->offset, R_UNIFORM_FLOAT, "OFFSET");
	R_ProgramParameter1f(&p->offset, 0.0);

	R_ProgramVariable(&p->sampler0, R_SAMPLER_2D, "SAMPLER0");
	R_ProgramParameter1i(&p->sampler0, 0);

	R_ProgramVariable(&p->projection_mat, R_UNIFORM_MAT4, "PROJECTION_MAT");
	R_ProgramVariable(&p->modelview_mat, R_UNIFORM_MAT4, "MODELVIEW_MAT");

	R_ProgramVariable(&p->current_color, R_UNIFORM_VEC4, "GLOBAL_COLOR");

	R_ProgramVariable(&p->time_fraction, R_UNIFORM_FLOAT, "TIME_FRACTION");

	const vec4_t white = { 1.0, 1.0, 1.0, 1.0 };
	R_ProgramParameter4fv(&p->current_color, white);

	R_ProgramParameter1f(&p->time_fraction, 0.0f);
}

/**
 * @brief
 */
void R_UseProgram_shell(void) {

	R_ProgramParameter1f(&r_shell_program.offset, r_view.time * 0.00025);
}

/**
 * @brief
 */
void R_UseMatrices_shell(const matrix4x4_t *projection, const matrix4x4_t *modelview, const matrix4x4_t *texture) {
	
	r_shell_program_t *p = &r_shell_program;

	if (projection)
		R_ProgramParameterMatrix4fv(&p->projection_mat, (const GLfloat *) projection->m);

	if (modelview)
		R_ProgramParameterMatrix4fv(&p->modelview_mat, (const GLfloat *) modelview->m);
}

/**
 * @brief
 */
void R_UseCurrentColor_shell(const vec4_t color) {

	r_shell_program_t *p = &r_shell_program;
	const vec4_t white = { 1.0, 1.0, 1.0, 1.0 };

	if (color)
		R_ProgramParameter4fv(&p->current_color, color);
	else
		R_ProgramParameter4fv(&p->current_color, white);
}

/**
 * @brief
 */
void R_UseInterpolation_shell(const float time_fraction) {

	r_shell_program_t *p = &r_shell_program;

	R_ProgramParameter1f(&p->time_fraction, time_fraction);
}