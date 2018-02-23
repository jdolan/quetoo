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

	r_uniform4fv_t current_color;

	r_uniform3f_t view_origin;
	r_uniform3f_t view_angles;
	r_uniform3f_t view_right;
	r_uniform3f_t view_up;
	r_uniform3f_t weather_right;
	r_uniform3f_t weather_up;
	r_uniform3f_t splash_right[2];
	r_uniform3f_t splash_up[2];
	r_uniform1f_t ticks;
} r_particle_program_t;

static r_particle_program_t r_particle_program;

/**
 * @brief
 */
void R_PreLink_particle(const r_program_t *program) {

	R_BindAttributeLocation(program, "POSITION", R_ATTRIB_POSITION);
	R_BindAttributeLocation(program, "COLOR", R_ATTRIB_COLOR);
	R_BindAttributeLocation(program, "TEXCOORD0", R_ATTRIB_DIFFUSE);
	R_BindAttributeLocation(program, "TEXCOORD1", R_ATTRIB_LIGHTMAP);
	R_BindAttributeLocation(program, "SCALE", R_ATTRIB_SCALE);
	R_BindAttributeLocation(program, "ROLL", R_ATTRIB_ROLL);
	R_BindAttributeLocation(program, "END", R_ATTRIB_END);
	R_BindAttributeLocation(program, "TYPE", R_ATTRIB_TYPE);
}

/**
 * @brief
 */
void R_InitProgram_particle(r_program_t *program) {

	r_particle_program_t *p = &r_particle_program;

	R_ProgramVariable(&program->attributes[R_ATTRIB_POSITION], R_ATTRIBUTE, "POSITION", true);
	R_ProgramVariable(&program->attributes[R_ATTRIB_COLOR], R_ATTRIBUTE, "COLOR", true);
	R_ProgramVariable(&program->attributes[R_ATTRIB_DIFFUSE], R_ATTRIBUTE, "TEXCOORD0", true);
	R_ProgramVariable(&program->attributes[R_ATTRIB_LIGHTMAP], R_ATTRIBUTE, "TEXCOORD1", true);
	R_ProgramVariable(&program->attributes[R_ATTRIB_SCALE], R_ATTRIBUTE, "SCALE", true);
	R_ProgramVariable(&program->attributes[R_ATTRIB_ROLL], R_ATTRIBUTE, "ROLL", true);
	R_ProgramVariable(&program->attributes[R_ATTRIB_END], R_ATTRIBUTE, "END", true);
	R_ProgramVariable(&program->attributes[R_ATTRIB_TYPE], R_ATTRIBUTE, "TYPE", true);
	
	R_ProgramVariable(&p->sampler0, R_SAMPLER_2D, "SAMPLER0", true);

	R_ProgramVariable(&p->fog.start, R_UNIFORM_FLOAT, "FOG.START", true);
	R_ProgramVariable(&p->fog.end, R_UNIFORM_FLOAT, "FOG.END", true);
	R_ProgramVariable(&p->fog.color, R_UNIFORM_VEC3, "FOG.COLOR", true);
	R_ProgramVariable(&p->fog.density, R_UNIFORM_FLOAT, "FOG.DENSITY", true);
	
	R_ProgramVariable(&p->view_origin, R_UNIFORM_VEC3, "VIEW_ORIGIN", true);
	R_ProgramVariable(&p->view_angles, R_UNIFORM_VEC3, "VIEW_ANGLES", true);
	R_ProgramVariable(&p->view_right, R_UNIFORM_VEC3, "VIEW_RIGHT", true);
	R_ProgramVariable(&p->view_up, R_UNIFORM_VEC3, "VIEW_UP", true);
	R_ProgramVariable(&p->weather_right, R_UNIFORM_VEC3, "WEATHER_RIGHT", true);
	R_ProgramVariable(&p->weather_up, R_UNIFORM_VEC3, "WEATHER_UP", true);

	for (int32_t i = 0; i < 2; i++) {
		R_ProgramVariable(&p->splash_right[i], R_UNIFORM_VEC4, va("SPLASH_RIGHT[%i]", i), true);
		R_ProgramVariable(&p->splash_up[i], R_UNIFORM_VEC4, va("SPLASH_UP[%i]", i), true);
	}

	R_ProgramVariable(&p->ticks, R_UNIFORM_FLOAT, "TICKS", true);

	R_ProgramVariable(&p->current_color, R_UNIFORM_VEC4, "GLOBAL_COLOR", true);

	R_ProgramParameter1i(&p->sampler0, R_TEXUNIT_DIFFUSE);

	R_ProgramParameter1f(&p->fog.density, 0.0);

	const vec4_t white = { 1.0, 1.0, 1.0, 1.0 };
	R_ProgramParameter4fv(&p->current_color, white);
}

/**
 * @brief
 */
void R_UseFog_particle(const r_fog_parameters_t *fog) {

	r_particle_program_t *p = &r_particle_program;

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
void R_UseCurrentColor_particle(const vec4_t color) {

	r_particle_program_t *p = &r_particle_program;
	const vec4_t white = { 1.0, 1.0, 1.0, 1.0 };

	if (color && r_state.color_array_enabled) {
		R_ProgramParameter4fv(&p->current_color, color);
	} else {
		R_ProgramParameter4fv(&p->current_color, white);
	}
}

/**
 * @brief
 */
void R_UseParticleData_particle(vec3_t weather_right, vec3_t weather_up, vec3_t splash_right[2], vec3_t splash_up[2]) {
	r_particle_program_t *p = &r_particle_program;

	R_ProgramParameter3fv(&p->view_origin, r_view.origin);
	R_ProgramParameter3fv(&p->view_angles, r_view.angles);
	R_ProgramParameter3fv(&p->view_right, r_view.right);
	R_ProgramParameter3fv(&p->view_up, r_view.up);
	R_ProgramParameter3fv(&p->weather_right, weather_right);
	R_ProgramParameter3fv(&p->weather_up, weather_up);

	for (int32_t i = 0; i < 2; i++) {
		R_ProgramParameter3fv(&p->splash_right[i], splash_right[i]);
		R_ProgramParameter3fv(&p->splash_up[i], splash_up[i]);
	}

	R_ProgramParameter1f(&p->ticks, r_view.ticks / 1000.0);
}