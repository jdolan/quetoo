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
	r_sampler2d_t sampler6;

	r_uniform1i_t tintmap;
	
	r_uniform_fog_t fog;

	r_uniform4fv_t current_color;

	r_uniform1f_t time_fraction;
	
	r_uniform4fv_t tints[TINT_TOTAL];
} r_null_program_t;

static r_null_program_t r_null_program;

/**
 * @brief
 */
void R_PreLink_null(const r_program_t *program) {
	
	R_BindAttributeLocation(program, "POSITION", R_ATTRIB_POSITION);
	R_BindAttributeLocation(program, "COLOR", R_ATTRIB_COLOR);
	R_BindAttributeLocation(program, "TEXCOORD", R_ATTRIB_DIFFUSE);

	R_BindAttributeLocation(program, "NEXT_POSITION", R_ATTRIB_NEXT_POSITION);
}

/**
 * @brief
 */
void R_InitProgram_null(r_program_t *program) {

	r_null_program_t *p = &r_null_program;

	R_ProgramVariable(&program->attributes[R_ATTRIB_POSITION], R_ATTRIBUTE, "POSITION", true);
	R_ProgramVariable(&program->attributes[R_ATTRIB_COLOR], R_ATTRIBUTE, "COLOR", true);
	R_ProgramVariable(&program->attributes[R_ATTRIB_DIFFUSE], R_ATTRIBUTE, "TEXCOORD", true);

	R_ProgramVariable(&program->attributes[R_ATTRIB_NEXT_POSITION], R_ATTRIBUTE, "NEXT_POSITION", true);
	
	R_ProgramVariable(&p->tintmap, R_UNIFORM_INT, "TINTMAP", true);

	R_ProgramVariable(&p->sampler0, R_SAMPLER_2D, "SAMPLER0", true);
	R_ProgramVariable(&p->sampler6, R_SAMPLER_2D, "SAMPLER6", true);

	R_ProgramVariable(&p->fog.start, R_UNIFORM_FLOAT, "FOG.START", true);
	R_ProgramVariable(&p->fog.end, R_UNIFORM_FLOAT, "FOG.END", true);
	R_ProgramVariable(&p->fog.color, R_UNIFORM_VEC3, "FOG.COLOR", true);
	R_ProgramVariable(&p->fog.density, R_UNIFORM_FLOAT, "FOG.DENSITY", true);
	
	for (int32_t i = 0; i < TINT_TOTAL; i++) {
		R_ProgramVariable(&p->tints[i], R_UNIFORM_VEC4, va("TINTS[%i]", i), true);
	}

	R_ProgramVariable(&p->current_color, R_UNIFORM_VEC4, "GLOBAL_COLOR", true);

	R_ProgramVariable(&p->time_fraction, R_UNIFORM_FLOAT, "TIME_FRACTION", true);

	R_ProgramParameter1i(&p->tintmap, 0);

	R_ProgramParameter1i(&p->sampler0, R_TEXUNIT_DIFFUSE);
	R_ProgramParameter1i(&p->sampler6, R_TEXUNIT_TINTMAP);

	R_ProgramParameter1f(&p->fog.density, 0.0);

	const vec4_t white = { 1.0, 1.0, 1.0, 1.0 };
	R_ProgramParameter4fv(&p->current_color, white);

	R_ProgramParameter1f(&p->time_fraction, 0.0f);

}

/**
 * @brief
 */
void R_UseFog_null(const r_fog_parameters_t *fog) {

	r_null_program_t *p = &r_null_program;

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
void R_UseCurrentColor_null(const vec4_t color) {

	r_null_program_t *p = &r_null_program;
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
void R_UseInterpolation_null(const vec_t time_fraction) {

	r_null_program_t *p = &r_null_program;

	R_ProgramParameter1f(&p->time_fraction, time_fraction);
}

/**
 * @brief
 */
void R_UseMaterial_null(const r_material_t *material) {
	r_null_program_t *p = &r_null_program;

	if (material && material->tintmap) {
		R_BindTintTexture(material->tintmap->texnum);
		R_ProgramParameter1i(&p->tintmap, 1);
	} else {
		R_ProgramParameter1i(&p->tintmap, 0);
	}
}

/**
 * @brief
 */
void R_UseTints_null(void) {
	r_null_program_t *p = &r_null_program;

	for (int32_t i = 0; i < TINT_TOTAL; i++) {
		if (r_view.current_entity->tints[i][3]) {
			R_ProgramParameter4fv(&p->tints[i], r_view.current_entity->tints[i]);
		} else {
			R_ProgramParameter4fv(&p->tints[i], r_state.active_material->cm->tintmap_defaults[i]);
		}
	}
}
