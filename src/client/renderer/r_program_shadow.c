/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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
typedef struct r_shadow_program_s {
	r_uniform_matrix4fv_t model_matrix;
	r_uniform_matrix4fv_t shadow_matrix;
} r_shadow_program_t;

static r_shadow_program_t r_shadow_program;

/*
 * @brief
 */
void R_InitProgram_shadow(void) {
	r_shadow_program_t *p = &r_shadow_program;

	R_ProgramVariable(&p->model_matrix, R_UNIFORM_MATRIX, "MODEL_MATRIX");
	R_ProgramVariable(&p->shadow_matrix, R_UNIFORM_MATRIX, "SHADOW_MATRIX");

	R_ProgramParameterMatrix4fv(&p->model_matrix, (GLfloat *) matrix4x4_identity.m);
	R_ProgramParameterMatrix4fv(&p->shadow_matrix, (GLfloat *) matrix4x4_identity.m);
}

/*
 * @brief
 */
void R_UseProgram_shadow(void) {
	r_shadow_program_t *p = &r_shadow_program;

	if (!r_view.current_entity || !r_view.current_shadow)
		return;

	R_ProgramParameterMatrix4fv(&p->model_matrix, (GLfloat *) r_view.current_entity->matrix.m);
	R_ProgramParameterMatrix4fv(&p->shadow_matrix, (GLfloat *) r_view.current_shadow->matrix.m);
}
