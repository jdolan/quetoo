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

void R_Color(const vec4_t color);
void R_Setup3D(void);
void R_Setup2D(void);

#if defined (__R_LOCAL_H__) || defined(__ECLIPSE__)

#include "r_program.h"

// vertex arrays are used for many things
#define MAX_GL_ARRAY_LENGTH 65536
extern const float default_texcoords[];

// texunits maintain multitexture state
typedef struct r_texunit_s {
	bool enabled;  // GL_TEXTURE_2D on / off
	GLenum texture;  // e.g. GL_TEXTURE0_ARB
	GLuint texnum;  // e.g 123
	GLfloat *texcoord_array;
} r_texunit_t;

#define MAX_GL_TEXUNITS		8

// opengl state management
typedef struct r_state_s {
	bool ortho; // 2d vs 3d projection

	GLfloat vertex_array_3d[MAX_GL_ARRAY_LENGTH * 3]; // default vertex arrays
	GLshort vertex_array_2d[MAX_GL_ARRAY_LENGTH * 2];
	GLfloat color_array[MAX_GL_ARRAY_LENGTH * 4];
	GLfloat normal_array[MAX_GL_ARRAY_LENGTH * 3];
	GLfloat tangent_array[MAX_GL_ARRAY_LENGTH * 3];

	GLenum blend_src, blend_dest; // blend function
	bool blend_enabled;

	r_texunit_t texunits[MAX_GL_TEXUNITS];
	r_texunit_t *active_texunit;

	r_shader_t shaders[MAX_SHADERS];
	r_program_t programs[MAX_PROGRAMS];
	r_program_t *default_program;
	r_program_t *warp_program;
	r_program_t *pro_program;
	r_program_t *active_program;

	r_material_t *active_material;

	bool color_array_enabled;
	bool alpha_test_enabled;
	bool stencil_test_enabled;
	bool lighting_enabled;
	bool warp_enabled;
	bool shell_enabled;
	bool fog_enabled;
} r_state_t;

extern r_state_t r_state;

// these are defined for convenience
#define texunit_diffuse			r_state.texunits[0]
#define texunit_lightmap		r_state.texunits[1]
#define texunit_deluxemap		r_state.texunits[2]
#define texunit_normalmap		r_state.texunits[3]
#define texunit_glossmap		r_state.texunits[4]

#define R_GetError(msg) R_GetError_(__func__, msg)
void R_GetError_(const char *function, const char *msg);
void R_SelectTexture(r_texunit_t *texunit);
void R_BindTexture(GLuint texnum);
void R_BindLightmapTexture(GLuint texnum);
void R_BindDeluxemapTexture(GLuint texnum);
void R_BindNormalmapTexture(GLuint texnum);
void R_BindGlossmapTexture(GLuint texnum);
void R_BindArray(GLenum target, GLenum type, GLvoid *array);
void R_BindDefaultArray(GLenum target);
void R_BindBuffer(GLenum target, GLenum type, GLuint id);
void R_BlendFunc(GLenum src, GLenum dest);
void R_EnableBlend(bool enable);
void R_EnableAlphaTest(bool enable);
void R_EnableStencilTest(bool enable);
void R_EnableTexture(r_texunit_t *texunit, bool enable);
void R_EnableColorArray(bool enable);
void R_EnableLighting(r_program_t *program, bool enable);
void R_EnableWarp(r_program_t *program, bool enable);
void R_EnableShell(bool enable);
void R_EnableFog(bool enable);
void R_UseMaterial(const r_bsp_surface_t *surf, const r_image_t *image);
void R_InitState(void);
void R_ShutdownState(void);

#endif /* __R_LOCAL_H__ */

#endif /* __R_STATE_H__ */
