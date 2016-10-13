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

	r_sampler2d_t sampler0;
	r_sampler2d_t sampler1;

	r_uniform_matrix4fv_t projection_mat;
	r_uniform_matrix4fv_t modelview_mat;
} r_shell_program_t;

static r_shell_program_t r_shell_program;

/**
 * @brief
 */
void R_InitProgram_shell(void) {
	r_shell_program_t *p = &r_shell_program;

	R_ProgramVariable(&p->offset, R_UNIFORM_FLOAT, "OFFSET");
	R_ProgramParameter1f(&p->offset, 0.0);

	R_ProgramVariable(&p->sampler0, R_SAMPLER_2D, "SAMPLER0");
	R_ProgramParameter1i(&p->sampler0, 0);

	R_ProgramVariable(&p->projection_mat, R_UNIFORM_MAT4, "PROJECTION_MAT");
	R_ProgramVariable(&p->modelview_mat, R_UNIFORM_MAT4, "MODELVIEW_MAT");
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
