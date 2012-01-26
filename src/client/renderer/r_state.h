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

#ifndef __R_STATE_H__
#define __R_STATE_H__

#include "r_types.h"

void R_Setup3D(void);
void R_Setup2D(void);

#ifdef __R_LOCAL_H__

#include "r_program.h"

// vertex arrays are used for many things
#define MAX_GL_ARRAY_LENGTH 16384
extern const float default_texcoords[];

// texunits maintain multitexture state
typedef struct r_texunit_s {
	boolean_t enabled;  // GL_TEXTURE_2D on / off
	GLenum texture;  // e.g. GL_TEXTURE0_ARB
	GLuint texnum;  // e.g 123
	GLfloat texcoord_array[MAX_GL_ARRAY_LENGTH * 2];
} r_texunit_t;

#define MAX_GL_TEXUNITS		4

// opengl state management
typedef struct r_state_s {
	boolean_t ortho;  // 2d vs 3d projection

	GLfloat vertex_array_3d[MAX_GL_ARRAY_LENGTH * 3];  // default vertex arrays
	GLshort vertex_array_2d[MAX_GL_ARRAY_LENGTH * 2];
	GLfloat color_array[MAX_GL_ARRAY_LENGTH * 4];
	GLfloat normal_array[MAX_GL_ARRAY_LENGTH * 3];
	GLfloat tangent_array[MAX_GL_ARRAY_LENGTH * 3];

	GLenum blend_src, blend_dest;  // blend function
	boolean_t blend_enabled;

	r_texunit_t texunits[MAX_GL_TEXUNITS];
	r_texunit_t *active_texunit;

	r_shader_t shaders[MAX_SHADERS];
	r_program_t programs[MAX_PROGRAMS];
	r_program_t *world_program;
	r_program_t *mesh_program;
	r_program_t *warp_program;
	r_program_t *pro_program;
	r_program_t *active_program;

	r_material_t *active_material;

	boolean_t color_array_enabled;
	boolean_t alpha_test_enabled;
	boolean_t stencil_test_enabled;
	boolean_t lighting_enabled;
	boolean_t bumpmap_enabled;
	boolean_t warp_enabled;
	boolean_t shell_enabled;
	boolean_t fog_enabled;
} r_state_t;

extern r_state_t r_state;

// these are defined for convenience
#define texunit_diffuse			r_state.texunits[0]
#define texunit_lightmap		r_state.texunits[1]
#define texunit_deluxemap		r_state.texunits[2]
#define texunit_normalmap		r_state.texunits[3]

void R_SelectTexture(r_texunit_t *texunit);
void R_BindTexture(GLuint texnum);
void R_BindLightmapTexture(GLuint texnum);
void R_BindDeluxemapTexture(GLuint texnum);
void R_BindNormalmapTexture(GLuint texnum);
void R_BindArray(GLenum target, GLenum type, GLvoid *array);
void R_BindDefaultArray(GLenum target);
void R_BindBuffer(GLenum target, GLenum type, GLuint id);
void R_BlendFunc(GLenum src, GLenum dest);
void R_EnableBlend(boolean_t enable);
void R_EnableAlphaTest(boolean_t enable);
void R_EnableStencilTest(boolean_t enable);
void R_EnableTexture(r_texunit_t *texunit, boolean_t enable);
void R_EnableColorArray(boolean_t enable);
void R_EnableLighting(r_program_t *program, boolean_t enable);
void R_EnableBumpmap(r_material_t *material, boolean_t enable);
void R_EnableWarp(r_program_t *program, boolean_t enable);
void R_EnableShell(boolean_t enable);
void R_EnableFog(boolean_t enable);
void R_SetDefaultState(void);
void R_InitState(void);

#endif /* __R_LOCAL_H__ */

#endif /* __R_STATE_H__ */
