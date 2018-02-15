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
	r_uniform_fog_t fog;

	r_uniform3f_t view_right;
	r_uniform3f_t view_up;
} r_particle_corona_program_t;

static r_particle_corona_program_t r_particle_corona_program;

/**
 * @brief
 */
void R_PreLink_particle_corona(const r_program_t *program) {

	R_BindAttributeLocation(program, "POSITION", R_ATTRIB_POSITION);
	R_BindAttributeLocation(program, "COLOR", R_ATTRIB_COLOR);
	R_BindAttributeLocation(program, "SCALE", R_ATTRIB_SCALE);
}

/**
 * @brief
 */
void R_InitProgram_particle_corona(r_program_t *program) {

	r_particle_corona_program_t *p = &r_particle_corona_program;

	R_ProgramVariable(&program->attributes[R_ATTRIB_POSITION], R_ATTRIBUTE, "POSITION", true);
	R_ProgramVariable(&program->attributes[R_ATTRIB_COLOR], R_ATTRIBUTE, "COLOR", true);
	R_ProgramVariable(&program->attributes[R_ATTRIB_SCALE], R_ATTRIBUTE, "SCALE", true);

	R_ProgramVariable(&p->fog.start, R_UNIFORM_FLOAT, "FOG.START", true);
	R_ProgramVariable(&p->fog.end, R_UNIFORM_FLOAT, "FOG.END", true);
	R_ProgramVariable(&p->fog.color, R_UNIFORM_VEC3, "FOG.COLOR", true);
	R_ProgramVariable(&p->fog.density, R_UNIFORM_FLOAT, "FOG.DENSITY", true);
	R_ProgramVariable(&p->fog.gamma_correction, R_UNIFORM_FLOAT, "FOG.GAMMA_CORRECTION", true);

	R_ProgramVariable(&p->view_right, R_UNIFORM_VEC3, "VIEW_RIGHT", true);
	R_ProgramVariable(&p->view_up, R_UNIFORM_VEC3, "VIEW_UP", true);

	R_ProgramParameter1f(&p->fog.density, 0.0);
}

/**
 * @brief
 */
void R_UseFog_particle_corona(const r_fog_parameters_t *fog) {
	r_particle_corona_program_t *p = &r_particle_corona_program;

	if (fog && fog->density) {
		R_ProgramParameter1f(&p->fog.density, fog->density);
		R_ProgramParameter1f(&p->fog.start, fog->start);
		R_ProgramParameter1f(&p->fog.end, fog->end);
		R_ProgramParameter3fv(&p->fog.color, fog->color);
	} else {
		R_ProgramParameter1f(&p->fog.density, 0.0);
	}

	R_ProgramParameter1f(&p->fog.gamma_correction, r_state.screenshot_pending || r_framebuffer_state.current_framebuffer ? 0.0 : 1.0);
}

/**
 * @brief
 */
void R_UseParticleData_particle_corona(vec3_t weather_right, vec3_t weather_up, vec3_t splash_right[2], vec3_t splash_up[2]) {
	r_particle_corona_program_t *p = &r_particle_corona_program;

	R_ProgramParameter3fv(&p->view_right, r_view.right);
	R_ProgramParameter3fv(&p->view_up, r_view.up);
}