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

#include "r_program.h"

// vertex arrays are used for many things
#define MAX_GL_ARRAY_LENGTH 16384
extern const float default_texcoords[];

// texunits maintain multitexture state
typedef struct r_texunit_s {
	qboolean enabled;  // GL_TEXTURE_2D on / off
	GLenum texture;  // e.g. GL_TEXTURE0_ARB
	GLint texnum;  // e.g 123
	GLfloat texcoord_array[MAX_GL_ARRAY_LENGTH * 2];
} r_texunit_t;

#define MAX_GL_TEXUNITS		4

// rendermodes, set via r_rendermode
typedef enum rendermode_s {
	rendermode_default,
	rendermode_pro
} r_rendermode_t;

// opengl state management
typedef struct renderer_state_s {
	int width, height;
	qboolean fullscreen;
	int virtualHeight, virtualWidth;
	float rx, ry;

	int redbits, greenbits, bluebits, alphabits;
	int depthbits, doublebits;

	qboolean ortho;  // 2d vs 3d projection

	GLfloat vertex_array_3d[MAX_GL_ARRAY_LENGTH * 3];  // default vertex arrays
	GLshort vertex_array_2d[MAX_GL_ARRAY_LENGTH * 2];
	GLfloat color_array[MAX_GL_ARRAY_LENGTH * 4];
	GLfloat normal_array[MAX_GL_ARRAY_LENGTH * 3];
	GLfloat tangent_array[MAX_GL_ARRAY_LENGTH * 3];

	GLenum blend_src, blend_dest;  // blend function
	qboolean blend_enabled;

	r_texunit_t texunits[MAX_GL_TEXUNITS];
	r_texunit_t *active_texunit;

	r_shader_t shaders[MAX_SHADERS];
	r_program_t programs[MAX_PROGRAMS];
	r_program_t *world_program;
	r_program_t *mesh_program;
	r_program_t *warp_program;
	r_program_t *pro_program;
	r_program_t *active_program;

	material_t *active_material;

	qboolean color_array_enabled;
	qboolean alpha_test_enabled;
	qboolean lighting_enabled;
	qboolean bumpmap_enabled;
	qboolean warp_enabled;
	qboolean shell_enabled;
	qboolean fog_enabled;

	r_rendermode_t rendermode;
} renderer_state_t;

extern renderer_state_t r_state;

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
void R_EnableBlend(qboolean enable);
void R_EnableAlphaTest(qboolean enable);
void R_EnableTexture(r_texunit_t *texunit, qboolean enable);
void R_EnableColorArray(qboolean enable);
void R_EnableLighting(r_program_t *program, qboolean enable);
void R_EnableBumpmap(material_t *material, qboolean enable);
void R_EnableWarp(r_program_t *program, qboolean enable);
void R_EnableShell(qboolean enable);
void R_EnableFog(qboolean enable);
void R_Setup3D(void);
void R_Setup2D(void);
void R_SetDefaultState(void);

#endif /* __R_STATE_H__ */
