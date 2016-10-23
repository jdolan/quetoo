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

#ifndef __R_STATE_H__
#define __R_STATE_H__

#include "r_types.h"

void R_EnableScissor(const SDL_Rect *bounds);
void R_Color(const vec4_t color);
void R_Setup3D(void);
void R_Setup2D(void);
void R_EnableColorArray(_Bool enable);
void R_BlendFunc(GLenum src, GLenum dest);
void R_EnableBlend(_Bool enable);
void R_EnableDepthTest(_Bool enable);
void R_EnableTextureID(const int texunit_id, _Bool enable);

void R_PushMatrix(const r_matrix_id_t id);
void R_PopMatrix(const r_matrix_id_t id);

#ifdef __R_LOCAL_H__

#include "r_program.h"

// vertex arrays are used for many things
#define MAX_GL_ARRAY_LENGTH 0x10000
extern const vec_t default_texcoords[];

// texunits maintain multitexture state
#define MAX_GL_TEXUNITS			8

typedef struct r_texunit_s {
	_Bool enabled; // on / off (off uses null texture)
	GLenum texture; // e.g. GL_TEXTURE0 + x
	GLuint texnum; // e.g 123
	GLuint bound; // the actual bound value regardless of enabled state
	GLfloat *texcoord_array;
	r_buffer_t buffer_texcoord_array;
} r_texunit_t;

// matrix stack
#define MAX_MATRIX_STACK		16

typedef struct r_matrix_stack_s {
	matrix4x4_t matrices[MAX_MATRIX_STACK];
	uint8_t depth;
} r_matrix_stack_t;

// opengl state management
typedef struct r_state_s {
	GLfloat vertex_array[MAX_GL_ARRAY_LENGTH * 3]; // default vertex arrays
	GLfloat color_array[MAX_GL_ARRAY_LENGTH * 4];
	GLfloat normal_array[MAX_GL_ARRAY_LENGTH * 3];
	GLfloat tangent_array[MAX_GL_ARRAY_LENGTH * 4];
	GLuint indice_array[MAX_GL_ARRAY_LENGTH * 4];

	// built-in buffers for the above vertex arrays
	r_buffer_t buffer_vertex_array;
	r_buffer_t buffer_color_array;
	r_buffer_t buffer_normal_array;
	r_buffer_t buffer_tangent_array;
	r_buffer_t buffer_indice_array;

	// the current buffers bound to the global
	// renderer state. This just prevents
	// them being bound multiple times in a row.
	GLuint active_buffers[R_NUM_BUFFERS];

	// the active element buffer.
	const r_buffer_t *element_buffer;

	// the buffers that will be passed to the
	// programs to be used in attributes.
	const r_buffer_t *array_buffers[R_ARRAY_MAX_ATTRIBS];

	GLenum blend_src, blend_dest; // blend function
	_Bool blend_enabled;

	float alpha_threshold;
	vec4_t current_color;

	r_texunit_t texunits[MAX_GL_TEXUNITS];
	r_texunit_t *active_texunit;

	r_matrix_stack_t matrix_stacks[R_MATRIX_TOTAL];

	r_shader_t shaders[MAX_SHADERS];
	r_program_t programs[MAX_PROGRAMS];
	r_program_t *default_program;
	r_program_t *shadow_program;
	r_program_t *shell_program;
	r_program_t *warp_program;
	r_program_t *null_program;
	r_program_t *corona_program;

	r_attrib_state_t attributes[R_ARRAY_MAX_ATTRIBS];

	const r_program_t *active_program;
	const r_material_t *active_material;

	// fog state
	r_fog_parameters_t active_fog_parameters;

	// stencil state
	GLenum stencil_op_pass;
	GLenum stencil_func_func;
	GLint stencil_func_ref;
	GLuint stencil_func_mask;

	// polygon offset state
	GLfloat polygon_offset_factor;
	GLfloat polygon_offset_units;

	uint8_t max_active_lights;

	_Bool color_array_enabled;
	_Bool alpha_test_enabled;
	_Bool stencil_test_enabled;
	_Bool polygon_offset_enabled;
	_Bool lighting_enabled;
	_Bool shadow_enabled;
	_Bool warp_enabled;
	_Bool shell_enabled;
	_Bool fog_enabled;
	_Bool depth_mask_enabled;
	_Bool depth_test_enabled;
} r_state_t;

extern r_state_t r_state;

// these are defined for convenience
#define texunit_diffuse			r_state.texunits[0]
#define texunit_lightmap		r_state.texunits[1]
#define texunit_deluxemap		r_state.texunits[2]
#define texunit_normalmap		r_state.texunits[3]
#define texunit_specularmap		r_state.texunits[4]

#define R_GetError(msg) R_GetError_(__func__, msg)
void R_GetError_(const char *function, const char *msg);
void R_SelectTexture(r_texunit_t *texunit);
void R_BindUnitTexture(r_texunit_t *texunit, GLuint texnum);
void R_BindTexture(GLuint texnum);
void R_BindLightmapTexture(GLuint texnum);
void R_BindDeluxemapTexture(GLuint texnum);
void R_BindNormalmapTexture(GLuint texnum);
void R_BindSpecularmapTexture(GLuint texnum);
void R_BindDefaultArray(int target);
void R_EnableDepthMask(_Bool enable);

void R_BindBuffer(const r_buffer_t *buffer);
void R_UnbindBuffer(const int type);
void R_UploadToBuffer(r_buffer_t *buffer, const size_t start, const size_t size, const void *data);
void R_CreateBuffer(r_buffer_t *buffer, const GLenum hint, const int type, const size_t size, const void *data);
void R_DestroyBuffer(r_buffer_t *buffer);
_Bool R_ValidBuffer(const r_buffer_t *buffer);

void R_BindArray(int target, const r_buffer_t *buffer);

#define ALPHA_TEST_DISABLED_THRESHOLD 0.0
#define ALPHA_TEST_ENABLED_THRESHOLD 0.25

void R_EnableAlphaTest(float threshold);
void R_EnableStencilTest(GLenum pass, _Bool enable);
void R_StencilFunc(GLenum func, GLint ref, GLuint mask);
void R_EnablePolygonOffset(GLenum mode, _Bool enable);
void R_PolygonOffset(GLfloat factor, GLfloat units);
void R_EnableTexture(r_texunit_t *texunit, _Bool enable);
void R_EnableLighting(const r_program_t *program, _Bool enable);
void R_EnableShadow(const r_program_t *program, _Bool enable);
void R_EnableWarp(const r_program_t *program, _Bool enable);
void R_EnableShell(const r_program_t *program, _Bool enable);
void R_EnableFog(_Bool enable);
void R_UseMaterial(const r_material_t *material);
void R_UseMatrices(void);
void R_UseInterpolation(const float lerp);
void R_UseAlphaTest(void);
void R_UseCurrentColor(void);
void R_UseFog(void);
void R_InitState(void);
void R_ShutdownState(void);

#endif /* __R_LOCAL_H__ */

#endif /* __R_STATE_H__ */
