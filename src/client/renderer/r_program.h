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

#ifndef __R_PROGRAM_H__
#define __R_PROGRAM_H__

#include "r_types.h"

#ifdef __R_LOCAL_H__

// glsl vertex and fragment shaders
typedef struct r_shader_s {
	GLenum type;
	GLuint id;
	char name[MAX_QPATH];
}r_shader_t;

#define MAX_SHADERS 16

// program variables
#define R_ATTRIBUTE 		0x1
#define R_UNIFORM_INT 		0x2
#define R_UNIFORM_FLOAT		0x4
#define R_UNIFORM_VECTOR	0x8
#define R_SAMPLER_2D		0x10

typedef union r_variable_value_u {
	GLint i;
	GLfloat f;
	vec3_t vec3;
}r_variable_value_t;

typedef struct r_variable_s {
	GLenum type;
	char name[64];
	GLint location;
	r_variable_value_t value;
}r_variable_t;

typedef r_variable_t r_attribute_t;
typedef r_variable_t r_uniform1i_t;
typedef r_variable_t r_uniform1f_t;
typedef r_variable_t r_uniform3fv_t;
typedef r_variable_t r_sampler2d_t;

#define MAX_PROGRAM_VARIABLES 32

// and glsl programs
typedef struct r_program_s {
	GLuint id;
	char name[64];
	r_shader_t *v;
	r_shader_t *f;
	uint32_t arrays_mask;
	void (*Init)(void);
	void (*Use)(void);
	void (*UseMaterial)(const r_bsp_surface_t *surf, const r_material_t *material);
}r_program_t;

#define MAX_PROGRAMS 8

void R_UseProgram(r_program_t *prog);
void R_ProgramVariable(r_variable_t *variable, GLenum type, const char *name);
void R_ProgramParameter1i(r_uniform1i_t *variable, GLint value);
void R_ProgramParameter1f(r_uniform1f_t *variable, GLfloat value);
void R_ProgramParameter3fv(r_uniform3fv_t *variable, GLfloat *value);
void R_AttributePointer(const char *name, GLuint size, GLvoid *array);
void R_EnableAttribute(r_attribute_t *attribute);
void R_DisableAttribute(r_attribute_t *attribute);
void R_ShutdownPrograms(void);
void R_InitPrograms(void);

#endif /* __R_LOCAL_H__ */

#endif /* __R_PROGRAM_H__ */
