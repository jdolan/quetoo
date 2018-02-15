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

void R_EnableScissor(const SDL_Rect *bounds);
void R_Color(const vec4_t color);
void R_Setup3D(void);
void R_Setup2D(void);
void R_EnableColorArray(_Bool enable);
void R_BlendFunc(GLenum src, GLenum dest);
void R_EnableBlend(_Bool enable);
void R_EnableDepthTest(_Bool enable);
void R_DepthRange(GLdouble znear, GLdouble zfar);
void R_SetViewport(GLint x, GLint y, GLsizei width, GLsizei height, _Bool force);

void R_GetMatrix(const r_matrix_id_t id, matrix4x4_t *matrix);
void R_SetMatrix(const r_matrix_id_t id, const matrix4x4_t *matrix);
const matrix4x4_t *R_GetMatrixPtr(const r_matrix_id_t id);

void R_PushMatrix(const r_matrix_id_t id);
void R_PopMatrix(const r_matrix_id_t id);

void R_EnableTextureByIdentifier(const r_texunit_id_t texunit_id, _Bool enable);

uint32_t R_GetNumAllocatedBuffers(void);
uint32_t R_GetNumAllocatedBufferBytes(void);

#ifdef __R_LOCAL_H__

#include "r_program.h"

// vertex arrays are used for many things
extern const vec2_t default_texcoords[4];

// texunits maintain multitexture state
typedef struct r_texunit_s {
	_Bool enabled; // on / off (off uses null texture)
	GLenum texture; // e.g. GL_TEXTURE0 + x
	GLuint texnum; // e.g 123
} r_texunit_t;

// matrix stack
#define MAX_MATRIX_STACK		16

typedef struct {
	matrix4x4_t matrices[MAX_MATRIX_STACK];
	uint8_t depth;
} r_matrix_stack_t;

// SEE uniforms.glsl BEFORE TOUCHING THIS STRUCTURE!!!
typedef struct {
	matrix4x4_t matrices[R_MATRIX_TOTAL];
	vec4_t global_color;
	vec_t time;
	vec_t time_fraction;
} r_program_uniforms_t;

typedef struct {
	size_t begin, end;
} r_update_bounds_t;

void R_ExpandUpdateBounds(r_update_bounds_t *bounds, const size_t offset, const size_t size);

#define R_EXPAND_BOUNDS(bounds, member) \
	R_ExpandUpdateBounds(&bounds, offsetof(r_program_uniforms_t, member), sizeof(r_state.uniforms.member));

// opengl state management
typedef struct r_state_s {
	// the current buffers bound to the global
	// renderer state. This just prevents
	// them being bound multiple times in a row.
	GLuint active_buffers[R_NUM_BUFFERS];

	uint32_t buffers_total_bytes;
	uint32_t buffers_total;
	GHashTable *buffers_list;

	// the active element buffer.
	const r_buffer_t *element_buffer;

	// the buffers that will be passed to the
	// programs to be used in attributes.
	const r_buffer_t *array_buffers[R_ATTRIB_ALL];
	GLsizei array_buffer_offsets[R_ATTRIB_ALL];
	r_attribute_mask_t array_buffers_dirty;
	GLuint vertex_array_object;

	GLenum blend_src, blend_dest; // blend function
	_Bool blend_enabled;

	float alpha_threshold;

	r_texunit_t texunits[R_TEXUNIT_TOTAL];
	r_texunit_t *active_texunit;

	r_program_uniforms_t uniforms;
	r_buffer_t uniforms_buffer;
	r_update_bounds_t uniforms_dirty;
	r_matrix_stack_t matrix_stacks[R_MATRIX_TOTAL];

	r_program_t programs[R_PROGRAM_TOTAL];
	r_program_t *particle_program;
	r_program_t *corona_program;

	r_attrib_state_t attributes[R_ATTRIB_ALL];

	const r_program_t *active_program;
	const r_material_t *active_material;

	// fog state
	r_fog_parameters_t active_fog_parameters;

	// caustic state
	r_caustic_parameters_t active_caustic_parameters;

	// stencil state
	GLenum stencil_op_pass;
	GLenum stencil_func_func;
	GLint stencil_func_ref;
	GLuint stencil_func_mask;

	// polygon offset state
	GLfloat polygon_offset_factor;
	GLfloat polygon_offset_units;

	// depth state
	GLdouble depth_near;
	GLdouble depth_far;

	SDL_Rect current_viewport;

	r_image_t *supersample_image;
	r_framebuffer_t *supersample_fb;

	uint16_t max_active_lights;

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
	_Bool scissor_enabled;
	_Bool screenshot_pending;
} r_state_t;

