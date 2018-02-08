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

#pragma once

#include "r_types.h"

#ifdef __R_LOCAL_H__

// program variables
#define R_ATTRIBUTE 		0x1
#define R_UNIFORM_INT 		0x2
#define R_UNIFORM_FLOAT		0x4
#define R_UNIFORM_VEC3		0x10
#define R_UNIFORM_VEC4		0x20
#define R_UNIFORM_MAT4		0x40
#define R_SAMPLER_2D		0x80

typedef union {
	GLint i;
	GLfloat f;
	vec3_t vec3;
	vec4_t vec4;
	u8vec4_t u8vec4;
	matrix4x4_t mat4;
	const r_buffer_t *buffer;
} r_variable_value_t;

typedef struct {
	GLenum type;
	char name[MAX_QPATH];
	GLint location;
	r_variable_value_t value;
} r_variable_t;

typedef r_variable_t r_attribute_t;
typedef r_variable_t r_uniform1i_t;
typedef r_variable_t r_uniform1f_t;
typedef r_variable_t r_uniform3f_t;
typedef r_variable_t r_uniform3fv_t;
typedef r_variable_t r_uniform4fv_t;
typedef r_variable_t r_uniform_matrix4fv_t;
typedef r_variable_t r_sampler2d_t;

// fog info
typedef struct {
	vec_t start;
	vec_t end;
	vec3_t color;
	vec_t density;
} r_fog_parameters_t;

typedef struct {
	r_variable_t start;
	r_variable_t end;
	r_variable_t color;
	r_variable_t density;
	r_variable_t gamma_correction;
} r_uniform_fog_t;

// light info
typedef struct {
	r_variable_t origin;
	r_variable_t color;
	r_variable_t radius;
} r_uniform_light_t;

// caustic info
typedef struct {
	_Bool enable;
	vec3_t color;
} r_caustic_parameters_t;

typedef struct {
	r_variable_t enable;
	r_variable_t color;
} r_uniform_caustic_t;

#define MAX_PROGRAM_VARIABLES 32

// and glsl programs
typedef struct {
	GLuint id; // the actual GL ID handle
	r_program_id_t program_id; // the program ID, for easy access
	char name[MAX_QPATH];

	r_attribute_mask_t arrays_mask;
	r_attribute_t attributes[R_ATTRIB_ALL];

	_Bool matrix_dirty[R_MATRIX_TOTAL];

	void (*Init)(void);
	void (*Shutdown)(void);
	void (*Use)(void);
	void (*UseMaterial)(const r_material_t *material);
	void (*UseEntity)(const r_entity_t *e);
	void (*UseShadow)(const r_shadow_t *s);
	void (*UseFog)(const r_fog_parameters_t *fog);
	void (*UseLight)(const uint16_t light_index, const matrix4x4_t *world_view, const r_light_t *light);
	void (*UseCaustic)(const r_caustic_parameters_t *caustic);
	void (*MatricesChanged)(void);
	void (*UseAlphaTest)(const vec_t threshold);
	void (*UseAttributes)(void);
	void (*UseTints)(void);
} r_program_t;

// attribute state
typedef struct {
	r_variable_value_t value;
	const r_attrib_type_state_t *type;
	GLsizeiptr offset;
	_Bool enabled;
} r_attrib_state_t;

void R_UseProgram(const r_program_t *prog);
void R_ProgramVariable(r_variable_t *variable, const GLenum type, const char *name, const _Bool required);
void R_ProgramParameter1i(r_uniform1i_t *variable, const GLint value);
void R_ProgramParameter1f(r_uniform1f_t *variable, const GLfloat value);
void R_ProgramParameter3fv(r_uniform3fv_t *variable, const GLfloat *value);
void R_ProgramParameter4fv(r_uniform4fv_t *variable, const GLfloat *value);
void R_ProgramParameter4ubv(r_uniform4fv_t *variable, const GLubyte *value);
_Bool R_ProgramParameterMatrix4fv(r_uniform_matrix4fv_t *variable, const GLfloat *value);
void R_BindAttributeLocation(const r_program_t *prog, const char *name, const GLuint location);
void R_EnableAttribute(const r_attribute_id_t attribute);
void R_DisableAttribute(const r_attribute_id_t attribute);
void R_SetupAttributes(void);
void R_SetupUniforms(void);
void R_ShutdownPrograms(void);
void R_InitPrograms(void);

extern cvar_t *r_geometry_shaders;

#endif /* __R_LOCAL_H__ */
