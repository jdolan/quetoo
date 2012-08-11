/*
 * Copyright(c) 1997-2001 Id Software, Inc.
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
typedef struct r_warp_program_s {
	r_uniform3fv_t offset;

	r_sampler2d_t sampler0;
	r_sampler2d_t sampler1;
} r_warp_program_t;

static r_warp_program_t r_warp_program;

/*
 * @brief
 */
void R_InitProgram_warp(void) {
	r_warp_program_t *p = &r_warp_program;

	R_ProgramVariable(&p->offset, R_UNIFORM_VECTOR, "OFFSET");

	R_ProgramVariable(&p->sampler0, R_SAMPLER_2D, "SAMPLER0");
	R_ProgramVariable(&p->sampler1, R_SAMPLER_2D, "SAMPLER1");

	R_ProgramParameter3fv(&p->offset, vec3_origin);

	R_ProgramParameter1i(&p->sampler0, 0);
	R_ProgramParameter1i(&p->sampler1, 1);
}

/*
 * @brief
 */
void R_UseProgram_warp(void) {
	static vec3_t offset;
	r_warp_program_t *p = &r_warp_program;

	offset[0] = offset[1] = r_view.time / 8.0;

	R_ProgramParameter3fv(&p->offset, offset);
}