extern r_state_t r_state;

// these are defined for convenience
#define texunit_diffuse				(&r_state.texunits[R_TEXUNIT_DIFFUSE])
#define texunit_lightmap			(&r_state.texunits[R_TEXUNIT_LIGHTMAP])
#define texunit_deluxemap			(&r_state.texunits[R_TEXUNIT_DELUXEMAP])
#define texunit_normalmap			(&r_state.texunits[R_TEXUNIT_NORMALMAP])
#define texunit_specularmap			(&r_state.texunits[R_TEXUNIT_SPECULARMAP])
#define texunit_warp				(&r_state.texunits[R_TEXUNIT_WARP])
#define texunit_tint				(&r_state.texunits[R_TEXUNIT_TINTMAP])
#define texunit_stainmap			(&r_state.texunits[R_TEXUNIT_STAINMAP])

#define program_default				(&r_state.programs[R_PROGRAM_DEFAULT])
#define program_shadow				(&r_state.programs[R_PROGRAM_SHADOW])
#define program_shell				(&r_state.programs[R_PROGRAM_SHELL])
#define program_warp				(&r_state.programs[R_PROGRAM_WARP])
#define program_null				(&r_state.programs[R_PROGRAM_NULL])
#define program_corona				(&r_state.programs[R_PROGRAM_CORONA])
#define program_stain				(&r_state.programs[R_PROGRAM_STAIN])
#define program_particle			(&r_state.programs[R_PROGRAM_PARTICLE])
#define program_particle_corona		(&r_state.programs[R_PROGRAM_PARTICLE_CORONA])

#define R_GetError(msg) R_GetError_(__func__, msg)
void R_GetError_(const char *function, const char *msg);
void R_SelectTexture(r_texunit_t *texunit);

void R_BindUnitTexture(r_texunit_t *texunit, GLuint texnum, GLenum target);

#define R_BindDiffuseTexture(texnum)		R_BindUnitTexture(texunit_diffuse, texnum, GL_TEXTURE_2D)
#define R_BindLightmapTexture(texnum)		R_BindUnitTexture(texunit_lightmap, texnum, GL_TEXTURE_2D_ARRAY)
#define R_BindDeluxemapTexture(texnum)		R_BindUnitTexture(texunit_deluxemap, texnum, GL_TEXTURE_2D)
#define R_BindNormalmapTexture(texnum)		R_BindUnitTexture(texunit_normalmap, texnum, GL_TEXTURE_2D)
#define R_BindSpecularmapTexture(texnum)	R_BindUnitTexture(texunit_specularmap, texnum, GL_TEXTURE_2D)
#define R_BindWarpTexture(texnum)			R_BindUnitTexture(texunit_warp, texnum, GL_TEXTURE_2D)
#define R_BindTintTexture(texnum)			R_BindUnitTexture(texunit_tint, texnum, GL_TEXTURE_2D)
#define R_BindStainmapTexture(texnum)		R_BindUnitTexture(texunit_stainmap, texnum, GL_TEXTURE_2D)

void R_EnableDepthMask(_Bool enable);

#define ALPHA_TEST_DISABLED_THRESHOLD 0.0
#define ALPHA_TEST_ENABLED_THRESHOLD 0.25

void R_InitSupersample(void);

void R_EnableAlphaTest(vec_t threshold);
void R_EnableStencilTest(GLenum pass, _Bool enable);
void R_StencilFunc(GLenum func, GLint ref, GLuint mask);
void R_EnablePolygonOffset(_Bool enable);
void R_PolygonOffset(GLfloat factor, GLfloat units);
void R_EnableTexture(r_texunit_t *texunit, _Bool enable);
void R_EnableLighting(const r_program_t *program, _Bool enable);
void R_EnableShadow(const r_program_t *program, _Bool enable);
void R_EnableWarp(const r_program_t *program, _Bool enable);
void R_EnableShell(const r_program_t *program, _Bool enable);
void R_EnableFog(_Bool enable);
void R_EnableCaustic(_Bool enable);
void R_UseMaterial(const r_material_t *material);
void R_UseMatrices(void);
void R_UseInterpolation(const vec_t lerp);
void R_UseAlphaTest(void);
void R_UseFog(void);
void R_UseCaustic(void);
void R_UseTints(void);
void R_InitState(void);
void R_ShutdownState(void);

#endif /* __R_LOCAL_H__ */
